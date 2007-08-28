/*
 * setup-dsc21.c
 *
 * BRIEF MODULE DESCRIPTION
 *   This has additional initialiation needed by the DSC21.
 *
 * Copyright (C) 2001 RidgeRun, Inc.
 * Author: RidgeRun, Inc.
 *          stevej@ridgerun.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <asm/arch/hardware.h>
#include <asm/io.h>
#include <linux/console.h>

const unsigned int _end_kernel_image;

static const char welcome[] = "\nWelcome to the RidgeRun DSC21 Linux port.\n";

void dsc21_console_write(struct console *con, const char *buf, unsigned len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (buf[i] == '\n') {
			while(!(inw(DSC21_UART1_SR) & 0x0400));
			outb('\r', DSC21_UART1_DTRR);
		}
		while(!(inw(DSC21_UART1_SR) & 0x0400));
		outb(buf[i], DSC21_UART1_DTRR);		
	}
}

static struct console dsc21_console = {
	name: "dsc21con",
	write: dsc21_console_write
};

static void dsc21_external_bus_init(void)
{
	/* reset */
	outw(1, DSC21_EXTBUS_RESET);
	outw(0, DSC21_EXTBUS_RESET);

	/* 
	 * write wait cycle = 5 * 21ns
	 * read wait cycle = 4 * 21ns 
	 * flash wait cycle = 2 * 21ns 
	 * type = 0 (CFC)
	 * mode = 0 (memory mapped) 
	 */
	outw(0x5420, DSC21_EXTBUS_MODE);

	/* enable 'flash ready' interrupt */
	outw(0, DSC21_EXTBUS_INTEN);
}

static void dsc21_uart_init(void)
{
	/* disable ints */
	outw(0, DSC21_UART0_MSR);
	outw(0, DSC21_UART1_MSR);

	/* clear fifos */
	outw(0x8000, DSC21_UART0_RFCR);
	outw(0x8000, DSC21_UART0_TFCR);
	outw(0x8000, DSC21_UART1_RFCR);
	outw(0x8000, DSC21_UART1_TFCR);

	/* no breaks */
	outw(0, DSC21_UART0_LCR);
	outw(0, DSC21_UART1_LCR);

	/* Now, setup UART1 for use as a printk console while we boot. */

	/* 115200 baud */
	outw(0x0013, DSC21_UART1_BRSR);

	/* 8E1 & enable rcv interrupt */
	outw(0x8010, DSC21_UART1_MSR);

	/* 1 byte rcv fifo trigger */
	outw(0, DSC21_UART1_RFCR);

	/* 16 byte xmt fifo trigger */
	outw(0x0300, DSC21_UART1_TFCR);
}


void setup_dsc21(void)
{
	extern void register_console(struct console *);

	dsc21_external_bus_init();
//	dsc21_uart_init();
//	dsc21_console_write(&dsc21_console, welcome, sizeof(welcome));
//	register_console(&dsc21_console);
}
