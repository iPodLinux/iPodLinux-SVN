#ifndef _M68K_PARAM_H
#define _M68K_PARAM_H

#ifndef HZ
#if defined(CONFIG_HW_FEITH)
#define HZ 1000
#else
#define HZ 100
#endif

#define EXEC_PAGESIZE	8192

#ifndef NGROUPS
#define NGROUPS		32
#endif

#ifndef NOGROUP
#define NOGROUP		(-1)
#endif

#define MAXHOSTNAMELEN	64	/* max length of hostname */

#ifdef __KERNEL__
# define CLOCKS_PER_SEC	HZ	/* frequency at which times() counts */
#endif

#endif /* _M68K_PARAM_H */
