/*
The FP structure has 4 words reserved for each register, the first is used
just
for the sign in bit 31, the second and third are for the mantissa (unsigned
integer, high 32 bit first) and the fourth is the exponent (signed integer).
The mantissa is always normalized.

If the exponent is 0x80000000, that is the most negative value, the number
represented is 0 and both mantissa words are also 0.

If the exponent is 0x7fffffff, that is the biggest positive value, the
number
represented is infinity if the high 32 mantissa bit are also 0, otherwise it
is
a NaN. The low 32 mantissa bit are 0 if the number represented is infinity.

Decimal and packed decimal numbers are not supported yet.
*/

/*---------------------------------------------------------------------------*/

	.text
	.globl	CPRT_flt
CPRT_flt:
	add	r0,r13,r0,lsr#10
	ldr	r2,[r0]
	mov	r3,#0
	cmp	r2,#0
	beq	CPRT_flt_zero
	
	ands	r0,r2,#0x80000000
	rsbne	r2,r2,#0
	mov	r4,#31
	
	cmp	r2,#0x00010000
	movcc	r2,r2,lsl#16
	subcc	r4,r4,#16
	
	cmp	r2,#0x01000000
	movcc	r2,r2,lsl#8
	subcc	r4,r4,#8
	
	cmp	r2,#0x10000000
	movcc	r2,r2,lsl#4
	subcc	r4,r4,#4
	
	cmp	r2,#0x40000000
	movcc	r2,r2,lsl#2
	subcc	r4,r4,#2
	
	cmp	r2,#0x80000000
	movcc	r2,r2,lsl#1
	subcc	r4,r4,#1

	stmia	r1,{r0,r2,r3,r4}
	b	fastfpe_next

CPRT_flt_zero:
	mov	r0,#0
	mov	r4,#0x80000000
	stmia	r1,{r0,r2,r3,r4}
	b	fastfpe_next
	
/*---------------------------------------------------------------------------*/

	.globl	CPRT_fix
CPRT_fix:
	ldmia	r2,{r1,r2,r3,r5}
	bl	CPDO_rnd_core
	
CPRT_back:
	add	r0,r13,r0,lsr#10
	cmp	r5,#0
	blt	CPRT_int_zero
	cmp	r5,#30
	bgt	CPRT_overflow
	
	rsb	r5,r5,#31
	mov	r2,r2,lsr r5
	tst	r1,#0x80000000
	rsbne	r2,r2,#0
	
	str	r2,[r0]
	b	fastfpe_next

CPRT_int_zero:
	mov	r2,#0
	str	r2,[r0]
	b	fastfpe_next

CPRT_overflow:
	mov	r2,#0x80000000
	tst	r1,#0x80000000
	subeq	r2,r2,#1
	str	r2,[r0]
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPRT_wfs
CPRT_wfs:
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPRT_rfs
CPRT_rfs:
	add	r0,r13,r0,lsr#10
	mov	r1,#0x02000000		@ Software Emulation, not Acorn FPE
	str	r1,[r0]
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPRT_cmf
CPRT_cmf:
	ldmia	r1,{r1,r3,r5,r7}
	ldmia	r2,{r2,r4,r6,r8}

CPRT_cmf_e:
	ldr	r0,[r13,#16*4]
	
	cmp	r7,#0x7fffffff
	bic	r0,r0,#0xf0000000

	cmpeq	r3,#0xffffffff
	beq	CPRT_cmf_unordered
	cmp	r8,#0x7fffffff
	cmpeq	r4,#0xffffffff
	beq	CPRT_cmf_unordered

	cmp	r1,r2
	beq	CPRT_cmf_equalsign
	b	CPRT_cmf_sign

CPRT_cmf_equalsign:
	cmp	r7,r8
	beq	CPRT_cmf_equalexponent
	bgt	CPRT_cmf_sign
	b	CPRT_cmf_signb	

CPRT_cmf_equalexponent:
	cmp	r3,r4
	cmpeq	r5,r6
	beq	CPRT_cmf_equal
	bhi	CPRT_cmf_sign
	b	CPRT_cmf_signb

CPRT_cmf_sign:
	cmp     r7,#0x80000000		@ (0.0 == -0.0)?
	cmpeq   r7,r8
	beq     CPRT_cmf_equal
	tst	r1,#0x80000000
	orreq	r0,r0,#0x20000000
	orrne	r0,r0,#0x80000000
	str	r0,[r13,#16*4]
	b	fastfpe_next

CPRT_cmf_signb:
	tst	r1,#0x80000000
	orrne	r0,r0,#0x20000000
	orreq	r0,r0,#0x80000000
	str	r0,[r13,#16*4] 
	b	fastfpe_next

CPRT_cmf_equal:
	orr	r0,r0,#0x60000000
	str	r0,[r13,#16*4]
	b	fastfpe_next

CPRT_cmf_unordered:
	orr	r0,r0,#0x10000000
        str     r0,[r13,#16*4]
        b       fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPRT_cnf
CPRT_cnf:
	ldmia	r1,{r1,r3,r5,r7}
	ldmia	r2,{r2,r4,r6,r8}
	eor	r2,r2,#0x80000000
	b	CPRT_cmf_e

/*---------------------------------------------------------------------------*/
