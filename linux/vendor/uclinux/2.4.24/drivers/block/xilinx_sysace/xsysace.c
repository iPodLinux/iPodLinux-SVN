/* $Id: xsysace.c,v 1.1 2003/11/28 05:11:30 gerg Exp $ */
/******************************************************************************
*
*       XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS"
*       AS A COURTESY TO YOU, SOLELY FOR USE IN DEVELOPING PROGRAMS AND
*       SOLUTIONS FOR XILINX DEVICES.  BY PROVIDING THIS DESIGN, CODE,
*       OR INFORMATION AS ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE,
*       APPLICATION OR STANDARD, XILINX IS MAKING NO REPRESENTATION
*       THAT THIS IMPLEMENTATION IS FREE FROM ANY CLAIMS OF INFRINGEMENT,
*       AND YOU ARE RESPONSIBLE FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE
*       FOR YOUR IMPLEMENTATION.  XILINX EXPRESSLY DISCLAIMS ANY
*       WARRANTY WHATSOEVER WITH RESPECT TO THE ADEQUACY OF THE
*       IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OR
*       REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM CLAIMS OF
*       INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*       FOR A PARTICULAR PURPOSE.
*
*       (c) Copyright 2002 Xilinx Inc.
*       All rights reserved.
*
******************************************************************************/
/*****************************************************************************/
/**
*
* @file xsysace.c
*
* The Xilinx System ACE driver component. This driver supports the Xilinx
* System Advanced Configuration Environment (ACE) controller. It currently
* supports only the CompactFlash solution. See xsysace.h for a detailed
* description of the driver.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -----------------------------------------------
* 1.00a rpm  06/17/02 work in progress
* 1.00a rmm  05/14/03 Fixed diab compiler warnings relating to asserts
* </pre>
*
******************************************************************************/

/***************************** Include Files *********************************/

#include <asm/xparameters.h>
#include "xsysace.h"
#include "xsysace_l.h"

/************************** Constant Definitions *****************************/


/**************************** Type Definitions *******************************/


/***************** Macros (Inline Functions) Definitions *********************/


/************************** Function Prototypes ******************************/

static void StubEventHandler(void *CallBackRef, int Event);

/************************** Variable Definitions *****************************/


