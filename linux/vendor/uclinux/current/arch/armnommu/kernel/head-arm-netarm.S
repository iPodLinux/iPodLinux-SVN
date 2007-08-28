/*
 * =================== STARTUP CODE, Kernel 2.4.x =====================
 *
 * linux/arch/armnommu/kernel/head-arm-netarm.S
 *
 * Copyright (C) 2000, 2001 NETsilicon, Inc.
 * Copyright (C) 2000, 2001 Red Hat, Inc.
 *
 * This software is copyrighted by Red Hat. LICENSEE agrees that
 * it will not delete this copyright notice, trademarks or protective
 * notices from any copy made by LICENSEE.
 *
 * This software is provided "AS-IS" and any express or implied 
 * warranties or conditions, including but not limited to any
 * implied warranties of merchantability and fitness for a particular
 * purpose regarding this software. In no event shall Red Hat
 * be liable for any indirect, consequential, or incidental damages,
 * loss of profits or revenue, loss of use or data, or interruption
 * of business, whether the alleged damages are labeled in contract,
 * tort, or indemnity.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * author(s) : Joe deBlaquiere
 *
 * Adapted for NS7520 (and some legacy code removed) by Art Shipkowski
 *
 * NS7520 improved code RAM copy from FS Forth-Systeme GmbH's fork.
 */

#include <linux/config.h>
#include <linux/linkage.h>
#include <asm/mach-types.h>
#include <asm/arch/netarm_registers.h>
#include <asm/arch/netarm_mmap.h>

#define	NETARM_INIT_CPSR	(CPSR_I_BIT | CPSR_F_BIT | CPSR_SVC_MODE)

/*
 * Define which output pin to use for NET+ARM development board blink routine
 */
#define NA_YELLOW_LED 0x04
#define NA_GREEN_LED  0x02

#ifdef CONFIG_NETARM_EMLIN
/* The (one) green LED is on bit 6 (0x40) */
#define NA_BLINK_LED 0x40
#define NA_FAIL_LED  0x40
#else
/* Use green and yellow LEDs */
#define NA_BLINK_LED NA_YELLOW_LED
#define NA_FAIL_LED NA_GREEN_LED
#endif

/*
 * This define is used to determine the number of wait states needed
 * for the 90 ns flash used on the NET+ARM reference board.  The macro
 * is only valid for 90 ns flash.  To get this value, the following
 * calculation was used:
 *
 *   ceil(90 / ((1 / FSYSCLK) * 1000)) - 1
 *
 * This define gives the same value as the above calculation, but based
 * on the PLLCNT (instead of on the FSYSCLK).
*/
#define FLASH_90ns_WAIT_STATES	((NETARM_PLL_COUNT_VAL + 2) / 3)

		.text
		.align	12

/*
 * Reset condition : 
 *	R14_svc 	= unknown
 *	SPSR_svc 	= CPSR
 *	CPSR		= 0b1101_0011 ; not IRQ, no FIQ, SVC mode
 *	PC		= 0x0
 */

		.section ".text.init",#alloc,#execinstr
		.type	stext, #function
ENTRY(stext)
  b _reset /* Reset Vector */
  b _undef /* Undefined Vector */
  b _swi		/* Software Interrupt */
  b _abortprefetch /* Abort (Prefetch) */
  b _abortdata		  /* Abort (Data) */
  b _reserved				/* Reserved vector */
  b _irq						  /* IRQ vector */
  b _firq						 /* Fast IRQ vector */

.global _undef
_undef:
	mov	r0, #'#'
	bl	_printch
	mov	r0, #'u'
	bl	_printch
	mov	r0, lr		
  bl  _printhex8
1:
  b 1b

.global _swi
_swi:
	mov	r0, #'#'
	bl	_printch
	mov	r0, #'s'
	bl	_printch
	mov	r0, lr		
  bl  _printhex8
1:
  b 1b

.global _abortprefetch
_abortprefetch:
	mov	r0, #'#'
	bl	_printch
	mov	r0, #'p'
	bl	_printch
	mov	r0, lr		
  bl  _printhex8
1:
  b 1b


.global _abortdata
_abortdata:
	mov	r4, r14
  mov r5, sp
	mov	r0, #'\n'
	bl	_printch
	mov	r0, #'#'
	bl	_printch
	mov	r0, #'d'
	bl	_printch
	mov	r0, r4
  bl  _printhex8
	mov	r0, #'\n'
	bl	_printch
	mov	r0, r5
  bl  _printhex8
1:
  b 1b

.global _reserved
_reserved:
	mov	r0, #'#'
	bl	_printch
	mov	r0, #'r'
	bl	_printch
	mov	r0, lr		
  bl  _printhex8
1:
  b 1b


.global _irq
_irq:
	mov	r0, #'#'
	bl	_printch
	mov	r0, #'i'
	bl	_printch
	mov	r0, lr		
  bl  _printhex8
1:
  b 1b


.global _firq
_firq:
	mov	r0, #'#'
	bl	_printch
	mov	r0, #'f'
	bl	_printch
	mov	r0, lr		
  bl  _printhex8
1:
  b 1b



.global _reset
_reset:
				 		mov	r0, #NETARM_INIT_CPSR
		msr	cpsr, r0
#if CONFIG_NETARM_BOOTLOADER_DEBUG_DEBUGGER
/* delay (0x10000) to allow developer to attach debugger before boot */
		mov	r1, #0x10000
1:
		subs	r1, r1, #1
		bge	1b	
