/*
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

#ifndef __ASM_ARCH_KEYBOARD_H__
#define __ASM_ARCH_KEYBOARD_H__

extern void ipodkb_init_hw(void);

/* drivers/char/keyboard.c */
#define kbd_setkeycode(sc,kc)		(-EINVAL)
#define kbd_getkeycode(sc)		(-EINVAL)
#define kbd_translate(sc,kcp,rm)	({ *(kcp) = (sc); 1; })
#define kbd_unexpected_up(kc)		(0200)
#define kbd_leds(leds)
#define kbd_init_hw()			ipodkb_init_hw()
#define kbd_enable_irq()
#define kbd_disable_irq()

#endif

