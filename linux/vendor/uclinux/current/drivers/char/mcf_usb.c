/*********************************************************************
 *
 * Copyright:
 *	MOTOROLA, INC. All Rights Reserved.  
 *  You are hereby granted a copyright license to use, modify, and
 *  distribute the SOFTWARE so long as this entire notice is
 *  retained without alteration in any modified and/or redistributed
 *  versions, and that such modified versions are clearly identified
 *  as such. No licenses are granted by implication, estoppel or
 *  otherwise under any patents or trademarks of Motorola, Inc. This 
 *  software is provided on an "AS IS" basis and without warranty.
 *
 *  To the maximum extent permitted by applicable law, MOTOROLA 
 *  DISCLAIMS ALL WARRANTIES WHETHER EXPRESS OR IMPLIED, INCLUDING 
 *  IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR
 *  PURPOSE AND ANY WARRANTY AGAINST INFRINGEMENT WITH REGARD TO THE 
 *  SOFTWARE (INCLUDING ANY MODIFIED VERSIONS THEREOF) AND ANY 
 *  ACCOMPANYING WRITTEN MATERIALS.
 * 
 *  To the maximum extent permitted by applicable law, IN NO EVENT
 *  SHALL MOTOROLA BE LIABLE FOR ANY DAMAGES WHATSOEVER (INCLUDING 
 *  WITHOUT LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS 
 *  INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR OTHER PECUNIARY
 *  LOSS) ARISING OF THE USE OR INABILITY TO USE THE SOFTWARE.   
 * 
 *  Motorola assumes no responsibility for the maintenance and support
 *  of this software
 ********************************************************************/

/* Purpose:	Device Driver for MCF5272 USB module
 */

#include <linux/config.h>
#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/malloc.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <asm/coldfire.h>
#include <linux/version.h>

#ifdef MODULE
#include <linux/module.h>   /* We need it if driver acts like a module */
#else
#define MOD_INC_USE_COUNT
#define MOD_DEC_USE_COUNT
#endif

#include "mcf_usb_defs.h"
#include "mcf5272.h"
#include "mcf_usb.h"
#include "mcf_descriptors.h"

//#define DEBUG

/* Deal with CONFIG_MODVERSIONS */
#if CONFIG_MODVERSIONS==1
#define MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/wrapper.h>

/* In 2.2.3 linux/version.h includes
 * a macro for this, but 2.0.xx doesn't - so I add
 * it here if necessary. */
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) ((a)*65536+(b)*256+(c))
#endif

#define mcf5272_get_immp()     ((MCF5272_IMM *)(MCF_MBAR))

/***************************************************************/
/* We need it for compatability with different kernel versions */
/***************************************************************/
/* Conditional compilation. LINUX_VERSION_CODE is
 * the code (as per KERNEL_VERSION) of this version. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
/* Copy data from user address space to kernel. */
#define GET_RDEV(filp)				((filp)->f_dentry->d_inode->i_rdev)
#define usb_kill_fasync(async_queue, sig)	(kill_fasync(&async_queue, sig, POLL_IN));
#else
#define GET_RDEV(filp)				((filp)->f_inode->i_rdev)
#define usb_kill_fasync(async_queue, sig)	(kill_fasync(async_queue, sig));
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,1)
#define DECLARE_WAIT_QUEUE_HEAD(head) struct wait_queue *head = NULL
typedef struct wait_queue *wait_queue_head_t;
#endif
/***************************************************************/

/* Wait queue for fetch_command function */
DECLARE_WAIT_QUEUE_HEAD(fetch_command_queue);
/* Wait queue for ep_wait function */
DECLARE_WAIT_QUEUE_HEAD(ep_wait_queue);
/* Structure for asynchronous notification */
struct fasync_struct *usb_async_queue;
/********************************************************************/

/* Global Endpoint Status Structures */
USB_EP_STATE ep[NUM_ENDPOINTS];
USB_ISO_EP_STATE iso_ep[NUM_ENDPOINTS];

/* Pointer to device descriptor */
uint8 * usb_device_descriptor;

/* Events mask */
uint32 usb_event=0;
/* Contains information about current configuration */
CONFIG_STATUS current_config;

/* Global pointer used to access the command */
DEVICE_COMMAND * NewC = NULL;

/* Indicates which endpoints are already opened */
uint8 active_ep_mask = 0;

/* Global pointer to IMM */
MCF5272_IMM *imm = mcf5272_get_immp();

/*************************USB_INIT******************************/
void 
usb_init(DESC_INFO * descriptor_info)
{
	uint32 i, DescSize;
	uint32 *pConfigRam;
	uint32 *pDevDesc;

	/* Get device descriptor's address from user space */
	usb_device_descriptor = descriptor_info -> pDescriptor;

	/* Initialize Descriptor pointers and variables */
	pConfigRam = (uint32 *)((uint32)imm + MCF5272_USB_CFG_RAM);
	pDevDesc = (uint32*)usb_device_descriptor;
	DescSize = descriptor_info -> DescSize + 3;

	/* Initialize Endpoint status structures */
	ep[0].ttype = CONTROL;
	ep[0].packet_size = ((USB_DEVICE_DESC *)pDevDesc)->bMaxPacketSize0;
	ep[0].fifo_length = (uint16)(ep[0].packet_size * FIFO_DEPTH);
	ep[0].buffer.start = 0;
	ep[0].buffer.length = 0;
	ep[0].buffer.position = 0;
	ep[0].state = USB_CONFIGURED;
	ep[0].sendZLP = FALSE;

	/* Set the EP0 IN-BUSY bit -- This is only required on
	   pre-2K75N masks. This bit is set by HW beginning with
	   the 2K75N silicon. */
	MCF5272_WR_USB_EP0CTL(imm, MCF5272_RD_USB_EP0CTL(imm) 
		| MCF5272_USB_EP0CTL_IN_BUSY);
	
	for (i = 1; i < NUM_ENDPOINTS; i++)
		ep[i].ttype = DISABLED;

	/* Invalidate Configuration RAM and disable Endpoint 0 */
	MCF5272_WR_USB_EP0CTL(imm, 0);

	/* Load the Configuration RAM with the descriptors */
	for (i = 0; i < (DescSize/4); i++)
		pConfigRam[i] = pDevDesc[i];

	/* Initialize FIFOs */
	usb_fifo_init();

	/* Initialize the Interrupts */
	usb_isr_init();

	/* Enable USB controller, Config RAM, EXT AFE */
#ifdef DEBUG
	MCF5272_WR_USB_EP0CTL(imm, 0
		| MCF5272_USB_EP0CTL_DEBUG
		| MCF5272_USB_EP0CTL_AFE_EN	/*	*/
		| MCF5272_USB_EP0CTL_USB_EN
		| MCF5272_USB_EP0CTL_CFG_RAM_VAL);
#else
	MCF5272_WR_USB_EP0CTL(imm, 0
		| MCF5272_USB_EP0CTL_AFE_EN	/*	*/
		| MCF5272_USB_EP0CTL_USB_EN
		| MCF5272_USB_EP0CTL_CFG_RAM_VAL);
#endif
        /* Enable USB signals */
        MCF5272_WR_GPIO_PACNT (imm,0x00001555);
        /* Configure all pins as inputs */
        MCF5272_WR_GPIO_PADDR (imm,0);
	
}
/***********************USB_INIT  end********************************/
void 
usb_isr_init(void)
{
	/* Clear any pending interrupts in all Endpoints */
	MCF5272_WR_USB_EP0ISR(imm, 0x0001FFFF);
	MCF5272_WR_USB_EP1ISR(imm, 0x001F);
	MCF5272_WR_USB_EP2ISR(imm, 0x001F);
	MCF5272_WR_USB_EP3ISR(imm, 0x001F);
	MCF5272_WR_USB_EP4ISR(imm, 0x001F);
	MCF5272_WR_USB_EP5ISR(imm, 0x001F);
	MCF5272_WR_USB_EP6ISR(imm, 0x001F);
	MCF5272_WR_USB_EP7ISR(imm, 0x001F);

	/* Enable all interrupts on all Endpoints */
	/* These will be altered on configuration/interface changes */
	/* Enable desired interrupts on EP0 */
	MCF5272_WR_USB_EP0IMR(imm, 0
		| MCF5272_USB_EP0IMR_DEV_CFG_EN
		| MCF5272_USB_EP0IMR_VEND_REQ_EN
		| MCF5272_USB_EP0IMR_WAKE_CHG_EN
		| MCF5272_USB_EP0IMR_RESUME_EN
		| MCF5272_USB_EP0IMR_SUSPEND_EN
		| MCF5272_USB_EP0IMR_RESET_EN
		| MCF5272_USB_EP0IMR_OUT_EOT_EN
		| MCF5272_USB_EP0IMR_OUT_EOP_EN
		| MCF5272_USB_EP0IMR_IN_EOT_EN
		| MCF5272_USB_EP0IMR_IN_EOP_EN
		| MCF5272_USB_EP0IMR_UNHALT_EN
		| MCF5272_USB_EP0IMR_HALT_EN
		| MCF5272_USB_EP0IMR_SOF_EN);

	MCF5272_WR_USB_EP1IMR(imm, 0x001F);
	MCF5272_WR_USB_EP2IMR(imm, 0x001F);
	MCF5272_WR_USB_EP3IMR(imm, 0x001F);
	MCF5272_WR_USB_EP4IMR(imm, 0x001F);
	MCF5272_WR_USB_EP5IMR(imm, 0x001F);
	MCF5272_WR_USB_EP6IMR(imm, 0x001F);
	MCF5272_WR_USB_EP7IMR(imm, 0x001F);

	/* Initialize Interrupt Control Registers */
	MCF5272_WR_SIM_ICR2(imm, 0
		| (0x00008888)
		| (USB_EP0_LEVEL << 12) 
		| (USB_EP1_LEVEL << 8) 
		| (USB_EP2_LEVEL << 4) 
		| (USB_EP3_LEVEL << 0));
	MCF5272_WR_SIM_ICR3(imm, 0
		| (0x88880000)
		| (USB_EP4_LEVEL << 28) 
		| (USB_EP5_LEVEL << 24) 
		| (USB_EP6_LEVEL << 20) 
		| (USB_EP7_LEVEL << 16));
}

