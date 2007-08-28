/*
At entry the registers contain the following information:

r14	return address for undefined exception return
r9	return address for return from exception
r13	user registers on stack, offset 0 up to offset 4*15 contains
	registers r0..15, then the psr
r10	FP workspace 35 words (init, reg[8][4], fpsr, fpcr)
 
*/

/*---------------------------------------------------------------------------*/

	.data
fp_const:
	.word	0, 0x00000000, 0, 0x80000000	@ 0
	.word	0, 0x80000000, 0,          0	@ 1
	.word	0, 0x80000000, 0,          1	@ 2
	.word	0, 0xc0000000, 0,          1	@ 3
	.word	0, 0x80000000, 0,          2	@ 4
	.word	0, 0xa0000000, 0,          2	@ 5
	.word	0, 0x80000000, 0,	  -1	@ 0.5
	.word	0, 0xa0000000, 0,          3	@ 10
fp_undef:
	.word	0
fp_cond:
	.word	0xf0f0	@ eq
	.word	0x0f0f	@ ne
	.word	0xcccc	@ cs
	.word	0x3333	@ cc
	.word	0xff00	@ mi
	.word	0x00ff	@ pl
	.word	0xaaaa	@ vs
	.word	0x5555	@ vc
	.word	0x0c0c	@ hi
	.word	0xf3f3	@ ls
	.word	0xaa55	@ ge
	.word	0x55aa	@ lt
	.word	0x0a05	@ gt
	.word	0xf5fa	@ le
	.word	0xffff	@ al
	.word	0x0000	@ nv
	
/*---------------------------------------------------------------------------*/
	
	.text
	.globl	fastfpe_enter
