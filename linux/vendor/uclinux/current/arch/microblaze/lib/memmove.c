/* bcopy -- copy memory regions of arbitary length

NAME
	bcopy -- copy memory regions of arbitrary length

SYNOPSIS
	void bcopy (char *in, char *out, int length)

DESCRIPTION
	Copy LENGTH bytes from memory region pointed to by IN to memory
	region pointed to by OUT.

BUGS
	Significant speed improvements can be made in some cases by
	implementing copies of multiple bytes simultaneously, or unrolling
	the copy loop.

*/

static void bcopy (register char *src, register char * dest, int len)
{
  if (dest < src)
    while (len--)
      *dest++ = *src++;
  else
    {
      char *lasts = src + (len-1);
      char *lastd = dest + (len-1);
      while (len--)
        *(char *)lastd-- = *(char *)lasts--;
    }
}

void *memmove (void *s1, void *s2, int n)
{
  bcopy ((char *)s2, (char *)s1, n);
  return s1;
}
