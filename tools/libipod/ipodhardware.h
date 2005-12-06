#ifndef _IPODHARDWARE_H_
#define _IPODHARDWARE_H_

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#ifdef IPOD
#include <linux/fb.h>
#endif

#ifdef IPOD
#define CPUINFOLOC "/proc/cpuinfo"
#define MOUNTSLOC "/proc/mounts"
#define APMLOC "/proc/apm"
#else
#define CPUINFOLOC "cpuinfo"
#define MOUNTSLOC "mounts"
#define APMLOC "apm"
#endif

#define LCD_DATA          0x10
#define LCD_CMD           0x08
#define IPOD_OLD_LCD_BASE 0xc0001000
#define IPOD_OLD_LCD_RTC  0xcf001110
#define IPOD_NEW_LCD_BASE 0x70003000
#define IPOD_NEW_LCD_RTC  0x60005010

#define FBIOGET_CONTRAST	_IOR('F', 0x22, int)
#define FBIOSET_CONTRAST	_IOW('F', 0x23, int)

#define FBIOGET_BACKLIGHT	_IOR('F', 0x24, int)
#define FBIOSET_BACKLIGHT	_IOW('F', 0x25, int)

#define FB_DEV_NAME			"/dev/fb0"
#define FB_DEVFS_NAME		"/dev/fb/0"

#define inb(a) (*(volatile unsigned char *) (a))
#define outb(a,b) (*(volatile unsigned char *) (b) = (a))

#define inl(a) (*(volatile unsigned long *) (a))
#define outl(a,b) (*(volatile unsigned long *) (b) = (a))

#endif /* _IPODHARDWARE_H_ */

