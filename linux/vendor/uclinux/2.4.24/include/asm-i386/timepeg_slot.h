#ifndef _TIMEPEG_SLOT_H_
#define _TIMEPEG_SLOT_H_

#include <linux/spinlock.h>	/* For spinlock_t */
#include <linux/threads.h>	/* For NR_CPUS */

/*
 * A 64 bit counter
 */

typedef unsigned long long timepeg_t;

/*
 * The system-wide list of timepeg slots constitutes a directed graph.
 * This is because a particular slot may have any number of predecessors.
 * We don't attempt to record all possible predecessors.  We record a finite number.
 * If a timepeg slot has more than TIMEPEG_SLOT_PREDS predecessors then
 * XXXX drops some info on the floor
 */

#define TIMEPEG_NR_PREDS	16

/* Mode bits */
#define TIMEPEG_MODE_INITIALISED	1	/* It has been constructed */
#define TIMEPEG_MODE_FULL		2	/* All timepeg arcs in preds[] are used */
#define TIMEPEG_MODE_DEAD		4	/* Flag: something wrong - ignore this timepeg
						 * from now on.
						 */

struct timepeg_slot
{
	struct timepeg_slot *next;		/* Singly linked */
	const char *name;			/* Human readable identifier */
	short line;				/* File line number */
	short mode_bits;			/* See above */
	spinlock_t lock;

	struct timepeg_slot *directed_pred;	/* If it's a directed timepeg, this is the predecessor */
	const char *directee_name;		/* Name of the timepeg we're directed at */

	/* We logically separate each CPU */
	struct timepeg_slot_percpu
	{
		timepeg_t departure_tp;		/* Most recent time when we hit this timepeg */

		/*
		 * A simple array of arcs to predecessors
		 * Each entry represents a predecessor.  That is, a most-recently-called
		 * TIMEPEG().
		 */
		struct timepeg_arc
		{
			/* This points at the most-recently-called timepeg */
			struct timepeg_slot *timepeg;

			/* Accumulated time on this arc */
			timepeg_t acc_tp;

			/* Best and worst transit times */
			timepeg_t min_tp, max_tp;

			/* The number of times this arc was used */
			unsigned long nr_times;
		} preds[TIMEPEG_NR_PREDS];
	} percpu[NR_CPUS];
};

#endif	/* _TIMEPEG_SLOT_H_ */

