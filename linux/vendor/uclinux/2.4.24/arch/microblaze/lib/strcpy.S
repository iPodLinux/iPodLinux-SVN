###################################-*-asm*- 
# 
# Copyright (c) 2001 Xilinx, Inc.  All rights reserved. 
# 
# Xilinx, Inc.  
# 
# strcpy.s 
#
#      char* strcpy (char *dst, const char * src)
#
# Copy string from source to destination
#	Input :	Destination address in Reg r5
#		Source string address in Reg r6
#	Return: Destination address in Reg r3
# 
#######################################

#include <asm/clinkage.h>

	.globl	C_SYMBOL_NAME(strcpy)
	.ent	C_SYMBOL_NAME(strcpy)

C_SYMBOL_NAME(strcpy):
	.frame rsp,0,r15
	add	r3,r0,r5
	addi 	r10,r0,0
	lbu   	r18,r6,r10
	.LL170:
	sb  	r18,r5,r10
	addi 	r10,r10,1
	bneid 	r18,.LL170
	lbu   	r18,r6,r10
	rtsd 	r15,8
	nop
.end C_SYMBOL_NAME(strcpy)
	.align	2
