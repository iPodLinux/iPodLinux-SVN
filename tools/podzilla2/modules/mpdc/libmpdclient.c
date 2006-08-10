/* libmpdclient
   (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
   This project's homepage is: http://www.musicpd.org

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of the Music Player Daemon nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "libmpdclient.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/param.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#ifndef MPD_NO_IPV6
#ifdef AF_INET6
#define MPD_HAVE_IPV6
#endif
#endif

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

#define COMMAND_LIST	1
#define COMMAND_LIST_OK	2

/* ensure all pointers we free() become NULL */
#define safe_free(ptr) do { free(ptr); ptr = NULL; } while (0)

#ifdef MPD_HAVE_IPV6
int mpd_ipv6Supported(void) {
        int s;
        s = socket(AF_INET6,SOCK_STREAM,0);
        if(s == -1) return 0;
        close(s);
        return 1;
}
#endif

#define MAX_INT_STRLEN (sizeof(int)*3)

#define CMD_0(func,cmd) \
void func (mpd_Connection *connection) \
{ \
	mpd_executeCommand(connection,cmd"\n"); \
}

/* the version that takes a static buffer */
static void mpd_sanitizeArg_st(char * dest, const char * arg) {
	size_t i;
	register const char *c;
	register char *rc;

	c = arg;
	rc = dest;
	for(i = strlen(arg)+1; i != 0; --i) {
		if(*c=='"' || *c=='\\') {
			*rc = '\\';
			rc++;
		}
		*(rc++) = *(c++);
	}
	*rc = '\0';
}

/* in an effort to save and reuse every single byte of memory we have
 * available to us, we'll reuse the _errorStr_ or _buffer_ array in
 * a mpd_Connection struct to send */
#  define CMD_1_int(func,cmd) \
void func (mpd_Connection *connection, const int arg1) \
{ \
	char * string = connection->buffer; \
	sprintf(string,cmd" \"%i\"\n",arg1); \
	mpd_executeCommand(connection,string); \
}

#  define CMD_1_long_long(func,cmd) \
void func (mpd_Connection *connection, const long long arg1) \
{ \
	char * string = connection->buffer; \
	sprintf(string,cmd" \"%lld\"\n",arg1); \
	mpd_executeCommand(connection,string); \
}

#  define CMD_2_int(func,cmd) \
void func (mpd_Connection *connection, const int arg1, const int arg2) \
{ \
	char * string = connection->buffer; \
	sprintf(string,cmd" \"%i\" \"%i\"\n",arg1,arg2); \
	mpd_executeCommand(connection,string); \
}

#  define CMD_1_str(func,cmd) \
void func (mpd_Connection *connection, const char *arg1) \
{ \
	char * sName = connection->errorStr; \
	char * string = connection->buffer; \
	mpd_sanitizeArg_st(sName, arg1); \
	sprintf(string,cmd" \"%s\"\n",sName); \
	*sName = '\0'; \
	mpd_executeCommand(connection,string); \
}

static inline void mpd_newReturnElement(mpd_ReturnElement * ret,
		const char * name, const char * value)
{
#ifdef LIBMPDCLIENT_STATS
	static char * save_name = NULL, * save_value = NULL;
	static size_t max_name = 0, max_value = 0;

	if (strlen(name) > max_name) {
		max_name = strlen(name);
		if (save_name)
			free(save_name);
		save_name = strdup(name);
		fprintf(stderr,"max_name:%zu:%s\n",max_name,save_name);
	}
	if (strlen(value) > max_value) {
		max_value = strlen(value);
		if (save_value)
			free(save_value);
		save_value = strdup(value);
		fprintf(stderr,"max_value:%zu:%s\n",max_value,save_value);
	}
#endif /* LIBMPDCLIENT_STATS */
	/* strncpy slows things down here: by a factor of 10 or so,
	 * even the strdup version below is faster than strncpy.
	 * this is called in a loop for playlists and listings, so
	 * this better be fast.
	 * - name: this is very short, 16 characters should be plenty
	 * - value: the longest this gets is for filenames, so MAXPATHLEN
	 */
	strcpy(ret->name, name);
	strcpy(ret->value, value);
}

void mpd_setConnectionTimeout(mpd_Connection * connection, int timeout) {
		connection->timeout.tv_sec = timeout;
		connection->timeout.tv_usec = 0;
}

static void mpd_initConnection(mpd_Connection *connection) {
	connection->buffer[0] = (size_t)0;
	connection->errorStr[0] = (size_t)0;
	connection->returnElement.name[0] = (size_t)0;
	connection->buflen = 0;
	connection->bufstart = 0;
	connection->error = 0;
	connection->doneProcessing = 0;
	connection->commandList = 0;
	connection->listOks = 0;
	connection->doneListOk = 0;
}

