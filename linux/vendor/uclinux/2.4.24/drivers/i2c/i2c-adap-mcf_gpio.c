/* vim:set ts=8 sw=8 et: */
/* ----------------------------------------------------------------------------
 * i2c-adap-mcf_gpio.c i2c-hw access using Motorola Coldfire GPIO
 *
 * ----------------------------------------------------------------------------
 * Cory T. Tusar, Videon Central, Inc.
 * <ctusar@videon-central.com>
 *
 * Copyright (C) 2003 Videon Central, Inc.
 *
 * ----------------------------------------------------------------------------
 * This driver supports using a pair of GPIO pins on the Motorola Coldfire
 * series of microcontrollers as an I2C adapter.
 *
 * Currently, this driver supports the MCF5204, MCF5206, MCF5206e, MCF5249,
 * MCF5272, MCF5307, and MCF5407 processors.  The MCF5282 is not currently
 * supported.
 *
 * This driver has only undergone basic testing on MCF5272 hardware.  Please be
 * aware that issues may be present when attempting to use this driver with
 * untested processors.
 *
 * ----------------------------------------------------------------------------
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: i2c-adap-mcf_gpio.c,v 1.2 2003/09/26 14:55:21 gerg Exp $ */

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

/*
 * Coldfire register offsets, etc.
 * I see no asm/m5204sim.h in the current uClinux tree, but asm/m5206sim.h
 * should work fine for the few registers we need to access.
 */
#include <asm/coldfire.h>
#if defined(CONFIG_M5204)
#include <asm/m5206sim.h>
#elif (defined(CONFIG_M5206) || defined(CONFIG_M5206e))
#include <asm/m5206sim.h>
#elif defined(CONFIG_M5249)
#include <asm/m5249sim.h>
#elif defined(CONFIG_M5272)
#include <asm/m5272sim.h>
#elif (defined(CONFIG_M5282) || defined(CONFIG_M5280))
#include <asm/m5282sim.h>
#elif defined(CONFIG_M5307)
#include <asm/m5307sim.h>
#elif defined(CONFIG_M5407)
#include <asm/m5407sim.h>
#endif  /* CONFIG_M5* */

/* Legacy includes */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 0))
#include <linux/string.h>               /* Provides NULL for 2.0 kernels */
#include <asm/errno.h>                  /* Provides ENODEV for 2.0 kernels */
#endif  /* LINUX_VERSION_CODE */


/* ----------------------------------------------------------------------------
 * Compile-time dummy checks to keep things relatively sane
 */
#if !defined(CONFIG_COLDFIRE)
#error "Can't build Coldfire I2C-Bus adapter driver for non-Coldfire processor"
#endif  /* !defined(CONFIG_COLDFIRE) */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0))
#warning "This driver has not been tested with kernel versions < 2.4.0"
#endif  /* LINUX_VERSION_CODE */


/* ----------------------------------------------------------------------------
 * Dig through the myriad of #define's created during configuration to
 * determine the registers used for controlling SDA and SCL, as well as the
 * bitmasks needed for pin function setup, data direction toggling, and general
 * bit-banging of the bus.
 *
 * This is kinda kludgey due to the way the configuration 'choice' operator
 * works.  I'm open to suggestions for improvement.
 */

/* ----------------------------------------------------------------------------
 * MCF5204, MCF5206, and MCF5206e driver ports and bitmasks
 */
#if (defined(CONFIG_M5204) || defined(CONFIG_M5206) || defined(MCF5206e))

/* Determine SCL line settings based on CONFIG_I2C_MCF_GPIO_SCL_* */
#define I2C_PORT_SCL_STRING     "PP"
#define I2C_PORT_SCL_READ       *(volatile u8 *)(MCF_MBAR + MCFSIM_PADAT)
#define I2C_PORT_SCL_WRITE      *(volatile u8 *)(MCF_MBAR + MCFSIM_PADAT)
#define I2C_DDR_SCL             *(volatile u8 *)(MCF_MBAR + MCFSIM_PADDR)

#if (defined(CONFIG_M5204) || defined(CONFIG_M5206))
#define I2C_CNT_SCL             *(volatile u8 *)(MCF_MBAR + MCFSIM_PAR)

#elif defined(CONFIG_M5206e)
#define I2C_CNT_SCL             *(volatile u16 *)(MCF_MBAR + MCFSIM_PAR)
#endif  /* CONFIG_M520* */

#if defined(CONFIG_I2C_MCF_GPIO_SCL_PP0)
#define I2C_BIT_SCL             0

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP1)
#define I2C_BIT_SCL             1

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP2)
#define I2C_BIT_SCL             2

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP3)
#define I2C_BIT_SCL             3

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP4)
#define I2C_BIT_SCL             4

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP5)
#define I2C_BIT_SCL             5

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP6)
#define I2C_BIT_SCL             6

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP7)
#define I2C_BIT_SCL             7

#else
#error "CONFIG_I2C_MCF_GPIO_SCL_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_MCF_GPIO_SCL_* */


/* Determine SDA line settings based on CONFIG_I2C_MCF_GPIO_SDA_* */
#define I2C_PORT_SDA_STRING     "PP"
#define I2C_PORT_SDA_READ       *(volatile u8 *)(MCF_MBAR + MCFSIM_PADAT)
#define I2C_PORT_SDA_WRITE      *(volatile u8 *)(MCF_MBAR + MCFSIM_PADAT)
#define I2C_DDR_SDA             *(volatile u8 *)(MCF_MBAR + MCFSIM_PADDR)

#if (defined(CONFIG_M5204) || defined(CONFIG_M5206))
#define I2C_CNT_SDA             *(volatile u8 *)(MCF_MBAR + MCFSIM_PAR)

#elif defined(CONFIG_M5206e)
#define I2C_CNT_SDA             *(volatile u16 *)(MCF_MBAR + MCFSIM_PAR)
#endif  /* CONFIG_M520* */

#if defined(CONFIG_I2C_MCF_GPIO_SDA_PP0)
#define I2C_BIT_SDA             0

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP1)
#define I2C_BIT_SDA             1

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP2)
#define I2C_BIT_SDA             2

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP3)
#define I2C_BIT_SDA             3

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP4)
#define I2C_BIT_SDA             4

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP5)
#define I2C_BIT_SDA             5

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP6)
#define I2C_BIT_SDA             6

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP7)
#define I2C_BIT_SDA             7

#else
#error "CONFIG_I2C_MCF_GPIO_SDA_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_MCF_GPIO_SDA_* */


/* Bitmasks for SCL and SDA lines */
#define I2C_MASK_SCL            (1 << I2C_BIT_SCL)
#define I2C_MASK_SDA            (1 << I2C_BIT_SDA)

#if defined(CONFIG_M5204)
#define I2C_MASK_CNT_SCL_AND    (~0)
#define I2C_MASK_CNT_SDA_AND    (~0)
#define I2C_MASK_CNT_SCL_OR     (1 << I2C_BIT_SCL)
#define I2C_MASK_CNT_SDA_OR     (1 << I2C_BIT_SDA)

#elif (defined(CONFIG_M5206) || defined(CONFIG_M5206e)
#define I2C_MASK_CNT_SCL_AND    (~(1 << ((I2C_BIT_SCL > 3) ? 5 : 4)))
#define I2C_MASK_CNT_SDA_AND    (~(1 << ((I2C_BIT_SDA > 3) ? 5 : 4)))
#define I2C_MASK_CNT_SCL_OR     (0)
#define I2C_MASK_CNT_SDA_OR     (0)
#endif  /* MCF520* */


/* ----------------------------------------------------------------------------
 * MCF5249 driver ports and bitmasks
 */
#elif defined(CONFIG_M5249)

/* Determine SCL line settings based on CONFIG_I2C_MCF_GPIO_SCL_* */
#if (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO0) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO1) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO2) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO3) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO4) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO5) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO6) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO7) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO9) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO10) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO11) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO12) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO13) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO14) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO15) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO16) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO17) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO18) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO19) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO20) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO21) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO22) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO23) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO24) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO25) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO26) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO29))
#define I2C_PORT_SCL_STRING     "GPIO"
#define I2C_PORT_SCL_READ       *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIOREAD)
#define I2C_PORT_SCL_WRITE      *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIOWRITE)
#define I2C_DDR_SCL             *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIOENABLE)
#define I2C_CNT_SCL             *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIOFUNC)

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO33) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO34) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO44) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO45) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO46) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO48) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO49) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO50) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO51) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO52) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO53) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO54) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO55) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO56) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO57) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO58) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO59) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO60) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO61) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO62))
#define I2C_PORT_SCL_STRING     "GPIO"
#define I2C_PORT_SCL_READ       *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIO1READ)
#define I2C_PORT_SCL_WRITE      *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIO1WRITE)
#define I2C_DDR_SCL             *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIO1ENABLE)
#define I2C_CNT_SCL             *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIO1FUNC)

