/****************************************************************************
*
*	Name:			gpio.c
*
*	Description:	This module contains the General Purpose I/O (GPIO) functions.
*
*	Copyright:		(c) 2002 Conexant Systems Inc.
*
*****************************************************************************

  This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.

  This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

  You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc., 59
Temple Place, Suite 330, Boston, MA 02111-1307 USA

****************************************************************************
*  $Author: davidm $
*  $Revision: 1.1 $
*  $Modtime: 9/20/02 6:43p $
****************************************************************************/

/*
| Module Name:  gpio.c
| Language:     ARM C
|
| Description:  This module is the General Purpose I/O (GPIO) functions.
|
|               GPIO pins and register interface have two modes.
|               The default mode consists of each set of 8 pins being
|               controlled by an individual GP{X}_IO regiser.
|
|        The GPIO register map are:
|
|        GP0_IO =  0x00350010
|        GP1_IO =  0x00350014
|        GP2_IO =  0x00350018
|        GP3_IO =  0x0035001C
|
|        Bits       Type     Default    Name        Description
|        [29:28]    RW       0          GP3_ECM     Event Counter Mode for Timer #4:
|                                                   00 = normal GPIO[22]
|                                                   01 = posedge event cntr for
|                                                        GPIO[22] input.
|                                                   10 = negedge event cntr for
|                                                        GPIO[22] input.
|                                                   11 = timer sq-wave output on
|                                                        GPIO[22].
|        [27]       RW       0          GP3_ISM     GPIO interrupt sensitivity mode:
|                                                   0 = active-lo level-sensitive.
|                                                   1 = negative (hi-to-lo) edge-sensitive
|                                                   This control bit affects the mode
|                                                   for all GPIO[25:23] interrupt inputs.
|        [26]       RW       0          GP3_RFP     A value of 1 enables GPIO[13:7]
|                                                   to be driven with internal RF
|                                                   power control signals from CUP.
|        [25]       RW       0          GP3_MODE    A value of 0 enables the GPIO
|                                                   register interface in default
|                                                   mode where each set of 8 pins
|                                                   are controlled by a GP{x}_IO
|                                                   register. A value of 1 enables
|                                                   all 32 GPIO and/or GPOE bits to
|                                                   be read/write simultaneously.
|        [24]      RW        0          GP3_I2C     A value of 1 enables GPIO[31:30]
|                                                   {SDA, SCL} to meet I2C transceiver
|                                                   electrical requirements.
|        [23:16]   Wd                   GP{X}_BWE   If this field is equal to 0x00,
|                                                   then the whole GPIO byte register
|                                                   operates in normal RW mode. If any
|                                                   bit is set, then the corresponding
|                                                   GP_OE and GP_IO bit locations are
|                                                   enabled for writing. If the bit
|                                                   write enable is not set, the
|                                                   corresponding GPIO bits will be
|                                                   unaffected.
|        [15:8]    RW        0          GP{x}_OE    A value of 1 enables corresponding
|                                                   GP_IO bit to be output on the GPIO
|                                                   pin.
|        [7:0]     RW        0          GP{x}_IO    Writing provides data for GPIO
|                                                   output pin drivers. Reading accesses
|                                                   data directly from input pin buffers.
|
|        GPIO =  0x00350010
|
|        Bits      Type      Default    Name        Description
|        [31:0]    RW        0x00000000 GP_IO       Writing provides data for GPIO output
|                                                   pin drivers. Reading accesses data
|                                                   directly from input pin buffers.
|
|        GPOE = 0x00350014
|
|        Bits      Type      Default    Name        Description
|        [31:0]    RW        0x00000000 GP_OE       A value of 1 enables corresponding GP_IO
|                                                   bit to be output on the GPIO pin.
|
|
| Included Subprograms:
|        GPIO_STATUS GPIO_Init(void)
|        GPIO_STATUS GPIO_Config(GPIO_MODE eMode)
|        GPIO_STATUS GPIO_32Bit_Write(UINT32 ulData, UINT32 ulOEnable)
|        UINT32      GPIO_32Bit_Read(UINT32 ulDisableOE)
|        GPIO_STATUS GPIO_8Bit_Write(volatile UINT32 *pulReg, UINT32 ulData, UINT8 ucOEnable)
|        UINT8       GPIO_8Bit_Read(volatile UINT32 *pulReg, UINT8 ucDisableOE)
|        GPIO_STATUS GPIO_Timer4_Event_Counter_Mode (TIMER4_EVENT_COUNTER_MODE eMode)
|        GPIO_STATUS GPIO_Int_Sensitivity_Mode (GPIO_INT_SENSITIVITY_MODE eMode)
|        GPIO_STATUS GPIO_RF_Power_Control (RF_POWER_CNTRL_MODE eMode)
|        GPIO_STATUS GPIO_I2C_Mode (I2C_MODE eMode)
|
+==========================================================================+
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <asm/arch/bspcfg.h>
#include <asm/arch/bsptypes.h>
#include <asm/arch/gpio.h>
#include <asm/arch/cnxtirq.h>
#include <asm/arch/cnxtbsp.h>
#include <asm/arch/syslib.h>

/* Exported Global Variable Declaration */

/* Local Global Variable Declaration */

/* Local Function Prototypes Declaration */

/*
+==========================================================================+
|                            CONEXANT SYSTEMS                              |
|                      PROPRIETARY AND CONFIDENTIAL                        |
|                           SUBPROGRAM HEADER                              |
+==========================================================================+
|
| Name:         GPIO_Init
|
| Description:  This routine initializes the GPIO registers.
|
| Parameters:   None.
|
| Return:       GPIO_STATUS - GPIO_STATUS_OK.
|                             GPIO_STATUS_ERROR.
|
+==========================================================================+
*/
GPIO_STATUS GPIO_Init(void)
{
	signed int	int_lvl;
	static unsigned char GpioCallCnt = 0;

#ifdef DEVICE_YELLOWSTONE
    *p_GRP3_INT_MASK       = 0xFFFF0000;
    *p_GRP3_OUTPUT_ENABLE  = 0xFFFF0000;
    *p_GRP3_INPUT_ENABLE   = 0xFFFF0000;
    *p_GRP3_DATA_OUT       = 0xFFFF0000;
    *p_GRP3_INT_EVENT_TYPE = 0xFFFF0000;
    *p_GRP3_INT_ENABLE_POS = 0xFFFF0000;
    *p_GRP3_INT_ENABLE_NEG = 0xFFFF0000;
    *p_GRP3_INT_STAT_POS   = 0xFFFF;
    *p_GRP3_INT_STAT_NEG   = 0xFFFF;
    *p_GRP3_INT_STAT       = 0xFFFF;
    *p_GRP3_INT_ASSIGN_A   = 0xFFFF0000;
    *p_GRP3_INT_ASSIGN_B   = 0xFFFF0000;
    *p_GRP3_INT_ASSIGN_C   = 0xFFFF0000;
    *p_GRP3_INT_ASSIGN_D   = 0xFFFF0000;

    *p_GRP2_INT_MASK       = 0xFFFF0000;
    *p_GRP2_OUTPUT_ENABLE  = 0xFFFF0000;
    *p_GRP2_INPUT_ENABLE   = 0xFFFF0000;
    *p_GRP2_DATA_OUT       = 0xFFFF0000;
    *p_GRP2_INT_EVENT_TYPE = 0xFFFF0000;
    *p_GRP2_INT_ENABLE_POS = 0xFFFF0000;
    *p_GRP2_INT_ENABLE_NEG = 0xFFFF0000;
    *p_GRP2_INT_STAT_POS   = 0xFFFF;
    *p_GRP2_INT_STAT_NEG   = 0xFFFF;
    *p_GRP2_INT_STAT       = 0xFFFF;
    *p_GRP2_INT_ASSIGN_A   = 0xFFFF0000;
    *p_GRP2_INT_ASSIGN_B   = 0xFFFF0000;
    *p_GRP2_INT_ASSIGN_C   = 0xFFFF0000;
    *p_GRP2_INT_ASSIGN_D   = 0xFFFF0000;

    *p_GRP1_INT_MASK       = 0xFFFF0000;
    *p_GRP1_OUTPUT_ENABLE  = 0xFFFF0000;
    *p_GRP1_INPUT_ENABLE   = 0xFFFF0000;
    *p_GRP1_DATA_OUT       = 0xFFFF0000;
    *p_GRP1_INT_EVENT_TYPE = 0xFFFF0000;
    *p_GRP1_INT_ENABLE_POS = 0xFFFF0000;
    *p_GRP1_INT_ENABLE_NEG = 0xFFFF0000;
    *p_GRP1_INT_STAT_POS   = 0xFFFF;
    *p_GRP1_INT_STAT_NEG   = 0xFFFF;
    *p_GRP1_INT_STAT       = 0xFFFF;
    *p_GRP1_INT_ASSIGN_A   = 0xFFFF0000;
    *p_GRP1_INT_ASSIGN_B   = 0xFFFF0000;
    *p_GRP1_INT_ASSIGN_C   = 0xFFFF0000;
    *p_GRP1_INT_ASSIGN_D   = 0xFFFF0000;

    *p_GRP0_INT_MASK       = 0xFFFF0000;
    *p_GRP0_OUTPUT_ENABLE  = 0xFFFF0000;
    *p_GRP0_INPUT_ENABLE   = 0xFFFF0000;
    *p_GRP0_DATA_OUT       = 0xFFFF0000;
    *p_GRP0_INT_EVENT_TYPE = 0xFFFF0000;
    *p_GRP0_INT_ENABLE_POS = 0xFFFF0000;
    *p_GRP0_INT_ENABLE_NEG = 0xFFFF0000;
    *p_GRP0_INT_STAT_POS   = 0xFFFF;
    *p_GRP0_INT_STAT_NEG   = 0xFFFF;
    *p_GRP0_INT_STAT       = 0xFFFF;
    *p_GRP0_INT_ASSIGN_A   = 0xFFFF0000;
    *p_GRP0_INT_ASSIGN_B   = 0xFFFF0000;
    *p_GRP0_INT_ASSIGN_C   = 0xFFFF0000;
    *p_GRP0_INT_ASSIGN_D   = 0xFFFF0000;

#else
   register UINT32 i = 0;

   /* Should come from EEPROM in the future */
   UINT32 uGPIODefaultValue[RUSHMORE_NUM_GPIODEFAULT] =
   {
      0x00000000, /*p_GRP0_ISM0: default to level IRQ*/
      0x00000000, /*p_GRP0_ISM1: default to level IRQ*/
      0x00000000, /*p_GRP0_ISM2: default to level IRQ*/

      #if defined(CONFIG_CHIP_P52) || defined(CONFIG_CHIP_CX82110)
            0x00000042,	/* GPIO_OPT: Enable all chip selects except GPIO 33 for outer relay */
            0x00003380,	/* p_GRP0_OUTPUT_ENABLE */
            0x000084bd,	/* p_GRP1_OUTPUT_ENABLE */	   /* 0x49C GPIO 21 output for inner relay */
            0x0000007F,	/* p_GRP2_OUTPUT_ENABLE */      /* GPIO38 output */
            0xffff3040,	/* p_GRP0_DATA_OUT */
            0xffff049C,	/* p_GRP1_DATA_OUT, 18, 19, and 20 LED OFF */
      #endif
      #ifdef CONFIG_CHIP_CX82100
	      0x000000c0, /*GPIO_OPT: Enable all chip selects */

	      0x000001e0, /*p_GRP0_OUTPUT_ENABLE */	
	      0x0000011d, /*p_GRP1_OUTPUT_ENABLE */	
	      0x00000000, /*p_GRP2_OUTPUT_ENABLE */	
	      0xffff01e0, /*p_GRP0_DATA_OUT */	
	      0xffff011c, /*p_GRP1_DATA_OUT */	
      #endif

      	0xffff003F,	/*p_GRP2_DATA_OUT */

		0xffffffff,	/*p_GRP0_INT_STAT */
		0xffffffff,	/*p_GRP1_INT_STAT */
		0xffffffff,	/*p_GRP2_INT_STAT */
		0xffff0000,	/*p_GRP0_INT_MASK */
		0xffff0000,	/*p_GRP1_INT_MASK */
        0xffff0000, /*p_GRP2_INT_MASK */
		0xffff0000,	/*GPIO_IPC1 */
		0xffff0000,	/*GPIO_IPC2 */
		0xffff0000,	/*GPIO_IPC3 */

        /*RUSHMORE*/
		0x00000000,	/*p_GRP0_INPUT_ENABLE */
		0x00000240,	/*p_GRP1_INPUT_ENABLE */
		0x00000080,	/*p_GRP2_INPUT_ENABLE */

	};

	volatile UINT32 *pGPIODefaultReg[RUSHMORE_NUM_GPIODEFAULT] =
	{
      p_GRP0_ISM0,
      p_GRP0_ISM1,
      p_GRP0_ISM2,

      GPIO_OPT,
      p_GRP0_OUTPUT_ENABLE,
      p_GRP1_OUTPUT_ENABLE,
      p_GRP2_OUTPUT_ENABLE,
      p_GRP0_DATA_OUT,
      p_GRP1_DATA_OUT,
      p_GRP2_DATA_OUT,
      p_GRP0_INT_STAT,
      p_GRP1_INT_STAT,
      p_GRP2_INT_STAT,
      p_GRP0_INT_MASK,
      p_GRP1_INT_MASK,
      p_GRP2_INT_MASK,
      GPIO_IPC1,
      GPIO_IPC2,
      GPIO_IPC3,

      /* RUSHMORE GPIO Registers. These are here even if device is not
	     Rushmore, but in that case, they are not initialized */
      p_GRP0_INPUT_ENABLE,
      p_GRP1_INPUT_ENABLE,
      p_GRP2_INPUT_ENABLE,
    };

    GpioCallCnt++;

	if (HWSTATE.eDeviceType >= DEVICE_RUSHMORE)
	{
		if( GpioCallCnt >=2 )
		{
			if( SYS_bDUALEMAC_Support() == TRUE )
			{
				int_lvl = intLock ();
				*GPIO_OPT = 0x00000042;
				*p_GRP0_DATA_OUT = 0xffff30E0;
				HWSTATE.p_GRP0_INPUT_ENABLE_Image = *p_GRP0_OUTPUT_ENABLE = 0x000033E0;
				( void ) intUnlock ( int_lvl );
			}
		}
		else
		{
			for (i = 0; i < RUSHMORE_NUM_GPIODEFAULT; i++ )
			{
				*pGPIODefaultReg[i] = uGPIODefaultValue[i];

				/* In Rushmore the 3 new GIO_IE register cannot be read. */
				/* Thus we have keep an image of each to write to them   */
				/* without overwritten the previous value.               */
				switch ((UINT32) pGPIODefaultReg[i])
				{
					case  (UINT32) p_GRP0_INPUT_ENABLE:
						HWSTATE.p_GRP0_INPUT_ENABLE_Image = uGPIODefaultValue[i];
						break;

					case  (UINT32) p_GRP1_INPUT_ENABLE:
						HWSTATE.p_GRP1_INPUT_ENABLE_Image = uGPIODefaultValue[i];
						break;

					case  (UINT32) p_GRP2_INPUT_ENABLE:
						HWSTATE.p_GRP2_INPUT_ENABLE_Image = uGPIODefaultValue[i];
						break;

					default:
						break;
				}
			}
		}
	}
	else /* eDeviceType < RUSHMORE */
	{
		if( GpioCallCnt >=2 )
		{
			if( SYS_bDUALEMAC_Support() == TRUE )
			{
				int_lvl = intLock ();
				*GPIO_OPT = 0x00000042 & ( ~0x2 );
				*p_GRP0_DATA_OUT = 0xffff30E0;
				*p_GRP0_OUTPUT_ENABLE = 0x000033E0;
				( void ) intUnlock ( int_lvl );
			}
		}
		else
		{
			for (i = 0; i < NUM_OF_GPIODEFAULT; i++)
			{
				if ( (UINT32 )GPIO_OPT == pGPIODefaultReg[i] && HWSTATE.eDeviceType != DEVICE_P5200X10)
					*pGPIODefaultReg[i] = uGPIODefaultValue[i] & ( ~0x2 );
				else
					*pGPIODefaultReg[i] = uGPIODefaultValue[i];
			}
		}
	}

	#if defined(BOARD_BRONX_MACKINAC)
	if( SYS_bADSL_Support() == FALSE )
	{
		/* Set GPIO31 as output port, OE2 */
		*((UINT32*)0x3500b8) = *((UINT32*)0x3500b8) | 0x80008000;

		/* Out 1 to GPIO31 to reset PCMCIA */
		*((UINT32*)0x3500d0) = 0xffff841C;
		for(i=0;i<0x30ffff;i++);
		/* Out 0 to GPIO31 to release reset */
		*((UINT32*)0x3500d0) = 0xffff041C;
		for(i=0;i<0x30ffff;i++);
	}
	#endif   /* End of BOARD_BRONX_MACKINAC */

#endif

   return (GPIO_STATUS_OK);
}

/****************************************************************************
 *
 *  Name:			void SetGPIOUsage( UINT8 uGPIOPin, BOOL bOpt )
 *
 *  Description:	Sets the GPIO Usage via the option register
 *
 *  Inputs:			UINT8 uGPIOPin = which GPIO
 *					BOOL bOpt = option
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void SetGPIOUsage( UINT8 uGPIOPin, BOOL bOpt )
{
#ifndef DEVICE_YELLOWSTONE
	/*assert ( (uGPIOPin > 31) && (uGPIOPin < NUM_OF_GPIOS) );*/

	if (uGPIOPin > 31)	/*only GPIOs 32-39 have options available */
	{
		if ( 0 == bOpt )
		{
			*GPIO_OPT &= ~( 1 << (uGPIOPin - 32));
		}
		else
		{
			*GPIO_OPT |= ( 1 << (uGPIOPin - 32));
		}
	}
#endif
}


/****************************************************************************
 *
 *  Name:			void SetGPIODir( UINT8 uGPIOPin, BOOL bDir )
 *
 *  Description:	Sets the GPIO direction
 *
 *  Inputs:			UINT8 uGPIOPin = which GPIO
 *					   BOOL bDir = direction
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void SetGPIODir( UINT8 uGPIOPin, BOOL bDir )
{
	UINT32 uBitPos = 0;
   UINT32 uValue;
	volatile	UINT32* pGPIOOutPtr = 0;
	volatile	UINT32* pGPIOInPtr = 0;
	volatile	UINT32* pImagePtr = 0;
   UINT32           ByteOffSet = 0;

   if (uGPIOPin < NUM_OF_GPIOS)
   {
      ByteOffSet = ((uGPIOPin / NUM_GPIOS_PER_REGISTER) * GRP_REG_OFFSET);

      // Calculate corresponding GPIO Output Enable register.  Use byte pointer addition
      // to match HW register definitions.
      pGPIOOutPtr = (volatile UINT32 *) ((volatile UINT8 *)(p_GRP0_OUTPUT_ENABLE) + ByteOffSet);

      if (HWSTATE.eDeviceType >= DEVICE_RUSHMORE)
      {
         // Calculate corresponding GPIO Output Enable register.  Use byte pointer addition
         // to match HW register definitions.
         pGPIOInPtr = (volatile UINT32 *) ((volatile UINT8 *)(p_GRP0_INPUT_ENABLE) + ByteOffSet);

         pImagePtr = (&HWSTATE.p_GRP0_INPUT_ENABLE_Image) + (uGPIOPin / NUM_GPIOS_PER_REGISTER);
      }

      // Calculate bit position within the register.
      uBitPos  = (uGPIOPin % NUM_GPIOS_PER_REGISTER);

      /************************/
      #ifdef DEVICE_YELLOWSTONE
      /************************/

      // Upper WORD of register is the write mask to access
      // the desired bit in the lower WORD of register.
      // So first set the write mask for the desired bit.
      uValue =  1 << (uBitPos + 16);

      if ( GP_INPUT == bDir )
      {
         // Disable GPIO output driver bit by writing 0 to lower WORD
         HW_REG_WRITE(pGPIOOutPtr, uValue);

         if (pGPIOInPtr)
         {
            // Keep same mask and enable GPIO input receiver bit by writing 1
            // to lower WORD
   		   uValue |= (1 << uBitPos);
            HW_REG_WRITE(pGPIOInPtr, uValue);
         }
      }

      else
      {
         if (pGPIOInPtr)
         {
            // Disable GPIO input receiver bit by writing 0 to lower WORD
            HW_REG_WRITE(pGPIOInPtr, uValue);
         }

         // Keep same mask and enable GPIO output driver bit by writing 1
         // to lower WORD
  		   uValue |= (1 << uBitPos);
         HW_REG_WRITE(pGPIOOutPtr, uValue);
      }

      /************************/
      #else
      /************************/

      uValue =  1 << uBitPos;

	   if ( GP_INPUT == bDir )
	   {
         // Disable GPIO output driver
         *pGPIOOutPtr &= ~uValue;

         if (pGPIOInPtr)
         {
		#if 0
            // Enable GPIO input receiver
            *pGPIOInPtr  |= uValue;
            // Update SW image of GPIO Input Enable register
            *pImagePtr    = *pGPIOInPtr;
		#else
            // Enable GPIO input receiver
            // Update SW image of GPIO Input Enable register
            *pImagePtr  |= uValue;

            // Update GPIO Input Enable register
            *pGPIOInPtr = *pImagePtr;
		#endif
         }
	   }

	   else
	   {
         if (pGPIOInPtr)
         {
		#if 0
            // Disable GPIO input receiver
            *pGPIOInPtr  &= ~uValue;
            // Update SW image of GPIO Input Enable register
            *pImagePtr    = *pGPIOInPtr;
		#else
            // Disable GPIO input receiver
            // Update SW image of GPIO Input Enable register
            *pImagePtr   &= ~uValue;
			
            // Update GPIO Input Enable register
            *pGPIOInPtr = *pImagePtr;
		#endif
         }

         // Enable GPIO output driver
         *pGPIOOutPtr |= uValue;
	   }
      #endif
   }
}



