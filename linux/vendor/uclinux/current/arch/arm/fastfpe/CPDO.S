/*
The FP structure has 4 words reserved for each register, the first is used just
for the sign in bit 31, the second and third are for the mantissa (unsigned
integer, high 32 bit first) and the fourth is the exponent (signed integer).
The mantissa is always normalized.

If the exponent is 0x80000000, that is the most negative value, the number
represented is 0 and both mantissa words are also 0.

If the exponent is 0x7fffffff, that is the biggest positive value, the number
represented is infinity if the high 32 mantissa bit are also 0, otherwise it is
a NaN. The low 32 mantissa bit are 0 if the number represented is infinity.

Decimal and packed decimal numbers are not supported yet.

The parameters to these functions are r0=destination pointer, r1 and r2
source pointers. r4 is the instruction. They may use r0-r8 and r14. They return
to fastfpe_next, except CPDO_rnf_core which expects the return address in r14.
*/

/*---------------------------------------------------------------------------*/

	.globl	CPDO_adf
CPDO_adf:
	ldmia	r1,{r1,r3,r5,r7}
	ldmia	r2,{r2,r4,r6,r8}

	cmp	r7,#0x7fffffff
	cmpne	r8,#0x7fffffff
	beq	CPDO_adf_extra

	cmp	r1,r2
	bne	CPDO_suf_s

CPDO_adf_s:
	subs	r2,r7,r8
	bge	CPDO_adf_2nd
	
	mov	r7,r8
	rsb	r2,r2,#0
	cmp	r2,#32
	ble	CPDO_adf_1st2

	sub	r2,r2,#32
	cmp	r2,#32
	movgt	r2,#32
	mov	r5,r3,lsr r2
	mov	r3,#0
	b	CPDO_adf_add

CPDO_adf_1st2:
	rsb	r8,r2,#32
	mov	r5,r5,lsr r2
	orr	r5,r5,r3,lsl r8
	mov	r3,r3,lsr r2	@ 1. op normalized
	b	CPDO_adf_add

CPDO_adf_2nd:
	cmp	r2,#32
	ble	CPDO_adf_2nd2

	sub	r2,r2,#32
	cmp	r2,#32
	movgt	r2,#32
	mov	r6,r4,lsr r2
	mov	r4,#0
	b	CPDO_adf_add

CPDO_adf_2nd2:
	rsb	r8,r2,#32
	mov	r6,r6,lsr r2
	orr	r6,r6,r4,lsl r8
	mov	r4,r4,lsr r2	@ 2. op normalized

CPDO_adf_add:
	adds	r5,r5,r6
	adcs	r3,r3,r4	@ do addition
	bcc	CPDO_adf_end

	add	r7,r7,#1
	movs	r3,r3,rrx
	mov	r5,r5,rrx	@ correct for overflow

CPDO_adf_end:
	cmp	r7,#0x20000000
	bge	CPDO_inf

	stmia	r0,{r1,r3,r5,r7}
	b	fastfpe_next

CPDO_adf_extra:
	cmp	r7,#0x7fffffff		@ was it the 1st ?
	bne	CPDO_infnan_2		@ no it was the 2nd
	cmp	r8,#0x7fffffff		@ if 1st, 2nd too ?
	bne	CPDO_infnan_1		@ no only 1st
	cmp	r3,#0
	cmpeq	r4,#0
	bne	CPDO_nan_12
	b	CPDO_inf

/*---------------------------------------------------------------------------*/

CPDO_infnan_1:
	stmia	r0,{r1,r3,r5,r7}
	b	fastfpe_next

CPDO_infnan_2:
	stmia	r0,{r2,r4,r6,r8}
	b	fastfpe_next
	
CPDO_nan_12:
	orr	r2,r3,r4
	b	CPDO_inf_1

CPDO_nan:
	mov	r2,#0x40000000		@ create non signalling NaN
	b	CPDO_inf_1

CPDO_inf:
	mov	r2,#0
CPDO_inf_1:
	mov	r3,#0
	mov	r4,#0x7fffffff
CPDO_store_1234:
	stmia	r0,{r1,r2,r3,r4}
	b	fastfpe_next

CPDO_zero:
	mov	r1,#0
CPDO_zero_1:
	mov	r2,#0
	mov	r3,#0
	mov	r4,#0x80000000
	stmia	r0,{r1,r2,r3,r4}
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPDO_suf
CPDO_suf:
	ldmia	r1,{r1,r3,r5,r7}
	ldmia	r2,{r2,r4,r6,r8}