/********************************************************************/
void 
usb_endpoint0_isr(int irq, void *dev_id, struct pt_regs *regs)
{
uint32 event;

	event = MCF5272_RD_USB_EP0ISR(imm) & MCF5272_RD_USB_EP0IMR(imm);

	if (event & MCF5272_USB_EP0ISR_SOF)
	{
		/* Clear this interrupt bit */
		MCF5272_WR_USB_EP0ISR(imm, MCF5272_USB_EP0ISR_SOF);

		usb_isochronous_transfer_service();
		
	}

	if (event & MCF5272_USB_EP0ISR_DEV_CFG)
	{
		usb_devcfg_service();
		/* Clear this interrupt bit */
		MCF5272_WR_USB_EP0ISR(imm, MCF5272_USB_EP0ISR_DEV_CFG);
	}
	if (event & MCF5272_USB_EP0ISR_VEND_REQ)
	{
		/* Clear this interrupt bit */
		MCF5272_WR_USB_EP0ISR(imm, MCF5272_USB_EP0ISR_VEND_REQ);

		usb_vendreq_service(
			(uint8)(MCF5272_RD_USB_DRR1(imm) & 0xFF),
			(uint8)(MCF5272_RD_USB_DRR1(imm) >> 8),
			(uint16)(MCF5272_RD_USB_DRR1(imm) >> 16),
			(uint16)(MCF5272_RD_USB_DRR2(imm) & 0xFFFF),
			(uint16)(MCF5272_RD_USB_DRR2(imm) >> 16));

	}
		
	if (event & MCF5272_USB_EP0ISR_FRM_MAT)
	{
		/* Clear this interrupt bit */
		MCF5272_WR_USB_EP0ISR(imm, MCF5272_USB_EP0ISR_FRM_MAT);
	}
	if (event & MCF5272_USB_EP0ISR_ASOF)
	{
		/* Clear this interrupt bit */
		MCF5272_WR_USB_EP0ISR(imm, MCF5272_USB_EP0ISR_ASOF);
	}

	if (event & MCF5272_USB_EP0ISR_WAKE_CHG)
	{
        if (MCF5272_RD_USB_EP0SR(imm) & MCF5272_USB_EP0SR_WAKE_ST)
        {
            /* Enable Wake-on-Ring */
            MCF5272_WR_USB_EP0CTL(imm,
                  MCF5272_RD_USB_EP0CTL(imm)
                | MCF5272_USB_EP0CTL_WOR_EN);
            /* Wake-on-Ring function is invoked when /INT1 pin is 0 */
            MCF5272_WR_USB_EP0CTL(imm, MCF5272_RD_USB_EP0CTL(imm)
                & ~MCF5272_USB_EP0CTL_WOR_LVL);
        }
        else
        {
			/* Disable Wake-on-Ring */
			MCF5272_WR_USB_EP0CTL(imm,
				   MCF5272_RD_USB_EP0CTL(imm)
				& ~MCF5272_USB_EP0CTL_WOR_EN);
        }
        /* Clear this interrupt bit */
		MCF5272_WR_USB_EP0ISR(imm, MCF5272_USB_EP0ISR_WAKE_CHG);
	}
	if (event & MCF5272_USB_EP0ISR_RESUME)
	{
	        usb_bus_state_chg_service(RESUME);
		/* Clear this interrupt bit */
		MCF5272_WR_USB_EP0ISR(imm, MCF5272_USB_EP0ISR_RESUME);
	}
	if (event & MCF5272_USB_EP0ISR_SUSPEND)
	{
	        if (MCF5272_RD_USB_EP0SR(imm) & MCF5272_USB_EP0SR_WAKE_ST)
	            usb_bus_state_chg_service(SUSPEND | ENABLE_WAKEUP);
	        else
	            usb_bus_state_chg_service(SUSPEND);
		/* Clear this interrupt bit */
		MCF5272_WR_USB_EP0ISR(imm, MCF5272_USB_EP0ISR_SUSPEND);
	}
	if (event & MCF5272_USB_EP0ISR_RESET)
	{
	        usb_bus_state_chg_service(RESET);
		/* Clear this interrupt bit */
		MCF5272_WR_USB_EP0ISR(imm, MCF5272_USB_EP0ISR_RESET);
	}
	if (event & ( MCF5272_USB_EP0ISR_OUT_EOT
				| MCF5272_USB_EP0ISR_OUT_EOP
				| MCF5272_USB_EP0ISR_OUT_LVL))
	{
		usb_out_service(0, event);
	}
	if (event & ( MCF5272_USB_EP0ISR_IN_EOT
				| MCF5272_USB_EP0ISR_IN_EOP
				| MCF5272_USB_EP0ISR_IN_LVL))
	{
		usb_in_service(0, event);
	}
	if (event & MCF5272_USB_EP0ISR_UNHALT)
	{
		/* Clear this interrupt bit */
		MCF5272_WR_USB_EP0ISR(imm, MCF5272_USB_EP0ISR_UNHALT);
	}
	if (event & MCF5272_USB_EP0ISR_HALT)
	{
		/* Clear this interrupt bit */
		MCF5272_WR_USB_EP0ISR(imm, MCF5272_USB_EP0ISR_HALT);
	}

}
/********************************************************************/
void 
usb_endpoint_isr(int irq, void *dev_id, struct pt_regs *regs)
{
	int event, epnum;
	epnum = irq - 77;  /* Obtain endpoint number from interrupt number */

	event = MCF5272_RD_USB_EPISR(imm, epnum) 
		  & MCF5272_RD_USB_EPIMR(imm, epnum);

	if (event & ( MCF5272_USB_EPNISR_EOT	
				| MCF5272_USB_EPNISR_EOP
				| MCF5272_USB_EPNISR_FIFO_LVL))
	{
		/* IN Endpoint */
		if (MCF5272_RD_USB_EPISR(imm, epnum) & MCF5272_USB_EPNISR_DIR)
			usb_in_service(epnum, event);
		/* OUT Endpoint */
		else
			usb_out_service(epnum,event);
	}
	if (event & MCF5272_USB_EPNISR_HALT)
	{
		usb_ep_halt(epnum);
		MCF5272_WR_USB_EPISR(imm, epnum, MCF5272_USB_EPNISR_HALT);
	}
	if (event & MCF5272_USB_EPNISR_UNHALT)
	{
		usb_ep_unhalt(epnum);
		MCF5272_WR_USB_EPISR(imm, epnum, MCF5272_USB_EPNISR_UNHALT);
	}

}
/********************************************************************/
inline uint32
usb_get_request(uint32 epnum)
{
	/* Check for requests in requests queue */
	if (iso_ep[epnum].number_of_requests > 0)
	{
	    /* Extract next request from the queue */
	    iso_ep[epnum].number_of_requests--;

	    /* Set up the iso_ep and buffer structures */
	    ep[epnum].buffer.length = iso_ep[epnum].requests_queue[iso_ep[epnum].first_io_request].length;
	    iso_ep[epnum].packet_length = (uint32*) iso_ep[epnum].requests_queue[iso_ep[epnum].first_io_request].start;
	    ep[epnum].buffer.start = (uint8*)iso_ep[epnum].packet_length + 4 * ep[epnum].buffer.length;
				    
	    ep[epnum].buffer.position = 0;
	    iso_ep[epnum].frame_counter = 0;

	    /* Modify first_io_request index */
	    if (++iso_ep[epnum].first_io_request >= MAX_IO_REQUESTS)
		iso_ep[epnum].first_io_request = 0;
	    
	    return TRUE;

	} else
	    return FALSE;
}
/********************************************************************/
void
usb_in_service(uint32 epnum, uint32 event)
{
int32 i;
int32 free_space = 0;
uint32 imr_mask;
uint32 remainder, length;

	/* Save mask register and disable RESET, DEV_CFG, and SOF interrupts on endpoint 0 */
	imr_mask = MCF5272_RD_USB_EP0IMR(imm);
	MCF5272_WR_USB_EP0IMR(imm, imr_mask & ~(MCF5272_USB_EP0ISR_RESET
						| MCF5272_USB_EP0ISR_DEV_CFG
						| MCF5272_USB_EP0ISR_SOF));

	if (event & MCF5272_USB_EPNISR_EOP)
	{
		/* Clear the EOP interrupt bits */
		MCF5272_WR_USB_EPISR(imm, epnum, MCF5272_USB_EPNISR_EOP);
		
		if ((ep[epnum].ttype == ISOCHRONOUS) && (MCF5272_RD_USB_EPDPR(imm, epnum) == 0))
		{
			if (iso_ep[epnum].transfer_monitoring == TRUE)
				/* EOP interrupt occured, so clear sent_packet_watch variable */
				iso_ep[epnum].sent_packet_watch = 0;

			iso_ep[epnum].frame_counter ++;
			/* Increase packet_position by the size of next packet */
			iso_ep[epnum].packet_position += iso_ep[epnum].packet_length[iso_ep[epnum].frame_counter];

			/* Check if sent packet was the last in current buffer */
			if (iso_ep[epnum].frame_counter >= ep[epnum].buffer.length)
			{
				/* Check for requests in requests queue */
				if (usb_get_request(epnum))
				    /* Initialize packet_position with the size of first packet in buffer */
				    iso_ep[epnum].packet_position = iso_ep[epnum].packet_length[0];
				else
				{
				    /* There are no requests in a queue, so we can free buffer structure */
				    ep[epnum].buffer.start = 0;
				    ep[epnum].buffer.length = 0;
				    ep[epnum].buffer.position = 0;
			
				    iso_ep[epnum].packet_position = 0;
				}

				/* Buffer is sent - wake up usb_ep_wait function */
				wake_up_interruptible(&ep_wait_queue);
			}
		}
	}

	if (event & (MCF5272_USB_EPNISR_EOP | MCF5272_USB_EPNISR_FIFO_LVL))
	{
		/* Clear this FIFO_LVL interrupt bit */
		MCF5272_WR_USB_EPISR(imm, epnum, MCF5272_USB_EPNISR_FIFO_LVL);
	
		i = ep[epnum].buffer.position;

		if (ep[epnum].ttype == ISOCHRONOUS)
			length = iso_ep[epnum].packet_position;
		else
			length = ep[epnum].buffer.length;

		if (i < length)
		{
			/* USBEPDP0 only monitors the OUT FIFO */
			if (epnum == 0)
				free_space = ep[0].packet_size;
			else
				free_space = (uint16)(ep[epnum].fifo_length - MCF5272_RD_USB_EPDPR(imm, epnum));
			/* If the amount of the rest of data to be sent less than free_space, modify free_space */
			if (free_space > (length - ep[epnum].buffer.position))
				free_space = (length - ep[epnum].buffer.position);
				
			free_space += i;
			free_space -= 3;
			
			/* Write to FIFO by 4 bytes */
			while (i < free_space)
			{
				/* We use here advantages of MMU-less CPU.
				   Data is copied directly to user's memory space */
				MCF5272_WR_USB_EPDR(imm, epnum, 32, *(uint32 *)(&ep[epnum].buffer.start[i]));
				i += 4;
			}
			
			free_space += 3;
			
			/* Write to FIFO by 1 byte */
			while (i < free_space)
			{
				MCF5272_WR_USB_EPDR(imm,epnum,8,ep[epnum].buffer.start[i]);
				i++;
			}
		
			ep[epnum].buffer.position = i;
		}
						
		if (ep[epnum].ttype != ISOCHRONOUS)
		{
			/* Is it all data to send ? */
			if ((ep[epnum].buffer.start) && (i == ep[epnum].buffer.length))
			{
				remainder = i % ep[epnum].packet_size;
			
				/* Send short packet (if buffer length isn't multiple to packet size) or 
					zero-length packet, if sendZLP field is set */
				if ((remainder != 0) || ((remainder == 0) && ep[epnum].sendZLP))
				{
					/* All done -> Clear the IN-DONE bit */
					MCF5272_WR_USB_EPCTL(imm, epnum, 
							MCF5272_RD_USB_EPCTL(imm, epnum)
							& ~MCF5272_USB_EPNCTL_IN_DONE);
				}
				else
					if (MCF5272_RD_USB_EPDPR(imm, epnum) == 0)
					{
						/* Is it a completion of data IN stage command? */
						if ((epnum == 0) && (NewC))
						{
							usb_vendreq_done(SUCCESS);
								
							kfree(NewC);
							NewC = NULL;
							
						}

						ep[epnum].bytes_transferred = i;

						ep[epnum].buffer.start = 0;
						ep[epnum].buffer.length = 0;
						ep[epnum].buffer.position = 0;

						/* Wake up usb_ep_wait function if it sleeps */
						wake_up_interruptible(&ep_wait_queue);
					}

					ep[epnum].sendZLP = FALSE;
			}
		}
	}
	
	if (event & MCF5272_USB_EPNISR_EOT)
	{
		MCF5272_WR_USB_EPISR(imm, epnum, 0
			| MCF5272_USB_EPNISR_EOT);
			
		/* Is it a completion of data IN stage command? */
		if ((epnum == 0) && (NewC))
		{
			usb_vendreq_done(SUCCESS);
			
			kfree(NewC);
			NewC = NULL;
		}
		ep[epnum].bytes_transferred = ep[epnum].buffer.length;

		ep[epnum].buffer.start = 0;
		ep[epnum].buffer.length = 0;
		ep[epnum].buffer.position = 0;

		/* Set the IN-DONE bit
		   This is only required on pre-2K75N masks. This bit is set by 
		   HW beginning with the 2K75N silicon. */
		MCF5272_WR_USB_EPCTL(imm, epnum, MCF5272_RD_USB_EPCTL(imm, epnum) 
			| MCF5272_USB_EPNCTL_IN_DONE);

		/* Wake up usb_ep_wait function if it sleeps */
		wake_up_interruptible(&ep_wait_queue);
	}
	/* Restore IMR */
	MCF5272_WR_USB_EP0IMR(imm, imr_mask);
}
/********************************************************************/
void
usb_out_service(uint32 epnum, uint32 event)
{
int32 i, fifo_data;
int32 bytes_read=0;
uint32 imr_mask;

	/* Save interrupt mask register */
	imr_mask = MCF5272_RD_USB_EP0IMR(imm);
	/* Disable RESET, DEV_CFG, and SOF interrupts */
	MCF5272_WR_USB_EP0IMR(imm, imr_mask & ~(MCF5272_USB_EP0ISR_RESET
					| MCF5272_USB_EP0ISR_DEV_CFG
					| MCF5272_USB_EP0ISR_SOF));
					
	if (event & (MCF5272_USB_EP0ISR_OUT_EOP
				| MCF5272_USB_EPNISR_EOP
				| MCF5272_USB_EPNISR_FIFO_LVL))
	{
		/* Clear the LVL interrupt bits */
		MCF5272_WR_USB_EPISR(imm, epnum, 0 
			| MCF5272_USB_EPNISR_FIFO_LVL);
		/* Clear the EOP interrupt bits */
		MCF5272_WR_USB_EPISR(imm, epnum, 0 
			| MCF5272_USB_EP0ISR_OUT_EOP
			| MCF5272_USB_EPNISR_EOP);

		/* Read the Data Present register */
		fifo_data = MCF5272_RD_USB_EPDPR(imm, epnum);

		if (ep[epnum].buffer.start)
		{
			if (ep[epnum].ttype == ISOCHRONOUS)
			{
			    /* Check if there are more data in FIFO than we need */
			    if (iso_ep[epnum].packet_position + fifo_data >
				iso_ep[epnum].packet_length[iso_ep[epnum].frame_counter])
				fifo_data = iso_ep[epnum].packet_length[iso_ep[epnum].frame_counter]
					    - iso_ep[epnum].packet_position;
			}
			else
			    /* Check if there are more data in FIFO than we need */
			    if (ep[epnum].buffer.position + fifo_data > ep[epnum].buffer.length)
				fifo_data = ep[epnum].buffer.length - ep[epnum].buffer.position;
		}

		/* In isochronous transfer handle the situation when device-side client
		   application did not allocate buffer in time */
		if ((ep[epnum].ttype == ISOCHRONOUS) &&
		    ((ep[epnum].buffer.start == 0) || (iso_ep[epnum].state == MISS_PACKET)))
		{
			/* Clear FIFO buffer */
			MCF5272_WR_USB_EPCFG(imm, epnum, MCF5272_RD_USB_EPCFG(imm, epnum));

			if (event & MCF5272_USB_EPNISR_FIFO_LVL)
				iso_ep[epnum].state = MISS_PACKET;

			if (event & MCF5272_USB_EPNISR_EOP)
				iso_ep[epnum].state = DEFAULT;
			
			/* It is nothing to read anymore */
			fifo_data = 0;
		}
		
		bytes_read = fifo_data;

		if (ep[epnum].buffer.start)
		{
			/* Read the data from the FIFO into the buffer */
			i = ep[epnum].buffer.position;
			
			fifo_data += i;
			fifo_data -= 3;
			
			/* Read data from FIFO by 4 bytes */
			while (i < fifo_data)
			{
				*((uint32 *)(&ep[epnum].buffer.start[i])) = MCF5272_RD_USB_EPDR(imm, epnum,32);
				i += 4;
			}
			
			fifo_data += 3;
			
			/* Read rest of data from FIFO by 1 byte */
			while (i < fifo_data)
			{
				ep[epnum].buffer.start[i] = MCF5272_RD_USB_EPDR(imm, epnum,8);
				i++;
			}

			ep[epnum].buffer.position = i;
		}

		if (ep[epnum].buffer.start)
		{
			if (ep[epnum].ttype == ISOCHRONOUS)
			{
				/* packet_position contains the number of bytes read
				   in current frame */
				iso_ep[epnum].packet_position += bytes_read;

				if (event & MCF5272_USB_EPNISR_EOP)
				{
				    if (iso_ep[epnum].transfer_monitoring == TRUE)
					    /* Indicate that EOP interrupt occured in this frame */
					    iso_ep[epnum].sent_packet_watch = 0;

				    /* Save the number of bytes that we have read */
				    iso_ep[epnum].packet_length[iso_ep[epnum].frame_counter] = iso_ep[epnum].packet_position;
				    iso_ep[epnum].packet_position = 0;

				    iso_ep[epnum].frame_counter ++;

				    /* Clear FIFO buffer for the case when client asks
				       to read less then we received */
				    MCF5272_WR_USB_EPCFG(imm, epnum, MCF5272_RD_USB_EPCFG(imm, epnum));

				    /* Check if it was the last packet in current buffer */
				    if (iso_ep[epnum].frame_counter == ep[epnum].buffer.length)
				    {
					/* Extract next request from the queue */
					if (!usb_get_request(epnum))
					{
						/* There are no requests in a queue, so we can free buffer structure */
						ep[epnum].buffer.start = 0;
						ep[epnum].buffer.length = 0;
						ep[epnum].buffer.position = 0;
					}
					/* Buffer is sent - wake up usb_ep_wait function */
					wake_up_interruptible(&ep_wait_queue);
				    }
				}
			}
			else
			/* If given number of bytes is received, complete the transfer */
			if (ep[epnum].buffer.position == ep[epnum].buffer.length)
			{
				if ((epnum == 0) && (NewC))
				{
					/* We have got a new command and can wake up fetch_command routine */
					wake_up_interruptible(&fetch_command_queue);
					/* and notify client about it */
					if (usb_async_queue) usb_kill_fasync(usb_async_queue, SIGIO);
				}
				else
				{
					ep[epnum].bytes_transferred = ep[epnum].buffer.length;
				}
				ep[epnum].buffer.start = 0;
				ep[epnum].buffer.length = 0;
				ep[epnum].buffer.position = 0;
				/* Wake up usb_ep_wait function if it sleeps */
				wake_up_interruptible(&ep_wait_queue);
			}

		}  /* if (ep[epnum].buffer.start) */
		
	} /* if (event &  (EPNISR_EOP | EPNISR_LVL)) */
	
	if (event & (MCF5272_USB_EP0ISR_OUT_EOT | MCF5272_USB_EPNISR_EOT))
	{
		/* The current transfer is complete */
		/* Clear the EOT Interrupt bits */
		MCF5272_WR_USB_EPISR(imm, epnum, 0
			| MCF5272_USB_EP0ISR_OUT_EOT
			| MCF5272_USB_EPNISR_EOT);
	}

	MCF5272_WR_USB_EP0IMR(imm, imr_mask);
}
/********************************************************************/
uint32
usb_tx_data(uint32 epnum, uint8 *start, uint32 length)
{
uint32 imr_mask, imr0_mask=0;
int32 i = 0;
int32 free_space;

	/* Exit if device is reset */
	if ((epnum !=0) && (ep[epnum].state == USB_DEVICE_RESET))
			return -EIO;

	/* See if non Isochronous EP is currently busy */
	if ((ep[epnum].ttype != ISOCHRONOUS) && (ep[epnum].buffer.start))
		return -EBUSY;

	/* Make sure there is data to send */
	if (!start)
		return -EFAULT;
	if (!length)
		return 0;
	/* Make sure this is an IN endpoint */
	if (epnum && !(MCF5272_RD_USB_EPISR(imm, epnum) & MCF5272_USB_EPNISR_DIR))
		return -EPERM;
	/* Make sure this EP is not HALTed */
	if (MCF5272_RD_USB_EPISR(imm, epnum) & MCF5272_USB_EPNISR_HALT)
		return -EIO;

	/* Save off the current IMR */
	imr_mask = MCF5272_RD_USB_EPIMR(imm, epnum);

	/* Clear the EOT, EOP, LVL mask bits */
	if (epnum != 0)
	{
		MCF5272_WR_USB_EPIMR(imm, epnum, imr_mask & 
			~(MCF5272_USB_EPNISR_EOT
			| MCF5272_USB_EPNISR_EOP
			| MCF5272_USB_EPNISR_FIFO_LVL));

		imr0_mask = MCF5272_RD_USB_EPIMR(imm, 0);

		MCF5272_WR_USB_EPIMR(imm, 0, imr0_mask & 
			~(MCF5272_USB_EP0ISR_RESET
			| MCF5272_USB_EP0ISR_DEV_CFG
			| MCF5272_USB_EP0ISR_SOF));
	}
	else
		MCF5272_WR_USB_EPIMR(imm, 0, imr_mask &
			~(MCF5272_USB_EP0ISR_OUT_EOP
			| MCF5272_USB_EPNISR_EOP
			| MCF5272_USB_EP0ISR_OUT_EOT
			| MCF5272_USB_EPNISR_EOT
			| MCF5272_USB_EP0ISR_RESET
			| MCF5272_USB_EP0ISR_DEV_CFG));

	if (ep[epnum].buffer.start)
	{
	    /* Isochronous EP is currently busy, so enqueue this request */
	    if (++iso_ep[epnum].number_of_requests >= MAX_IO_REQUESTS)
	    {
		iso_ep[epnum].number_of_requests--;
		return -EBUSY;
	    }
	    if (++iso_ep[epnum].last_io_request >= MAX_IO_REQUESTS)
		    iso_ep[epnum].last_io_request = 0;
	    iso_ep[epnum].requests_queue[iso_ep[epnum].last_io_request].start = start;
	    iso_ep[epnum].requests_queue[iso_ep[epnum].last_io_request].length = length;

	    /* Restore saved imr_mask */
	    MCF5272_WR_USB_EPIMR(imm, 0, imr0_mask);
	    MCF5272_WR_USB_EPIMR(imm, epnum, imr_mask);
	    return 0;
	}

	/* Setup the EP Buffer structure */
	ep[epnum].buffer.start = start;
	ep[epnum].buffer.length = length;
	ep[epnum].buffer.position = 0;
	
	if (ep[epnum].ttype == ISOCHRONOUS)
	{
		iso_ep[epnum].frame_counter = 0;
		iso_ep[epnum].packet_length = (uint32*) start;
		/* Set up packet_position boundary */
		iso_ep[epnum].packet_position = iso_ep[epnum].packet_length[0];
		ep[epnum].buffer.start = start + 4 * length;

		/* Check if first packet has to be skipped */
		if (iso_ep[epnum].state == SKIP_FIRST_PACKET)
		{
			/* Indicate that packet was not sent. */
			iso_ep[epnum].packet_length[0] = 0;
			
			/* Does buffer contain more than 1 packet ? */
			if (ep[epnum].buffer.length > 1)
			{
				/* Stepover the first packet */
				iso_ep[epnum].frame_counter = 1;
				ep[epnum].buffer.position = iso_ep[epnum].packet_position;
				iso_ep[epnum].packet_position += iso_ep[epnum].packet_length[1];

				/* Clear state field */
				iso_ep[epnum].state = DEFAULT;
			}
			else
			{
				/* It is nothing to send anymore. */
				ep[epnum].buffer.start = 0;
				ep[epnum].buffer.position = 0;
				ep[epnum].buffer.length = 0;

				iso_ep[epnum].packet_position = 0;
				/* Wake up usb_ep_wait function */
				wake_up_interruptible(&ep_wait_queue);

				/* Restore saved imr_mask */
				MCF5272_WR_USB_EPIMR(imm, 0, imr0_mask);
				MCF5272_WR_USB_EPIMR(imm, epnum, imr_mask);
				return 0;
			}
		}
	}

	free_space = (uint16)(ep[epnum].fifo_length - MCF5272_RD_USB_EPDPR(imm, epnum));

	/* If the amount of data to be sent less than free_space, modify free_space */
	if (ep[epnum].ttype == ISOCHRONOUS) 
	{
		if (free_space > iso_ep[epnum].packet_position - ep[epnum].buffer.position)
			free_space = iso_ep[epnum].packet_position - ep[epnum].buffer.position;
	}
	else
		if (free_space > ep[epnum].buffer.length)
			free_space = ep[epnum].buffer.length;

	i = ep[epnum].buffer.position;
	free_space += i;

	/* Write data to FIFO by 4 bytes */
	free_space -= 3;
	while (i < free_space)
	{
		MCF5272_WR_USB_EPDR(imm, epnum, 32, *(uint32 *)(&ep[epnum].buffer.start[i]));
		i += 4;
	}

	free_space += 3;
	/* Write data to FIFO by 1 byte */
	while (i < free_space)
	{
		MCF5272_WR_USB_EPDR(imm, epnum, 8, ep[epnum].buffer.start[i]);
		i++;
	}
	
	ep[epnum].buffer.position = i;

	if ((i == ep[epnum].buffer.length) && (ep[epnum].ttype != ISOCHRONOUS))
	{
		/* This is all of the data to be sent */
		if ((i % ep[epnum].packet_size) != 0)
			/*Sent short packet - Clear the IN-DONE bit */
			MCF5272_WR_USB_EPCTL(imm, epnum, MCF5272_RD_USB_EPCTL(imm, epnum)
				& ~MCF5272_USB_EPNCTL_IN_DONE);
	}
	
	if (epnum != 0)
		MCF5272_WR_USB_EPIMR(imm, 0, imr0_mask);

	/* Restore saved imr_mask */
	MCF5272_WR_USB_EPIMR(imm, epnum, imr_mask);

	return i;
}
/********************************************************************/
uint32
usb_rx_data(uint32 epnum, uint8 *start, uint32 length)
{
uint32 imr_mask, imr0_mask=0;
int32 i = 0;
int32 fifo_data;

//printk("rx: %p", start);
	/* Exit if device is reset */
	if ((epnum !=0) && (ep[epnum].state == USB_DEVICE_RESET))
			return -EIO;

	/* Make sure there is data to send */
	if (!length)
		return 0;
	/* See if the EP is currently busy */
	if (ep[epnum].buffer.start && (ep[epnum].ttype != ISOCHRONOUS))
		return -EBUSY;
	/* Make sure the buffer is allocated */
	if (!start)
		return -EFAULT;
	/* Make sure this is an OUT endpoint */
	if (epnum && (MCF5272_RD_USB_EPISR(imm, epnum) & MCF5272_USB_EPNISR_DIR))
		return -EPERM;
	/* Make sure this EP is not HALTed */
	if (MCF5272_RD_USB_EPISR(imm, epnum) & MCF5272_USB_EPNISR_HALT)
		return -EIO;

	/* Save off the current IMR */
	imr_mask = MCF5272_RD_USB_EPIMR(imm, epnum);

	/* Clear the EOT, EOP and FIFO LVL mask bits */
	if (epnum != 0)
	{
		MCF5272_WR_USB_EPIMR(imm, epnum, imr_mask & 
			~(MCF5272_USB_EPNISR_EOT
			| MCF5272_USB_EPNISR_EOP
			| MCF5272_USB_EPNISR_FIFO_LVL));

		imr0_mask = MCF5272_RD_USB_EPIMR(imm, 0);

		MCF5272_WR_USB_EPIMR(imm, 0, imr0_mask & 
			~(MCF5272_USB_EP0ISR_RESET
			| MCF5272_USB_EP0ISR_DEV_CFG
			| MCF5272_USB_EP0ISR_SOF));
	}
	else
		MCF5272_WR_USB_EPIMR(imm, 0, imr_mask & 
			~(MCF5272_USB_EP0ISR_OUT_EOP
			| MCF5272_USB_EPNISR_EOP
			| MCF5272_USB_EP0ISR_OUT_EOT
			| MCF5272_USB_EPNISR_EOT
			| MCF5272_USB_EP0ISR_RESET
			| MCF5272_USB_EP0ISR_DEV_CFG));

	if (ep[epnum].buffer.start)
	{
	    /* Isochronous EP is currently busy, so enqueue this request */
	    if (++iso_ep[epnum].number_of_requests >= MAX_IO_REQUESTS)
	    {
		iso_ep[epnum].number_of_requests--;
		return -EBUSY;
	    }
	    if (++iso_ep[epnum].last_io_request >= MAX_IO_REQUESTS)
		    iso_ep[epnum].last_io_request = 0;
	    iso_ep[epnum].requests_queue[iso_ep[epnum].last_io_request].start = start;
	    iso_ep[epnum].requests_queue[iso_ep[epnum].last_io_request].length = length;

	    /* Restore saved imr_mask */
	    MCF5272_WR_USB_EPIMR(imm, 0, imr0_mask);
	    MCF5272_WR_USB_EPIMR(imm, epnum, imr_mask);
	    return 0;
	}

	/* Setup the EP Buffer structure */
	ep[epnum].buffer.start = start;
	ep[epnum].buffer.length = length;
	ep[epnum].buffer.position = 0;
	
	if (ep[epnum].ttype != ISOCHRONOUS)
	{
		/* Read the Data Present register */
		fifo_data = MCF5272_RD_USB_EPDPR(imm, epnum);

		/* If the amount of data to be received less than amount of data in FIFO, modify fifo_data */
		if (fifo_data > length)
		{
			fifo_data = length;
		}

		/* Read the data from the FIFO into the buffer */
		fifo_data -= 3;
		while (i < fifo_data)
		{
			*(uint32 *)(&ep[epnum].buffer.start[i]) = MCF5272_RD_USB_EPDR(imm, epnum,32);
			i += 4;
		}
	
		fifo_data +=3;
		while (i < fifo_data)
		{
			*(uint8 *)(&ep[epnum].buffer.start[i]) = MCF5272_RD_USB_EPDR(imm, epnum,8);
			i++;
		}
		
		ep[epnum].buffer.position = i;

		/* If it is all the data to receive, complete the transfer */
		if ( i == ep[epnum].buffer.length )
		{
			ep[epnum].bytes_transferred = i;
		
			ep[epnum].buffer.start = 0;
			ep[epnum].buffer.length = 0;
			ep[epnum].buffer.position = 0;

			wake_up_interruptible(&ep_wait_queue);
		}
	}
	else
	{
		/* Setup the iso_ep and buffer structures for ISO transfer */
		iso_ep[epnum].frame_counter = 0;
		iso_ep[epnum].packet_length = (uint32*) start;
		ep[epnum].buffer.start = start + 4 * length;
		iso_ep[epnum].packet_position = 0;
	}

	if (epnum != 0)
		MCF5272_WR_USB_EPIMR(imm, 0, imr0_mask);
	
	/* Restore saved imr_mask */
	MCF5272_WR_USB_EPIMR(imm, epnum, imr_mask);

	return i;
}
/********************************************************************/
void 
usb_bus_state_chg_service(uint32 event)
{
uint32 i;
    if (event & SUSPEND)
    {
	if (event & ENABLE_WAKEUP)
	{
		#ifdef DEBUG
			printk("Remote wakeup enabled.\n");
		#endif
        }
    }
    if (event & RESUME)
    {
		#ifdef DEBUG
			printk("The bus has been resumed (K state)\n");
		#endif
    }
    if (event & RESET)
    {
		#ifdef DEBUG
			printk("USB reset signal detected\n");
		#endif

	/* Set "Reset" status to all endpoints */
	for (i=0; i< NUM_ENDPOINTS; i++)
	{
		/* Clear buffers for all endpoints */
		ep[i].buffer.start = 0;
		ep[i].buffer.length = 0;
		ep[i].buffer.position = 0;
		ep[i].bytes_transferred = 0;
		
		/* Assing "Reset" status to all endpoints */
		ep[i].state = USB_DEVICE_RESET;

		/* Set up default values */
		if (ep[i].ttype == ISOCHRONOUS)
		{
			iso_ep[i].frame_counter = 0;
			iso_ep[i].packet_position = 0;
			iso_ep[i].start_frame_number = -1;
			iso_ep[i].final_frame_number = -1;
			iso_ep[i].state = DEFAULT;
			iso_ep[i].transfer_monitoring = FALSE;
			iso_ep[i].sent_packet_watch = 0;
			iso_ep[i].last_io_request = MAX_IO_REQUESTS;
			iso_ep[i].first_io_request = 0;
			iso_ep[i].number_of_requests = 0;
		}
	}
	/* Permit transfer for ep 0 on unconfigured device */
	ep[0].state = USB_CONFIGURED;

	/* If NewC points to a command, delete it */
	if (NewC)
	{
		kfree(NewC);
		NewC = NULL;
	}

	/* Wake up all waiting functions */
	usb_event |= USB_DEVICE_RESET;
	wake_up_interruptible(&ep_wait_queue);
	wake_up_interruptible(&fetch_command_queue);

	/* Notify client program about reset */
	if (usb_async_queue) usb_kill_fasync(usb_async_queue, SIGIO);
    }
}