#endif 
/*
 * prior to software reset : need to set pin PORTC4 to be *HRESET
 */

		ldr	r4, =NETARM_GEN_MODULE_BASE

		ldr	r1, =( NETARM_GEN_PORT_MODE(0x10) | \
		               NETARM_GEN_PORT_DIR(0x10) )
		str	r1, [r4, #+NETARM_GEN_PORTC]		

/*
 * software reset : see HW Ref. Guide 8.2.4 : Software Service Register
 *                  for an explanation of this process
 */

		ldr	r4, =NETARM_GEN_MODULE_BASE

		ldr	r1, =NETARM_GEN_SW_SVC_RESETA
		str	r1, [r4, #+NETARM_GEN_SOFTWARE_SERVICE]
		
		ldr	r1, =NETARM_GEN_SW_SVC_RESETB
		str	r1, [r4, #+NETARM_GEN_SOFTWARE_SERVICE]
		
		ldr	r1, =NETARM_GEN_SW_SVC_RESETA
		str	r1, [r4, #+NETARM_GEN_SOFTWARE_SERVICE]
		
		ldr	r1, =NETARM_GEN_SW_SVC_RESETB
		str	r1, [r4, #+NETARM_GEN_SOFTWARE_SERVICE]

/*
 * set up PLL and System Config
 */

@ r4 already set
		ldr	r4, =NETARM_GEN_MODULE_BASE
#ifdef	CONFIG_NETARM_NET40_REV2
		ldr	r1, =( NETARM_GEN_SYS_CFG_LENDIAN |	\
			       NETARM_GEN_SYS_CFG_BUSFULL |	\
			       NETARM_GEN_SYS_CFG_USER_EN |	\
			       NETARM_GEN_SYS_CFG_ALIGN_ABORT | \
		               NETARM_GEN_SYS_CFG_BUSARB_INT )
#else
		ldr	r1, [r4, #+NETARM_GEN_SYSTEM_CONTROL]
		ldr	r2, =( NETARM_GEN_SYS_CFG_LENDIAN |	\
			       NETARM_GEN_SYS_CFG_BUSFULL |	\
			       NETARM_GEN_SYS_CFG_USER_EN |	\
		               NETARM_GEN_SYS_CFG_ALIGN_ABORT |	\
			       NETARM_GEN_SYS_CFG_BUSARB_INT |	\
							 NETARM_GEN_SYS_CFG_BUSMON_EN )
		orr	r1, r1, r2
#endif
		str	r1, [r4, #+NETARM_GEN_SYSTEM_CONTROL]

#ifndef CONFIG_NETARM_PLL_BYPASS		 
		ldr	r1, =NETARM_GEN_PLL_CTL_PLLCNT(NETARM_PLL_COUNT_VAL)
		str	r1, [r4, #+NETARM_GEN_PLL_CONTROL ]
#endif

/*
 * initialize both serial ports
 */
		
		bl	_netarm_debug_serial_init

		mov	r0, #'A'
		bl	_printch
		
/*
 *	if CS0 is already mapped high, assume it's mapped correctly
 */

		ldr	r4, =NETARM_MEM_MODULE_BASE
		ldr	r1, [r4, #NETARM_MEM_CS0_BASE_ADDR]
		ldr	r2, =NETARM_MEM_BAR_BASE_MASK
		and	r1, r1, r2
		cmp	r1, #0
		bne	_exec_in_RAM
		
		mov	r0, #'B'
		bl	_printch

/*		b	_netarm_led_FAIL2 */

/*
 *	since the system boots without knowing the size of the ROM
 *	the code is mirrored on every page... so we'll jump to
 *	a page way up where we're moving the PROM to
 */
		
		ldr	pc, =(_jump_to_high + NETARM_MMAP_CS0_BASE)

/*
 *	either we now set up the RAM or we were always running from RAM
 *	and we didn't change the memory settings...
 */

_jump_to_high:

		mov	r0, #'C'
		bl	_printch
		
/*
 * test point - jump to blink
 */
/*
 * The NS7520 has a different method of chip select configuration
 */

		ldr	r4, =NETARM_MEM_MODULE_BASE

#ifndef CONFIG_NETARM_NS7520
#ifndef CONFIG_NETARM_EMLIN
/* Settings for NetSilicon's development boards (NET+40, NET+50, ...) */

/* mem config */
		ldr	r1, =( NETARM_MEM_REFR_PERIOD_USEC(16) | \
			       NETARM_MEM_CFG_REFRESH_EN | \
			       NETARM_MEM_CFG_REFR_CYCLE_5CLKS )
		str	r1, [r4, #NETARM_MEM_MODULE_CONFIG]
		
@		mov	r0, #'D'
@		bl	_printch
		
/* CS0 */
		
		ldr	r1, =( NETARM_MEM_BAR_BASE(NETARM_MMAP_CS0_BASE) | 	\
			       NETARM_MEM_BAR_DRAM_FP |			\
			       NETARM_MEM_BAR_DRAM_MUX_INT |		\
			       NETARM_MEM_BAR_DRAM_MUX_BAL |		\
			       NETARM_MEM_BAR_VALID )
		str	r1, [r4, #NETARM_MEM_CS0_BASE_ADDR]

@		mov	r0, #'E'
@		bl	_printch
		
		/* trust that the bus size for flash was strapped correctly */
		/* this saves the bus width in r2 and then ORs it back in */
		/* it's a pretty safe assumption, otherwise it wouldn't boot */

		ldr	r2, [r4, #NETARM_MEM_CS0_OPTIONS]
		and	r2, r2, #NETARM_MEM_OPT_BUS_SIZE_MASK

/*		just a test: assume 32 bit flash mem --rp */
/*		mov	r2, #NETARM_MEM_OPT_32BIT */

		ldr	r1, =( NETARM_MEM_OPT_BASE_USE(NETARM_MMAP_CS0_MASK) |	\
			       NETARM_MEM_OPT_WAIT_STATES(FLASH_90ns_WAIT_STATES) |		\
			       NETARM_MEM_OPT_BCYC_2 |			\
			       NETARM_MEM_OPT_BSIZE_4 | 		\
			       NETARM_MEM_OPT_READ_ASYNC | 		\
			       NETARM_MEM_OPT_WRITE_ASYNC )
		orr	r1, r1, r2
		str	r1, [r4, #NETARM_MEM_CS0_OPTIONS]

@		mov	r0, #'F'
@		bl	_printch

#ifdef	CONFIG_NETARM_NET40_REV2
/* CS1 */
		ldr	r1, =( NETARM_MEM_OPT_BASE_USE(NETARM_MMAP_CS1_MASK) |	\
			       NETARM_MEM_OPT_BCYC_2 |			\
			       NETARM_MEM_OPT_BSIZE_16 | 		\
			       NETARM_MEM_OPT_WAIT_STATES(0) | 		\
			       NETARM_MEM_OPT_32BIT | 			\
			       NETARM_MEM_OPT_READ_ASYNC | 		\
			       NETARM_MEM_OPT_WRITE_ASYNC )
		str	r1, [r4, #NETARM_MEM_CS1_OPTIONS]

@		mov	r0, #'G'
@		bl	_printch
		
		ldr	r1, =( NETARM_MEM_BAR_BASE(NETARM_MMAP_CS1_BASE) | 	\
			       NETARM_MEM_BAR_DRAM_SYNC |		\
			       NETARM_MEM_BAR_DRAM_MUX_INT |		\
			       NETARM_MEM_BAR_DRAM_MUX_UNBAL |		\
			       NETARM_MEM_BAR_DRAM_SEL |		\
			       NETARM_MEM_BAR_BURST_EN |		\
			       NETARM_MEM_BAR_VALID )
		str	r1, [r4, #NETARM_MEM_CS1_BASE_ADDR]

@		mov	r0, #'H'
@		bl	_printch

/* CS2 */
		ldr	r1, =( 0 )
		str	r1, [r4, #NETARM_MEM_CS2_BASE_ADDR]

@		mov	r0, #'I'
@		bl	_printch

		ldr	r1, =( 0 )
		str	r1, [r4, #NETARM_MEM_CS2_OPTIONS]

@		mov	r0, #'J'
@		bl	_printch
#else
/* CS1 */
		ldr	r1, =( NETARM_MEM_OPT_BASE_USE(NETARM_MMAP_CS1_MASK) |	\
			       NETARM_MEM_OPT_BCYC_3 |			\
			       NETARM_MEM_OPT_BSIZE_4 | 		\
			       NETARM_MEM_OPT_WAIT_STATES(0) | 		\
			       NETARM_MEM_OPT_32BIT | 			\
			       NETARM_MEM_OPT_READ_ASYNC | 		\
			       NETARM_MEM_OPT_WRITE_ASYNC )
		str	r1, [r4, #NETARM_MEM_CS1_OPTIONS]

@		mov	r0, #'G'
@		bl	_printch
		
		ldr	r1, =( NETARM_MEM_BAR_BASE(NETARM_MMAP_CS1_BASE) | 	\
			       NETARM_MEM_BAR_DRAM_FP |		\
			       NETARM_MEM_BAR_DRAM_MUX_INT |		\
			       NETARM_MEM_BAR_DRAM_MUX_BAL |		\
			       NETARM_MEM_BAR_DRAM_SEL |		\
			       NETARM_MEM_BAR_BURST_EN |		\
			       NETARM_MEM_BAR_VALID )
		str	r1, [r4, #NETARM_MEM_CS1_BASE_ADDR]

@		mov	r0, #'H'
@		bl	_printch
		
/* CS2 */
		ldr	r1, =( NETARM_MEM_OPT_BASE_USE(NETARM_MMAP_CS2_MASK) |	\
			       NETARM_MEM_OPT_BCYC_3 |			\
			       NETARM_MEM_OPT_BSIZE_4 | 		\
			       NETARM_MEM_OPT_WAIT_STATES(0) | 		\
			       NETARM_MEM_OPT_32BIT | 			\
			       NETARM_MEM_OPT_READ_ASYNC | 		\
			       NETARM_MEM_OPT_WRITE_ASYNC )
		str	r1, [r4, #NETARM_MEM_CS2_OPTIONS]

@		mov	r0, #'I'
@		bl	_printch
		
		ldr	r1, =( NETARM_MEM_BAR_BASE(NETARM_MMAP_CS2_BASE) | 	\
			       NETARM_MEM_BAR_DRAM_FP |		\
			       NETARM_MEM_BAR_DRAM_MUX_INT |		\
			       NETARM_MEM_BAR_DRAM_MUX_BAL |		\
			       NETARM_MEM_BAR_DRAM_SEL |		\
			       NETARM_MEM_BAR_BURST_EN |		\
			       NETARM_MEM_BAR_VALID )
		str	r1, [r4, #NETARM_MEM_CS2_BASE_ADDR]

@		mov	r0, #'J'
@		bl	_printch
#endif

/* CS3 */
#ifdef CONFIG_NETARM_EEPROM
		ldr	r1, =( NETARM_MEM_OPT_BASE_USE(NETARM_MMAP_CS3_MASK) |	\
			       NETARM_MEM_OPT_BCYC_1 |\
			       NETARM_MEM_OPT_BSIZE_2 | \
			       NETARM_MEM_OPT_WAIT_STATES(10) | \
			       NETARM_MEM_OPT_8BIT | \
			       NETARM_MEM_OPT_READ_ASYNC | \
			       NETARM_MEM_OPT_WRITE_ASYNC )
#else
		ldr	r1, =( 0 )
#endif
		str	r1, [r4, #NETARM_MEM_CS3_OPTIONS]

@		mov	r0, #'K'
@		bl	_printch
		
#ifdef CONFIG_NETARM_EEPROM
		ldr	r1, =( NETARM_MEM_BAR_BASE(NETARM_MMAP_CS3_BASE) | 	\
			       NETARM_MEM_BAR_DRAM_FP |			\
			       NETARM_MEM_BAR_DRAM_MUX_INT |		\
			       NETARM_MEM_BAR_DRAM_MUX_BAL |		\
			       NETARM_MEM_BAR_VALID )
#else
		ldr	r1, =( 0 )
#endif
		str	r1, [r4, #NETARM_MEM_CS3_BASE_ADDR]

@		mov	r0, #'L'
@		bl	_printch
		
/* CS4 */
		ldr	r1, =( NETARM_MEM_OPT_BASE_USE(NETARM_MMAP_CS4_MASK) |	\
			       NETARM_MEM_OPT_32BIT_EXT_ACK | \
			       NETARM_MEM_OPT_READ_ASYNC | \
			       NETARM_MEM_OPT_WRITE_ASYNC )
		str	r1, [r4, #NETARM_MEM_CS4_OPTIONS]

		ldr	r1, =( NETARM_MEM_BAR_BASE(NETARM_MMAP_CS4_BASE) | 	\
			       NETARM_MEM_BAR_DRAM_FP |			\
			       NETARM_MEM_BAR_DRAM_MUX_INT |		\
			       NETARM_MEM_BAR_DRAM_MUX_BAL |		\
			       NETARM_MEM_BAR_VALID )
		str	r1, [r4, #NETARM_MEM_CS4_BASE_ADDR]

#else
/* Settings for IMMS' EMLIN board */
/* mem config */
		ldr	r1, =( NETARM_MEM_REFR_PERIOD_USEC(15) | \
			       NETARM_MEM_CFG_REFRESH_EN | \
			       NETARM_MEM_CFG_REFR_CYCLE_8CLKS )
		str	r1, [r4, #NETARM_MEM_MODULE_CONFIG]
		
		mov	r0, #'D'
		bl	_printch
		
/* CS0 (Flash) */
		ldr	r1, =( NETARM_MEM_BAR_BASE(NETARM_MMAP_CS0_BASE) | 	\
			       NETARM_MEM_BAR_DRAM_FP |			\
			       NETARM_MEM_BAR_DRAM_MUX_INT |		\
			       NETARM_MEM_BAR_DRAM_MUX_BAL |		\
			       NETARM_MEM_BAR_VALID )
		str	r1, [r4, #NETARM_MEM_CS0_BASE_ADDR]

		mov	r0, #'E'
		bl	_printch
		
		/* trust that the bus size for flash was strapped correctly */
		/* this saves the bus width in r2 and then ORs it back in */
		/* it's a pretty safe assumption, otherwise it wouldn't boot */

		ldr	r2, [r4, #NETARM_MEM_CS0_OPTIONS]
		and	r2, r2, #NETARM_MEM_OPT_BUS_SIZE_MASK

		ldr	r1, =( NETARM_MEM_OPT_BASE_USE(0xFFC00000) |	\
			       NETARM_MEM_OPT_WAIT_STATES(FLASH_90ns_WAIT_STATES) |		\
			       NETARM_MEM_OPT_BCYC_2 |			\
			       NETARM_MEM_OPT_BSIZE_4 | 		\
			       NETARM_MEM_OPT_READ_ASYNC | 		\
			       NETARM_MEM_OPT_WRITE_ASYNC )
		orr	r1, r1, r2
		str	r1, [r4, #NETARM_MEM_CS0_OPTIONS]

		mov	r0, #'F'
		bl	_printch

/* CS1 (SDRAM - optional) */
		ldr	r1, =( NETARM_MEM_OPT_BASE_USE(0xFF000000) |	\
			       NETARM_MEM_OPT_BCYC_2 |			\
			       NETARM_MEM_OPT_BSIZE_8 | 		\
			       NETARM_MEM_OPT_WAIT_STATES(0) | 		\
			       NETARM_MEM_OPT_32BIT | 			\
			       NETARM_MEM_OPT_READ_ASYNC | 		\
			       NETARM_MEM_OPT_WRITE_ASYNC )
		str	r1, [r4, #NETARM_MEM_CS1_OPTIONS]

		mov	r0, #'G'
		bl	_printch
		
		ldr	r1, =( NETARM_MEM_BAR_BASE(NETARM_MMAP_CS2_BASE) | 	\
			       NETARM_MEM_BAR_DRAM_SYNC |		\
			       NETARM_MEM_BAR_DRAM_MUX_INT |		\
			       NETARM_MEM_BAR_DRAM_MUX_UNBAL |		\
			       NETARM_MEM_BAR_DRAM_SEL )
		str	r1, [r4, #NETARM_MEM_CS1_BASE_ADDR]

		mov	r0, #'H'
		bl	_printch

/* CS2 (mainboard SDRAM) */
		ldr	r1, =( NETARM_MEM_OPT_BASE_USE(0xFF000000) |	\
			       NETARM_MEM_OPT_BCYC_2 |			\
			       NETARM_MEM_OPT_BSIZE_8 | 		\
			       NETARM_MEM_OPT_WAIT_STATES(0) | 		\
			       NETARM_MEM_OPT_32BIT | 			\
			       NETARM_MEM_OPT_READ_ASYNC | 		\
			       NETARM_MEM_OPT_WRITE_ASYNC )
		str	r1, [r4, #NETARM_MEM_CS2_OPTIONS]

		mov	r0, #'I'
		bl	_printch
		
		ldr	r1, =( NETARM_MEM_BAR_BASE(NETARM_MMAP_CS1_BASE) | 	\
			       NETARM_MEM_BAR_DRAM_SYNC |		\
			       NETARM_MEM_BAR_DRAM_MUX_INT |		\
			       NETARM_MEM_BAR_DRAM_MUX_UNBAL |		\
			       NETARM_MEM_BAR_DRAM_SEL |		\
			       NETARM_MEM_BAR_VALID )
		str	r1, [r4, #NETARM_MEM_CS2_BASE_ADDR]

		mov	r0, #'J'
		bl	_printch

/* CS3 (Peripherals) */
		ldr	r1, =( NETARM_MEM_OPT_BASE_USE(0xFFF00000) |	\
			       NETARM_MEM_OPT_BCYC_4 |\
			       NETARM_MEM_OPT_BSIZE_2 | \
			       NETARM_MEM_OPT_WAIT_STATES(15) | \
			       NETARM_MEM_OPT_16BIT | \
			       NETARM_MEM_OPT_READ_SYNC | \
			       NETARM_MEM_OPT_WRITE_SYNC )
		str	r1, [r4, #NETARM_MEM_CS3_OPTIONS]

		mov	r0, #'K'
		bl	_printch
		
		ldr	r1, =( NETARM_MEM_BAR_BASE(NETARM_MMAP_CS3_BASE) | 	\
			       NETARM_MEM_BAR_DRAM_FP |			\
			       NETARM_MEM_BAR_DRAM_MUX_INT |		\
			       NETARM_MEM_BAR_1BCLK_IDLE |		\
			       NETARM_MEM_BAR_DRAM_MUX_BAL |		\
			       NETARM_MEM_BAR_VALID )
		str	r1, [r4, #NETARM_MEM_CS3_BASE_ADDR]

		mov	r0, #'L'
		bl	_printch

/* CS4 (FPGA) */
		ldr	r1, =( NETARM_MEM_OPT_BASE_USE(0xFFFFF000) |	\
			       NETARM_MEM_OPT_WAIT_STATES(15) | \
			       NETARM_MEM_OPT_32BIT | \
			       NETARM_MEM_OPT_READ_SYNC | \
			       NETARM_MEM_OPT_WRITE_SYNC )
		str	r1, [r4, #NETARM_MEM_CS4_OPTIONS]

		ldr	r1, =( 0x30000000 | 	\
			       NETARM_MEM_BAR_DRAM_FP |			\
			       NETARM_MEM_BAR_DRAM_MUX_INT |		\
			       NETARM_MEM_BAR_1BCLK_IDLE |		\
			       NETARM_MEM_BAR_DRAM_MUX_BAL |		\
			       NETARM_MEM_BAR_VALID )
		str	r1, [r4, #NETARM_MEM_CS4_BASE_ADDR]

#endif
#else

#ifdef CONFIG_BOARD_UNC20
/* mem config  (0xFFC0 0000)*/
  	ldr	r1, =( 0x0d000000 | \
	  					 NETARM_MEM_CFG_REFRESH_EN )
  	str	r1, [r4, #NETARM_MEM_MODULE_CONFIG]
  	mov	r0, #'D'
  	bl	_printch
/* CS0 FLASH */


  /* We are running already from flash, so changing the option register is not a
     good idea before adjusting base address as the size of chip is stored there.
     The flash should also be writeable - necessary for probing */
  	ldr	r1, =( NETARM_MEM_BAR_BASE(NETARM_MMAP_CS0_BASE) | 	\
		         NETARM_MEM_BAR_DRAM_FP |			\
		         NETARM_MEM_BAR_DRAM_MUX_INT |		\
		         NETARM_MEM_BAR_DRAM_MUX_BAL |		\
	  	       NETARM_MEM_BAR_VALID )
  	str	r1, [r4, #NETARM_MEM_CS0_BASE_ADDR]

  	mov	r0, #'E'
  	bl	_printch
  	ldr	r1, =( NETARM_MEM_OPT_BASE_USE(NETARM_MMAP_CS0_MASK) |	\
      NETARM_MEM_OPT_WAIT_STATES(8) | 		\
      NETARM_MEM_OPT_16BIT | 			\
      NETARM_MEM_OPT_READ_ASYNC | 		\
      NETARM_MEM_OPT_WRITE_ASYNC )
  	str	r1, [r4, #NETARM_MEM_CS0_OPTIONS]

  	ldr	r1, =( NETARM_MEM_OPTB_SYNC_1_STAGE | \
      NETARM_MEM_OPTB_BCYC_PLUS0 | \
      NETARM_MEM_OPTB_WAIT_PLUS0 )
  	str	r1, [r4, #NETARM_MEM_CS0_OPTIONS_B]

	mov	r0, #'F'
	bl	_printch
	
/* CS1 SDRAM*/
	ldr	r1, =( NETARM_MEM_OPT_BASE_USE(NETARM_MMAP_CS1_MASK) |	\
		       NETARM_MEM_OPT_BCYC_2 |			\
		       NETARM_MEM_OPT_BSIZE_16 | 		\
		       NETARM_MEM_OPT_WAIT_STATES(0) | 		\
		       NETARM_MEM_OPT_32BIT | 			\
		       NETARM_MEM_OPT_READ_ASYNC | 		\
		       NETARM_MEM_OPT_WRITE_ASYNC )
	str	r1, [r4, #NETARM_MEM_CS1_OPTIONS]

	ldr	r1, =( NETARM_MEM_OPTB_SYNC_1_STAGE | \
		       NETARM_MEM_OPTB_BCYC_PLUS0 | \
		       NETARM_MEM_OPTB_WAIT_PLUS0 )
	str	r1, [r4, #NETARM_MEM_CS1_OPTIONS_B]

	ldr	r1, =( NETARM_MEM_BAR_BASE(NETARM_MMAP_CS1_BASE) | 	\
		       NETARM_MEM_BAR_DRAM_SYNC |		\
		       NETARM_MEM_BAR_DRAM_MUX_INT |		\
		       NETARM_MEM_BAR_DRAM_MUX_UNBAL |		\
		       NETARM_MEM_BAR_DRAM_SEL |		\
		       NETARM_MEM_BAR_BURST_EN |		\
		       NETARM_MEM_BAR_VALID )
	str	r1, [r4, #NETARM_MEM_CS1_BASE_ADDR]

	mov	r0, #'G'
	bl	_printch
#endif

#endif /* !CONFIG_NETARM_NS7520 */

	/* Wait 8 refresh cycles off SDRAM 120uS */

	ldr	r1,=6600
_waitsdram:
	subs	r1,r1,#1
	bne	_waitsdram

#ifdef CONFIG_NETARM_BOOTLOADER_DEBUG_RAMPTEST
/* ramp test RAM */
	mov r0, #'H'
	bl _printch
	mov r1, #NETARM_MMAP_RAM_BASE;
  mov r7, #0x00800000;
  mov r5, #0x0
1:
	str r5, [r1], #4
	add r5, r5, #0x1
	subs r7, r7, #4
	bgt 1b

	mov r6, #NETARM_MMAP_RAM_BASE;
	mov r7, #0x00800000;
	mov r5, #0x0
1:
	ldr r4, [r6], #4
	teq r5, r4
	beq 2f
	mov r0, #'\n'
	bl _printch
	mov r0, #'A'
	bl _printch
	mov r0, #' '
	bl _printch
	mov r0, r6
  sub r0, r0, #4 /* r6 auto-post-increments */
	bl _printhex8
	mov r0, #'\n'
	bl _printch
	mov r0, #'W'
	bl _printch
	mov r0, #' '
	bl _printch
	mov r0, r5
	bl _printhex8
	mov r0, #' '
	bl _printch
	mov r0, #'G'
	bl _printch
	mov r0, #' '
	bl _printch
	mov r0, r4
	bl _printhex8
2:
	add r5, r5, #0x1
	subs r7, r7, #4
	bgt 1b
#endif

/* copy to RAM */
	mov	r0, #'H'
	bl	_printch

		mov	r1, #NETARM_MMAP_RAM_BASE;
		mov	r0, #NETARM_MMAP_FLASH_BASE;
		mov	r2, #NETARM_MMAP_FLASH_COPY_SIZE;
1:
		ldmia	r0!,{r3,r4,r5,r6,r7,r8,r9,r10}
		stmia	r1!,{r3,r4,r5,r6,r7,r8,r9,r10}
		subs	r2, r2, #0x20
		bgt	1b



_exec_in_RAM:
		mov	r0, #'N'
		bl	_printch
		
 		ldr	pc, =(__zentry)
__zentry:

/*
 * print bootloaded hello and current PC
 */

		/* Now we are in RAM */
		mov	r0, #'O'
		bl	_printch

/*
 * Copy ROMFS for use. 
 */
#ifdef CONFIG_RAM_ATTACHED_ROMFS 
		ldr	r5, =LC1
		ldmia	r5, {r5, r8}  /* r5 = _edata, r8 = _ebss */

		mov r0, r5
		mov r1, r8
  
		ldr r2, [r0, #+8] /* Big-endian Size */
		mov r3, #0				 /* Init result */
		and r4, r2, #0x000000ff
		orr r3, r3, r4, lsl #24
		and r4, r2, #0x0000ff00
		orr r3, r3, r4, lsl #8
		and r4, r2, #0x00ff0000
		orr r3, r3, r4, lsr #8
		and r4, r2, #0xff000000
		orr r3, r3, r4, lsr #24
		mov r2, r3
		
		add r2, r2, #8
		and r2, r2, #0xfffffffc
		
		add r0, r0, r2 /* Copy from end */
		add r1, r1, r2
		ldr r3, =_ramstart
		str r1, [r3]

_copy_romfs:
		ldr r3,[r0,#-4]!		  /* Copy dword */
		str r3,[r1,#-4]!
		cmp r0, r5				/* Check if at end */
		bne _copy_romfs
		
		mov r0, r2
		bl _printhex8
		
		mov	r0, #'P'
		bl	_printch
#endif  
/*
 * print banner message
 */
		mov	r0, #'@'
		bl	_printch

		mov	r0, pc
		bl 	_printhex8

		mov	r0, #'\n'
		bl	_printch
		
		ldr	r0, =_hello_world
		bl 	_printascii

		mov	r0, #'\n'
		bl	_printch

#if CONFIG_NETARM_BOOTLOADER_DEBUG_OUTPUT	
/*
 * dump serial and memory control registers
 */

		bl	_netarm_debug_serial_dump
		bl	_netarm_debug_mem_ctl_dump		
	
/*
 * count down to boot...
 */

		mov	r8, #15

1:		
		mov	r0, r8
		bl 	_printhex2

		mov	r0, #'\r'
		bl	_printch

		mov	r2, #0x20000
2:
		subs	r2, r2, #1
		bgt	2b
		
		subs	r8, r8, #1
		bgt	1b

/*
 * test point - jump to blink
 */

/*		b	_netarm_led_blink */

#endif	
	
/*
 * set up initial stack 
 */

		ldr	r5, =LC0
		ldmia	r5, {r5, r6, r8, r9, sp}

/* clear BSS segment */
		mov	r4, #0
1:		cmp	r5, r8
		strcc	r4, [r5], #4
		bcc	1b

        /*  Put initial values into stack.  This would normally be done
        by sched_init() in kernel/sched.c, but that would overwrite the
        stack we're already using.  That would be bad.
        (code from the DSC21 head file by Steve Johnson) 
	Only fill half the task_union area, because the task list starts
	in the lower part. --rp */
        	mov r5, sp
        	sub r5, r5, #0x1000
        	ldr r4, L_STACK_MAGIC
        	str r4, [r5], #4
        	ldr r4, L_STACK_UNTOUCHED_MAGIC
1:      	cmp r5, sp
        	strcc r4, [r5], #4
        	bcc 1b

/*
 * Stob in the ARMID - since we don't have CP15 to query... 
 */

		ldr	r2, =0x41007700
		str	r2, [r6]
/*  Pretend we know what our processor code is (for arm_id)   */
		mov     r2, #MACH_TYPE_NETARM
		str     r2, [r9]

/*
 * Jump to linux
 */

		mov	fp, #0
		b	start_kernel


/*
 * NET+ARM development board blink routine - turns led on and then off
 * approx one second on, one second off.
 * NET+40 board: The RED led is on bit 2 (0x04)
 */

		.globl	_netarm_led_blink
_netarm_led_blink:

/*
 * config C1 as GPIO output
 */
		ldr	r4, =NETARM_GEN_MODULE_BASE
		ldr	r1, [r4, #+NETARM_GEN_PORTC]
		ldr	r2, =((0xFFFFFFFF) ^ (NETARM_GEN_PORT_MODE(NA_BLINK_LED) | \
			                      NETARM_GEN_PORT_DIR(NA_BLINK_LED)))
		and	r1, r1, r2
		ldr	r2, =(NETARM_GEN_PORT_DIR(NA_BLINK_LED))
		orr	r1, r1, r2
		str	r1, [r4, #+NETARM_GEN_PORTC]
/*
 * turn the LED off ( output high )
 */
1:
		eor	r1, r1, #(NA_BLINK_LED)
		str	r1, [r4, #+NETARM_GEN_PORTC]
/*
 * wait a bit - million iterations, give or take
 */
		mov	r2, #0x100000
2:
		subs	r2, r2, #1
		bgt	2b

		b 	1b		

		.globl	_netarm_led_FAIL
_netarm_led_FAIL:

		mov	r0, lr
		bl	_printhex8

/*
 * config port C pins as GPIO output
 */
		ldr	r4, =NETARM_GEN_MODULE_BASE
		ldr	r1, [r4, #+NETARM_GEN_PORTC]
		ldr	r2, =((0xFFFFFFFF) ^ \
		                (NETARM_GEN_PORT_MODE(NA_BLINK_LED | NA_FAIL_LED) | \
			         NETARM_GEN_PORT_DIR(NA_BLINK_LED | NA_FAIL_LED)))
		and	r1, r1, r2
		ldr	r2, =(NETARM_GEN_PORT_DIR(0x06))
		orr	r1, r1, r2
		orr	r1, r1, #(NA_BLINK_LED | NA_FAIL_LED)
		eor	r1, r1, #NA_FAIL_LED
		str	r1, [r4, #+NETARM_GEN_PORTC]
/*
 * turn the LED off ( output high )
 */
1:
		eor	r1, r1, #(NA_BLINK_LED | NA_FAIL_LED)
		str	r1, [r4, #+NETARM_GEN_PORTC]
/*
 * wait a bit - million iterations, give or take
 */
		mov	r2, #0x20000
2:
		subs	r2, r2, #1
		bgt	2b

		b 	1b		

		.globl	_netarm_led_FAIL2
_netarm_led_FAIL2:

		mov	r0, lr
		bl	_printhex8

/*
 * config port C "blink" pin as GPIO output
 */
		ldr	r4, =NETARM_GEN_MODULE_BASE
		ldr	r1, [r4, #+NETARM_GEN_PORTC]
		ldr	r2, =((0xFFFFFFFF) ^ (NETARM_GEN_PORT_MODE(NA_BLINK_LED) | \
			                      NETARM_GEN_PORT_DIR(NA_BLINK_LED)))
		and	r1, r1, r2
		ldr	r2, =(NETARM_GEN_PORT_DIR(NA_BLINK_LED))
		orr	r1, r1, r2
		orr	r1, r1, #(NA_BLINK_LED | NA_FAIL_LED)
		eor	r1, r1, #NA_FAIL_LED
		str	r1, [r4, #+NETARM_GEN_PORTC]
/*
 * turn the LED off ( output high )
 */
1:
		eor	r1, r1, #(NA_BLINK_LED)
		str	r1, [r4, #+NETARM_GEN_PORTC]
/*
 * wait a bit - 64k iterations, give or take
 */
		mov	r2, #0x10000
2:
		subs	r2, r2, #1
		bgt	2b

		b 	1b		

_netarm_debug_serial_init:
/* 
 * step 0... make sure pins are configured for serial without losing any
 * settings 
 */
		ldr	r4, =NETARM_GEN_MODULE_BASE
		
		ldr	r1, =( NETARM_GEN_PORT_MODE(0x88) | \
							 NETARM_GEN_PORT_DIR(0x80) )
		ldr	r2, [r4, #+NETARM_GEN_PORTA]
		orr	r1, r1, r2
		str	r1, [r4, #+NETARM_GEN_PORTA]
#ifdef CONFIG_NETARM_NS7520
		 /* NS7520 has a CSF field that needs to be set */
		ldr	r1, =( NETARM_GEN_PORT_MODE(0x88) | \
							 NETARM_GEN_PORT_DIR(0x80) | \
								 NETARM_GEN_PORT_CSF(0x88) )
		ldr	r2, [r4, #+NETARM_GEN_PORTC]
		orr	r1, r1, r2
		str	r1, [r4, #+NETARM_GEN_PORTC]
#else
		str	r1, [r4, #+NETARM_GEN_PORTB]
#endif
		ldr	r4, =NETARM_SER_MODULE_BASE
		
/* first turn em off */
		mov	r1, #0
		str	r1, [r4, #+NETARM_SER_CH1_CTRL_A]
		str	r1, [r4, #+NETARM_SER_CH2_CTRL_A]
		
/* clear match register */
		str	r1, [r4, #+NETARM_SER_CH1_RX_MATCH]
		str	r1, [r4, #+NETARM_SER_CH2_RX_MATCH]

/* setup bit rate generator */
		ldr	r1, =NETARM_SER_BR_X16(DEFAULT_BAUD_RATE)
		str	r1, [r4, #+NETARM_SER_CH1_BITRATE]
		str	r1, [r4, #+NETARM_SER_CH2_BITRATE]

/* setup rx buffer gap timer */
		ldr	r1, =NETARM_SER_RXGAP(DEFAULT_BAUD_RATE)
		str	r1, [r4, #+NETARM_SER_CH1_RX_BUF_TMR]
		str	r1, [r4, #+NETARM_SER_CH2_RX_BUF_TMR]

/* setup port mode */		
		mov	r1, #NETARM_SER_CTLB_UART_MODE
		str	r1, [r4, #+NETARM_SER_CH1_CTRL_B]
		str	r1, [r4, #+NETARM_SER_CH2_CTRL_B]

		ldr	r1, =( NETARM_SER_CTLA_ENABLE |		\
			       NETARM_SER_CTLA_P_NONE |		\
			       NETARM_SER_CTLA_2STOP | 		\
			       NETARM_SER_CTLA_8BITS |		\
			       NETARM_SER_CTLA_DTR_EN |		\
			       NETARM_SER_CTLA_RTS_EN)
		str	r1, [r4, #+NETARM_SER_CH1_CTRL_A]
		str	r1, [r4, #+NETARM_SER_CH2_CTRL_A]

		mov	pc, lr		

#if CONFIG_NETARM_BOOTLOADER_DEBUG_OUTPUT	
_netarm_debug_serial_dump:
		ldr	r9, =NETARM_SER_MODULE_BASE
		mov	r10, lr
		
		ldr	r0, =_serial_hdr
		bl	_printascii

		mov	r0, #'\n'
		bl	_printch

/* read control A */
		ldr	r0, =_serial_ctla
		bl	_printascii

		ldr	r0, [r9, #+NETARM_SER_CH1_CTRL_A]
		bl	_printhex8
		mov	r0, #' '
		bl	_printch
		
		ldr	r0, [r9, #+NETARM_SER_CH2_CTRL_A]
		bl	_printhex8
		mov	r0, #'\n'
		bl	_printch

/* read control A */
		ldr	r0, =_serial_ctlb
		bl	_printascii

		ldr	r0, [r9, #+NETARM_SER_CH1_CTRL_B]
		bl	_printhex8
		mov	r0, #' '
		bl	_printch
		
		ldr	r0, [r9, #+NETARM_SER_CH2_CTRL_B]
		bl	_printhex8
		mov	r0, #'\n'
		bl	_printch

/* read match register */
		ldr	r0, =_serial_mr
		bl	_printascii

		ldr	r0, [r9, #+NETARM_SER_CH1_RX_MATCH]
		bl	_printhex8
		mov	r0, #' '
		bl	_printch

		ldr	r0, [r9, #+NETARM_SER_CH2_RX_MATCH]
		bl	_printhex8
		mov	r0, #'\n'
		bl	_printch

/* bit rate generator */
		ldr	r0, =_serial_brr
		bl	_printascii

		ldr	r0, [r9, #+NETARM_SER_CH1_BITRATE]
		bl	_printhex8
		mov	r0, #' '
		bl	_printch
		
		ldr	r0, [r9, #+NETARM_SER_CH2_BITRATE]
		bl	_printhex8
		mov	r0, #'\n'
		bl	_printch
		
/* rx buffer gap timer */
		ldr	r0, =_serial_bgt
		bl	_printascii

		ldr	r0, [r9, #+NETARM_SER_CH1_RX_BUF_TMR]
		bl	_printhex8
		mov	r0, #' '
		bl	_printch

		ldr	r0, [r9, #+NETARM_SER_CH2_RX_BUF_TMR]
		bl	_printhex8
		mov	r0, #'\n'
		bl	_printch

		mov	lr, r10
		mov	pc, lr		

_netarm_debug_mem_ctl_dump:
		ldr	r9, =NETARM_MEM_MODULE_BASE
		mov	r10, lr
		
		ldr	r0, =_memctl_hdr
		bl	_printascii

/* MMCR */
		ldr	r0, [r9, #+NETARM_MEM_MODULE_CONFIG]
		bl	_printhex8

		mov	r0, #'\n'
		bl	_printch
		
/* CS0 BAR */
		ldr	r0, =_memctl_bar1
		bl	_printascii

		mov	r0, #0
		bl	_printhex2

		ldr	r0, =_memctl_bar2
		bl	_printascii

		ldr	r0, [r9, #+NETARM_MEM_CS0_BASE_ADDR]
		bl	_printhex8
		
		mov	r0, #'\n'
		bl	_printch
		
/* CS0 OPT */
		ldr	r0, =_memctl_opt1
		bl	_printascii

		mov	r0, #0
		bl	_printhex2

		ldr	r0, =_memctl_opt2
		bl	_printascii

		ldr	r0, [r9, #+NETARM_MEM_CS0_OPTIONS]
		bl	_printhex8
		
		mov	r0, #'\n'
		bl	_printch
		
/* CS1 BAR */
		ldr	r0, =_memctl_bar1
		bl	_printascii

		mov	r0, #1
		bl	_printhex2

		ldr	r0, =_memctl_bar2
		bl	_printascii

		ldr	r0, [r9, #+NETARM_MEM_CS1_BASE_ADDR]
		bl	_printhex8
		
		mov	r0, #'\n'
		bl	_printch
		
/* CS1 OPT */
		ldr	r0, =_memctl_opt1
		bl	_printascii

		mov	r0, #1
		bl	_printhex2

		ldr	r0, =_memctl_opt2
		bl	_printascii

		ldr	r0, [r9, #+NETARM_MEM_CS1_OPTIONS]
		bl	_printhex8
		
		mov	r0, #'\n'
		bl	_printch
		
/* CS2 BAR */
		ldr	r0, =_memctl_bar1
		bl	_printascii

		mov	r0, #2
		bl	_printhex2

		ldr	r0, =_memctl_bar2
		bl	_printascii

		ldr	r0, [r9, #+NETARM_MEM_CS2_BASE_ADDR]
		bl	_printhex8
		
		mov	r0, #'\n'
		bl	_printch
		
/* CS2 OPT */
		ldr	r0, =_memctl_opt1
		bl	_printascii

		mov	r0, #2
		bl	_printhex2

		ldr	r0, =_memctl_opt2
		bl	_printascii

		ldr	r0, [r9, #+NETARM_MEM_CS2_OPTIONS]
		bl	_printhex8
		
		mov	r0, #'\n'
		bl	_printch
		
/* CS3 BAR */
		ldr	r0, =_memctl_bar1
		bl	_printascii

		mov	r0, #3
		bl	_printhex2

		ldr	r0, =_memctl_bar2
		bl	_printascii

		ldr	r0, [r9, #+NETARM_MEM_CS3_BASE_ADDR]
		bl	_printhex8
		
		mov	r0, #'\n'
		bl	_printch
		
/* CS3 OPT */
		ldr	r0, =_memctl_opt1
		bl	_printascii

		mov	r0, #3
		bl	_printhex2

		ldr	r0, =_memctl_opt2
		bl	_printascii

		ldr	r0, [r9, #+NETARM_MEM_CS3_OPTIONS]
		bl	_printhex8
		
		mov	r0, #'\n'
		bl	_printch
		
/* CS4 BAR */
		ldr	r0, =_memctl_bar1
		bl	_printascii

		mov	r0, #4
		bl	_printhex2

		ldr	r0, =_memctl_bar2
		bl	_printascii

		ldr	r0, [r9, #+NETARM_MEM_CS4_BASE_ADDR]
		bl	_printhex8
		
		mov	r0, #'\n'
		bl	_printch
		
/* CS4 OPT */
		ldr	r0, =_memctl_opt1
		bl	_printascii

		mov	r0, #4
		bl	_printhex2

		ldr	r0, =_memctl_opt2
		bl	_printascii

		ldr	r0, [r9, #+NETARM_MEM_CS4_OPTIONS]
		bl	_printhex8
		
		mov	r0, #'\n'
		bl	_printch
		
/* return */

		mov	lr, r10
		mov	pc, lr		

#endif // CONFIG_NETARM_BOOTLOADER_DEBUG_OUTPUT
/*
 * Useful debugging routines
 */
		.globl	_printhex8
_printhex8:	mov	r1, #8
		b	printhex

		.globl	_printhex4
_printhex4:	mov	r1, #4
		b	printhex

		.globl	_printhex2
_printhex2:	mov	r1, #2
printhex:	ldr	r2, =hexbuf
		add	r3, r2, r1
		mov	r1, #0
		strb	r1, [r3]
1:		and	r1, r0, #15
		mov	r0, r0, lsr #4
		cmp	r1, #10
		addlt	r1, r1, #'0'
		addge	r1, r1, #'a' - 10
		strb	r1, [r3, #-1]!
		teq	r3, r2
		bne	1b
		mov	r0, r2

		.globl	_printascii
_printascii:
		ldr	r3, =NETARM_SER_MODULE_BASE
		b	3f
1:		
#ifndef CONFIG_NETARM_BOOTLOADER_SECOND_SERIAL    
		ldr	r2, [r3, #+(NETARM_SER_CH1_STATUS_A)]
#else
		ldr	r2, [r3, #+(NETARM_SER_CH2_STATUS_A)]
#endif
		tst	r2, #NETARM_SER_STATA_TX_RDY
		beq	1b
#ifndef CONFIG_NETARM_BOOTLOADER_SECOND_SERIAL    
		strb	r1, [r3, #+(NETARM_SER_CH1_FIFO)]
#else
		strb	r1, [r3, #+(NETARM_SER_CH2_FIFO)]
#endif
		teq	r1, #'\n'
		moveq	r1, #'\r'
		beq	1b
3:		teq	r0, #0
		ldrneb	r1, [r0], #1
		teqne	r1, #0
		bne	1b
		mov	pc, lr

		.globl	_printch
_printch:
		ldr	r3, =NETARM_SER_MODULE_BASE
		mov	r1, r0
		mov	r0, #0
		b	1b

/*
 * Useful debugging routine: List first 8 words
 */
#if CONFIG_NETARM_BOOTLOADER_DEBUG_OUTPUT 
		.globl	_printexcvectors
_printexcvectors:
		mov	r10, lr
		ldr	r0, =_excvec_hdr
		bl	_printascii

		mov	r9,#0
1:		ldr	r0, [r9], #4
		bl	_printhex8
		mov	r0, #'\n'
		bl	_printch
		cmp	r9, #32
		bne	1b

		mov	lr, r10
		mov	pc,lr
#endif
/*
 * Useful debugging routines
 */
		.globl	_printhex8
_printhex8_b:	mov	r1, #8
		b	printhex

		.globl	_printhex4
_printhex4_b:	mov	r1, #4
		b	printhex

		.globl	_printhex2
_printhex2_b:	mov	r1, #2
printhex_b:	ldr	r2, =hexbuf
		add	r3, r2, r1
		mov	r1, #0
		strb	r1, [r3]
1:		and	r1, r0, #15
		mov	r0, r0, lsr #4
		cmp	r1, #10
		addlt	r1, r1, #'0'
		addge	r1, r1, #'a' - 10
		strb	r1, [r3, #-1]!
		teq	r3, r2
		bne	1b
		mov	r0, r2

		.globl	_printascii_b
_printascii_b:
		ldr	r3, =NETARM_SER_MODULE_BASE
		b	3f
1:		
#ifndef CONFIG_NETARM_BOOTLOADER_SECOND_SERIAL    
		ldr	r2, [r3, #+(NETARM_SER_CH1_STATUS_A)]
#else
		ldr	r2, [r3, #+(NETARM_SER_CH2_STATUS_A)]
#endif
		tst	r2, #NETARM_SER_STATA_TX_RDY
		beq	1b
#ifndef CONFIG_NETARM_BOOTLOADER_SECOND_SERIAL    
		strb	r1, [r3, #+(NETARM_SER_CH1_FIFO)]
#else
		strb	r1, [r3, #+(NETARM_SER_CH2_FIFO)]
#endif
		teq	r1, #'\n'
		moveq	r1, #'\r'
		beq	1b
3:		teq	r0, #0
		ldrneb	r1, [r0], #1
		teqne	r1, #0
		bne	1b
		mov	pc, lr

		.globl	_printch
_printch_b:
		ldr	r3, =NETARM_SER_MODULE_BASE
		mov	r1, r0
		mov	r0, #0
		b	1b


/*  These values should stay in sync with linux/kernel.h */
L_STACK_MAGIC:
		.long 0xdeadbeef
L_STACK_UNTOUCHED_MAGIC:
		.long 0xfeef1ef0

		.data
		.align	12
_hello_world:	.ascii	"NET+Lx Bootloader v0.9\n\0"
#if CONFIG_NETARM_BOOTLOADER_DEBUG_OUTPUT
_serial_hdr:	.ascii	"Serial Configuration:\n"
		.ascii	"Channel               01       02\0"
_serial_ctla:	.ascii	"Serial Control A : \0"
_serial_ctlb:	.ascii	"Serial Control B : \0"
_serial_bgt:	.ascii	"Buffer Gap Timer : \0"
_serial_brr:	.ascii	"Bit Rate Control : \0"
_serial_mr:	.ascii	"Match Register   : \0"
_memctl_hdr:	.ascii	"Memory Control Module Configuration:\n"
		.ascii	"Mem Config Reg   : \0"
_memctl_bar1:	.ascii	"BAR\0"
_memctl_bar2:	.ascii	"            : \0"
_memctl_opt1:	.ascii	"OPT\0"
_memctl_opt2:	.ascii	"            : \0"
_excvec_hdr:	.ascii	"Exception vectors (0x00-0x1C):\n\0"
#endif

#ifdef	CONFIG_BLK_DEV_RAMDISK_DATA

		.align	12
		.globl	__ramdisk_data
__ramdisk_data:
#include "ramdisk.inc"
		.globl	__ramdisk_data_end
__ramdisk_data_end:
		.align
	
#endif
#ifdef CONFIG_RAM_ATTACHED_ROMFS 
.global	_ramstart
_ramstart:
.long	0
#endif

		.align	8
LC0:		.long	__bss_start
        	.long	processor_id
        	.long	_end
		.long   __machine_arch_type
        	.long	init_task_union+8192
#ifdef CONFIG_RAM_ATTACHED_ROMFS 
LC1:		.long	_edata
        .long	_ebss
#endif        

		.section	.rodata
		.align	12
__zzzz:		.space	16
		.align

		.bss
		.align	12
hexbuf:		.space 16
		.align

