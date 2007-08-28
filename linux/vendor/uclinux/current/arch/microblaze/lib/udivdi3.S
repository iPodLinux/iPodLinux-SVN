###################################-*-asm*- 
# 
# Copyright (c) 2001 Xilinx, Inc.  All rights reserved. 
# 
# Xilinx, Inc.  
# 
# udivdi3.s 
# 
# Unsigned divide operation for 64 bit integers.
#	Input :	divisor[H] in Reg r5
#		divisor[L] in Reg r6		
#		dividend[H] in Reg r7
#		dividend[L] in Reg r8	
#	Output: Result [H] in Reg r3
#		Result [L] in Reg r4	
# 
#######################################

#include <asm/clinkage.h>

	.globl	C_SYMBOL_NAME(udivdi3_proc)
	.ent	C_SYMBOL_NAME(udivdi3_proc)
C_SYMBOL_NAME(udivdi3_proc):
	.frame	r1,0,r15	

#Change the stack pointer value and Save callee saved regs
	addi	rsp,rsp,-24
	swi	r25,rsp,0
	swi	r26,rsp,4
	swi	r27,rsp,8	# used for sign
	swi	r28,rsp,12	# used for loop count
	swi	r29,rsp,16	# Used for mod value High
	swi	r30,rsp,20	# Used for mod value Low

#Check for Zero Value in op1 and op2
	OR	r9,r5,r6			# Check for the divisor being zero
	BEQID	r9,$LaResult_Is_Zero		# Result is zero
	OR	r9,r7,r8			# Check for the dividend being zero
	BEQI	r9,$LaDiv_By_Zero	        # Div_by_Zero   # Division Error
$La2_Pos:
	ADDI	r30,r0,0			# Clear mod low
	ADDI	r29,r0,0                        # Clear mod high
	ADDI	r3,r0,0				# clear div high
	ADDI	r4,r0,0				# clear div low
	ADDI	r28,r0,64			# Initialize the loop count
   # First part try to find the first '1' in the r5/r6
$LaDIV0:
	BLTI	r5,$LaDIV2
$LaDIV1:
	ADD	r6,r6,r6
	ADDC	r5,r5,r5			# left shift logical r5
	BGEID	r5,$LaDIV1			
	ADDI	r28,r28,-1
$LaDIV2:
	ADD	r6,r6,r6
	ADDC	r5,r5,r5	# left shift logical r5/r6 get the '1' into the Carry
	ADDC	r30,r30,r30	# Move that bit into the Mod register
	ADDC	r29,r29,r29	# Move carry into high mod register
	rsub	r18,r7,r29	# Compare the High Parts of Mod and Divisor
	bnei	r18,$L_High_EQ
	rsub	r18,r6,r30	# Compare Low Parts only if Mod[h] == Divisor[h]
$L_High_EQ:	
	rSUB	r26,r8,r30	# Subtract divisor[L] from Mod[L]
	rsubc	r25,r7,r29	# Subtract divisor[H] from Mod[H]
	BLTi	r25,$LaMOD_TOO_SMALL
	OR	r29,r0,r25	# move r25 to mod [h]
	OR	r30,r0,r26	# move r26 to mod [l]
	ADDI	r4,r4,1
	ADDC	r3,r3,r0
$LaMOD_TOO_SMALL:
	ADDI	r28,r28,-1
	BEQi	r28,$LaLOOP_END
	ADD	r4,r4,r4		# Shift in the '1' into div [low]
	ADDC	r3,r3,r3		# Move the carry generated into high
	BRI	$LaDIV2   # Div2
$LaLOOP_END:
	BGEI	r27,$LaRETURN_HERE
	rsubi	r4,r4,0
	rsubc	r3,r3,r0
	BRI	$LaRETURN_HERE
$LaDiv_By_Zero:
$LaResult_Is_Zero:
	or	r3,r0,r0	# set result to 0 [High]
	or	r4,r0,r0	# set result to 0 [Low]
$LaRETURN_HERE:
# Restore values of CSRs and that of r3 and the divisor and the dividend
	
	lwi	r25,rsp,0
	lwi	r26,rsp,4
	lwi	r27,rsp,8
	lwi	r28,rsp,12
	lwi	r29,rsp,16
	lwi	r30,rsp,20
	rtsd	r15,8
	addi rsp,rsp,24
.end C_SYMBOL_NAME(udivdi3_proc)
	