void mpd_newConnection_st(mpd_Connection *connection, const char * host,
				int port, int timeout) {
	int err;
	struct hostent * he;
	struct sockaddr * dest;
#ifdef HAVE_SOCKLEN_T
	socklen_t destlen;
#else
	int destlen;
#endif
	struct sockaddr_in sin;
	char * rt;
	char * output;
	struct timeval tv;
	fd_set fds;
#ifdef MPD_HAVE_IPV6
	struct sockaddr_in6 sin6;
#endif
	mpd_initConnection(connection);

	if(!(he=gethostbyname(host))) {
		snprintf(connection->errorStr,MPD_BUFFER_MAX_LENGTH,
				"host \"%s\" not found",host);
		connection->error = MPD_ERROR_UNKHOST;
		return;
	}

	memset(&sin,0,sizeof(struct sockaddr_in));
	/*dest.sin_family = he->h_addrtype;*/
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
#ifdef MPD_HAVE_IPV6
	memset(&sin6,0,sizeof(struct sockaddr_in6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(port);
#endif
	switch(he->h_addrtype) {
	case AF_INET:
		memcpy((char *)&sin.sin_addr.s_addr,(char *)he->h_addr,
				he->h_length);
		dest = (struct sockaddr *)&sin;
		destlen = sizeof(struct sockaddr_in);
		break;
#ifdef MPD_HAVE_IPV6
	case AF_INET6:
		if(!mpd_ipv6Supported()) {
			strcpy(connection->errorStr,"no IPv6 suuport but a "
					"IPv6 address found\n");
			connection->error = MPD_ERROR_SYSTEM;
			return;
		}
		memcpy((char *)&sin6.sin6_addr.s6_addr,(char *)he->h_addr,
				he->h_length);
		dest = (struct sockaddr *)&sin6;
		destlen = sizeof(struct sockaddr_in6);
		break;
#endif
	default:
		strcpy(connection->errorStr,"address type is not IPv4 or "
				"IPv6\n");
		connection->error = MPD_ERROR_SYSTEM;
		return;
		break;
	}

	if((connection->sock = socket(dest->sa_family,SOCK_STREAM,0))<0) {
		strcpy(connection->errorStr,"problems creating socket");
		connection->error = MPD_ERROR_SYSTEM;
		return;
	}

	mpd_setConnectionTimeout(connection,timeout);

	/* connect stuff */
	{
		int flags = fcntl(connection->sock, F_GETFL, 0);
		fcntl(connection->sock, F_SETFL, flags | O_NONBLOCK);

		if(connect(connection->sock,dest,destlen)<0 &&
				errno!=EINPROGRESS)
		{
			snprintf(connection->errorStr,MPD_BUFFER_MAX_LENGTH,
					"problems connecting to \"%s\" on port"
				 	" %i",host,port);
			connection->error = MPD_ERROR_CONNPORT;
			return;
		}
	}

	while(!(rt = strchr(connection->buffer,'\n'))) {
		tv.tv_sec = connection->timeout.tv_sec;
		tv.tv_usec = connection->timeout.tv_usec;
		FD_ZERO(&fds);
		FD_SET(connection->sock,&fds);
		if((err = select(connection->sock+1,&fds,NULL,NULL,&tv)) == 1) {
			int readed;
			readed = recv(connection->sock,
				&(connection->buffer[connection->buflen]),
				MPD_BUFFER_MAX_LENGTH-connection->buflen,0);
			if(readed<=0) {
				snprintf(connection->errorStr,MPD_BUFFER_MAX_LENGTH,
					"problems getting a response from"
					" \"%s\" on port %i : %s",host,
					port, strerror(errno));
				connection->error = MPD_ERROR_NORESPONSE;
				return;
			}
			connection->buflen+=readed;
			/* this is to ensure the strstr/strchr works */
			connection->buffer[connection->buflen] = '\0';
			tv.tv_sec = connection->timeout.tv_sec;
			tv.tv_usec = connection->timeout.tv_usec;
		}
		else if(err<0) {
			switch(errno) {
			case EINTR:
				continue;
			default:
				snprintf(connection->errorStr,
					MPD_BUFFER_MAX_LENGTH,
					"problems connecting to \"%s\" on port"
				 	" %i",host,port);
				connection->error = MPD_ERROR_CONNPORT;
				return;
			}
		}
		else {
			snprintf(connection->errorStr,MPD_BUFFER_MAX_LENGTH,
				"timeout in attempting to get a response from"
				 " \"%s\" on port %i",host,port);
			connection->error = MPD_ERROR_NORESPONSE;
			return;
		}
	}

	*rt = '\0';

	/* output = strdup(connection->buffer); */ /* use errorStr as a temp holder :) */
	output = connection->errorStr;
	strcpy(output,connection->buffer);

	strcpy(connection->buffer,rt+1);
	connection->buflen = strlen(connection->buffer);

	if(strncmp(output,MPD_WELCOME_MESSAGE,strlen(MPD_WELCOME_MESSAGE))) {
		snprintf(connection->errorStr,MPD_BUFFER_MAX_LENGTH,
				"mpd not running on port %i on host \"%s\"",
				port,host);
		connection->error = MPD_ERROR_NOTMPD;
		return;
	}

	{
		char * test;
		char * version[3];
		char * tmp = &output[strlen(MPD_WELCOME_MESSAGE)];
		char * search = ".";
		int i;

		for(i=0;i<3;i++) {
			char * tok;
			if(i==3) search = " ";
			version[i] = strtok_r(tmp,search,&tok);
			if(!version[i]) {
				snprintf(connection->errorStr,
					MPD_BUFFER_MAX_LENGTH,
					"error parsing version number at "
					"\"%s\"",
					&output[strlen(MPD_WELCOME_MESSAGE)]);
				connection->error = MPD_ERROR_NOTMPD;
				return;
			}
			connection->version[i] = strtol(version[i],&test,10);
			if(version[i]==test || *test!='\0') {
				snprintf(connection->errorStr,
					MPD_BUFFER_MAX_LENGTH,
					"error parsing version number at "
					"\"%s\"",
					&output[strlen(MPD_WELCOME_MESSAGE)]);
				connection->error = MPD_ERROR_NOTMPD;
				return;
			}
			tmp = NULL;
		}
	}

	connection->doneProcessing = 1;
	output[0] = '\0'; /* same as: connection->errorStr[0] = '\0'; */

	return;
}

void mpd_clearError(mpd_Connection * connection) {
	connection->error = 0;
	connection->errorStr[0] = '\0'; /* seriously, is this even needed? */
}

void mpd_closeConnection(mpd_Connection * connection) {
	close(connection->sock);
}

/* note: it's safe to use/reuse command == connection->buffer or even
 * connection->errorStr (it's only reset on error) */
void mpd_executeCommand(mpd_Connection * connection, char * command) {
	int ret;
	struct timeval tv;
	fd_set fds;
	char * commandPtr = command;
	int commandLen = strlen(command);

	if(!connection->doneProcessing && !connection->commandList) {
		strcpy(connection->errorStr,"not done processing current command");
		connection->error = 1;
		return;
	}

	mpd_clearError(connection);

	FD_ZERO(&fds);
	FD_SET(connection->sock,&fds);
	tv.tv_sec = connection->timeout.tv_sec;
	tv.tv_usec = connection->timeout.tv_usec;

	while((ret = select(connection->sock+1,NULL,&fds,NULL,&tv)==1) ||
			(ret==-1 && errno==EINTR)) {
		ret = send(connection->sock,commandPtr,commandLen,
#ifdef WIN32
			   ioctlsocket(connection->sock, commandLen, commandPtr));
#endif
#ifndef WIN32
				MSG_DONTWAIT);
#endif
		if(ret<=0)
		{
			if(ret==EAGAIN || ret==EINTR) continue;
			snprintf(connection->errorStr,MPD_BUFFER_MAX_LENGTH,
				"problems giving command \"%s\"",command);
			connection->error = MPD_ERROR_SENDING;
			return;
		}
		else {
			commandPtr+=ret;
			commandLen-=ret;
		}

		if(commandLen<=0) break;
	}

	if(commandLen>0) {
		perror("");
		snprintf(connection->errorStr,MPD_BUFFER_MAX_LENGTH,
			"timeout sending command \"%s\"",command);
		connection->error = MPD_ERROR_TIMEOUT;
		return;
	}

	if(!connection->commandList) connection->doneProcessing = 0;
	else if(connection->commandList == COMMAND_LIST_OK) {
		connection->listOks++;
	}
}

static void mpd_getNextReturnElement(mpd_Connection * connection) {
	char * output = NULL;
	char * rt = NULL;
	char * name = NULL;
	char * value = NULL;
	fd_set fds;
	struct timeval tv;
	char * tok = NULL;
	int readed;
	char * bufferCheck = NULL;
	int err;

	/* functions will check the first word in this array for non-zero */
	connection->returnElement.name[0] = (size_t)0;
	if(connection->doneProcessing || (connection->listOks &&
			connection->doneListOk))
	{
		strcpy(connection->errorStr,"already done processing current command");
		connection->error = 1;
		return;
	}

	bufferCheck = connection->buffer+connection->bufstart;
	while(connection->bufstart>=connection->buflen ||
			!(rt = strchr(bufferCheck,'\n'))) {
		if(connection->buflen>=MPD_BUFFER_MAX_LENGTH) {
			memmove(connection->buffer,
					connection->buffer+
					connection->bufstart,
					connection->buflen-
					connection->bufstart+1);
			connection->buflen-=connection->bufstart;
			connection->bufstart = 0;
		}
		if(connection->buflen>=MPD_BUFFER_MAX_LENGTH) {
			strcpy(connection->errorStr,"buffer overrun");
			connection->error = MPD_ERROR_BUFFEROVERRUN;
			connection->doneProcessing = 1;
			connection->doneListOk = 0;
			return;
		}
		bufferCheck = connection->buffer+connection->buflen;
		tv.tv_sec = connection->timeout.tv_sec;
		tv.tv_usec = connection->timeout.tv_usec;
		FD_ZERO(&fds);
		FD_SET(connection->sock,&fds);
		if((err = select(connection->sock+1,&fds,NULL,NULL,&tv) == 1)) {
			readed = recv(connection->sock,
				connection->buffer+connection->buflen,
				MPD_BUFFER_MAX_LENGTH-connection->buflen,
#ifdef WIN32
				ioctlsocket(connection->sock,
					    commandLen,
					    commandPtr));
#endif
#ifndef WIN32
				MSG_DONTWAIT);
#endif
			if(readed<0 && (errno==EAGAIN || errno==EINTR)) {
				continue;
			}
			if(readed<=0) {
				strcpy(connection->errorStr,"connection"
					" closed");
				connection->error = MPD_ERROR_CONNCLOSED;
				connection->doneProcessing = 1;
				connection->doneListOk = 0;
				return;
			}
			connection->buflen+=readed;
			connection->buffer[connection->buflen] = '\0';
		}
		else if(err<0 && errno==EINTR) continue;
		else {
			strcpy(connection->errorStr,"connection timeout");
			connection->error = MPD_ERROR_TIMEOUT;
			connection->doneProcessing = 1;
			connection->doneListOk = 0;
			return;
		}
	}

	*rt = '\0';
	output = connection->buffer+connection->bufstart;
	connection->bufstart = rt - connection->buffer + 1;

	if(strcmp(output,"OK")==0) {
		if(connection->listOks > 0) {
			strcpy(connection->errorStr, "expected more list_OK's");
			connection->error = 1;
		}
		connection->listOks = 0;
		connection->doneProcessing = 1;
		connection->doneListOk = 0;
		return;
	}

	if(strcmp(output, "list_OK") == 0) {
		if(!connection->listOks) {
			strcpy(connection->errorStr,
					"got an unexpected list_OK");
			connection->error = 1;
		}
		else {
			connection->doneListOk = 1;
			connection->listOks--;
		}
		return;
	}

	if(strcmp(output,"ACK")==0) {
		char * test;
		char * needle;
		int val;

		strcpy(connection->errorStr, output);
		connection->error = MPD_ERROR_ACK;
		connection->errorCode = MPD_ACK_ERROR_UNK;
		connection->errorAt = MPD_ERROR_AT_UNK;
		connection->doneProcessing = 1;
		connection->doneListOk = 0;

		needle = strchr(output, '[');
		if(!needle) return;
		val = strtol(needle+1, &test, 10);
		if(*test != '@') return;
		connection->errorCode = val;
		val = strtol(test+1, &test, 10);
		if(*test != ']') return;
		connection->errorAt = val;
		return;
	}

	name = strtok_r(output,":",&tok);
	if(name && (value = strtok_r(NULL,"",&tok)) && value[0]==' ') {
			mpd_newReturnElement(&(connection->returnElement),
					name,&(value[1]));
	}
	else {
		if(!name || !value) {
			snprintf(connection->errorStr,MPD_BUFFER_MAX_LENGTH,
					"error parsing: %s",output);
		}
		else {
			snprintf(connection->errorStr,MPD_BUFFER_MAX_LENGTH,
					"error parsing: %s:%s",name,value);
		}
		connection->errorStr[MPD_BUFFER_MAX_LENGTH] = '\0';
		connection->error = 1;
	}
}