CPDO_suf_l:
	cmp	r7,#0x7fffffff
	cmpne	r8,#0x7fffffff
	beq	CPDO_suf_extra

	cmp	r1,r2
	bne	CPDO_adf_s

CPDO_suf_s:
	subs	r2,r7,r8		@ determine greater number
	bgt	CPDO_suf_2nd		@ first number is greater
	blt	CPDO_suf_1st		@ second number is greater
	cmp	r3,r4			@ also mantissa is important
	cmpeq	r5,r6
	bhi	CPDO_suf_2nd		@ first number is greater
	beq	CPDO_zero

CPDO_suf_1st:
	eor	r1,r1,#0x80000000	@ second number is greater, invert sign
	mov	r7,r8
	rsb	r2,r2,#0
	cmp	r2,#32
	ble	CPDO_suf_1st2

	sub	r2,r2,#32
	cmp	r2,#32
	movgt	r2,#32
	mov	r5,r3,lsr r2
	mov	r3,#0
	b	CPDO_suf_1st_sub

CPDO_suf_1st2:
	rsb	r8,r2,#32
	mov	r5,r5,lsr r2
	orr	r5,r5,r3,lsl r8
	mov	r3,r3,lsr r2	@ 1. op normalized

CPDO_suf_1st_sub:
	subs	r5,r6,r5	@ do subtraction
	sbc	r3,r4,r3
	b	CPDO_suf_norm

CPDO_suf_2nd:
	cmp	r2,#32
	ble	CPDO_suf_2nd2

	sub	r2,r2,#32
	cmp	r2,#32
	movgt	r2,#32
	mov	r6,r4,lsr r2
	mov	r4,#0
	b	CPDO_suf_2nd_sub

CPDO_suf_2nd2:
	rsb	r8,r2,#32
	mov	r6,r6,lsr r2
	orr	r6,r6,r4,lsl r8
	mov	r4,r4,lsr r2	@ 2. op normalized

CPDO_suf_2nd_sub:
	subs	r5,r5,r6
	sbc	r3,r3,r4	@ do subtraction

CPDO_suf_norm:
	teq	r3,#0		@ normalize 32bit
	moveq	r3,r5
	moveq	r5,#0
	subeq	r7,r7,#32
	
	cmp	r3,#0x00010000	@ 16bit
	movcc	r3,r3,lsl#16
	orrcc	r3,r3,r5,lsr#16
	movcc	r5,r5,lsl#16
	subcc	r7,r7,#16
	
	cmp	r3,#0x01000000	@ 8bit
	movcc	r3,r3,lsl#8
	orrcc	r3,r3,r5,lsr#24
	movcc	r5,r5,lsl#8
	subcc	r7,r7,#8
	
	cmp	r3,#0x10000000	@ 4bit
	movcc	r3,r3,lsl#4
	orrcc	r3,r3,r5,lsr#28
	movcc	r5,r5,lsl#4
	subcc	r7,r7,#4
	
	cmp	r3,#0x40000000	@ 2bit
	movcc	r3,r3,lsl#2
	orrcc	r3,r3,r5,lsr#30
	movcc	r5,r5,lsl#2
	subcc	r7,r7,#2
	
	cmp	r3,#0x80000000	@ 1bit
	movcc	r3,r3,lsl#1
	orrcc	r3,r3,r5,lsr#31
	movcc	r5,r5,lsl#1
	subcc	r7,r7,#1

	cmp	r7,#0xe0000000
	ble	CPDO_zero_1

	stmia	r0,{r1,r3,r5,r7}
	b	fastfpe_next

CPDO_suf_extra:
	cmp	r7,#0x7fffffff		@ was it the 1st ?
	eorne	r2,r2,#0x80000000	@ change sign, might have been INF
	bne	CPDO_infnan_2		@ no it was the 2nd
	cmp	r8,#0x7fffffff		@ if 1st, 2nd too ?
	bne	CPDO_infnan_1		@ no only 1st
	cmp	r3,#0
	cmpeq	r4,#0
	bne	CPDO_nan_12
	b	CPDO_nan		@ here is difference with adf !

/*---------------------------------------------------------------------------*/

	.globl CPDO_rsf
CPDO_rsf:
	mov	r3,r2
	ldmia	r1,{r2,r4,r6,r8}
	ldmia	r3,{r1,r3,r5,r7}
	b	CPDO_suf_l
	
