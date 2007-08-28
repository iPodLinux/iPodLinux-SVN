
static void bcopy (char *src, char *dest, int len)
{
  if (dest < src)
    while (len--)
    {
	*dest = *src;
	dest++;
	src++;
    }
  else
    {
      char *lasts = src + (len-1);
      char *lastd = dest + (len-1);
      while (len--)
        *(char *)lastd-- = *(char *)lasts--;
    }
}

unsigned char *memcpy(unsigned char *out, unsigned char *in, unsigned int length)
{
    bcopy(in, out, length);
    return out;
}
