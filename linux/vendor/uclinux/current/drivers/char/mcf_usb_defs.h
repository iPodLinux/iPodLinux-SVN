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

/*
 * File:		usb_defs.h
 * Purpose:		USB Header File
 */

#ifndef USB_DEFS_H
#define USB_DEFS_H

#include "mcf5272.h"
#include "mcf_usb.h"

/* Depth of the FIFOs in packet lengths (Must be at */
/* least 2 for non-isochronous endpoints) */
#define FIFO_DEPTH		4
#define ISOCHRONOUS_FIFO_LENGTH	256
#define THRESHOLD_FIFO_FULLNESS	32	/* At least 32 bytes must be in FIFO when FIFO_LVL is reached */
#define MAX_IO_REQUESTS		16

/* Definitions for internal isochronous transfer status */
#define DEFAULT			0
#define MISS_PACKET		1
#define SKIP_FIRST_PACKET	2

 /********************************************************************
 *	Interrupt Controller Definitions
 ********************************************************************/

#define USB_EP0_LEVEL		6
#define USB_EP1_LEVEL		5
#define USB_EP2_LEVEL		5
#define USB_EP3_LEVEL		5
#define USB_EP4_LEVEL		5
#define USB_EP5_LEVEL		5
#define USB_EP6_LEVEL		5
#define USB_EP7_LEVEL		5

/* Structure for storing IN and OUT transfer data */
typedef struct {
	uint8 *start;				/* Starting address for buffer */
	uint32 position;			/* Offset pointer within buffer */
	uint32 length;				/* Length of the buffer in bytes */
} USB_BUFFER;

/* IO Request to Driver */
typedef struct {
	uint8 *start;				/* Stores start of data buffer */
	uint32 length;				/* Stores length of data buffer */
} IO_REQUEST;

/* USB Endpoint State Info */
typedef struct {
	uint32 fifo_length;			/* Length of FIFO */
	uint32 in_fifo_start;			/* Starting address of IN-FIFO */
	uint32 out_fifo_start;			/* Starting address of OUT-FIFO */
	uint16 packet_size;			/* Maximum Packet Size */
	uint8 ttype;				/* Transfer Type */
	uint8 state;				/* State of endpoint */
	uint8 dir;				/* Direction of transfer */
	uint8 sendZLP;				/* send zero-length packet (if required) */
	uint32 bytes_transferred;		/* The amount of successfully transferred bytes */
	USB_BUFFER buffer;			/* Data Buffer for IN and OUT transfers */
} USB_EP_STATE;

/* USB Isochronous Endpoint State Info */
typedef struct {
	uint32 frame_counter;			/* Number of frames to be read or written */
	uint32 packet_position;			/* Indicates current position within a packet */
	uint16 start_frame_number;		/* Number of frame after which data stream will be monitored */
	uint16 final_frame_number;		/* Number of frame after which data stream monitoring must be stopped */
	uint16 state;				/* State of last transfer */
	uint8 transfer_monitoring;
	uint8 sent_packet_watch;
	uint32* packet_length;			/* Pointer to the buffer header */
	uint32 first_io_request;		/* Index of first request in a queue */
	uint32 last_io_request;			/* Index of last request in a queue */
	uint32 number_of_requests;		/* The number of requests in a queue */
	IO_REQUEST requests_queue[MAX_IO_REQUESTS];
} USB_ISO_EP_STATE;
/********************************************************************/
/* USB driver's functions definition */

/* Initialize USB driver */
void usb_init(DESC_INFO * descriptor_info);
/* Initialize interrupts */
void usb_isr_init(void);
/* Endpoint ISRs */
void usb_endpoint0_isr(int irq, void *dev_id, struct pt_regs *regs);
void usb_endpoint_isr(int irq, void *dev_id, struct pt_regs *regs);
/* EOP, EOT, FIFO_LVL interrupt handlers */
void usb_in_service(uint32, uint32);
void usb_out_service(uint32, uint32);
/* Initialize FIFO buffer */
void usb_fifo_init(void);
/* Initiate reading/writing through endpoint */
uint32 usb_tx(uint32, uint8 *, uint32);
uint32 usb_rx_data(uint32, uint8 *, uint32);
/* SOF interrupt handler. Is used for monitoring isochronous transfer */
void usb_isochronous_transfer_service(void);
/* Checks if endpoint is busy */
int usb_ep_is_busy(uint32);
/* Waits until endpoint becomes free */
int usb_ep_wait(uint32, uint32);
/* Returns address of required descriptor */
uint8* usb_get_desc(int8, int8, int8, int8);
/* Returns last command to client */
uint32 usb_fetch_command(DEVICE_COMMAND *);
/* Vendor specific requests service routine */
void usb_vendreq_service(uint8,uint8, uint16, uint16, uint16);
/* Handles DEV_CFG interrupt */
void usb_devcfg_service(void);
/* Handles RESUME, SUSPEND and RESET interrupts */
void usb_bus_state_chg_service(uint32);
/* Sets EP0CTL[CMD_OVER], EP0CTL[CMD_ERR] bits to appropriate values */
void usb_vendreq_done(uint32);
/* Extracts request from the queue */
uint32 usb_get_request(uint32 epnum);

void usb_ep_unhalt(uint32 epnum);
void usb_ep_halt(uint32 epnum);
/********************************************************************/

#endif /* USB_DEFS_H */
