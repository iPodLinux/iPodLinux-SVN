#define CSGBA_ADDR      0xfffff100
#define CSGBB_ADDR      0xfffff102
#define CSGBC_ADDR      0xfffff104
#define CSGBD_ADDR      0xfffff106
#define CSA_ADDR        0xfffff110
#define CSB_ADDR        0xfffff112
#define CSC_ADDR        0xfffff114
#define CSD_ADDR        0xfffff116
#define DRAMMC_ADDR     0xfffffc00
#define DRAMC_ADDR      0xfffffc02
#define SCR_ADDR	0xfffff000
#define ICEMSR_ADDR	0xfffffd0e
#define WATCHDOG_ADDR   0xfffffb0a
#define PBSEL_ADDR      0xfffff40b              /* Port B Select Register */
#define PESEL_ADDR      0xfffff423              /* Port E Select Register */
#define PFSEL_ADDR      0xfffff42b              /* Port F Select Register */
#define PDSEL_ADDR      0xfffff41b              /* Port D Select Register */
#define PDDIR_ADDR      0xfffff418              /* Port D direction reg */
#define ICR_ADDR        0xfffff302
#define IVR_ADDR        0xfffff300
#define IMR_ADDR        0xfffff304
#define PD(x)           (1 << (x))
#define PD_INT0         0x01    /* Use INT0 as PD[0] */
#define PD_INT1         0x02    /* Use INT1 as PD[1] */
#define PD_INT2         0x04    /* Use INT2 as PD[2] */
#define PD_INT3         0x08    /* Use INT3 as PD[3] */
#define PD_IRQ1         0x10    /* Use IRQ1 as PD[4] */
#define PD_IRQ2         0x20    /* Use IRQ2 as PD[5] */
#define PD_IRQ3         0x40    /* Use IRQ3 as PD[6] */
#define PD_IRQ6         0x80    /* Use IRQ6 as PD[7] */
#define PUT_FIELD(field, val) (((val) << field##_SHIFT) & field##_MASK)
#define GET_FIELD(reg, field) (((reg) & field##_MASK) >> field##_SHIFT)
#define CSA_EN          0x0001          /* Chip-Select Enable */
#define CSA_SIZ_MASK    0x000e          /* Chip-Select Size */
#define CSA_SIZ_SHIFT   1
#define CSA_WS_MASK     0x0070          /* Wait State */
#define CSA_WS_SHIFT    4
#define CSA_BSW         0x0080          /* Data Bus Width */
#define CSA_FLASH       0x0100          /* FLASH Memory Support */
#define CSA_RO          0x8000          /* Read-Only */
#define SCR_WDTH8       0x01    /* 8-Bit Width Select */
#define SCR_DMAP        0x04    /* Double Map */
#define SCR_SO          0x08    /* Supervisor Only */
#define SCR_BETEN       0x10    /* Bus-Error Time-Out Enable */
#define SCR_PRV         0x20    /* Privilege Violation */
#define SCR_WPV         0x40    /* Write Protect Violation */
#define SCR_BETO        0x80    /* Bus-Error TimeOut */
#define PB_CSC0_RAS0    0x04    /* Use CSC0/RAS0 as PB[2] */    
#define PB_CSC1_RAS1    0x08    /* Use CSC1/RAS1 as PB[3] */    
#define PB_CSD0_CAS0    0x10    /* Use CSD0/CAS0 as PB[4] */    
#define PB_CSD1_CAS1    0x20    /* Use CSD1/CAS1 as PB[5] */
#define CSD_EN          0x0001          /* Chip-Select Enable */
#define CSD_SIZ_MASK    0x000e          /* Chip-Select Size */
#define CSD_SIZ_SHIFT   1
#define CSD_WS_MASK     0x0070          /* Wait State */
#define CSD_WS_SHIFT    4
#define CSD_BSW         0x0080          /* Data Bus Width */
#define CSD_FLASH       0x0100          /* FLASH Memory Support */
#define CSD_DRAM        0x0200          /* Dram Selection */
#define CSD_COMB        0x0400          /* Combining */
#define CSD_UPSIZ_MASK  0x1800          /* Unprotected memory block size */
#define CSD_UPSIZ_SHIFT 11
#define CSD_ROP         0x2000          /* Readonly if protected */
#define CSD_SOP         0x4000          /* Supervisor only if protected */
#define CSD_RO          0x8000          /* Read-Only */
#define PB_CSB0         0x01    /* Use CSB0      as PB[0] */
#define PE_DWE          0x08    /* Use DWE    as PE[3] */
#define PE_RXD          0x10    /* Use RXD    as PE[4] */
#define PE_TXD          0x20    /* Use TXD    as PE[5] */
#define PF_A20          0x08    /* Use A20       as PF[3] */
#define PF_A21          0x10    /* Use A21       as PF[4] */
#define PF_A22          0x20    /* Use A22       as PF[5] */
#define ICR_POL2        0x4000  /* Polarity Control for IRQ2 */
#define ICR_POL1        0x8000  /* Polarity Control for IRQ1 */
#define   DRAMMC_ROW12_PA23     0x8000
#define DRAMMC_ROW11            0x0800  /* Row address bit for MD11 PA20/PA22 */
#define DRAMMC_ROW10            0x0400  /* Row address bit for MD10 PA19/PA21 */
#define DRAMMC_ROW9             0x0200  /* Row address bit for MD9  PA9/PA19 */
#define DRAMMC_ROW8             0x0100  /* Row address bit for MD8  PA10/PA20 */
#define DRAMC_EN           0x8000       /* DRAM Controller enable */
#define DRAMC_EDO          0x0400       /* EDO DRAM */
#define   DRAMC_PGSZ_1024K 0x0200
#define DRAMC_MSW          0x0020       /* Slow Multiplexing */
#define DRAMC_BC_MASK      0x3000       /* Page Access Clock Cycle (FP mode) */
#define DRAMC_BC_SHIFT     12
#define DRAMC_DWE          0x0001       /* DRAM Write Enable */
#define DRAMC_RST          0x0002       /* Reset Burst Refresh Enable */
#define DRAMC_LPR          0x0004       /* Low-Power Refresh Enable */
#define USTCNT_TXEN     0x2000  /* Transmitter Enable */
#define USTCNT_RXEN     0x4000  /* Receiver Enable */
#define USTCNT_UEN      0x8000  /* UART Enable */
#define USTCNT_8_7      0x0100  /* Eight or seven-bit transmission */
#define USTCNT_ADDR     0xfffff900
#define PLLCR_ADDR      0xfffff200
#define PLLCR_SYSCLK_SEL_MASK  0x0700   /* System Clock Selection */
#define PLLCR_SYSCLK_SEL_SHIFT 8
#define PLLCR_LCDCLK_SEL_MASK  0x3800   /* LCD Clock Selection */
#define PLLCR_LCDCLK_SEL_SHIFT 11
#define UBAUD_PRESCALER_MASK    0x003f  /* Actual divisor is 65 - PRESCALER */
#define UBAUD_PRESCALER_SHIFT   0
#define UBAUD_DIVIDE_MASK       0x0700  /* Baud Rate freq. divizor */
#define UBAUD_DIVIDE_SHIFT      8
#define UBAUD_ADDR      0xfffff902
#define UTX_ADDR        0xfffff906
#define LCKCON_ADDR     0xfffffa27
#define UTX_TXDATA_ADDR 0xfffff907
#define UTX_TX_AVAIL     0x2000 /* Transmit FIFO has a slot available */
#define UTX_NOCTS        0x0800 /* Ignore CTS */


	.global _start
	.global _stext

	.global _rambase
	.global _ramvec
	.global _ramstart
	.global _ramend
	.global splash_bits