#else
#error "CONFIG_I2C_MCF_GPIO_SCL_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_MCF_GPIO_SCL_* */


#if (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO0) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO32))
#define I2C_BIT_SCL             0

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO1) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO33))
#define I2C_BIT_SCL             1

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO2) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO34))
#define I2C_BIT_SCL             2

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO3) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO35))
#define I2C_BIT_SCL             3

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO4) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO36))
#define I2C_BIT_SCL             4

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO5) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO37))
#define I2C_BIT_SCL             5

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO6) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO38))
#define I2C_BIT_SCL             6

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO7) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO39))
#define I2C_BIT_SCL             7

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO8) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO40))
#define I2C_BIT_SCL             8

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO9) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO41))
#define I2C_BIT_SCL             9

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO10) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO42))
#define I2C_BIT_SCL             10

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO11) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO43))
#define I2C_BIT_SCL             11

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO12) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO44))
#define I2C_BIT_SCL             12

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO13) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO45))
#define I2C_BIT_SCL             13

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO14) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO46))
#define I2C_BIT_SCL             14

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO15) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO47))
#define I2C_BIT_SCL             15

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO16) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO48))
#define I2C_BIT_SCL             16

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO17) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO49))
#define I2C_BIT_SCL             17

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO18) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO50))
#define I2C_BIT_SCL             18

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO19) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO51))
#define I2C_BIT_SCL             19

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO20) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO52))
#define I2C_BIT_SCL             20

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO21) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO53))
#define I2C_BIT_SCL             21

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO22) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO54))
#define I2C_BIT_SCL             22

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO23) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO55))
#define I2C_BIT_SCL             23

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO24) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO56))
#define I2C_BIT_SCL             24

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO25) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO57))
#define I2C_BIT_SCL             25

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO26) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO58))
#define I2C_BIT_SCL             26

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO27) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO59))
#define I2C_BIT_SCL             27

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO28) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO60))
#define I2C_BIT_SCL             28

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO29) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO61))
#define I2C_BIT_SCL             29

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO30) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO62))
#define I2C_BIT_SCL             30

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO31) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_GPIO63))
#define I2C_BIT_SCL             31

#else
#error "CONFIG_I2C_MCF_GPIO_SCL_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_MCF_GPIO_SCL_* */


/* Determine SDA line settings based on CONFIG_I2C_MCF_GPIO_SDA_* */
#if (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO0) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO1) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO2) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO3) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO4) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO5) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO6) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO7) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO9) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO10) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO11) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO12) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO13) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO14) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO15) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO16) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO17) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO18) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO19) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO20) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO21) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO22) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO23) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO24) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO25) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO26) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO29))
#define I2C_PORT_SDA_STRING     "GPIO"
#define I2C_PORT_SDA_READ       *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIOREAD)
#define I2C_PORT_SDA_WRITE      *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIOWRITE)
#define I2C_DDR_SDA             *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIOENABLE)
#define I2C_CNT_SDA             *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIOFUNC)

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO33) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO34) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO44) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO45) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO46) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO48) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO49) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO50) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO51) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO52) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO53) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO54) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO55) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO56) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO57) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO58) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO59) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO60) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO61) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO62))
#define I2C_PORT_SDA_STRING     "GPIO"
#define I2C_PORT_SDA_READ       *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIO1READ)
#define I2C_PORT_SDA_WRITE      *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIO1WRITE)
#define I2C_DDR_SDA             *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIO1ENABLE)
#define I2C_CNT_SDA             *(volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIO1FUNC)

#else
#error "CONFIG_I2C_MCF_GPIO_SDA_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_MCF_GPIO_SDA_* */


