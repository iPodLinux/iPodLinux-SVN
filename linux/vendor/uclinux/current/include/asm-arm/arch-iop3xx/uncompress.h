/*
 *  linux/include/asm-arm/arch-iop80310/uncompress.h
 */

#include <linux/serial_reg.h>

#ifdef CONFIG_ARCH_IQ80310
#define UART1_BASE    ((volatile unsigned char *)0xfe800000)
#define UART2_BASE    ((volatile unsigned char *)0xfe810000)
#elif defined(CONFIG_ARCH_IQ80321)
#define UART2_BASE    ((volatile unsigned char *)0xfe800000)
#endif

static __inline__ void putc(char c)
{
	while ((UART2_BASE[5] & 0x60) != 0x60);
	UART2_BASE[0] = c;
}

/*
 * This does not append a newline
 */
static void puts(const char *s)
{
	while (*s) {
		putc(*s);
		if (*s == '\n')
			putc('\r');
		s++;
	}
}

#if 0
static void arch_decomp_setup()
{
	UART2_BASE[UART_LCR] = 0x36;
	UART2_BASE[UART_DLL] = 2;
	UART2_BASE[UART_DLM] = 0;
	UART2_BASE[UART_LCR] = 0x3;
	UART2_BASE[UART_MCR] = 0;
	UART2_BASE[UART_FCR] = 0x6;
	UART2_BASE[UART_IER] = 0;
}
#endif

/*
 * nothing to do
 */
#define arch_decomp_setup()
#define arch_decomp_wdog()
