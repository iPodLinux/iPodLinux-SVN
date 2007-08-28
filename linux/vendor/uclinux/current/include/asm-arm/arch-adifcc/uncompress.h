/*
 * linux/include/asm-arm/arch-adifcc/uncompress.h
 *
 * Author: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (c) 2001 MontaVista Software, Inc.
 *
 */

#ifndef _ARCH_UNCOMPRESS_H_
#define _ARCH_UNCOMPRESS_H_

#ifdef CONFIG_ARCH_ADI_EVB
#define UART_BASE    ((volatile unsigned char *)0x00400000)
#elif defined(CONFIG_ARCH_BRH)
#define UART_BASE    ((volatile unsigned char *)0x03000000)
#define UART_BASE2    ((volatile unsigned char *)0x03100000)
#endif

static __inline__ void putc(char c)
{
	/*
	 * Boards have peripherals wired in LE mode, so need to
	 * fiddle with the address to get the proper byte lane
	 * enabled. :)
	 */
#if defined(__ARMEB__) && defined(_FOOBAR_)
	while ((UART_BASE[6] & 0x60) != 0x60);
	UART_BASE[3] = c;
#else

	while ((UART_BASE[5] & 0x60) != 0x60);
	UART_BASE[0] = c;
#endif
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

#ifdef CONFIG_ARCH_BRH

/*
 * We need to determine what rev board we're running so that we
 * can tell the system how much RAM there is.  Can't do this in
 * the board fixup function b/c the revision of the board can
 * only be found through PCI config cycles, which would require
 * the PCI registers to be mapped very early on.
 */

#include <linux/pci.h>
#include <asm/setup.h>

static u32 get_mem_size(void)
{
	u32 where = PCI_CLASS_REVISION & ~3;
	u32 *paddress = (u32*)(0x08000000 | ( 1 << PCI_SLOT(0) + 11) | (PCI_FUNC(0) << 8) | where);
	u8 rev_id;

	rev_id = (u8)(*paddress & 0xff);

	/*
	 * Old revision board, only 32 MB usable
	 */
	if(!rev_id)
		return 0x02000000;
	else
		return 0x08000000;
}

#define PARAM_ADDR 0xC0000100

static void setup_params(void)
{
	volatile struct tag *params = (struct tag*)PARAM_ADDR;

	params->hdr.tag = ATAG_CORE;
	params->hdr.size = tag_size(tag_core);
	params->u.core.flags = 0;
	params->u.core.pagesize = PAGE_SIZE;
	params->u.core.rootdev = 0;

	params = (volatile struct tag*)tag_next(params);

	params->hdr.tag = ATAG_MEM;
	params->hdr.size = tag_size(tag_mem32);
	params->u.mem.start = 0xc0000000;
	params->u.mem.size = get_mem_size();

	params = (volatile struct tag*)tag_next(params);

	params->hdr.tag = ATAG_NONE;
	params->hdr.size = 0;
}

#define arch_decomp_setup()	setup_params()

#else
#define arch_decomp_setup()
#endif

#define arch_decomp_wdog()


#endif // _ARCH_UNCOMPRESS_H_
