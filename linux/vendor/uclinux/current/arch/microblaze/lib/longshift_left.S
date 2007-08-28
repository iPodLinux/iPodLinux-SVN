###################################-*-asm*- 
# 

#include <asm/clinkage.h>

	.globl	C_SYMBOL_NAME(longshift_left)
	.ent	C_SYMBOL_NAME(longshift_left)

C_SYMBOL_NAME(longshift_left):
	beqi    r7, 2f
1:      add     r6, r6, r6
	addc    r5, r5, r5
	addi    r7, r7, -1
	bnei   r7, 1b
2:      add     r3,r0, r5 
	add     r4, r0, r6

.end C_SYMBOL_NAME(longshift_left)
	
