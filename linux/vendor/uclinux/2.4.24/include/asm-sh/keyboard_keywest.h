#ifndef __ASM_SH_KEYBOARD_KEYWEST_H
#define __ASM_SH_KEYBOARD_KEYWEST_H

#ifdef __KERNEL__

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <asm/io.h>
#ifdef CONFIG_SH_KEYWEST
#include <asm/hitachi_keywest.h>
#endif
#ifdef CONFIG_SH_HD64465
#include <asm/hd64465.h>
#endif

#ifdef  CONFIG_SH_HD64465
#define KEYBOARD_IRQ (HD64465_IRQBASE+15)
#define AUX_IRQ      (HD64465_IRQBASE+7)
#else
#define KEYBOARD_IRQ 1
#define AUX_IRQ      12
#endif

#define DISABLE_KBD_DURING_INTERRUPTS 0

extern int  pckbd_setkeycode(unsigned int, unsigned int);
extern int  pckbd_getkeycode(unsigned int);
extern int  pckbd_translate(unsigned char, unsigned char*, char);
extern char pckbd_unexpected_up(unsigned char);
extern void pckbd_leds(unsigned char);
extern void pckbd_init_hw(void);
extern unsigned char pckbd_sysrq_xlate[128];

#define kbd_setkeycode    pckbd_setkeycode
#define kbd_getkeycode    pckbd_getkeycode
#define kbd_translate     pckbd_translate
#define kbd_unexpected_up pckbd_unexpected_up
#define kbd_leds          pckbd_leds
#define kbd_init_hw       pckbd_init_hw
#define kbd_sysrq_xlate   pckbd_sysrq_xlate

#define kbd_request_region()

#define kbd_request_irq(handler) \
  request_irq(KEYBOARD_IRQ, handler, 0, "keyboard", NULL)

#ifdef CONFIG_SH_HD64465
#define kbd_read_input()       inw(KBD_DATA_REG)
#define kbd_read_status()      inw(KBD_STATUS_REG)
#define kbd_write_output(val)  outw(val, KBD_DATA_REG)
#define kbd_write_command(val) do { } while(0)
#define kbd_clear_status()     outw(0x0001, KBD_STATUS_REG)
#define aux_read_input()       inw(AUX_DATA_REG)
#define aux_read_status()      inw(AUX_STATUS_REG)
#define aux_write_output(val)  outw(val, AUX_DATA_REG)
#define aux_clear_status()     outw(0x0001, AUX_STATUS_REG)
#else
#define kbd_read_input()       inb(KBD_DATA_REG)
#define kbd_read_status()      inb(KBD_STATUS_REG)
#define kbd_write_output(val)  outb(val, KBD_DATA_REG)
#define kbd_write_command(val) outb(val, KBD_CNTL_REG)
#endif

#define kbd_pause() do { } while(0)

#define aux_request_irq(hand, dev_id) \
  request_irq(AUX_IRQ, hand, SA_SHIRQ, "PS/2 Mouse", dev_id)
#define aux_free_irq(dev_id) free_irq(AUX_IRQ, dev_id)

#define SYSRQ_KEY 0x54

#endif /* __KERNEL__ */

#endif


