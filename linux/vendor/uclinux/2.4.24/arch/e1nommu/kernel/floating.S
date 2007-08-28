;----------------------------------------------------------------------------;
; Floating point emulation support for Hyperstone E1-32X-E1-16X              ;
; Copyright: hyperstone AG                                                   ;
;            Am Seerhein 8                                                   ;
;            D-7750 Konstanz, Germany                                        ;
; Date:      October 16, 2001                                                ;
;----------------------------------------------------------------------------;
; Changed:   96-09-24 cb                                                     ;
; - C flag is undefined after shl instructions                               ;
; - not all allowed NaN values were recognized                               ;
; - FADD, FADDD: inexact flag was not set in some cases where it should be   ;
; - FCMP...: comparison with infinity should deliver <=>, not unordered      ;
; - FCVT: if 0 < |num| < 2**-1042, inexact and underflow were not set        ;
; - FMULD: x*0 did not return zero                                           ;
; - FDIVD: wrong remainder calculation for rounding                          ;
; - FADDD: exp.diff 54 needs still be added                                  ;
;----------------------------------------------------------------------------;
; uClinux comments by G.Thanos                                               ;
; -Since we dont change stacks when excecuting an FP                         ;
;  instruction we have operate with interrupts locked!!!                     ;
;----------------------------------------------------------------------------;
	    .global     FloatErrorEntry

	    .global     StartOfFPRoutines
	    .global     EndOfFPRoutines
	    .global     FADDEntry
	    .global     FSUBEntry
	    .global     FMULEntry
	    .global     FDIVEntry
	    .global     FADDDEntry
	    .global     FSUBDEntry
	    .global     FMULDEntry
	    .global     FDIVDEntry
	    .global     FCMPEntry
	    .global     FCMPUEntry
	    .global     FCMPDEntry
	    .global     FCMPUDEntry
	    .global     FCVTEntry
	    .global     FCVTDEntry
	    .global     EXEntry
	    .global     DOEntry
;
;       *** exception numbers ***
;
;       the real exception number of every operation
;       and exception
;       is the following sum
;       exception number = Offset**** + ExNr*****
;       example:
;         error number of FADDD with inexact exception
;         exception number = OffsetAddd + ExNrInex

#define OffsetAdd         0                         ; fadd
#define OffsetSub         4                         ; fsub
#define OffsetMul         8                         ; fmul
#define OffsetDiv         12                        ; fdiv
#define OffsetAddd        17                        ; faddd
#define OffsetSubd        21                        ; fsubd
#define OffsetMuld        25                        ; fmuld
#define OffsetDivd        29                        ; fdivd
#define OffsetCmp         34                        ; fcvtd
#define OffsetCmpd        35                        ; fcmp
#define OffsetCvtd        36                        ; fcmpd
#define OffsetCvt         37                        ; fcvt
;
;
;
#define ExNrInv           1                         ; additional err.number invalid
#define ExNrInex          2                         ; .... inexact
#define ExNrOver          3                         ; .... overflow
#define ExNrUnder         4                         ; .... underflow
#define ExNrZero          5                         ; .... division by zero
;
#define NANq              0x7FF80000                ; quiet Not a Number
#define NANs              0x7FF80001                ; signaling Not a Number
;
;
;
;
#define LFLAG             (1 << 15)                 ; interrupt-lock flag

;============================================================================;


StartOfFPRoutines:

#ifdef NOFLOAT

FADDEntry:
FSUBEntry:
FMULEntry:
FDIVEntry:
FADDDEntry:
FSUBDEntry:
FMULDEntry:
FDIVDEntry:
FCMPEntry:
FCMPUEntry:
FCMPDEntry:
FCMPUDEntry:
FCVTEntry:
FCVTDEntry:
EXEntry:
DOEntry:

	     FRAME  L8, L0
	     MOVI   L7, 0             ; errorcode = not implemented
	     SUM    PC, PC, FloatErrorEntry - $
	     RET    PC, L3

EndOfFPRoutines:

#else

;============================================================================;
;
; Register Layout:
;
;       L0 : ADDRESS of LD
;       L1 : LS high
;       L2 : LS low | TEMP
;       L3 : PC
;       L4 : SR
;       L5 : LD high
;       L6 : LD low
;       L7 : LS.EXP | result.sign
;       L8 : LD.EXP
;       L9 : EXP
;       L10: Sign, if result equal zero
;       L11: LS (saving)
;       L12: used as exception dummy
;       L13: Exceptionnumber
;
FSUBEntry:   FRAME  L14, L0
	     MOVI   L13, OffsetSub
	     DBR    ADDE
	     XORI   L1, 0x80000000     ; negate LS, LS = - LS
FADDEntry:   FRAME  L14, L0           ; prolog
	     MOVI   L13, OffsetAdd
ADDE:        CMPBI  L4, LFLAG         ; ReturnSR.LockFlag set ?
	     DBNZ   DelActExc         ; Yes: --> DelActExc
	     MOV    L11, L1
;	     ANDNI  SR, LFLAG         ; No : remove lock flag
DelActExc:   ANDNI  G2, 0x00001F00     ; delete actual exceptions
	     LDW.S  L0, L5, 0         ; load LD
	     MOV    L8, L5            ; ld.exp = ld
	     ADD    L5, L5            ; remove sign.ld
	     SETC   L10               ; save sign
	     CMPI   L5, 0xFF000000     ; if ld infinity or nan
	     BHE    NAN_OR_INFD       ; yes, branch
	     SET0   L6                ; set ld.low = 0
	     CMPBI  L5, 0xFF000000     ; test ld.exp
	     DBZ    DEN0              ; branch if denormal
	     SHLI   L5, 8             ; remove exp.ld
	     SHRI   L5, 1             ; else correct ld.mantisse
	     ORI    L5, 0x80000000     ; insert hidden bit
DEN0:        SHRI   L5, 2             ; correct position of ld
	     ADD    L8, L8            ; remove ld.sign
	     DBNC   POSITIV0          ; positive, then branch
	     SHRI   L8, 24            ;  correct position of ld.exp
	     NEG    L5, L5            ; negate ld
POSITIV0:
	     MOV    L7, L1            ; ls.exp = ls
	     ADD    L1, L1            ; remove sign.ls
	     ADD    L10, C            ; save ls.sign+ld.sign
	     CMPI   L1, 0xFF000000     ; if ls = nan or inf
	     BHE    NAN_OR_INFS       ; yes, branch
	     MOV    L9, L8            ; temp = ld.exp
	     CMPBI  L1, 0xFF000000     ; test ls.exp
	     DBZ    DEN1              ; branch if denormal
	     SHLI   L1, 8             ; remove exp.ls
	     SHRI   L1, 1             ; else correct ls.exp
	     ORI    L1, 0x80000000     ; insert hidden bit
DEN1:        SHRI   L1, 2             ; correct position of ls
	     ADD    L7, L7            ; remove ls.sign
	     DBNC   POSITIV1          ; positive, then branch
	     SHRI   L7, 24            ;  correct position of ls.exp
	     NEG    L1, L1            ; negate ls
POSITIV1:
	     SUB    L9, L7            ; EXP = ld.exp - ls.exp
	     DBLT   LS_GREATER        ; if exp < 0 then ls_greater
	     SET0   L2                ; lsf = 0
	     CMPI   L9, 32            ; if exp.diff > 32
	     BGE    NO_ADD1           ; do not add
	     DBR    REAL_ADD
	     SARD   L1,L9             ; align ls by exp.diff
NO_ADD1:     CMPBI  SR, 0x00006000     ; else, check rounding mode
	     BNZ    LD_TO_HIGH        ; if rm<>0 branch
	     CMPI   L1, 0             ; check if result inexact
	     BZ     NoAddInex         ; branch if inexact
	     CMPBI  SR, 0x00000100
	     BNE    TRAP_INEX
	     ANDNI  G2,0x00001F00
	     ORI    G2, 1
NoAddInex:   RET    PC, L3

LS_GREATER:  MOV    L8, L7            ; EXP = ls.exp
	     NEG    L9, L9            ; correct EXP.diff
	     CMPI   L9, 32            ; if exp.diff > 32
	     BGE    NO_ADD2           ;   do not add
	     DBR    REAL_ADD          ;
	     SARD   L5,L9             ;
NO_ADD2:     CMPBI  SR,0x00006000      ; check rounding mode
	     BNZ    LS_TO_HIGH        ; if rm<>00 branch
	     MOV    L5, L5            ; check for ld != 0 (inexact)
	     BZ     REAL_ADD          ; branch if ld == 0
	     ORI    G2, 0x00000100     ; set inexact flag
	     SET0   L5                ; and set ld = 0; L6 is already 0
REAL_ADD:    ADD    L6, L2            ; ldf += lsf
	     ADDC   L5, L1            ; ld += ls
	     BZ     REAL_ZERO         ; if result == zero, then branch
	     DBNN   NONEG1            ; if ld == negativ, then correct
	     SETN   L7                ; save result.sign
	     NEG    L5, L5            ; correct ld
NONEG1:      TESTLZ L2, L5            ; check leading zeros
	     SHLD   L5, L2            ; align ld by tlz
	     SHRDI  L5,  8            ; correct ld by 8
	     SUB    L8, L2            ; correct exp by tlz
	     ADDI   L8, 2             ; correct ld.exp
	     BLE    UNDERFLOW         ;
CvtRound:    CMPI   L6, 0             ; ckeck if inexact
	     BE     AddNotInex        ;
	     ORI    G2, 0x00000100     ; set actual inexact.exception
AddNotInex:
	     CMPBI  SR, 0x00006000     ; check rounding mode '00'
	     BNZ    NOT_NEAREST       ; if rm<>00 branch
	     ADD    L6, L6            ; round to nearest, if ld.f >= 0.5
             ADDI   L5, CZ            ; perform rounding
CORRECT:     CMPBI  L5, 0xff000000     ; check for overflow during rounding
             BZ     Round_NOV_1       ; branch if no overflow
	     ADDI   L8, 1             ; correct exponent if overflow
Round_NOV_1: ANDNI  L5, 0xff800000     ; clear exponent part and hidden bit
	     CMPI   L8,255            ; check exp
	     BNN    OVERFLOW          ; if l8>255 branch
	     SHLI   L8, 23            ; correct ld.exp
	     OR     L5, L8            ; ld = ld.exp | ld.man
STORE_ADDS:  SHLI   L7, 31            ; shift sign
	     OR     L5, L7            ; ld = ld.sign | ld.exp | ld.man
	     CMPBI  G2, 0x00000100     ; ckeck if inexact.ex = 1
	     BZ     INEX_ADD          ; if result exact, then branch
	     CMPBI  SR, 0x00000100     ; check if inexact.trap=enable
	     BNE    TRAP_INEX         ; yes, then branch
	     ANDNI  G2, 0x00001F00     ; delete actual exceptions
	     ORI    G2, 1             ; set accrued inexact.exception
INEX_ADD:    STW.S  L0,L5,0
	     RET    PC, L3            ; leave

REAL_ZERO:   CMPI   L10,1             ; check result.sign
	     BN     ZERO_POS          ; ld.sign = ls.sign = (+)
	     BNZ    ZERO_NEG          ; ld.sign = ls.sign = (-)
	     CMPBI  SR, 0x00006000     ; check rounding mode 00
	     BNZ    CHECK_RM          ; if rm <> 00 branch
ZERO_POS:    STW.S  L0,  0, 0         ; store 0
	     RET    PC, L3            ; leave
ZERO_NEG:    MOVI   L1, 0x80000000     ; l1=-0
	     STW.S  L0, L1, 0         ; store -0
	     RET    PC, L3            ; leave
CHECK_RM:    CMPBI  SR,0x00002000      ; check rounding mode
	     BZ     ZERO_NEG          ; if SR(13)=0 round toward -infinity
	     STW.S  L0, 0, 0          ; store +0
	     RET    PC, L3            ; leave

NOT_NEAREST: CMPBI  SR, 0x00004000     ; check rounding mode 01, SR(14)
	     SETNZ  L10
	     CMPI   L6,0              ; check l6=0
	     BZ     CORRECT           ; if ldf=0 branch
	     CMPBI  SR, 0x00002000     ; check rm, SR(13)
	     SETNZ  L6                ;
	     XOR    L6, L7            ;
	     AND    L6, L10           ;
	     DBR    CORRECT
	     ADD    L5, L6            ;
LS_TO_HIGH:  MOV    L10,L1            ;
	     MOV    L1, L5
	     MOV    L5, L10
