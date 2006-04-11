#include "bootloader.h"
#include "minilibc.h"
#include "ipodhw.h"
#define RTC inl(0x60005010)

#define DPRINTF(args...)

static ipod_t *ipod;

static void opto_i2c_init(void)
{
	int i, curr_value;

	/* wait for value to settle */
	i = 1000;
	curr_value = (inl(0x7000c104) << 16) >> 24;
	while (i > 0)
	{
		int new_value = (inl(0x7000c104) << 16) >> 24;

		if (new_value != curr_value) {
			i = 10000;
			curr_value = new_value;
		}
		else {
			i--;
		}
	}

	outl(inl(0x6000d024) | 0x10, 0x6000d024);	/* port B bit 4 = 1 */

	outl(inl(0x6000600c) | 0x10000, 0x6000600c);	/* dev enable */
	outl(inl(0x60006004) | 0x10000, 0x60006004);	/* dev reset */
        int x;
        for (x = 0; x < 100; x++) {
        	__asm__ ("mov r0, r0"); /* ~5us delay */
        }
	outl(inl(0x60006004) & ~0x10000, 0x60006004);	/* dev reset finish */

	outl(0xffffffff, 0x7000c120);
	outl(0xffffffff, 0x7000c124);
	outl(0xc00a1f00, 0x7000c100);
	outl(0x1000000, 0x7000c104);
}

int keypad_getstate_opto(void) {
  uint32 reg,in,st,touch,button = 0;
  static int wheelloc = -1;
  static int lasttouch = 0;

  if (lasttouch && ((RTC - lasttouch) > 500000)) {
    DPRINTF ("trelease %d\n", wheelloc);
    lasttouch = 0;
    wheelloc = -1;
  }

  reg = 0x7000c104;
  if (inl (reg) & 0x4000000) {
    reg += 0x3C;                /* 0x7000c140 */
    in = inl(0x7000C140);
    if ((in & 0x800000ff) == 0x8000001a) {
      outl (0x0, 0x7000c140);     /* clear interrupt status? */
      st = in & 0x40000000;
      touch = (in >> 16) & 0x7f;
      button = (in >> 8) & 0x1f;
      
      // Do scroll stuff
      if (st) {
        int adjtouch = touch;
        if (touch > wheelloc) {
          if ((touch - wheelloc) > ((96 + wheelloc - touch)))
            adjtouch -= 96;
        } else {
          if ((wheelloc - touch) > ((96 + touch - wheelloc)))
            adjtouch += 96;
        }

        if (wheelloc == -1) {
          DPRINTF ("touch %d\n", touch);
          wheelloc = touch;
          lasttouch = RTC;
        } else if ((adjtouch - wheelloc) >= 5) {
          DPRINTF ("right %d->%d(%d)\n", wheelloc, touch, adjtouch);
          wheelloc = touch;
          lasttouch = RTC;
          button |= IPOD_KEYPAD_SCRR;
        } else if ((adjtouch - wheelloc) <= -5) {
          DPRINTF ("left %d->%d(%d)\n", wheelloc, touch, adjtouch);
          wheelloc = touch;
          lasttouch = RTC;
          button |= IPOD_KEYPAD_SCRL;
        } else if (wheelloc != touch) {
          DPRINTF ("tup %d\n", touch);
          lasttouch = RTC;
        }
      } else {
        DPRINTF ("release %d\n", wheelloc);
        wheelloc = -1;
      }
    } else if (in == 0xffffffff) {
      opto_i2c_init();
    }
  }

  if ((inl (reg) & 0x80000000) != 0) {
    outl (0xffffffff, 0x7000c120);
    outl (0xffffffff, 0x7000c124);
  }
    
  /* IAK? */
  outl(0,0x7000C180);
  
  outl(inl(0x7000C104) | 0xC0000000, 0x7000C104 );
  outl(0x400A1F00, 0x7000C100);
  
  outl(inl(0x6000D024) | 0x10, 0x6000D024 );

  return(button);
}