/****************************************************************************
 *
 *  Name:			void SetGPIOInputEnable( UINT8 uGPIOPin, BOOL bState )
 *
 *  Description:	Sets the GPIO direction
 *
 *  Inputs:			UINT8 uGPIOPin = which GPIO
 *					BOOL bDir = direction
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void SetGPIOInputEnable( UINT8 uGPIOPin, BOOL bState )
{
   /* New register in Rushmore and OldFaithful devices to enable input. */
   UINT32 uBitPos;
   UINT32 uValue;
   UINT32 ByteOffSet = 0;
   volatile	UINT32* pGPIOInPtr;
   volatile	UINT32* pImagePtr;

   if ( (HWSTATE.eDeviceType >= DEVICE_RUSHMORE) && (uGPIOPin < NUM_OF_GPIOS) )
   {
      ByteOffSet = ((uGPIOPin / NUM_GPIOS_PER_REGISTER) * GRP_REG_OFFSET);
      // Calculate corresponding GPIO Output Enable register.  Use byte pointer addition
      // to match HW register definitions.
      pGPIOInPtr = (volatile UINT32 *) ((volatile UINT8 *)(p_GRP0_INPUT_ENABLE) + ByteOffSet);
      pImagePtr = (&HWSTATE.p_GRP0_INPUT_ENABLE_Image) + (uGPIOPin / NUM_GPIOS_PER_REGISTER);

      // Calculate bit position within the register.
      uBitPos  = (uGPIOPin % NUM_GPIOS_PER_REGISTER);

      /************************/
      #ifdef DEVICE_YELLOWSTONE
      /************************/

      // Upper WORD of register is the write mask to access
      // the desired bit in the lower WORD of register.
      // So first set the write mask for the desired bit.
      uValue =  1 << (uBitPos + 16);

	   if ( GP_INPUTON == bState )
	   {
          // Keep same mask and enable GPIO input receiver bit by writing 1
         // to lower WORD
         uValue |= (1 << uBitPos);
         HW_REG_WRITE(pGPIOInPtr, uValue);
      }

      else
      {
         // Disable GPIO input receiver bit by writing 0 to lower WORD
         uValue &= (~(1 << uBitPos));
         HW_REG_WRITE(pGPIOInPtr, uValue);
      }

      /************************/
      #else
      /************************/

      uValue =  (1 << uBitPos);

      if ( GP_INPUTON == bState )
      {
         *pImagePtr |= uValue;
      }

      else
      {
         *pImagePtr &= ~uValue;
      }

      *pGPIOInPtr = *pImagePtr;

      #endif
   }  /*End if DEVICE_RUSHMORE*/
}