/*****************************************************************************/
/**
*
* Initialize a specific XSysAce instance. The configuration information for
* the given device ID is found and the driver instance data is initialized
* appropriately.
*
* @param InstancePtr is a pointer to the XSysAce instance to be worked on.
* @param DeviceId is the unique id of the device controlled by this XSysAce
*        instance.
*
* @return
*
* XST_SUCCESS if successful, or XST_DEVICE_NOT_FOUND if the device was not
* found in the configuration table in xsysace_g.c.
*
* @note
*
* We do not want to reset the configuration controller here since this could
* cause a reconfiguration of the JTAG target chain, depending on how the
* CFGMODEPIN of the device is wired.
*
******************************************************************************/
XStatus XSysAce_Initialize(XSysAce *InstancePtr, u16 DeviceId)
{
    XSysAce_Config *ConfigPtr;

    XASSERT_NONVOID(InstancePtr != NULL);

    InstancePtr->IsReady = 0;

    /*
     * Lookup configuration data in the device configuration table.
     * Use this configuration info down below when initializing this component.
     */
    ConfigPtr = XSysAce_LookupConfig(DeviceId);

    if (ConfigPtr == (XSysAce_Config *)NULL)
    {
        return XST_DEVICE_NOT_FOUND;
    }

    /*
     * Set some default values for the instance data
     */
    InstancePtr->BaseAddress = ConfigPtr->BaseAddress;
    InstancePtr->EventHandler = StubEventHandler;
    InstancePtr->NumRequested = 0;
    InstancePtr->NumRemaining = 0;
    InstancePtr->BufferPtr = NULL;

    /*
     * Put the device into 16-bit mode or 8-bit mode depending on compile-time
     * parameter
     */
#if (XPAR_XSYSACE_MEM_WIDTH == 16)
    XSysAce_RegWrite16(InstancePtr->BaseAddress + XSA_BMR_OFFSET,
                       XSA_BMR_16BIT_MASK);
#else
    XSysAce_RegWrite16(InstancePtr->BaseAddress + XSA_BMR_OFFSET, 0);
#endif

    /*
     * Disable interrupts. Interrupts must be enabled by the user using
     * XSysAce_EnableInterrupt(). Put the interrupt request line in reset and
     * clear the interrupt enable bits.
     */
    XSysAce_mOrControlReg(InstancePtr->BaseAddress, XSA_CR_RESETIRQ_MASK);
    XSysAce_mAndControlReg(InstancePtr->BaseAddress, ~(XSA_CR_DATARDYIRQ_MASK |
                           XSA_CR_ERRORIRQ_MASK | XSA_CR_CFGDONEIRQ_MASK));

    /*
     * Indicate the instance is now ready to use, initialized without error
     */
    InstancePtr->IsReady = XCOMPONENT_IS_READY;

    return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* Attempt to lock access to the CompactFlash. The CompactFlash may be accessed
* by the MPU port as well as the JTAG configuration port within the System ACE
* device. This function requests exclusive access to the CompactFlash for the
* MPU port. This is a non-blocking request. If access cannot be locked
* (because the configuration controller has the lock), an appropriate status is
* returned. In this case, the user should call this function again until
* successful.
*
* If the user requests a forced lock, the JTAG configuration controller will
* be put into a reset state in case it currently has a lock on the CompactFlash.
* This effectively aborts any operation the configuration controller had in
* progress and makes the configuration controller restart its process the
* next time it is able to get a lock.
*
* A lock must be granted to the user before attempting to read or write the
* CompactFlash device.
*
* @param InstancePtr is a pointer to the XSysAce instance to be worked on.
* @param Force is a boolean value that, when set to TRUE, will force the MPU
*        lock to occur in the System ACE.  When set to FALSE, the lock is
*        requested and the device arbitrates between the MPU request and
*        JTAG requests. Forcing the MPU lock resets the configuration
*        controller, thus aborting any configuration operations in progress.
*
* @return
*
* XST_SUCCESS if the lock was granted, or XST_DEVICE_BUSY if the lock was
* not granted because the configuration controller currently has access to
* the CompactFlash.
*
* @note
*
* If the lock is not granted to the MPU immediately, this function removes its
* request for a lock so that a lock is not later granted at a time when the
* application is (a) not ready for the lock, or (b) cannot be informed
* asynchronously about the granted lock since there is no such interrupt event.
*
******************************************************************************/
XStatus XSysAce_Lock(XSysAce *InstancePtr, u32 Force)
{
    u32 IsLocked;

    XASSERT_NONVOID(InstancePtr != NULL);
    XASSERT_NONVOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

    /*
     * Check to see if the configuration controller currently has the lock
     */
    IsLocked = (XSysAce_mGetStatusReg(InstancePtr->BaseAddress) &
                XSA_SR_CFGLOCK_MASK);

    if (Force)
    {
        /*
         * Reset the configuration controller if it has the lock. Per ASIC
         * designer, this eliminates a potential deadlock if the FORCELOCK and
         * LOCKREQ bits are both set and the RDYFORCFCMD is not set.
         */
        if (IsLocked)
        {
            /* Reset the configuration controller */
            XSysAce_mOrControlReg(InstancePtr->BaseAddress,
                                  XSA_CR_CFGRESET_MASK);
        }

        /* Force the MPU lock. The lock will occur immediately. */
        XSysAce_mOrControlReg(InstancePtr->BaseAddress, XSA_CR_LOCKREQ_MASK |
                              XSA_CR_FORCELOCK_MASK);
    }
    else
    {
        /*
         * Check to see if the configuration controller has the lock. If so,
         * return a busy status.
         */
        if (IsLocked)
        {
            return XST_DEVICE_BUSY;
        }

        /* Request the lock, but do not force it */
        XSysAce_mOrControlReg(InstancePtr->BaseAddress, XSA_CR_LOCKREQ_MASK);
    }

    /*
     * See if the lock was granted. Note that it is guaranteed to occur if
     * the user forced it.
     */
    if (!XSysAce_mIsMpuLocked(InstancePtr->BaseAddress))
    {
        /* Lock was not granted, so remove request and return a busy */
        XSysAce_mAndControlReg(InstancePtr->BaseAddress,
                               ~(XSA_CR_LOCKREQ_MASK | XSA_CR_FORCELOCK_MASK));

        return XST_DEVICE_BUSY;
    }

    /*
     * Lock has been granted.
     *
     * If the configuration controller had the lock and has been reset,
     * go ahead and release it from reset as it will not be able to get
     * the lock again until the MPU lock is released.
     */
    if (IsLocked && Force)
    {
        /* Release the reset of the configuration controller */
        XSysAce_mAndControlReg(InstancePtr->BaseAddress, ~XSA_CR_CFGRESET_MASK);
    }

    return XST_SUCCESS;
}


/*****************************************************************************/
/**
*
* Release the MPU lock to the CompactFlash. If a lock is not currently granted
* to the MPU port, this function has no effect.
*
* @param InstancePtr is a pointer to the XSysAce instance to be worked on.
*
* @return
*
* None.
*
* @note
*
* None.
*
******************************************************************************/
void XSysAce_Unlock(XSysAce *InstancePtr)
{
    XASSERT_VOID(InstancePtr != NULL);
    XASSERT_VOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

    /*
     * Blindly clear the lock and force-lock request bits of the control
     * register
     */
    XSysAce_mAndControlReg(InstancePtr->BaseAddress,
                ~(XSA_CR_LOCKREQ_MASK | XSA_CR_FORCELOCK_MASK));
}

/*****************************************************************************/
/**
*
* Get all outstanding errors. Errors include the inability to read or write
* CompactFlash and the inability to successfully configure FPGA devices along
* the target FPGA chain.
*
* @param InstancePtr is a pointer to the XSysAce instance to be worked on.
*
* @return
*
* A 32-bit mask of error values. See xsysace_l.h for a description of possible
* values. The error identifiers are prefixed with XSA_ER_*.
*
* @note
*
* None.
*
******************************************************************************/
u32 XSysAce_GetErrors(XSysAce *InstancePtr)
{
    XASSERT_NONVOID(InstancePtr != NULL);
    XASSERT_NONVOID(InstancePtr->IsReady == XCOMPONENT_IS_READY);

    return XSysAce_mGetErrorReg(InstancePtr->BaseAddress);
}

/*****************************************************************************/
/**
*
* Stub for the asynchronous event callback. The stub is here in case the upper
* layers forget to set the handler.
*
* @param    CallBackRef is a pointer to the upper layer callback reference
* @param    Event is the event that occurs
*
* @return
*
* None.
*
* @note
*
* None.
*
******************************************************************************/
static void StubEventHandler(void *CallBackRef, int Event)
{
    XASSERT_VOID_ALWAYS();
}

/*****************************************************************************/
/**
*
* Looks up the device configuration based on the unique device ID. The table
* XSysAce_ConfigTable contains the configuration info for each device in the
* system.
*
* @param DeviceId is the unique device ID to look for.
*
* @return
*
* A pointer to the configuration data for the device, or NULL if no match is
* found.
*
* @note
*
* None.
*
******************************************************************************/
XSysAce_Config *XSysAce_LookupConfig(u16 DeviceId)
{
    extern XSysAce_Config XSysAce_ConfigTable[];
    XSysAce_Config *CfgPtr = NULL;
    int i;

    for (i=0; i < XPAR_XSYSACE_NUM_INSTANCES; i++)
    {
        if (XSysAce_ConfigTable[i].DeviceId == DeviceId)
        {
            CfgPtr = &XSysAce_ConfigTable[i];
            break;
        }
    }

    return CfgPtr;
}

