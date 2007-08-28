#include <net/checksum.h>
#include <asm/checksum.h>

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 * computes the checksum of a memory block at buff, length len,
 * and adds in "sum" (32-bit)
 *
 * returns a 32-bit number suitable for feeding into itself
 * or csum_tcpudp_magic
 *
 * this function must be called with even lengths, except
 * for the last fragment, which may be odd
 *
 * it's best to have buff aligned on a 32-bit boundary
 */

unsigned int csum_partial(const unsigned char * buff, int len, unsigned int sum)
{
#if 0
	__asm__ __volatile__("
		movi	%%g5, 0		; g5 = result
		cmpi	%1, 0
		skps	cc_hi
		br	9f
		mov	%%g7, %0
		skp1	%%g7, 0		; g7 = odd
		br	1f
		 nop
		subi	%1, 1		; if(odd) { result = *buff;
		ld	%%g5, [%0]	;           len--;
		ext8d	%%g5, %0	;	    postion byte (bits 0..7)
        lsli    %%g5, 8     ; little endian
		addi	%0, 1		;           buff++ }
1:
		mov	%%g6, %1
		lsri	%%g6, 1	        ; g6 = count = (len >> 1)
		cmpi	%%g6, 0		; if (count) {
		skps	cc_nz
		br	8f
		 nop
		skp1	%0, 1		; if (2 & buff) {
		br	1f
		 nop
		subi	%1, 2		;	result += *(unsigned short *) buff;
		ld	%%g1, [%0]	;	count--; 
		ext16d	%%g1, %0	;	postion half word (bits 0..15)
		subi	%%g6, 1		;	len -= 2;
		add	%%g5, %%g1	;	buff += 2; 
		addi	%0, 2		; }
1:
		lsri	%%g6, 1		; Now have number of 32 bit values
		cmpi	%%g6, 0		; if (count) {
		skps	cc_nz
		br	2f
		 nop
1:						; do {
		ld	%%g1, [%0]		; csum aligned 32bit words
		addi	%0, 4
		add	%%g5, %%g1
		skps	cc_nc
		addi	%%g5, 1			; add carry
		subi	%%g6, 1
		skps	cc_z
		br	1b			; } while (count)
		 nop
		mov	%%g2, %%g5
		lsri	%%g2, 16
		ext16s	%%g5, 0		       	; g5 & 0x0000ffff
		add	%%g5, %%g2	; }
2:
		skp1	%1, 1		; if (len & 2) {
		br	8f
		 nop
		ld	%%g1, [%0]	;	result += *(unsigned short *) buff; 
		ext16d	%%g1, %0	;	position half word (bits 0..15)
		add	%%g5, %%g1	;	buff += 2; 
		addi	%0, 2		; }
8:
		skp1	%1, 0		; if (len & 1) {
		br	1f
		 nop
		ld	%%g1, [%0]
		ext8d	%%g1, %0	;	position byte (bits 0..7)
		add	%%g5, %%g1	; }	result += (*buff); 
1:
		mov	%%g1, %%g5
		lsli	%%g1, 16
		add	%%g5, %%g1	; result = from32to16(result);
		lsri	%%g5, 16
		skps	cc_nc
		addi	%%g5, 1		; add carry
		skp1	%%g7, 0		; if(odd) {
		br	9f
		 nop
		mov	%%g2, %%g5	;	result = ((result >> 8) & 0xff) |
                pfx     %%hi(0xff)
		and	%%g2, %%lo(0xff);	((result & 0xff) << 8);
		lsli	%%g2, 8
		lsri	%%g5, 8
		or	%%g5, %%g2	; }
9:
		add	%2, %%g5	; add result and sum with carry
		skps	cc_nc
		addi	%2, 1		; add carry
		mov	%%g5,%2		; result = from32to16(result)
		lsli	%%g5,16
		add	%2,%%g5
		lsri	%2,16
		skps	cc_nc
		addi	%2, 1
	" 
	:"=&r" (buff), "=&r" (len), "=&r" (sum)
	:"0" (buff), "1" (len), "2" (sum)
	:"g1", "g2", "g5", "g6", "g7"); 


	return sum;
#else
	unsigned int result = do_csum(buff, len);

	/* add in old sum, and carry.. */
	result += sum;
	if (sum > result)
		result += 1;
	return result;
#endif
}


/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


/*
 * the same as csum_partial, but copies from fs:src while it
 * checksums
 *
 * here even more important to align src and dst on a 32-bit (or even
 * better 64-bit) boundary
 */

unsigned int csum_partial_copy(const char *src, char *dst, int len, int sum)
{
	memcpy(dst, src, len);
	return csum_partial(dst, len, sum);

}
