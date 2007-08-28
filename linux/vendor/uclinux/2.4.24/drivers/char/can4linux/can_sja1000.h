/*
 * can_sja1000.h - can4linux CAN driver module
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2001 port GmbH Halle/Saale
 *------------------------------------------------------------------
 * $Header: /var/cvs/uClinux-2.4.x/drivers/char/can4linux/can_sja1000.h,v 1.2 2003/08/28 00:38:31 gerg Exp $
 *
 *--------------------------------------------------------------------------
 *
 *
 * modification history
 * --------------------
 * $Log: can_sja1000.h,v $
 * Revision 1.2  2003/08/28 00:38:31  gerg
 * I hope my patch doesn't come to late for the next uClinux distribution.
 * The new patch is against the latest CVS uClinux-2.4.x/drivers/char. The
 * FlexCAN driver is working but still needs some work. Phil Wilshire is
 * supporting me and we expect to have a complete driver in some weeks.
 *
 * commit text: added support for ColdFire FlexCAN
 *
 * Patch submitted by Heinz-Juergen Oertel <oe@port.de>.
 *
 * Revision 1.1  2003/07/18 00:11:46  gerg
 * I followed as much rules as possible (I hope) and generated a patch for the
 * uClinux distribution. It contains an additional driver, the CAN driver, first
 * for an SJA1000 CAN controller:
 *   uClinux-dist/linux-2.4.x/drivers/char/can4linux
 * In the "user" section two entries
 *   uClinux-dist/user/can4linux     some very simple test examples
 *   uClinux-dist/user/horch         more sophisticated CAN analyzer example
 *
 * Patch submitted by Heinz-Juergen Oertel <oe@port.de>.
 *
 *
 *
 *
 *--------------------------------------------------------------------------
 */


/* typedef unsigned char uint8; */

extern u8 CanTiming[10][2];

/*----------*/

#define CAN_PELICANMODE
#ifdef  CAN_PELICANMODE


#ifdef CAN4LINUX_PCI

/* Definition with offset 4, EMS CPC-PCI */
union frame {
	struct {
	    u8	 canid1;
	    u8	 			dummy01;
	    u8	 			dummy02;
	    u8	 			dummy03;
	    u8	 canid2;
	    u8	 			dummy04;
	    u8	 			dummy05;
	    u8	 			dummy06;
	    u8	 canid3;
	    u8	 			dummy07;
	    u8	 			dummy08;
	    u8	 			dummy09;
	    u8	 canid4;
	    u8	 			dummy10;
	    u8	 			dummy11;
	    u8	 			dummy12;
	    /* !! use offset 4 !! */
	    u8    canxdata[8*4];

	} extframe;
	struct {
	    u8	 canid1;
	    u8	 			dummy01;
	    u8	 			dummy02;
	    u8	 			dummy03;
	    u8	 canid2;
	    u8	 			dummy04;
	    u8	 			dummy05;
	    u8	 			dummy06;
	    /* !! use offset 4 !! */
	    u8    candata[8*4];
	} stdframe;
};

typedef struct canregs {
	u8    canmode;		/* 0 */
	u8	 			dummy01;
	u8	 			dummy02;
	u8	 			dummy03;
	u8    cancmd;
	u8	 			dummy04;
	u8	 			dummy05;
	u8	 			dummy06;
	u8    canstat;
	u8	 			dummy07;
	u8	 			dummy08;
	u8	 			dummy09;
	u8	 canirq;
	u8	 			dummy10;
	u8	 			dummy11;
	u8	 			dummy12;
	u8	 canirq_enable;
	u8	 			dummy13;
	u8	 			dummy14;
	u8	 			dummy15;
	u8	 reserved1;		/* 5 */
	u8	 			dummy16;
	u8	 			dummy17;
	u8	 			dummy18;
	u8	 cantim0;
	u8	 			dummy19;
	u8	 			dummy20;
	u8	 			dummy21;
	u8	 cantim1;
	u8	 			dummy22;
	u8	 			dummy23;
	u8	 			dummy24;
	u8	 canoutc;
	u8	 			dummy25;
	u8	 			dummy26;
	u8	 			dummy27;
	u8	 cantest;
	u8	 			dummy28;
	u8	 			dummy29;
	u8	 			dummy30;
	u8	 reserved2;		/* 10 */
	u8	 			dummy31;
	u8	 			dummy32;
	u8	 			dummy33;
	u8	 arbitrationlost;	/* read only */
	u8	 			dummy34;
	u8	 			dummy35;
	u8	 			dummy36;
	u8	 errorcode;		/* read only */
	u8	 			dummy37;
	u8	 			dummy38;
	u8	 			dummy39;
	u8	 errorwarninglimit;
	u8	 			dummy40;
	u8	 			dummy41;
	u8	 			dummy42;
	u8	 rxerror;
	u8	 			dummy43;
	u8	 			dummy44;
	u8	 			dummy45;
	u8	 txerror;		/* 15 */
	u8	 			dummy46;
	u8	 			dummy47;
	u8	 			dummy48;
	u8	 frameinfo;
	u8	 			dummy49;
	u8	 			dummy50;
	u8	 			dummy51;
	union    frame frame;
	u8	 reserved3;
	u8	 			dummy52;
	u8	 			dummy53;
	u8	 			dummy54;
	u8	 canrxbufferadr		/* 30 */;
	u8	 			dummy55;
	u8	 			dummy56;
	u8	 			dummy57;
	u8	 canclk; 	 
	u8	 			dummy58;
	u8	 			dummy59;
	u8	 			dummy60;
} canregs_t;