/********************************************************************/
void 
usb_devcfg_service(void)
{
uint8 new_config = 0;
uint32 i, j, fifo_level;
uint32 isoep_present = FALSE;
uint8 epnum;
USB_CONFIG_DESC *pCfgDesc;
USB_INTERFACE_DESC *pIfcDesc;
USB_ENDPOINT_DESC *pEpDesc;

	/* Initialize Endpoint settings for this configuration */
	new_config = (uint8)(MCF5272_RD_USB_EP0SR(imm) >> 12);
		
	/* Start by marking all Endpoints as disabled (except EP0) */
	for (i = 1; i < NUM_ENDPOINTS; i++)
		ep[i].ttype = DISABLED;

	/* Get pointer to active Configuration descriptor */
	pCfgDesc = (USB_CONFIG_DESC *)usb_get_desc(new_config, -1, -1, -1);

	ep[0].state = USB_CONFIGURED;

	ep[0].buffer.start = 0;
	ep[0].buffer.position = 0;
	ep[0].buffer.length = 0;

	/* Iterate through all the valid Interfaces in this Configuration */
	for (i = 0; i < pCfgDesc->bNumInterfaces; i++)
	{
		pIfcDesc = (USB_INTERFACE_DESC *)usb_get_desc(new_config, i, AltSetting(imm, i), -1);
		/* Iterate through all the valid Endpoints in this interface */
		for (j = 0; j < pIfcDesc->bNumEndpoints; j++)
		{
			pEpDesc = (USB_ENDPOINT_DESC *)usb_get_desc(new_config, i, AltSetting(imm, i), j);
			if (pEpDesc)
			{
				epnum = pEpDesc->bEndpointAddress & 0x0F;

				/* See if this Endpoint is actually active */
				if (!(MCF5272_RD_USB_EPISR(imm, epnum) & 
					  MCF5272_USB_EPNISR_PRES))
					return;

				/* Set the new transfer type */
				ep[epnum].ttype = pEpDesc->bmAttributes;

				/* Set the current direction */
				if (MCF5272_RD_USB_EPISR(imm, epnum) & 
					MCF5272_USB_EPNISR_DIR)
					ep[epnum].dir = IN;
				else
					ep[epnum].dir = OUT;

				/* Get the current packet size */
				ep[epnum].packet_size = (uint16)(pEpDesc->wMaxPacketSizeL + (pEpDesc->wMaxPacketSizeH << 8));

				/* No buffer allocated yet */
				ep[epnum].buffer.start = 0;
				ep[epnum].buffer.position = 0;
				ep[epnum].buffer.length = 0;
										
				ep[epnum].sendZLP = FALSE;
				
				MCF5272_WR_USB_EPCTL(imm, epnum, 0);

				if (ep[epnum].ttype != ISOCHRONOUS)
				{
					/* Set the FIFO length */
					ep[epnum].fifo_length = (uint16)(ep[epnum].packet_size * FIFO_DEPTH);
					
					ep[epnum].bytes_transferred = 0;

					/* Enable EOP and EOT interrupts, disable FIFO_LVL */
					MCF5272_WR_USB_EPIMR(imm, epnum,
						((MCF5272_RD_USB_EPIMR(imm, epnum)
							& ~MCF5272_USB_EPNISR_FIFO_LVL)
							| (MCF5272_USB_EPNISR_EOP
							| MCF5272_USB_EPNISR_EOT)));

					if (ep[epnum].dir == IN)
						/* Set the IN-DONE bit - This is only required
						   on pre-2K75N masks. This bit is set by HW 
						   beginning with the 2K75N silicon. */
						MCF5272_WR_USB_EPCTL(imm, epnum, MCF5272_USB_EPNCTL_IN_DONE);
				}
				else
				{
					/* Set the FIFO length for isochronous endpoint */
					ep[epnum].fifo_length = ISOCHRONOUS_FIFO_LENGTH;

					/* Set up default values */
					iso_ep[epnum].start_frame_number = -1;
					iso_ep[epnum].final_frame_number = -1;
					iso_ep[epnum].transfer_monitoring = FALSE;
					iso_ep[epnum].state = DEFAULT;
					iso_ep[epnum].frame_counter = 0;
					iso_ep[epnum].packet_position = 0;
					iso_ep[epnum].last_io_request = MAX_IO_REQUESTS;
					iso_ep[epnum].first_io_request = 0;
					iso_ep[epnum].number_of_requests = 0;

					/* Disable EOT interrupt, enable FIFO_LVL and EOP*/
					MCF5272_WR_USB_EPIMR(imm, epnum,
						((MCF5272_RD_USB_EPIMR(imm, epnum)
							& ~(MCF5272_USB_EPNISR_EOT))
							| (MCF5272_USB_EPNISR_FIFO_LVL
							| MCF5272_USB_EPNISR_EOP)));
						
					/* Set up appropriate FIFO threshold level */
					if ((ISOCHRONOUS_FIFO_LENGTH >> 2) >= THRESHOLD_FIFO_FULLNESS)
						fifo_level =2;
					else
					if ((ISOCHRONOUS_FIFO_LENGTH >> 1) >= THRESHOLD_FIFO_FULLNESS)
						fifo_level = 1;
					else
						fifo_level = 0;
						
					MCF5272_WR_USB_EPCTL(imm, epnum, MCF5272_USB_EPNCTL_ISO_MODE
									| MCF5272_USB_EPNCTL_FIFO_LVL(fifo_level));
					isoep_present = TRUE;
				}
				
				ep[epnum].state = USB_CONFIGURED;
			}
		}
	}
	
	/* Initialize FIFO module according the new configuration / interface */
	usb_fifo_init();

	/* If at least one isochronous endpoint is present in configuration / interface,
		enable SOF interrupt. Otherwise, disable it. */
	if (isoep_present)
		MCF5272_WR_USB_EP0IMR(imm, MCF5272_RD_USB_EP0IMR(imm)
					| MCF5272_USB_EP0IMR_SOF_EN);
	else
		MCF5272_WR_USB_EP0IMR(imm, MCF5272_RD_USB_EP0IMR(imm)
					& (~MCF5272_USB_EP0IMR_SOF_EN));

	usb_event |= USB_CONFIGURATION_CHG;
	current_config.cur_config_num = new_config;
	current_config.altsetting = MCF5272_RD_USB_ASR(imm);

	/* Notify client application about change of configuration */
	if (usb_async_queue) usb_kill_fasync(usb_async_queue, SIGIO);
}


