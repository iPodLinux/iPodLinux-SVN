/*
 *
 */

	b	stext
	b	unexpected
	b	unexpected
	b	unexpected
	b	unexpected
	b	unexpected
	b	unexpected
	b	unexpected

unexpected:
	b	unexpected

	/* Leave room for the IRQ stubs to get copied to */
	.space 32768-(9*4)

stext:
