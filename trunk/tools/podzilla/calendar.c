/*
 * acalendar.c, a simple calendar for AV3xx
 *
 * Copyright 2004, Goetz Minuth
 * Copyright 2004, Bernard Leach, ported to iPod
 *
 * This File is free software; I give unlimited permission to copy and/or
 * distribute it, with or without modifications, as long as this notice is
 * preserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "pz.h"
#include "piezo.h"

#define DaySpace  23
#define WeekSpace 15

#define XCALENDARPOS 2
#define YCALENDARPOS 12

struct today {
	int mday;		/* day of the month */
	int mon;		/* month */
	int year;		/* year since 1900 */
	int wday;		/* day of the week */
};

struct shown {
	int mday;		/* day of the month */
	int mon;		/* month */
	int year;		/* year since 1900 */
	int wday;		/* day of the week */
	int firstday;		/* first (w)day of month */
	int lastday;		/* last (w)day of month */
};

struct today today;
static int leap_year;
struct shown shown;
static int last_mday;

static void clear_calendar();

static int days_in_month[2][13] = {
	{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
};

static char *month_name[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static GR_WINDOW_ID calendar_wid;
static GR_GC_ID calendar_gc;
static GR_SCREEN_INFO screen_info;

/*
 * leap year -- account for gregorian reformation in 1752
 */
static int is_leap_year(int yr);

/*
 * searches the weekday of the first day in month, relative to the given
 * values
 */
static int calc_weekday(struct shown *shown);
static void calendar_init();
static void draw_headers(void);
static void calendar_draw();

static void next_month(struct shown *shown, int step);
static void prev_month(struct shown *shown, int step);

static int
calendar_do_keystroke(GR_EVENT * event)
{
	int ret = 0;
	last_mday = shown.mday;
	switch (event->keystroke.ch) {
	case 'w':
		prev_month(&shown, 0);
		ret = 1;
		break;

	case 'f':
		next_month(&shown, 0);
		ret = 1;
		break;

	case 'm':
		pz_close_window(calendar_wid);
		ret = 1;
		break;

	case 'r':
		shown.mday++;
		if (shown.mday > days_in_month[leap_year][shown.mon])
			next_month(&shown, 1);
		else {
			calendar_draw(0);
		}
		ret = 1;
		break;

	case 'l':
		shown.mday--;
		if (shown.mday < 1)
			prev_month(&shown, 1);
		else {
			calendar_draw(0);
		}
		ret = 1;
		break;
	}

	return ret;
}


/*
 * leap year -- account for gregorian reformation in 1752
 */
static int
is_leap_year(int yr)
{
	return ((yr) <= 1752 ? !((yr) % 4) :
		(!((yr) % 4) && ((yr) % 100)) || !((yr) % 400)) ? 1 : 0;
}

/*
 * searches the weekday of the first day in month, relative to the given
 * values
 */
static int
calc_weekday(struct shown *shown)
{
	return (shown->wday + 36 - shown->mday) % 7;
}


static void
calendar_init()
{
	time_t now;
	struct tm *tm;

	time(&now);
	tm = localtime(&now);

	today.wday = tm->tm_wday - 1;
	today.year = tm->tm_year + 1900;
	today.mon = tm->tm_mon + 1;
	today.mday = tm->tm_mday;

	shown.mday = today.mday;
	shown.mon = today.mon;
	shown.year = today.year;
	shown.wday = today.wday;

	shown.firstday = calc_weekday(&shown);
	leap_year = is_leap_year(shown.year);
}

static void
draw_headers(void)
{
	int i;
	char *Dayname[8] =
	    { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su", "" };
	int ws = XCALENDARPOS;

	for (i = 0; i < 8;) {
		GrSetGCForeground(calendar_gc, BLACK);
		GrText(calendar_wid, calendar_gc, ws + 2,
		       YCALENDARPOS, Dayname[i++], -1,
		       GR_TFASCII);
		ws += DaySpace;
	}
}

static void
calendar_do_draw(GR_EVENT * event)
{
	calendar_draw(1);
}

static void
calendar_draw(int redraw)
{
	int ws, row, pos, days_per_month, j;
	char buffer[9];

	if(redraw) {
		clear_calendar();

		GrSetGCForeground(calendar_gc, WHITE);
		GrFillRect(calendar_wid, calendar_gc, XCALENDARPOS, YCALENDARPOS,
			screen_info.cols - XCALENDARPOS, 2 * WeekSpace);

		snprintf(buffer, 9, "%s %04d", month_name[shown.mon - 1], shown.year);
		pz_draw_header(buffer);

		draw_headers();

		if (shown.firstday > 6) {
			shown.firstday -= 7;
		}
	}
	row = 1;
	pos = shown.firstday;
	days_per_month = days_in_month[leap_year][shown.mon];
	ws = XCALENDARPOS + (pos * DaySpace);

	for (j = 1; j <= days_per_month; j++) {
		snprintf(buffer, 4, "%02d", j);

		if ((pos == 5) || (pos == 6)) {
			// Wochenende --> Tage in rot schreiben
			GrSetGCForeground(calendar_gc, GRAY);
		}
		else {
			GrSetGCForeground(calendar_gc, BLACK);
		}

		if(j == shown.mday) {
			// Den selektierten Tag besonders farblich
			// markieren -->
			// höchste Prio der Farben
			GrSetGCForeground(calendar_gc, LTGRAY);
			GrText(calendar_wid, calendar_gc, ws + 2, YCALENDARPOS + 2 +
				row * WeekSpace, buffer, -1, GR_TFASCII);
		}
		if ((j == today.mday) && (shown.mon == today.mon)
			&& (shown.year == today.year)) {
			GrSetGCForeground(calendar_gc, GRAY);
		}
		if(redraw) {
			GrText(calendar_wid, calendar_gc, ws + 2, YCALENDARPOS + 2 +
				row * WeekSpace, buffer, -1, GR_TFASCII);
		}
		if(j == last_mday) {
			GrText(calendar_wid, calendar_gc, ws + 2, YCALENDARPOS + 2 +
				row * WeekSpace, buffer, -1, GR_TFASCII);
		}
		if (shown.mday == j) {
			shown.wday = pos;
		}
		ws += DaySpace;
		pos++;
		if (pos >= 7) {
			row++;
			pos = 0;
			ws = XCALENDARPOS;
		}
	}

	shown.lastday = pos;
}

static void
next_month(struct shown *shown, int step)
{
	shown->mon++;
	if (shown->mon > 12) {
		shown->mon = 1;
		shown->year++;
		leap_year = is_leap_year(shown->year);
	}
	if (step > 0) {
		//shown->mday = shown->mday - days_in_month[leap_year][shown->mon - 1];
		shown->mday = 1;
	}
	else if (shown->mday > days_in_month[leap_year][shown->mon]) {
		shown->mday = days_in_month[leap_year][shown->mon];
	}
	shown->firstday = shown->lastday;
	calendar_draw(1);
}

static void
prev_month(struct shown *shown, int step)
{
	shown->mon--;
	if (shown->mon < 1) {
		shown->mon = 12;
		shown->year--;
		leap_year = is_leap_year(shown->year);
	}
	if (step > 0) {
		shown->mday = shown->mday + days_in_month[leap_year][shown->mon];
	}
	else if (shown->mday > days_in_month[leap_year][shown->mon]) {
		shown->mday = days_in_month[leap_year][shown->mon];
	}
	shown->firstday += 7 - (days_in_month[leap_year][shown->mon] % 7);
	calendar_draw(1);
}

static void
clear_calendar()
{
	GrSetGCForeground(calendar_gc, WHITE);
	GrFillRect(calendar_wid, calendar_gc, XCALENDARPOS, YCALENDARPOS - 10, 170, 120);
}

void
new_calendar_window(void)
{
	calendar_init();

	GrGetScreenInfo(&screen_info);

	calendar_gc = GrNewGC();
	GrSetGCUseBackground(calendar_gc, GR_FALSE);
	GrSetGCForeground(calendar_gc, BLACK);

	calendar_wid =
	    pz_new_window(0, HEADER_TOPLINE + 1, screen_info.cols,
			  screen_info.rows - (HEADER_TOPLINE + 1),
			  calendar_do_draw, calendar_do_keystroke);

	GrSelectEvents(calendar_wid, GR_EVENT_MASK_EXPOSURE | GR_EVENT_MASK_KEY_UP | GR_EVENT_MASK_KEY_DOWN);

	GrMapWindow(calendar_wid);
}
