/*
 * Derived from the Hyperstone's library source code.
 * Modefied src in order to apply the -mgnu-param compiler option.
 * Copyright (C) 2002-2003 GDT, Yannis Mitsos <yannis.mitsos@gdt.gr>
 * 				George Thanos <george.thanos@gdt.gr>
 */
	    .text
	    .align   2
	    .global  _memchr

;ENTRY(_memchr)
_memchr:
	frame L6, L3
	andni L1, 0xFFFFFF00
	cmpi L0, 0
	be L_10
	ldbu.n L2, L5, 1
L_12:
	cmp  L5, L1
	be L_15
	addi L0, -1
	dbne L_12
	ldbu.n L2, L5, 1
L_10:
	movi L2, 0
	ret PC, L3
L_15:
	addi L2, -1
	ret PC, L3
	.end
