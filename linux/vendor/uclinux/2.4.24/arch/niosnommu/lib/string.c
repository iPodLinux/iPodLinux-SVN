#include <linux/types.h>
#include <linux/autoconf.h>
#include <asm/string.h>

#ifdef __HAVE_ARCH_MEMSET
void * memset(void * s,int c,size_t count)
{

    if (count > 8) {
	int dummy0, dummy1, dummy2;
	__asm__ __volatile__ (
	 /* memset a char to make s word-aligned */
	 "skp1	%0, 0\n\t"
	 "br	1f\n\t"
	 "fill8 %%r0, %3\n\t"	/* delay slot */
	 "dec	%1\n\t"
	 "st8d  [%0], %%r0\n\t"
	 "inc	%0\n\t"

	 /* memset a word to make s dword-aligned */
	 "1: skp1	%0, 1\n\t"
	 "br	2f\n\t"
	 "mov	%2, %1\n\t"		/* delay slot */
	 "subi	%1, 2\n\t"
	 "st16d [%0], %%r0\n\t"
	 "addi  %0,   2\n\t"

	 /* %2 is how many dwords to set */
	 "mov	%2, %1\n\t"
	 "2:lsri	%2, 2\n\t"

	 /* store dwords */
	 "3:st	[%0], %%r0\n\t"
	 "dec	%2\n\t"
	 "skprz	%2\n\t"
	 "br	3b\n\t"
	 "addi	%0, 4\n\t"		/* delay slot */

	 /* store a word if necessary*/
	 "skp1	%1, 1\n\t"
	 "br	4f\n\t"
	 "nop\n\t"
	 "st16d [%0], %%r0\n\t"
	 "addi	%0, 2\n\t"

	 /* store a char if necessary*/
	 "4:skp0 %1, 0\n\t"
	 "st8d  [%0], %%r0\n\t"

	:"=r"(dummy0), "=r" (dummy1), "=r" (dummy2)
	:"r" (c), "0" (s), "1" (count)
	:"r0", "memory");
	}
	else {
	char* xs=(char*)s;
	while (count--)
		*xs++ = c;
	}
	return s;
}
#endif

#ifdef __HAVE_ARCH_MEMCPY
void * memcpy(void * d, const void * s, size_t count)
{
    unsigned long dst, src;
	dst = (unsigned long) d;
	src = (unsigned long) s;

	if ((count < 8) || ((dst ^ src) & 3))
	    goto restup;

	if (dst & 1) {
		*(char*)dst++=*(char*)src++;
	    count--;
	}
	if (dst & 2) {
		*(short*)dst=*(short*)src;
	    src += 2;
	    dst += 2;
	    count -= 2;
	}
	while (count > 3) {
		*(long*)dst=*(long*)src;
	    src += 4;
	    dst += 4;
	    count -= 4;
	}

    restup:
	while (count--)
		*(char*)dst++=*(char*)src++;

	return d;
} 
#endif

#ifdef __HAVE_ARCH_MEMMOVE
void * memmove(void * d, const void * s, size_t count)
{
    unsigned long dst, src;

    if (d < s) {
	dst = (unsigned long) d;
	src = (unsigned long) s;

	if ((count < 8) || ((dst ^ src) & 3))
	    goto restup;

	if (dst & 1) {
		*(char*)dst++=*(char*)src++;
	    count--;
	}
	if (dst & 2) {
		*(short*)dst=*(short*)src;
	    src += 2;
	    dst += 2;
	    count -= 2;
	}
	while (count > 3) {
		*(long*)dst=*(long*)src;
	    src += 4;
	    dst += 4;
	    count -= 4;
	}

    restup:
	while (count--)
		*(char*)dst++=*(char*)src++;
    } else {
	dst = (unsigned long) d + count;
	src = (unsigned long) s + count;

	if ((count < 8) || ((dst ^ src) & 3))
	    goto restdown;

	if (dst & 1) {
	    src--;
	    dst--;
	    count--;
		*(char*)dst=*(char*)src;
	}
	if (dst & 2) {
	    src -= 2;
	    dst -= 2;
	    count -= 2;
		*(short*)dst=*(short*)src;
	}
	while (count > 3) {
	    src -= 4;
	    dst -= 4;
	    count -= 4;
		*(long*)dst=*(long*)src;
	}

    restdown:
	while (count--) {
	    src--;
	    dst--;
		*(char*)dst=*(char*)src;
	}
    }

    return d;	

}
#endif