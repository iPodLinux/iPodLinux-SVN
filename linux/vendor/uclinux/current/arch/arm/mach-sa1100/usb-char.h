/*
 * Copyright (C) 2001 Extenex Corporation
 *
 * usb-char.h
 *
 * Character device emulation client for SA-1100 client usb core.
 *
 *
 *
 */
#ifndef _USB_CHAR_H
#define _USB_CHAR_H

#define USBC_MAJOR 10      /* miscellaneous character device */
#define USBC_MINOR 240     /* in the "reserved for local use" range */

#define USBC_MAGIC 0x8E

/* zap everything in receive ring buffer */
#define USBC_IOC_FLUSH_RECEIVER    _IO( USBC_MAGIC, 0x01 )

/* reset transmitter */
#define USBC_IOC_FLUSH_TRANSMITTER _IO( USBC_MAGIC, 0x02 )

/* do both of above */
#define USBC_IOC_FLUSH_ALL         _IO( USBC_MAGIC, 0x03 )






#endif /* _USB_CHAR_H */