/********************************************************************/
void
usb_vendreq_done(uint32 error)
{
	if (error)
		/* This sends a single STALL as the handshake */
		MCF5272_WR_USB_EP0CTL(imm,
			MCF5272_RD_USB_EP0CTL(imm)
			| MCF5272_USB_EP0CTL_CMD_OVER
			| MCF5272_USB_EP0CTL_CMD_ERR);
	else
		MCF5272_WR_USB_EP0CTL(imm,
			MCF5272_RD_USB_EP0CTL(imm)
			| MCF5272_USB_EP0CTL_CMD_OVER);
	return;
}
/********************************************************************/
void
usb_ep_halt(uint32 epnum)
{
	#ifdef DEBUG
		printk("Endpoint %d has been HALTed\n", (int)epnum);
	#endif
}

/********************************************************************/
void
usb_ep_unhalt(uint32 epnum)
{
	#ifdef DEBUG
		printk("Endpoint %d has been UNHALTed\n", (int)epnum);
	#endif
}


/********************************************************************/
void
usb_sort_ep_array(USB_EP_STATE *list[], int n)
{
/* Simple bubble sort function called by usb_fifo_init() */
int i, pass, sorted;
USB_EP_STATE *temp;
pass = 1;

	do {
		sorted = 1;
		for (i = 0; i < n - pass; ++i)
		{
			if (list[i]->fifo_length < list[i+1]->fifo_length )
			{
				/* Exchange out-of-order pair */
				temp = list[i];
				list[i] = list[i + 1];
				list[i+1] = temp;
				sorted = 0;
			}
		}
		++pass;
	} while(!sorted);
}

/********************************************************************/
void
usb_make_power_of_two(uint32 *size)
{
/* Called by usb_fifo_init() */
int i, not_power_of_two = 0;
uint32 new_size = *size;

	for (i = 0; new_size != 1; i++)
	{
		if (new_size & 0x0001)
			not_power_of_two = 1;
		new_size >>= 1;
	}
	new_size = 1 << (i + not_power_of_two);
	if (new_size > 256)
		new_size = 256;
	*size = new_size;
}
/********************************************************************/
void
usb_fifo_init(void)
{
USB_EP_STATE *pIN[NUM_ENDPOINTS];
USB_EP_STATE *pOUT[NUM_ENDPOINTS];
uint8 nIN, nOUT, i;
uint16 INpos, OUTpos;

	/* Endpoint 0 is always present and bi-directional */
	pIN[0] = &ep[0];
	pOUT[0] = &ep[0];
	nIN = nOUT = 1;

	/* Sort the active endpoints by direction */
	for (i = 1; i < NUM_ENDPOINTS; i++)
	{
		if (ep[i].ttype != DISABLED)
		{
			if (ep[i].dir == IN)
				pIN[nIN++] = &ep[i];
			else
				pOUT[nOUT++] = &ep[i];
		}
	}

	/* Make sure FIFO size is a power of 2; if not, make it so */
	for (i = 0; i < nIN; i++)
		usb_make_power_of_two( &(pIN[i]->fifo_length) );
	for (i = 0; i < nOUT; i++)
		usb_make_power_of_two( &(pOUT[i]->fifo_length) );

	/* Sort the endpoints by FIFO length (decending) */
	usb_sort_ep_array(pIN, nIN);
	usb_sort_ep_array(pOUT, nOUT);

	/* Calculate the FIFO address for each endpoint */
	INpos = 0;
	OUTpos = 0;
	for (i = 0; i < nIN; i++)
	{
		pIN[i]->in_fifo_start = INpos;
		INpos += pIN[i]->fifo_length;
	}
	for (i = 0; i < nOUT; i++)
	{
		pOUT[i]->out_fifo_start = OUTpos;
		OUTpos += pOUT[i]->fifo_length;
	}

	/* Save the FIFO settings into the Configuration Registers */
	
	/* Initialize Endpoint 0 IN FIFO */
	MCF5272_WR_USB_IEP0CFG(imm, 0
		| (ep[0].packet_size << 22)
		| (ep[0].fifo_length << 11)
		|  ep[0].in_fifo_start);

	/* Initialize Endpoint 0 OUT FIFO */
	MCF5272_WR_USB_OEP0CFG(imm, 0
		| (ep[0].packet_size << 22)
		| (ep[0].fifo_length << 11)
		|  ep[0].out_fifo_start);

	for (i = 1; i < NUM_ENDPOINTS; i++)
	{
		if (ep[i].ttype != DISABLED)
		{
			if (ep[i].dir == IN)
				/* Initialize Endpoint i FIFO */
				MCF5272_WR_USB_EPCFG(imm, i, 0
				| (ep[i].packet_size << 22)
				| (ep[i].fifo_length << 11)
				|  ep[i].in_fifo_start);
			else
				/* Initialize Endpoint i FIFO */
				MCF5272_WR_USB_EPCFG(imm, i, 0
				| (ep[i].packet_size << 22)
				| (ep[i].fifo_length << 11)
				|  ep[i].out_fifo_start);
		}
	}
}

/********************************************************************/
int
usb_ep_is_busy(uint32 epnum)
{
	/* If device is reset, return with error */
	if (ep[epnum].state == USB_DEVICE_RESET)
		return -USB_DEVICE_RESET;

	/* See if the EP is currently busy */
	if (ep[epnum].buffer.start)
		return -USB_EP_IS_BUSY;
	
	return -USB_EP_IS_FREE;
}

/********************************************************************/
int
usb_ep_wait(uint32 epnum, uint32 requests)
{ 
int length;
	/* Sleep until EP becomes free or reset detected */
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0)
	if (ep[epnum].ttype != ISOCHRONOUS)
	{
		if (wait_event_interruptible(ep_wait_queue, ((ep[epnum].state == USB_DEVICE_RESET)
		    || (ep[epnum].buffer.start == 0))))
			return(-ERESTARTSYS);
	}
	else
		if (wait_event_interruptible(ep_wait_queue, ( (ep[epnum].state == USB_DEVICE_RESET)
		    || (!ep[epnum].buffer.start && !iso_ep[epnum].number_of_requests)
		    || (iso_ep[epnum].number_of_requests < requests) )))
			return(-ERESTARTSYS);
	#else

	uint32 flags;
	save_flags(flags); cli();
	if (ep[epnum].ttype != ISOCHRONOUS)
	{
		while ((ep[epnum].state != USB_DEVICE_RESET) && (ep[epnum].buffer.start != 0))
		{
			if (current->signal & ~current->blocked) 
			{
				restore_flags(flags);
				return(-ERESTARTSYS);
			}
			interruptible_sleep_on(&ep_wait_queue);
		}
	}
	else
		while ((ep[epnum].state != USB_DEVICE_RESET)
		    && !(!ep[epnum].buffer.start && !iso_ep[epnum].number_of_requests)
		    && !(iso_ep[epnum].number_of_requests < requests) )
		{
			if (current->signal & ~current->blocked) 
			{
				restore_flags(flags);
				return(-ERESTARTSYS);
			}
			interruptible_sleep_on(&ep_wait_queue);
		}
	
	restore_flags(flags);
	#endif

	/* See if Reset has occured */
	if (ep[epnum].state != USB_DEVICE_RESET)
	{
		if (ep[epnum].ttype == ISOCHRONOUS)
			return -USB_EP_IS_FREE;
		else
		{
			length = ep[epnum].bytes_transferred;
			ep[epnum].bytes_transferred = 0;
			return length;
		}
	}

	return -USB_DEVICE_RESET;
}
/********************************************************************/
uint8*
usb_get_desc(int8 config, int8 iface, int8 setting, int8 ep)
{
	/* Note:
	 * ep is the offset and not the physical endpoint number
	 * In order to get the config desc pointer, specify -1 for 
	 * all other inputs except config
	 * In order to get the interface/alternate setting desc pointer,
	 * specify -1 for epNum
	 */
	int i;
	uint8 *pDesc = usb_device_descriptor;

	if (config != -1)
	{
		if (config > ((USB_DEVICE_DESC *)pDesc)->bNumConfigurations)
			return 0;
		
		/* Allow for non-standard desc between device and config desc */
		while (pDesc[1] != CONFIGURATION)
			pDesc += pDesc[0];	

		/* pDesc now points to Config 1 descriptor */
		for (i = 1; i < config;)
		{
			pDesc += pDesc[0];
			if (pDesc[1] == CONFIGURATION)
				i++;
		}

		/* pDesc now points to the correct Configuration descriptor */
		if ((iface != -1) && (setting != -1))
		{
			if (iface >= ((USB_CONFIG_DESC *)pDesc)->bNumInterfaces)
				return 0;

			/* Allow for non-standard desc between config and iface desc */
			while (pDesc[1] != INTERFACE)
				pDesc += pDesc[0];

			/* pDesc now points to first Interface descriptor */
			for (i = 0; i < iface;)
			{
				pDesc += pDesc[0];
				if (pDesc[1] == INTERFACE && pDesc[3] == 0)
					i++;
			}
			/* pDesc now points to correct Interface descriptor */
			for (i = 0; i < setting;)
			{
				pDesc += pDesc[0];
				if (pDesc[1] == INTERFACE)
				{
					if (pDesc[2] != iface)
						return 0;
					else
						i++;
				}
			}

			/* pDesc now points to correct Alternate Setting descriptor */
			if (ep != -1)
			{
				if (ep >= pDesc[4])
					return 0;

				/* Allow for non-standard desc between iface and ep desc */
				while (pDesc[1] != ENDPOINT)
					pDesc += pDesc[0];

				/* pDesc now points to first Endpoint descriptor */
				for (i = 0; i < ep;)
				{
					pDesc += pDesc[0];
					if (pDesc[1] == ENDPOINT)
						i++;
				}
				/* pDesc now points to the correct Endpoint descriptor */
			}
		}
	}
	return (pDesc);
}

/********************************************************************/
void 
usb_vendreq_service(uint8  bmRequestType,
					uint8  bRequest, 
					uint16 wValue,
					uint16 wIndex, 
					uint16 wLength)
{
	if ((bmRequestType < 128) && (wLength > 0))
	{
		/* Allocate memory for a new command */
		/* There is a data stage in this request and direction of data transfer is from Host to Device */
		NewC = (DEVICE_COMMAND *) kmalloc(sizeof(DEVICE_COMMAND) + wLength, GFP_ATOMIC);
		
		/* Store the address where new command will be placed */
		NewC -> cbuffer = (uint8 *) NewC + sizeof(DEVICE_COMMAND);
	}
	else
		/* Direction of data transfer is from Device to Host, or no data stage */
		NewC = (DEVICE_COMMAND *) kmalloc(sizeof(DEVICE_COMMAND), GFP_ATOMIC);
	
	if (NewC == NULL)
	{
		#ifdef DEBUG
			printk("Could not allocate queue item.\n");
		#endif
		
		if ((wLength != 0) && (bmRequestType > 127))
			/* The direction of data transfer is from Device to Host,
			send zero-length packet to indicate no data will be provided */
			MCF5272_WR_USB_EP0CTL(imm, MCF5272_RD_USB_EP0CTL(imm)
					& (~ MCF5272_USB_EP0CTL_IN_DONE));
		
		usb_vendreq_done(MALLOC_ERROR);

		return;
	}
	
	/* Store request specific values */
	NewC -> request.bmRequestType = bmRequestType;
	NewC -> request.bRequest = bRequest;
	NewC -> request.wValue = wValue;
	NewC -> request.wIndex = wIndex;
	NewC -> request.wLength = wLength;
	
	if ((bmRequestType < 128) && (wLength > 0))
	{
		/* This request has data stage and the direction of command block
		transfer is from Host to Device */
	
		/* Prepare ep[0] structure to accept command (during data stage)*/
		ep[0].buffer.start = NewC -> cbuffer;
		ep[0].buffer.length = wLength;
		ep[0].buffer.position = 0;
		
		/* usb_out_service() will receive a command and will call usb_vendreq_done() */
	}
	else
	{
		/* This request has no data stage or direction of command block transfer
		is from Device to Host.
		In any case, client application has to be informed about new request */

		if (usb_async_queue) usb_kill_fasync(usb_async_queue, SIGIO);
		
		/* We have got a new command and can wake up fetch_command routine */
		wake_up_interruptible(&fetch_command_queue);
	}
}