#else

/* Standard definition with offset 1 */
union frame {
	struct {
	    u8	 canid1;
	    u8	 canid2;
	    u8	 canid3;
	    u8	 canid4;
	    u8    canxdata[8];
	} extframe;
	struct {
	    u8	 canid1;
	    u8	 canid2;
	    u8    candata[8];
	} stdframe;
};

typedef struct canregs {
	u8    canmode;		/* 0 */
	u8    cancmd;
	u8    canstat;
	u8	 canirq;
	u8	 canirq_enable;
	u8	 reserved1;		/* 5 */
	u8	 cantim0;
	u8	 cantim1;
	u8	 canoutc;
	u8	 cantest;
	u8	 reserved2;		/* 10 */
	u8	 arbitrationlost;	/* read only */
	u8	 errorcode;		/* read only */
	u8	 errorwarninglimit;
	u8	 rxerror;
	u8	 txerror;		/* 15 */
	u8	 frameinfo;
	union    frame frame;
	u8	 reserved3;
	u8	 canrxbufferadr		/* 30 */;
	u8	 canclk; 	 
} canregs_t;
#endif

#else
typedef struct canregs {
	u8    canctl;
	u8    cancmd;
	u8    canstat;
	u8	 canirq;
	u8	 canacc;
	u8	 canmask;
	u8	 cantim0;
	u8	 cantim1;
	u8	 canoutc;
	u8	 cantest;
	u8	 cantxdes1;
	u8	 cantxdes2;
	u8    cantxdata[8];
	u8	 canrxdes1;
	u8    canrxdes2;
	u8    canrxdata[8];
	u8    unused;
	u8	 canclk; 	 
} canregs_t;
#endif

#define CAN_RANGE 0x20

#ifdef CAN_PELICANMODE
/*--- Mode Register -------- PeliCAN -------------------*/

#  define CAN_SLEEP_MODE		0x10    /* Sleep Mode */
#  define CAN_ACC_FILT_MASK		0x08    /* Acceptance Filter Mask */
#  define CAN_SELF_TEST_MODE		0x04    /* Self test mode */
#  define CAN_LISTEN_ONLY_MODE		0x02    /* Listen only mode */
#  define CAN_RESET_REQUEST		0x01	/* reset mode */
#  define CAN_MODE_DEF CAN_ACC_FILT_MASK	 /* Default ModeRegister Value*/

   /* bit numbers of mode register */
#  define CAN_SLEEP_MODE_BIT		4	/* Sleep Mode */
#  define CAN_ACC_FILT_MASK_BIT		3	/* Acceptance Filter Mask */
#  define CAN_SELF_TEST_MODE_BIT	2	/* Self test mode */
#  define CAN_LISTEN_ONLY_MODE_BIT	1	/* Listen only mode */
#  define CAN_RESET_REQUEST_BIT		0	/* reset mode */


/*--- Interrupt enable Reg -----------------------------*/
#define CAN_ERROR_BUSOFF_INT_ENABLE		(1<<7)
#define CAN_ARBITR_LOST_INT_ENABLE		(1<<6)
#define CAN_ERROR_PASSIVE_INT_ENABLE		(1<<5)
#define CAN_WAKEUP_INT_ENABLE			(1<<4)
#define CAN_OVERRUN_INT_ENABLE			(1<<3)
#define CAN_ERROR_INT_ENABLE			(1<<2)
#define CAN_TRANSMIT_INT_ENABLE			(1<<1)
#define CAN_RECEIVE_INT_ENABLE			(1<<0)

/*--- Frame information register -----------------------*/
#define CAN_EFF				0x80	/* extended frame */
#define CAN_SFF				0x00	/* standard fame format */


#else
/*--- Control Register ----- Basic CAN -----------------*/
 
#define CAN_TEST_MODE				(1<<7)
#define CAN_SPEED_MODE				(1<<6)
#define CAN_OVERRUN_INT_ENABLE			(1<<4)
#define CAN_ERROR_INT_ENABLE			(1<<3)
#define CAN_TRANSMIT_INT_ENABLE			(1<<2)
#define CAN_RECEIVE_INT_ENABLE			(1<<1)
#define CAN_RESET_REQUEST			(1<<0)
#endif

/*--- Command Register ------------------------------------*/
 
#define CAN_GOTO_SLEEP				(1<<4)
#define CAN_CLEAR_OVERRUN_STATUS		(1<<3)
#define CAN_RELEASE_RECEIVE_BUFFER		(1<<2)
#define CAN_ABORT_TRANSMISSION			(1<<1)
#define CAN_TRANSMISSION_REQUEST		(1<<0)

/*--- Status Register --------------------------------*/
 
