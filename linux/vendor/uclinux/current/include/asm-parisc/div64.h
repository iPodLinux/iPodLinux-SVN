#ifndef __ASM_PARISC_DIV64
#define __ASM_PARISC_DIV64

#ifdef __LP64__
#include <asm-generic/div64.h>
#else
/*
 * unsigned long long division.  Yuck Yuck!  What is Linux coming to?
 * This is 100% disgusting
 */
#define do_div(n,base)							\
({									\
	unsigned long __low, __low2, __high, __rem;			\
	__low  = (n) & 0xffffffff;					\
	__high = (n) >> 32;						\
	if (__high) {							\
		__rem   = __high % (unsigned long)(base);		\
		__high  = __high / (unsigned long)(base);		\
		__low2  = __low >> 16;					\
		__low2 += __rem << 16;					\
		__rem   = __low2 % (unsigned long)(base);		\
		__low2  = __low2 / (unsigned long)(base);		\
		__low   = __low & 0xffff;				\
		__low  += __rem << 16;					\
		__rem   = __low  % (unsigned long)(base);		\
		__low   = __low  / (unsigned long)(base);		\
		(n) = __low  + ((long long)__low2 << 16) +		\
			((long long) __high << 32);			\
	} else {							\
		__rem = __low % (unsigned long)(base);			\
		(n) = (__low / (unsigned long)(base));			\
	}								\
	__rem;								\
})
#endif

#endif

