/* memset
   This implementation is in the public domain.  */

#define size_t unsigned long

unsigned char *memset(unsigned char *dest, unsigned char val, unsigned int len)
{
  register unsigned char *ptr = (unsigned char*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}