#include <linux/config.h>

	.data

/*
 *	Set up the usable of RAM stuff. Size of RAM is determined then
 *	an initial stack set up at the end.
 */

.align 4
_ramvec:
.long   0
_rambase:
.long   0
_ramstart:
.long   0
_ramend:
.long   0




splash_bits:
#include "bootlogo.rh"
	.text
_start:
_stext:
        /*
         * Program the basic registers. Only those registers that
         * are required at startup should be initialized with the
         * values required for startup. All other registers should
         * be initialized by related components.
         */
/*
 * Use dual mapping of the CPU register space and allow access to them
 * from the user mode (this is the default, anyway). Also enable bus
 * timeout watchdog.
 */
#define SCR_INIT (SCR_DMAP | SCR_BETEN)

/*
 * Disable the watchdog timer for now
 */
#define WATCHDOG_INIT 0

/*
 * Connect internal chip functions to those pins that are required for
 * startup. Device drivers will do the rest
 */

/*
 * Port B. We need:
 *       CSC[01]/RAS[01] and CSD[01]/CAS[01] for our 2 DRAM  chips
 *       CSB0 enabled for EXTRN access.
 */      
#define PBSEL_INIT (~(PB_CSC0_RAS0 | \
                      PB_CSD0_CAS0 | \
                      PB_CSD1_CAS1 | \
                      PB_CSB0))