#if (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO0) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO32))
#define I2C_BIT_SDA             0

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO1) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO33))
#define I2C_BIT_SDA             1

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO2) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO34))
#define I2C_BIT_SDA             2

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO3) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO35))
#define I2C_BIT_SDA             3

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO4) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO36))
#define I2C_BIT_SDA             4

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO5) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO37))
#define I2C_BIT_SDA             5

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO6) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO38))
#define I2C_BIT_SDA             6

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO7) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO39))
#define I2C_BIT_SDA             7

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO8) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO40))
#define I2C_BIT_SDA             8

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO9) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO41))
#define I2C_BIT_SDA             9

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO10) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO42))
#define I2C_BIT_SDA             10

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO11) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO43))
#define I2C_BIT_SDA             11

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO12) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO44))
#define I2C_BIT_SDA             12

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO13) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO45))
#define I2C_BIT_SDA             13

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO14) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO46))
#define I2C_BIT_SDA             14

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO15) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO47))
#define I2C_BIT_SDA             15

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO16) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO48))
#define I2C_BIT_SDA             16

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO17) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO49))
#define I2C_BIT_SDA             17

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO18) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO50))
#define I2C_BIT_SDA             18

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO19) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO51))
#define I2C_BIT_SDA             19

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO20) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO52))
#define I2C_BIT_SDA             20

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO21) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO53))
#define I2C_BIT_SDA             21

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO22) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO54))
#define I2C_BIT_SDA             22

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO23) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO55))
#define I2C_BIT_SDA             23

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO24) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO56))
#define I2C_BIT_SDA             24

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO25) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO57))
#define I2C_BIT_SDA             25

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO26) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO58))
#define I2C_BIT_SDA             26

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO27) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO59))
#define I2C_BIT_SDA             27

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO28) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO60))
#define I2C_BIT_SDA             28

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO29) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO61))
#define I2C_BIT_SDA             29

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO30) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO62))
#define I2C_BIT_SDA             30

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO31) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_GPIO63))
#define I2C_BIT_SDA             31

#else
#error "CONFIG_I2C_MCF_GPIO_SDA_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_MCF_GPIO_SDA_* */


/* Bitmasks for SCL and SDA lines */
#define I2C_MASK_SCL            (1 << I2C_BIT_SCL)
#define I2C_MASK_SDA            (1 << I2C_BIT_SDA)

#define I2C_MASK_CNT_SCL_AND    (~0)
#define I2C_MASK_CNT_SDA_AND    (~0)
#define I2C_MASK_CNT_SCL_OR     (1 << I2C_BIT_SCL)
#define I2C_MASK_CNT_SDA_OR     (1 << I2C_BIT_SDA)


/* ----------------------------------------------------------------------------
 * MCF5272 driver ports and bitmasks
 */
#elif defined(CONFIG_M5272)

/* Determine SCL line settings based on CONFIG_I2C_MCF_GPIO_SCL_* */
#if (defined(CONFIG_I2C_MCF_GPIO_SCL_PA0) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA1) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA2) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA3) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA4) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA5) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA6) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA7) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA8) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA9) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA10) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA11) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA12) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA13) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA14) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PA15))
#define I2C_PORT_SCL_STRING     "PA"
#define I2C_PORT_SCL_READ       *(volatile u16 *)(MCF_MBAR + MCFSIM_PADAT)
#define I2C_PORT_SCL_WRITE      *(volatile u16 *)(MCF_MBAR + MCFSIM_PADAT)
#define I2C_DDR_SCL             *(volatile u16 *)(MCF_MBAR + MCFSIM_PADDR)
#define I2C_CNT_SCL             *(volatile u32 *)(MCF_MBAR + MCFSIM_PACNT)

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PB0) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB1) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB2) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB3) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB4) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB5) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB6) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB7) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB8) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB9) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB10) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB11) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB12) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB13) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB14) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB15))
#define I2C_PORT_SCL_STRING     "PB"
#define I2C_PORT_SCL_READ       *(volatile u16 *)(MCF_MBAR + MCFSIM_PBDAT)
#define I2C_PORT_SCL_WRITE      *(volatile u16 *)(MCF_MBAR + MCFSIM_PBDAT)
#define I2C_DDR_SCL             *(volatile u16 *)(MCF_MBAR + MCFSIM_PBDDR)
#define I2C_CNT_SCL             *(volatile u32 *)(MCF_MBAR + MCFSIM_PBCNT)

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PC0) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC1) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC2) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC3) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC4) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC5) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC6) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC7) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC8) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC9) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC10) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC11) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC12) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC13) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC14) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC15))
#define I2C_PORT_SCL_STRING     "PC"
#define I2C_PORT_SCL_READ       *(volatile u16 *)(MCF_MBAR + MCFSIM_PCDAT)
#define I2C_PORT_SCL_WRITE      *(volatile u16 *)(MCF_MBAR + MCFSIM_PCDAT)
#define I2C_DDR_SCL             *(volatile u16 *)(MCF_MBAR + MCFSIM_PCDDR)
#undef  I2C_CNT_SCL             /* Port C has no control register */