/********************************************************************/
void
usb_isochronous_transfer_service(void)
{
uint32 imr_mask, epnum;
uint32 free_space, i;

for (epnum = 1; epnum < NUM_ENDPOINTS; epnum++)
{
	if (ep[epnum].ttype == ISOCHRONOUS)
	{
		/* Save IMR */
		imr_mask = MCF5272_RD_USB_EP0IMR(imm);
		/* Disable RESET and DEV_CFG interrupts */
		MCF5272_WR_USB_EP0IMR(imm, imr_mask & ~(MCF5272_USB_EP0ISR_RESET
						| MCF5272_USB_EP0ISR_DEV_CFG));

		if ((iso_ep[epnum].transfer_monitoring == TRUE) && (ep[epnum].buffer.start))
		{
			if (ep[epnum].dir == IN)
			{
				/* Set up sent_packet_watch for the next frame to see, if EOP occurs */
				iso_ep[epnum].sent_packet_watch ++;
				/* It must be 1, now */
				
				/* If EOP interrupt did not occur in previous frame, handle it */
				if (iso_ep[epnum].sent_packet_watch > 1)
				{
					/* Remove unsent packet from FIFO buffer */
					MCF5272_WR_USB_EPCFG(imm, epnum, MCF5272_RD_USB_EPCFG(imm, epnum));
					
					/* Reset the counter */
					iso_ep[epnum].sent_packet_watch = 0;
					iso_ep[epnum].packet_length[iso_ep[epnum].frame_counter] = 0;
					
					iso_ep[epnum].frame_counter ++;
					
					/* Is it last packet ? */
					if (iso_ep[epnum].frame_counter == ep[epnum].buffer.length)
					{
						/* End of buffer transfer - wake up usb_ep_wait */
						wake_up_interruptible(&ep_wait_queue);

						/* case 3 */
						/* The first packet of next buffer must be skipped */
						if (iso_ep[epnum].number_of_requests > 0)
						{
						    iso_ep[epnum].number_of_requests--;
						    /* Extract next request from the queue */
						    ep[epnum].buffer.length = iso_ep[epnum].requests_queue[iso_ep[epnum].first_io_request].length;
						    iso_ep[epnum].packet_length = (uint32*) iso_ep[epnum].requests_queue[iso_ep[epnum].first_io_request].start;

						    ep[epnum].buffer.position = iso_ep[epnum].packet_length[0];
						    /* Indicate that this packet was not sent. */
						    iso_ep[epnum].packet_length[0] = 0;

						    if (++iso_ep[epnum].first_io_request >= MAX_IO_REQUESTS)
							iso_ep[epnum].first_io_request = 0;

						    /* Check if new buffer contains only 1 packet. If true - skip entire buffer,
						       if false - skip only first packet of buffer */
						    if (ep[epnum].buffer.length > 1)
						    {
							    ep[epnum].buffer.start = (uint8*)iso_ep[epnum].packet_length + 4 * ep[epnum].buffer.length;
							    iso_ep[epnum].packet_position = ep[epnum].buffer.position + iso_ep[epnum].packet_length[1];
							    
							    iso_ep[epnum].frame_counter = 1;
						    }
						    else
						    
							/* Extract next request from the queue */
							if (usb_get_request(epnum))
							    iso_ep[epnum].packet_position = iso_ep[epnum].packet_length[0];
							    
						} else /* If there are no requests in a queue */
						{
							/* The first packet of next buffer must be skipped by tx_data */
							iso_ep[epnum].state = SKIP_FIRST_PACKET;

							ep[epnum].buffer.start = 0;
							ep[epnum].buffer.length = 0;
							
							ep[epnum].buffer.position = 0;
					
							iso_ep[epnum].frame_counter = 0;
						}
					}
					else
					/* Is it next to last packet ? */
					if ((iso_ep[epnum].frame_counter + 1) >= ep[epnum].buffer.length)
					{
						/* case 2 */
						/* The next transfer must be started from first packet */
						iso_ep[epnum].state = DEFAULT;
						
						iso_ep[epnum].packet_length[ep[epnum].buffer.length] = 0;

						/* Extract next request from the queue */
						if (usb_get_request(epnum))
						    iso_ep[epnum].packet_position = iso_ep[epnum].packet_length[0];
						else
						{
							ep[epnum].buffer.start = 0;
							ep[epnum].buffer.length = 0;
							ep[epnum].buffer.position = 0;
					
							iso_ep[epnum].frame_counter = 0;
						}
					}
					else
					{
						/* case 1 */
						/* Step over the next packet in the buffer */
						ep[epnum].buffer.position = iso_ep[epnum].packet_position + iso_ep[epnum].packet_length[iso_ep[epnum].frame_counter];
						/* Indicate that this packet was not sent. */
						iso_ep[epnum].packet_length[iso_ep[epnum].frame_counter] = 0;
						
						iso_ep[epnum].frame_counter ++;
						/* Set up packet_position boundary for the packet */
						iso_ep[epnum].packet_position = ep[epnum].buffer.position + iso_ep[epnum].packet_length[iso_ep[epnum].frame_counter];
					}

					if (ep[epnum].buffer.start)
					{
						free_space = ep[epnum].fifo_length;
						if (free_space > iso_ep[epnum].packet_length[iso_ep[epnum].frame_counter])
							free_space = iso_ep[epnum].packet_length[iso_ep[epnum].frame_counter];

						i = ep[epnum].buffer.position;

						free_space += i;
						free_space -= 3;
					
						/* Write to FIFO by 4 bytes */
						while (i < free_space)
						{
							MCF5272_WR_USB_EPDR(imm, epnum, 32, *(uint32 *)(&ep[epnum].buffer.start[i]));
							i += 4;
						}
				
						free_space += 3;
				
						/* Write to FIFO by 1 byte */
						while (i < free_space)
						{
							MCF5272_WR_USB_EPDR(imm,epnum,8,ep[epnum].buffer.start[i]);
							i++;
						}

						ep[epnum].buffer.position = i;
					}
				}
			}  /* if (ep[epnum].dir == IN) */
			else
			{
				/* Set up sent_packet_watch for the next frame to see, if EOP occurs */
				iso_ep[epnum].sent_packet_watch ++;
				/* It must be 1, now */

				/* Check if EOP occured in previous frame */
				if (iso_ep[epnum].sent_packet_watch > 1)
				{
					/* Reset the counter */
					iso_ep[epnum].sent_packet_watch = 1;
					ep[epnum].buffer.position += iso_ep[epnum].packet_length[iso_ep[epnum].frame_counter];
					/* Indicate that last packet was not received. */
					iso_ep[epnum].packet_length[iso_ep[epnum].frame_counter] = 0;
					iso_ep[epnum].frame_counter ++;
					iso_ep[epnum].packet_position = 0;

					/* Is it all packets to read ? */
					if (iso_ep[epnum].frame_counter == ep[epnum].buffer.length)
					{
						/* Buffer sent - wake up usb_ep_wait */
						wake_up_interruptible(&ep_wait_queue);

						/* Extract next request from the queue */
						if (usb_get_request(epnum))
						    iso_ep[epnum].packet_position = 0;
						else
						{
						    ep[epnum].buffer.start = 0;
						    ep[epnum].buffer.length = 0;
						    ep[epnum].buffer.position = 0;
						}
					}
				}
			}
		}
		else
			if (iso_ep[epnum].start_frame_number == MCF5272_RD_USB_FNR(imm))
			{
				/* Open data stream */
				iso_ep[epnum].transfer_monitoring = TRUE;
				iso_ep[epnum].start_frame_number = -1;
				iso_ep[epnum].sent_packet_watch = 0;
			}
		if (iso_ep[epnum].final_frame_number == MCF5272_RD_USB_FNR(imm))
		{
			/* Close data stream */
			iso_ep[epnum].transfer_monitoring = FALSE;
			iso_ep[epnum].final_frame_number = -1;
			iso_ep[epnum].state = DEFAULT;
		}

		MCF5272_WR_USB_EP0IMR(imm, imr_mask);

	}  /* if (ep[epnum].ttype == ISOCHRONOUS) */
}  /* for (epnum = 1; ... */
}
/********************************************************************/
uint32
usb_fetch_command(DEVICE_COMMAND * client_item)
{
uint32 event;

	/* Sleep until the new command is received */
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0)
	if (wait_event_interruptible(fetch_command_queue, NewC || usb_event))
		return(-ERESTARTSYS);
	#else
	uint32 flags;
	while (!NewC && !usb_event) {
		save_flags(flags); cli();
		if (current->signal & ~current->blocked) {
			restore_flags(flags);
			return(-ERESTARTSYS);
		}
		interruptible_sleep_on(&fetch_command_queue);
		restore_flags(flags);
	}
	#endif

	event = usb_event;
	usb_event = 0;
	
	/* Check if device is configured */
	if (ep[0].state == USB_NOT_CONFIGURED)
	{
		return USB_DEVICE_RESET;
	}

	/* We have to see if command Queue is not empty */
	if (NewC == NULL)
	{
		return event;
	}

	/* Copy the request to client's structure, pointed by client_item.
	   In order to save address of client's command buffer in client,
	   we have start fill structure from client_item -> cbuffer_length,
	   not from client_item (which means client_item -> cbuffer)  */
	memcpy((uint8 *) (&(client_item -> request.bmRequestType)), (uint8 *) (&(NewC -> request.bmRequestType)), sizeof(REQUEST));

	/* Now, we can copy the contents of command buffer (the command itself)
	   to the command buffer of client, pointed by client_item -> cbuffer */
	if (NewC->request.bmRequestType < 128)
		memcpy(client_item -> cbuffer, NewC -> cbuffer,  NewC -> request.wLength);

	return (event | USB_NEW_COMMAND);
}

/****************************END_OF_USB_SECTION******************************/
void 
irq64_h(int irq, void *dev_id, struct pt_regs *regs)
{
/* for debug purposes */
}

/* This function is called whenever the FASYNC flag
   changes for an open file */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
int usb_device_fasync (int ff, struct file *file, int mode)
{
    return fasync_helper(ff, file, mode, &usb_async_queue);
}
#else
int usb_device_fasync (struct inode *inode, struct file *file, int mode)
{
    return fasync_helper(inode, file, mode, &usb_async_queue);
}
#endif

/* This function is called whenever a process
 * attempts to open the device file */
