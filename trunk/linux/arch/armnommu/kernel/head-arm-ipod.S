
#include <linux/config.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

	.equ	C_PROCESSOR_ID, 0xc4000000
	.equ	C_CPU_SLEEP, 0xca
	.equ	C_CPU_ID, 0x55
	.equ	C_CFG_COPROC, 0xcf004058

/*
 * Reset condition :
 *      R14_svc         = unknown
 *      SPSR_svc        = CPSR
 *      CPSR            = 0b1101_0011 ; not IRQ, no FIQ, SVC mode
 *      PC              = 0x0
 */
	.section ".text.init",#alloc,#execinstr
	.type   stext, #function
ENTRY(stext)
	mov     r0, #0xd3
	msr     CPSR_c, r0

.ifdef NOTDEF
	ldr	r1, L_0xcf00502c
	mov	r0, #0
	str	r0, [r1]

	ldr	r1, L_0xcf004024
	mov	r0, #0
	str	r0, [r1]

	ldr	r1, L_0xcf005000
	ldr	r0, L_0xb9f
	str	r0, [r1]

	ldr	r1, L_0xcf005004
	mov	r0, #0x35
	str	r0, [r1]
.endif

	/* clear and mask all interrupts */
	ldr	r1, L_0xcf00101c
	mov	r0, #-1
	str	r0, [r1]

	ldr	r1, L_0xcf001028
	mov	r0, #-1
	str	r0, [r1]

	ldr	r1, L_0xcf001038
	mov	r0, #-1
	str	r0, [r1]

	/* clear timers */
	ldr	r1, L_0xcf001100
	mov	r0, #0
	str	r0, [r1]

	ldr	r1, L_0xcf001104
	ldr	r1, [r1]

	ldr	r1, L_0xcf001108
	mov	r0, #0
	str	r0, [r1]

	ldr	r1, L_0xcf00110c
	ldr	r1, [r1]

	/* setup memory mapping */
	ldr	r1, C_0x40010000
	ldr	r2, L_install_reloc_start
	ldr	r3, L_install_reloc_end

	/* copy the code to 0x40010000 */
1:
	ldr	r4, [r2], #4
	str	r4, [r1], #4
	cmp	r2, r3
	ble	1b

	ldr	r3, C_0x28003f84
	ldr	r2, C_0xf000f014
	mov	r1, #0x3b00
	ldr	r0, C_0xf000f010
	ldr	r4, C_0x40010000
	mov	pc, r4

install_reloc_start:
	str	r1, [r0]
	str	r3, [r2]
	ldr	r0, L_post_reloc_start
	mov	pc, r0

L_post_reloc_start:	.word post_reloc_start

install_reloc_end:

post_reloc_start:
	/* check if we are cpu or co-processor */
	mov	r0, #C_PROCESSOR_ID
	ldr	r0, [r0]
	and	r0, r0, #0xff
	cmp	r0, #C_CPU_ID
	beq	cpu_startup

	/* we are the co-processor, go to sleep until cpu wakes us up */
	ldr	r0, L_CFG_COPROC
	mov	r1, #C_CPU_SLEEP
	str	r1, [r0]

	/* set stack pointer for FIQ mode */
	mov	r0, #0xd1
	msr	CPSR_c, r0
	ldr	sp, fiq_sp

	/* set stack pointer for SVC mode */
	mov	r0, #0xd3
	msr	CPSR_c, r0
	ldr	sp, svc_sp

.ifdef NOTDEF
	/* copy our FIQ code to 0x4000000 */
	mov	r1, #0x40000000
	ldr	r2, L_cop_start
	ldr	r3, L_cop_end

	/* copy the code to 0x40000000 */
1:
	ldr	r4, [r2], #4
	str	r4, [r1], #4
	cmp	r2, r3
	ble	1b
.endif

.ifdef NOTDEF
	/* install our FIQ handler */
	mov	r1, #0x1c
	ldr	r2, cop_fiq_vec
	ldr	r2, [r2]
	str	r2, [r1]