/****************************************************************************
 *
 *  Name:			void WriteGPIOData( UINT8 uGPIOPin, BOOL bState )
 *
 *  Description:	Sets the state of a GPIO output pin
 *
 *  Inputs:			UINT8 uGPIOPin = which GPIO
 *					BOOL bState = pin state
 *
 *  Outputs:		none
 *
 ***************************************************************************/

/* The register bit position setting/resetting.
 * Note that only mask bit is important since it should "mask"
 * off setting/resetting of other bits in the register
 * value to write to register for direction
 */
void WriteGPIOData( UINT8 uGPIOPin, BOOL bState )
{
   UINT32 uBitPos = 0;
   UINT32 uValue = 0;
   UINT32 ByteOffSet = 0;
   volatile UINT32* pGPIOOutPtr = 0;
   int    OldLevel = 0;


   if (uGPIOPin < NUM_OF_GPIOS)
   {
      ByteOffSet = ((uGPIOPin / NUM_GPIOS_PER_REGISTER) * GRP_REG_OFFSET);

      // Calculate corresponding GPIO Output Enable register.  Use byte pointer addition
      // to match HW register definitions.
      pGPIOOutPtr = (volatile UINT32 *) ((volatile UINT8 *)(p_GRP0_DATA_OUT) + ByteOffSet);

      // Calculate bit position within the register.
      uBitPos  = (uGPIOPin % NUM_GPIOS_PER_REGISTER);

      /************************/
      #ifdef DEVICE_YELLOWSTONE
      /************************/

      // Upper WORD of register is the write mask to access
      // the desired bit in the lower WORD of register.
      // So first set the write mask for the desired bit.
      uValue =  1 << (uBitPos + 16);

      if ( 0 != bState )
      {
         uValue |= (1 << uBitPos);
      }

      else
      {
         uValue &= (~(1 << uBitPos));
      }

      HW_REG_WRITE(pGPIOOutPtr, uValue);

      /************************/
      #else
      /************************/

      /* Set all mask bits and do read-modify-write due to register 2 mask bug */
      OldLevel = intLock();
      HW_REG_READ(pGPIOOutPtr, uValue);
      uValue |= 0xFFFF0000;

      if ( 0 != bState )		/*uValue  0 in lower 16 bits, only check for positive*/
      {
         uValue |= (1 << uBitPos);
      }
      else
      {
         uValue &= ~(1 << uBitPos);
      }

      HW_REG_WRITE(pGPIOOutPtr, uValue);
      intUnlock(OldLevel);
      #endif
   }
}