static int usb_device_open(struct inode *inode, struct file *file)
{
#ifdef DEBUG
  printk ("usb_device_open(%p,%p)\n", inode, file);
#endif

  /* See if EP is already opened */
  if (active_ep_mask & (1 << MINOR(inode->i_rdev))) 
	return -EBUSY;
  active_ep_mask |= (1 << MINOR(inode->i_rdev));

  if (MINOR(inode->i_rdev) >= NUM_ENDPOINTS)
	return -ENODEV;

  /* Increment module usage count */
  MOD_INC_USE_COUNT;
  return SUCCESS;
}

/* This function is called when a process closes the
 * device file. It doesn't have a return value in
 * version 2.0.x because it can't fail (you must ALWAYS
 * be able to close a device). In version 2.2.x it is
 * allowed to fail - but we won't let it. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
static int usb_device_release(struct inode *inode, struct file *file)
#else
static void usb_device_release(struct inode *inode, struct file *file)
#endif
{
#ifdef DEBUG
  printk ("usb_device_release(%p,%p)\n", inode, file);
#endif

  /* Reset start, length and position to prevent from copying
     to/from user memory space from interrupt handler when 
     application crashed or device file is closed*/
  ep[MINOR(GET_RDEV(file))].buffer.start = 0;
  ep[MINOR(GET_RDEV(file))].buffer.position = 0;
  ep[MINOR(GET_RDEV(file))].buffer.length = 0;

  iso_ep[MINOR(GET_RDEV(file))].transfer_monitoring = FALSE;
  
  /* Reset corresponding bit for closed endpoint file */
  active_ep_mask &= (0xFE << MINOR(inode->i_rdev));

  if (!active_ep_mask)
  {
	  /* Remove this file from the asynchronously notified files */
	  #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
	  usb_device_fasync(-1, file, 0);
	  #else
	  usb_device_fasync(inode, file, 0);
	  #endif
	
	  /* Disable MCF5272 USB module */
	  MCF5272_WR_USB_EP0CTL(imm, 0);
  }
  
  /* Decrement the usage count, otherwise once you
   * opened the file you'll never get rid of the module. */
  MOD_DEC_USE_COUNT;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
  return 0;
#endif
}

/* This function is called whenever a process which
 * have already opened the device file attempts to
 * read from it. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
static ssize_t usb_device_read(struct file *file,
    char *buffer,    /* The buffer to fill with data */
    size_t length,   /* The length of the buffer */
    loff_t *offset)  
#else
static int usb_device_read(
    struct inode *inode,
    struct file *file,
    char *buffer,   /* The buffer to fill with the data */
    int length)     /* The length of the buffer */
#endif
{
#ifdef DEBUG
    printk ("device_read(%d,%d)\n", MINOR(GET_RDEV(file)), length);
#endif
return usb_rx_data(MINOR(GET_RDEV(file)), (uint8 *) buffer, (uint32) length);
}

/* This function is called when somebody tries to
 * write into our device file. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
static ssize_t usb_device_write(struct file *file, const char *buffer,
                            size_t length, loff_t *offset)
#else
static int usb_device_write(struct inode *inode, struct file *file,
                        const char *buffer, int length)
#endif
{
#ifdef DEBUG
  printk ("device_write(%d,%d)\n", MINOR(GET_RDEV(file)), length);
#endif
return usb_tx_data(MINOR(GET_RDEV(file)), (uint8 *) buffer, (uint32) length)
;
}

/***************************IOCTL section****************************/
int usb_device_ioctl(struct inode *inode, struct file *file,
                 unsigned int ioctl_num,/* The number of the ioctl */
                 unsigned long ioctl_param) /* The parameter to it */
{
  /* Switch according to the ioctl called */
  switch (ioctl_num) {
    /* Initialization of USB driver */
    case USB_INIT:
	/* Initialization allowed only through EP0 */
	if (MINOR(inode->i_rdev) != 0) 
	    return -EPERM;
	usb_init((DESC_INFO *)ioctl_param);
	break;
    /* Returns the command */
    case USB_GET_COMMAND:
	if (MINOR(inode->i_rdev) != 0) 
	    return -EPERM;
	return usb_fetch_command((DEVICE_COMMAND*)ioctl_param);
    /* Is EP busy ? */
    case USB_EP_BUSY:
	return usb_ep_is_busy(MINOR(inode->i_rdev));
    /* Wait while EP is busy */
    case USB_EP_WAIT:
	return usb_ep_wait(MINOR(inode->i_rdev), (uint32)ioctl_param);
    /* Returns the number of current configuration */
    case USB_GET_CURRENT_CONFIG:
	if (MINOR(inode->i_rdev) != 0)
	    return -EPERM;
	((CONFIG_STATUS*)(ioctl_param))->altsetting = current_config.altsetting;
	((CONFIG_STATUS*)(ioctl_param))->cur_config_num = current_config.cur_config_num;
	return 0;
    /* Sets send_ZLP flag */
    case USB_SET_SEND_ZLP:
	ep[MINOR(inode->i_rdev)].sendZLP = TRUE;
	break;
    case USB_COMMAND_ACCEPTED:
	if (NewC)
	{
		/* Send CMD_OVER and free NewC */
		usb_vendreq_done(SUCCESS);

		kfree(NewC);
		NewC = NULL;
		return SUCCESS;
	}
	else return -EPERM;
    case USB_NOT_SUPPORTED_COMMAND:
	if (NewC)
	{
		/* The direction of command block transfer is from Device to Host,
		but client application does not accept a request (no data is provided) */
		if ((NewC->request.bmRequestType > 127) && (NewC->request.wLength != 0))
			MCF5272_WR_USB_EP0CTL(imm, MCF5272_RD_USB_EP0CTL(imm)
				& (~ MCF5272_USB_EP0CTL_IN_DONE));

		/* Send CMD_OVER and CMD_ERR and free NewC */
		usb_vendreq_done(NOT_SUPPORTED_COMMAND);

		kfree(NewC);
		NewC = NULL;
		return SUCCESS;
	}
	else return -EPERM;
    /* Set start_frame_number to begin ISO transfer monitoring */
    case USB_SET_START_FRAME:
	iso_ep[MINOR(inode->i_rdev)].start_frame_number = (uint32)ioctl_param;
	break;
    /* Set final_frame_number to stop ISO transfer monitoring */
    case USB_SET_FINAL_FRAME:
	iso_ep[MINOR(inode->i_rdev)].final_frame_number = (uint32)ioctl_param;
	break;
    /* Returns current frame number */
    case USB_GET_FRAME_NUMBER:
	return MCF5272_RD_USB_FNR(imm);
    /* STALLs given EP */
    case USB_EP_STALL:
	if ((MINOR(inode->i_rdev)) && (ep[(MINOR(inode->i_rdev))].ttype != ISOCHRONOUS))
		MCF5272_WR_USB_EPCTL(imm, (MINOR(inode->i_rdev)),
		MCF5272_RD_USB_EPCTL(imm, (MINOR(inode->i_rdev)))
				| MCF5272_USB_EPNCTL_STALL);
	else
		return -EPERM;
	break;
    default: return -ENOTTY;
  }
  return SUCCESS;
}

/* Module Declarations ************************************* */
struct file_operations Fops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
  NULL,			/* owner */
#endif
  NULL,   		/* seek */
  usb_device_read,	/* read */
  usb_device_write,	/* write */
  NULL,   		/* readdir */
  NULL,   		/* select */
  usb_device_ioctl,   	/* ioctl */
  NULL,   		/* mmap */
  usb_device_open,	/* open */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
  NULL,    		/* flush */
#endif
  usb_device_release,  	/* close */
  NULL,			/* fsync */
  usb_device_fasync	/* fasync */
};

/* Initialize the module - Register the character device */
#ifndef MODULE
long init_usb(void) {
#else /* MODULE */
int init_module() {
#endif /* MODULE */

  int reg_result;

  /* Register the character device.
     Negative values signify an error */
  reg_result = register_chrdev(MAJOR_NUM, DEVICE_NAME, &Fops);
  if (reg_result < 0) {
    printk ("%s device failed with %d\n",
            "Registering the USB", reg_result);
    return 0;
  }
  printk ("%sthe major device number is %d.\n",
          "MCF5272 USB driver successfully installed:\n", MAJOR_NUM);
	  
  /* Setup interrupt handlers */
  request_irq(77, usb_endpoint0_isr, SA_INTERRUPT, "ColdFire USB (EP0)", NULL);
  request_irq(78, usb_endpoint_isr, SA_INTERRUPT, "ColdFire USB (EP1)", NULL);
  request_irq(79, usb_endpoint_isr, SA_INTERRUPT, "ColdFire USB (EP2)", NULL);
  request_irq(80, usb_endpoint_isr, SA_INTERRUPT, "ColdFire USB (EP3)", NULL);
  request_irq(81, usb_endpoint_isr, SA_INTERRUPT, "ColdFire USB (EP4)", NULL);
  request_irq(82, usb_endpoint_isr, SA_INTERRUPT, "ColdFire USB (EP5)", NULL);
  request_irq(83, usb_endpoint_isr, SA_INTERRUPT, "ColdFire USB (EP6)", NULL);
  request_irq(84, usb_endpoint_isr, SA_INTERRUPT, "ColdFire USB (EP7)", NULL);

  return 0;
}

/* Cleanup - unregister the appropriate file if driver acts like a module */
#ifdef MODULE
void cleanup_module()
{
  int unreg_result;

/* Disable all interrupts */
  MCF5272_WR_USB_EP0IMR(imm, 0);
  MCF5272_WR_USB_EP1IMR(imm, 0);
  MCF5272_WR_USB_EP2IMR(imm, 0);
  MCF5272_WR_USB_EP3IMR(imm, 0);
  MCF5272_WR_USB_EP4IMR(imm, 0);
  MCF5272_WR_USB_EP5IMR(imm, 0);
  MCF5272_WR_USB_EP6IMR(imm, 0);
  MCF5272_WR_USB_EP7IMR(imm, 0);

  /* Disable MCF5272 USB module */
  MCF5272_WR_USB_EP0CTL(imm, 0);

  /* Release interrupt handlers */
  free_irq(77, NULL);
  free_irq(78, NULL);
  free_irq(79, NULL);
  free_irq(80, NULL);
  free_irq(81, NULL);
  free_irq(82, NULL);
  free_irq(83, NULL);
  free_irq(84, NULL);

  /* Unregister the device */
  unreg_result = unregister_chrdev(MAJOR_NUM, DEVICE_NAME);

  /* If there's an error, report it */
  if (unreg_result < 0) {
    printk("Error in unregister_chrdev: %d\n", unreg_result);
    return;
    }
  printk("USB driver succesfully uninstalled\n");
}
#endif
