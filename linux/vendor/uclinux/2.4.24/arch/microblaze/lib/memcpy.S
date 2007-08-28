###################################-*-asm*- 
# 
# Copyright (c) 2001 Xilinx, Inc.  All rights reserved. 
# 
# Xilinx, Inc.  
# 
# memcpy.s 
#        
#       void* memcpy(void *dest, void *src, size_t n);
#
# Copies memory from one location to other, one byte at a time.
#	Input :	Destination address in reg r5
#		Source address in reg r6
#	Output : Desination in reg r3 	
# 
#######################################

#include <asm/clinkage.h>

	.globl	C_SYMBOL_NAME(memcpy)
	.ent	C_SYMBOL_NAME(memcpy)

C_SYMBOL_NAME(memcpy):
	.frame rsp,0,r15
	add 	r3,r0,r5
	beqid r7,$Lret
	addi r10,r0,0
	lbu   r18,r6,r10
$L170:
	sb  r18,r5,r10
	addi r10,r10,1
	rsub r18,r10,r7
	bneid r18,$L170
	lbu   r18,r6,r10
$Lret:		
	rtsd r15,8
	nop
	.end C_SYMBOL_NAME(memcpy)
	.align	2
