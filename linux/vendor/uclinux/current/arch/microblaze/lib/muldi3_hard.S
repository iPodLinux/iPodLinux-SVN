###################################-*-asm*- 
# 
# Copyright (c) 2001 Xilinx, Inc.  All rights reserved. 
# 
# Xilinx, Inc.  
# 
# muldi3_hard.s 
# 
# Multiply operation for 64 bit integers, for devices with hard multiply
#	Input :	Operand1[H] in Reg r5
#		Operand1[L] in Reg r6		
#		Operand2[H] in Reg r7
#		Operand2[L] in Reg r8	
#	Output: Result[H] in Reg r3
#		Result[L] in Reg r4	
# 
# Explaination:
#
# 	Both the input numbers are divided into 16 bit number as follows
#		op1 = A B C D
# 		op2 = E F G H
#	result =    D * H 
#		 + (C * H + D * G) << 16
#		 + (B * H + C * G + D * F) << 32
#		 + (A * H + B * G + C * F + D * E) << 48 
#
# 	Only 64 bits of the output are considered
#
#######################################

#include <asm/clinkage.h>

	.globl	C_SYMBOL_NAME(muldi3_hardproc)
	.ent	C_SYMBOL_NAME(muldi3_hardproc)

C_SYMBOL_NAME(muldi3_hardproc):
	addi	r19,r19,-40

#  Save the input operands on the callers stack
	swi	r5,r19,44
	swi	r6,r19,48
	swi	r7,r19,52
	swi	r8,r19,56

# Store all the callee saved registers 
	sw	r20,r19,r0
	swi	r21,r19,4
	swi	r22,r19,8
	swi	r23,r19,12
	swi	r24,r19,16
	swi	r25,r19,20
	swi	r26,r19,24
	swi	r27,r19,28

# Load all the 16 bit values for A thru H
	lhui	r20,r19,44   # A
	lhui	r21,r19,46   # B
	lhui	r22,r19,48   # C
	lhui	r23,r19,50   # D
	lhui	r24,r19,52   # E
	lhui	r25,r19,54   # F
	lhui	r26,r19,56   # G
	lhui	r27,r19,58   # H

# D * H ==> LSB of the result on stack ==> Store1
	mul	r9,r23,r27
	swi	r9,r19,36    # Pos2 and Pos3

# Hi (Store1) + C * H + D * G ==> Store2 ==> Pos1 and Pos2
	lhui	r11,r19,36   # Pos2
	mul	r9,r22,r27   # C * H
	mul	r10,r23,r26  # D * G
	add	r9,r9,r10
	add	r9,r9,r11
	shi	r9,r19,36    # Store Pos2
	swi	r9,r19,32 
	lhui	r11,r19,32
	shi	r11,r19,34   # Store Pos1

# Hi (Store2) + B * H + C * G + D * F ==> Store3 ==> Pos0 and Pos1
	mul	r9,r21,r27  # B * H
	mul	r10,r22,r26 # C * G
	add	r9,r9,r11
	add	r9,r9,r10
	mul	r10,r23,r25 # D * F
	add	r9,r9,r10
	swi	r9,r19,32   # Pos0 and Pos1

# Hi (Store3) + A * H + B * G + C * F + D * E ==> Store3 ==> Pos0
	lhui	r11,r19,32  # Pos0
	mul	r9,r20,r27  # A * H
	mul	r10,r21,r26 # B * G
	add	r9,r9,r11
	add 	r9,r9,r10
	mul	r10,r22,r25 # C * F
	mul	r11,r23,r24 # D * E
	add	r9,r9,r10
	add	r9,r9,r11
	sext16	r9,r9       # Sign extend the MSB
	shi	r9,r19,32

# Move results to r3 and r4
	lwi	r3,r19,32  # Hi Part
	lwi	r4,r19,36  # Lo Part

# Restore Callee saved registers
	lw	r20,r19,r0
	lwi	r21,r19,4
	lwi	r22,r19,8
	lwi	r23,r19,12
	lwi	r24,r19,16
	lwi	r25,r19,20
	lwi	r26,r19,24
	lwi	r27,r19,28

# Restore Frame and return	
	rtsd	r15,8
	addi	r19,r19,40

.end C_SYMBOL_NAME(muldi3_hardproc)
	

