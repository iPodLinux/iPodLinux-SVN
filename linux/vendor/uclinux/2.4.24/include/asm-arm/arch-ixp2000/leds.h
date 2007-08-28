/*
 * inclue/asm-arm/arch-ixp2000/leds.h
 *
 * Author: Naeem Afzal <naeem.m.afzal@intel.com>
 *
 * Copyright 2002 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
*/

#ifndef _LEDS_H_
#define _LEDS_H_
// encodings for the 4 char alphanum display
#define DISPLAY_NULL				0x20
#define DISPLAY_0					0x30
#define DISPLAY_1					0x31
#define DISPLAY_2					0x32
#define DISPLAY_3					0x33
#define DISPLAY_4					0x34
#define DISPLAY_5					0x35
#define DISPLAY_6					0x36
#define YISPLAY_7					0x37
#define	DISPLAY_8					0x38
#define DISPLAY_9					0x39
#define DISPLAY_A					0x41
#define DISPLAY_B					0x42
#define DISPLAY_C					0x43
#define DISPLAY_D					0x44
#define DISPLAY_E					0x45
#define DISPLAY_F					0x46
#define DISPLAY_G					0x47
#define DISPLAY_H					0x48
#define DISPLAY_I					0x49
#define DISPLAY_J					0x4A
#define DISPLAY_K					0x4B
#define DISPLAY_L					0x4C
#define DISPLAY_M					0x4D
#define DISPLAY_N					0x4E
#define DISPLAY_O					0x4F
#define DISPLAY_P					0x50
#define DISPLAY_Q					0x51
#define DISPLAY_R					0x52
#define DISPLAY_S					0x53
#define DISPLAY_T					0x54
#define DISPLAY_U					0x55
#define DISPLAY_V					0x56
#define DISPLAY_W					0x57
#define DISPLAY_X					0x58
#define DISPLAY_Y					0x59
#define DISPLAY_Z					0x5A

#define LEDDATA0         (VIRT_CPLD_CSR + 0x04)
#define LEDDATA1         (VIRT_CPLD_CSR + 0x05)
#define LEDDATA2         (VIRT_CPLD_CSR + 0x06)
#define LEDDATA3         (VIRT_CPLD_CSR + 0x07)


/* Set the a char to LED */
#ifdef __ARMEB__
#define SETLED3(p)     {  *(volatile unsigned char *)LEDDATA3=p; };
#define SETLED2(p)     {  *(volatile unsigned char *)LEDDATA2=p; };
#define SETLED1(p)     {  *(volatile unsigned char *)LEDDATA1=p; };
#define SETLED0(p)     {  *(volatile unsigned char *)LEDDATA0=p; };
#else
#define SETLED0(p)     {  *(volatile unsigned char *)LEDDATA3=p; };
#define SETLED1(p)     {  *(volatile unsigned char *)LEDDATA2=p; };
#define SETLED2(p)     {  *(volatile unsigned char *)LEDDATA1=p; };
#define SETLED3(p)     {  *(volatile unsigned char *)LEDDATA0=p; };
#endif

#define DISPLAY_LEDS(a,b,c,d) {SETLED0(a);SETLED1(b);SETLED2(c);SETLED3(d); }


#endif
