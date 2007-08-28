###################################-*-asm*- 
# 
# Copyright (c) 2001 Xilinx, Inc.  All rights reserved. 
# 
# Xilinx, Inc.  
# 
# strcmp.s 
#
# Compare strings, This function is optimized for MicroBlaze.
#       Input : String1 in Reg r5
#               String2 in Reg r6
#       Output: If the strings are equal R3 == 0
#               else R3 = difference between the value of the first differing byte
# 
#######################################

	
#include <asm/clinkage.h>

	.globl	C_SYMBOL_NAME(strcmp)

	.text
	.text
	.ent	C_SYMBOL_NAME(strcmp)
C_SYMBOL_NAME(strcmp):
	.frame	r1,0,r15		# vars= 0, regs= 0/0, args= 0, extra= 0
	.mask	0x00000000,0
	or	r3,r5,r6 # MICROBLAZEor
	andi	r3,r3,3 # MICROBLAZEandi-long
	bneid	r3,$L16 # MICROBLAZEbnei1
	addi	r10,r0,0 # MICROBLAZEaddi1
	bri	$L15 # MICROBLAZEbri6
/* word to word compares but with more checks for null */
$L6:
	lw	r3,r0,r5 #MICROBLAZElw1
	addi	r4,r3,-16843009 # MICROBLAZEaddi1
	xori	r3,r3,-1 # MICROBLAZE1scmpl
	and	r4,r4,r3 # MICROBLAZEand
	andi	r4,r4,-2139062144 # MICROBLAZEandi-long
	beqid	r4,$L7 # MICROBLAZEbeqi1
	addi	r5,r5,4 # MICROBLAZEaddi1
	rtsd	r15, 8 # MICROBLAZErtsd1
	add	r3,r0,r0 #MICROBLAZEadd to move

$L7:
	addi	r6,r6,4 # MICROBLAZEaddi1
$L15:
	lw	r4,r0,r5 #MICROBLAZElw1
	lw	r3,r0,r6 #MICROBLAZElw1
	rsub	r18,r3,r4 # MICROBLAZEcmp
	beqi	r18,$L6 # MICROBLAZEbeqi12 #changes

$L16:
	lbu	r7,r5,r10 #MICROBLAZElhu1
	beqi	r7,$L10 # MICROBLAZEbeqi1
	lbu	r4,r6,r10 #MICROBLAZElhu1
	rsub	r18,r4,r7 # MICROBLAZEcmp
	beqid	r18,$L16 # MICROBLAZEbeqi12 #changes
	addi	r10,r10,1 # MICROBLAZEaddi1
	addi	r10,r10,-1 # MICROBLAZEaddi1
$L10:
	lbu	r4,r10,r5 #MICROBLAZElhu1
	lbu	r3,r10,r6 #MICROBLAZElhu1
	rtsd	r15, 8 # MICROBLAZErtsd1

	rsub	r3,r3,r4 # MICROBLAZErsub1

	.end	C_SYMBOL_NAME(strcmp)
