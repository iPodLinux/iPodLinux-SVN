	.globl	_start
_start:	mov	r0, #0x40
	mov	r1, #0x80
loop:	cmp	r0, r1
	bhi	done
	add	r0, r0, #2
	add	r1, r1, #1
	b	loop
done:	mov	r2, #0x70000000
	ldr	r3, [r2, #-16]
	mov	r3, r3, ror #16
	str	r3, [r2, #-4]
	adr	r3, string
	str	r3, [r2, #-12]
	ldr	r3, =1000000
	str	r3, [r2, #-8]
	mov	pc, lr
string:	.asciz	"Hello, world!"

@ Local Variables:
@ asm-comment-char: ?@
@ comment-start: "@ "
@ block-comment-start: "/*"
@ block-comment-end: "*/"
@ indent-tabs-mode: t
@ End:
