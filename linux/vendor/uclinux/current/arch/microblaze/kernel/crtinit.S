###################################-*-asm*- 
# 
# Copyright (c) 2001 Xilinx, Inc.  All rights reserved. 
# 
# Xilinx, Inc. CONFIDENTIAL 
#
# XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A 
# COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
# ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR 
# STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION
# IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE 
# FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.  
# XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO 
# THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO 
# ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE 
# FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY 
# AND FITNESS FOR A PARTICULAR PURPOSE.
# 
# crtinit.s 
# 
#	C RunTime:
#	Used for initialization of user's bss area
#	All external and static variables are initialized 
# 
#######################################

	.globl _crtinit
	.align 2
	.ent _crtinit

_crtinit:
	/* Save Link register	 */
	addi	r1,r1,-4
	sw	r15,r1,r0

	/* clear sbss */
	addi	r6,r0,__sbss_start      /* SBSS beginning	*/
	addi	r7,r0,__sbss_end	/* SBSS end		*/
	rsub	r18,r6,r7		/* Compare		*/
	blei	r18,.Lendsbss

.Lloopsbss:	
	sw	r0,r6,r0
	addi	r6,r6,4
	rsub	r18,r6,r7
	bgti	r18,.Lloopsbss
.Lendsbss:

	/* clear bss */
	addi	r6,r0,__bss_start      /* BSS beginning		*/
	addi	r7,r0,__bss_end		/* BSS end		*/

	rsub	r18,r6,r7		/* Compare		*/
	blei	r18,.Lendbss
.Lloopbss:	
	sw	r0,r6,r0
	addi	r6,r6,4
	rsub	r18,r6,r7
	bgti	r18,.Lloopbss
.Lendbss:

/* 	Call the init function here 			  */
	brlid	r15,_program_init
	nop

/* 	Restore r15 					*/
	lw	r15,r1,r0
	addi	r1,r1,4

	rtsd	r15,8
	nop
	.end _crtinit