/*---------------------------------------------------------------------------*/

	.globl	CPDO_muf
CPDO_muf:
	ldmia	r1,{r1,r3,r5,r7}
	ldmia	r2,{r2,r4,r6,r8}

	cmp	r7,#0x7fffffff
	cmpne	r8,#0x7fffffff
	beq	CPDO_muf_extra
	
	eor	r1,r1,r2
	adds	r8,r7,r8
	bvs	CPDO_zero_1

	umull	r7,r2,r3,r4
	umull	r14,r3,r6,r3
	adds	r7,r7,r3	@ r2|r7|r14 = r2|r7|#0 + #0|r3|r14
	adc	r2,r2,#0
	umull	r4,r3,r5,r4
	adds	r14,r14,r4	@ r2|r7|r14 += #0|r3|r4
	adcs	r7,r7,r3
	adc	r2,r2,#0
	umull	r4,r3,r5,r6
	adds	r14,r14,r3	@ r2|r7|r14 += #0|#0|r3
	adcs	r7,r7,#0
	adcs	r2,r2,#0

	bpl	CPDO_muf_norm
	
	add	r8,r8,#1
	b	CPDO_muf_end
	
CPDO_muf_norm:
	adds	r14,r14,r14
	adcs	r7,r7,r7
	adcs	r2,r2,r2

CPDO_muf_end:
	cmp	r8,#0x20000000
	bge	CPDO_inf
	cmp	r8,#0xe0000000
	ble	CPDO_zero_1
	stmia	r0,{r1,r2,r7,r8}
	b	fastfpe_next

CPDO_muf_extra:
	cmp	r7,#0x7fffffff		@ was it the first?
	bne	CPDO_muf_extra_2nd	@ no, so it was the second
	cmp	r8,#0x7fffffff		@ yes, second too?
	bne	CPDO_muf_extra_1st	@ no, only first
	orr	r3,r3,r4		@ if both inf -> inf, otherwise nan
	eor	r1,r1,r2		@ sign for the inf case
	b	CPDO_infnan_1

CPDO_muf_extra_1st:
	cmp	r3,#0			@ is it a nan?
	bne	CPDO_infnan_1
	cmp	r8,#0x80000000		@ is the second 0?
	beq	CPDO_nan
	eor	r1,r1,r2		@ correct sign for inf
	b	CPDO_inf

CPDO_muf_extra_2nd:
	cmp	r4,#0			@ is it a nan?
	bne	CPDO_infnan_2
	cmp	r7,#0x80000000		@ is the first 0?
	beq	CPDO_nan
	eor	r1,r1,r2		@ correct sign for inf
	b	CPDO_inf

/*---------------------------------------------------------------------------*/

	.globl	CPDO_dvf
CPDO_dvf:
	ldmia	r1,{r1,r3,r5,r7}
	ldmia	r2,{r2,r4,r6,r8}

CPDO_dvf_l:
	cmp	r7,#0x7fffffff
	cmpne	r8,#0x7fffffff
	beq	CPDO_dvf_extra
	cmp	r8,#0x80000000
	beq	CPDO_dvf_by0

	eor	r1,r1,r2
	cmp	r7,#0x80000000
	beq	CPDO_zero_1
	
	sub	r8,r7,r8
	
	mov	r2,#0
	mov	r7,#1

	cmp	r3,r4
	cmpeq	r5,r6
	bcs	CPDO_dvf_loop_

	sub	r8,r8,#1

CPDO_dvf_loop:
	adds	r5,r5,r5
	adcs	r3,r3,r3
	bcs	CPDO_dvf_anyway
CPDO_dvf_loop_:
	subs	r5,r5,r6
	sbcs	r3,r3,r4
	bcs	CPDO_dvf_okay

	adds	r5,r5,r6
	adc	r3,r3,r4
	adds	r7,r7,r7
	adcs	r2,r2,r2
	bcc	CPDO_dvf_loop
	b	CPDO_dvf_end

CPDO_dvf_anyway:
	adcs	r7,r7,r7
	adcs	r2,r2,r2
	bcs	CPDO_dvf_end
	subs	r5,r5,r6
	sbc	r3,r3,r4
	b	CPDO_dvf_loop

CPDO_dvf_okay:
	adcs	r7,r7,r7
	adcs	r2,r2,r2
	bcc	CPDO_dvf_loop

CPDO_dvf_end:
	b	CPDO_muf_end