fastfpe_enter:
	ldr	r4,=fp_undef
	str	r14,[r4]		@ to free one register
	add	r10,r10,#4		@ to make the code simpler
	ldr	r4,[r13,#60]		@ r4=saved PC
	ldr	r4,[r4,#-4]		@ r4=trapped instruction
	and	r1,r4,#0x00000f00	@ r1=coprocessor << 8
next_enter:
	cmp	r1,#1<<8		@ copro 1 ?
	beq	copro_1
	cmp	r1,#2<<8
	movne	pc,r14

copro_2:
	and	r1,r4,#0x0f000000
	cmp	r1,#0x0c000000          @ CPDT with post indexing
        cmpne   r1,#0x0d000000          @ CPDT with pre indexing
        beq     CPDT_M_enter
	mov	pc,r14

copro_1:
	and	r1,r4,#0x0f000000
	cmp	r1,#0x0e000000		@ CPDO
	beq	CPDO_CPRT_enter
	cmp	r1,#0x0c000000		@ CPDT with post indexing
	cmpne	r1,#0x0d000000		@ CPDT with pre indexing
	beq	CPDT_1_enter
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	fastfpe_next
fastfpe_next:
	ldr	r5,[r13,#60]
next_after_cond:
__x1:
	ldrt	r4,[r5],#4
	
	ldr	r0,=fp_cond		@ check condition of next instruction
	ldr	r1,[r13,#64]		@ psr containing flags
	mov	r2,r4,lsr#28
	mov	r1,r1,lsr#28
	ldr	r0,[r0,r2,lsl#2]
	mov	r0,r0,lsr r1
	tst	r0,#1
	beq	next_after_cond		@ must not necessarily have been an
					@ FP instruction !
	and	r1,r4,#0x0f000000	@ Test for copro instruction
	cmp	r1,#0x0c000000
	rsbgts	r0,r1,#0x0e000000	@ cmpgt #0x0e000000,r1
	movlt	pc,r9			@ next is no copro instruction, return
	
	ands	r1,r4,#0x00000f00	@ r1 = coprocessor << 8
	cmpne	r1,#3<<8
	movge	pc,r9			@ copro = 0 or >=3, return
		
	str	r5,[r13,#60]		@ save updated pc
	b	next_enter

/*---------------------------------------------------------------------------*/

undefined:
	ldr	r4,=fp_undef
	ldr	pc,[r4]	

/*---------------------------------------------------------------------------*/

CPDT_1_enter:
	and	r5,r4,#0x000f0000	@ r5=base register number << 16
	ldr	r6,[r13,r5,lsr#14]	@ r6=base address
	cmp	r5,#0x000f0000		@ base register = pc ?
	addeq	r6,r6,#4
	and	r7,r4,#0x000000ff	@ r7=offset value

	tst	r4,#0x00800000		@ up or down?
	addne	r7,r6,r7,lsl#2
	subeq	r7,r6,r7,lsl#2		@ r6=base address +/- offset
	tst	r4,#0x01000000		@ preindexing ?
	movne	r6,r7
	tst	r4,#0x00200000		@ write back ?
	cmpne	r5,#0x000f0000		@ base register = pc ?
	strne	r7,[r13,r5,lsr#14]

	and	r0,r4,#0x00007000	@ r0=fp register number << 12
	add	r0,r10,r0,lsr#8		@ r0=address of fp register
	mov	r1,#0
	tst	r4,#0x00008000
	orrne	r1,r1,#1		@ T0
	tst	r4,#0x00400000
	orrne	r1,r1,#2		@ T1
	tst	r4,#0x00100000
	orrne	r1,r1,#4		@ L/S

	add	pc,pc,r1,lsl#2
	mov	r0,r0
	b	CPDT_store_single	@ these functions get 
	b	CPDT_store_double	@ r0=address of fp register
	b	CPDT_store_extended	@ r6=address of data
	b	undefined		@ CPDT_store_decimal
        b       CPDT_load_single
        b       CPDT_load_double
        b       CPDT_load_extended
        b       undefined		@ CPDT_load_decimal

/*---------------------------------------------------------------------------*/

CPDT_M_enter:
	and	r5,r4,#0x000f0000	@ r5=base register number << 16
	ldr	r6,[r13,r5,lsr#14]	@ r6=base address
	cmp	r5,#0x000f0000		@ base register = pc ?
	addeq	r6,r6,#4
	and	r7,r4,#0x000000ff	@ r7=offset value

	tst	r4,#0x00800000		@ up or down?
	addne	r7,r6,r7,lsl#2
	subeq	r7,r6,r7,lsl#2		@ r7=base address +/- offset
	tst	r4,#0x01000000		@ preindexing ?
	movne	r6,r7
	tst	r4,#0x00200000		@ write back ?
	cmpne	r5,#0x000f0000		@ base register = pc ?
	strne	r7,[r13,r5,lsr#14]

	and	r0,r4,#0x00007000	@ r0=fp register number << 12
	and	r1,r4,#0x00008000
	mov	r1,r1,lsr#15		@ N0
	and	r2,r4,#0x00400000
	orrs	r1,r1,r2,lsr#21		@ N1
	addeq	r1,r1,#4		@ r1=register count

	tst	r4,#0x00100000		@ load/store
	beq	CPDT_sfm
	b	CPDT_lfm

/*---------------------------------------------------------------------------*/

CPDO_CPRT_enter:
	tst	r4,#0x00000010
	bne	CPRT_enter
	
	and	r0,r4,#0x00007000
	add	r0,r10,r0,lsr#8		@ r0=address of Fd
	and	r1,r4,#0x00070000
	add	r1,r10,r1,lsr#12	@ r1=address of Fn
	tst	r4,#0x00000008
	bne	CPDO_const
	and	r2,r4,#0x00000007
	add	r2,r10,r2,lsl#4		@ r2=address of Fm
	
CPDO_constback:
	and	r3,r4,#0x00f00000
	tst	r4,#0x00008000
	orrne	r3,r3,#0x01000000
		
	add	pc,pc,r3,lsr#18
	mov	r0,r0
	b	CPDO_adf
	b	CPDO_muf
	b	CPDO_suf
	b	CPDO_rsf
	b	CPDO_dvf
	b	CPDO_rdf
	b	undefined
	b	undefined
	b	undefined		@ CPDO_rmf
	b	CPDO_muf
	b	CPDO_dvf
	b	CPDO_rdf
	b	undefined
	b	undefined
	b	undefined
	b	undefined
	b	CPDO_mvf
	b	CPDO_mnf
	b	CPDO_abs
	b	CPDO_rnd
	b	CPDO_sqt
	b	undefined
	b	undefined
	b	undefined
	b	undefined
	b	undefined
	b	undefined
	b	undefined
	b	undefined
	b	undefined
	b	CPDO_rnd
	b	fastfpe_next

CPDO_const:
	ldr	r2,=fp_const
	and	r3,r4,#0x00000007
	add	r2,r2,r3,lsl#4	
	b	CPDO_constback
	
/*---------------------------------------------------------------------------*/

CPRT_enter:
	and	r0,r4,#0x0000f000	@ r0=Rd<<12
	and	r1,r4,#0x00070000
	add	r1,r10,r1,lsr#12	@ r1=address of Fn
	tst	r4,#0x00000008
	bne	CPRT_const
	and	r2,r4,#0x00000007
	add	r2,r10,r2,lsl#4		@ r2=address of Fm
	
CPRT_constback:
	and	r3,r4,#0x00f00000
		
	add	pc,pc,r3,lsr#18
	mov	r0,r0
	b	CPRT_flt
	b	CPRT_fix
	b	CPRT_wfs
	b	CPRT_rfs
	b	undefined
	b	undefined
	b	undefined
	b	undefined
	b	undefined
	b	CPRT_cmf
	b	undefined
	b	CPRT_cnf
	b	undefined
	b	CPRT_cmf
	b	undefined
	b	CPRT_cnf

CPRT_const:
	ldr	r2,=fp_const
	and	r3,r4,#0x00000007
	add	r2,r2,r3,lsl#4	
	b	CPRT_constback
	
/*---------------------------------------------------------------------------*/

	@ The fetch of the next instruction to emulate could fault

	.section .fixup,"ax"
	.align
__f1:
	mov	pc,r9
	.previous
	.section __ex_table,"a"
	.align 3
	.long	__x1,__f1
	.previous
	
/*---------------------------------------------------------------------------*/
