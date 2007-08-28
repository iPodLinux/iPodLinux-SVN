###################################-*-asm*- 
# 
# Copyright (c) 2001 Xilinx, Inc.  All rights reserved. 
# 
# Xilinx, Inc.  
# 
# muldi3.s 
# 
# Multiply operation for 64 bit integers.
#	Input :	Operand1[H] in Reg r5
#		Operand1[L] in Reg r6		
#		Operand2[H] in Reg r7
#		Operand2[L] in Reg r8	
#	Output: Result[H] in Reg r3
#		Result[L] in Reg r4	
# 
#######################################

#include <asm/clinkage.h>

	.globl	C_SYMBOL_NAME(muldi3_proc)
	.ent	C_SYMBOL_NAME(muldi3_proc)

C_SYMBOL_NAME(muldi3_proc):
	or	r3,r5,r6
	BEQId	r3,$L_Result_Is_Zero	
	or	r4,r7,r8
	beqi	r4,$L_Result_Is_Zero
	BGEId	r5,$LaS1_Pos 
	XOR	r9,r5,r7  # Get the sign of the result
	RSUBI	r6,r6,0	    # Make r6 positive
	rsubc	r5,r5,r0    # Make r5 positive
$LaS1_Pos:
	BGEI	r7,$LaR5_Pos
	RSUBI	r8,r8,0	    # Make r8 positive
	rsubc	r7,r7,r0    # Make r7 positive
$LaR5_Pos:
	add	r3,r0,r0
	add	r4,r0,r0
	bri	$L1
$L2:	
	add	r6,r6,r6
	addc	r5,r5,r5
$L1:
	srl	r7,r7
	src	r8,r8
	addc	r10,r0,r0
	beqi	r10,$L2
	add	r4,r4,r6
	bneid	r8,$L2
	addc	r3,r3,r5
	bnei	r7,$L2
	blti	r9,$L_NegateResult
	rtsd	r15,8
	nop
$L_Result_Is_Zero:
	addi	r4,r0,0
	rtsd	r15,8
	addi	r3,r0,0
$L_NegateResult:
	rsub	r4,r4,r0
	rtsd	r15,8
	rsubc	r3,r3,r0
.end C_SYMBOL_NAME(muldi3_proc)
	
