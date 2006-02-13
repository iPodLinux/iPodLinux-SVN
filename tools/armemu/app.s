        .global IVEC_rst
IVEC_rst:
	b	start
IVEC_und:
	b	IVEC_rst
IVEC_swi:
	movs	PC, R14
IVEC_pabt:
	b	IVEC_rst
IVEC_dabt:
	b	IVEC_rst
IVEC_irq:
	subs	PC, R14, #4
IVEC_fiq:
	subs	PC, R14, #4

start:	mov	r0, #0x40
	mov	r1, #0x80
loop:	cmp	r0, r1
	bhi	.
	add	r0, r0, #2
	add	r1, r1, #1
	b	loop

@ Local Variables:
@ indent-tabs-mode: t
@ End:
