/******************************************************************/
/*                                                                */
/* Module:       jblaster.c                                       */
/*                                                                */
/* Descriptions: Main source file that manages the configuration  */
/*               processes.                                       */
/*                                                                */
/* Revisions:    0.1 02/12/07                                     */
/*                                                                */
/******************************************************************/
/* $Id* */

#include <asm/MC68VZ328.h>
#include "jb_io.h"
#include "jb_jtag.h"
#include "jb_const.h"
#include "jb_cdf.h"

#define DEBUG 1

/* JTAG instruction lengths of all devices */
int  ji_info[MAX_DEVICE_ALLOW] = {0x0};

/* JBlaster Controller Functions */
int  VerifyChain      (void);
void Configure        (int, int, int);
void ProcessFileInput (int);
int  CheckStatus      (int);
void Startup          (int);

void jb_main(void)
{
	int i=0;
	int file_id=0;
	int config_count=0;

	/**********Initialization**********/

	/* Introduction */
	low_printf("JTAGBlaster (JBlaster) Version %s",VERSION);
	low_printf("\r\n Altera Corporation ");
   	low_printf("\r\n Ported to linux by Marcos Lois Bermudez (marcos.lois@securez.org)\r\n");

   /* Setup JTAG interface */
	initialize_jtag_hardware();

	if(!device_count)
		return;

	for(i=0;i<device_count;i++)
	{
   	ji_info[i] = device_list[i].inst_len;
		if(!ji_info[i])
		{
			low_printf("Error: JTAG instruction length of device #%d NOT specified!\r\n",i);
			return;
		}
	}

	/**********Device Chain Verification**********/

	low_printf("Info: Verifying device chain...\r\n" );

	/* Verify the JTAG-compatible devices chain */
	if(VerifyChain())
		return;

#ifdef DEBUG
	for(i=0;i<device_count;i++)
	{
		low_printf("Debug: (Id)0x%X (Action)%c (Part)%s (File Len)%d Bytes (Inst Len)%d\r\n",
      	device_list[i].idcode,
			device_list[i].action,device_list[i].partname,
			device_list[i].buffer_len,device_list[i].inst_len);
	}
#endif

	/**********Configuration**********/
	file_id=0;

	for(i=1;i<device_count+1;i++)
	{
		config_count=0;

		if(device_list[i-1].action != 'P')
			continue;

		while(config_count<MAX_CONFIG_COUNT)
		{
			if(!config_count)
				low_printf("Info: Configuration setup device #%d\r\n",i);
			else
				low_printf("Info: Configuration setup device #%d (Retry)\r\n",i);

			config_count++;

			/* Start configuration */
			Configure(i-1,i,(device_list[i-1].idcode? JI_PROGRAM:JI_BYPASS));

			if(CheckStatus(i))
			{
				low_printf("Warning: Configuration of device #%d NOT successful!\r\n",i );
			}
			else
			{
				Startup(i);
				config_count=MAX_CONFIG_COUNT;
				low_printf("Info: Configuration of device #%d successful...\r\n",i );
			}
		}
	}

   // Release JTAG interface
	close_jtag_hardware();

	return;
}