void mpd_finishCommand(mpd_Connection * connection) {
	while(!connection->doneProcessing) {
		if(connection->doneListOk) connection->doneListOk = 0;
		mpd_getNextReturnElement(connection);
	}
}

void mpd_finishListOkCommand(mpd_Connection * connection) {
	while(!connection->doneProcessing && connection->listOks &&
			!connection->doneListOk )
	{
		mpd_getNextReturnElement(connection);
	}
}

int mpd_nextListOkCommand(mpd_Connection * connection) {
	mpd_finishListOkCommand(connection);
	if(!connection->doneProcessing) connection->doneListOk = 0;
	if(connection->listOks == 0 || connection->doneProcessing) return -1;
	return 0;
}

CMD_0(mpd_sendStatusCommand,"status")

void mpd_statusReset(mpd_Status *status) {
	status->volume = -1;
	status->repeat = 0;
	status->random = 0;
	status->playlist = -1;
	status->playlistLength = -1;
	status->state = -1;
	status->song = 0;
	status->elapsedTime = 0;
	status->totalTime = 0;
	status->bitRate = 0;
	status->sampleRate = 0;
	status->bits = 0;
	status->channels = 0;
	status->crossfade = -1;
	if (status->error) safe_free(status->error);
	status->updatingDb = 0;
}

