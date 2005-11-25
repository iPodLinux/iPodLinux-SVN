#ifndef _IPODHW_H_
#define _IPODHW_H_

void   ipod_wait_usec(uint32 usecs);
uint32 ipod_get_hwrev(void);
int    timer_get_current();
int    timer_check(int clock_start, int usecs);

#endif
