/*
 * Optimized IO string functions.
 */


#include <asm/io.h>


void insw(unsigned long port, void *dst, unsigned long count)
{
	if (count > 8) {
		/* Long word align buffer ptr */
		if	((unsigned long)dst & 2) {
			*(((unsigned short *)dst)++) = inw(port);
			count--;
		}

		/* Input pairs of short and store as longs */
		while (count >= 8) {
			*(((unsigned long *)dst)++) = inw(port) + (inw(port) << 16);
			*(((unsigned long *)dst)++) = inw(port) + (inw(port) << 16);
			*(((unsigned long *)dst)++) = inw(port) + (inw(port) << 16);
			*(((unsigned long *)dst)++) = inw(port) + (inw(port) << 16);
			count -= 8;
		}
	}

	/* Input remaining shorts */
	while (count--)
		*(((unsigned short *)dst)++) = inw(port);
}


void outsw(unsigned long port, void *src, unsigned long count)
{
	unsigned int lw;

	if (count > 8) {
		/* Long word align buffer ptr */
		if	((unsigned long)src & 2) {
			outw( *(((unsigned short *)src)++), port );
			count--;
		}

		/* Read long words and output as pairs of short */
		while (count >= 8) {
			lw = *(((unsigned long *)src)++);
			outw(lw, port);
			outw((lw >> 16), port);
			lw = *(((unsigned long *)src)++);
			outw(lw, port);
			outw((lw >> 16), port);
			lw = *(((unsigned long *)src)++);
			outw(lw, port);
			outw((lw >> 16), port);
			lw = *(((unsigned long *)src)++);
			outw(lw, port);
			outw((lw >> 16), port);
			count -= 8;
		}
	}

	/* Output remaining shorts */
	while (count--) 
		outw( *(((unsigned short *)src)++), port );
}
