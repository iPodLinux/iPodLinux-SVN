/***********************************************************************
 * linux/arch/armnommu/kernel/head-arm-c5471.S
 *
 *   Copyright (C) 2003 Cadenux, LLC. All rights reserved.
 *   todd.fischer@cadenux.com  <www.cadenux.com>
 *
 *   Copyright (C) 2001 RidgeRun, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ***********************************************************************/

#include <linux/linkage.h>
#include <linux/autoconf.h>
#include <asm/assembler.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>

#define UART_115K_DIV         0x0000019c
#define UART_BAUD_115200      0x00000001
#define UART_M0D8_NOPRTY_STP1 0x00000003


/* This is the defines to put us into service mode. */

#define I_BIT        0x80
#define F_BIT        0x40           
#define ARM_MODE_SVC 0x13

/* Pretend we know what our processor type code is */

#define ARM_PROCESSOR_ID 0x41007700

/* Size of the initial stack.   Thit init_task_union is the union of
 * a struct task_struct that is pre-initialized and a size arrary of
 * size 2048*sizeof(long) == 8192 (see include/linux/sched.h).
 * We want to write initial values to the stack for crash analysis,
 * but we don't want to clobber the initialized task_struct,
 * so we define: */

#define INIT_TASK_OFFSET  8192
#define INIT_TASK_RESERVE 1024
#define INITIAL_STACK_SIZE (INIT_TASK_OFFSET-INIT_TASK_RESERVE)

/* Register usage: */

#define ARG1        r0

#define TMP1        r0
#define TMP2        r1
#define TMP3        r2
#define TMP4        r3

#define SBSS        r4
#define EBSS        r5

#define COMPAT      r4
#define PROCID      r5
#define MACHID      r6
#define CFGALN      r7

#define DATA_SROM   TMP1
#define DATA_SRAM   TMP2
#define DATA_ERAM   TMP3
#define DATA_TMP    TMP4
#define DATA_FIRST  r3
#define DATA_LAST   r10

#define INT_VECTOR_TABLE (C5471_IRAM_START)
#define VECT_UNDEFINSTR  (INT_VECTOR_TABLE+0x4)
#define VECT_SWI         (INT_VECTOR_TABLE+0x8)
#define VECT_PREFETCH    (INT_VECTOR_TABLE+0xC)
#define VECT_DATA        (INT_VECTOR_TABLE+0x10)
#define VECT_ADDREXCPTN  (INT_VECTOR_TABLE+0x14)
#define VECT_IRQ         (INT_VECTOR_TABLE+0x18)
#define VECT_FIQ         (INT_VECTOR_TABLE+0x1C)
                                         
/* These are checkpoint values */

#define INITIAL_ENTRY     #'A'
#define ENTRY_VERIFY      #'B'
#define BSS_CLEARED       #'C'
#define DATA_INITIALIZED  #'D'
#define STACK_INITIALIZED #'E'
#define CALL_START_KERNEL #'F'
#define END_CHECKPOINT    #'\n'

/* These macros are used to output debug status on the serial console.
 * These macros can be used only after a C stack has been initialized.
 *
 * These macros (incl. printch/hex8) use R0-R3 and lr.
 *
 */

#ifdef CONFIG_DEBUG_LL
	.globl	printch
	.type	printch, #function

	.macro	checkpoint, ch
	mov	ARG1, \ch
	bl	printch
	.endm

	.globl	printhex8
	.type	printhex8, #function

	.macro	showvalue, addr, tmp
	ldr	\tmp, =addr
	mov	ARG1, \tmp
	bl	printhex8
	.endm
#else
	.macro	checkpoint, ch
	.endm
	.macro	showvalue, addr, tmp
	.endm
#endif /* CONFIG_DEBUG_LL */

        .text
        .align
        .global reset_logic

/* The lower portion (low addresses) of the IRAM are used
 * to hold the interrupt vector table. This table will be
 * loaded as part of early bootup and contains
 * the addresses of the various interrupt handlers
 * assigned to each GPP interrupt. This is necessary since
 * most C5471 devices have flash down at the low end of
 * the memory map where the real interrupt vector logic is
 * expected to reside. Meaning that the kernel won't be
 * able to plug in its interrupt vectors at run time (the
 * flash being read-only as far as the kernel is
 * concerned).  Instead, we'll assume that flash starts
 * at address 0x0000000 and this file will put the necessary
 * logic in place to use the RAM based vector table.
 * When an interrupt occurs this code at the flash based interrupt
 * location will pick up the addresses deposited in the
 * RAM table then branch to that specific
 * routine itemized in the table.
 */

