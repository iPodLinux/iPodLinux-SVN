/*---------------------------------------------------------------------------*/

	.text
	.globl	CPRT_flt
CPRT_flt:
	add	r0,r13,r0,lsr#10
	ldr	r3,[r0]
	cmp	r3,#0
	beq	CPRT_zero
	
	ands	r2,r3,#0x80000000
	rsbne	r3,r3,#0
	mov	r4,#31
	
	cmp	r3,#0x00010000
	movcc	r3,r3,lsl#16
	subcc	r4,r4,#16
	
	cmp	r3,#0x01000000
	movcc	r3,r3,lsl#8
	subcc	r4,r4,#8
	
	cmp	r3,#0x10000000
	movcc	r3,r3,lsl#4
	subcc	r4,r4,#4
	
	cmp	r3,#0x40000000
	movcc	r3,r3,lsl#2
	subcc	r4,r4,#2
	
	cmp	r3,#0x80000000
	movcc	r3,r3,lsl#1
	subcc	r4,r4,#1

	stmia	r1,{r2,r3,r4}
	mov	pc,r14

CPRT_zero:
	mov	r2,#0
	mov	r3,#0
	mov	r4,#0x80000000
	stmia	r1,{r2,r3,r4}
	mov	pc,r14
	
/*---------------------------------------------------------------------------*/

	.globl	CPRT_fix
CPRT_fix:
	adr	r5,CPRT_back
	b	CPDO_rnd_core
	
CPRT_back:
	add	r0,r13,r0,lsr#10
	cmp	r3,#0
	blt	CPRT_int_zero
	cmp	r3,#30
	bgt	CPRT_overflow
	
	rsb	r3,r3,#31
	mov	r2,r2,lsr r3
	tst	r1,#0x80000000
	rsbne	r2,r2,#0
	
	str	r2,[r0]
	mov	pc,r14

CPRT_int_zero:
	mov	r2,#0
	str	r2,[r0]
	mov	pc,r14

CPRT_overflow:
	mov	r2,#0x80000000
	tst	r1,#0x80000000
	subeq	r2,r2,#1
	str	r2,[r0]
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPRT_wfs
CPRT_wfs:
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPRT_rfs
CPRT_rfs:
	add	r0,r13,r0,lsr#10
	mov	r1,#0x02000000		@ Software Emulation, not Acorn FPE
	str	r1,[r0]
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPRT_cmf
CPRT_cmf:
	ldmia	r1,{r1,r3,r5}
	ldmia	r2,{r2,r4,r6}

CPRT_cmf_e:
	ldr	r0,[r13,#16*4]
	bic	r0,r0,#0xf0000000
	
	cmp	r1,r2
	beq	CPRT_cmf_equalsign
	b	CPRT_cmf_sign

CPRT_cmf_equalsign:
	cmp	r5,r6
	beq	CPRT_cmf_equalexponent
	bgt	CPRT_cmf_sign
	b	CPRT_cmf_signb	

CPRT_cmf_equalexponent:
	cmp	r3,r4
	beq	CPRT_cmf_equal
	bgt	CPRT_cmf_sign
	b	CPRT_cmf_signb

CPRT_cmf_sign:
	tst	r1,#0x80000000
	orreq	r0,r0,#0x20000000
	orrne	r0,r0,#0x80000000
	str	r0,[r13,#16*4]
	mov	pc,r14

CPRT_cmf_signb:
	tst	r1,#0x80000000
	orrne	r0,r0,#0x20000000
	orreq	r0,r0,#0x80000000
	str	r0,[r13,#16*4] 
	mov	pc,r14

CPRT_cmf_equal:
	orr	r0,r0,#0x60000000
	str	r0,[r13,#16*4]
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPRT_cnf
CPRT_cnf:
	ldmia	r1,{r1,r3,r5}
	ldmia	r2,{r2,r4,r6}
	eor	r2,r2,#0x80000000
	b	CPRT_cmf_e

/*---------------------------------------------------------------------------*/
