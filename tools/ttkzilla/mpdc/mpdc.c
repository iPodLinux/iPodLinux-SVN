/*
 * Copyright (C) 2005 Courtney Cavin <courtc@ipodlinux.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "../pz.h"
#include "mpdc.h"

mpd_Connection *mpdz = NULL;

int mpdc_status(mpd_Connection *con_fd)
{
	mpd_Status status;
	status.error = NULL;
	if (mpdz == NULL)
		return -2;
	mpd_sendStatusCommand(con_fd);
	if (mpd_getStatus_st(&status, con_fd)) {
		mpd_finishCommand(con_fd);
		return status.state;
	}
	mpd_finishCommand(con_fd);
	return -1;
}

void mpdc_change_volume(mpd_Connection *con_fd, int volume)
{
	if (mpdc_tickle() < 0)
		return;
	mpd_sendSetvolCommand(con_fd, volume);
	mpd_finishCommand(con_fd);
}

void mpdc_next(mpd_Connection *con_fd)
{
	if (mpdc_tickle() < 0)
		return;
	mpd_sendNextCommand(con_fd);
	mpd_finishCommand(con_fd);
}

void mpdc_prev(mpd_Connection *con_fd)
{
	if (mpdc_tickle() < 0)
		return;
	mpd_sendPrevCommand(con_fd);
	mpd_finishCommand(con_fd);
}

void mpdc_playpause(mpd_Connection *con_fd)
{
	int state;
	if (mpdc_tickle() < 0)
		return;
	if ((state = mpdc_status(con_fd)) < 0)
		return;
	if (state == MPD_STATUS_STATE_PLAY)
		mpd_sendPauseCommand(con_fd, 1);
	else if (state == MPD_STATUS_STATE_PAUSE)
		mpd_sendPauseCommand(con_fd, 0);
	else
		mpd_sendPlayCommand(con_fd, -1);
	mpd_finishCommand(con_fd);
}

mpd_Connection *mpd_init_connection()
{
	static mpd_Connection con_fd;
	char *hostname = getenv("MPD_HOST");
 	char *port = getenv("MPD_PORT");
 
 	if (hostname == NULL)
 		hostname = strdup("127.0.0.1");
 	if (port == NULL)
 		port = strdup("6600");
	
	mpd_newConnection_st(&con_fd, hostname, atoi(port), 16);

	free(hostname);
	free(port);

	if (con_fd.error) {
		return NULL;
	}

	return &con_fd;
}

void mpdc_init()
{
	if ((mpdz = mpd_init_connection()) == NULL) {
		pz_error("Unable to connect to MPD");
	}
}

void mpdc_destroy()
{
	mpd_closeConnection(mpdz);
	mpdz = NULL;
}

/*  0 - connected
 *  1 - was error, connection restored
 * -1 - unable to connect to MPD
 */ 
int mpdc_tickle()
{
	char err = 0;

	if (mpdz == NULL) {
		mpdc_init();
	} else if (mpdz->error) {
		pz_error(mpdz->errorStr);
		mpd_clearError(mpdz);
		err = 1;
	}

	if (mpdc_status(mpdz) == -1) {
		mpdc_destroy();
		mpdc_init();
		if (mpdc_status(mpdz) < 0) {
			if (!err) { /* prevent new_message_window segfault */
				pz_error("Unable to determine MPD status.");
			}
			return -1;
		}
		return 1;
	}

	return (mpdz == NULL) ? -1 : err;
}
