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
*/

/*---------------------------------------------------------------------------*/

	.globl	CPDT_load_single
CPDT_load_single:
	ldr	r1,[r6]
	
	and	r2,r1,#0x80000000	@ r2 = sign
	
	mov	r5,r1,lsr#23
	bics	r5,r5,#0x100
	beq	CPDT_ls_e0		@ exponent = 0; zero/denormalized
	teq	r5,#255
	beq	CPDT_ls_e255		@ exponent = 255; infinity/NaN

	sub     r5,r5,#127		@ r5 = exponent, remove normalized bias

	mov	r3,r1,lsl#8
	orr	r3,r3,#0x80000000
	mov	r4,#0			@ r3,r4 = mantissa

	stmia	r0,{r2-r5}
	b	fastfpe_next

CPDT_ls_e0:
	movs	r3,r1,lsl#9
	beq	CPDT_load_zero

	mov	r5,#-127

CPDT_ls_e0_norm:
	tst	r3,#0x80000000
	subeq	r5,r5,#1
	moveq	r3,r3,lsl#1
	beq	CPDT_ls_e0_norm

	mov	r4,#0
	stmia	r0,{r2-r5}
	b	fastfpe_next

CPDT_ls_e255:
	mov	r3,r1,lsl#9
	mov	r4,#0
	mov	r5,#0x7fffffff
	stmia	r0,{r2-r5}
	b	fastfpe_next

