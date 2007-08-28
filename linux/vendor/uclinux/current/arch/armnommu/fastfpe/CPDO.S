/*
Inside the emulator the FP numbers are kept with 32 bit accuracy for both
mantissa and exponent. The FP structure has 4 words reserved for each
register, the first is used just for the sign in bit 31, the second is the
mantissa (unsigned integer) and the third is the exponent (signed integer).

The functions do actually only work properly for normalized values, and if
no overflow occurs. Hopfully most programs are not disturbed by this, and it
will probably be improved in future versions.

The parameters to these functions are r0=destination pointer, r1 and r2
source pointers. r4 is the instruction. They may use r0-r7. The return address
is in r14, except CPDO_rnf_core which expects the return address in r5 to
save memory accesses.
*/

/*---------------------------------------------------------------------------*/

	.globl	CPDO_adf
CPDO_adf:
	ldmia	r1,{r1,r3,r5}
	ldmia	r2,{r2,r4,r6}
	cmp	r1,r2
	bne	CPDO_suf_s

CPDO_adf_s:
	subs	r2,r5,r6
	bge	CPDO_adf_1
	
	rsb	r2,r2,#0
	mov	r5,r6
	adds	r3,r4,r3,lsr r2
	b	CPDO_adf_2
	
CPDO_adf_1:
	adds	r3,r3,r4,lsr r2

CPDO_adf_2:
	addcs	r5,r5,#1
	movcss	r3,r3,rrx
	beq	CPDO_zero
	stmia	r0,{r1,r3,r5}
	
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPDO_suf
CPDO_suf:
	ldmia	r1,{r1,r3,r5}
	ldmia	r2,{r2,r4,r6}

CPDO_suf_l:
	cmp	r1,r2
	bne	CPDO_adf_s

CPDO_suf_s:	
	subs	r2,r5,r6
	bge	CPDO_suf_1
	
	rsb	r2,r2,#0
	mov	r5,r6
	rsbs	r3,r4,r3,lsr r2
	b	CPDO_suf_2
	
CPDO_suf_1:
	subs	r3,r3,r4,lsr r2
	
CPDO_suf_2:
        beq	CPDO_zero
	
	eorcc	r1,r1,#0x80000000
	rsbcc	r3,r3,#0
	
	cmp	r3,#0x00010000
	movcc	r3,r3,lsl#16
	subcc	r5,r5,#16
	
	cmp	r3,#0x01000000
	movcc	r3,r3,lsl#8
	subcc	r5,r5,#8
	
	cmp	r3,#0x10000000
	movcc	r3,r3,lsl#4
	subcc	r5,r5,#4
	
	cmp	r3,#0x40000000
	movcc	r3,r3,lsl#2
	subcc	r5,r5,#2
	
	cmp	r3,#0x80000000
	movcc	r3,r3,lsl#1
	subcc	r5,r5,#1
	
	stmia	r0,{r1,r3,r5}
	
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl CPDO_rsf
CPDO_rsf:
	mov	r3,r2
	ldmia	r1,{r2,r4,r6}
	ldmia	r3,{r1,r3,r5}
	b	CPDO_suf_l
	
/*---------------------------------------------------------------------------*/

	.globl	CPDO_muf
CPDO_muf:
	ldmia	r1,{r1,r3,r5}
	ldmia	r2,{r2,r4,r6}
	
	eor	r1,r1,r2
	add	r6,r5,r6
	umulls	r2,r5,r4,r3
	beq	CPDO_zero
	bpl	CPDO_muf_norm
	
	add	r6,r6,#1
	stmia	r0,{r1,r5,r6}
	mov	pc,r14

CPDO_muf_norm:
	adds	r2,r2,r2
	adcs	r5,r5,r5
	
	stmia	r0,{r1,r5,r6}
	mov	pc,r14

/*---------------------------------------------------------------------------*/
/* Divison ignores the LSB in both mantissa, but needs only ~110 cycles.     */

	.globl	CPDO_dvf
CPDO_dvf:
	ldmia	r1,{r1,r3,r5}
	ldmia	r2,{r2,r4,r6}

CPDO_dvf_l:
	eor	r1,r1,r2
	sub	r6,r5,r6

	movs	r3,r3,lsr#1
	beq	CPDO_zero
	mov	r4,r4,lsr#1
	rsb	r4,r4,#0
	
	.macro	div_step
	adcs	r3,r4,r3,lsl#1
	subcc	r3,r3,r4
	adc	r5,r5,r5
	.endm
	
	adds	r3,r4,r3
	adc	r5,r5,r5
	bcs	CPDO_dvf_b

	subcc	r3,r3,r4
	add	r3,r4,r3,lsl#1
	mov	r5,#1
	sub	r6,r6,#1
	
CPDO_dvf_b:	
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	div_step
	
CPDO_dvf_e:	
	stmia	r0,{r1,r5,r6}
	mov	pc,r14
	
CPDO_zero:
	mov	r1,#0
	mov	r2,#0
	mov	r3,#0x80000000
	stmia	r0,{r1-r3}
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPDO_rdf
CPDO_rdf:
	mov	r3,r2
	ldmia	r1,{r2,r4,r6}
	ldmia	r3,{r1,r3,r5}
	b	CPDO_dvf_l

/*---------------------------------------------------------------------------*/

	.globl	CPDO_rmf