#else
#error "CONFIG_I2C_MCF_GPIO_SCL_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_MCF_GPIO_SCL_* */


#if (defined(CONFIG_I2C_MCF_GPIO_SCL_PA0) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB0) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC0))
#define I2C_BIT_SCL             0

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA1) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB1) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC1))
#define I2C_BIT_SCL             1

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA2) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB2) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC2))
#define I2C_BIT_SCL             2

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA3) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB3) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC3))
#define I2C_BIT_SCL             3

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA4) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB4) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC4))
#define I2C_BIT_SCL             4

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA5) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB5) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC5))
#define I2C_BIT_SCL             5

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA6) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB6) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC6))
#define I2C_BIT_SCL             6

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA7) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB7) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC7))
#define I2C_BIT_SCL             7

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA8) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB8) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC8))
#define I2C_BIT_SCL             8

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA9) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB9) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC9))
#define I2C_BIT_SCL             9

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA10) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB10) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC10))
#define I2C_BIT_SCL             10

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA11) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB11) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC11))
#define I2C_BIT_SCL             11

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA12) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB12) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC12))
#define I2C_BIT_SCL             12

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA13) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB13) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC13))
#define I2C_BIT_SCL             13

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA14) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB14) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC14))
#define I2C_BIT_SCL             14

#elif (defined(CONFIG_I2C_MCF_GPIO_SCL_PA15) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PB15) || \
        defined(CONFIG_I2C_MCF_GPIO_SCL_PC15))
#define I2C_BIT_SCL             15

#else
#error "CONFIG_I2C_MCF_GPIO_SCL_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_MCF_GPIO_SCL_* */


/* Determine SDA line settings based on CONFIG_I2C_MCF_GPIO_SDA_* */
#if (defined(CONFIG_I2C_MCF_GPIO_SDA_PA0) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA1) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA2) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA3) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA4) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA5) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA6) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA7) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA8) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA9) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA10) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA11) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA12) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA13) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA14) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PA15))
#define I2C_PORT_SDA_STRING     "PA"
#define I2C_PORT_SDA_READ       *(volatile u16 *)(MCF_MBAR + MCFSIM_PADAT)
#define I2C_PORT_SDA_WRITE      *(volatile u16 *)(MCF_MBAR + MCFSIM_PADAT)
#define I2C_DDR_SDA             *(volatile u16 *)(MCF_MBAR + MCFSIM_PADDR)
#define I2C_CNT_SDA             *(volatile u32 *)(MCF_MBAR + MCFSIM_PACNT)

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PB0) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB1) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB2) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB3) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB4) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB5) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB6) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB7) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB8) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB9) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB10) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB11) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB12) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB13) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB14) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB15))
#define I2C_PORT_SDA_STRING     "PB"
#define I2C_PORT_SDA_READ       *(volatile u16 *)(MCF_MBAR + MCFSIM_PBDAT)
#define I2C_PORT_SDA_WRITE      *(volatile u16 *)(MCF_MBAR + MCFSIM_PBDAT)
#define I2C_DDR_SDA             *(volatile u16 *)(MCF_MBAR + MCFSIM_PBDDR)
#define I2C_CNT_SDA             *(volatile u32 *)(MCF_MBAR + MCFSIM_PBCNT)

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PC0) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC1) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC2) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC3) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC4) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC5) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC6) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC7) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC8) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC9) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC10) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC11) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC12) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC13) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC14) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC15))
#define I2C_PORT_SDA_STRING     "PC"
#define I2C_PORT_SDA_READ       *(volatile u16 *)(MCF_MBAR + MCFSIM_PCDAT)
#define I2C_PORT_SDA_WRITE      *(volatile u16 *)(MCF_MBAR + MCFSIM_PCDAT)
#define I2C_DDR_SDA             *(volatile u16 *)(MCF_MBAR + MCFSIM_PCDDR)
#undef  I2C_CNT_SDA             /* Port C has no control register */