CPDO_dvf_by0:
	cmp	R7,#0x80000000
	beq	CPDO_nan		@ first also 0 -> nan
	eor	r1,r1,r2		@ otherwise calculatesign for inf
	b	CPDO_inf

CPDO_dvf_extra:
	cmp	r7,#0x7fffffff		@ was it the first?
	bne	CPDO_dvf_extra_2nd	@ no, so it was the second
	cmp	r8,#0x7fffffff		@ yes, second too?
	bne	CPDO_dvf_extra_1st	@ no, only first
	orrs	r3,r3,r4
	beq	CPDO_nan		@ if both inf -> create nan
	b	CPDO_nan_12		@ otherwise keep nan

CPDO_dvf_extra_1st:
	eor	r1,r1,r2		@ correct sign for inf
	b	CPDO_infnan_1

CPDO_dvf_extra_2nd:
	cmp	r4,#0			@ is it a nan?
	bne	CPDO_infnan_2
	eor	r1,r1,r2		@ correct sign for zero
	b	CPDO_zero_1

/*---------------------------------------------------------------------------*/

	.globl	CPDO_rdf
CPDO_rdf:
	mov	r3,r2
	ldmia	r1,{r2,r4,r6,r8}
	ldmia	r3,{r1,r3,r5,r7}
	b	CPDO_dvf_l

/*---------------------------------------------------------------------------*/

	.globl	CPDO_rmf
CPDO_rmf:
	b	fastfpe_next
	
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/

	.globl	CPDO_mvf
CPDO_mvf:
	ldmia	r2,{r1,r2,r3,r4}
	stmia	r0,{r1,r2,r3,r4}
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPDO_mnf
CPDO_mnf:
	ldmia	r2,{r1,r2,r3,r4}
	eor	r1,r1,#0x80000000
	stmia	r0,{r1,r2,r3,r4}
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPDO_abs
CPDO_abs:
	ldmia	r2,{r1,r2,r3,r4}
	bic	r1,r1,#0x80000000
	stmia	r0,{r1,r2,r3,r4}
	b	fastfpe_next

/*---------------------------------------------------------------------------*/
	
	.globl	CPDO_sqt
CPDO_sqt:
	ldmia	r2,{r1,r2,r3,r4}
	cmp	r1,#0
	bne	CPDO_nan
	cmp	r4,#0x7fffffff
	beq	CPDO_store_1234

	tst	r4,r4,lsr#1		@carry=exponent bit 0
	bcc	CPDO_sqt_exponenteven
	adds	r3,r3,r3
	adcs	r2,r2,r2		@carry is needed in loop!