/*
 * Port D. We need
 *      INT0	function
 *      INT1	function
 */
#define PDSEL_INIT (~(PD_IRQ1 | PD_IRQ2))
#define PDDIR_INIT (~(PD(12) | PD(13) | PD_IRQ1 | PD_IRQ2))
#define IRQ_INIT (ICR_POL1 | ICR_POL2)

/*
 * Port E. We need
 *	DWE    for DRAM Write Enable
 *      RXD    RS-232 receive
 *      TXD    RS-232 Transmit
 *      RTS    RS-232 RTS
 *      CTS    RS-232 CTS
 */
#define PESEL_INIT (~(PE_DWE | PE_RXD | PE_TXD ))

/*
 * Port F. We need
 *	A20..A22 to address the second 8 meg FLASH store
 */
#define PFSEL_INIT (~(PF_A20 | PF_A21 | PF_A22))

/*
 * In-circuit emulation should be disabled
 */

        moveb   #0x08, 0xfffffd0d           /* disable hardware compare map */
        moveb   #0x7, ICEMSR_ADDR           /* clear level 7 interrupt. */
        moveb   #SCR_INIT,      SCR_ADDR 
        movew   #WATCHDOG_INIT, WATCHDOG_ADDR
        moveb   #PBSEL_INIT,    PBSEL_ADDR
        moveb   #PESEL_INIT,    PESEL_ADDR
        moveb   #PFSEL_INIT,    PFSEL_ADDR

        moveb   #PDSEL_INIT,    PDSEL_ADDR	/* setup ethernet interrupts. */
        movew   #PDDIR_INIT,    PDDIR_ADDR
        movew   #IRQ_INIT,      ICR_ADDR

        moveb   #0x40,          IVR_ADDR    /* Use 0x40-0x47 for IRQ 0-7 */
        movel   #0x00FFFFFF,    IMR_ADDR    /* Mask all h/w interrupts */

        /*
         * Program Chip selects & DRAM controller
         *
         * CSA[0]            FLASH      10000000 -- 107FFFFF
         * CSB[0]            EXTRN      02000000 -- 0201FFFF
         * CAS[01]/RAS[01]   DRAM       00000000 -- 007FFFFF
         */

#define CSGBA_INIT (0x10000000 >> 13)      /* FLASH starts at 0x10000000 */
#define CSGBB_INIT (0x02000000 >> 13)      /* EXTRN starts at 0x02000000 */
#define CSGBD_INIT (0x00000000 >> 13)      /* DRAM  starts at 0x00000000 */

#define CSA_INIT \
        (CSA_FLASH              |       /* CSA controls FLASH memory */ \
         CSA_BSW                |       /* 16-bit bus Width */          \
/*       PUT_FIELD(CSA_WS, 2) */|       /* 2 Wait States */             \
         PUT_FIELD(CSA_SIZ, 5)  |       /* Total FLASH size is 4M */  \
         CSA_EN)                        /* Enable it */