/****************************************************************************
 *
 *  Name:			BOOL ReadGPIOData( UINT8 uGPIOPin )
 *
 *  Description:	Reads a GPIO input pin
 *
 *  Inputs:			UINT8 uGPIOPin = which GPIO
 *
 *  Outputs:		Returns BOOL pin state
 *
 ***************************************************************************/
BOOL ReadGPIOData( UINT8 uGPIOPin )
{
	UINT32 uBitPos = 0;
	UINT32 uValue = 0;
	volatile UINT32* pGPIOInPtr = 0;
	UINT32 ByteOffSet = 0;

	if (uGPIOPin < NUM_OF_GPIOS)
	{
		ByteOffSet = ((uGPIOPin / NUM_GPIOS_PER_REGISTER) * GRP_REG_OFFSET);

		// Calculate corresponding GPIO Output Enable register.  Use byte pointer addition
		// to match HW register definitions.
		pGPIOInPtr = (volatile UINT32 *) ((volatile UINT8 *)(p_GRP0_DATA_IN) + ByteOffSet);

		// Calculate bit position within the register.
		uBitPos  = (uGPIOPin % NUM_GPIOS_PER_REGISTER);

		HW_REG_READ(pGPIOInPtr, uValue);

		if( uValue & (1 << uBitPos) )
		{
			return TRUE;
		}
	}
	return FALSE;
}

/****************************************************************************
 *
 *  Name:			BOOL ReadGPIOIntStatus( UINT8 uGPIOPin )
 *
 *  Description:	Reads a GPIO pin interrupt status bit
 *
 *  Inputs:			UINT8 uGPIOPin = which GPIO
 *
 *  Outputs:		Returns BOOL interrupt status
 *
 ***************************************************************************/
BOOL ReadGPIOIntStatus( UINT8 uGPIOPin )
{
	register UINT32 uBitPos;
	register volatile UINT32* pGPIOPtr;
	register BOOL  bIntState = 0;

	if (uGPIOPin < NUM_OF_GPIOS)
   {
	   if (uGPIOPin > 31)
	   {
		   uBitPos = uGPIOPin - 32;
		   pGPIOPtr = p_GRP2_INT_STAT;
	   }
	   else if (uGPIOPin > 15)
	   {
		   uBitPos = uGPIOPin - 16;
		   pGPIOPtr = p_GRP1_INT_STAT;
	   }
	   else
	   {
		   uBitPos = uGPIOPin;
		   pGPIOPtr = p_GRP0_INT_STAT;
	   }

	   if ( *pGPIOPtr & (1 << uBitPos) )
	   {
		   bIntState = 1;
	   }
	   else
	   {
		   bIntState = 0;
	   }
   }

	return bIntState;
}

/****************************************************************************
 *
 *  Name:			void ClearGPIOIntStatus( UINT8 uGPIOPin )
 *
 *  Description:	Clears the interrupt associated with the GPIO
 *
 *  Inputs:			UINT8 uGPIOPin = which GPIO
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void ClearGPIOIntStatus( UINT8 uGPIOPin )
{
	UINT32 uBitPos;
   UINT32 ByteOffSet; 
   volatile UINT32* pGPIOStatPtr;

	if (uGPIOPin < NUM_OF_GPIOS)
   {
      ByteOffSet = ((uGPIOPin / NUM_GPIOS_PER_REGISTER) * GRP_REG_OFFSET);

      // Calculate corresponding GPIO Output Enable register.  Use byte pointer addition
      // to match HW register definitions.
      pGPIOStatPtr = (volatile UINT32 *) ((volatile UINT8 *)(p_GRP0_INT_STAT) + ByteOffSet);

      // Calculate bit position within the register.
      uBitPos  = (uGPIOPin % NUM_GPIOS_PER_REGISTER);

      HW_REG_WRITE(pGPIOStatPtr, (1 << uBitPos));    /* writing a 1 clears the status */
   }
}

