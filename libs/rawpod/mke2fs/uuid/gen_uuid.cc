/*
 * gen_uuid.c --- generate a DCE-compatible uuid
 *
 * Copyright (C) 1996, 1997, 1998, 1999 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU 
 * Library General Public License.
 * %End-Header%
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "uuidP.h"

#ifdef HAVE_SRANDOM
#define srand(x) 	srandom(x)
#define rand() 		random()
#endif


/*
 * Generate a series of random bytes.  Use /dev/urandom if possible,
 * and if not, use srandom/random.
 */
static void get_random_bytes(void *buf, int nbytes)
{
	int i;
	unsigned char *cp = (unsigned char *) buf;

        srand ((unsigned) time (0));

	/*
	 * We do this all the time, but this is the only source of
	 * randomness if /dev/random/urandom is out to lunch.
	 */
	for (cp = (u8 *)buf, i = 0; i < nbytes; i++)
		*cp++ ^= (rand() >> 7) & 0xFF;
	return;
}

/*
 * Get the ethernet hardware address, if we can find it...
 */
static int get_node_id(unsigned char *node_id)
{
	return 0;
}

/* Assume that the gettimeofday() has microsecond granularity */
#define MAX_ADJUSTMENT 10

static int get_clock(unsigned int *clock_high, unsigned int *clock_low, unsigned short *ret_clock_seq)
{
    struct {
        unsigned int tv_sec;
        unsigned int tv_usec;
    } tv;
    
    unsigned short clock_seq;
    unsigned long long clock_reg;

    tv.tv_sec = time (0);
    tv.tv_usec = rand();
    get_random_bytes (&clock_seq, sizeof(clock_seq));
		
    clock_reg = tv.tv_usec*10;
    clock_reg += ((unsigned long long) tv.tv_sec)*10000000;
    clock_reg += (((unsigned long long) 0x01B21DD2) << 32) + 0x13814000;

    *clock_high = clock_reg >> 32;
    *clock_low = clock_reg;
    *ret_clock_seq = clock_seq;
    return 0;
}

void uuid_generate_time(uuid_t out)
{
	static unsigned char node_id[6];
	static int has_init = 0;
	struct uuid uu;
	unsigned int	clock_mid;

	if (!has_init) {
		if (get_node_id(node_id) <= 0) {
			get_random_bytes(node_id, 6);
			/*
			 * Set multicast bit, to prevent conflicts
			 * with IEEE 802 addresses obtained from
			 * network cards
			 */
			node_id[0] |= 0x01;
		}
		has_init = 1;
	}
	get_clock(&clock_mid, &uu.time_low, &uu.clock_seq);
	uu.clock_seq |= 0x8000;
	uu.time_mid = (unsigned short) clock_mid;
	uu.time_hi_and_version = (clock_mid >> 16) | 0x1000;
	memcpy(uu.node, node_id, 6);
	uuid_pack(&uu, out);
}

void uuid_generate_random(uuid_t out)
{
	uuid_t	buf;
	struct uuid uu;

	get_random_bytes(buf, sizeof(buf));
	uuid_unpack(buf, &uu);

	uu.clock_seq = (uu.clock_seq & 0x3FFF) | 0x8000;
	uu.time_hi_and_version = (uu.time_hi_and_version & 0x0FFF) | 0x4000;
	uuid_pack(&uu, out);
}

/*
 * This is the generic front-end to uuid_generate_random and
 * uuid_generate_time.  It uses uuid_generate_random only if
 * /dev/urandom is available, since otherwise we won't have
 * high-quality randomness.
 */
void uuid_generate(uuid_t out)
{
    uuid_generate_time(out);
}