/******************************************************************/
/* Name:         VerifyChain                                      */
/*                                                                */
/* Parameters:   None.                                            */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                          */
/* Descriptions: Putting all devices in BYPASS mode, a 8-bit      */
/*               vector is driven to TDI, the number of '0'       */
/*               detected indicates the number of devices in      */
/*               chain. The 8-bit vector must follows the zeroes. */
/*                                                                */
/******************************************************************/
int VerifyChain()
{
	unsigned int data=0,temp=0,test_vect=0x55;
	int i,num=0,error=0;

	Js_Reset();


	/* Load BYPASS instruction and test JTAG chain with a few vectors */
	if(Ji_Bypass(device_count,ji_info))
		return (1);
	Js_Shiftdr();

	/* Drive a 8-bit vector of "10101010" (right to left) to test */
	data = ReadTDO(8+device_count,test_vect,0);
	/* The number of leading '0' detected must equal to the number of devices specified */
	temp = data;

	for(i=0;i<device_count;i++)
	{
		temp = temp&1;
		if(temp)
			break;
		else
			num++;
		temp = data>>(i+1);
	}

	if(temp==test_vect)
		low_printf("Info: Detected %d device(s) in chain...\r\n", num);
	else
	{
		low_printf("Error: JTAG chain broken or #device in chain unmatch!\r\nError: Bits unloaded: 0x%X\r\n",temp);
		return (1);
	}

	Js_Updatedr();

	/* Read device IDCODE */
	Ji_Idcode(device_count,ji_info);
	Js_Shiftdr();

	for(i=device_count-1;i>=0;i--)
	{
		data = ReadTDO(CDF_IDCODE_LEN,0,0);

		if(device_list[i].idcode)
		{
			/* The partname specified in CDF must match with its ID Code */
			if((unsigned)device_list[i].idcode != data)
			{
				low_printf("Error: Expected 0x%X but detected 0x%X!\r\n",device_list[i].idcode,data);
				error=1;
			}
			else
				low_printf("Info: Dev%d: Altera: 0x%X\r\n",i+1,data);
		}
		else
		{
			low_printf("Info: Dev%d: Non-Altera: 0x%X\r\n",i+1,data);
		}
	}

	Js_Updatedr();
	Js_Runidle();

	return error;
}

/******************************************************************/
/* Name:         Configure                                        */
/*                                                                */
/* Parameters:   file_id,dev_seq,action                           */
/*               -cnum is num of chain (0 first chain)            */
/*               -dev_seq is the device sequence in chains.       */
/*               -action is the action to take:BYPASS or PROGRAM  */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                             */
/* Descriptions: Issue PROGRAM instruction to the device to be    */
/*               configured and BYPASS for the rest of the devices*/
/*               Call function that processes the source file.    */
/*                                                                */
/******************************************************************/
void Configure(int cnum,int dev_seq,int action)
{
	int i;

	/* Load PROGRAM instruction */
	SetupChain(device_count,dev_seq,ji_info,action);

	if(action==JI_PROGRAM)
	{
		/* Drive TDI HIGH while moving JSM to SHIFTDR */
		DriveSignal(SIG_TDI,1,0,1);
		Js_Shiftdr();
		/* Issue MAX_JTAG_INIT_CLOCK clocks in SHIFTDR state */
		for(i=0;i<MAX_JTAG_INIT_CLOCK[device_family];i++)
		{
			DriveSignal(SIG_TDI,1,1,0);
		}
		/* Start dumping configuration bits into TDI and clock with TCK */
		ProcessFileInput(cnum);

		/* Move JSM to RUNIDLE */
		Js_Updatedr();
		Js_Runidle();
	}
}

/******************************************************************/
/* Name:         CheckStatus                                      */
/*                                                                */
/* Parameters:   dev_seq                                          */
/*               -dev_seq is the device sequence in chains.       */
/*                                                                */
/* Return Value: '0' if CONF_DONE is HIGH;'1' if it is LOW.       */
/*               		                                          */
/* Descriptions: Issue CHECK_STATUS instruction to the device to  */
/*               be configured and BYPASS for the rest of the     */
/*               devices.                                         */
/*                                                                */
/*               <conf_done_bit> =                                */
/*                  ((<Maximum JTAG sequence> -                   */
/*                    <JTAG sequence for CONF_DONE pin>)*3) + 1   */
/*                                                                */
/*               The formula calculates the number of bits        */
/*               to be shifted out from the device, excluding the */
/*               1-bit register for each device in BYPASS mode.   */
/*                                                                */
/******************************************************************/
int CheckStatus(int dev_seq)
{
	int bit,data=0,error=0;
	int jseq_max=0,jseq_conf_done=0,conf_done_bit=0;

	low_printf("Info: Checking Status\r\n" );

	/* Load CHECK_STATUS instruction */
	SetupChain(device_count,dev_seq,ji_info,JI_CHECK_STATUS);

	Js_Shiftdr();

	/* Maximum JTAG sequence of the device in chain */
	jseq_max= device_list[dev_seq-1].jseq_max;

	jseq_conf_done= device_list[dev_seq-1].jseq_conf_done;

	conf_done_bit = ((jseq_max-jseq_conf_done)*3)+1;

	/* Compensate for 1 bit unloaded from every Bypass register */
	conf_done_bit+= (device_count-dev_seq);

	for(bit=0;bit<conf_done_bit;bit++)
	{
		DriveSignal(SIG_TDI,0,1,1);
	}

	data = ReadTDO(1,0,0);

	if(!data)
		error++;

	/* Move JSM to RUNIDLE */
	Js_Updatedr();
	Js_Runidle();

	return (error);
}

/******************************************************************/
/* Name:         Startup                                          */
/*                                                                */
/* Parameters:   dev_seq                                          */
/*               -the device sequence in the chain.               */
/*                                                                */
/* Return Value: None.                                            */
/*               		                                          */
/* Descriptions: Issue STARTUP instruction to the device to       */
/*               be configured and BYPASS for the rest of the     */
/*               devices.                                         */
/*                                                                */
/******************************************************************/
void Startup(int dev_seq)
{
	int i;

	/* Load STARTUP instruction to move the device to USER mode */
	SetupChain(device_count,dev_seq,ji_info,JI_STARTUP);

	Js_Runidle();

	for(i=0;i<INIT_COUNT;i++)
	{
		DriveSignal(SIG_TCK,0,0,0);
		DriveSignal(SIG_TCK,1,0,0);
	}

	/* Reset JSM after the device is in USER mode */
	Js_Reset();
}

/******************************************************************/
/* Name:         ProcessFileInput                                 */
/*                                                                */
/* Parameters:   cnum                                             */
/*               -num of chain to program (0 first chain)         */
/*                                                                */
/* Return Value: None.                                            */
/*                                                                */
/* Descriptions: Get programming file size, parse through every   */
/*               single byte and dump to parallel port.           */
/*                                                                */
/******************************************************************/
void ProcessFileInput(int cnum)
{
	unsigned char *one_byte, bit = 0;
	int i,j,k;
  	int nbytes;

	/* Start configuration */
	low_printf("Info: Start configuration process.\r\n  Please wait...");

   /* Check for compressed data */
   if(device_list[cnum].compress) {
   	/* Process compress data */
		one_byte = device_list[cnum].buffer;
		for(i=0;i<device_list[cnum].buffer_len;)
		{
			nbytes = (*(one_byte) & 0x7f) + 1;
			if (*(one_byte) & 0x80) {
				one_byte++; i++; // Get next byte
         	for(k=0;k<nbytes;k++) {
					// Write nbytes
               /* Progaram a byte,from LSb to MSb */
					for (j=0;j<8;j++ )
					{
						bit = *(one_byte) >> j;
						bit = bit & 0x1;

						/* Dump to TDI and drive a positive edge pulse at the same time */
						DriveSignal(SIG_TDI,bit,1,0);
					}
	         }
			} else {
   	   	for(k=0;k<nbytes;k++) {
      	   	one_byte++; i++; // Get next byte
					// Write byte
               /* Progaram a byte,from LSb to MSb */
					for (j=0;j<8;j++ )
					{
						bit = *(one_byte) >> j;
						bit = bit & 0x1;

						/* Dump to TDI and drive a positive edge pulse at the same time */
						DriveSignal(SIG_TDI,bit,1,0);
					}
	         }
			}
      	one_byte++; i++;
      }
   } else {
	/* Loop through every single byte */
   	one_byte = device_list[cnum].buffer;
	for(i=device_list[cnum].buffer_len;i;i--)
	{
      		bit = 0;
		/* Progaram a byte,from LSb to MSb */
		for (j=0;j<8;j++ )
		{
			bit = (*(one_byte) >> j) & 0x01;
			PDDATA = bit ? (PDDATA | 0x01) : (PDDATA & 0xfe);
			PDDATA |= 0x02;
			PDDATA &= 0xfd;
   			/* Dump to TDI and drive a positive edge pulse at the same time */
			//DriveSignal(SIG_TDI,bit,1,0);
		}
		one_byte++;
	}
	low_printf(" done\r\n");
   }
}
