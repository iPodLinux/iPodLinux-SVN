/*
 * This file is needed to pad out the start of the kernel to
 * leave room for the IRQ stubs.
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