CPDO_sqt_exponenteven:
	mov	r4,r4,asr #1
	str	r4,[r0,#12]

	mov	r4,#0x80000000
	mov	r5,#0
	sub	r2,r2,#0x80000000

	mov	r8,#0x40000000
	mov	r14,#0x80000000

	mov	r1,#1
	b	CPDO_sqt_loop1_first
CPDO_sqt_loop1:
	adds	r3,r3,r3
	adcs	r2,r2,r2
CPDO_sqt_loop1_first:
	add	r6,r4,r8,lsr r1		@r7 const = r5
	bcs	CPDO_sqt_loop1_1
	cmp	r2,r6
	cmpeq	r3,r5			@r5 for r7
	bcc	CPDO_sqt_loop1_0
CPDO_sqt_loop1_1:
	orr	r4,r4,r14,lsr r1
	subs	r3,r3,r5		@r5 for r7
	sbc	r2,r2,r6
CPDO_sqt_loop1_0:
	add	r1,r1,#1
	cmp	r1,#30
	ble	CPDO_sqt_loop1

	adds	r3,r3,r3
	adcs	r2,r2,r2
	bcs	CPDO_sqt_between_1
	adds	r7,r5,#0x80000000
	adc	r6,r4,#0
	cmp	r2,r6
	cmpeq	r3,r7
	bcc	CPDO_sqt_between_0
CPDO_sqt_between_1:
	orr	r4,r4,#0x00000001
	subs	r3,r3,r5
	sbc	r2,r2,r4
	subs	r3,r3,#0x80000000
	sbc	r2,r2,#0
CPDO_sqt_between_0:
	mov	r1,#0

CPDO_sqt_loop2:
	adds	r3,r3,r3
	adcs	r2,r2,r2
	bcs	CPDO_sqt_loop2_1
	adds	r7,r5,r8,lsr r1
	adc	r6,r4,#0
	cmp	r2,r6
	cmpeq	r3,r7
	bcc	CPDO_sqt_loop2_0
CPDO_sqt_loop2_1:
	orr	r5,r5,r14,lsr r1
	subs	r3,r3,r5
	sbc	r2,r2,r4
	subs	r3,r3,r8,lsr r1
	sbc	r2,r2,#0
CPDO_sqt_loop2_0:
	add	r1,r1,#1
	cmp	r1,#30
	ble	CPDO_sqt_loop2

	adds	r3,r3,r3
	adcs	r2,r2,r2
	bcs	CPDO_sqt_after_1
	cmp	r2,r6
	cmpeq	r3,r7
	bcc	CPDO_sqt_after_0
CPDO_sqt_after_1:
	orr	r5,r5,#0x00000001
CPDO_sqt_after_0:

	mov	r1,#0
	stmia	r0,{r1,r4,r5}
	b	fastfpe_next

/*---------------------------------------------------------------------------*/
	
	.globl	CPDO_rnd
CPDO_rnd:
	ldmia	r2,{r1,r2,r3,r5}
        bl      CPDO_rnd_core

CPDO_rnd_store:
	stmia	r0,{r1,r2,r3,r5}
    	b	fastfpe_next
	
/*---------------------------------------------------------------------------*/

	.globl	CPDO_rnd_core
CPDO_rnd_core:
	and	r4,r4,#0x00000060
	add	pc,pc,r4,lsr#3
	mov	r0,r0
	b	CPDO_rnd_N
	b	CPDO_rnd_P
	b	CPDO_rnd_M
	b	CPDO_rnd_Z
	
CPDO_rnd_N:
	cmp	r5,#-1
	blt	CPDO_rnd_zero
	cmp	r5,#63
	movge	pc,r14
	mov	r4,#0x40000000
	cmp	r5,#31
	bge	CPDO_rnd_N_2

	adds	r2,r2,r4,lsr r5
	bcc	CPDO_rnd_end
	b	CPDO_rnd_end_norm

CPDO_rnd_N_2:
CPDO_rnd_P_2:
	sub	r6,r5,#32
	adds	r3,r3,r4,ror r6	@ror ist needed to handle a -1 correctly
	adcs	r2,r2,#0
	bcc	CPDO_rnd_end
	b	CPDO_rnd_end_norm

CPDO_rnd_P:
	tst	r1,#0x80000000
	bne	CPDO_rnd_M_entry
CPDO_rnd_P_entry:
	cmp	r5,#0
	blt	CPDO_rnd_P_small
	cmp	r5,#63
	movge	pc,r14
	mov	r4,#0x7fffffff
	cmp	r5,#32
	bge	CPDO_rnd_P_2

	adds	r3,r3,#0xffffffff
	adcs	r2,r2,r4,lsr r5
	bcc	CPDO_rnd_end
	b	CPDO_rnd_end_norm

CPDO_rnd_P_small:
	cmp	r5,#0x80000000
	moveq	pc,r14
	b	CPDO_rnd_one

CPDO_rnd_M:
	tst	r1,#0x80000000
	bne	CPDO_rnd_P_entry
CPDO_rnd_M_entry:
	cmp	r5,#0
	blt	CPDO_rnd_zero
	cmp	r5,#63
	movge	pc,r14

	b	CPDO_rnd_end
	
CPDO_rnd_Z:
	cmp	r5,#0
	blt	CPDO_rnd_zero
	cmp	r5,#63
	movge	pc,r14
	b	CPDO_rnd_end

CPDO_rnd_end_norm:
	add	r5,r5,#1
	movs	r2,r2,rrx
	mov	r3,r3,rrx
CPDO_rnd_end:
	rsbs	r4,r5,#31
	bmi	CPDO_rnd_end_2
	mov     r3,#0
	mov     r2,r2,lsr r4
	mov	r2,r2,lsl r4
	mov	pc,r14

CPDO_rnd_end_2:
	rsb	r4,r5,#63
	mov	r3,r3,lsr r4
	mov	r3,r3,lsl r4
	mov	pc,r14

CPDO_rnd_one:
	mov	r2,#0x80000000
	mov	r3,#0
	mov	r5,#0
	mov	pc,r14
	
CPDO_rnd_zero:
	mov	r1,#0
	mov	r2,#0
	mov	r3,#0
	mov	r5,#0x80000000
	mov	pc,r14

/*---------------------------------------------------------------------------*/