LD_TO_HIGH:  ADDI   L8, 2
	     CMPI   L5, 0             ; check sign
	     DBNN   NONEG2            ; if ld=negative correct
	     SETN   L7                ; store sign
	     NEG    L5,L5             ; correct ld
NONEG2:      TESTLZ L2, L5            ; check leading zeros
	     SHLD   L5, L2            ; align ld by tlz
	     SHRDI  L5,  8            ; correct ld by 8
	     SUB    L8, L2            ; correct exp by tlz
	     CMPI   L1,0              ; check ls.mant
	     BZ     CORRECT           ; else leave
	     SETN   L6                ; l6=ls.sign
	     ORI    G2, 0x00000100     ; set inexact.exception
	     CMPBI  SR, 0x00004000     ; check SR(14)
	     SETNZ  L9
	     CMPBI  SR,0x00002000      ; check SR(13)
	     SETNZ  L10               ;
	     XOR    L10,L7            ;
	     AND    L10,L9           ;
	     XOR    L6, L7            ;
	     BNZ    SIGN_DIFF         ; if ls.sign <> ld.sign
	     DBR    CORRECT
	     ADD    L5, L10           ; ld.mant += l10

SIGN_DIFF:   SHRI   L10, 1            ;
	     SETNC  L10               ;
	     SUB    L5, L10           ;
	     CMPBI  L5, 0xff800000     ;
	     SETZ   L10               ;
	     SUB    L8,L10            ; correct exponent
	     ANDNI  L5, 0xff800000     ;
	     ORI    L5, 0x00800000     ;
	     BR     CORRECT
NAN_OR_INFD:
	     BHT    NanLd             ; ld = nan
	     ADD    L1, L1            ; ld = inf, check ls.sign
	     SETC   L2                ; save ls.sign
	     CMPI   L1, 0xFF000000     ; if l1=nan or inf
	     BHE    InfLs             ; yes, branch
	     RET    PC, L3            ; ld=inf, ls=normal -> return ld
InfLs:       BHT    NanLs             ; branch, if ls=nan
	     CMP    L10, L2           ; ld=ls=inf, compare ls.sign and ld.sign
	     BNE    STORE_NAN_S       ; signaling nan, invalid op.
	     RET    PC, L3            ; leave, inf+inf -> inf

NanLd:       SHRI   L5, 2             ; if ld=NaNs
	     BC     STORE_NAN_S       ; yes, branch, NaNs
	     ANDNI  L1, 0x807ffffe     ; check for ls=NaNs
	     CMPI   L1, 0x7f800001     ;
	     BZ     STORE_NAN_S       ; yes, branch
	     BR     STORE_NAN_Q       ; store NANq

NAN_OR_INFS: BHT    NanLs             ; ls = nan, ld=normal
	     STW.S  L0, L11, 0        ; store ls=+-infinity
	     RET    PC, L3
NanLs:       SHRI   L1, 2             ; if ls = NANs
	     BC     STORE_NAN_S       ; yes, exception
	     BR     STORE_NAN_Q       ; LD = NANq

STORE_NAN_S: CMPBI  SR, 0x00001000     ; if invalid.floating trap = enable
	     BE     NOINV_TRAP        ; no, branch
	     ANDNI  G2, 0x00001F00     ; delete actual exception
	     ORI    G2, 0x00001000     ; set actual.invalid.exception
	     MOVI   L5, NANs
	     ADDI   L13, ExNrInv      ; correct exceptionnumber
	     MOVD   L1, L10
	     MOV    L7, L13
	     SUM    PC, PC, FloatErrorEntry - $

NOINV_TRAP:  ANDNI  G2, 0x00001F00      ; clear actual exceptions
	     ORI    G2, 0x00000010     ; set accrued.invalid.exception
STORE_NAN_Q: MOVI   L5, NANq          ; LD = NANq
	     STW.S  L0, L5, 0         ; store LD
	     RET    PC, L3            ; leave
;
;
; Register Layout:
;
;       L0 : ADDRESS of LD
;       L1 : LS high
;       L2 : TEMP
;       L3 : PC
;       L4 : SR
;       L5 : LD high
;       L6 : LD low
;       L7 : SIGN LS
;       L8 : EXP
;       L9 : TEMP
;       L10: TEMP
;       L11: LS (saving)
;       L12: Exception dummy
;       L13: Exceptionnumber
;
;  A * B --> A.m * B.m * (A.exp + B.exp)
;  Single Floating Point multiplication
;

FMULEntry:   FRAME  L14, L0            ; prolog
	     MOVI   L13, OffsetMul
	     CMPBI  L4, LFLAG          ; ReturnSR.LockFlag set ?
	     DBNZ   FMULDelAExc        ; Yes: --> FMULDelAExc
	     MOV    L11, L1
	     ANDNI  SR,LFLAG           ; No : remove lock flag
FMULDelAExc: ANDNI  G2,0x00001F00       ; delete actual exceptions
	     LDW.S  L0, L5, 0          ; load LD
	     ADD    L5, L5             ; remove ld.sign
	     SETC   L7                 ; save ld.sign
	     CMPI   L5, 0xFF000000      ; if LD == (NAN OR INF)
	     BNC    NAN_INF_DM         ; yes, branch
	     ADD    L1, L1             ; remove ls.sign
	     ADD    L7, C              ; LS: Store Sign Bit
	     CMPI   L1, 0xFF000000      ; if LS == (NAN OR INF)
	     BNC    NAN_INF_SM         ; yes, branch
	     MOV    L8, L5             ; EXP = LD.EXP
	     SHLI   L5, 7              ; ld.man = ld - ld.exp
	     SHRI   L8, 24             ; if (EXP = real EXP) == 0
	     BNZ    HIDDEN1            ; yes, branch
	     TESTLZ L2, L5             ;
	     CMPI   L2, 32             ; check if l2>=32
	     BZ     Hidden1Zero        ;
	     SHL    L5, L2             ;
	     SHRI   L5, 1              ;
	     SUB    L8, L2             ; correct ld.exp
	     ADDI   L8, 1
	     BR     Hidden1Zero

HIDDEN1:     ORI    L5, 0x80000000      ; insert hidden bit
	     SHRI   L5, 1              ; correct ld
Hidden1Zero:
	     MOV    L2, L1             ; Temp = ls.exp
	     SHLI   L1, 7              ; ld.man = ld - ld.exp
	     SHRI   L2, 24             ; if (TEMP = real LS.EXP) == 0
	     BNZ    HIDDEN2            ; yes, branch
	     TESTLZ L6, L1             ;
	     CMPI   L6, 32             ;
	     BZ     Hidden2Zero        ;
	     SHL    L1, L6
	     SHRI   L1, 8
	     SUB    L2, L6
	     ADDI   L2, 1
	     BR     Hidden2Zero

HIDDEN2:     ORI    L1, 0x80000000      ; insert hidden bit
	     SHRI   L1, 8              ; correct ls
Hidden2Zero:
	     ADD    L8, L2
	     MULS   L5, L1             ; LD = LD * LS
	     BZ     STORE_ZERO         ; if LD == 0 then store it
	     TESTLZ L2, L5             ; count first zero bits
	     ADDI   L2, -8             ; 8 must be free
	     SHLD   L5, L2             ; align first bit
	     SUB    L8, L2             ;
	     ADDI   L8, -126 + 1       ; EXP += -126 + (64-2*48-8-7)
	     BLE    UNDERFLOW          ;
	     CMPI   L6, 0              ; check if inexact
	     BZ     MulNotInex         ; if not inexact branch
	     ORI    G2, 0x00000100      ; set inexact.exception
MulNotInex:
             BR     AddNotInex

STORE_ZERO:  SHLI   L7, 31             ; right position of sign
	     OR     L5, L7             ; ld.sign | zero
	     STW.S  L0, L5, 0          ; store ld
	     RET    PC, L3             ; leave

NAN_INF_SM:  BZ     INF_SM             ; if LS == INF then branch
	     SHRI   L1, 2              ; check, if ls=NANs
	     BC     STORE_NAN_S        ;
	     BR     STORE_NAN_Q        ;
INF_SM:      CMPI   L5, 0              ; check if ld=0
	     BZ     STORE_NAN_S        ; yes, exception
	     BR     STORE_INF1         ; store +- infinity

NAN_INF_DM:  BHT    NanLdM             ; ld = nan
	     ADD    L1, L1             ; check and remove ls.sign
	     BZ     STORE_NAN_S        ; if l1=0 invalid operation
	     ADD    L7, C              ; store ls.sign
	     CMPI   L1, 0xFF000000      ;
	     BSE    STORE_INF1         ; ld=inf, branch if ls=inf or normal
NanLsM:      SHRI   L1, 2              ; check, if ls = NANs
	     BC     STORE_NAN_S        ; yes
	     BR     STORE_NAN_Q        ; no
NanLdM:      SHRI   L5, 2              ; check, if ld=NANS
	     BC     STORE_NAN_S        ; yes
	     ANDNI  L1, 0x807ffffe      ; check for ls=NaNs
	     CMPI   L1, 0x7f800001      ;
	     BZ     STORE_NAN_S        ; yes
	     BR     STORE_NAN_Q        ; no
;
;
; Register Layout:
;
;       L0 : ADDRESS of1 LD
;       L1 : LS high
;       L2 : TEMP
;       L3 : PC
;       L4 : SR
;       L5 : LD high
;       L6 : LD low
;       L7 : SIGN LS
;       L8 : EXP
;       L9 : TEMP
;       L10: TEMP
;       L11: LS (saving)
;       L12: Exception dummy
;       L13: Exceptionnumber
;
;  A / B --> A.m / B.m * (A.exp - B.exp)
;  Single Floating Point division
;
NO_HIDDEN3:  TESTLZ L2, L5             ; test leading zeros from ld
	     SUB    L8, L2             ; correct exp by leading zeros
	     ADDI   L8, 8              ; first 8 bits are zero
	     SHL    L5, L2             ; correct ld
	     SHRI   L5, 1              ; first bit must be zero
	     BR     SKIP3

NO_HIDDEN4:  TESTLZ L2, L1             ; test leadingt zeros from ls
	     ADD    L8, L2             ; correct exp by leading zeros
	     ADDI   L8, -8             ; first 8 bits are zero
	     DBR    SKIP4              ; leave denorm 2
	     SHL    L1, L2             ; correct ls

F_DIV_ZERO:  ADD    L5, L5             ; check and remove ld.sign
	     DBZ    STORE_NAN_S        ; if zero, then INF exception
	     ADD    L7, C              ; correct true sign for result
	     CMPI   L5, 0xFF000000      ; else if LD == NAN
	     BHT    NanLdD             ; then branch, check NANs or NANq
	     SHLI   L7, 31             ; correct position of sign
	     MOVI   L5, 0x7F800000      ;
	     OR     L5, L7             ; l5=+-infinity
	     CMPBI  SR, 0x00000800      ; if divison_by_zero.trap = enable
	     BZ     NO_DIV_TRAP        ; no, branch
	     ANDNI  G2, 0x00001F00      ; else, del all actual exceptions
	     ORI    G2, 0x00000800      ; set actual.division_by_zero.exception
	     ADDI   L13, ExNrZero      ; correct exceptionnumber
	     MOVD   L1, L11
	     MOV    L7, L13
	     SUM    PC, PC, FloatErrorEntry - $

NO_DIV_TRAP: ANDNI  G2, 0x00001F00
	     ORI    G2, 8
	     STW.S  L0, L5, 0
	     RET    PC, L3

FDIVEntry:   FRAME  L14, L0            ; prolog
	     MOVI   L13, OffsetDiv     ; basic exception offset, fdiv
	     CMPBI  L4, LFLAG          ; ReturnSR.LockFlag set ?
	     DBNZ   FDIVDelAExc        ; Yes: --> FDIVDelAExc
	     MOV    L11, L1            ; save ls
;	     ANDNI  SR, LFLAG          ; No : remove lock flag
FDIVDelAExc: ANDNI  G2, 0x00001F00      ; delete actual exceptions
	     LDW.S  L0, L5, 0          ; load LD
	     ADD    L1, L1             ; check and remove ls.sign
	     DBZ    F_DIV_ZERO         ; XX / 0 == undefined
	     SETC   L7                 ; save ls.sign
	     CMPI   L1, 0xFF000000      ; if LS == (NAN OR INF)
	     BNC    NAN_INF_S          ; yes, branch
	     ADD    L5, L5             ; check and remove ld.sign
	     DBZ    STORE_ZERO         ; if ld=0 store l7 | zero
	     ADD    L7, C              ;   LD: Store Sign Bit
	     CMPI   L5, 0xFF000000      ; if LD == (NAN OR INF)
	     BNC    NAN_INF_D          ; yes, branch
	     MOV    L8, L5             ; EXP = LD.EXP
	     SHRI   L8, 24             ; if (EXP = real EXP) == 0
	     BZ     NO_HIDDEN3         ; yes, branch
	     SHLI   L5, 7              ; ld.man = ld - ld.exp
	     ORI    L5, 0x80000000      ; insert hidden bit
	     SHRI   L5, 1              ; correct ld
