.text
.align 2
.global ___divsi3;
.type ___divsi3, STT_FUNC;
.global __divsi3;
.type __divsi3, STT_FUNC;

___divsi3:
__divsi3:
	[--SP] = RETS;
	CC = R0 < 0;
	IF CC JUMP num_neg;
	CC = R1 < 0;
	IF CC JUMP den_neg;
	CALL pos_ans;

num_neg:
	CC = R1 < 0;
	IF CC JUMP both_neg;
	R0 = -R0;
	CALL ___udivsi3;
	R0 = -R0;
	CALL done;

den_neg:
	R1 = -R1;
	CALL ___udivsi3;
	R0 = -R0;
	CALL done;

both_neg:
	R0 = -R0;
	R1 = -R1;

pos_ans:
	CALL ___udivsi3;
	CALL done;
	NOP;
done:
	RETS = [SP++];
	RTS;

.align 2
.global ___udivsi3;
.type ___udivsi3, STT_FUNC;
.global __udivsi3;
.type __udivsi3, STT_FUNC;

___udivsi3:
__udivsi3:
	[--SP] = RETS;
	[--SP] = R4;
	[--SP] = P4;
	P4 = 32;
	LSETUP (udiv_begin, udiv_end) LC0 = P4;
	R3 = 0;

udiv_begin:
	CALL lshft;
	R2 = R3;
	R3 = R3 - R1;
	CC = R3 < 0;
	IF !CC JUMP one_qb;
	R3 = R2;
	R2 = -2;
	R0 = R0 & R2;
	JUMP.S udiv_end;

one_qb:
	R2 = 1;
	R0 = R0 | R2;

udiv_end:
	NOP;
return:
	R1 = R3;
	P4 = [SP++];
	R4 = [SP++];
	RETS = [SP++];
	RTS;

lshft:
	R2 = R0;
	R4 = 31;
	R2 >>= R4;
	R4 = 1;
	R2 = R2 & R4;
	R3 <<= 0x1;
	R3 = R3 | R2;
	R0 <<= 0x1;
	RTS;


.global ___umodsi3;
.type ___umodsi3, STT_FUNC;
.global __umodsi3;
.type __umodsi3, STT_FUNC;

___umodsi3:
__umodsi3:
	LINK 0x0;
	[--SP] = (R7:6);
	R6 = R0;
	R7 = R1;
	CALL ___udivsi3;
	R7 *= R0;
	R0 = R6 - R7;
	( R7:6 ) = [SP++];
	UNLINK;
	RTS;


.global ___modsi3;
.type ___modsi3, STT_FUNC;
.global __modsi3;
.type __modsi3, STT_FUNC;

___modsi3:
__modsi3:
	LINK 0x0;
	[--SP] = (R7:6);
	R6 = R0;
	R7 = R1;
	CALL ___divsi3;
	R7 *= R0;
	R0 = R6 - R7;
	( R7:6 ) = [SP++];
	UNLINK;
	RTS;