unsigned int mpd_getStatus_st(mpd_Status * status, mpd_Connection * connection)
{
	if(connection->error) return 0;
	if(!connection->returnElement.name[0])
		mpd_getNextReturnElement(connection);
	if(connection->error) return 0;

	mpd_statusReset(status);

	if(connection->doneProcessing || (connection->listOks &&
			connection->doneListOk))
	{
		return 0;
	}

	while(connection->returnElement.name[0]) {
		char * re_name = connection->returnElement.name;
		char * re_value = connection->returnElement.value;
		if(strcmp(re_name,"volume")==0) {
			status->volume = atoi(re_value);
		}
		else if(strcmp(re_name,"repeat")==0) {
			status->repeat = atoi(re_value);
		}
		else if(strcmp(re_name,"random")==0) {
			status->random = atoi(re_value);
		}
		else if(strcmp(re_name,"playlist")==0) {
			status->playlist = strtol(re_value,NULL,10);
		}
		else if(strcmp(re_name,"playlistlength")==0) {
			status->playlistLength = atoi(re_value);
		}
		else if(strcmp(re_name,"bitrate")==0) {
			status->bitRate = atoi(re_value);
		}
		else if(strcmp(re_name,"state")==0) {
			if(strcmp(re_value,"play")==0) {
				status->state = MPD_STATUS_STATE_PLAY;
			}
			else if(strcmp(re_value,"stop")==0) {
				status->state = MPD_STATUS_STATE_STOP;
			}
			else if(strcmp(re_value,"pause")==0) {
				status->state = MPD_STATUS_STATE_PAUSE;
			}
			else {
				status->state = MPD_STATUS_STATE_UNKNOWN;
			}
		}
		else if(strcmp(re_name,"song")==0) {
			status->song = atoi(re_value);
		}
		else if(strcmp(re_name,"songid")==0) {
			status->songid = atoi(re_value);
		}
		else if(strcmp(re_name,"time")==0) {
			char * tok = strchr(re_value,':');
			/* the second strchr below is a safety check */
			if (tok && (strchr(tok,0) > (tok+1))) {
				/* atoi stops at the first non-[0-9] char: */
				status->elapsedTime = atoi(re_value);
				status->totalTime = atoi(tok+1);
			}
		}
		else if(strcmp(re_name,"error")==0) {
			/* the only malloc in the whole function now: */
			status->error = strdup(re_value);
		}
		else if(strcmp(re_name,"xfade")==0) {
			status->crossfade = atoi(re_value);
		}
		else if(strcmp(re_name,"updating_db")==0) {
			status->updatingDb = atoi(re_value);
		}
		else if(strcmp(re_name,"audio")==0) {
			char * tok = strchr(re_value,':');
			if (tok && (strchr(tok,0) > (tok+1))) {
				status->sampleRate = atoi(re_value);
				status->bits = atoi(++tok);
				tok = strchr(tok,':');
				if (tok && (strchr(tok,0) > (tok+1)))
					status->channels = atoi(tok+1);
			}
		}

		mpd_getNextReturnElement(connection);
		if(connection->error) return 0;
	}

	if(connection->error)
		return 0;
	else if(status->state<0) {
		strcpy(connection->errorStr,"state not found");
		connection->error = 1;
		return 0;
	}

	return 1;
}

