;;; memcpy.S

#include <asm/linkage.h>
#include <linux/config.h>
	
#if defined(__H8300H__) 
	.h8300h
#include <asm/regs306x.h>
#endif
#if defined(__H8300S__) 
	.h8300s
#endif

#define DMAC_THRESHOLD 16
	
	.text
.global SYMBOL_NAME(memcpy)

;;; void *memcpy(void *to, void *from, size_t n)
SYMBOL_NAME_LABEL(memcpy)
	mov.l	er2,er2
	bne	1f
	rts	
1:
#if defined(__H8300H__) && defined(CONFIG_H8300_DMAMEMCPY)
	cmp.l	#DMAC_THRESHOLD,er2
	bcs	10f
	;; use DMAC transfer
	mov.l	#0x1016,er3	; byte burst
	bld	#0,r0l
	bxor	#0,r1l
	bcs	3f		; non word alignment to byte transfer

	bld	#0,r0l
	bor	#0,r1l
	bcc	1f		; even alignment

	mov.b	@er1,r3l	; odd 1byte transfer
	mov.b	r3l,@er0	; r3l sets it again later
	adds	#1,er0
	adds	#1,er1
	subs	#1,er2
		
1:		
	btst	#0,r2l		; odd size?
	beq	2f
	inc.w	#1,e3		; yes
2:	
	mov.b	#0x56,r3l	; use word transfer
	shlr.l	er2		; byte count / 2
3:
	;; setup DMAC
	mov.l	er1,@MAR0AR:16
	mov.l	er0,@MAR0BR:16
	mov.w	r2,@ETCR0AH:16
	mov.b	r3h,@DTCR0B:8
	mov.b	r3l,@DTCR0A:8
4:
	;; enable DMAC
	bset	#7,@DTCR0B:8
	bset	#7,@DTCR0A:8
	dec.w	#1,e2
	bpl	4b
	mov.w	e3,e3		; rest check
	beq	5f		; no

	mov.l	@MAR0AR:16,er1
	mov.l	@MAR0BR:16,er0
	mov.b	@er1,r3l	; copy rest
	mov.b	r3l,@er0
5:
	rts
#endif
10:	
	;; cpu memcpy
	mov.l	er4,er3		; save regs and param set
	mov.l	er2,er4
	mov.l	er5,er2
	mov.l	er1,er5
	mov.l	er6,er1
	mov.l	er0,er6
11:
	eepmov.w		; block transfer
	mov.w	r4,r4
	bne	11b
	dec.w	#1,e4
	bpl	11b
	mov.l	er1,er6		; restore regs
	mov.l	er2,er5
	mov.l	er3,er4
	rts