/* Note: reset_logic must have an address of 0x00000000 */
        
reset_logic:    
        b start_hw_setup     /* address 0x00 */
IVectors:
        b Vector_undefinstr  /* address 0x04 */
        b Vector_swi         /* address 0x08 */
        b Vector_prefetch    /* address 0x0c */
        b Vector_data /* address 0x10 */
        b Vector_addrexcptn /* address 0x14 */
        b Vector_IRQ /* address 0x18 */
        b Vector_fiq /* address 0x1c */

        nop  
        nop  
        nop  
        nop
        nop  /* force the logic down a few addresses */
        nop  
        nop  
        nop  
        nop

Vector_undefinstr:
        ldr r13, =VECT_UNDEFINSTR
        ldr pc, [r13]
Vector_swi:
/* Note: a little different than the others in that we
 *       have to honor all current register values.
 */
        str r14,[sp,#-0x4]   /* preserve r14 so we have a scratch reg. */
        ldr r14, =VECT_SWI   /* get address of Kernel int routine. */
        ldr pc, [r14]        /* jump to it. (starts by popping r14 value) */
Vector_prefetch:
        ldr r13, =VECT_PREFETCH
        ldr pc, [r13]
Vector_data:
        ldr r13, =VECT_DATA
        ldr pc, [r13]
Vector_addrexcptn:
        ldr r13, =VECT_ADDREXCPTN
        ldr pc, [r13]
Vector_IRQ:
        ldr r13, =VECT_IRQ
        ldr pc, [r13]
Vector_fiq:
        ldr r13, =VECT_FIQ
        ldr pc, [r13]
        
/* -------------------- Stage One Init ---------------------- */

        .global start
	.type	start, #function
        .global stext
	.type	stext, #function
        .global _start
	.type	_start, #function
        .global _stext
	.type	_stext, #function
        .global _entry
	.type	_entry, #function
        .global __stext
	.type	__stext, #function
        .global __entry
	.type	__entry, #function
start:
stext:
_start:
_stext:
_entry:
__stext:
__entry:
        
start_hw_setup: /* Called by reset vector; Running from Flash. */

/* Turn off IRQ/FIQ and put into service mode. */
        mov r0, #(ARM_MODE_SVC | I_BIT | F_BIT)
        msr cpsr, r0

#if (ETHERNET_PHY == ETHERNET_PHY_LU3X31T_T64)
/* Assert the C5471's nRESET_out line to *external* peripherals.
 * Note: | This pin doesn't reset the processor but rather is a
 *       | reset line being sourced by the processor and feeds
 *       | external logic surrounding it like ethernet transciever 
 *       | chip(s), etc.                                
 */
        ldr r0,clkm_ctl_rst_reg
        mov r1,#0x02
        str r1,[r0] /* nRESET_out asserted and DSP reset asserted. */
        ldr r2,=0x000000ff
hold:  
        subs r2,r2,#0x1
        bne hold
        mov r1,#0x00
        str r1,[r0] /* nRESET_out de-asserted, DSP reset de-asserted. */
#endif

/* init Memory I/F (MEMINT) */

        ldr r0,CS0_reg
        ldr r1,=0x00001eaf
        str r1,[r0]
        
        ldr r0,CS1_reg
        ldr r1,=0x00001eaf
        str r1,[r0]
 
        ldr r0,CS2_reg
        ldr r1,=0x000013db
        str r1,[r0]
        
        ldr r0,CS3_reg
        ldr r1,=0x000013c2
        str r1,[r0]

/* init PLL */

        ldr r0,clkm_stop_reg
        ldr r1,=0x00000c00
        str r1,[r0]
        
        ldr r0,clkm_wakeup_reg
        ldr r1,=0x00000c00
        str r1,[r0]

        ldr r0,arm_pll_reg
        ldr r1,=0x00000000
        str r1,[r0]

        ldr r0,=0x0000ffff
loop:  
        subs r0,r0,#0x1
        bne loop

        ldr r0,arm_pll_reg        
        ldr r1,=0x00001014  /* Set ARM frequency to 47.5 Mhz */
        str r1,[r0]

        ldr r0,clkm_dsp_reg
        ldr r1,[r0]
        orr r1,r1,#0x0038
        str r1,[r0]
        
        ldr r0,arm_pll_reg
        ldr r1,[r0]
        orr r1,r1,#0x0002
        str r1,[r0]

loop1:
        ldr r0,arm_pll_reg
        ldr r1,[r0]      
        ldr r2,=0x000007f8
        ands r1,r1,r2
        bne loop1
        
/* switch clock module from test mode to normal mode. */

        ldr r0,clkm_low_power_reg
        ldr r1,=0x00000000
        str r1,[r0]

        ldr r0,clkm_dsp_reg
        ldr r1,[r0]
        orr r1,r1,#0x40
        str r1,[r0]
        
        ldr r0,clkm_dsp_reg
        ldr r1,[r0]
        orr r1,r1,#0x80
        str r1,[r0]
                
/* GPIO Clocks init */

        ldr r0,gpio_kbcio_reg
        ldr r1,=0x00000000
        str r1,[r0]
	
        ldr r0,gpio_keybrd_reg
        ldr r2,[r0]
        ldr r1,=0x00003fff
        and r1,r2,r1
        str r1,[r0]

/* init SDRAM I/F */

        ldr r0,gpio_mux_select_reg 
        ldr r1,=0xffffffdf
        str r1,[r0]

        ldr r0,sdram_reg
        ldr r1,=0x0000000E
        str r1,[r0]

        ldr r0,sdram_BS_reg
        ldr r1,=0x02000000
        str r1,[r0]

        ldr r0,sdram_config
        ldr r1,sdram_config_data
        str r1,[r0]
        
        ldr r0,sdram_ref_count
        ldr r1,=0x00000320
        str r1,[r0]
        
        ldr r0,sdram_init_config
        ldr r1,=0x00021388
        str r1,[r0]

        ldr r0,sdram_cntl
        ldr r1,=0x00000001
        str r1,[r0]
loop2:
        ldr r0,sdram_cntl
        ldr r1,[r0]
        ands r1,r1,#0x2
        beq loop2

/* Setup UART for early debug output */

        ldr r0,=UART_MODEM_MDR
        ldr r1,=0x00000007 /* reset mode. */
        str r1,[r0]
        ldr r2,=0x00010000
loop3:  
        subs r2,r2,#0x1
        bne loop3
        
        ldr r0,=UART_MODEM_MDR
        ldr r1,=0x00000000 /* normal mode. */
        str r1,[r0]
        ldr r0,=UART_MODEM_115K_DIV
        ldr r1,=UART_115K_DIV
        str r1,[r0]
        ldr r2,=0x00010000
loop4:  
        subs r2,r2,#0x1
        bne loop4
          
        ldr r0,=UART_MODEM_FCR
        ldr r1,=0x0006          /* clear RX and TX fifos */
        str r1,[r0]
        ldr r0,=UART_MODEM_FCR
        ldr r1,=0x0001          /* enable RX and TX fifos */
        str r1,[r0]
        ldr r0,=UART_MODEM_DIV
        ldr r1,=UART_BAUD_115200
        str r1,[r0]
        ldr r0,=UART_MODEM_SER_LCR
        ldr r1,=UART_M0D8_NOPRTY_STP1
        str r1,[r0]

/* Next, Some of the GPIO pins on the C5472 chip are
 * routed internally to the GPIO block as well as the Uart
 * block depending on how the internal mux is setup.  In
 * these cases you pick what you want the pins to route
 * to. Here we route them to the Uart block to give that
 * block a connection to the outside world. The chip pins of
 * interest here are the ones that are physically wired
 * to the c547X EVM's external serial port.
 */
        ldr r0,gpio_mux_select_reg
        ldr r1,[r0]
        ldr r2,=0xffffe0ff /* gpio pins of interest are pins 8,9,10,11,12 */
        and r1,r1,r2
        str r1,[r0]

        ldr r0,gpio_cio_reg
        ldr r1,[r0]
        ldr r2,=0xfffbffff
        and r1,r1,r2
        str r1,[r0]

        ldr r0,gpio_io_reg
        ldr r1,[r0]
        ldr r2,=0x00040000
        orr r1,r1,r2
        str r1,[r0]

/* Setup kernel stack (and get the BSS range) */

	adr	TMP1, LC0
	ldmia   TMP1, {SBSS, EBSS, sp}

	checkpoint INITIAL_ENTRY /* Indicates that we have started */
	checkpoint ENTRY_VERIFY  /* Proves that printch works */

/* Clear kernel BSS section */

	mov	TMP1, #0
1:	cmp	SBSS, EBSS                                  
	strcc	TMP1, [SBSS], #4
	bcc	1b

	checkpoint BSS_CLEARED   /* Indicates survived BSS clear */

#if defined(CONFIG_ROMKERNEL)
        
/* Copy kernel's .data sections to new home in RAM. */

	adr	DATA_TMP, LC2
	ldmia	DATA_TMP, {DATA_SROM, DATA_SRAM, DATA_ERAM}

1:	ldmia	DATA_SROM!, {DATA_FIRST - DATA_LAST}
	stmia	DATA_SRAM!, {DATA_FIRST - DATA_LAST}
	cmp	DATA_SRAM, DATA_ERAM
	blt	1b

	checkpoint DATA_INITIALIZED   /* Indicates survived DATA init */
#endif
  
/* Get more memory references */
	
	adr	TMP1, LC1
	ldmia   TMP1, {COMPAT, PROCID, MACHID}

/* Initialize Kernel Stack Contents */

	mov	TMP2, sp
	sub	TMP2, TMP2, #INITIAL_STACK_SIZE
	ldr	TMP1, L_STACK_MAGIC
	str	TMP1, [TMP2], #4
	ldr	TMP1, L_STACK_UNTOUCHED_MAGIC
1:      cmp	TMP2, sp
        strcc	TMP1, [TMP2], #4
        bcc	1b

	checkpoint STACK_INITIALIZED   /* Indicates survived stack init */

/* Setup parameters for start_kernel... */
/* Suppress messages about compatibility... */

	mov	TMP1, #0
	str	TMP1, [COMPAT]

/* Pretend we know what our processor code is */

	ldr	TMP1, =ARM_PROCESSOR_ID
	str	TMP1, [PROCID]

	mov	TMP1, #MACH_TYPE_C5471
	str	TMP1, [MACHID]

/* Jump to kernel entry */

	checkpoint CALL_START_KERNEL   /* Indicates start_kernel called */
	checkpoint END_CHECKPOINT      /* (for cosmetic reasons) */

	mov	fp, #0
	b	start_kernel

/* Variables */

sdram_config_data:
        .long 0x001486a2 /* 32 Mbytes memory */
        
LC0:	.long	SYMBOL_NAME(__bss_start)
	.long	SYMBOL_NAME(_end)
	.long	SYMBOL_NAME(init_task_union)+INIT_TASK_OFFSET

LC1:	.long	SYMBOL_NAME(compat)
	.long	SYMBOL_NAME(processor_id)
	.long	SYMBOL_NAME(__machine_arch_type)
 
LC2:	.long	__RONLY_end  /* Flash .data defaults location */
	.long	__data_start /* Target SDRAM .data location */
	.long	__data_end

L_STACK_UNTOUCHED_MAGIC:
        .long 0xfeef1ef0
L_STACK_MAGIC:
        .long 0xdeadbeef
gpio_io_reg:
        .word 0xffff2800
gpio_cio_reg:
        .word 0xffff2804      
gpio_mux_select_reg:
        .word 0xffff2814
gpio_keybrd_reg:
        .word 0xffff2914
gpio_kbcio_reg:
        .word 0xffff2904        
CS0_reg:
        .word 0xffff2e00
CS1_reg:
        .word 0xffff2e04
CS2_reg:
        .word 0xffff2e08
CS3_reg:
        .word 0xffff2e0c
sdram_reg:
        .word 0xffff2e18
sdram_BS_reg:
        .word 0xffff2e1C
clkm_stop_reg:
        .word 0xffff2f00
clkm_dsp_reg:
        .word 0xffff2f04
clkm_wakeup_reg:
        .word 0xffff2f08
clkm_ctl_rst_reg:
        .word 0xffff2f10
clkm_low_power_reg:
        .word 0xffff2f1c
sdram_config:
        .word 0xffff3000
sdram_ref_count:
        .word 0xffff3004
sdram_cntl:
        .word 0xffff3008
sdram_init_config:
        .word 0xffff300c
arm_pll_reg:
        .word 0xffff3200