#else
#error "CONFIG_I2C_MCF_GPIO_SDA_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_MCF_GPIO_SDA_* */


#if (defined(CONFIG_I2C_MCF_GPIO_SDA_PA0) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB0) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC0))
#define I2C_BIT_SDA             0

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA1) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB1) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC1))
#define I2C_BIT_SDA             1

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA2) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB2) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC2))
#define I2C_BIT_SDA             2

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA3) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB3) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC3))
#define I2C_BIT_SDA             3

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA4) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB4) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC4))
#define I2C_BIT_SDA             4

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA5) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB5) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC5))
#define I2C_BIT_SDA             5

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA6) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB6) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC6))
#define I2C_BIT_SDA             6

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA7) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB7) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC7))
#define I2C_BIT_SDA             7

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA8) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB8) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC8))
#define I2C_BIT_SDA             8

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA9) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB9) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC9))
#define I2C_BIT_SDA             9

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA10) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB10) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC10))
#define I2C_BIT_SDA             10

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA11) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB11) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC11))
#define I2C_BIT_SDA             11

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA12) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB12) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC12))
#define I2C_BIT_SDA             12

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA13) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB13) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC13))
#define I2C_BIT_SDA             13

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA14) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB14) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC14))
#define I2C_BIT_SDA             14

#elif (defined(CONFIG_I2C_MCF_GPIO_SDA_PA15) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PB15) || \
        defined(CONFIG_I2C_MCF_GPIO_SDA_PC15))
#define I2C_BIT_SDA             15

#else
#error "CONFIG_I2C_MCF_GPIO_SDA_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_MCF_GPIO_SDA_* */


/* Bitmasks for SCL and SDA lines */
#define I2C_MASK_SCL            (1 << I2C_BIT_SCL)
#define I2C_MASK_SDA            (1 << I2C_BIT_SDA)

#define I2C_MASK_CNT_SCL_AND    (~(3 << (I2C_BIT_SCL << 1)))
#define I2C_MASK_CNT_SDA_AND    (~(3 << (I2C_BIT_SDA << 1)))
#define I2C_MASK_CNT_SCL_OR     (0)
#define I2C_MASK_CNT_SDA_OR     (0)


/* ----------------------------------------------------------------------------
 * MCF5307 and MCF5407 driver ports and bitmasks
 */
#elif (defined(CONFIG_M5307) || defined(MCF5407))

/* Determine SCL line settings based on CONFIG_I2C_MCF_GPIO_SCL_* */
#define I2C_PORT_SCL_STRING     "PP"
#define I2C_PORT_SCL_READ       *(volatile u16 *)(MCF_MBAR + MCFSIM_PADAT)
#define I2C_PORT_SCL_WRITE      *(volatile u16 *)(MCF_MBAR + MCFSIM_PADAT)
#define I2C_DDR_SCL             *(volatile u16 *)(MCF_MBAR + MCFSIM_PADDR)
#define I2C_CNT_SCL             *(volatile u16 *)(MCF_MBAR + MCFSIM_PAR)

#if defined(CONFIG_I2C_MCF_GPIO_SCL_PP0)
#define I2C_BIT_SCL             0

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP1)
#define I2C_BIT_SCL             1

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP2)
#define I2C_BIT_SCL             2

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP3)
#define I2C_BIT_SCL             3

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP4)
#define I2C_BIT_SCL             4

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP5)
#define I2C_BIT_SCL             5

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP6)
#define I2C_BIT_SCL             6

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP7)
#define I2C_BIT_SCL             7

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP8)
#define I2C_BIT_SCL             8

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP9)
#define I2C_BIT_SCL             9

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP10)
#define I2C_BIT_SCL             10

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP11)
#define I2C_BIT_SCL             11

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP12)
#define I2C_BIT_SCL             12

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP13)
#define I2C_BIT_SCL             13

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP14)
#define I2C_BIT_SCL             14