CPDO_rmf:
	mov	pc,r14
	
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/

	.globl	CPDO_mvf
CPDO_mvf:
	ldmia	r2,{r1,r2,r3}
	stmia	r0,{r1,r2,r3}
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPDO_mnf
CPDO_mnf:
	ldmia	r2,{r1,r2,r3}
	eor	r1,r1,#0x80000000
	stmia	r0,{r1,r2,r3}
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPDO_abs
CPDO_abs:
	ldmia	r2,{r1,r2,r3}
	bic	r1,r1,#0x80000000
	stmia	r0,{r1,r2,r3}
	mov	pc,r14

/*---------------------------------------------------------------------------*/
	
	.globl	CPDO_sqt
CPDO_sqt:
	ldmia	r2,{r1,r2,r4}
	
	and	r5,r4,#1
	movs	r2,r2,lsl r5
	
	mov	r3,#0x80000000
	sub	r2,r2,#0x80000000

	.macro sqrt_step,N
	add	r5,r3,#(0x40000000>>\N)
	cmpcc	r2,r5
	addcs	r3,r3,#(0x80000000>>\N)
	subcs	r2,r2,r5
	movs	r2,r2,lsl#1
	.endm
	
	sqrt_step 1
	sqrt_step 2
	sqrt_step 3
	sqrt_step 4
	sqrt_step 5
	sqrt_step 6
	sqrt_step 7
	sqrt_step 8
	sqrt_step 9
	sqrt_step 10
	sqrt_step 11
	sqrt_step 12
	sqrt_step 13
	sqrt_step 14
	sqrt_step 15
	sqrt_step 16
	sqrt_step 17
	sqrt_step 18
	sqrt_step 19
	sqrt_step 20
	sqrt_step 21
	sqrt_step 22
	sqrt_step 23
	sqrt_step 24
	sqrt_step 25
	sqrt_step 26
	sqrt_step 27
	sqrt_step 28
	sqrt_step 29
	sqrt_step 30 
	sqrt_step 31 
	
	mov	r4,r4,asr#1
	stmia	r0,{r1,r3,r4}
	mov	pc,r14

/*---------------------------------------------------------------------------*/
	
	.globl	CPDO_rnd
CPDO_rnd:
	adr	r5,CPDO_rnd_store
	b	CPDO_rnd_core

CPDO_rnd_store:
	stmia	r0,{r1,r2,r3}
    	mov	pc,r14
	
/*---------------------------------------------------------------------------*/

	.globl	CPDO_rnd_core
CPDO_rnd_core:
	ldmia	r2,{r1,r2,r3}
	and	r4,r4,#0x00000060
	add	pc,pc,r4,lsr#3
	mov	r0,r0
	b	CPDO_rnd_N
	b	CPDO_rnd_P
	b	CPDO_rnd_M
	b	CPDO_rnd_Z
	
CPDO_rnd_N:
	cmp	r3,#-1
	blt	CPDO_rnd_zero
	cmp	r3,#31
	bge	CPDO_rnd_end
	
	rsb	r4,r3,#30
	mov	r2,r2,lsr r4
	add	r2,r2,#1
	bic	r2,r2,#1
	movs	r2,r2,lsl r4
	addcs	r3,r3,#1
	movcs	r2,r2,rrx
	
	mov	pc,r5

CPDO_rnd_P:
	cmp	r3,#0
	blt	CPDO_rnd_P_small
	cmp	r3,#31
	movge	pc,r5
	
	tst	r1,#0x80000000
	bne	CPDO_rnd_end
	
	mov	r4,#0x80000000
	sub	r4,r4,#1
	adds	r2,r2,r4,lsr r3
	addcs	r3,r3,#1
	movcs	r2,r2,rrx

	b	CPDO_rnd_end
	
CPDO_rnd_P_small:
	cmp	r2,#0	
	beq	CPDO_rnd_zero
	tst	r1,#0x80000000
	bne	CPDO_rnd_zero
	b	CPDO_rnd_one
	
	
CPDO_rnd_M:
	cmp	r3,#0
	blt	CPDO_rnd_M_small
	cmp	r3,#31
	movge	pc,r5
	
	tst	r1,#0x80000000
	beq	CPDO_rnd_end
	
	mov	r4,#0x80000000
	sub	r4,r4,#1
	adds	r2,r2,r4,lsr r3
	addcs	r3,r3,#1
	movcs	r2,r2,rrx

	b	CPDO_rnd_end
	
CPDO_rnd_M_small:
	cmp	r2,#0	
	beq	CPDO_rnd_zero
	tst	r1,#0x80000000
	beq	CPDO_rnd_zero
	b	CPDO_rnd_one
	
CPDO_rnd_Z:
	cmp	r3,#0
	blt	CPDO_rnd_zero
	cmp	r3,#31
	movge	pc,r5
	
	b	CPDO_rnd_end

CPDO_rnd_one:
	mov	r2,#0x80000000
	mov	r3,#0
	mov	pc,r5
	
CPDO_rnd_zero:
	mov	r1,#0
	mov	r2,#0
	mov	r3,#0x80000000
	mov	pc,r5

CPDO_rnd_end:
	rsb	r4,r3,#31
	mov	r2,r2,lsr r4
	mov	r2,r2,lsl r4
	mov	pc,r5
	
/*---------------------------------------------------------------------------*/
