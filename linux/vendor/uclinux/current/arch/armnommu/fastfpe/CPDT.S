/*
Inside the emulator the FP numbers are kept with 32 bit accuracy for both
mantissa and exponent. The FP structure has 4 words reserved for each
register, the first is used just for the sign in bit 31, the second is the
mantissa (unsigned integer) and the third is the exponent (signed integer).

The functions do actually only work properly for normalized values, and if
no overflow occurs. Hopfully most programs are not disturbed by this, and it
will probably be improved in future versions.

Decimal and packed decimal numbers are not supported so yet.
*/

/*---------------------------------------------------------------------------*/

	.globl	CPDT_load_single
CPDT_load_single:
	ldr	r1,[r6]
	
	bics	r2,r1,#0x80000000
	beq	CPDT_load_zero		@ test for 0
	
	mov	r2,r1,lsl#8
	orr	r2,r2,#0x80000000	@ insert leading 1

	mov     r3,r1,lsr#23
	bic     r3,r3,#0x100
	sub     r3,r3,#127		@ subtract normalized bias

	and	r1,r1,#0x80000000	@ only sign

	stmia	r0,{r1-r3}
	mov	pc,r14

CPDT_load_zero:
	mov	r1,#0
	mov	r2,#0
	mov	r3,#0x80000000
	stmia	r0,{r1-r3}
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPDT_load_double
CPDT_load_double:
	ldr	r2,[r6,#4]
	ldr	r1,[r6]

	bics	r3,r1,#0x80000000
	cmpeq	r2,#0
	beq	CPDT_load_zero		@ test for 0
	
	mov	r2,r2,lsr#21
	orr	r2,r2,r1,lsl#11
	orr	r2,r2,#0x80000000	@ insert leading 1

	mov     r3,r1,lsr#20
	bic     r3,r3,#0x800
	sub     r3,r3,#1024
	add	r3,r3,#1		@ subtract normalized bias

	and	r1,r1,#0x80000000

	cmp	r2,#0
	beq	CPDT_load_zero
	
	stmia   r0,{r1-r3}
	mov     pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPDT_load_extended
CPDT_load_extended:
	ldr	r2,[r6,#4]
	ldr	r1,[r6]
	
	cmp	r2,#0
	bics	r3,r1,#0x80000000
	beq	CPDT_load_zero		@ test for 0

	orr	r2,r2,#0x80000000	@ insert leading 1

	bic     r3,r1,#0x80000000
	sub     r3,r3,#16384
	add	r3,r3,#1		@ subtract normalized bias

	and	r1,r1,#0x80000000
	
	cmp	r2,#0
	beq	CPDT_load_zero	

	stmia   r0,{r1-r3}
	mov     pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPDT_load_decimal
CPDT_load_decimal:
	
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPDT_store_single
CPDT_store_single:
	ldmia	r0,{r1-r3}
	
	cmp	r2,#0
	beq	CPDT_store_single_zero

	adds	r3,r3,#127
	ble	CPDT_store_single_zero
	
	bic	r3,r3,#0x100
	orr	r1,r1,r3,lsl#23
	
	bic	r2,r2,#0x80000000
	orr	r1,r1,r2,lsr#8

	str	r1,[r6]
	mov	pc,r14

CPDT_store_single_zero:
	mov	r1,#0
	str	r1,[r6]
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPDT_store_double
CPDT_store_double:
	ldmia	r0,{r1-r3}
	
	cmp	r2,#0
	beq	CPDT_store_double_zero

	adds	r3,r3,#1024
	ble	CPDT_store_double_zero
	
	sub	r3,r3,#1
	bic	r3,r3,#0x800
	orr	r1,r1,r3,lsl#20
	
	bic	r2,r2,#0x80000000
	orr	r1,r1,r2,lsr#11

	mov	r2,r2,lsl#21

	stmia	r6,{r1-r2}
	mov	pc,r14

CPDT_store_double_zero:
	mov	r1,#0
	mov	r2,#0
	stmia	r6,{r1-r2}
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPDT_store_extended
CPDT_store_extended:
	ldmia	r0,{r1-r3}
	
	cmp	r2,#0
	beq	CPDT_store_extended_zero

	adds	r3,r3,#16384
	ble	CPDT_store_extended_zero
	
	sub	r3,r3,#1
	mov	r3,r3,lsl#17
	orr	r1,r1,r3,lsr#17

	mov	r3,#0

	stmia	r6,{r1-r3}
	mov	pc,r14
	
CPDT_store_extended_zero:
	mov	r1,#0
	mov	r2,#0
	mov	r3,#0
	stmia	r6,{r1-r3}
	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPDT_store_decimal
CPDT_store_decimal:

	mov	pc,r14

/*---------------------------------------------------------------------------*/

	.globl	CPDT_sfm
CPDT_sfm:
	add	r2,r10,r0,lsr#8
	ldr	r3,[r2],#4
	str	r3,[r6],#4
	ldr	r3,[r2],#4
	str	r3,[r6],#4
	ldr	r3,[r2],#4
	str	r3,[r6],#4

	add	r0,r0,#1<<12
	and	r0,r0,#7<<12
	subs	r1,r1,#1
	bne	CPDT_sfm
	mov	pc,r14
	
/*---------------------------------------------------------------------------*/

	.globl	CPDT_lfm
CPDT_lfm:
	add	r2,r10,r0,lsr#8
	ldr	r3,[r6],#4
	str	r3,[r2],#4
	ldr	r3,[r6],#4
	str	r3,[r2],#4
	ldr	r3,[r6],#4
	str	r3,[r2],#4

	add	r0,r0,#1<<12
	and	r0,r0,#7<<12
	subs	r1,r1,#1
	bne	CPDT_lfm
	mov	pc,r14
	
/*---------------------------------------------------------------------------*/