SKIP3:
	     MOV    L2, L1             ; TEMP = LS.EXP
	     SHRI   L2, 24             ; if (TEMP = real LS.EXP) == 0
	     BZ     NO_HIDDEN4         ; yes, branch
	     SUB    L8, L2             ; EXP = LD.EXP - LS.EXP
	     SHLI   L1, 7              ; correct LS
	     ORI    L1, 0x80000000      ; insert hidden bit
SKIP4:
	     SET0   L6                 ; LDf = 0
	     DIVU   L5, L1             ; LD = LD//LDf / LS
	     MOV    L1, L5             ; l1 = remainder
	     MOV    L5, L6             ; LD = quotient
	     TESTLZ L2, L5             ; count first zero bits
	     SHL    L5, L2
	     SET0   L6
	     CMPI   L1, 0
	     BZ     NoRemainder
	     ORI    L6, 0x00000800
NoRemainder:
	     SHRDI  L5, 8              ; align first bit
	     SUB    L8, L2             ; correct EXP by shiftet numbers
	     ADDI   L8, 126 + 1        ; EXP += 126 + 8 - 7
	     BLE    UNDERFLOW          ; if ld.exp <= 0 then branch
	     CMPI   L6, 0              ;
	     BZ     DivNotInex         ;
	     ORI    G2, 0x00000100      ; set actual inexact.exception
DivNotInex:
	     BR     AddNotInex

NAN_INF_D:   BZ     STORE_INF1         ; else store +-INF
NanLdD:      ANDNI  L5, 0x00fffffc      ; ls=normal, check for ld=NaNs
	     CMPBI  L5, 0xff000002      ;
	     BZ     STORE_NAN_S        ; yes, exception
	     BR     STORE_NAN_Q        ; no, store ld=NANq

NAN_INF_S:   BZ     INF_S1             ; IF LS == INF then branch
	     SHRI   L1, 2              ; IF LS == NANq
	     BC     STORE_NAN_S        ; then branch, else LS == NANs
	     ANDNI  L5, 0x807ffffe      ; check for ld=NaNs
	     CMPI   L5, 0x7f800001      ;
	     BZ     STORE_NAN_S        ; no, branch exception
	     BR     STORE_NAN_Q        ; branch NANq

INF_S1:      ADD    L5, L5             ; check and remove ld.sign
	     ADD    L7, C              ; store ld.sign
	     CMPI   L5, 0xFF000000      ; CMP LD == (NAN OR INF)
	     BE     STORE_NAN_S        ; if LD == INF then branch
	     BHT    NanLdD             ; ld=nan
	     MOVI   L5, 0              ; store ld=+-0
	     SHLI   L7, 31             ;
	     OR     L5, L7             ;
	     STW.S  L0, L5, 0          ;
	     RET    PC, L3             ;

STORE_INF1:  MOVI   L5, 0x7F800000      ; LD = INF
	     SHLI   L7, 31             ; correct sign bit position
	     OR     L5, L7             ; insert sign bit
	     STW.S  L0, L5, 0          ; store LD
	     RET    PC, L3             ; leave
;
;
UNDERFLOW:
	     ORI    L5, 0x00800000     ; insert removed hidden bit
	     CMPBI  SR, 0x00000200     ; if underflow.trap = enable
	     BNE    TRAP_UNDER        ; yes, branch
	     ANDNI  G2, 0x00001F00     ; delete actual exceptions
	     ORI    G2, 2             ; set accrued.underflow.exception
	     NEG    L8, L8            ; correct exp, exp = -exp
	     CMPI   L8, 25            ; if exp.diff > 25
	     BLT    UNDER_1           ; yes, then branch
	     CMPBI  SR,0x00004000      ; check rm=00,01
	     DBNZ   UNDER_2           ; if rm <> 00,01 branch
	     SET0   L5
	     BR     SIGNED_ZERO

UNDER_1:     MOV    L1, L7            ; store sign bit
	     SET0   L7
	     ADDI   L8, 1             ;
	     SHRD   L6, L8            ; shift l6//l7
	     SHL    L6, L8            ;
	     SHRD   L5, L8            ; triple shift
	     SET0   L8                ;

	     SHLDI  L6, 0             ;
	     BZ     UNDER_3

	     CMPBI  SR,0x00000100      ;
	     BNE    TRAP_INEX

	     ORI    G2, 1
	     CMPBI  SR, 0x00006000     ; ckeck if rm=00
	     BNZ    UnderNotNear      ; if rm <> 00 branch
	     ADD    L7, L7            ; round to nearest-even
	     ADDC   L6, L6
	     ADDI   L5, CZ            ; perform rounding
CheckHid:    CMPBI  L5, 0xff000000     ; check for overflow during rounding
             BZ     Round_NOV_2       ; branch if no overflow
	     ADDI   L8, 1             ; correct exponent if overflow
Round_NOV_2: ANDNI  L5, 0xff800000     ; clear exponent part and hidden bit
UNDER_3:     DBR    CORRECT           ;
	     MOV    L7, L1            ;

UnderNotNear:
	     CMPBI  SR,0x00004000       ; ckeck rounding mode
	     SETNZ  L6
	     CMPBI  SR,0x00002000       ; ckeck rounding mode
	     SETNZ  L7
	     XOR    L7, L1
	     AND    L7, L6
	     DBR    CheckHid
	     ADD    L5, L7

UNDER_2:     CMPBI  SR,0x00002000      ; check SR(13)
	     SETNZ  L6
	     XOR    L6,L7             ;
	     ADD    L5,L6
SIGNED_ZERO:
	     SHLI   L7,31             ;
	     OR     L5,L7             ;
	     CMPBI  SR,0x00000100      ;
	     BNE    TRAP_INEX
	     STW.S  L0,L5,0
	     ANDNI  G2,0x00001F00
	     ORI    G2, 1
	     RET    PC,L3

TRAP_UNDER:  ANDNI  G2, 0x00001F00     ; del all actual exceptions
	     ORI    G2, 0x00000200     ; set actual.underflow.exception
	     ADDI   L8, 192           ; correct bias for trap-handler
	     TESTLZ L2, L5
	     ADDI   L2, -8
	     SUB    L8, L2
	     SHLD   L5, L2
	     CMPBI  SR, 0x00006000
	     BNZ    TrapNotNear
	     ADD    L6, L6            ; round to nearest-even
	     ADDI   L5, CZ            ; perform rounding
TrapStore:   CMPBI  L5, 0xff000000     ; check for overflow during rounding
             BZ     Round_NOV_3       ; branch if no overflow
	     ADDI   L8, 1             ; correct exponent if overflow
Round_NOV_3: ANDNI  L5, 0xff800000     ; clear exponent part and hidden bit
	     SHRI   L8, 23
	     SHRI   L7, 31
	     OR     L5, L8
	     OR     L5, L7
	     ADDI   L13, ExNrUnder    ; correct errornumber
	     MOVD   L1, L11
	     MOV    L7, L13
	     SUM    PC, PC, FloatErrorEntry - $

TrapNotNear:
	     CMPBI  SR, 0x00004000     ; check rounding mode 01, SR(14)
	     SETNZ  L10
	     CMPI   L6,0              ; check l6=0
	     BZ     TrapStore         ; if ldf=0 branch
	     CMPBI  SR, 0x00002000     ; check rm, SR(13)
	     SETNZ  L6
	     XOR    L6, L7            ;
	     AND    L6, L10           ;
	     DBR    TrapStore
	     ADD    L5, L6            ;
INEXACT:
	     SHLI   L7, 31            ; correct position of sign
	     CMPBI  SR, 0x00000100     ; if inexact.trap = enable
	     DBNE   TRAP_INEX         ; yes, skip
	     OR     L5, L7
	     STW.S  L0, L5, 0
	     ANDNI  G2, 0x00001F00
	     ORI    G2, 1             ; set accrued.inexact.exception
	     RET    PC, L3
TRAP_INEX:   ANDNI  G2,0x00001F00      ; clear actual exceptions
	     ORI    G2,0x00000100      ; set actual inexact.exception
	     ADDI   L13, ExNrInex
	     MOVD   L1, L11
	     MOV    L7, L13
	     SUM    PC, PC, FloatErrorEntry - $

OVERFLOW:    CMPBI  SR, 0x00000400     ; if overfow.trap = enable
	     BNE    TRAP_OVER         ; yes, branch
	     CMPBI  SR, 0x00000100     ; if inexact.trap = enable
	     BNE    TRAP_INEX         ; yes, branch
	     ANDNI  G2, 0x00001F00     ;
	     ORI    G2, 5             ; set accrued.overflow exception
	     CMPBI  SR, 0x00006000     ; check rm=00
	     BNZ    O_NOT_NEAR        ; if rm <> 0 branch
	     MOVI   L5, 0x7F800000     ; round to nearest, ld=inf
	     SHLI   L7, 31            ; extract correct sign
	     XOR    L5, L7            ; insert correct sign into LD
	     STW.S  L0, L5, 0         ; store LD
	     RET    PC, L3            ; leave

O_NOT_NEAR:  MOVI   L5,0x7F7FFFFF
	     CMPBI  SR,0x00004000
	     BZ     OVER_RM1
	     CMPBI  SR,0x00002000
	     SETNZ  L1
	     XOR    L1,L7
	     ADD    L5,L1
OVER_RM1:    SHLI   L7,31
	     OR     L5,L7
	     STW.S  L0,L5,0
	     RET    PC,L3

TRAP_OVER:   ANDNI  G2, 0x00001F00     ; delete actual exception
	     ORI    G2, 0x00000400     ; set actual.overflow.exception
	     ORI    G2, 1
	     ADDI   L8, -192          ; correct bias for trap-handler
	     CMPBI  SR, 0x00006000
	     BNZ    TrapOverNNear
	     ADD    L6, L6            ; round to nearest-even
	     ADDI   L5, CZ            ; perform rounding
TrapOverSt:  CMPBI  L5, 0xff000000     ; check for overflow during rounding
             BZ     Round_NOV_4       ; branch if no overflow
	     ADDI   L8, 1             ; correct exponent if overflow
Round_NOV_4: ANDNI  L5, 0xff800000     ; clear exponent part and hidden bit
	     SHRI   L8, 23
	     SHRI   L7, 31
	     OR     L5, L8
	     OR     L5, L7
	     ADDI   L13, ExNrOver
	     MOVD   L1, L11
	     MOV    L7, L13
	     SUM    PC, PC, FloatErrorEntry - $

TrapOverNNear:
	     CMPBI  SR, 0x00004000     ; check rounding mode 01, SR(14)
	     SETNZ  L10
	     CMPI   L6,0              ; check l6=0
	     BZ     TrapOverSt        ; if ldf=0 branch
	     CMPBI  SR, 0x00002000     ; check rm, SR(13)
	     SETNZ  L6
	     XOR    L6, L7            ;
	     AND    L6, L10           ;
	     DBR    TrapOverSt
	     ADD    L5, L6            ;
;
; Register Layout:
;
;       L0 : ADDRESS of LD
;       L1 : LS
;       L2 : sign LS
;       L3 : PC
;       L4 : SR
;       L5 : LD | Exception dummy
;       L6 : sign LD | Exception dummy
;       L7 : LS (saving)
;       L8 : Exception dummy
;       L9 : Exceptionnumber
;       L10: Exception dummy
;
;  Single Floating Point compare
;
FCMPEntry:   FRAME  L11, L0
	     MOVI   L9, OffsetCmp      ; basic exception number, fcmp
	     CMPBI  L4, LFLAG          ; ReturnSR.LockFlag det ?
	     DBNZ   FCMPDelAExc        ; YES: --> FCMPDelAExc
	     MOV    L7, L1
;	     ANDNI  SR, LFLAG          ; No, remove lock flag
FCMPDelAExc: ANDNI  G2, 0x00001F00      ; delete actual exceptions
	     LDW.S  L0, L5, 0          ; load LD
	     ADD    L1, L1             ; check and remove ls.sign
	     DBZ    tmp1
	     SET0   L2                 ; sign=0 for zero input
	     SETC   L2                 ; save ls.sign
	     CMPI   L1, 0xFF000000      ; test INF and NAN
	     DBHT   UNORDERED          ; branch if NaN