void mpd_freeStatus(mpd_Status * status) {
	if(status->error) safe_free(status->error);
}

CMD_0(mpd_sendStatsCommand,"stats")

unsigned int mpd_getStats_st(mpd_Stats * stats, mpd_Connection * connection) {
	if(connection->doneProcessing || (connection->listOks &&
			connection->doneListOk))
	{
		return 0;
	}

	if(!connection->returnElement.name[0])
		mpd_getNextReturnElement(connection);

	stats->numberOfArtists = 0;
	stats->numberOfAlbums = 0;
	stats->numberOfSongs = 0;
	stats->uptime = 0;
	stats->dbUpdateTime = 0;
	stats->playTime = 0;
	stats->dbPlayTime = 0;

	if(connection->error)
		return 0;
	while(connection->returnElement.name[0]) {
		char * re_name = connection->returnElement.name;
		char * re_value = connection->returnElement.value;
		if(strcmp(re_name,"artists")==0) {
			stats->numberOfArtists = atoi(re_value);
		}
		else if(strcmp(re_name,"albums")==0) {
			stats->numberOfAlbums = atoi(re_value);
		}
		else if(strcmp(re_name,"songs")==0) {
			stats->numberOfSongs = atoi(re_value);
		}
		else if(strcmp(re_name,"uptime")==0) {
			stats->uptime = strtol(re_value,NULL,10);
		}
		else if(strcmp(re_name,"db_update")==0) {
			stats->dbUpdateTime = strtol(re_value,NULL,10);
		}
		else if(strcmp(re_name,"playtime")==0) {
			stats->playTime = strtol(re_value,NULL,10);
		}
		else if(strcmp(re_name,"db_playtime")==0) {
			stats->dbPlayTime = strtol(re_value,NULL,10);
		}

		mpd_getNextReturnElement(connection);
		if(connection->error)
			return 0;
	}

	if(connection->error)
		return 0;

	return 1;
}

static void mpd_initSong(mpd_Song * song) {
	song->file = NULL;
	song->artist = NULL;
	song->album = NULL;
	song->track = NULL;
	song->title = NULL;
	song->name = NULL;
	song->date = NULL;
	/* added by Qball */
	song->genre = NULL;
	song->composer = NULL;

	song->time = MPD_SONG_NO_TIME;
	song->pos = MPD_SONG_NO_NUM;
	song->id = MPD_SONG_NO_ID;
}

#define song_force_null(tag) do { if (tag) safe_free(tag); } while (0)
static void mpd_finishSong(mpd_Song * song) {
	song_force_null(song->file);
	song_force_null(song->artist);
	song_force_null(song->album);
	song_force_null(song->title);
	song_force_null(song->track);
	song_force_null(song->name);
	song_force_null(song->date);
	song_force_null(song->genre);
	song_force_null(song->composer);
}
#undef song_force_null


/* This is for mpd_getNextInfoEntity_st ONLY */
#define init_static_song_str_element(song_element) do { \
	if (!song_element) song_element = malloc(MAXPATHLEN); \
	song_element[0] = '\0'; \
} while (0)
static void mpd_resetSong_st(mpd_Song * song) {
	init_static_song_str_element(song->file);
	init_static_song_str_element(song->artist);
	init_static_song_str_element(song->album);
	init_static_song_str_element(song->track);
	init_static_song_str_element(song->title);
	init_static_song_str_element(song->name);
	init_static_song_str_element(song->date);
	init_static_song_str_element(song->genre);
	init_static_song_str_element(song->composer);
	song->time = MPD_SONG_NO_TIME;
	song->pos = MPD_SONG_NO_NUM;
	song->id = MPD_SONG_NO_ID;
}
#undef init_static_song_str_element

/* This is for mpd_getNextInfoEntity_st ONLY */
static mpd_Song * mpd_getSong_st(void) {
	static mpd_Song * ret = NULL;

	if (!ret)
		ret = mpd_newSong();

	mpd_resetSong_st(ret);

	return ret;
}

static mpd_Song * mpd_getTmpSong_st(void) {
	static mpd_Song tmp;
	tmp.file = NULL;
	tmp.artist = NULL;
	tmp.album = NULL;
	tmp.track = NULL;
	tmp.title = NULL;
	tmp.name = NULL;
	tmp.date = NULL;
	tmp.genre = NULL;
	tmp.composer = NULL;
	tmp.time = MPD_SONG_NO_TIME;
	tmp.pos = MPD_SONG_NO_NUM;
	tmp.id = MPD_SONG_NO_ID;

	return &tmp;
}

mpd_Song * mpd_newSong(void) {
	mpd_Song * ret = malloc(sizeof(mpd_Song));

	mpd_initSong(ret);

	return ret;
}

