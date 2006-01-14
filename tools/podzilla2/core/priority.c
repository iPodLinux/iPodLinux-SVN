/*
 * Copyright (c) 2006 Courtney Cavin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <stdlib.h>
#include "pz.h"

#define IS_ACTIVE 2
#define IS_VITAL  4
#define IS_READY  8
#define IS_PRIME  128

#define ROUND_TIME 30000

static struct pz_Idle {
	int (*func)(void *);
	void *data;
} *idle_callback = NULL;

static ttk_timer pz_idle_timer;
static unsigned char idle_status = PZ_PRIORITY_IDLE;

/* super amazing code here */
void pz_set_priority(unsigned char st)
{ idle_status = st; }
unsigned char pz_get_priority()
{ return idle_status; }

/* boring allocation and assignment, nothing to see here, move along */
void pz_register_idle(int (*func)(void *), void *data)
{
	if (idle_callback) free(idle_callback);
	if ((idle_callback = malloc(sizeof(struct pz_Idle))) == NULL)
		return;
	idle_callback->func = func;
	idle_callback->data = data;
	pz_reset_idle_timer();
}

/* duldrum */
void pz_unregister_idle(int (*func)(void *))
{
	if (idle_callback && idle_callback->func == func) {
		free(idle_callback);
		idle_callback = NULL;
	}
}

static void pz_call_idle()
{
	if (!idle_callback) return;
	if ((* idle_callback->func)(idle_callback->data)) {
		free(idle_callback);
		idle_callback = NULL;
	}
}

/* here's where the interesting stuff is; the five upper bits of -idle_status-
 * are used as a counter of sorts, once the top bit is filled, the condition
 * IS_PRIME is met. basically this function needs to be called once before
 * IS_READY is met and five times for IS_PRIME; IS_ACTIVE is true when the
 * priority is either VITAL or ACTIVE; these conditions specify that the idle
 * function should be called after 1 round of idle in the IDLE state and after
 * 5 rounds of idle in the ACTIVE state; the idle function never gets called
 * while in the VITAL state */
static void pz_check_idle()
{
	if ((idle_status & IS_READY && !(idle_status & IS_ACTIVE)) ||
	    (idle_status & IS_PRIME && !(idle_status & IS_VITAL))) {
		pz_idle_timer = 0;
		pz_call_idle();
		idle_status &= PZ_PRIORITY_VITAL;
	}
	else {
		int i;
		for (i = 3; i < 8 && (idle_status >> i) & 1; i++);
		if (i < 8) idle_status |= 1 << i;
		pz_idle_timer = ttk_create_timer(ROUND_TIME, pz_check_idle);
	}
}

void pz_reset_idle_timer()
{
	idle_status &= PZ_PRIORITY_VITAL;
	if (pz_idle_timer)
		ttk_destroy_timer(pz_idle_timer);
	pz_idle_timer = ttk_create_timer(ROUND_TIME, pz_check_idle);
}