/****************************************************************************
 *
 *  Name:			void SetGPIOIntPolarity( UINT8 uGPIOPin, BOOL bPolarity )
 *
 *  Description:	Sets GPIO interrupt polarity
 *
 *  Inputs:			UINT8 uGPIOPin = which GPIO
 *					BOOL bPolarity = polarity (1 = rising edge,
 *											   0 = falling edge)
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void SetGPIOIntPolarity( UINT8 uGPIOPin, BOOL bPolarity )
{
#ifndef DEVICE_YELLOWSTONE

	register UINT32  uBitPos;
	register UINT32 uValue;
	register volatile UINT32* pGPIOPtr;
    register int OldLevel;

	uValue = 0;

	/*assert ( (uGPIOPin > =0) && (uGPIOPin < NUM_OF_GPIOS) );*/

	if (uGPIOPin > 31)
	{
		uBitPos = uGPIOPin - 32;
		pGPIOPtr = GPIO_IPC3;
	}
	else if (uGPIOPin > 15)
	{
		uBitPos = uGPIOPin - 16;
		pGPIOPtr = GPIO_IPC2;
	}
	else
	{
		uBitPos = uGPIOPin;
		pGPIOPtr = GPIO_IPC1;
	}


    /* Upper word is the IPC bit mask */
    /* Lower work is the IPC bit level polarity */
	/* Set all mask bits and do read-modify-write due to register mask bug */
    OldLevel = intLock();
	uValue = *pGPIOPtr | 0xFFFF0000;

	if ( GP_IRQ_POL_POSITIVE ==  bPolarity )	/*uValue  0 in lower 16 bits, only check for positive */
	{
		uValue |= (1 << uBitPos);
	}

   else
	{
		uValue &= ~(1 << uBitPos);
	}

	*pGPIOPtr = uValue;
    intUnlock(OldLevel);
#endif
}

/****************************************************************************
 *
 *  Name:			BOOL ReadGPIOIntPolarity( UINT8 uGPIOPin )
 *
 *  Description:	Reads a GPIO pin interrupt polarity control bit
 *
 *  Inputs:			UINT8 uGPIOPin = which GPIO
 *
 *  Outputs:		Returns BOOL polarity control state
 *
 ***************************************************************************/
BOOL ReadGPIOIntPolarity( UINT8 uGPIOPin )
{
	register BOOL  bIntState = 0;
#ifndef DEVICE_YELLOWSTONE
	register UINT32 uBitPos;
	register volatile UINT32* pGPIOPtr;

	/*assert ( (uGPIOPin >= 0) && (uGPIOPin < NUM_OF_GPIOS) );*/

	if (uGPIOPin > 31)
	{
		uBitPos = uGPIOPin - 32;
		pGPIOPtr = GPIO_IPC3;
	}
	else if (uGPIOPin > 15)
	{
		uBitPos = uGPIOPin - 16;
		pGPIOPtr = GPIO_IPC2;
	}
	else
	{
		uBitPos = uGPIOPin;
		pGPIOPtr = GPIO_IPC1;
	}

    /* Only look at lower 16-bits.  Top 16-bits are function masks */
	if ( (*pGPIOPtr & 0xFFFF) & (1 << uBitPos) )
	{
		bIntState = 1;
	}
	else
	{
		bIntState = 0;
	}

#endif

	return bIntState;
}



/****************************************************************************
 *
 *  Name:			void SetGPIOIntMode( UINT8 uGPIOIntS, BOOL bState )
 *
 *  Description:	Enables or disables the particular GPIO interrupt
 *					   and enables the global interrupt if necessary
 *
 *  Inputs:			UINT8 uGPIOPin = which GPIO
 *					   BOOL bState = pin state
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void SetGPIOIntMode( UINT8 uGPIOPin, BOOL bPolarity )
{
#ifndef DEVICE_YELLOWSTONE
   UINT32 uBitPos;
   UINT32 ByteOffSet; 
   UINT32 uValue = 0;
   volatile UINT32* pGPIOIntModePtr;
   register int OldLevel;

   if (uGPIOPin<NUM_OF_GPIOS)
   {
      ByteOffSet = ((uGPIOPin / NUM_GPIOS_PER_REGISTER) * GRP_REG_OFFSET);

      // Calculate corresponding GPIO Output Enable register.  Use byte pointer addition
      // to match HW register definitions.
      pGPIOIntModePtr = (volatile UINT32 *) ((volatile UINT8 *)(p_GRP0_ISM0) + ByteOffSet);

      // Calculate bit position within the register.
      uBitPos  = (uGPIOPin % NUM_GPIOS_PER_REGISTER);

      /* Upper word is the IER bit mask */
      /* Lower work is the IER bit enable */
	   /* Set all mask bits and do read-modify-write due to register mask bug */
      OldLevel = intLock();

      HW_REG_READ(pGPIOIntModePtr, uValue);
      uValue |= 0xFFFF0000;

      if ( 0 != bPolarity  )
      {
         uValue |= (1 << uBitPos);
      }

      else
      {
         uValue &= ~(1 << uBitPos);
      }

      HW_REG_WRITE(pGPIOIntModePtr, uValue);
      intUnlock(OldLevel);
   }