void mpd_freeSong(mpd_Song * song) {
	mpd_finishSong(song);
	safe_free(song);
}

mpd_Song * mpd_songDup(mpd_Song * song) {
	mpd_Song * ret = mpd_newSong();

	if(song->file) ret->file = strdup(song->file);
	if(song->artist) ret->artist = strdup(song->artist);
	if(song->album) ret->album = strdup(song->album);
	if(song->title) ret->title = strdup(song->title);
	if(song->track) ret->track = strdup(song->track);
	if(song->name) ret->name = strdup(song->name);
	if(song->date) ret->date = strdup(song->date);
	if(song->genre) ret->genre= strdup(song->genre);
	if(song->composer) ret->composer= strdup(song->composer);
	ret->time = song->time;
	ret->pos = song->pos;
	ret->id = song->id;

	return ret;
}

void mpd_initDirectory(mpd_Directory * directory) {
	directory->path = NULL;
}

void mpd_finishDirectory(mpd_Directory * directory) {
	if(directory->path) safe_free(directory->path);
}

/* This is for mpd_getNextInfoEntity_st ONLY */
static void mpd_resetDirectory_st (mpd_Directory *directory) {
	if (!directory->path)
		directory->path = malloc(MAXPATHLEN);
	directory->path[0] = '\0';
}

/* This is for mpd_getNextInfoEntity_st ONLY */
static mpd_Directory * mpd_getDirectory_st (void) {
	static mpd_Directory * ret = NULL;

	if (!ret)
		ret = mpd_newDirectory();

	mpd_resetDirectory_st(ret);

	return ret;
}

mpd_Directory * mpd_newDirectory (void) {
	mpd_Directory * directory = malloc(sizeof(mpd_Directory));;

	mpd_initDirectory(directory);

	return directory;
}

void mpd_freeDirectory(mpd_Directory * directory) {
	mpd_finishDirectory(directory);

	safe_free(directory);
}

mpd_Directory * mpd_directoryDup(mpd_Directory * directory) {
	mpd_Directory * ret = mpd_newDirectory();

	if(directory->path) ret->path = strdup(directory->path);

	return ret;
}

void mpd_initPlaylistFile(mpd_PlaylistFile * playlist) {
	playlist->path = NULL;
}

void mpd_finishPlaylistFile(mpd_PlaylistFile * playlist) {
	if(playlist->path) safe_free(playlist->path);
}

/* This is for mpd_getNextInfoEntity_st ONLY */
static void mpd_resetPlaylistFile_st (mpd_PlaylistFile *playlistFile) {
	if (!playlistFile->path)
		playlistFile->path = malloc(MAXPATHLEN);
	playlistFile->path[0] = '\0';
}

/* This is for mpd_getNextInfoEntity_st ONLY */
static mpd_PlaylistFile * mpd_getPlaylistFile_st (void) {
	static mpd_PlaylistFile * ret = NULL;

	if (!ret)
		ret = mpd_newPlaylistFile();

	mpd_resetPlaylistFile_st(ret);

	return ret;
}

mpd_PlaylistFile * mpd_newPlaylistFile(void) {
	mpd_PlaylistFile * playlist = malloc(sizeof(mpd_PlaylistFile));

	mpd_initPlaylistFile(playlist);

	return playlist;
}

void mpd_freePlaylistFile(mpd_PlaylistFile * playlist) {
	mpd_finishPlaylistFile(playlist);
	safe_free(playlist);
}

mpd_PlaylistFile * mpd_playlistFileDup(mpd_PlaylistFile * playlist) {
	mpd_PlaylistFile * ret = mpd_newPlaylistFile();

	if(playlist->path) ret->path = strdup(playlist->path);

	return ret;
}

void mpd_initInfoEntity(mpd_InfoEntity * entity) {
	entity->info.directory = NULL;
}

mpd_InfoEntity * mpd_newInfoEntity(void) {
	mpd_InfoEntity * entity = malloc(sizeof(mpd_InfoEntity));

	mpd_initInfoEntity(entity);

	return entity;
}

void mpd_freeInfoEntityInfo_st(void) {
	mpd_freeDirectory(mpd_getDirectory_st());
	mpd_freeSong(mpd_getSong_st());
	mpd_freePlaylistFile(mpd_getPlaylistFile_st());
}