#elif defined(CONFIG_I2C_MCF_GPIO_SCL_PP15)
#define I2C_BIT_SCL             15

#else
#error "CONFIG_I2C_MCF_GPIO_SCL_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_MCF_GPIO_SCL_* */


/* Determine SDA line settings based on CONFIG_I2C_MCF_GPIO_SDA_* */
#define I2C_PORT_SDA_STRING     "PP"
#define I2C_PORT_SDA_READ       *(volatile u16 *)(MCF_MBAR + MCFSIM_PADAT)
#define I2C_PORT_SDA_WRITE      *(volatile u16 *)(MCF_MBAR + MCFSIM_PADAT)
#define I2C_DDR_SDA             *(volatile u16 *)(MCF_MBAR + MCFSIM_PADDR)
#define I2C_CNT_SDA             *(volatile u16 *)(MCF_MBAR + MCFSIM_PAR)

#if defined(CONFIG_I2C_MCF_GPIO_SDA_PP0)
#define I2C_BIT_SDA             0

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP1)
#define I2C_BIT_SDA             1

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP2)
#define I2C_BIT_SDA             2

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP3)
#define I2C_BIT_SDA             3

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP4)
#define I2C_BIT_SDA             4

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP5)
#define I2C_BIT_SDA             5

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP6)
#define I2C_BIT_SDA             6

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP7)
#define I2C_BIT_SDA             7

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP8)
#define I2C_BIT_SDA             8

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP9)
#define I2C_BIT_SDA             9

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP10)
#define I2C_BIT_SDA             10

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP11)
#define I2C_BIT_SDA             11

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP12)
#define I2C_BIT_SDA             12

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP13)
#define I2C_BIT_SDA             13

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP14)
#define I2C_BIT_SDA             14

#elif defined(CONFIG_I2C_MCF_GPIO_SDA_PP15)
#define I2C_BIT_SDA             15

#else
#error "CONFIG_I2C_MCF_GPIO_SDA_* is not correctly #define'd!"
#endif  /* CONFIG_I2C_MCF_GPIO_SDA_* */


/* Bitmasks for SCL and SDA lines */
#define I2C_MASK_SCL            (1 << I2C_BIT_SCL)
#define I2C_MASK_SDA            (1 << I2C_BIT_SDA)

#define I2C_MASK_CNT_SCL_AND    (~(1 << I2C_BIT_SCL))
#define I2C_MASK_CNT_SDA_AND    (~(1 << I2C_BIT_SDA))
#define I2C_MASK_CNT_SCL_OR     (0)
#define I2C_MASK_CNT_SDA_OR     (0)


#else
#error "CONFIG_M5* is not correctly #define'd!  What processor is this?"
#endif  /* CONFIG_M5* */


/* ----------------------------------------------------------------------------
 * Local prototypes
 */
static void bit_mcf_gpio_inc_use(struct i2c_adapter *adap);
static void bit_mcf_gpio_dec_use(struct i2c_adapter *adap);
static int bit_mcf_gpio_reg(struct i2c_client *client);
static int bit_mcf_gpio_unreg(struct i2c_client *client);
static void bit_mcf_gpio_init(void);
static void bit_mcf_gpio_setscl(void *data, int state);
static void bit_mcf_gpio_setsda(void *data, int state);
static int bit_mcf_gpio_getscl(void *data);
static int bit_mcf_gpio_getsda(void *data);


/* ----------------------------------------------------------------------------
 * Data structures
 */
static struct i2c_algo_bit_data bit_mcf_gpio_data = {
        NULL,
        bit_mcf_gpio_setsda,
        bit_mcf_gpio_setscl,
        bit_mcf_gpio_getsda,
        bit_mcf_gpio_getscl,
        10,
        10,
        100
};

