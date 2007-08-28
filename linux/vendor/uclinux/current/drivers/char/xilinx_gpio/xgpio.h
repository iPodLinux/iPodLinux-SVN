/******************************************************************************
*
*     Author: Xilinx, Inc.
*     
*     
*     This program is free software; you can redistribute it and/or modify it
*     under the terms of the GNU General Public License as published by the
*     Free Software Foundation; either version 2 of the License, or (at your
*     option) any later version.
*     
*     
*     XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
*     COURTESY TO YOU. BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
*     ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE, APPLICATION OR STANDARD,
*     XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION IS FREE
*     FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE FOR OBTAINING
*     ANY THIRD PARTY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
*     XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
*     THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY
*     WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM
*     CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND
*     FITNESS FOR A PARTICULAR PURPOSE.
*     
*     
*     Xilinx hardware products are not intended for use in life support
*     appliances, devices, or systems. Use in such applications is
*     expressly prohibited.
*     
*     
*     (c) Copyright 2002-2003 Xilinx Inc.
*     All rights reserved.
*     
*     
*     You should have received a copy of the GNU General Public License along
*     with this program; if not, write to the Free Software Foundation, Inc.,
*     675 Mass Ave, Cambridge, MA 02139, USA.
*
******************************************************************************/
/*****************************************************************************/
/**
* @file xgpio.h
*
* This file contains the software API definition of the Xilinx General Purpose
* I/O (XGpio) component.
*
* The Xilinx GPIO controller is a soft IP core designed for  Xilinx FPGAs and
* contains the following general features:
*   - Support for 8, 16, or 32 I/O discretes
*   - Each of the discretes can be configured for input or output.
*
* @note
*
* This API utilizes 32 bit I/O to the GPIO registers. With 16 and 8 bit GPIO
* components, the unused bits from registers are read as zero and written as
* don't cares.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -----------------------------------------------
* 1.00a rmm  03/13/02 First release
* </pre>
*****************************************************************************/

#ifndef XGPIO_H			/* prevent circular inclusions */
#define XGPIO_H			/* by using protection macros */

/***************************** Include Files ********************************/

#include <xbasic_types.h>
#include <xstatus.h>
#include "xgpio_l.h"

/************************** Constant Definitions ****************************/

/**************************** Type Definitions ******************************/

/*
 * This typedef contains configuration information for the device.
 */
typedef struct {
	u16 DeviceId;		/* Unique ID  of device */
	u32 BaseAddress;	/* Device base address */
} XGpio_Config;

/**
 * The XGpio driver instance data. The user is required to allocate a
 * variable of this type for every GPIO device in the system. A pointer
 * to a variable of this type is then passed to the driver API functions.
 */
typedef struct {
	u32 BaseAddress;	/* Device base address */
	u32 IsReady;		/* Device is initialized and ready */
} XGpio;

/***************** Macros (Inline Functions) Definitions ********************/

/************************** Function Prototypes *****************************/

/*
 * API Basic functions implemented in xgpio.c
 */
XStatus XGpio_Initialize(XGpio * InstancePtr, u16 DeviceId);
void XGpio_SetDataDirection(XGpio * InstancePtr, u32 DirectionMask);
u32 XGpio_DiscreteRead(XGpio * InstancePtr);
void XGpio_DiscreteWrite(XGpio * InstancePtr, u32 Mask);

XGpio_Config *XGpio_LookupConfig(u16 DeviceId);

/*
 * API Functions implemented in xgpio_extra.c
 */
void XGpio_DiscreteSet(XGpio * InstancePtr, u32 Mask);
void XGpio_DiscreteClear(XGpio * InstancePtr, u32 Mask);

/*
 * API Functions implemented in xgpio_selftest.c
 */
XStatus XGpio_SelfTest(XGpio * InstancePtr);
#endif				/* end of protection macro */