unsigned int mpd_getNextInfoEntity_st(mpd_InfoEntity * entity,
		mpd_Connection * connection) {
	mpd_Song * s = NULL;
	if(connection->doneProcessing || (connection->listOks &&
			connection->doneListOk))
	{
		return 0;
	}

	if(!connection->returnElement.name[0])
		mpd_getNextReturnElement(connection);

	if(connection->returnElement.name[0]) {
		char * re_name = connection->returnElement.name;

		if(strcmp(re_name,"file")==0) {
			s = mpd_getSong_st();
			entity->type = MPD_INFO_ENTITY_TYPE_SONG;
			entity->info.song = mpd_getTmpSong_st();
			strcpy(s->file, connection->returnElement.value);
			entity->info.song->file = s->file;
		}
		else if(strcmp(re_name,"directory")==0) {
			entity->type = MPD_INFO_ENTITY_TYPE_DIRECTORY;
			entity->info.directory = mpd_getDirectory_st();
			strcpy(entity->info.directory->path,
					connection->returnElement.value);
		}
		else if(strcmp(re_name,"playlist")==0) {
			entity->type = MPD_INFO_ENTITY_TYPE_PLAYLISTFILE;
			entity->info.playlistFile = mpd_getPlaylistFile_st();
			strcpy(entity->info.playlistFile->path,
					connection->returnElement.value);
		}
		else {
			connection->error = 1;
			strcpy(connection->errorStr,"problem parsing song info");
			return 0;
		}
	}
	else return 0;

	mpd_getNextReturnElement(connection);
	while(connection->returnElement.name[0]) {
		char * re_value = connection->returnElement.value;
		char * re_name = connection->returnElement.name;

		if (!strcmp(re_name,"file") ||
				!strcmp(re_name,"directory") ||
				!strcmp(re_name,"playlist"))
			return 1;

		/* excuse the nasty CPP abuse */
#define case_str(song_tag,tmp,name) \
	if (!song_tag && !strcmp(re_name,name)) { \
		song_tag = tmp; \
		strcpy(song_tag,re_value); \
	}
#define case_int(song_tag,def_value,name) \
	if (song_tag == def_value && !strcmp(re_name,name)) \
		song_tag = atoi(re_value);

		if(entity->type == MPD_INFO_ENTITY_TYPE_SONG &&
				strlen(re_value)) {
			mpd_Song * es = entity->info.song;
			case_str(es->artist, s->artist, "Artist")
			else case_str(es->album, s->album, "Album")
			else case_str(es->title, s->title, "Title")
			else case_str(es->track, s->track, "Track")
			else case_str(es->name, s->name, "Name")
			else case_str(es->date, s->date, "Date")
			else case_str(es->genre, s->genre, "Genre")
			else case_str(es->composer, s->composer, "Composer")
			else case_int(es->time, MPD_SONG_NO_TIME, "Time")
			else case_int(es->pos, MPD_SONG_NO_NUM, "Pos")
			else case_int(es->id, MPD_SONG_NO_ID, "Id")
		}
#undef case_int
#undef case_str
#if 0
		else if(entity->type == MPD_INFO_ENTITY_TYPE_DIRECTORY) {
		}
		else if(entity->type == MPD_INFO_ENTITY_TYPE_PLAYLISTFILE) {
		}
#endif
		mpd_getNextReturnElement(connection);
	}
	return 1;
}

char * mpd_getNextReturnElementNamed(mpd_Connection * connection,
		const char * name)
{
	if(connection->doneProcessing || (connection->listOks &&
			connection->doneListOk))
	{
		return NULL;
	}

	mpd_getNextReturnElement(connection);
	while(connection->returnElement.name[0]) {
		if(strcmp(connection->returnElement.name,name)==0)
			return strdup(connection->returnElement.value);
		mpd_getNextReturnElement(connection);
	}

	return NULL;
}

char * mpd_getNextArtist(mpd_Connection * connection) {
	return mpd_getNextReturnElementNamed(connection,"Artist");
}

char * mpd_getNextAlbum(mpd_Connection * connection) {
	return mpd_getNextReturnElementNamed(connection,"Album");
}

CMD_1_int(mpd_sendPlaylistInfoCommand,"playlistinfo")
CMD_1_int(mpd_sendPlaylistIdCommand,"playlistid")
CMD_1_long_long(mpd_sendPlChangesCommand,"plchanges")

CMD_1_str(mpd_sendListallCommand,"listall")
CMD_1_str(mpd_sendListallInfoCommand,"listallinfo")
CMD_1_str(mpd_sendLsInfoCommand,"lsinfo")

CMD_0(mpd_sendCurrentSongCommand,"currentsong")

void mpd_sendSearchCommand(mpd_Connection * connection, int table,
		const char * str)
{
	char st[9]; /* "filename" is the longest string here */
	char * string, * sanitStr;
	switch (table) {
	case MPD_TABLE_ARTIST:    strcpy(st,"artist"); break;
	case MPD_TABLE_ALBUM:     strcpy(st,"album"); break;
	case MPD_TABLE_TITLE:     strcpy(st,"title"); break;
	case MPD_TABLE_FILENAME:  strcpy(st,"filename"); break;
	default:
		connection->error = 1;
		strcpy(connection->errorStr,"unknown table for search");
		return;
	}
	sanitStr = connection->errorStr;
	mpd_sanitizeArg_st(sanitStr, str);
	string = connection->buffer;
	sprintf(string,"search %s \"%s\"\n",st,sanitStr);
	mpd_executeCommand(connection,string);
}

void mpd_sendFindArtistAlbum(mpd_Connection * connection, const char *artist,
		const char *album)
{
	char * str = connection->buffer;
	if (artist == album && !album) return;
	strcpy(str, "find");
	if (artist)
		sprintf(str, "%s artist \"%s\"", str, artist);
	if (album)
		sprintf(str, "%s album \"%s\"", str, album);
	strcat(str, "\n");
	mpd_executeCommand(connection, str);
}

void mpd_sendFindCommand(mpd_Connection * connection, int table,
		const char * str)
{
	char st[7];
	char * string, * sanitStr;
	switch (table) {
	case MPD_TABLE_ARTIST:    strcpy(st,"artist"); break;
	case MPD_TABLE_ALBUM:     strcpy(st,"album"); break;
	case MPD_TABLE_TITLE:     strcpy(st,"title"); break;
	default:
		connection->error = 1;
		strcpy(connection->errorStr,"unknown table for find");
		return;
	}
	sanitStr = connection->errorStr;
	mpd_sanitizeArg_st(sanitStr, str);
	string = connection->buffer;
	sprintf(string,"find %s \"%s\"\n",st,sanitStr);
	mpd_executeCommand(connection,string);
}