#define CSB_INIT \
         (CSA_BSW               |       /* 16-bit bus Width */          \
         CSA_EN)                        /* Enable it */

#define CSD_INIT \
        (CSD_DRAM               |       /* CSD Controls DRAM */         \
         CSD_COMB               |       /* RAS0 operates entire RAM. */ \
         CSD_BSW                |       /* 16-bit bus width */          \
/*       PUT_FIELD(CSD_WS, 1) */|       /* 1 Wait State */              \
         PUT_FIELD(CSD_SIZ, 7)  |       /* The board has 4MB DRAM */   \
         CSD_EN)                        /* Enable it */

#define DRAMMC_INIT \
        (DRAMMC_ROW12_PA23      |       /* PA23 -> MD12 */      \
         DRAMMC_ROW11           |       /* PA22 -> MD11 */      \
         DRAMMC_ROW10           |       /* PA21 -> MD10 */      \
         DRAMMC_ROW9            |       /* PA19 -> MD9  */      \
         DRAMMC_ROW8)                   /* PA20 -> MD8  */      \
 
#define DRAMC_INIT \
        (DRAMC_EN               |       /* Enable DRAM controller */    \
         DRAMC_EDO              |       /* We have EDO DRAM */          \
         DRAMC_PGSZ_1024K       |       /* 1k page. */                  \
         DRAMC_MSW              |       /* Slow address MUXing. */      \
/*       PUT_FIELD(DRAMC_WS, 1) */|     /* 1 Wait State */              \
         PUT_FIELD(DRAMC_BC, 1) |       /* 1 Page Access Clock Cycle.*/ \
         DRAMC_LPR              |       /* Low-Power Refresh Enable */  \
         DRAMC_RST              |       /* Reset Burst Refresh Enable */\
         DRAMC_DWE)                     /* DRAM Write Enable */

        movew   #CSGBA_INIT,  CSGBA_ADDR        /* FLASH */
        movew   #CSA_INIT,    CSA_ADDR

        movew   #CSGBB_INIT,  CSGBB_ADDR        /* EXTRN */
        movew   #CSB_INIT,    CSB_ADDR

        movew   #DRAMMC_INIT, DRAMMC_ADDR       /* DRAM */
        movew   #DRAMC_INIT,  DRAMC_ADDR
        movew   #CSGBD_INIT,  CSGBD_ADDR
        movew   #CSD_INIT,    CSD_ADDR

        /*
         * Program the UART (115200 cs8 -parenb -cstopb -crtscts */
         */
#define PLLCR_INIT \
        (PUT_FIELD(PLLCR_SYSCLK_SEL, 4) | PUT_FIELD(PLLCR_LCDCLK_SEL, 4))
 
#define USTCNT_INIT \
        (USTCNT_UEN | USTCNT_RXEN | USTCNT_TXEN | USTCNT_8_7)
 
#define UBAUD_INIT \
        (PUT_FIELD(UBAUD_DIVIDE, 0) | PUT_FIELD(UBAUD_PRESCALER, 0x38))
 
#define UTX_INIT \
        (UTX_NOCTS)
 
        movew   #(USTCNT_UEN | USTCNT_RXEN), USTCNT_ADDR
        tstw    USTCNT_ADDR
        movew   #USTCNT_INIT, USTCNT_ADDR
        movew   #PLLCR_INIT,  PLLCR_ADDR
        movew   #UBAUD_INIT,  UBAUD_ADDR
        moveb   #(UTX_INIT>>8),    UTX_ADDR                                     

	movew	#0x2700, %sr

	moveb   #0,    LCKCON_ADDR		/* LCD is off */
