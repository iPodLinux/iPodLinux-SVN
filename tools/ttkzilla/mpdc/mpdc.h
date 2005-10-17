#ifndef _MPDC_H_
#define _MPDC_H_

#include "libmpdclient.h"

extern mpd_Connection *mpdz;

/* mpdc.c */
void mpdc_change_volume(mpd_Connection *con_fd, int volume);
void mpdc_next(mpd_Connection *con_fd);
void mpdc_prev(mpd_Connection *con_fd);
void mpdc_playpause(mpd_Connection *con_fd);
void mpd_destroy_connection(mpd_Connection *con_fd, char *err);
mpd_Connection *mpd_init_connection(void);
int mpdc_status(mpd_Connection *con_fs);
void mpdc_init();
void mpdc_destroy();
int mpdc_tickle();

/* player.c */
void mpd_currently_playing(void);

#endif /* _MPDC_H_ */
