#ifndef _KEYPAD_H_
#define _KEYPAD_H_

/* bitmasks for 4g+; SCRL and SCRR are my own invention. */
#define IPOD_KEYPAD_SCRL   0x80
#define IPOD_KEYPAD_SCRR   0x40
#define IPOD_KEYPAD_HOLD   0x20
#define IPOD_KEYPAD_MENU   0x10
#define IPOD_KEYPAD_PLAY   0x08
#define IPOD_KEYPAD_PREV   0x04
#define IPOD_KEYPAD_NEXT   0x02
#define IPOD_KEYPAD_ACTION 0x01

/* buttons returned by keypad_getkey() */
#define IPOD_KEY_NONE    0
#define IPOD_KEY_SELECT  1
#define IPOD_KEY_FWD     2
#define IPOD_KEY_REW     3
#define IPOD_KEY_PLAY    4
#define IPOD_KEY_MENU    5

int  keypad_getkey(void);
int  keypad_getstate(void);
void keypad_init(void);

#endif