tmp1:	     ANDNI  L4, 0x0F            ; clear all flags

	     ADD    L5, L5             ; check and remove ld.sign
	     DBZ    CMP_Ordered
	     SET0   L6                 ; sign=0 for zero input
	     SETC   L6                 ; save ld.sign
	     CMPI   L5, 0xFF000000      ; test INF and NAN
	     BHT    UNORDERED          ; branch if NaN

CMP_Ordered: CMP    L2, L6             ; if ld.sign = ls.sign
	     BNE    DIFF_SIGN          ; no, branch
	     SHRI   L1, 1              ; correct ls
	     SHRI   L5, 1              ; correct ld
	     CMPI   L2, 1              ; if ls.sign = neg = ld.sign
	     DBE    BOTH_NEG0          ; yes,
	     CMP    L1, L5             ; then, compare ls, ld
	     CMP    L5, L1             ; else, compare ld, ls
BOTH_NEG0:   MASK   L2, SR, 0x07        ; copy flags to l2
	     OR     L4, L2             ; insert flags in SR
	     RET    PC, L3             ; leave fcmpu

DIFF_SIGN:   BGT    LD_GT_LS           ; if ls.sign > ld.sign
	     ORI    L4, 5              ; yes, insert flags
LD_GT_LS:    RET    PC, L3             ; leave fcmpu

UNORDERED:   ORI    G2, 0x00000010      ; set accrued.invalid.exception
	     ORI    L4, 0x0C            ; set flags
	     CMPBI  SR, 0x00001000      ; if invalid.floating trap = enable
	     BE     NOUNOR_TRAP        ; no, branch
	     ADDI   L9, ExNrInv        ; add invalid exception number
	     ANDNI  G2, 0x00001F00      ; else, del all actual exceptions
	     ORI    G2, 0x00001000      ; set actual.invalid.exception
	     MOVD   L1, L7
	     MOV    L7, L9
	     SUM    PC, PC, FloatErrorEntry - $

NOUNOR_TRAP: RET    PC, L3             ; leave
;
;
; Register Layout:
;
;       L0 : ADDRESS of LD
;       L1 : LS high
;       L2 : sign LS
;       L3 : PC
;       L4 : SR
;       L5 : LD high
;       L6 : sign LD
;
;  Single Floating Point compare unordered
;
FCMPUEntry:  FRAME  L7, L0
	     CMPBI  L4, LFLAG          ; ReturnSR-LockFlag set ?
	     BNZ    LoadLD             ; Yes: --> LoadLD
;	     ANDNI  SR, LFLAG          ; No : Remove LFlag
LoadLD:      LDW.S  L0, L5, 0          ; load LD
	     ADD    L1, L1             ; check and remove ls.sign
	     DBZ    tmp3
	     SET0   L2                 ; sign=0 for zero input
	     SETC   L2                 ; save ls.sign
	     CMPI   L1, 0xFF000000      ; test INF and NAN
	     DBHT   UNORDERED1         ; branch if NaN
tmp3:	     ANDNI  L4, 0x0F            ; clear all flags
	     
	     ADD    L5, L5             ; check and remove ld.sign
	     DBZ    tmp4
	     SET0   L6                 ; sign=0 for zero input
	     SETC   L6                 ; save ld.sign
	     CMPI   L5, 0xFF000000      ; test INF and NAN
	     BHT    UNORDERED1         ; branch if NaN
tmp4:	     BR     CMP_Ordered

UNORDERED1:  ORI    G2, 0x00000010      ; set accrued.invalid.exception
	     ORI    L4, 0x0C            ; set flags
	     RET    PC, L3             ; leave

; Register Layout:
;
;       L0 : ADDRESS of LD
;       L1 : LS high
;       L2 : LS low
;       L3 : PC
;       L4 : SR
;       L5 : LD high
;       L6 : LD low
;       L7 : SIGN LS
;       L8 : SIGN LD
;       L9 : LS high (saving)
;       L10: LS low (saving)
;       L11: Exceptionnumber
;
;  Double Floating Point compare
;

FCMPDEntry:  FRAME  L12, L0
	     CMPBI  L4, LFLAG          ; ReturnSR.LockFlag set ?
	     DBNZ   GetExcNo           ; Yes: --> GetExcNo
	     MOVD   L9, L1
;	     ANDNI  SR, LFLAG          ; No : Remove LFLAG
GetExcNo:    MOVI   L11, OffsetCmpd    ; basic exception number offset
	     LDW.S  L0, L5, 4          ; load Ld
	     LDW.S  L0, L6, 0          ; load Ldf
	     ADD    L1, L1             ; check and remove ls.sign
	     DBZ    tmp5
	     SET0   L7                 ; sign=0 for zero input
	     SETC   L7                 ; save ls.sign
	     CMPI   L1, 0xFFE00000      ; test INF and NAN
	     DBHT   UNORDERED2         ; branch if NaN
tmp5:	     ANDNI  L4, 0xF

	     ADD    L5, L5             ; check ld.sign
	     DBZ    ForUnorder
	     SET0   L0                 ; sign=0 for zero input
	     SETC   L8                 ; save ld.sign
	     CMPI   L5, 0xFFE00000      ; test INF and NAN
	     BHT    UNORDERED2         ; branch if NaN

ForUnorder:  CMP    L7, L8             ; if ls.sign = ld.sign
	     BNE    DIFF_SIGN
	     CMPI   L8, 1
	     BE     AlNeg              ; if both numbers are negative

	     CMP    L5, L1
	     BST    SetNegC            ; if smaller then set N and C flag
	     BE     TmpEqP             ; if equal then test Ldf and Lsf
	     RET    PC, L3

UNORDERED2:  MOVD   L7, L9             ; move ls//lsf
	     DBR    UNORDERED
	     MOV    L9, L11            ; move exceptionnumber

TmpEqP:      CMP    L6, L2
	     BE     AlEqual            ; if equal then set zero flag
	     BST    SetNegC            ; if smaller then set N and C flag
	     RET    PC, L3


AlNeg:       CMP    L5, L1
	     BHT    SetNegC            ; if higher then set N and C flag
	     BE     TmpEq              ; if equal then test Ldf and Lsf
	     RET    PC, L3

TmpEq:       CMP    L6, L2
	     BE     AlEqual            ; if Ldf equal Lsf then set zero flag
	     BHT    SetNegC            ; if higher then set N and C flag
	     RET    PC, L3

AlEqual:     ORI    L4, 2              ; set zero flag
	     RET    PC, L3

SetNegC:     ORI    L4, 5              ; set negative and carry flag
	     RET    PC, L3


; Register Layout:
;
;       L0 : ADDRESS of LD
;       L1 : LS high
;       L2 : LS low
;       L3 : PC
;       L4 : SR
;       L5 : LD high
;       L6 : LD low
;       L7 : SIGN LS
;       L8 : SIGN LD
;
;  Double Floating Point compare unoredered
;

FCMPUDEntry: FRAME  L9, L0
	     CMPBI  L4, LFLAG          ; ReturnSR.LockFlag set ?
	     BNZ    FCMPSHL            ; Yes: --> FCMPSHL
;	     ANDNI  SR, LFLAG          ; No : Remove LockFlag
FCMPSHL:     LDW.S  L0, L5, 4          ; load Ld
	     LDW.S  L0, L6, 0          ; load Ldf
	     ADD    L1, L1             ; check and remove ls.sign
	     DBZ    tmp7
	     SET0   L7                 ; sign=0 for zero input
	     SETC   L7                 ; save ls.sign
	     CMPI   L1, 0xFFE00000      ; test INF and NAN
	     DBHT   UNORDERED1         ; branch if NaN
tmp7:	     ANDNI  L4, 0xF             ; clear all flags

	     ADD    L5, L5             ; check and remove ld.sign
	     DBZ    ForUnorder
	     SET0   L8                 ; sign=0 for zero input
	     SETC   L8                 ; save ld.sign
	     CMPI   L5, 0xFFE00000      ; test INF and NAN
	     BHT    UNORDERED1         ; branch if NaN
	     BR     ForUnorder
;
;
; Register Layout:
;
;       L0 : ADDRESS of LD
;       L1 : LS high
;       L2 : LS low | sign LS
;       L3 : PC
;       L4 : SR
;       L5 : LD high
;       L6 : LD low
;       L7 : Result sign
;       L8 : LD.EXP
;       L9 : Rounding Mode
;       L10: Exception dummy
;       L11: LS high (saving)
;       L12: LS low (saving)
;       L13: Exceptionnumber
;
;  Single Floating Point FCVT
;  Conversion Double to Single Precision
;
FCVTEntry:   FRAME  L14,L0             ; prolog
	     MOVI   L13, OffsetCvt     ; basic exc.nr.offset of fcvt
	     CMPBI  L4, LFLAG          ; ReturnSR.LockFlag set ?
	     BNZ    SaveLS             ; Yes: --> SaveLS
;	     ANDNI  SR, LFLAG          ; remove lock flag
SaveLS:      MOVD   L11, L1            ; save ls//lsf
	     MOVD   L5, L1             ; store mantissa
	     SETN   L7                 ; store sign bit
	     MOV    L8, L1             ; store exp
	     ANDNI  L8, 0x80000000      ; remove sign
	     BNZ    CvtNonZero
	     MOV    L2, L2             ; ls.high is zero, check ls.low
	     BZ     CvtZero            ; ls=0, store zero
CvtNonZero:  SHRI   L8, 20             ; extract exp
	     CMPI   L8, 0x000007FF      ; check nan or inf
	     BZ     CVT_NANINF         ; if nan or inf branch
	     ADDI   L8, 127-1023       ; transformed exp
	     BLE    CVT_UNDER          ; if denormalized or zero branch
	     CMPI   L8, 256            ; check overflow
	     BHE    OVERFLOW           ; if overflow branch
	     SHLDI  L5, 11             ; remove exp
	     ORI    L5, 0x80000000      ; insert hidden bit
	     SHRDI  L5, 8
	     BR     CvtRound
CvtZero:     STW.S  L0, L5, 0
	     RET    PC, L3

CVT_UNDER:   SHLDI  L5, 12             ; remove exp
	     SHRDI  L5, 9              ; correct position
	     BR     UNDERFLOW

CVT_NANINF:  SHLDI  L5, 12             ; extract mantissa
	     BZ     CVT_INF            ; if mantissa=0 branch
	     CMPI   L5, 0x00001000      ; check NANq
	     BZ     STORE_NAN_Q        ; if NANq branch
	     BR     STORE_NAN_S        ; branch to NANs

CVT_INF:     SHLI   L7, 31             ; correct position of sign
	     OR     L5, L7             ; = MOV L5, L7
	     ORI    L5, 0x7F800000      ; correct exp
	     STW.S  L0, L5, 0          ; store signed inf
	     RET    PC, L3
;
; Register Layout:
;
;       L0 : ADDRESS of LD
;       L1 : LS high
;       L2 : sign LS
;       L3 : PC
;       L4 : SR
;       L5 : LD high
;       L6 : LD low
;       L7 : LS (saving)
;       L8 : Exception dummy
;       L9 : Exceptionnumber
;       L10: Exception dummy
;
;  Single Floating Point FCVTD
;     Conversion Single to Double Precision
;
FCVTDEntry:  FRAME  L11, L0            ;
	     CMPBI  L4, LFLAG          ; ReturnSR.LockFlag set ?
	     BNZ    GetOffset          ; Yes: --> GetOffset
;	     ANDNI  SR, LFLAG          ; No : Remove LFLAG
GetOffset:   MOVI   L9, OffsetCvtd     ; basic ex.nr.offset, fcvtd
             ADD    L1, L1             ; check and remove ls.sign
	     SETC   L2                 ; save ls.sign
	     CMPI   L1, 0xFF000000      ; IF LS == NAN OR INF
	     BHE    FCvtdNan           ; yes, branch
	     MOV    L5, L1             ; LD = LS
	     SHRI   L5, 24             ; extract real EXP
	     DBNZ   StrNFCvtd          ; if exp > 0 then branch
	     MOV    L6, L1             ; mov matisee to LDf
	     BE     StoreFCvtd         ; if exp == 0 and mant == 0 then branch
	     TESTLZ L5, L6             ; count leading zeros
	     ADDI   L5, -7             ; seven must be free
	     SHL    L6, L5             ; correct fraction
	     ADDI   L5, -1             ; correct exp
	     NEG    L5, L5             ; ready

StrNFCvtd:   ADDI   L5, 1023 - 127     ; correct EXP for double
	     SHLI   L6, 8              ; remove exp
	     SHLDI  L5, 20             ; insert mantisse into LD
StoreFCvtd:  SHLI   L2, 31             ; correct sign position
	     OR     L5, L2             ; insert sign
	     STW.S  L0, L5, 4          ; store LD
	     STW.S  L0, L6, 0          ; store LDf
	     RET    PC, L3             ; leave
;
;************************************************
;
;
FCvtdNan:    BNZ    FCvtdNan1          ; branch if LS == NAN
	     MOVI   L5, 0x7FF00000      ; store INF
	     MOVI   L6, 0              ; LDF = 0
	     BR     StoreFCvtd

FCvtdNan1:   CMPBI  L1, 2              ; if LS == INFq
	     DBNE   CvtNanS            ; no, store NANs
	     ADDI   L0, 4              ; correct pointer
	     BR     CvtNanQ
NoCvtDTrap:  ORI    G2, 0x00000010     ; set accrued.invalid.exception
	     ANDNI  G2, 0x00001F00
CvtNanQ:     MOVI   L5, 0
	     STW.S  L0, L5, -4
	     MOVI   L5, NANq          ; LD = NANq
	     STW.S  L0, L5, 0         ; store NANq
	     RET    PC, L3            ; leave

CvtNanS:     CMPBI  SR, 0x00001000     ; if invalid.floating trap = enable
	     BE     NoCvtDTrap        ; no, branch
	     ANDNI  G2, 0x00001F00     ; delete actual exception
	     ORI    G2, 0x00001000     ; set actual.invalid.exception
	     MOVI   L5, NANs          ; LD = NANs
	     ADDI   L9, ExNrInv       ; correct errornumber
	     ADDI   L0,-4
	     MOVD   L1, L7
	     MOV    L7, L9
	     SUM    PC, PC, FloatErrorEntry - $

;
;**************************
;**********************
; Register Layout:
;
;       L0 : ADDRESS of LD
;       L1 : LS [0]
;       L2 : LS [1]
;       L3 : LS [2] | LS.sign
;       L4 : LS [3] | LS.exp
;       L5 : LD [0]
;       L6 : LD [1]
;       L7 : LD [2] | LD.sign
;       L8 : LD [3] | LD.exp
;       L9 : SIGN
;       L10: EXP
;       L11: PC
;       L12: SR
;       L13: LS high (saving)
;       L14: LS low (saving)
;       L15: Errornumber
;
FSUBDEntry:
	     FRAME  L0, L0
	     MOVI   L15, OffsetSubd
	     DBR    ADDDE
	     XORI   L1, 0x80000000      ; ls = -ls
FADDDEntry:
	     FRAME  L0, L0            ;
	     MOVI   L15, OffsetAddd
ADDDE:       CMPBI  L4, LFLAG          ; ReturnSR.LockFlag set ?
	     DBNZ   ADDDelAExc         ; Yes: --> AddDelAExc
	     MOVD   L13, L1
	     ANDNI  SR,LFLAG           ; No : Remove LockFlag
ADDDelAExc:  ANDNI  G2, 0x00001F00      ; delete actual exceptions
	     MOVD   L11, L3            ; save PC//SR
	     LDW.S  L0, L5, 4
	     LDW.S  L0, L6, 0          ; load LDf
	     ADD    L2, L2             ; check and remove ls.sign
	     ADDC   L1, L1
	     SETC   L3                 ; save ls.sign
	     MOV    L9,L3              ; store sign bit if result=zero
	     ADD    L6, L6             ; check and remove ld.sign
	     ADDC   L5, L5
	     SETC   L7                 ; save ld.sign
	     ADD    L9,L7              ; store sign bit if result=zero

	     CMPI   L5, 0xFFE00000      ; if LD == (NAN OR INF)
	     BNC    NanInfADD          ; yes, branch
	     CMPI   L1, 0xFFE00000      ; if LS == (NAN OR INF)
	     BNC    NanInfADS          ; yes, branch

	     MOV    L8, L5             ; ld.exp = ld
	     SHRI   L8, 21             ; EXP = real EXP
	     BZ     NoHidA1            ; if ld.exp == 0 yes, branch
	     SHRDI  L5, 1              ; correct LD.fract
	     SHLI   L5, 11             ; ld.man = ld - ld.exp
	     ORI    L5, 0x80000000      ; insert hidden bit
	     SHRI   L5, 11             ; correct position of ld.man
NoHidA1:     CMPI   L7, 0              ; if LD.sign
	     DBE    NoNegAd1           ; no, branch
	     MOV    L4, L1             ; Temp = ls
	     NOT    L6, L6
	     NOT    L5, L5
	     ADDI   L6, 1
	     ADDC   L5, C
NoNegAd1:
	     SHRI   L4, 21             ; TEMP = real LS.EXP
	     BZ     NoHidA2            ; yes, branch
	     SHRDI  L1, 1              ; correct LS.EXP
	     SHLI   L1, 11             ; ls.man = ls - ls.exp
	     ORI    L1, 0x80000000      ; insert hidden bit
	     SHRI   L1, 11             ; correct position of ls.man
NoHidA2:     CMPI   L3, 0              ; if LS.sign
	     DBE    NoNegAd2           ; no, branch
	     MOV    L10, L8            ; EXP = LD.exp
	     NOT    L2, L2
	     NOT    L1, L1
	     ADDI   L2, 1
	     ADDC   L1, C
NoNegAd2:
	     SUB    L10, L4            ;
	     BLT    LsGtADD            ;
	     DBE    SkipAd4            ;
	     MOVD   L3, 0              ;
	     CMPI   L10, 54            ;
	     BLE    ADDD1              ;
	     CMPBI  SR,0x00006000       ; chech rm=00
	     BNZ    NoADDD1            ;
	     MOVD   L1, L1             ; check for ls != 0 (inexact)
	     BZ     NoAdddSt
	     ORI    G2, 0x00000100      ;
	     CMPBI  SR, 0x00000100      ; check if inexact trap=enable
	     BNE    TrapInexDA1        ; yes, branch
	     ANDNI  G2, 0x00001F00
	     ORI    G2, 1              ;
NoAdddSt:    RET    PC, L11

ADDD1:       CMPI   L10, 32            ;
	     BLT    SkipAd0            ;
	     MOV    L3, L2             ;
	     MOV    L2, L1             ;
	     SETNM  L1                 ;
	     ADDI   L10, -32           ;
SkipAd0:     SHRD   L3, L10            ; L1//L2//L3//L4 << L10
	     SHL    L3, L10            ;
	     SHRD   L2, L10            ;
	     SHL    L2, L10            ;
	     SARD   L1, L10            ;
SkipAd4:     MOV    L10, L8            ;
	     DBR    RealADDD           ;
	     MOVD   L7, 0              ;

LsGtADD:     NEG    L10, L10           ;
	     CMPI   L10, 54            ;
	     DBLE   SkipAd1            ;
	     MOVD   L7, 0              ;
	     CMPBI  SR,0x00006000       ; check rm=00
	     BNZ    NoADDD2            ; if rm <> 00 branch
	     MOV    L5, L5             ; check for ld != 0 (inexact)
	     DBZ    RealADDD1          ; no, perform addition
	     MOVD   L5, 0              ; using ld=0
	     ORI    G2, 0x00000100      ; set inexact flag
	     BR     RealADDD1          ; and continue with addition

SkipAd1:     CMPI   L10, 32            ;
	     BLT    SkipAd2            ;
	     MOV    L7, L6             ;
	     MOV    L6, L5             ;
	     SETNM  L5                 ;
	     ADDI   L10, -32           ;
SkipAd2:     SHRD   L7, L10            ; L5//L4//L3//L2 >> L10
	     SHL    L7, L10            ;
	     SHRD   L6, L10            ;
	     SHL    L6, L10            ;
	     SARD   L5, L10            ;
RealADDD1:   MOV    L10, L4            ;
	     MOVD   L3, 0              ;

RealADDD:    ADD    L8, L4             ; Ld += Ls
	     ADDC   L7, L3             ;
	     ADDC   L6, L2             ;
	     ADDC   L5, L1             ;
	     BZ     StrADDDZ           ;
	     DBNN   NormADDD           ;
	     SETN   L9                 ;
	     NOT    L8, L8             ;  twos complement of
	     NOT    L7, L7             ;  Ld//Ldf//Ldff//Ldfff
	     NOT    L6, L6
	     NOT    L5, L5
	     ADDI   L8, 1
	     ADDC   L7, C
	     ADDC   L6, C
	     ADDC   L5, C
NormADDD:    TESTLZ L4, L5             ;
	     ADDI   L4, -11            ;
	     BZ     FinADDD            ;
	     DBN    ShrADDD            ;
	     SUB    L10, L4            ;

ShlADDD:     SHLD   L5, L4             ; L5//L6//L7//L8 << L4
	     SHR    L6, L4             ;
	     SHLD   L6, L4             ;
	     SHR    L7, L4             ;
	     DBR    NormADDD           ;
	     SHLD   L7, L4             ;

ShrADDD:     NEG    L4, L4             ; L5//L6//L7//L8 >> L4
	     SHRD   L7, L4             ;
	     SHL    L7, L4             ;
	     SHRD   L6, L4             ;
	     SHL    L6, L4             ;
	     SHRD   L5, L4             ;

FinADDD:     CMPI   L10, 0             ;
	     BLE    UnderDIVDA         ; branch if underflow
	     SHLDI  L7, 0
	     BZ     AddNoInex
	     ORI    G2, 0x00000100
AddNoInex:   CMPBI  SR, 0x00006000      ; check rm=00
	     BNZ    ARm1               ; if not 00 branch
	     ADD    L8, L8             ; round to nearest-even
	     ADDC   L7, L7
	     ADDI   L6, CZ             ; perform rounding
	     ADD    L5, C              ;
CorrADD:     CMPBI  L5, 0x00200000      ;
	     DBE    StrADDD            ;
	     MOVD   L3, L11            ;
	     ADDI   L10, 1             ;
StrADDD:     CMPI   L10, 0x000007FF     ;
	     BNN    OverDIVD           ;
	     SHLI   L10, 20            ;
	     BE     StrUnder           ;
	     ANDNI  L5, 0xFFF00000      ;
	     OR     L5, L10            ;
	     SHLI   L9, 31             ;
	     CMPBI  G2, 0x00000100      ; check if result inexact
	     DBZ    NoInexSt           ; no, branch
	     OR     L5, L9
	     CMPBI  SR, 0x00000100      ; check if inexact.trap=enable
	     BNE    TrapInexD
	     ANDNI  G2, 0x00001F00
	     ORI    G2, 1
NoInexSt:
	     STW.S  L0, L6,-4          ; store LD
	     STW.S  L0, L5, 0          ; store LDf
	     RET    PC, L11            ; leave

StrADDDZ:    SET0   L10
	     SHRI   L9, 1              ;
	     BNC    SameSign           ;
	     MOV    L9,SR              ;
	     SHRI   L9,14              ; check SR(13)
	     BC     AChk               ; if SR(13)=1 branch
SameSign:    SHRDI  L9, 1              ;
AChk:        STW.S  L0, 0,-4           ; store LDf == zero
	     STW.S  L0, L10, 0         ; store LD  == zero
	     RET    PC, L11
ARm1:
	     CMPBI  SR,0x00004000       ; check SR(14)
	     SETNZ  L1
	     SHRDI  L7,0               ; check l7//l8=0
	     BZ     CorrADD            ; if zero branch
	     CMPBI  SR, 0x00002000      ; check SR(13)
	     SETNZ  L2
	     XOR    L2,L9              ;
	     AND    L2,L1              ;
	     ADD    L6,L2              ; l6+= rounding bit
	     DBR    CorrADD
	     ADD    L5,C               ; l5+= carry bit

NoADDD2:     MOVD   L9,L1
	     MOVD   L1,L5
	     MOVD   L5,L9
	     MOV    L8,L4
NoADDD1:     MOV    L10,L8
	     CMPI   L5,0               ;
	     DBNN   NoNegD1            ; if l5>0 branch
	     SETN   L9                 ; l9=result.sign
	     NOT    L6, L6
	     NOT    L5, L5
	     ADDI   L6, 1
	     ADDC   L5, C
NoNegD1:     SHRDI  L1,0
	     BZ     CorrADD
	     SETN   L2
	     ORI    G2,0x00000100
	     CMPBI  SR,0x00004000       ; check SR(14)
	     SETNZ  L3
	     CMPBI  SR,0x00002000       ; check SR(13)
	     SETNZ  L4
	     XOR    L4,L9              ;
	     AND    L4,L3              ;
	     XOR    L2,L9              ;
	     BNZ    SignDiff           ;
	     ADD    L6,L4              ;
	     DBR    CorrADD
	     ADD    L5,C               ;
SignDiff:
	     SHRI   L4, 1              ;
	     SETNC  L4                 ;
	     SUB    L6,L4              ;
	     SUB    L5,C               ;
	     CMPBI  L5, 0xfff00000     ;
	     SETZ   L4                 ;
	     SUB    L10,L4             ;
	     ANDNI  L5, 0xfff00000     ;
	     ORI    L5,0x00100000      ;
	     BR     CorrADD

NanInfADD:   BHT    NanADD             ; if LD == NAN then branch
	     MOV    L6, L6             ; check ld.low
	     BNZ    NanADD             ; branch if ld=nan
	     CMPI   L1, 0xFFE00000     ; if LS != (NAN OR INF)
	     BST    InfADD1            ; do not add, LD//Ldf == +- INF
	     BHT    InfNanDS           ; IF LS == NAN, then branch
	     MOV    L2, L2             ; check ls.low
	     BNZ    InfNanDS           ; branch if ls=nan
	     CMP    L3, L7             ; if LD.sign == LS.SIGN
	     BNE    StdNanAS           ; inf-inf
InfADD1:     RET    PC, L11

NanADD:      SHRI   L5, 2              ; if LD == NANs
	     BST    StdNanAS           ; yes, branch
	     ANDNI  L1, 0x001ffffc      ; check for ls=NANs
	     CMPI   L1, 0xFFE00002      ;
	     BE     StdNanAS           ; yes, exception
	     BR     StdNanAQ           ; no, no exception
InfNanDS:
	     SHRI   L1, 2              ;
	     BHE    StdNanAQ           ; yes, branch
	     BR     StdNanAS           ; no, exception

NanInfADS:   BHT    InfNanDS           ; if LS == NAN, then branch
	     MOV    L2, L2             ; check ls.low
	     BNZ    InfNanDS           ; branch if ls=nan
	     STW.S  L0, L14, -4        ; store LS (infinity)
	     STW.S  L0, L13, 0         ; store LSf
	     RET    PC, L11            ; leave

NoInvATRAP:  ORI    G2, 0x00000010     ; set accrued.invalid.exception
StdNanAQ:    ANDNI  G2, 0x00001F00     ; clear actual except.
	     MOVI   L5, NANq          ; LD = NANq
	     STW.S  L0, 0, -4         ; store Ldf
	     STW.S  L0, L5, 0         ; store Ld=NANq
	     RET    PC, L11           ; leave

StdNanAS:    CMPBI  SR, 0x00001000     ; if invalid.floating trap = enable
	     BE     NoInvATRAP        ; no, branch
	     ANDNI  G2, 0x00001F00     ; delete actual exception
	     ORI    G2, 0x00001000     ; set actual.invalid.exception
	     MOVI   L5, NANs
	     ADDI   L0,-4
	     MOVD   L3, L11
	     ADDI   L15, ExNrInv
	     MOVD   L1, L13
	     MOV    L7, L15
	     SUM    PC, PC, FloatErrorEntry - $
;
; Register Layout:
;
;       L0 : ADDRESS of LD
;       L1 : LS high
;       L2 : LS low
;       L3 : PC
;       L4 : SR
;       L5 : LD high
;       L6 : LD low
;       L7 : RESULT[0]
;       L8 : RESULT[1]
;       L9 : RESULT[2]
;       L10: RESULT[3]
;       L11: SIGN
;       L12: EXP
;       L13: LS high (saving)
;       L14: LS low (saving)
;       L15: Exceptionnumber
;
;  A * B --> A.m * B.m * (A.exp + B.exp)
;  Single Floating Point multiplication
;

NoHidM1:     TESTLZ L9, L5             ;
	     CMPI   L9, 32             ;
	     BLT    NotAllZ1           ;
	     MOV    L5, L6
	     SET0   L6
	     ADDI   L12, -32
	     TESTLZ L9, L5
	     CMPI   L9, 4
	     BHE    NotAllZ1
	     SHRI   L5, 4
	     ADDI   L12, 4
	     ADDI   L9, 4
NotAllZ1:    ADDI   L9, -4             ;
	     SHLD   L5, L9             ;
	     BZ     StrDZZ             ;
	     ADDI   L12, 6             ;
	     DBR    HidM1              ;
	     SUB    L12, L9            ;
NoHidM2:     TESTLZ L9, L1             ;
	     CMPI   L9, 32             ;
	     BLT    NotAllZ2           ;
	     MOV    L1, L2             ;
	     SET0   L2
	     ADDI   L12, -32
	     TESTLZ L9, L1
	     CMPI   L9, 5
	     BHE    NotAllZ2
	     SHRI   L1, 5
	     ADDI   L12, 5
	     ADDI   L9, 5

NotAllZ2:    ADDI   L9, -5             ;
	     SHLD   L1, L9             ;
	     BZ     StrDZZ1            ;
	     ADDI   L12, 5             ;
	     DBR    HidM2              ;
	     SUB    L12, L9            ;
StrDZZ1:     MOVD   L5, L1
StrDZZ:      SHLI   L11, 31
	     OR     L5, L11
	     STW.S  L0, L6, -4          ; store LDf
	     STW.S  L0, L5, 0           ; store LD
	     RET    PC, L3              ; leave

FMULDEntry:
	     FRAME  L0, L0            ;
	     MOVI   L15, OffsetMuld
	     CMPBI  L4, LFLAG          ; ReturnSR.LockFlag set ?
	     DBNZ   LoadLDf            ; Yes: --> LoadLDf
	     MOVD   L13, L1
;	     ANDNI  SR, LFLAG          ; No : remove lock-flag
LoadLDf:     LDW.S  L0, L5, 4          ;
	     LDW.S  L0, L6, 0          ; load LDf
	     ADD    L2, L2             ; check and remove ls.sign
	     ADDC   L1, L1
	     SETC   L11                ; save ls.sign
	     ADD    L6, L6             ; check and remove ld.sign
	     ADDC   L5, L5
	     ADD    L11, C             ; LD: Store Sign Bit
	     CMPI   L5, 0xFFE00000      ; if LD == (NAN OR INF)
	     BHE    NanInfMDD          ; yes, branch
	     CMPI   L1, 0xFFE00000      ; if LS == (NAN OR INF)
	     BHE    NanInfMDS          ; yes, branch
	     MOV    L12, L5            ; EXP = LD
	     SHRI   L12, 21            ; if ld.exp == 0, EXP = real EXP
	     BZ     NoHidM1            ; yes, branch
	     SHLDI  L5, 5              ; correct LD.EXP
	     SHLI   L5, 5              ; ld.man = ld - ld.exp
	     ORI    L5, 0x80000000      ; insert hidden bit
	     SHRI   L5, 5              ; correct ld.man position
HidM1:
	     MOV    L10, L1            ; TEMP = LS
	     SHRI   L10, 21            ; if ls.exp == 0, TEMP = real LS.EXP
	     DBZ    NoHidM2            ; yes, branch
	     ADD    L12, L10           ; EXP = LD.EXP + LS.EXP
	     SHLDI  L1, 4              ; correct LS.EXP
	     SHLI   L1, 6              ; ls.man = ls - ls.exp
	     ORI    L1, 0x80000000      ; insert hidden bit
	     SHRI   L1, 6              ; correct ls.man position
HidM2:
	     ; LD//LDf * LS//LSf -> L7//L8//L9//L10

	     MOV    L9, L2             ;
	     MULU   L9, L6             ; LDf * LSf -> L9//L10
	     MOV    L7, L1             ;
	     MULU   L7, L6             ; LDf * LS -> L7//L8
	     ADD    L9, L8             ; L8//L9 += L7//L8
	     SET0   L8                 ;
	     ADDC   L8, L7             ;
	     MOV    L6, L2             ;
	     MULU   L6, L5             ; LD * LSf -> L6//L7
	     ADD    L9, L7             ; L8//L9 += L6//L7
	     ADDC   L8, L6             ;
	     MULU   L5, L1             ; LD * LS -> L5//L6
	     SET0   L7                 ; L7//L8 += L5//L6
	     ADD    L8, L6             ;
	     ADDC   L7, L5             ;
	     TESTLZ L6, L7             ; (only L6=11 and L6=12 are possible???)
	     ADDI   L6, -11            ;
	     BZ     SkipMShift         ;
	     BN     ShiftNeg           ;
	     SUB    L12, L6            ; L7//L8//L9//L10 >> L6
	     SHLD   L7, L6             ;
	     SHR    L8, L6             ;
	     SHLD   L8, L6             ;
	     SHR    L9, L6             ;
	     DBR    SkipMShift
	     SHLD   L9, L6             ;

ShiftNeg:    NEG    L6, L6             ; L7//L8//L9//L10 << L6
	     ADD    L12, L6
	     SHRD   L9, L6
	     SHL    L9, L6
	     SHRD   L8, L6
	     SHL    L8, L6
	     SHRD   L7, L6
SkipMShift:
	     ADDI   L12, -1022         ;
	     BLE    UnderMD            ;
	     SHLDI  L9, 0              ; check if result inexact
	     BZ     MulNoInex          ;
	     ORI    G2, 0x00000100      ; set inexact
MulNoInex:
	     CMPBI  SR,0x00006000       ; check rm=00
	     BNZ    MRm1               ; if rm<>00 branch
	     ADD    L10, L10           ; round ro nearest-even
	     ADDC   L9, L9
	     ADDI   L8, CZ             ; perform rounding
	     ADD    L7, C              ;
CorrMUD:     CMPBI  L7, 0x00200000      ;
	     BZ     NoMulDCor          ;
	     ADDI   L12, 1             ;
NoMulDCor:   CMPI   L12,0x000007FF      ;
	     BGE    OverMD             ;
StdMULT:
	     SHLI   L12, 20            ;
	     BE     UnderMZD           ;
	     ANDNI  L7, 0xFFF00000      ;
	     OR     L7, L12            ;
	     SHLI   L11, 31            ;
	     OR     L7, L11            ;
	     STW.S  L0, L8, -4         ;
	     STW.S  L0, L7, 0          ;
	     CMPBI  G2, 0x00000100      ; check if result inexact
	     BZ     StdNoInex          ; no, then branch
	     CMPBI  SR, 0x00000100      ; check, if inexact.trap=enable
	     BNE    TrapInexD          ;
	     ANDNI  G2, 0x00001F00
	     ORI    G2, 1              ; set accrued.inexact exception
StdNoInex:   RET    PC, L3             ;
;
;************************************************
;
UnderMD:     MOVD   L5, L7             ; l7//l8  -> l5//l6
	     MOVD   L7, L9             ; l9//l10 -> l7//l8
	     DBR    UnderDIVD
	     MOVD   L9, L11            ;
OverMD:      MOVD   L5, L7
	     DBR    OverDIVD
	     MOVD   L9, L11            ;
UnderMZD:    MOVD   L5, L7             ;
	     DBR    StrUnder           ;
	     MOVD   L9, L11            ;
;
;************************************************
;
MRm1:
	     CMPBI  SR,0x00004000       ; check SR(14)
	     SETNZ  L1
	     SHRDI  L9,0               ; check l9//l10=0
	     BZ     CorrMUD            ; branch without rounding
	     CMPBI  SR, 0x00002000      ; check SR(13)
	     SETNZ  L2
	     XOR    L2,L11             ;
	     AND    L2,L1              ;
	     ADD    L8,L2              ; l6+= rounding bit
	     DBR    CorrMUD
	     ADD    L7,C               ; l5+= carry bit

NanInfMDS:   BHT    NanMD1             ;
	     MOV    L2, L2             ; check ls.low
	     BNZ    NanMD1             ; branch if ls=nan
	     SHLDI  L5, 0              ; ls=inf, check if ld=0
	     BE     StdNanS            ; invalid operation in this case
	     BR     InfMD              ; else store inf

NanInfMDD:   BHT    NanMD              ; IF LD == NAN then branch
	     MOV    L6, L6             ; check ld.low
	     BNZ    NanMD              ; branch if ld=nan
	     CMPI   L1, 0xFFE00000      ; if LS == (NAN OR INF ?)
	     BHT    NanMDS             ; if ls=nan, branch
	     BST    NanInfMDD1         ; if ls=normal, continue
	     MOV    L2, L2             ; check ls.low
	     BNZ    NanMDS             ; branch if ls=nan
NanInfMDD1:  SHLDI  L1, 0              ; ld=inf, ls=inf or norm, check if ls=0
	     BE     StdNanS            ; invalid operation in this case
InfMD:       MOVI   L5, 0x7FF00000      ; else return inf
	     SHLI   L11, 31            ;
	     OR     L5, L11            ;
	     STW.S  L0,  0,-4          ;
	     STW.S  L0, L5, 0          ;
	     RET    PC, L3             ;

NanMDS:      SHRI   L1, 2              ; check, if ls=NANs
	     BC     StdNanS
	     BR     StdNanQ

NanMD:       SHRI   L5, 2              ; check, if ld=NANs
	     BC     StdNanS            ; yes, exception
	     ANDNI  L1, 0x001ffffc      ; check if ls=NaNs
	     CMPI   L1, 0xFFE00002      ;
	     BZ     StdNanS            ; yes, exception
	     BR     StdNanQ            ; no exception

NanMD1:      SHRI   L5, 2              ; check, if ld=NANs
	     BC     StdNanS            ;
	     BR     StdNanQ            ;
;
; Register Layout:
;
;       L0 : ADDRESS of LD
;       L1 : LS high
;       L2 : LS low
;       L3 : PC
;       L4 : SR
;       L5 : LD high
;       L6 : LD low
;       L7 : RESULT high
;       L8 : RESULT low
;       L9 : SIGN
;       L10: EXP
;       L11: TEMP
;       L12: TEMP
;       L13: LS high (saving)
;       L14: LS low (saving)
;       L15: Exceptionnumber
;
;  A / B --> A.m / B.m * (A.exp - B.exp)
;  Single Floating Point division
;
DenDIVD0:                              ; shift as much as possible-1 to left
	     TESTLZ L8, L5             ; test leading zeros
	     CMPI   L8, 32             ; if LD == zero
	     BLT    SMALLER1           ; no, skip
	     SHLDI  L5, 31             ; LD//LDf << 31
	     BZ     StrDZ              ; LD = 0, then store 0
	     ADDI   L10, -31           ; correct exponent
	     BR     DenDIVD0

SMALLER1:    SUB    L10, L8            ; correct exponent
	     ADDI   L10, 11            ; without the first 11 bit
	     ADDI   L8, -1             ; shift one less as possible
	     DBR    SkipD0             ; skip
	     SHLD   L5, L8             ; correct LD
;
DenDIVD1:                              ; shift as much as possible to left
	     TESTLZ L8, L1             ; test leading zeros LS
	     CMPI   L8, 32             ; if LS.high == zero
	     BLT    SMALLER2           ; no, skip
	     SHLDI  L1, 31             ; yes, LS//LSf << 31
	     DBR    DenDIVD1           ; once again
	     ADDI   L10, 31            ; correct EXP

SMALLER2:    ADD    L10, L8            ; correct EXP
	     ADDI   L10, -11           ; whitout the first 11 bit
	     DBR    SkipD1
	     SHLD   L1, L8             ; shift as much as possible
;
FDIVDZero:   ADD    L6, L6             ; check and remove ld.sign
	     ADDC   L5, L5
	     DBZ    StdNanS            ; branch if LD and Ldf == ZERO
	     ADD    L9, C              ; set correct sign bit
	     CMPI   L5, 0xFFE00000      ; if, ld//ldf=nan or inf
	     BHT    ZeroNan            ; branch if ld=nan
	     BST    FDIVDZero1         ; if ld=normal, continue
	     MOV    L6, L6             ; check ld.low
	     BNZ    ZeroNan            ; branch if ld=nan
FDIVDZero1:  MOVI   L5,0x7FF00000       ; return inf on normal/0 and inf/0
	     MOVI   L6,0
	     SHLI   L9,31
	     OR     L5,L9
	     CMPBI  SR, 0x00000800      ; if div_by_zero.trap == enable
	     BZ     NoDIVDEX1          ; no, then branch
	     ANDNI  G2, 0x00001F00      ; del all actual.exception
	     ORI    G2, 0x00000800      ; set actual.div_by_zero.exception
	     ADDI   L15, ExNrZero      ; correct exceptionnumber
	     ADDI   L0, -4
	     MOVD   L1, L13
	     MOV    L7, L15
	     SUM    PC, PC, FloatErrorEntry - $

ZeroNan:     SHRI   L5, 2              ; if ld//ldf = NANs
	     BNC    StdNanQ            ; no ld=NANq
	     BR     StdNanS            ; yes, exception
NoDIVDEX1:
	     ORI    G2, 8              ; set accrued.div_by_zero.flag
	     STW.S  L0, 0, -4          ; store INF, LDf
	     STW.S  L0, L5, 0          ; store INF, LD
	     RET    PC, L3             ; leave
;
;
FDIVDEntry:
	     FRAME  L0, L0             ;
	     MOVI   L15, OffsetDivd
	     CMPBI  L4, LFLAG          ; ReturnSR.LockFlag set ?
	     DBNZ   FDIVLoad           ; Yes: --> FDIVLoad
	     MOVD   L13, L1
	     ANDNI  SR,LFLAG           ; No : Remove LFLAG
FDIVLoad:    LDW.S  L0, L5, 4          ;
	     LDW.S  L0, L6, 0          ; Load LD.high
	     ADD    L2, L2             ; check and remove ls.sign
	     ADDC   L1, L1
	     DBZ    FDIVDZero          ; XX / 0 == undefined
	     SETC   L9                 ; save ls.sign
	     ADD    L6, L6             ; check and remove ld.sign
	     ADDC   L5, L5
	     ADD    L9, C              ; LD: Store Sign Bit
	     CMPI   L5, 0xFFE00000      ; if LD == (NAN OR INF)
	     BNC    NanInfDDD          ; yes, branch
	     CMPI   L1, 0xFFE00000      ; if LS == (NAN OR INF)
	     BNC    NanInfDDS          ; yes, branch
	     MOV    L10, L5            ; EXP = LD.EXP
	     SHRI   L10, 21            ; if ld.exp == 0,EXP = real EXP
	     BZ     DenDIVD0           ; yes, branch
	     SHLDI  L5, 10             ; ld.man = ld - ld.exp
	     ORI    L5, 0x80000000      ; insert hidden bit
	     SHRDI  L5, 1              ; correct position of ld
SkipD0:
	     MOV    L8, L1             ; TEMP = LS.EXP
	     SHRI   L8, 21             ; if ls.exp == 0, TEMP = real LS.EXP
	     BZ     DenDIVD1           ; yes, branch
	     SUB    L10, L8            ; EXP = LD.EXP + LS.EXP
	     SHLDI  L1, 10             ; correct LS
	     ORI    L1, 0x80000000      ; insert hidden bit
SkipD1:

; Newton iteration for 64bit / 64bit
;
NEWTON:      DIVU   L5, L1             ;
	     MOV    L7, L6             ;
	     SHRI   L2, 1              ;
	     MULU   L7, L2             ;
	     SHRI   L5, 1              ;
	     SUB    L7, L5             ;
	     BNC    PosDIVD            ;   modified 1/11/96 m.schlett
	     NOT    L8, L8
	     NOT    L7, L7
	     ADDI   L8, 1
	     ADDC   L7, C
	     DIVU   L7, L1             ;
	     MOV    L5, L6             ;
	     MOV    L1, L8
	     ADD    L8, L8             ; shli l8, 1
	     ADD    L5, C              ;
	     MOV    L6, L8             ;
	     DBR    SkipD3             ;
	     SET1   L8                 ; marker for remainder calculation

PosDIVD:     DIVU   L7, L1             ;
	     MOV    L5, L6             ;
	     MOV    L1, L8             ; save l8
	     ADD    L8, L8             ; shli l8, 1
	     SUB    L5, C              ;
	     SET0   L6                 ;
	     SUB    L6, L8             ;
	     SUB    L5, C              ;
	     SET1M  L8                 ; marker for remainder calculation
SkipD3:
	     ;ANDNI  L6, 0x00000001      ; last bit may be wrong
	     MOV    L11, L7            ; store ls
	     MOV    L12, L2            ; store lsf
	     TESTLZ L2, L5             ; check how far we must shift
	     SUB    L10, L2            ; correct EXP if important
	     NEG    L2, L2             ; correct shift value
	     ADDI   L2, 11             ; 11 bits must be free
	     SET0   L7                 ; trible shift
	     SHRD   L6, L2             ; LD//LDf//L7 >> L2
	     SHL    L6, L2             ;
	     SHRD   L5, L2             ;
	     ADDI   L10, 1023
	     CMPI   L7, 0x7FC00000      ; check if
	     BNC    CalcMod            ; if N=0 calculate remainder
	     CMPBI  SR, 0x00006000      ; check rm
	     BNZ    CalcMod            ;
	     SET0   L8                 ; no remainder
DDNearest:
	     CMPI   L10, 0
	     BLE    UnderDIVDD
	     ADD    L7, L8
	     CMPI   L7, 0
	     BE     DDNoInex
	     ORI    G2, 0x00000100
DDNoInex:    BNN    DDnoRound
	     SET0   L8
	     ADD    L7, L7             ; round to nearest-even
	     ADDI   L6, CZ             ; perform rounding
             ADD    L5, C

DDnoRound:
	     CMPBI  L5, 0x00200000      ; is there an overflow after rounding
	     BZ     NoDIVDCor          ; no, skip
	     ADDI   L10, 1             ; correct EXP

NoDIVDCor:   CMPI   L10, 0x000007FF     ; check if overflow
	     BNN    OverDIVD           ; if l10>=7ff branch
	     SHLI   L10, 20            ; align EXP
	     BE     StrUnder           ;
StoreDIVDS:  SHLI   L9, 31             ; set correct sign
	     ANDNI  L5, 0xFFF00000      ; remove hidden bit
	     OR     L5, L10            ; ld = ld.man + ld.exp
	     OR     L5, L9             ; insert correct sign
StoreDIVD:   STW.S  L0, L6, -4         ; store LD low
	     STW.S  L0, L5, 0          ; store LD high
	     CMPBI  G2, 0x00000100      ; check if inexact
	     BZ     DIVDNoInex
	     CMPBI  SR, 0x00000100      ; check if inexact.trap=enable
	     BNE    TrapInexD
	     ANDNI  G2, 0x00001F00
	     ORI    G2, 1              ; set accrued.inexact exception
DIVDNoInex:  RET    PC, L3             ; return

;            ---- calculate remainder
CalcMod:     ;MOV    L8, L12            ;
	     MULU   L1, L12            ; preliminary remainder (+-1) in L8
	     CMP    L11, L1 
	     BN     NegMod             ; store sign of remainder
	     BNE    ForwMod            ; leave remainder as is
	     MOV    L2, L2             ; check low word of product
	     BZ     ZeroMod            ; set remainder zero, else negative
NegMod:
	     DBR    ForwMod
	     NEG    L8, L8             ; change sign of remainder
ZeroMod:     
	     SET0   L8                 ; l8=CarryFlag
ForwMod:     CMPBI  SR, 0x00006000      ; check rounding mode = 00
	     BZ     DDNearest          ; if rm=00 branch
DDnotNear:
	     ADD    L7, L8             ;
	     SHLI   L7, 0
	     BZ     DDnNZero
	     BNN    DDnNFor
	     ADDI   L6, 0xFFFFFFFF      ; l6 -= 1
	     BC     DDnNFor            ; if carry branch
	     ADDI   L5, 0xFFFFFFFF      ; l5 -= 1
	     CMPBI  L5, 0xfff00000
	     SETZ   L8
	     SUB    L10, L8
	     SHLD   L5, L8
	     ANDNI  L5, 0xfff00000
DDnNFor:
	     ORI    G2, 0x00000100
	     SHRDI  L10,0
	     DBLE   UnderDIVD
	     SET0   L8
	     CMPBI  SR,0x00004000       ; check SR(14)
	     SETNZ  L1
	     CMPBI  SR, 0x00002000      ; check SR(13)
	     SETNZ  L2
	     XOR    L2,L9              ;
	     AND    L2,L1              ;
	     ADD    L6,L2              ; l6+= rounding bit
	     DBR    DDnoRound
	     ADD    L5,C               ; l5+= carry bit
;
DDnNZero:    SHRDI  L10,0
	     BLE    UnderDIVD
	     BR     DDnoRound
;
;************************************************
;
UnderDIVDD:
	     ADD    L7, L8
	     DBNN   UnderDIVD
	     SUB    L6, C
	     SUB    L5, C
	     CMPBI  L5, 0xfff00000
	     SETZ   L8
	     SUB    L10, L8
	     SHLD   L5, L8
	     ANDNI  L5, 0xfff00000
	     SHR    L6, L8
	     SHLD   L6, L8
	     DBR    UnderDIVD
	     SET0   L8
UnderDIVDA:
	     MOVD   L3, L11            ; save PC//SR correctly
UnderDIVD:
	     CMPBI  SR, 0x00000200      ; if underflow.trap = enable
	     BNE    TrapUnderD         ; yes, branch
	     ORI    G2, 2              ; set accrued.underflow.exception
	     MOVD   L1, L9
	     NEG    L2, L2             ; correct exp, exp = -exp
	     CMPI   L2, 54             ; if exp.diff > 55
	     BGE    RoundDZ            ; yes, then branch
StrUnder:    ADDI   L2, 1              ; exp +=1 for denorm. numbers
	     CMPI   L2, 32             ; if exp.diff < 32
	     BLT    UnderSHD           ; then branch
	     MOV    L9, L8
	     MOV    L8, L7
	     MOV    L7, L6
	     MOV    L6, L5
	     SET0   L5
	     ADDI   L2,-32             ;
UnderSHD:    SET0   L10
	     SHRD   L8, L2             ; L5//L6//L7//L8//L9//L10 >>L2
	     SHL    L8, L2             ;
	     SHRD   L7, L2             ;
	     SHL    L7, L2
	     SHRD   L6, L2             ;
	     SHL    L6, L2             ;
	     SHRD   L5, L2             ;
	     SHRDI  L9, 0
	     BNZ    InexactD            ;
	     SHRDI  L7, 0              ;
	     BNE    InexactD           ;
	     MOV    L9, L1
	     MOVI   L10, 0             ;
	     BR     StoreDIVDS

TrapUnderD:  ANDNI  G2, 0x00001F00      ; del all actual.exception
	     ORI    G2, 0x00000200      ; set actual.underflow.exception
	     ADDI   L10, -1536
	     TESTLZ L2, L5
	     ADDI   L2, -11
	     SUB    L10, L2
	     CMPI   L2, 21
	     BLT    TrUnShift
TrCorrZero:
	     SHLDI  L5, 21
	     SHRI   L6, 21
	     SHLDI  L6, 21
	     SHRI   L7, 21
	     SHLDI  L7, 21
	     ADDI   L10, -21
	     TESTLZ L2, L5
	     CMPI   L2, 32
	     DBGE   TrCorrZero
	     ADDI   L2, -11
TrUnShift:
	     SHLD   L5, L2
	     SHR    L6, L2
	     SHLD   L6, L2
	     CMPBI  SR,0x00006000
	     BNZ    TrUnderNNear
	     ADD    L8, L8             ; round to nearest-even
	     ADDC   L7, L7
	     ADDI   L6, CZ             ; perform rounding
	     ADD    L5, C
TrStoreUn:   CMPBI  L5, 0x00200000
	     BZ     NoCorUnder
	     ADDI   L10, 1
NoCorUnder:
	     ANDNI  L5, 0xFFF00000
	     SHLI   L9, 31
	     SHLI   L10, 20
	     OR     L5, L9
	     OR     L5, L10
	     ADDI   L15, ExNrUnder
	     ADDI   L0,-4
	     MOVD   L1, L13
	     MOV    L7, L15
	     SUM    PC, PC, FloatErrorEntry - $

TrUnderNNear:
	     CMPBI  SR,0x00004000       ; check SR(14)
	     SETNZ  L1
	     SHRDI  L7, 0              ; check l7//l8=0
	     BZ     TrStoreUn          ; branch without rounding
	     CMPBI  SR, 0x00002000      ; check SR(13)
	     SETNZ  L2
	     XOR    L2, L9             ;
	     AND    L2, L1             ;
	     ADD    L6, L2              ; l6+= rounding bit
	     DBR    TrStoreUn
	     ADD    L5,C               ; l5+= carry bit

RoundDZ:     SET0   L5                 ;
	     MOV    L9, L1
	     CMPBI  SR,0x00004000       ; check SR(14)
	     DBZ    StrDZ              ; if SR(14)=1 branch
	     SET0   L6
	     CMPBI  SR,0x00002000       ; check SR(13)
	     SETNZ  L1
	     XOR    L1, L9
	     ADD    L6, L1
StrDZ:       SHLI   L9, 31
	     OR     L5, L9
	     STW.S  L0, L6, -4          ; store LDf
	     STW.S  L0, L5, 0           ; store LD
	     RET    PC, L3              ; leave

InexactD:    ORI    G2, 0x00000100      ; set actual underflow.except.
	     ORI    G2, 2              ; set accrued underflow.except.
	     CMPBI  SR, 0x00006000      ;
	     BNZ    InexNotNear        ;
	     SHLDI  L9, 0              ;
	     BZ     InexUnAdd          ;
	     ORI    L8, 0x00000001
InexUnAdd:   ADD    L8, L8             ; round to nearest-even
             ADDC   L7, L7
	     ADDI   L6, CZ             ; perform rounding
InexForw:    ADD    L5, C              ;
InexNoRound: MOV    L9, L1
	     SET0   L10
	     CMPBI  L5, 0xfff00000
	     SETNZ  L11
	     ADD    L10, L11
	     ANDNI  L5, 0xfff00000
	     DBR    StoreDIVDS
	     SHLI   L10, 20
InexNotNear:
	     CMPBI  SR,0x00004000       ; check SR(14)
	     SETNZ  L11
	     CMPBI  SR, 0x00002000      ; check SR(13)
	     SETNZ  L12
	     XOR    L12,L1             ;
	     AND    L12,L11            ;
	     DBR    InexForw
	     ADD    L6,L12             ; l6+= rounding bit
TrapInexDA1:
	     ADDI   L0,-4
	     LDW.S  L0,L5,4
	     LDW.S  L0,L6,0
	     MOVD   L3, L11
TrapInexD:   ANDNI  G2, 0x00001F00      ; clear all actual exceptions
	     ORI    G2, 0x00000100      ; set actual inexact.exception
	     ADDI   L15, ExNrInex
	     ADDI   L0,-4
	     MOVD   L1, L13
	     MOV    L7, L15
	     SUM    PC, PC, FloatErrorEntry - $

OverDIVD:    CMPBI  SR, 0x00000400      ; if overflow.trap = enable
	     BNE    TrapOverD          ; yes, branch
	     CMPBI  SR, 0x00000100      ; if inexact.trap=enable
	     BNE    TrapInexD
	     ANDNI  G2,0x00001F00       ; clear all actual exceptions
	     ORI    G2, 5              ; set accrued.overflow.exception
	     CMPBI  SR, 0x00006000      ; check rm=00
	     BNZ    ODRm1              ; if rm<>00 branch
	     MOVI   L5, 0x7FF00000      ; LD = INF
	     SHLI   L9, 31             ; correct sign position
	     OR     L5, L9             ; insert sign
	     STW.S  L0,  0,-4          ; store LDf
	     STW.S  L0, L5, 0          ; store LD
	     RET    PC, L3             ; leave

TrapOverD:   ANDNI  G2, 0x00001F00      ; del all actual.eception
	     ORI    G2, 0x00000400      ; set actual.overflow.exception
	     ADDI   L10, 1536
	     TESTLZ L2, L5
	     ADDI   L2, -11
	     SUB    L10, L2
	     SHLD   L5, L2
	     SHR    L6, L2
	     SHLD   L6, L2
	     CMPBI  SR,0x00006000
	     BNZ    TrOverNNear
	     ADD    L8, L8             ; round to nearest-even
	     ADDC   L7, L7
	     ADDI   L6, CZ             ; perform rounding
	     ADD    L5, C
TrStoreOv:   CMPBI  L5, 0x00200000
	     BZ     NoTrapCor
	     ADDI   L10, 1
NoTrapCor:   ANDNI  L5, 0xFFF00000
	     SHLI   L9, 31
	     SHLI   L10, 20
	     OR     L5, L9
	     OR     L5, L10
	     ADDI   L15, ExNrOver      ; correct errornumber
	     ADDI   L0,-4
	     MOVD   L1, L13
	     MOV    L7, L15
	     SUM    PC, PC, FloatErrorEntry - $

TrOverNNear:
	     CMPBI  SR,0x00004000       ; check SR(14)
	     SETNZ  L1
	     SHRDI  L7, 0              ; check l7//l8=0
	     BZ     TrStoreOv          ; branch without rounding
	     CMPBI  SR, 0x00002000      ; check SR(13)
	     SETNZ  L2
	     XOR    L2,L11             ;
	     AND    L2,L1              ;
	     ADD    L6,L2              ; l6+= rounding bit
	     DBR    TrStoreOv
	     ADD    L5,C               ; l5+= carry bit

ODRm1:       MOVI   L5,0x7FEFFFFF       ;
	     MOVI   L6,0xFFFFFFFF       ; ld//ldf=max
	     CMPBI  SR,0x00004000       ; check SR(14)
	     BZ     Rm1Store           ; if SR(14)=0 rm=01
	     CMPBI  SR,0x00002000       ; check SR(13)
	     SETNZ  L1
	     XOR    L1,L9              ;
	     ADD    L6,L1              ; ldf+=l1
	     ADD    L5,C               ; ld+=carry
Rm1Store:    SHLI   L9,31              ; correct position of sign
	     OR     L5,L9              ; signed max or inf
	     STW.S  L0, L6, -4         ; store ldf
	     STW.S  L0, L5, 0          ; store ld
	     RET    PC, L3             ; leave

;
;************************************************
;
NanInfDDS:   BHT    NanDDS             ;
	     MOV    L2, L2             ; check ls.low
	     BNZ    NanDDS             ; branch if ls=nan
	     SHLI   L9, 31             ; correct position of sign
	     SET0   L5                 ; normal/inf, return 0
	     OR     L5, L9             ; ld=ld.sign|zero
	     STW.S  L0, 0, -4          ; store ldf
	     STW.S  L0, L5, 0          ; store ld
	     RET    PC, L3             ; leave

NanInfDDD:   BHT    NanDD              ; IF LD == NAN then branch
	     MOV    L6, L6             ; check ld.low
	     BNZ    NanDD              ; branch if ld=nan
	     CMPI   L1, 0xFFE00000      ; if LS == (NAN OR INF ?)
	     BHT    NanDDS             ; ls=nan, branch
	     BST    NanInfDDD2         ; ls=normal, continue
	     MOV    L2, L2             ; check ls.low
	     BNZ    NanDDS             ; branch if ls=nan
	     BR     StdNanS            ; ls=ld=infinity, exception
NanInfDDD2:  MOVI   L5, 0x7FF00000      ; inf/normal, return inf
	     SHLI   L9, 31             ;
	     OR     L5, L9             ;
	     STW.S  L0,  0,-4          ;
	     STW.S  L0, L5, 0          ;
	     RET    PC, L3             ;


NanDDS:      SHRI   L1, 2              ; check if Ls=NANs
	     BC     StdNanS            ;
	     BR     StdNanQ            ;

NanDD:       SHRI   L5, 2              ; if, ld=NANs
	     BC     StdNanS            ; yes, branch exception
	     ANDNI  L1, 0x001ffffc      ; check for ls=NaNs
	     CMPI   L1, 0xFFE00002      ;
	     BST    StdNanS            ; yes, branch exception
	     BR     StdNanQ            ; store NANq
;
;************************************************
;
;
NoInvDTrap:  ORI    G2, 0x00000010     ; set accrued.invalid.exception
	     ANDNI  G2, 0x00001F00
StdNanQ:     MOVI   L5, 0
	     STW.S  L0, L5, -4
	     MOVI   L5, NANq          ; LD = NANq
	     STW.S  L0, L5, 0         ; store NANq
	     RET    PC, L3            ; leave

StdNanS:     CMPBI  SR, 0x00001000     ; if invalid.floating trap = enable
	     BE     NoInvDTrap        ; no, branch
	     ANDNI  G2, 0x00001F00     ; delete actual exception
	     ORI    G2, 0x00001000     ; set actual.invalid.exception
	     MOVI   L5, NANs          ; LD = NANs
	     ADDI   L15, ExNrInv      ; correct errornumber
	     ADDI   L0,-4
	     MOVD   L1, L13
	     MOV    L7, L15
	     SUM    PC, PC, FloatErrorEntry - $
EndOfFPRoutines:

;----------------------------- EX, DO Entry ----------------------------------
; Note: The halfword succeding the EX, DO differentiate branches to routines
;-----------------------------------------------------------------------------

EXEntry:     FRAME  L5, L0
	     ADDI   L3, 2
	     RET    PC, L3

DOEntry:     FRAME  L5, L0
	     ADDI   L3, 2
	     RET    PC, L3
#endif


	     .END