static struct i2c_adapter bit_mcf_gpio_ops = {
        "Motorola Coldfire GPIO",
        I2C_HW_B_COLDF,
        NULL,
        &bit_mcf_gpio_data,
        bit_mcf_gpio_inc_use,
        bit_mcf_gpio_dec_use,
        bit_mcf_gpio_reg,
        bit_mcf_gpio_unreg,
};


/* ----------------------------------------------------------------------------
 * Public methods
 */
#if defined(MODULE)
int init_module(void)
{
        return(i2c_bit_mcf_gpio_init());
}


void cleanup_module(void)
{
        i2c_bit_del_bus(&bit_mcf_gpio_ops);
}
#endif  /* MODULE */


int __init i2c_bit_mcf_gpio_init(void)
{
        /* Set SCL and SDA lines for use as GPIO */
        bit_mcf_gpio_init();

        /* Set SDA and SCL as output initially */
        bit_mcf_gpio_setsda(NULL, 1);
        bit_mcf_gpio_setscl(NULL, 1);

        if (i2c_bit_add_bus(&bit_mcf_gpio_ops) < 0)
                return(-ENODEV);

        printk("i2c-mcf_gpio.o: Motorola Coldfire GPIO I2C adapter\n");
        printk("i2c-mcf_gpio.o: SCL on %s%d, SDA on %s%d\n",
                        I2C_PORT_SCL_STRING, I2C_BIT_SCL,
                        I2C_PORT_SDA_STRING, I2C_BIT_SDA);

        return(0);
}


/* ----------------------------------------------------------------------------
 * Private methods and helper functions
 */
static void bit_mcf_gpio_inc_use(struct i2c_adapter *adap)
{
#if defined(MODULE)
        MOD_INC_USE_COUNT;
#endif  /* MODULE */
}


static void bit_mcf_gpio_dec_use(struct i2c_adapter *adap)
{
#if defined(MODULE)
        MOD_DEC_USE_COUNT;
#endif  /* MODULE */
}


static int bit_mcf_gpio_reg(struct i2c_client *client)
{
        return(0);
}


static int bit_mcf_gpio_unreg(struct i2c_client *client)
{
        return(0);
}


static void bit_mcf_gpio_init(void)
{
        /* I2C_CNT_* will not be defined for MCF5272 Port C */
#if defined(I2C_CNT_SCL)
        I2C_CNT_SCL &= I2C_MASK_CNT_SCL_AND;
        I2C_CNT_SCL |= I2C_MASK_CNT_SCL_OR;
#endif  /* I2C_CNT_SCL */

#if defined(I2C_CNT_SDA)
        I2C_CNT_SDA &= I2C_MASK_CNT_SDA_AND;
        I2C_CNT_SDA |= I2C_MASK_CNT_SDA_OR;
#endif  /* I2C_CNT_SDA */
}


static void bit_mcf_gpio_setscl(void *data, int state)
{
        I2C_DDR_SCL |= I2C_MASK_SCL;

        if (state)
                I2C_PORT_SCL_WRITE |= I2C_MASK_SCL;
        else
                I2C_PORT_SCL_WRITE &= ~I2C_MASK_SCL;
}


static void bit_mcf_gpio_setsda(void *data, int state)
{
        I2C_DDR_SDA |= I2C_MASK_SDA;

        if (state)
                I2C_PORT_SDA_WRITE |= I2C_MASK_SDA;
        else
                I2C_PORT_SDA_WRITE &= ~I2C_MASK_SDA;
}


static int bit_mcf_gpio_getscl(void *data)
{
        I2C_DDR_SCL &= ~I2C_MASK_SCL;
        return((I2C_PORT_SCL_READ & I2C_MASK_SCL) != 0);
}


static int bit_mcf_gpio_getsda(void *data)
{
        I2C_DDR_SDA &= ~I2C_MASK_SDA;
        return((I2C_PORT_SDA_READ & I2C_MASK_SDA) != 0);
}


/* ----------------------------------------------------------------------------
 * Driver public symbols
 */
EXPORT_NO_SYMBOLS;

#if defined(MODULE)
MODULE_AUTHOR("Cory T. Tusar <ctusar@videon-central.com>");
MODULE_DESCRIPTION("I2C-Bus GPIO driver for Motorola Coldfire processors");
MODULE_LICENSE("GPL");
#endif  /* MODULE */