#define CAN_BUS_STATUS 				(1<<7)
#define CAN_ERROR_STATUS			(1<<6)
#define CAN_TRANSMIT_STATUS			(1<<5)
#define CAN_RECEIVE_STATUS			(1<<4)
#define CAN_TRANSMISSION_COMPLETE_STATUS	(1<<3)
#define CAN_TRANSMIT_BUFFER_ACCESS		(1<<2)
#define CAN_DATA_OVERRUN			(1<<1)
#define CAN_RECEIVE_BUFFER_STATUS		(1<<0)

/*--- Status Register --------------------------------*/
 
#define CAN_BUS_STATUS_BIT 			(1<<7)
#define CAN_ERROR_STATUS_BIT			(1<<6)
#define CAN_TRANSMIT_STATUS_BIT			(1<<5)
#define CAN_RECEIVE_STATUS_BIT			(1<<4)
#define CAN_TRANSMISSION_COMPLETE_STATUS_BIT	(1<<3)
#define CAN_TRANSMIT_BUFFER_ACCESS_BIT		(1<<2)
#define CAN_DATA_OVERRUN_BIT			(1<<1)
#define CAN_RECEIVE_BUFFER_STATUS_BIT		(1<<0)

/*--- Interrupt Register -----------------------------------*/
 
#define CAN_WAKEUP_INT				(1<<4)
#define CAN_OVERRUN_INT				(1<<3)
#define CAN_ERROR_INT				(1<<2)
#define CAN_TRANSMIT_INT			(1<<1)
#define CAN_RECEIVE_INT 			(1<<0)

/*--- Output Control Register -----------------------------------------*/
/*
 *	7	6	5	4	3	2	1	0
 * 	OCTP1	OCTN1	OCPOL1	OCTP0	OCTN0	OCPOL0	OCMODE1	OCMODE0
 *	----------------------  ----------------------  ---------------
 *	    TX1 Output		    TX0 Output		  programmable
 *	  Driver Control	  Driver Control	  output functions
 *
 *	MODE
 *	OCMODE1	OCMODE0
 *	  0	  1	Normal Mode; TX0, TX1 bit sequenze TXData
 *	  1	  1	Normal Mode; TX0 bit sequenze, TX1 busclock TXCLK
 *	  0	  0	Biphase Mode
 *	  1	  0	Test Mode; TX0 bit sequenze, TX1 COMPOUT
 *
 *	In normal Mode Voltage Output Levels depend on 
 *	Driver Characteristic: OCTPx, OCTNx
 *	and programmed Output Polarity: OCPOLx
 *
 *	Driver Characteristic
 *	OCTPx	OCTNx
 *	  0	 0	always Floating Outputs,
 *	  0	 1	Pull Down
 *	  1	 0	Pull Up
 *	  1	 1	Push Pull
 */
 
/*--- Output control register --------------------------------*/

#define CAN_OCTP1			(1<<7)
#define CAN_OCTN1			(1<<6)
#define CAN_OCPOL1			(1<<5)
#define CAN_OCTP0			(1<<4)
#define CAN_OCTN0			(1<<3)
#define CAN_OCPOL0			(1<<2)
#define CAN_OCMODE1			(1<<1)
#define CAN_OCMODE0			(1<<0)

/*--- Clock Divider register ---------------------------------*/

#define CAN_MODE_BASICCAN		(0x00)
#define CAN_MODE_PELICAN		(0xC0)
#define CAN_MODE_CLK			(0x07)		/* CLK-out = Fclk   */
#define CAN_MODE_CLK2			(0x00)		/* CLK-out = Fclk/2 */


/*--- Remote Request ---------------------------------*/
/*    Notes:
 *    Basic CAN: RTR is Bit 4 in TXDES1.
 *    Peli  CAN: RTR is Bit 6 in frameinfo.
 */
#ifdef CAN_PELICANMODE
# define CAN_RTR				(1<<6)
#else
# define CAN_RTR				(1<<4)
#endif


/* the base address register array */
extern unsigned int Base[];
/*---------- Timing values */

/* the timings are valid for clock 8Mhz */
#define CAN_TIM0_10K		  49
#define CAN_TIM1_10K		0x1c
#define CAN_TIM0_20K		  24	
#define CAN_TIM1_20K		0x1c
#define CAN_TIM0_40K		0x89	/* Old Bit Timing Standard of port */
#define CAN_TIM1_40K		0xEB	/* Old Bit Timing Standard of port */
#define CAN_TIM0_50K		   9
#define CAN_TIM1_50K		0x1c
#define CAN_TIM0_100K              4    /* sp 87%, 16 abtastungen, sjw 1 */
#define CAN_TIM1_100K           0x1c
#define CAN_TIM0_125K		   3
#define CAN_TIM1_125K		0x1c
#define CAN_TIM0_250K		   1
#define CAN_TIM1_250K		0x1c
#define CAN_TIM0_500K		   0
#define CAN_TIM1_500K		0x1c
#define CAN_TIM0_800K		   0
#define CAN_TIM1_800K		0x16
#define CAN_TIM0_1000K		   0
#define CAN_TIM1_1000K		0x14