void mpd_sendListCommand(mpd_Connection * connection, int table,
		const char * arg1)
{
	char st[7];
	char * string;
	switch (table) {
	case MPD_TABLE_ARTIST:    strcpy(st,"artist"); break;
	case MPD_TABLE_ALBUM:     strcpy(st,"album"); break;
	default:
		connection->error = 1;
		strcpy(connection->errorStr,"unknown table for list");
		return;
	}
	string = connection->buffer;
	if (arg1 && arg1[0]) {
		char * sanitArg1 = connection->errorStr;
		mpd_sanitizeArg_st(sanitArg1,arg1);
		sprintf(string,"list %s \"%s\"\n",st,sanitArg1);
		*sanitArg1 = '\0'; /* clear the error string again */
	} else
		sprintf(string,"list %s\n",st);
	mpd_executeCommand(connection,string);
}

CMD_1_str(mpd_sendAddCommand,"add")
CMD_1_int(mpd_sendDeleteCommand,"delete")
CMD_1_int(mpd_sendDeleteIdCommand,"deleteid")
CMD_1_str(mpd_sendSaveCommand,"save")
CMD_1_str(mpd_sendLoadCommand,"load")

CMD_1_str(mpd_sendRmCommand,"rm")

CMD_0(mpd_sendShuffleCommand,"shuffle")
CMD_0(mpd_sendClearCommand,"clear")

CMD_1_int(mpd_sendPlayCommand,"play")
CMD_1_int(mpd_sendPlayIdCommand,"playid")

CMD_0(mpd_sendStopCommand,"stop")

CMD_1_int(mpd_sendPauseCommand,"pause")

CMD_0(mpd_sendNextCommand,"next")

CMD_2_int(mpd_sendMoveCommand,"move")
CMD_2_int(mpd_sendMoveIdCommand,"moveid")

CMD_2_int(mpd_sendSwapCommand,"swap")
CMD_2_int(mpd_sendSwapIdCommand,"swapid")

CMD_2_int(mpd_sendSeekCommand,"seek")
CMD_2_int(mpd_sendSeekIdCommand,"seekid")

CMD_1_str(mpd_sendUpdateCommand,"update")

int mpd_getUpdateId(mpd_Connection * connection) {
	char * jobid;
	int ret = 0;

	jobid = mpd_getNextReturnElementNamed(connection,"updating_db");
	if(jobid) {
		ret = atoi(jobid);
		free(jobid);
	}

	return ret;
}

CMD_0(mpd_sendPrevCommand,"previous")
CMD_1_int(mpd_sendRepeatCommand,"repeat")
CMD_1_int(mpd_sendRandomCommand,"random")
CMD_1_int(mpd_sendSetvolCommand,"setvol")
CMD_1_int(mpd_sendVolumeCommand,"volume")
CMD_1_int(mpd_sendCrossfadeCommand,"crossfade")
CMD_1_str(mpd_sendPasswordCommand,"password")

void mpd_sendCommandListBegin(mpd_Connection * connection) {
	if(connection->commandList) {
		strcpy(connection->errorStr,"already in command list mode");
		connection->error = 1;
		return;
	}
	connection->commandList = COMMAND_LIST;
	mpd_executeCommand(connection,"command_list_begin\n");
}

void mpd_sendCommandListOkBegin(mpd_Connection * connection) {
	if(connection->commandList) {
		strcpy(connection->errorStr,"already in command list mode");
		connection->error = 1;
		return;
	}
	connection->commandList = COMMAND_LIST_OK;
	mpd_executeCommand(connection,"command_list_ok_begin\n");
	connection->listOks = 0;
}

void mpd_sendCommandListEnd(mpd_Connection * connection) {
	if(!connection->commandList) {
		strcpy(connection->errorStr,"not in command list mode");
		connection->error = 1;
		return;
	}
	connection->commandList = 0;
	mpd_executeCommand(connection,"command_list_end\n");
}

/* 0.12.0 commands: */
CMD_0(mpd_sendOutputsCommand,"outputs")

mpd_OutputEntity * mpd_getNextOutput(mpd_Connection * connection) {
	mpd_OutputEntity * output = NULL;

	if(connection->doneProcessing || (connection->listOks &&
			connection->doneListOk))
	{
		return NULL;
	}

	if(connection->error) return NULL;

	output = malloc(sizeof(mpd_OutputEntity));
	output->id = -10;
	output->name = NULL;
	output->enabled = 0;

	if(!connection->returnElement.name[0])
		mpd_getNextReturnElement(connection);

	while(connection->returnElement.name[0]) {
		char * re_name, * re_value;
		if(connection->error) {
			free(output);
			return NULL;
		}
		re_name = connection->returnElement.name;
		re_value = connection->returnElement.value;
		if(strcmp(re_name,"outputid")==0) {
			if(output!=NULL && output->id>=0) return output;
			output->id = atoi(re_value);
		}
		else if(strcmp(re_name,"outputname")==0) {
			output->name = strdup(re_value);
		}
		else if(strcmp(re_name,"outputenabled")==0) {
			output->enabled = atoi(re_value);
		}
		mpd_getNextReturnElement(connection);
	}

	return output;
}

CMD_1_int(mpd_sendDisableOutputCommand,"disableoutput")
CMD_1_int(mpd_sendEnableOutputCommand,"enableoutput")

void mpd_freeOutputElement(mpd_OutputEntity * output) {
	free(output->name);
	free(output);
}