int keypad_getstate(void) {
  uint8 state;
  uint8 ret = 0;

  if( (ipod->hw_rev>>16) >= 5 ) return( keypad_getstate_opto() );
  
  if( (ipod->hw_rev>>16) < 4 ) {
    state = inb(0xCF000030);
    if ((state & 0x10) == 0) ret |= IPOD_KEYPAD_MENU;
    if ((state & 0x08) == 0) ret |= IPOD_KEYPAD_PREV;
    if ((state & 0x04) == 0) ret |= IPOD_KEYPAD_PLAY;
    if ((state & 0x02) == 0) ret |= IPOD_KEYPAD_ACTION;
    if ((state & 0x01) == 0) ret |= IPOD_KEYPAD_NEXT;
  } else {  /* 0x4 */
    state = inb(0x6000d030);
    if ((state & 0x10) == 0) ret |= IPOD_KEYPAD_PREV;
    if ((state & 0x08) == 0) ret |= IPOD_KEYPAD_NEXT;
    if ((state & 0x04) == 0) ret |= IPOD_KEYPAD_PLAY;
    if ((state & 0x02) == 0) ret |= IPOD_KEYPAD_MENU;
    if ((state & 0x01) == 0) ret |= IPOD_KEYPAD_ACTION;
  }
  return ret;
}

void keypad_init(void) {
  ipod = ipod_get_hwinfo();

  if( (ipod->hw_rev>>16) < 4 ) {
    /* 1G -> 3G Keyboard init */
    outb(~inb(0xcf000030), 0xcf000060);
    outb(inb(0xcf000040), 0xcf000070);
    
    outb(inb(0xcf000004) | 0x1, 0xcf000004);
    outb(inb(0xcf000014) | 0x1, 0xcf000014);
    outb(inb(0xcf000024) | 0x1, 0xcf000024);
    
    outb(0xff, 0xcf000050);
  } else if( (ipod->hw_rev>>16) == 4 ) {
    /* mini keyboard init */
    outl(inl(0x6000d000) | 0x3f, 0x6000d000);
    outl(inl(0x6000d010) & ~0x3f, 0x6000d010);
  } else if( (ipod->hw_rev>>16) >= 5 ) {
    opto_i2c_init();
    /* IAK? */
    outl(0,0x7000C180);
    
    outl( inl(0x7000C104) | 0xC0000000, 0x7000C104 );
    outl(0x400A1F00, 0x7000C100);
    
    outl( inl(0x6000D024) | 0x10, 0x6000D024 );
  }
}

int keypad_getkey(void) 
{
  static int oldstate = 0;
  static int downs = 0;
  int newstate = keypad_getstate();
  int changes = oldstate ^ newstate;
  int bit;

  if (newstate == oldstate) return 0;

  for (bit = 1; bit < 0x20; bit <<= 1) {
    if ((changes & bit) && (newstate & bit)) {
      downs |= bit;
    }
  }

  DPRINTF ("O/N/C: %x %x %x\n", oldstate, newstate, changes);

  oldstate = newstate & 0x1f;

  if ((newstate & IPOD_KEYPAD_SCRR) || (downs & IPOD_KEYPAD_PLAY) || (downs & IPOD_KEYPAD_NEXT)) {
    downs &= ~(IPOD_KEYPAD_PLAY | IPOD_KEYPAD_NEXT);
    return IPOD_KEY_DOWN;
  }
  if ((newstate & IPOD_KEYPAD_SCRL) || (downs & IPOD_KEYPAD_MENU) || (downs & IPOD_KEYPAD_PREV)) {
    downs &= ~(IPOD_KEYPAD_MENU | IPOD_KEYPAD_PREV);
    return IPOD_KEY_UP;
  }
  if (downs & IPOD_KEYPAD_ACTION) {
    downs &= ~IPOD_KEYPAD_ACTION;
    return IPOD_KEY_SELECT;
  }
  return IPOD_KEY_NONE;
}