#if CONFIG_EZSIMM_LCD && CONFIG_EZSIMM_EG9013
	/* this is for the EG9013 display. */
	movel	#splash_bits, LSSA_ADDR 	/* LCD starting address. */
	moveb	#0x28,   LVPW_ADDR 		/* LCD Virtual Page Width Register. */
	movew	#0x280,  LXMAX_ADDR 		/* LCD Screen Width Register. */
	movew	#0x1df,  LYMAX_ADDR 		/* LCD Screen Height Register. */
	moveb	#0,      LRRA_ADDR		/* LCD refresh rate. */
	moveb	#0,      LPXCD_ADDR		/* LCD Pixel Clock Divider Register. */
	moveb	#0x08,   LPICF_ADDR		/* LCD Panel Interface Config Register. */
	moveb	#0x01,   LPOLCF_ADDR		/* LCD Polarity Configuration Register. */
	moveb	#0x81,   LCKCON_ADDR		/* LCD Clocking Control Register. */
	movew	#0xff00, PCPDEN_ADDR		/* LCD pins Port C Pull-Down enb. reg */
#endif

#if CONFIG_EZSIMM_LCD && CONFIG_EZSIMM_LM32019
	/* this is for the LM32019 display. */
	movel	#splash_bits, LSSA_ADDR 	/* LCD starting address. */
	moveb	#0x14,   LVPW_ADDR 		/* LCD Virtual Page Width Register. */
	movew	#0x140,  LXMAX_ADDR 		/* LCD Screen Width Register. */
	movew	#0xef,   LYMAX_ADDR 		/* LCD Screen Height Register. */
	moveb	#0,      LRRA_ADDR		/* LCD refresh rate. */
	moveb	#1,      LPXCD_ADDR		/* LCD Pixel Clock Divider Register. */
	moveb	#0x08,   LPICF_ADDR		/* LCD Panel Interface Config Register. */
	moveb	#0x00,   LPOLCF_ADDR		/* LCD Polarity Configuration Register. */
	moveb	#0x81,   LCKCON_ADDR		/* LCD Clocking Control Register. */
	movew	#0xff00, PCPDEN_ADDR		/* LCD pins Port C Pull-Down enb. reg */
#endif

	/*
	 * Here we start doing some real things
	 */
	moveq	#13, %d7		/* '\r' */
	jsr putc

	moveq	#10, %d7		/* '\n' */
	jsr putc

	moveq	#65, %d7		/* 'A' */
	jsr putc

	movew	#16384, %d0  /* PLL settle wait loop */
L0:
	subw	#1, %d0
	bne	L0

	moveq	#66, %d7		/* 'B' */
	jsr	putc

	/* Copy data segment from ROM to RAM */
	moveal	#__data_rom_start, %a0
	moveal	#_sdata, %a1
	moveal	#_edata, %a2
	moveq	#67, %d7		/* 'C' */
	jsr	putc

	/* Copy %a0 to %a1 until %a1 == %a2 */
LD1:
	movel	%a0@+, %d0
	movel	%d0, %a1@+
	cmpal	%a1, %a2
	bhi	LD1

	moveq	#68, %d7		/* 'D' */
	jsr	putc

	moveal	#_sbss,	%a0
	moveal	#_ebss, %a1
	/* Copy 0 to %a0 until %a0 == %a1 */

_clear_bss:
	movel	#0, %a0@+
	cmpal	%a0, %a1
	bhi	_clear_bss

	movel	#_sdata, %d0
	movel	%d0, _rambase
	movel	#_ebss,  %d0
	movel	%d0, _ramstart
	movel   #__ramend, %d0

	movel	%d0, _ramend
	movel	#__ramvec, %d0
	movel	%d0, _ramvec

	moveq	#69, %d7		/* 'E' */
	jsr	putc

	/*
	 * load the current task pointer and stack
	 */

	 lea	init_task_union,%a0
	 movel	%a0, _current_task
	 lea	0x2000(%a0),%sp

	 moveq	#70, %d7		/* 'F' */
	 jsr	putc

	 moveq	#13, %d7		/* '\r' */
	 jsr	putc

	 moveq	#10, %d7		/* '\n' */
	 jsr	putc

lp:
	jsr	start_kernel
	jmp	lp
_exit:

	jmp	_exit


putc:
	moveb	%d7,UTX_TXDATA_ADDR
pclp:
	movew	UTX_ADDR, %d7
	andw	#UTX_TX_AVAIL, %d7
	beq	pclp
	rts

	.data
env:
	.long	0
	.text
