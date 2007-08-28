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
/**
* @file xgpio.c
*
* The implementation of the XGpio component's basic functionality. See xgpio.h
* for more information about the component.
*
* @note
*
* None
*
*****************************************************************************/

/***************************** Include Files ********************************/
#include <asm/xparameters.h>
#include "xgpio.h"
#include "xgpio_i.h"
#include "xstatus.h"

/************************** Constant Definitions ****************************/

/**************************** Type Definitions ******************************/

/***************** Macros (Inline Functions) Definitions ********************/

/************************** Variable Definitions ****************************/

/************************** Function Prototypes *****************************/

/****************************************************************************/
/**
* Initialize the XGpio instance provided by the caller based on the
* given DeviceID.
*
* Nothing is done except to initialize the InstancePtr.
*
* @param InstancePtr is a pointer to an XGpio instance. The memory the pointer
*        references must be pre-allocated by the caller. Further calls to
*        manipulate the component through the XGpio API must be made with this
*        pointer.
*
* @param DeviceId is the unique id of the device controlled by this XGpio
*        component.  Passing in a device id associates the generic XGpio
*        instance to a specific device, as chosen by the caller or application
*        developer.
*
* @return
*
* - XST_SUCCESS           Initialization was successfull.
* - XST_DEVICE_NOT_FOUND  Device configuration data was not found for a device
*                         with the supplied device ID.
*
* NOTES:
*
* None
*
*****************************************************************************/
XStatus
XGpio_Initialize(XGpio * InstancePtr, u16 DeviceId)
{
	XGpio_Config *ConfigPtr;

	/*
	 * Assert arguments
	 */
	XASSERT_NONVOID(InstancePtr != NULL);

	/*
	 * Lookup configuration data in the device configuration table.
	 * Use this configuration info down below when initializing this component.
	 */
	ConfigPtr = XGpio_LookupConfig(DeviceId);
	if (ConfigPtr == (XGpio_Config *) NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	/*
	 * Set some default values.
	 */
	InstancePtr->BaseAddress = ConfigPtr->BaseAddress;

	/*
	 * Indicate the instance is now ready to use, initialized without error
	 */
	InstancePtr->IsReady = XCOMPONENT_IS_READY;
	return (XST_SUCCESS);
}

/******************************************************************************/
/**
* Lookup the device configuration based on the unique device ID.  The table
* ConfigTable contains the configuration info for each device in the system.
*
* @param DeviceID is the device identifier to lookup.
*
* @return
*
* - XGpio configuration structure pointer if DeviceID is found.
* - NULL if DeviceID is not found.
*
******************************************************************************/
XGpio_Config *
XGpio_LookupConfig(u16 DeviceId)
{
	XGpio_Config *CfgPtr = NULL;

	int i;

	for (i = 0; i < XPAR_XGPIO_NUM_INSTANCES; i++) {
		if (XGpio_ConfigTable[i].DeviceId == DeviceId) {
			CfgPtr = &XGpio_ConfigTable[i];
			break;
		}
	}

	return CfgPtr;
}

/****************************************************************************/
/**
* Set the input/output direction of all discrete signals.
*
* @param InstancePtr is a pointer to an XGpio instance to be worked on.
* @param DirectionMask is a bitmask specifying which discretes are input and
*        which are output. Bits set to 0 are output and bits set to 1 are input.
*
* @note
*
* None
*
*****************************************************************************/
void
XGpio_SetDataDirection(XGpio * InstancePtr, u32 DirectionMask)
{
	XASSERT_VOID(InstancePtr != NULL);
	XASSERT_VOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);
	XGpio_mWriteReg(InstancePtr->BaseAddress, XGPIO_TRI_OFFSET,
			DirectionMask);
}

/****************************************************************************/
/**
* Read state of discretes.
*
* @param InstancePtr is a pointer to an XGpio instance to be worked on.
*
* @return Current copy of the discretes register.
*
* @note
*
* None
*
*****************************************************************************/
u32
XGpio_DiscreteRead(XGpio * InstancePtr)
{
	u32 temp;
	XASSERT_NONVOID(InstancePtr != NULL);
	XASSERT_NONVOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);
	temp = XGpio_mReadReg(InstancePtr->BaseAddress, XGPIO_DATA_OFFSET);
	return (XGpio_mReadReg(InstancePtr->BaseAddress, XGPIO_DATA_OFFSET));
}

/****************************************************************************/
/**
* Write to discretes register
*
* @param InstancePtr is a pointer to an XGpio instance to be worked on.
* @param Data is the value to be written to the discretes register.
*
* @note
*
* See also XGpio_DiscreteSet() and XGpio_DiscreteClear().
*
*****************************************************************************/
void
XGpio_DiscreteWrite(XGpio * InstancePtr, u32 Data)
{
	XASSERT_VOID(InstancePtr != NULL);
	XASSERT_VOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);
	XGpio_mWriteReg(InstancePtr->BaseAddress, XGPIO_DATA_OFFSET, Data);
}