#endif
}


/****************************************************************************
 *
 *  Name:			void SetGPIOIntEnable( UINT8 uGPIOIntE, BOOL bState )
 *
 *  Description:	Enables or disables the particular GPIO interrupt
 *					and enables the global interrupt if necessary
 *
 *  Inputs:			UINT8 uGPIOPin = which GPIO
 *					BOOL bState = pin state
 *
 *  Outputs:		none
 *
 ***************************************************************************/
void SetGPIOIntEnable( UINT8 uGPIOPin, BOOL bState )
{
#ifndef DEVICE_YELLOWSTONE
   UINT32 uBitPos;
   UINT32 ByteOffSet; 
   UINT32 uValue = 0;
   volatile UINT32* pGPIOIntMaskPtr;
   register int OldLevel;

	if (uGPIOPin < NUM_OF_GPIOS)
   {
      ByteOffSet = ((uGPIOPin / NUM_GPIOS_PER_REGISTER) * GRP_REG_OFFSET);

      // Calculate corresponding GPIO Output Enable register.  Use byte pointer addition
      // to match HW register definitions.
      pGPIOIntMaskPtr = (volatile UINT32 *) ((volatile UINT8 *)(p_GRP0_INT_MASK) + ByteOffSet);

      // Calculate bit position within the register.
      uBitPos  = (uGPIOPin % NUM_GPIOS_PER_REGISTER);

      /* Upper word is the IER bit mask */
      /* Lower work is the IER bit enable */

      /*************************/
      #ifdef DEVICE_YELLOWSTONE
      /*************************/

      uValue =  1 << (uBitPos + 16);

	   if ( IRQ_ON == bState  )
	   {
		   uValue |= (1 << uBitPos);
	   }

      else
	   {
		   uValue &= (~(1 << uBitPos));
	   }

      HW_REG_WRITE(pGPIOIntMaskPtr, uValue);

      /*************************/
      #else
      /*************************/

	   /* Set all mask bits and do read-modify-write due to register mask bug */
      OldLevel = intLock();
      HW_REG_READ(pGPIOIntMaskPtr, uValue);
      uValue |= 0xFFFF0000;

      if ( IRQ_ON == bState  )
      {
         uValue |= (1 << uBitPos);
      }

      else
      {
         uValue &= (~(1 << uBitPos));
      }

      /* Before enabling the interrupt, must clear the status to not get initial interrupt */
      ClearGPIOIntStatus ( uGPIOPin );

      HW_REG_WRITE(pGPIOIntMaskPtr, uValue);

      intUnlock(OldLevel);
      #endif

   }
#endif
}


/****************************************************************************
 *
 *  Name:			BOOL ReadGPIOIntEnable( UINT8 uGPIOPin )
 *
 *  Description:	Reads a GPIO pin interrupt status bit
 *
 *  Inputs:			UINT8 uGPIOPin = which GPIO
 *
 *  Outputs:		Returns BOOL interrupt status
 *
 ***************************************************************************/
BOOL ReadGPIOIntEnable( UINT8 uGPIOPin )
{
   UINT32 Value = 0;
   UINT32 uBitPos;
   UINT32 ByteOffSet; 
   volatile UINT32* pGPIOIntMaskPtr;

   if (uGPIOPin < NUM_OF_GPIOS)
   {
      ByteOffSet = ((uGPIOPin / NUM_GPIOS_PER_REGISTER) * GRP_REG_OFFSET);

      // Calculate corresponding GPIO Output Enable register.  Use byte pointer addition
      // to match HW register definitions.
      pGPIOIntMaskPtr = (volatile UINT32 *) ((volatile UINT8 *)(p_GRP0_INT_MASK) + ByteOffSet);

      // Calculate bit position within the register.
      uBitPos  = (uGPIOPin % NUM_GPIOS_PER_REGISTER);

      HW_REG_READ(pGPIOIntMaskPtr, Value);

      Value &= (1 << uBitPos);
   }

   return (Value);  
}

EXPORT_SYMBOL(SetGPIODir);
EXPORT_SYMBOL(SetGPIOUsage);
EXPORT_SYMBOL(WriteGPIOData);
EXPORT_SYMBOL(ReadGPIOData);
EXPORT_SYMBOL(ReadGPIOIntStatus);
EXPORT_SYMBOL(ClearGPIOIntStatus);
EXPORT_SYMBOL(SetGPIOIntPolarity);
EXPORT_SYMBOL(ReadGPIOIntPolarity);
EXPORT_SYMBOL(SetGPIOIntMode);
EXPORT_SYMBOL(SetGPIOIntEnable);
EXPORT_SYMBOL(SetGPIOInputEnable);

