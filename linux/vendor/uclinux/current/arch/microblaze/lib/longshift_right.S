###################################-*-asm*- 
# 

#include <asm/clinkage.h>

	.globl	C_SYMBOL_NAME(longshift_right)
	.ent	C_SYMBOL_NAME(longshift_right)

C_SYMBOL_NAME(longshift_right):
	beqi    r7, 2f
1:      sra     r5, r5
	src     r6, r6
	addi    r7, r7, -1
	bnei   r7, 1b
2:      add     r3, r0, r5
	add     r4, r0, r6
.end C_SYMBOL_NAME(longshift_left)
	