.endif

	bl	ipod_init_cache

	/* jump to our idle loop */
	b	cop_idle

cop_fiq_vec:
	mov	pc, #0x40000000

svc_sp:		.word cop_svc_stack
fiq_sp:		.word cop_fiq_stack

L_cop_start:	.word cop_fiq
L_cop_end:	.word cop_end

L_0xcf00502c:	.word 0xcf00502c
L_0xcf004024:	.word 0xcf004024
L_0xcf005000:	.word 0xcf005000
L_0xb9f:	.word 0xb9f
L_0xcf005004:	.word 0xcf005004
L_0xcf00101c:	.word 0xcf00101c
L_0xcf001028:	.word 0xcf001028
L_0xcf001038:	.word 0xcf001038
L_0xcf001100:	.word 0xcf001100
L_0xcf001104:	.word 0xcf001104
L_0xcf001108:	.word 0xcf001108
L_0xcf00110c:	.word 0xcf00110c

L_0xcf00000c:	.word 0xcf00000c
L_0xcf004040:	.word 0xcf004040

cpu_startup:
	/* enable ttyS0 (remote) */
	/* outl(inl(0xcf00000c) & ~0x8, 0xcf00000c); */
	ldr	r1, L_0xcf00000c
	ldr	r0, [r1]
	bic	r0, r0, #0x8
	str	r0, [r1]

	/* enable ttyS1 (piezo) */
	/* outl(0xcf004040, inl(0xcf004040) & ~0x1000); */
	ldr	r1, L_0xcf004040
	ldr	r0, [r1]
	bic	r0, r0, #0x1000
	str	r0, [r1]

	/* get important "magic" values */
	adr	r5, LC0
	ldmia	r5, {r5, r6, r8, r9, sp}

	/* initialise bss */
	mov	r4, #0
1:
	cmp	r5, r8
	strcc	r4, [r5], #4
	bcc	1b

	bl	ipod_init_cache
	bl	ipod_set_cpu_speed
	bl	ipod_set_sys_info

	/* put initial values into stack?? */
	mov	r5, sp
	sub	r5, r5, #0x1000
	ldr	r4, L_STACK_MAGIC
	str	r4, [r5], #4
	ldr	r4, L_STACK_UNTOUCHED_MAGIC
1:
	cmp	r5, sp
	strcc	r4, [r5], #4
	bcc	1b

	/* hard code the ARMID */
	ldr	r2, L_ARMID
	str	r2, [r6]

	/* hard code the processor code */
	mov	r2, #MACH_TYPE_IPOD
	str	r2, [r9]

	/* jump to the kernel */
	mov	fp, #0x0
	b	SYMBOL_NAME(start_kernel)

/*  These values should stay in sync with linux/kernel.h */
L_STACK_MAGIC:
		.long 0xdeadbeef
L_STACK_UNTOUCHED_MAGIC:
		.long 0xfeef1ef0

L_ARMID:	.long 0x41007700


C_0x40010000:	.word 0x40010000

L_install_reloc_start:	.word install_reloc_start
L_install_reloc_end:	.word install_reloc_end

C_0x28003f84:	.word 0x28003f84
C_0xf000f014:	.word 0xf000f014
C_0xf000f010:	.word 0xf000f010

L_CFG_COPROC:	.word C_CFG_COPROC

	.align 8
LC0:	.long	SYMBOL_NAME(__bss_start)
	.long	SYMBOL_NAME(processor_id)
	.long	SYMBOL_NAME(_end)
	.long	SYMBOL_NAME(__machine_arch_type)
	.long	SYMBOL_NAME(init_task_union)+8192

#ifdef CONFIG_BLK_DEV_RAM
	.global __ramdisk_data
	.global __ramdisk_data_end

	.align 12
__ramdisk_data:
	.incbin "../../../initrd"
__ramdisk_data_end:
	.align

#endif

