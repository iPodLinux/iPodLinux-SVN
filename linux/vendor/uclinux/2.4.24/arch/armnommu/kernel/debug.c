#include "linux/config.h"

static unsigned long *out_size;
static unsigned long out_pos;

//#define AT91_DEBUG_BASE 0x1400000L

void at91_debug_init()
{
  at91_debug_init_adr(AT91_DEBUG_BASE);
}

void at91_debug_init_adr(unsigned long adr)
{
  out_size =  (unsigned long*) adr;
  *out_size = 0;
  out_pos=0;
  at91_debug_str("AT91 debugging initialized\n");
}

void at91_debug_str(char *s)
{
char *p;

  p = (char*)  out_size+4;
  p += out_pos;
  while (*s){ 
        *p++ = *s++;
        out_pos++;
  } 
  *out_size = (out_pos+3)>>2;   // nr. of words
  *p++='\n';
  *p++='\n';
  *p++='\n';
}

void at91_debug_putn(char *s,int n)
{
char v[2];
v[1]=0;
  while(n--) {
	v[0]=*s++; at91_debug_str(v);
  }
}

at91_mark_a()
{
  at91_debug_str("mark A\n");
}


void at91_sv(long x, long *px)
{
*px=x;
}

