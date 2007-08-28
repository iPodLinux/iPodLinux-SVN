###################################-*-asm*- 
# 
# Copyright (c) 2001 Xilinx, Inc.  All rights reserved. 
# 
# Xilinx, Inc.  
# 
# memset.s 
# 
# prototype :	void* memset(void* mem, int c, size_t n)
#	Set memory location from "mem" thru "mem + n" with the value c [conv to char]
#	 
#	Input :	Source address in reg r5
#		Int value to be set in reg r6
#		Number of bytes in reg r7	
#	Return: Source address in reg r3.	
# 
#######################################

#include <asm/clinkage.h>
	.globl	C_SYMBOL_NAME(memset)
	.ent	C_SYMBOL_NAME(memset)

C_SYMBOL_NAME(memset):
	.frame rsp,0,r15
	add	r3,r5,r0
	beqid	r7,$Lret
	addi	r10,r0,0
$L170:
	sb	r6,r5,r10
	addi	r10,r10,1
	rsub	r18,r10,r7
	bnei	r18,$L170
$Lret:		
	rtsd	r15,8
	add	r3,r0,r5
	.end C_SYMBOL_NAME(memset)
	.align	2
