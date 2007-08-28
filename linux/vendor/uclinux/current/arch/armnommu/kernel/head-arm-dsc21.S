/*
 * head-arm-dsc21.S
 *
 * Derived from head-arm-atmel.S and Boot.asm
 *
 * Copyright (C) 2001 RidgeRun, Inc.
 * Author: RidgeRun, Inc.
 *          stevej@ridgerun.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/hardware.h>        
#include <asm/mach-types.h>
	                                
/*
        This is the define to put us into service mode.
*/
#define ARM_MODE_SVC 0x13

/*
        We need a place for our stack.  For now, let's put it at the end of RAM.
*/
#define IRAM_END 0x8000
                        
        .text
        .align

        .globl	__entry,_entry
        .globl	__stext,_stext
        .globl _start,start   

start:
_start:
_stext:
_entry:             
__stext:
__entry:
ENTRY(stext)

        /*  turn off IRQ/FIQ and put into service mode, then define top of stack */
        mov	r0, #(ARM_MODE_SVC | I_BIT | F_BIT )
        msr	cpsr, r0

        /* don't bother to set stack here.  It will get set after sdram_ini
        ldr r13, =IRAM_END
        */
        
        /*  Initialize the various clock(pll) registers to TI values */
        
        bl clock_ini

        @ wait for a while (for SDRAM)
        mov	r1, #0xFF00
wloop:	subs r1, r1, #1
        bne	wloop

        /*  Initialize the SDRAM controller so we can talk with the 16MB SDRAM */

        bl sdram_ini

#define MEMORY_CLEAR        
#ifdef MEMORY_CLEAR
        /*  Write memory test to IRAM */
        ldr r8, =0x8000
        ldr r5, =0x0
        ldr r4, MEMTEST_DATA
1:      cmp r5, r8 
        strcc r4, [r5], #4
        bcc 1b       
        
        /*  Write memory test to SDRAM */
        ldr r8, =0x8200000
        ldr r5, =0x8000000
        ldr r4, MEMTEST_DATA
1:      cmp r5, r8 
        strcc r4, [r5], #4
        bcc 1b       
        
#endif
        
        adr	r5, LC0
        ldmia	r5, {r5, r6, r8, r9, sp}			@ Setup stack

        /*  Copy data sections to their new home.  */


        /*  Clear BSS */
        mov	r4, #0
1:      cmp	r5, r8					
        strcc	r4, [r5],#4
        bcc	1b

        /*  Put initial values into stack.  This would normally be done
        by sched_init() in kernel/sched.c, but that would overwrite the
        stack we're already using.  That would be bad.
        */
        mov r5, sp
        sub r5, r5, #0x2000
        ldr r4, L_STACK_MAGIC
        str r4, [r5], #4
        ldr r4, L_STACK_UNTOUCHED_MAGIC
1:      cmp r5, sp
        strcc r4, [r5], #4
        bcc 1b

        /*  Pretend we know what our processor code is (for arm_id)   */
	ldr     r2, =0x41007000
	str     r2, [r6]
	mov     r2, #MACH_TYPE_DSC21
	str     r2, [r9]

        mov fp, #0
        b	start_kernel
        
LC0:	.long	__bss_start
        .long	processor_id
        .long	_end
	.long   __machine_arch_type
        .long	init_task_union+8192

/*
        Local functions.
*/
        
        .macro regw, reg_add, reg_data
        ldr r0, \reg_add
        ldr r1, \reg_data
        strh r1, [r0]       
        .endm

        
                
/*******************************************************************
;* CLOCK controller initialize
;*******************************************************************/
clock_ini:
	regw	CLOCKC_PLLARM, PLLARM_DATA
	regw	CLOCKC_ARMCLK1, ARMCLK_DATA1
	regw	CLOCKC_ARMCLK2, ARMCLK_DATA2
	regw	CLOCKC_ARMCLK3, ARMCLK_DATA3
	regw	CLOCKC_PLLDSP, PLLDSP_DATA
	regw	CLOCKC_DSPCLK, DSPCLK_DATA
	regw	CLOCKC_PLLSDR, PLLSDR_DATA
	regw	CLOCKC_SDRCLK, SDRCLK_DATA
	regw	CLOCKC_ENCCLK, ENCCLK_DATA
	regw	CLOCKC_USBCLK, USBCLK_DATA
	regw	CLOCKC_CCDCLK, CCDCLK_DATA

	mov		pc, lr
        
        
/*******************************************************************
         SDRAM initialize  (and SDRAM precharge)

        Obviously, there's a lot of redundant work
        being done here.  Reloading r0 and r1 with values they
        already have looks silly.  But I don't know if the
        precharge of the RAM will work if I hammer it 8
        times in a row without the intervening reads.  So
        I'll keep TI's timing.
*******************************************************************/
sdram_ini:
	regw	SDRAMC_MODE, SDMODE_DATA
	regw	SDRAMC_REFCTL, SDREF_DATA
	regw	SDRAMC_MODE, SDCNT_DATA1
	regw	SDRAMC_MODE, SDCNT_DATA2
	regw	SDRAMC_MODE, SDCNT_DATA2
	regw	SDRAMC_MODE, SDCNT_DATA2
	regw	SDRAMC_MODE, SDCNT_DATA2
	regw	SDRAMC_MODE, SDCNT_DATA2
	regw	SDRAMC_MODE, SDCNT_DATA2
	regw	SDRAMC_MODE, SDCNT_DATA2
	regw	SDRAMC_MODE, SDCNT_DATA2
	regw	SDRAMC_MODE, SDCNT_DATA3
	mov		pc, lr


CLOCKC_PLLARM:    .word	0x30A80
CLOCKC_ARMCLK1: 	.word	0x30A82
CLOCKC_ARMCLK2: 	.word	0x30A84
CLOCKC_ARMCLK3: 	.word	0x30A86
CLOCKC_PLLDSP:  	.word	0x30A88
CLOCKC_DSPCLK:  	.word	0x30A8A
CLOCKC_PLLSDR:  	.word	0x30A8C
CLOCKC_SDRCLK:  	.word	0x30A8E
CLOCKC_ENCCLK:  	.word	0x30A90
CLOCKC_USBCLK:  	.word	0x30A92
CLOCKC_CCDCLK:  	.word	0x30A94

PLLARM_DATA:   		.word	0x2F80			@ ARM CLK is 37.125MHz
ARMCLK_DATA1:   	.word	0x7300
ARMCLK_DATA2:   	.word	0x7E1F
ARMCLK_DATA3:   	.word	0x3C78
PLLDSP_DATA:   		.word	0x0C00			@ DSP CLK is 81MHz
DSPCLK_DATA:   		.word	0x0607
PLLSDR_DATA:   		.word	0x0800			@ SDRAM CLK is 54MHz
SDRCLK_DATA:   		.word	0x017E
ENCCLK_DATA:   		.word	0x475F
USBCLK_DATA:   		.word	0x0003
CCDCLK_DATA:   		.word	0x0087

        
SDRAMC_MODE:      .word	0x309A6
SDRAMC_REFCTL:    .word	0x309A8
SDMODE_DATA:      .word	0x0500
SDREF_DATA:       .word	0x0140
SDCNT_DATA1:      .word	0x0502
SDCNT_DATA2:      .word	0x0504
SDCNT_DATA3:      .word	0x0501

MEMTEST_DATA:   .long 0xa5a5a5a5

        /*  These values should stay in sync with linux/kernel.h */
        
L_STACK_MAGIC:  .long 0xdeadbeef
L_STACK_UNTOUCHED_MAGIC:        .long 0xfeef1ef0