CPDT_load_zero:
	mov	r3,#0
	mov	r4,#0
	mov	r5,#0x80000000
	stmia	r0,{r2-r5}
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPDT_load_double
CPDT_load_double:
	ldr	r1,[r6]
	ldr	r6,[r6,#4]

	and	r2,r1,#0x80000000	@ r2 = sign

	mov	r5,r1,lsr#20
	bics	r5,r5,#0x800
	beq	CPDT_ld_e0		@ exponent = 0; zero/denormalized
	add	r4,r5,#1
	teq	r4,#2048
	beq	CPDT_ld_e2047		@ exponent = 2047; infinity/NaN

	add     r5,r5,#1
	sub	r5,r5,#1024		@ r5 = exponent, remove normalized bias

	mov	r3,r1,lsl#11
	orr	r3,r3,#0x80000000
	orr	r3,r3,r6,lsr #21
	mov	r4,r6,lsl#11		@ r3,r4 = mantissa

	stmia	r0,{r2-r5}
	b	fastfpe_next

CPDT_ld_e0:
	mov	r3,r1,lsl#12
	orr	r3,r3,r6,lsr#20
	movs	r4,r6,lsl#12
	teqeq	r3,#0
	beq	CPDT_load_zero
	
	mov	r5,#1
	sub	r5,r5,#1024

CPDT_ld_e0_norm:
	tst	r3,#0x80000000
	subeq	r5,r5,#1
	moveqs	r4,r4,lsl#1
	adceq	r3,r3,r3
	beq	CPDT_ld_e0_norm

	stmia	r0,{r2-r5}
	b	fastfpe_next

CPDT_ld_e2047:
	mov	r3,r1,lsl#12
	orr	r3,r3,r6,lsr#1
	bic	r6,r6,#0x80000000
	orr	r3,r3,r6		@ to get all fraction bits !
	mov	r4,#0
	mov	r5,#0x7fffffff
	stmia	r0,{r2-r5}
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPDT_load_extended
CPDT_load_extended:
	ldr	r1,[r6]
	ldr	r3,[r6,#4]
	ldr	r4,[r6,#8]
	
	and	r2,r1,#0x80000000
	bics	r5,r1,#0x80000000
	beq	CPDT_le_e0
	add	r1,r5,#1
	teq	r4,#32768
	beq	CPDT_le_e32767

	add	r5,r5,#1
	sub	r5,r5,#16384

	stmia	r0,{r2-r5}
	b	fastfpe_next

CPDT_le_e0:
	teq	r3,#0
	teqeq	r4,#0
	beq	CPDT_load_zero
	
	mov	r5,#2
	sub	r5,r5,#16384
	b	CPDT_ld_e0_norm

CPDT_le_e32767:
	mov	r3,r3,lsl#1
	orr	r3,r3,r4,lsr#1
	bic	r4,r4,#0x80000000
	orr	r3,r3,r4
	mov	r5,#0x7fffffff
	stmia	r0,{r2-r5}
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPDT_load_decimal
CPDT_load_decimal:
	
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPDT_store_single
CPDT_store_single:
	ldmia	r0,{r1-r4}
	
	cmp	r4,#-127
	ble	CPDT_ss_e0
	cmp	r4,#128
	bge	CPDT_ss_e255

	adds	r2,r2,#1<<7		@ round to nearest
	bcs	CPDT_ss_rnd_ovfl	@ very very seldom taken

CPDT_ss_store:
	add	r4,r4,#127
	orr	r1,r1,r4,lsl#23
	
	bic	r2,r2,#0x80000000
	orr	r1,r1,r2,lsr#8

	str	r1,[r6]
	b	fastfpe_next

CPDT_ss_rnd_ovfl:
	add	r4,r4,#1
	cmp	r4,#128
	bge	CPDT_ss_e255

	mov	r2,#0x80000000
	mov	r3,#0
	b	CPDT_ss_store

CPDT_ss_e0:
	cmp	r4,#-150
	ble	CPDT_ss_zero

	add	r4,r4,#126
CPDT_ss_unnormalize:
	mov	r2,r2,lsr#1
	adds	r4,r4,#1
	bne	CPDT_ss_unnormalize

	orr	r1,r1,r2,lsr#8

CPDT_ss_zero:
	str	r1,[r6]
	b	fastfpe_next

CPDT_ss_e255:
	cmp	r4,#0x7fffffff
	bne	CPDT_ss_inf
	cmp	r2,#0
	beq	CPDT_ss_inf

	orr	r1,r1,#0x00200000	@ for safety so that it is not INF
	orr	r1,r1,r2,lsr#9		@ get highest bit of mantissa

CPDT_ss_inf:
	orr	r1,r1,#0x7f000000
	orr	r1,r1,#0x00800000
	str	r1,[r6]
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPDT_store_double
CPDT_store_double:
	ldmia	r0,{r1-r4}
	
	cmp	r4,#1024		@ this check has to be first, or
	bge	CPDT_sd_e2047		@ overflow can occur on second !
	add	r0,r4,#3
	cmp	r0,#-1023+3		@ cmp with -1023
	ble	CPDT_sd_e0

	adds	r3,r3,#1<<10		@ round to nearest
	adcs	r2,r2,#0
	bcs	CPDT_sd_rnd_ovfl	@ very very seldom taken

CPDT_sd_store:
	sub	r4,r4,#1
	add	r4,r4,#1024
	orr	r1,r1,r4,lsl#20
	
	bic	r2,r2,#0x80000000
	orr	r1,r1,r2,lsr#11

	mov	r2,r2,lsl#21
	orr	r2,r2,r3,lsr#11

	stmia	r6,{r1,r2}
	b	fastfpe_next

CPDT_sd_rnd_ovfl:
	add	r4,r4,#1
	cmp	r4,#1024
	bge	CPDT_sd_e2047

	mov	r2,#0x80000000
	mov	r3,#0
	b	CPDT_sd_store

CPDT_sd_e0:
	add	r0,r4,#1075-1024
	cmp	r0,#-1024
	ble	CPDT_sd_zero

	add	r4,r4,#1024
	sub	r4,r4,#2
CPDT_sd_unnormalize:
	movs	r2,r2,lsr#1
	mov	r3,r3,rrx
	adds	r4,r4,#1
	bne	CPDT_sd_unnormalize

	orr	r1,r1,r2,lsr#11
	mov	r2,r2,lsl#21
	orr	r2,r2,r3,lsr#11

	stmia	r6,{r1,r2}
	b	fastfpe_next

CPDT_sd_zero:
	mov	r2,#0
	stmia	r6,{r1,r2}
	b	fastfpe_next

CPDT_sd_e2047:
	cmp	r4,#0x7fffffff
	bne	CPDT_sd_inf
	cmp	r2,#0
	beq	CPDT_sd_inf

	orr	r1,r1,#0x00040000	@ for safety so that it is not INF
	orr	r1,r1,r2,lsr#12		@ get highest bit of mantissa

CPDT_sd_inf:
	orr	r1,r1,#0x7f000000
	orr	r1,r1,#0x00f00000
	stmia	r6,{r1,r2}
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPDT_store_extended
CPDT_store_extended:
	ldmia	r0,{r1-r4}
	
	cmp	r4,#16384		@ this check has to be first, or
	bge	CPDT_se_e32767		@ overflow can occur with second !
	add	r0,r4,#63
	cmp	r0,#-16383+63
	ble	CPDT_se_e0

	sub	r4,r4,#1
	add	r4,r4,#16384
	orr	r1,r1,r4
	
	stmia	r6,{r1-r3}
	b	fastfpe_next

CPDT_se_e0:
	add	r0,r4,#16446-16384
	cmp	r0,#-16384
	ble	CPDT_se_zero

	add	r4,r4,#16384
	sub	r4,r4,#2
CPDT_se_unnormalize:
	movs	r2,r2,lsr#1
	mov	r3,r3,rrx
	adds	r4,r4,#1
	bne	CPDT_se_unnormalize

	stmia	r6,{r1-r3}
	b	fastfpe_next
	
CPDT_se_zero:
	mov	r2,#0
	mov	r3,#0
	stmia	r6,{r1-r3}
	b	fastfpe_next

CPDT_se_e32767:
	cmp	r4,#0x7fffffff
	bne	CPDT_se_inf
	cmp	r2,#0
	beq	CPDT_se_inf

	mov	r2,r2,lsl#1
	orr	r2,r2,#0x20000000

CPDT_se_inf:
	orr	r1,r1,#0x00007f00
	orr	r1,r1,#0x000000ff
	stmia	r6,{r1-r3}
	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPDT_store_decimal
CPDT_store_decimal:

	b	fastfpe_next

/*---------------------------------------------------------------------------*/

	.globl	CPDT_sfm
CPDT_sfm:
	add	r2,r10,r0,lsr#8
	ldr	r4,[r2,#0]
	ldr	r3,[r2,#4]
	bic	r3,r3,#0x80000000
	orr	r3,r3,r4
	str	r3,[r6],#4
	ldr	r3,[r2,#8]
	str	r3,[r6],#4
	ldr	r3,[r2,#12]
	str	r3,[r6],#4

	add	r0,r0,#1<<12
	and	r0,r0,#7<<12
	subs	r1,r1,#1
	bne	CPDT_sfm
	b	fastfpe_next
	
/*---------------------------------------------------------------------------*/

	.globl	CPDT_lfm
CPDT_lfm:
	add	r2,r10,r0,lsr#8
	ldr	r4,[r6],#4
	and	r3,r4,#0x80000000
	str	r3,[r2,#0]
	ldr	r3,[r6],#4
	str	r3,[r2,#8]
	ldr	r3,[r6],#4
	str	r3,[r2,#12]

	cmp	r3,#0x80000000		@ does the exp indicate zero?
	biceq	r4,r4,#0x80000000	@ if so, indicate 'denormalized'
	beq	CPDT_lfm_storer4
	cmp	r3,#0x7fffffff		@ does the exp indicate inf or NaN?
	biceq	r4,r4,#0x80000000	@ if so, indicate 'denormalized'
	beq	CPDT_lfm_storer4
	orrne	r4,r4,#0x80000000	@ otherwise, set normalized bit

CPDT_lfm_storer4:
	str	r4,[r2,#4]

	add	r0,r0,#1<<12
	and	r0,r0,#7<<12
	subs	r1,r1,#1
	bne	CPDT_lfm
	b	fastfpe_next
	
/*---------------------------------------------------------------------------*/
