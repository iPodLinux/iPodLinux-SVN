#ifndef _BOOTLOADER_H_
#define _BOOTLOADER_H_

typedef unsigned int   uint32;
typedef unsigned short uint16;
typedef unsigned char  uint8;
typedef   signed int    int32;
typedef   signed short  int16;
typedef   signed char   int8;

typedef unsigned long size_t;

#undef NULL
#define NULL ((void*)0x0)

#define inl(a) (*(volatile unsigned long *) (a))
#define outl(a,b) (*(volatile unsigned long *) (b) = (a))
#define inw(a) (*(volatile unsigned short *) (a))
#define outw(a,b) (*(volatile unsigned short *) (b) = (a))
#define inb(a) (*(volatile unsigned char *) (a))
#define outb(a,b) (*(volatile unsigned char *) (b) = (a))


#endif
