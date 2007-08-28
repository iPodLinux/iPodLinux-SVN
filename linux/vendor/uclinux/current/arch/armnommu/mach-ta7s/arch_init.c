/*
 * arch_init.c	Generic Triscend A7S initialization functions
 *         
 * copyright:
 *         (C) 2003 Triscend Corp. (www.triscend.com)
 * authors: Craig Hackney (craig@triscend.com)
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/config.h>
#include <asm/arch/arch.h>
#include <asm/dma.h>
#include <asm/mach/dma.h>
#ifdef CONFIG_USE_A7HAL
#include <asm/arch/a7hal/uart.h>
#include <asm/arch/a7hal/timer.h>
#include <asm/arch/a7hal/clock.h>
#include <asm/arch/a7hal/mpu.h>
#include <asm/arch/a7hal/dma.h>

static a7hal_mpu_map myMap[] = {
/*
 *   base            size               permissions     cacheEnable
 */
  {0x00000000, 0, 0, NO},	/* Empty */
  {0x00000000, 0, 0, NO},	/* Empty */
  {0x00000000, 0, 0, NO},	/* Empty */
  {0x00000000, 0, 0, NO},	/* Empty */
  {0x00000000, A7HAL_SIZE_4GB, A7HAL_SUP_RW_USR_RW, NO},/* Enable access to all memory */
  {0xd0000000, A7HAL_SIZE_8MB, A7HAL_SUP_RW_USR_RW, YES},/* FLASH	*/
  {0xd1010000, A7HAL_SIZE_64KB, A7HAL_SUP_RW_USR_NO, NO},/* Config regs	*/
  {0x00000000, A7HAL_SIZE_32MB, A7HAL_SUP_RW_USR_RW, YES}/* SDRAM	*/
};

#else

#define PIPELINE_ENABLE        0x00008000
#define DMA_BUFFERS_ENABLE     0x00ff0000

extern unsigned long a7hal_clock_getFreq (int noRAM);

void
a7hal_sdram_init (void)
{
  unsigned long refresh;
  unsigned long freq;

  freq = a7hal_clock_getFreq (1);
  refresh = (unsigned long) ((0.064 / 4096) * freq);

  refresh <<= 16;
  refresh &= 0x0fff0000;

  refresh |= 0x00000003;

#if defined(A7VE) || defined(A7VC) || defined(A7VT) || defined(A7VL)
  refresh |= (1<<REF_EN_BIT);
#endif

  /* Set the SDRAM refresh rate */
  *((volatile unsigned long *) MSS_SDR_CTRL_REG) = refresh;

  /* Read the value back to flush the CSI pipeline */
  refresh = *((volatile unsigned long *) MSS_SDR_CTRL_REG);

  /*
   * If the clock frequency is greater than 40MHz
   * enable the memory pipeline.
   */
  if (freq > 40000000)
    *((unsigned long *) MSS_CONFIG_REG) |= PIPELINE_ENABLE;

  /* Enable the DMA buffers */
  *((unsigned long *) MSS_CONFIG_REG) |= DMA_BUFFERS_ENABLE;
}

void
a7hal_uart_setBaud (int port, int baudRate)
{
  int oldLineControl;
  int baud;

  baud = (a7hal_clock_getFreq (0) / (baudRate * 16)) - 1;

  if (port == 0)
    {
      oldLineControl = A7_REG (UART0_LINE_CONTROL_REG);
      A7_REG (UART0_LINE_CONTROL_REG) = (1L << DLAB_BIT);
      A7_REG (UART0_DIVISOR_MSB_REG) = ((baud >> 8) & 0xff);
      A7_REG (UART0_DIVISOR_LSB_REG) = (baud & 0xff);
      A7_REG (UART0_LINE_CONTROL_REG) = oldLineControl;
    }
  else
    {
      oldLineControl = A7_REG (UART1_LINE_CONTROL_REG);
      A7_REG (UART1_LINE_CONTROL_REG) = (1L << DLAB_BIT);
      A7_REG (UART1_DIVISOR_MSB_REG) = ((baud >> 8) & 0xff);
      A7_REG (UART1_DIVISOR_LSB_REG) = (baud & 0xff);
      A7_REG (UART1_LINE_CONTROL_REG) = oldLineControl;
    }
}

void
a7hal_uart_init (void)
{
  A7_REG (UART0_CONTROL_REG) = (1L << PRESCALE_EN_BIT);
  A7_REG (UART1_CONTROL_REG) = (1L << PRESCALE_EN_BIT);

  a7hal_uart_setBaud (0, 115200);
  a7hal_uart_setBaud (1, 115200);

  /*
   * Set the UARTs to no parity, 8 data, 1 stop
   */
  A7_REG (UART0_LINE_CONTROL_REG) = 0x03;
  A7_REG (UART1_LINE_CONTROL_REG) = 0x03;
}


void
a7hal_lancs8900_reset (int interface)
{
  int n;

  A7_REG (0x10000020) = 0x01;

  for (n = 0; n < 0x100000; n++);

  A7_REG (0x10000020) = 0x00;

  A7_REG (0x10000020) = 0x02;
  A7_REG (0x10000020) = 0x00;
  A7_REG (0x10000020) = 0x02;
  A7_REG (0x10000020) = 0x00;
}

int
a7hal_lancs8900_init (const unsigned long *params)
{
  a7hal_lancs8900_reset (0);
  return (0);
}

#endif

/*
 * Defines the base address and IRQ to used for
 * the CS8900.
 */
const unsigned long cs8900Params[] = { 0x10000000, 11 };

void
ta7_arch_init (void)
{
  a7hal_uart_init ();
#ifdef CONFIG_CS89x0
  a7hal_lancs8900_init (cs8900Params);
#endif

#ifdef CONFIG_USE_A7HAL
  a7hal_icu_init ();
  a7hal_clock_init ();
  a7hal_timer_init ();
  a7hal_mpu_init ();
  a7hal_mpu_setMap (myMap);
  a7hal_mpu_enableMap ();
#endif
}

static struct dma_ops dma_operation = {
  type:"TA7"
};

/*
 * Minimum DMA support, just initialize the d_ops
 * for each channel so that kernel level drivers
 * can call request_dma and free_dma.
 */
void
arch_dma_init (dma_t dma_chan[])
{
#ifdef CONFIG_USE_A7HAL
  int n;

  a7hal_dma_init ();

  for (n = 0; n < MAX_DMA_CHANNELS; n++)
    dma_chan[n].d_ops = &dma_operation;
#endif
}
