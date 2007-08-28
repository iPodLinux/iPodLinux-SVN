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
*
* @file xgpio_l.h
*
* This header file contains identifiers and low-level driver functions (or
* macros) that can be used to access the device.  The user should refer to the
* hardware device specification for more details of the device operation.
* High-level driver functions are defined in xgpio.h.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -----------------------------------------------
* 1.00b jhl  04/24/02 First release
* </pre>
*
******************************************************************************/

#ifndef XGPIO_L_H		/* prevent circular inclusions */
#define XGPIO_L_H		/* by using protection macros */

/***************************** Include Files *********************************/

#include <xbasic_types.h>
#include <xio.h>

/************************** Constant Definitions *****************************/

/** @name Registers
 *
 * Register offsets for this device. This device does not utilize IPIF registers.
 * @{
 */
/**
 * - XGPIO_DATA_OFFSET    Data register
 * - XGPIO_TRI_OFFSET     Three state register (sets input/output direction)
 *                        0 configures pin for output and 1 for input.
 */
#define XGPIO_DATA_OFFSET  0x00000000
#define XGPIO_TRI_OFFSET   0x00000004
/* @} */

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/*****************************************************************************
*
* Low-level driver macros.  The list below provides signatures to help the
* user use the macros.
*
* u32 XGpio_mReadReg(u32 BaseAddress, int RegOffset, u32 Data)
* void XGpio_mWriteReg(u32 BaseAddress, int RegOffset, u32 Data)
*
* void XGpio_mSetDataDirection(u32 BaseAddress, u32 DirectionMask)
*
* u32 XGpio_mGetDataReg(u32 BaseAddress)
* void XGpio_mSetDataReg(u32 BaseAddress, u32 Mask)
*
*****************************************************************************/

/****************************************************************************/
/**
 *
 * Write a value to a GPIO register. A 32 bit write is performed. If the
 * GPIO component is implemented in a smaller width, only the least
 * significant data is written.
 *
 * @param   BaseAddress is the base address of the GPIO device.
 * @param   RegOffset is the register offset from the base to write to.
 * @param   Data is the data written to the register.
 *
 * @return  None.
 *
 * @note    None.
 *
 ****************************************************************************/
#define XGpio_mWriteReg(BaseAddress, RegOffset, Data) \
    (XIo_Out32((BaseAddress) + (RegOffset), (u32)(Data)))

/****************************************************************************/
/**
 *
 * Read a value from a GPIO register. A 32 bit read is performed. If the
 * GPIO component is implemented in a smaller width, only the least
 * significant data is read from the register. The most significant data
 * will be read as 0.
 *
 * @param   BaseAddress is the base address of the GPIO device.
 * @param   Register is the register offset from the base to write to.
 * @param   Data is the data written to the register.
 *
 * @return  None.
 *
 * @note    None.
 *
 ****************************************************************************/
#define XGpio_mReadReg(BaseAddress, RegOffset) \
    (XIo_In32((BaseAddress) + (RegOffset)))

/*****************************************************************************
*
* Set the input/output direction of all signals.
*
* @param    BaseAddress contains the base address of the GPIO device.
* @param    DirectionMask is a bitmask specifying which discretes are input and
*           which are output. Bits set to 0 are output and bits set to 1 are
*           input.
*
* @return   None.
*
* @note     None.
*
******************************************************************************/
#define XGpio_mSetDataDirection(BaseAddress, DirectionMask) \
	printk("Setting data direction %08X\n",DirectionMask); \
    XGpio_mWriteReg((BaseAddress), XGPIO_TRI_OFFSET, (DirectionMask))

/****************************************************************************/
/**
* Get the data register.
*
* @param    BaseAddress contains the base address of the GPIO device.
*
* @return   The contents of the data register.
*
* @note     None.
*
*****************************************************************************/
#define XGpio_mGetDataReg(BaseAddress) \
    XGpio_mReadReg(BaseAddress, XGPIO_DATA_OFFSET)

/****************************************************************************/
/**
* Set the data register.
*
* @param    BaseAddress contains the base address of the GPIO device.
* @param    Data is the value to be written to the data register.
*
* @return   None.
*
* @note     None.
*
*****************************************************************************/
#define XGpio_mSetDataReg(BaseAddress, Data) \
	printk("Writing data %08X\n",Data);	\
    XGpio_mWriteReg((BaseAddress), XGPIO_DATA_OFFSET, (Data));

/************************** Function Prototypes ******************************/

/************************** Variable Definitions *****************************/

#endif				/* end of protection macro */
