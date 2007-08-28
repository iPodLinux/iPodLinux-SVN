/***********************************************************************
 * linux/drivers/net/eth_c5471hw.c
 *
 *   Copyright (C) 2003 Cadenux, LLC. All rights reserved.
 *   todd.fischer@cadenux.com  <www.cadenux.com>
 *
 *   Copyright (C) 2001 RidgeRun, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * THIS  SOFTWARE  IS  PROVIDED  ``AS  IS''  AND   ANY  EXPRESS  OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
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
 ***********************************************************************/

/***********************************************************************
 * Included Files
 ***********************************************************************/

#include <linux/kernel.h>
#include <asm/string.h>
#include "eth_c5471.h"

/***********************************************************************
 * Definitions
 ***********************************************************************/

#ifdef MODULE
static int   eth_strlen(const char *str);
static char *eth_strcpy(char *dst, const char *src);
static int   eth_strcmp(const char *s1, const char *s2);
#else
# define eth_strlen(str)      strlen(str)
# define eth_strcpy(dst,src)  strcpy(dst,src)
# define eth_strcmp(s1,s2)    strcmp(s1,s2)
#endif

/***********************************************************************
 * Public Variables
 ***********************************************************************/

/***********************************************************************
 * Private Variables
 ***********************************************************************/

static unsigned char d_MAC[6] = {0,0,0,0,0,0};

/* According to the C5471 documentation: "The software has
 * to maintain two pointers to the current RX-CPU and TX-CPU
 * descriptors. At init time, they have to be set to the first
 * descriptors of each queue, and they have to be incremented
 * each time a descriptor ownership is give to the SWITCH".
 */

static long         *current_tx_cpu_desc;
static long         *current_rx_cpu_desc;
static long         *last_packet_desc_start;
static long         *last_packet_desc_end;
static unsigned long eim_status_shadow;
static unsigned long cached_rx_status;
static unsigned long cached_tx_status;

/***********************************************************************
 * eth_strlen, eth_strcpy, eth_strcmp
 ***********************************************************************/

#ifdef MODULE
static int eth_strlen(const char *str)
{
  const char *ptr;
  for (ptr = str; *ptr ; ptr++);
  return (int)(ptr-str);
}
static char *eth_strcpy(char *dst, const char *src)
{
  char *ptr;
  for (ptr = dst; *src; )
    *ptr++ = *src++;
  *ptr = '\0';
  return dst;
}

static int eth_strcmp(const char *s1, const char *s2)
{
  unsigned char c1, c2;

  do
    {
      c1 = (unsigned char) *s1++;
      c2 = (unsigned char) *s2++;
      if (c1 == '\0')
	{
	  return c1 - c2;
	}
    }
  while (c1 == c2);

  return c1 - c2;
}
#endif

/***********************************************************************
 * do_delay
 ***********************************************************************/

static void do_delay(int c)
{
  volatile int i;
  for (i = 0; i <= c; i++){}
}

/***********************************************************************
 * eth_out32.  Write to 32-bit HW registers
 ***********************************************************************/

static __inline__ void eth_out32(volatile unsigned long *addr,
				unsigned long val)
{
#undef ADJUST_ENDIANNESS
#ifdef ADJUST_ENDIANNESS
  *addr = __swap_32(val);
#else
  *addr = val;
#endif  
}
  
/***********************************************************************
 * eth_in32. Read from 32-bit HW registers
 ***********************************************************************/

static __inline__ unsigned long eth_in32(volatile unsigned long *addr)
{
#undef ADJUST_ENDIANNESS
#ifdef ADJUST_ENDIANNESS
  unsigned long val;
  val = *addr;
  return __swap_32(val);
#else
  return *addr;
#endif
}

/***********************************************************************
 * eth_md_tx_bit
 ***********************************************************************/

/* This is a  helper routine used when serially communicating
 * with the C5471's external ethernet transeiver device.
 * GPIO pins are connected to the transeiver's MDCLK and
 * MDIO pins and are used to accomplish the serial comm.
 *
 * protocol:
 *                     ___________
 *    MDCLK   ________/           \_
 *             ________:____
 *    MDIO    <________:____>--------
 *                     :
 *                     ^
 *             Pin state internalized
 */

#define MDIO_bit  0x00004000
#define MDCLK_bit 0x00008000

static void eth_md_tx_bit (int bit_state)
{
  /* config MDIO as output pin. */

  eth_out32(GPIO_CIO_REG,(eth_in32(GPIO_CIO_REG)&~MDIO_bit)); 

  if (bit_state)
    {
      /* set MDIO state high. */

      eth_out32(GPIO_IO_REG,(eth_in32(GPIO_IO_REG)|MDIO_bit));
    }
  else
    {
      /* set MDIO state low. */

      eth_out32(GPIO_IO_REG,(eth_in32(GPIO_IO_REG)&~MDIO_bit));
    }

  nop();                    
  nop();
  nop();
  nop();

  /* MDCLK rising edge */

  eth_out32(GPIO_IO_REG,(eth_in32(GPIO_IO_REG)|MDCLK_bit));
  nop();
  nop();

  /* release MDIO */

  eth_out32(GPIO_CIO_REG,(eth_in32(GPIO_CIO_REG)|MDIO_bit));
  nop();
  nop();

  /* MDCLK falling edge. */

  eth_out32(GPIO_IO_REG,(eth_in32(GPIO_IO_REG)&~MDCLK_bit));
}

/***********************************************************************
 * eth_md_rx_bit()
 ***********************************************************************/

/* This is a helper routine used when serially communicating
 * with the C5471's external ethernet transeiver device.
 * GPIO pins are connected to the transeiver's MDCLK and
 * MDIO pins and are used to accomplish the serial comm.
 * 
 * protocol:
 *                    ___________
 *    MDCLK  ________/           \_
 *           _______:_____
 *    MDIO   _______:_____>--------
 *                  :
 *                  ^
 *         pin state sample point
 */

#define MDIO_bit  0x00004000
#define MDCLK_bit 0x00008000

static unsigned long eth_md_rx_bit (void)
{
  register unsigned long bit_state;

  /* sanity; insure MDIO configured as input pin. */

  eth_out32(GPIO_CIO_REG,(eth_in32(GPIO_CIO_REG)|MDIO_bit));

  /* sanity; insure MDCLK low. */

  eth_out32(GPIO_IO_REG,(eth_in32(GPIO_IO_REG)&~MDCLK_bit));
  nop();
  nop();
  nop();
  nop();

  /* sample MDIO */

  bit_state = eth_in32(GPIO_IO_REG) & MDIO_bit;

  /* MDCLK rising edge */

  eth_out32(GPIO_IO_REG,(eth_in32(GPIO_IO_REG)|MDCLK_bit));
  nop();
  nop();
  nop();
  nop();

  /* MDCLK falling edge. */

  eth_out32(GPIO_IO_REG,(eth_in32(GPIO_IO_REG)&~MDCLK_bit));
  if (bit_state)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_md_write_reg
 ***********************************************************************/

/* This function is a helper routine used when serially communicating
 * with the C5471's external ethernet transeiver device.
 * GPIO pins are connected to the transeiver's MDCLK and
 * MDIO pins and are used to accomplish the serial comm.
 */

static void eth_md_write_reg (int adr, int reg, int data)
{
  int i;

  /* preamble: 11111111111111111111111111111111 */

  for (i = 0; i < 32; i++)
    {
      eth_md_tx_bit(1);
    }

  /* start of frame: 01 */

  eth_md_tx_bit(0);
  eth_md_tx_bit(1);

  /* operation code: 01 - write */

  eth_md_tx_bit(0);
  eth_md_tx_bit(1);

  /* PHY device address: AAAAA, msb first */

  for (i = 0; i < 5; i++)
    {
      eth_md_tx_bit(adr & 0x10);
      adr = adr << 1;
    }

  /* MII register address: RRRRR, msb first */

  for (i = 0; i < 5; i++)
    {
      eth_md_tx_bit(reg & 0x10);
      reg = reg << 1;
    }

  /* turnaround time: ZZ */

  eth_md_tx_bit(1);
  eth_md_tx_bit(0);

  /* data: DDDDDDDDDDDDDDDD, msb first */

  for (i = 0; i < 16; i++)
    {
      eth_md_tx_bit(data & 0x8000);
      data = data << 1;
    }
}

/***********************************************************************
 * eth_md_read_reg
 ***********************************************************************/

/* This is a helper routine used when serially communicating
 * with the C5471's external ethernet transeiver device.
 * GPIO pins are connected to the transeiver's MDCLK and
 * MDIO pins and are used to accomplish the serial comm.
 */

static int eth_md_read_reg (int adr, int reg)
{
  int i;
  int data = 0;

  /* preamble: 11111111111111111111111111111111 */

  for (i = 0; i < 32; i++)
    {
      eth_md_tx_bit(1);
    }

  /* start of frame: 01 */

  eth_md_tx_bit(0);
  eth_md_tx_bit(1);

  /* operation code: 10 - read */

  eth_md_tx_bit(1);
  eth_md_tx_bit(0);

  /* PHY device address: AAAAA, msb first */

  for (i = 0; i < 5; i++)
    {
      eth_md_tx_bit(adr & 0x10);
      adr = adr << 1;
    }

  /* MII register address: RRRRR, msb first */

  for (i = 0; i < 5; i++)
    {
      eth_md_tx_bit(reg & 0x10);
      reg = reg << 1;
    }

  /* turnaround time: ZZ */

  eth_md_rx_bit();
  eth_md_rx_bit(); /* PHY should drive a 0 */

  /* data: DDDDDDDDDDDDDDDD, msb first */

  for (i = 0; i < 16; i++)
    {
      data = data << 1;
      data |= eth_md_rx_bit();
    }

  return data;
}

/***********************************************************************
 * eth_phy_init
 ***********************************************************************/

/* The C5471 EVM board (C5472) uses a Lucent LU3X31T-T64 transeiver
 * device to handle the physical layer (PHY). It's a HW block that
 * on the one end offers a Media Independent Interface (MII) which is
 * connected to the Ethernet Interface Module (EIM) internal to the
 * C5472 and on the other end offers either the 10baseT or 100baseT
 * electrical interface connecting to an RJ45 onboard network connector.
 * The PHY transeiver has several internal registers allowing host
 * configuration and status access. These internal registers are
 * accessable by clocking serial data in/out of the MDIO pin of the
 * LU3X31T-T64 chip. For C5471, the MDC and the MDIO pins are
 * connected to the C5472 GPIO15 and GPIO14 pins respectivley. Host
 * software twidles the GPIO pins appropriately to get data serially
 * into and out of the chip. This is typically a one time operation at
 * boot and normal operation of the transeiver involves EIM/Transeiver
 * interaction at the other pins of the transeiver chip and doesn't
 * require host intervention at the MDC and MDIO pins.
 */

#if (CADENUX_ETHERNET_PHY == ETHERNET_PHY_LU3X31T_T64)
static int eth_phy_init (void)
{
  int phy_id;
  int status;
  
  /* Next, Setup GPIO pins to talk serially to the
   * Lucent transeiver chip.
   */

  dbg("Setup GPIO connection to PHY.");

  /* enable gpio bits 15,14 */

  eth_out32(GPIO_EN_REG, (eth_in32(GPIO_EN_REG)|0x0000C000));

  /* config gpio(15); out -> MDCLK */

  eth_out32(GPIO_CIO_REG,(eth_in32(GPIO_CIO_REG)&~0x00008000));

  /* config gpio(14); in <- MDIO */

  eth_out32(GPIO_CIO_REG,(eth_in32(GPIO_CIO_REG)|0x00004000));

  /* initial pin state; MDCLK = 0 */

  eth_out32(GPIO_IO_REG, (eth_in32(GPIO_IO_REG)&0x000F3FFF));

  /* Next, request a chip reset */

  eth_md_write_reg(0, MD_PHY_CONTROL_REG, 0x8000);

  dbg("PHY reset");

  while (eth_md_read_reg(0, MD_PHY_CONTROL_REG) & 0x8000)
    {
      /* wait for chip reset to complete. */
    }

  /* Next, Read out the chip ID */
 
  dbg("PHY chip ID read");

  phy_id = (eth_md_read_reg(0, MD_PHY_MSB_REG) << 16) |
    eth_md_read_reg(0, MD_PHY_LSB_REG);
  if (phy_id != LU3X31_T64_IDval)
    {
      err("Unexpected PHY chip ID 0x%X", phy_id);
      return -1;
    }

  /* Next, Set desired operation mode. */

  switch (LAN_Rate)
    {
    case 100: /* 100BaseT */
      dbg("Setting PHY Transceiver for 100BaseT FullDuplex");
      eth_md_write_reg(0, MD_PHY_CONTROL_REG, MODE_100MBIT_FULLDUP);
      break;

    case 10: /* 10BaseT */
      dbg("Setting PHY Transceiver for 10BaseT FullDuplex");
      eth_md_write_reg(0, MD_PHY_CONTROL_REG, MODE_10MBIT_FULLDUP);
      break;

    case 0: /* Auto negotiation */
      dbg("Setting PHY Transceiver for Autonegotiation");
      eth_md_write_reg(0, MD_PHY_CONTROL_REG, MODE_AUTONEG);
      break;
    }

  status = eth_md_read_reg(0, MD_PHY_CTRL_STAT_REG);
  return status;
}
#else
# define eth_phy_init()
# if !defined(CADENUX_ETHERNET_PHY)
#  warning "CADENUX_ETHERNET_PHY not defined -- assumed NONE"
# endif
#endif

/***********************************************************************
 * eth_eim_reset
 ***********************************************************************/

/* The C5472 docs states that a module should generally be reset
 * according to the following algorithm:
 *
 *  1. Put the module in reset.
 *  2. Switch on the module clock.
 *  3. Wait for eight clock cycles.
 *  4. Release the reset.
 */

static void eth_eim_reset (void)
{
  /* Stop the EIM module clock. */

  eth_out32(CLKM_REG,(eth_in32(CLKM_REG)|CLKM_EIM_CLK_STOP));

  /* put EIM module in reset. */

  eth_out32(CLKM_RESET_REG,(eth_in32(CLKM_RESET_REG)&~CLKM_RESET_EIM));

  /* Start the EIM module clock. */

  eth_out32(CLKM_REG,(eth_in32(CLKM_REG)&~CLKM_EIM_CLK_STOP));
  
  /* Assert nRESET to reset the board's PHY0/1 chips 
   * (i.e the external LU3X31T-T64 transeiver parts).
  */

  eth_out32(CLKM_CTL_RST_REG,(CLKM_CTL_RST_EXT_RESET | CLKM_CTL_RST_LEAD_RESET));

  do_delay(2000);

  /* Release the peripheral nRESET signal. */

  eth_out32(CLKM_CTL_RST_REG,CLKM_CTL_RST_LEAD_RESET);
  
  /* Release EIM module reset. */

  eth_out32(CLKM_RESET_REG,(eth_in32(CLKM_RESET_REG) | CLKM_RESET_EIM));

  last_packet_desc_start = (void *)0;
  last_packet_desc_end = (void *)0;
  cached_tx_status = 0;
  cached_rx_status = 0;

  /* All EIM register should now be in their
   * power-up default states. 
   */
}

/***********************************************************************
 * eth_eim_specific_config
 ***********************************************************************/

/* This function sssumes that all registers are currently in the
 * power-up reset state. This routine then modifies that state to
 * provide our  specific ethernet configuration.
 */

static void eth_eim_specific_config(void)
{
  char *p_buf;
  unsigned long *p_desc, val;
  int i;
  
  p_desc = EIM_RAM_START;
  p_buf  = (unsigned char *)EIM_RAM_START + 0x6C0;

  /* TX ENET 0 */
  /* 16bit offset address. */

  eth_out32(ENET0_TDBA_REG,(unsigned long)((unsigned long)(p_desc)&0x0000ffff)); 
  for (i = NUM_DESC_TX-1; i >= 0; i--)
    {
      if (i == 0)
	val = EIM_TXDESC_WRAP;
      else
	val = 0x0000;
      val |= EIM_DESC_OWNER_h_w | CPU_TXDESC_INTEN | ENET_DESC_PADCRC | EIM_PACKET_BYTES;
      eth_out32(p_desc,val);
      p_desc++;
      eth_out32(p_desc,(unsigned long)p_buf);
      p_desc++;
      *((unsigned long *)p_buf) = 0x00000000;
      p_buf += EIM_PACKET_BYTES;
      *((unsigned long *)p_buf) = 0x00000000;

      /* As per C5471 doc; this space for Ether Module's
       * "Buffer Usage Word". */

      p_buf += 4;
    }

  /* RX ENET 0 */

  /* 16bit offset address. */

  eth_out32(ENET0_RDBA_REG,(unsigned long)((unsigned long)(p_desc)&0x0000ffff));
  for (i=NUM_DESC_RX-1; i>=0; i--)
    {
      if (i == 0)
	val = EIM_RXDESC_WRAP;
      else
	val = 0x0000;
      val |= EIM_DESC_OWNER_f_w | CPU_TXDESC_INTEN | ENET_DESC_PADCRC | EIM_PACKET_BYTES;
      eth_out32(p_desc,val);
      p_desc++;
      eth_out32(p_desc,(unsigned long)p_buf);
      p_desc++;
      *((unsigned long *)p_buf) = 0x00000000;
      p_buf += EIM_PACKET_BYTES;
      *((unsigned long *)p_buf) = 0x00000000;

      /* As per C5471 doc; this space for Ether Module's "Buffer Usage Word". */

      p_buf += 4;
    }

  /* TX CPU */

  current_tx_cpu_desc = p_desc;

  /* 16bit offset address. */

  eth_out32(EIM_CPU_TXBA_REG,(unsigned long)((unsigned long)(p_desc)&0x0000ffff));
  for (i = NUM_DESC_TX-1; i >= 0; i--)
    {
      if (i == 0)
	val = EIM_TXDESC_WRAP;
      else
	val = 0x0000;
      val |= EIM_DESC_OWNER_h_w | CPU_TXDESC_INTEN | ENET_DESC_PADCRC | EIM_PACKET_BYTES;
      eth_out32(p_desc,val);
      p_desc++;
      eth_out32(p_desc,(unsigned long)p_buf);
      p_desc++;
      *((unsigned long *)p_buf) = 0x00000000;
      p_buf += EIM_PACKET_BYTES;
      *((unsigned long *)p_buf) = 0x00000000;

      /* As per C5471 doc; this space for Ether Module's "Buffer Usage Word". */

      p_buf += 4;
    }
  
  /* RX CPU */

  current_rx_cpu_desc = p_desc;

  /* 16bit offset address. */

  eth_out32(EIM_CPU_RXBA_REG,(unsigned long)((unsigned long)(p_desc)&0x0000ffff));
  for (i=NUM_DESC_RX-1; i>=0; i--)
    {
      if (i == 0)
	val = EIM_RXDESC_WRAP;
      else
	val = 0x0000;
      val |= EIM_DESC_OWNER_Kernal | CPU_TXDESC_INTEN | ENET_DESC_PADCRC | EIM_PACKET_BYTES;
      eth_out32(p_desc,val);
      p_desc++;
      eth_out32(p_desc,(unsigned long)p_buf);
      p_desc++;
      *((unsigned long *)p_buf) = 0x0000;
      p_buf += EIM_PACKET_BYTES;
      *((unsigned long *)p_buf) = 0x0000;

      /* As per C5471 doc; this space for Ether Module's "Buffer Usage Word". */

      p_buf += 4;
    }
  
  eth_out32(EIM_BUFSIZE_REG      ,EIM_PACKET_BYTES);
#if 0
  eth_out32(EIM_CPU_FILTER_REG   ,EIM_FILTER_UNICAST);
#else

  /* eth_out32(EIM_CPU_FILTER_REG   ,0x08 | EIM_FILTER_UNICAST | EIM_FILTER_MULTICAST | EIM_FILTER_BROADCAST); */

  eth_out32(EIM_CPU_FILTER_REG   ,EIM_FILTER_MULTICAST | EIM_FILTER_BROADCAST | EIM_FILTER_UNICAST);
#endif

  /* 0 == all ethernet interrupts disabled. */

  eth_out32(EIM_INT_EN_REG       ,0x00000000);
#if 1  
  eth_out32(EIM_CTRL_REG         ,EIM_CTRL_ENET1_DIS  |
	   EIM_CTRL_RXENET0_EN | EIM_CTRL_TXENET0_EN |
	   EIM_CTRL_RXCPU_EN   | EIM_CTRL_TXCPU_EN);
#else  
  eth_out32(EIM_CTRL_REG         ,EIM_CTRL_ENET1_DIS  | EIM_CTRL_ENET0_FLW  |
	   EIM_CTRL_RXENET0_EN | EIM_CTRL_TXENET0_EN |
	   EIM_CTRL_RXCPU_EN   | EIM_CTRL_TXCPU_EN);
#endif
#if 1  
  eth_out32(EIM_MFVHI_REG        ,0x00000000);
#else
  eth_out32(EIM_MFVHI_REG        ,0x0000ff00);
#endif  
  eth_out32(EIM_MFVLO_REG        ,0x00000000);
  eth_out32(EIM_MFMHI_REG        ,0x00000000);
  eth_out32(EIM_MFMLO_REG        ,0x00000000);
  eth_out32(EIM_RXTH_REG         ,0x00000018);
  eth_out32(EIM_CPU_RXRDY_REG    ,0x00000000);
#if 1
  eth_out32(ENET0_MODE_REG       ,ENET_MODE_RJCT_SFE | ENET_MODE_MWIDTH |
	   ENET_MODE_FULLDUPLEX);
#else
  eth_out32(ENET0_MODE_REG       ,ENET_MODE_RJCT_SFE | ENET_MODE_MWIDTH |
	   ENET_MODE_HALFDUPLEX);
#endif  
  eth_out32(ENET0_BOFFSEED_REG   ,0x00000000);
  eth_out32(ENET0_FLWPAUSE_REG   ,0x00000000);
  eth_out32(ENET0_FLWCONTROL_REG ,0x00000000);
  eth_out32(ENET0_VTYPE_REG      ,0x00000000);
#if 0  
  eth_out32(ENET0_ADRMODE_EN_REG ,ENET_ADR_BROADCAST | ENET_ADR_PROMISCUOUS);
#else

  /* The CPU port is not PROMISCUOUS, it wants a no-promiscuous address
   * match yet the the SWITCH receives packets from the PROMISCUOUS ENET0
   * which routes all packets for filter matching at the CPU port which
   * then allows the s/w to see the new incoming packetes that passed
   * the filter. Here we are setting the main SWITCH closest the ether
   * wire.
   */

  eth_out32(ENET0_ADRMODE_EN_REG ,ENET_ADR_PROMISCUOUS);
#endif  
  eth_out32(ENET0_DRP_REG        ,0x00000000);

#if 1  
  {
    int i;
    for (i = 0; i < 2000000; i++);
  }
#endif  
}

static const unsigned char hextable[256] = {
  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  0,   1,   2,   3,   4,   5,   6,   7,   
  8,   9, 255, 255, 255, 255, 255, 255,

  255,  10,  11,  12,  13,  14,  15, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255,  10,  11,  12,  13,  14,  15, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,

  255, 255, 255, 255, 255, 255, 255, 255, 
  255, 255, 255, 255, 255, 255, 255, 255,
};

/***********************************************************************
 * eth_ascii_to_bin
 ***********************************************************************/

static unsigned char eth_ascii_to_bin(unsigned char c)
{
  return hextable[c];
}

/***********************************************************************
 * eth_hexstrtobyte
 ***********************************************************************/

/* Incoming string is assumed to be a hex numeric string
 * (without the 0x prefix) whos converted value will fit within
 * a single byte.  example "1", "ff", "7d", "22", etc.
 */

static unsigned char eth_hexstrtobyte(char *str)
{
  unsigned short retval;
  int len;
  retval = 0;
  len = eth_strlen(str);
  if (2 == len)
    {
      retval += (unsigned short)(eth_ascii_to_bin(str[0])*0x10);
      retval += (unsigned short)(eth_ascii_to_bin(str[1]));
    }
  else if (1 == len)
    {
      retval += (unsigned short)(eth_ascii_to_bin(str[0]));
    }
  else
    {
      panic("Logic Error");
    }
  if (retval > 0x00FF)
    {
      panic("Logic Error");
    }
  return (unsigned char)retval;
}

/***********************************************************************
 * eth_fill_mac
 ***********************************************************************/

/* Input string is expected to be in standard MAC format;
 * x:x:x:x:x:x, where x can be up to two numeric hex characters.
 * example. "ff:0:e0:10:c:22" This call will convert the incoming
 * string into a series of numeric values which are loaded into
 * the callers supplied array; mac_array[0] through mac_array[5].
 */

#define MIN_MAC 11
#define MAX_MAC 17

static void eth_fill_mac(unsigned char *mac_array, char *MAC_str)
{
  /* example MAC: 1:2:3:4:5:6 (11 chars).
   * example MAC: 10:20:30:40:50:60 (17 chars).
  */

  char str[MAX_MAC+1];
  int i;
  unsigned char bval;
  unsigned char *start;
  unsigned char *end;
  bval = 0;
  if (eth_strlen(MAC_str) < MIN_MAC) return;
  if (eth_strlen(MAC_str) > MAX_MAC) return;

 /* get a local writable copy. */

  eth_strcpy(str,MAC_str);
  end = str;
  start = end;
  for (i = 0; i < 6; i++)
    {
      if (i < 5)
	{
	  while (*end != ':') end++;

	  /* replace the ':' with a 0 to terminate the string. */

	  *end = 0;
	}
      bval = eth_hexstrtobyte(start);
      mac_array[i] = bval;
      end++;
      start = end;
    }
}

/***********************************************************************
 * eth_mac_is_null
 ***********************************************************************/

static int eth_mac_is_null(unsigned char *mac_array)
{
  int i, ret_val;
  ret_val = 1;
  for (i=0; i<6; i++)
    {
      if (mac_array[i] != 0x00)
	{
	  ret_val = 0;
	  break;
	}
    }
  return ret_val;
}

/***********************************************************************
 * eth_mac_assign
 ***********************************************************************/

/* Set the mac address of our CPU ether port so that when
 * the SWITCH receives packets from the PROMISCUOUS ENET0
 * it will switch them to the CPU port and cause a packet
 * arrival event on the Switch's CPU TX queue when an address
 * match occurs. The CPU port is not PROMISCUOUS and wants to
 * see only packets specifically addressed to this device.
 */

static void eth_mac_assign(unsigned char *mac_array)
{
  /* Set CPU port mac address. s/w will only see
   * incoming packets that match this destination address.
  */

  eth_out32(EIM_CPU_DAHI_REG,((mac_array[0] << 8)  |
			     (mac_array[1])));
  eth_out32(EIM_CPU_DALO_REG,((mac_array[2] << 24) |
			     (mac_array[3] << 16) |
			     (mac_array[4] << 8)  |
			     (mac_array[5])));
  
  /* Set ENET port mac address.
   * incoming packets that match this destination address.
   * Except for multicast, ENET runs in PROMISCUOUS mode,
   * so these are only used to filter packets when in multicast
   * hast table mode (which is different than multicast
   * accept all mode which runs the ENET in PROMISCUOUS
     mode).
   */

  eth_out32(ENET0_PARHI_REG,((mac_array[0] << 8)  |
			     (mac_array[1])));
  eth_out32(ENET0_PARLO_REG,((mac_array[2] << 24) |
			     (mac_array[3] << 16) |
			     (mac_array[4] << 8)  |
			     (mac_array[5])));
}

/***********************************************************************
 * eth_incr_tx_cpu_desc
 ***********************************************************************/

#define WRAPbit 0x40000000

static void eth_incr_tx_cpu_desc(void)
{
  if (WRAPbit & eth_in32(current_tx_cpu_desc))
    {
      /* Loop back around to base of descriptor queue. */

      current_tx_cpu_desc = (unsigned long *)(eth_in32(EIM_CPU_TXBA_REG)
					      + EIM_RAM_START);
    }
  else
    {
      current_tx_cpu_desc+=2;
    }
}
  
/***********************************************************************
 * incr_rx_cpu_desc
 ***********************************************************************/

#define WRAPbit 0x40000000

static void incr_rx_cpu_desc(void)
{
  if (WRAPbit & eth_in32(current_rx_cpu_desc))
    {
      /* Loop back around to base of descriptor queue. */

      current_rx_cpu_desc = (unsigned long *)(eth_in32(EIM_CPU_RXBA_REG)
					      + EIM_RAM_START);
    }
  else
    {
      current_rx_cpu_desc+=2;
    }
}
  
/***********************************************************************
 * eth_tx_errors
 ***********************************************************************/

#define WRAP 0x40000000

int eth_tx_errors(void)
{
  int more;
  long *Descriptor_p = last_packet_desc_start;

  /* Note:
   * We walk that last packet we just sent to collect up
   * xmit status bits.
   */

  if (last_packet_desc_start && last_packet_desc_end)
    {
      more = 1;
      cached_tx_status = 0;
      while (more)
	{
	  cached_tx_status |= (eth_in32(Descriptor_p)&0x007f0000);
	  if (Descriptor_p == last_packet_desc_end)
	    {
	      more = 0;
	    }

	  /* This packet is made up of several descriptors;
	   * Find next one in chain. */

	  if (WRAPbit & eth_in32(Descriptor_p))
	    {

	      /* Loop back around to base of descriptor queue. */

	      Descriptor_p = (unsigned long *)(eth_in32(EIM_CPU_RXBA_REG)+EIM_RAM_START);
	    }
	  else
	    {
	      Descriptor_p+=2;
	    }
	}
    }
  
  if (cached_tx_status)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_tx_window_errors
 ***********************************************************************/

int eth_tx_window_errors(void)
{
  if ((TXSTATUS_RETRY_BIT     & cached_tx_status) ||
      (TXSTATUS_UNDER_BIT     & cached_tx_status) ||
      (TXSTATUS_CRC_BIT       & cached_tx_status) ||
      (TXSTATUS_COLLISION_BIT & cached_tx_status) ||
      (TXSTATUS_LATE_COL_BIT  & cached_tx_status))
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_tx_heartbeat_errors
 ***********************************************************************/

int eth_tx_heartbeat_errors(void)
{
  if (TXSTATUS_HEART_BIT & cached_tx_status)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_tx_fifo_errors
 ***********************************************************************/

int eth_tx_fifo_errors(void)
{
  return 0;
}

/***********************************************************************
 * eth_tx_carrier_errors
 ***********************************************************************/

int eth_tx_carrier_errors(void)
{
  if (TXSTATUS_LOSS_BIT & cached_tx_status)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_tx_aborted_errors
 ***********************************************************************/

int eth_tx_aborted_errors(void)
{
  return 0;
}

/***********************************************************************
 * eth_rx_missed_errors
 ***********************************************************************/

int eth_rx_missed_errors(void)
{
  if (RXSTATUS_MISS_BIT & cached_rx_status)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_rx_fifo_errors
 ***********************************************************************/

int eth_rx_fifo_errors(void)
{
  return 0;
}

/***********************************************************************
 * eth_rx_frame_errors
 ***********************************************************************/

int eth_rx_frame_errors(void)
{
  if ((RXSTATUS_LONG_BIT & cached_rx_status) ||
      (RXSTATUS_SHORT_BIT & cached_rx_status))
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_rx_crc_errors
 ***********************************************************************/

int eth_rx_crc_errors(void)
{
  if (RXSTATUS_CRC_BIT & cached_rx_status)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_rx_over_errors
 ***********************************************************************/

int eth_rx_over_errors(void)
{
  if (RXSTATUS_OVER_BIT & cached_rx_status)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_rx_length_errors
 ***********************************************************************/

int eth_rx_length_errors(void)
{
  if ((RXSTATUS_VLAN_BIT & cached_rx_status) ||
      (RXSTATUS_ALIGN_BIT & cached_rx_status))
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_rx_errors
 ***********************************************************************/

int eth_rx_errors(void)
{
  if (cached_rx_status)
    {
      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_recv_int_active
 ***********************************************************************/

int eth_recv_int_active(void)
{
  eim_status_shadow = eth_in32(EIM_STATUS_REG);
  if (EIM_CPU_TX_INT & eim_status_shadow)
    {
      /* An in coming packet has been received by
       * the EIM from the network and the interrupt
       * associated with EIM's CPU TX que has been
       * asserted. Rememeber that it is the EIM's CPU
       * TX que that we need to read from to get those
       * packets *into* the linux system.
       * This may seem a bit backwards to you but we
       * are keeping this terminology to stay consistent
       * with the C5471 documentation.
       */

      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_xmit_int_active
 ***********************************************************************/

int eth_xmit_int_active(void)
{
  /* Unfortunately with this platform the HW status
   * we need is implemented in a clear-on-read register.
   * We'll refrain from referencing the HW register
   * here since we happen to *know* that the client
   * will always ask about the recv_int *prior* to the
   * xmit_int and hence our shadow value here has the
   * current bits we need to test. Yes, this impl is
   * dependent on the *particular* usage model we know
   * our client (generic_ether.c) is using. I really 
   * dislike clear-on-read HW registers.
   */

  if (EIM_CPU_RX_INT & eim_status_shadow)
    {
      /* An outgoing packet has been processed by
       * the EIM and the interrupt associated with
       * EIM's CPU RX que has been asserted. Rememeber
       * that it is the EIM's CPU RX que that we put
       * packets on to send them *out*. This may seem a
       * bit backwards to you but we are keeping this
       * terminology to stay consistent with the C5471
       * documentation.
       */

      return 1;
    }
  else
    {
      return 0;
    }
}

/***********************************************************************
 * eth_get_default_MAC
 ***********************************************************************/

void eth_get_default_MAC(unsigned char *mac_array)
{
  int got_mac = 0;

  /* Ether MAC string deposited by bootloader to IRAM.  */

  if (0 == *(__EtherMACMagicStr+eth_strlen("etherMAC-->")))
    {
      /* okay, looks reasonable so far, do final comparison. */

      if (0 == eth_strcmp(__EtherMACMagicStr,"etherMAC-->"))
	{
	  /* okay, the magic string is there so we can assume that
	   * the invoker of the linux kernel has deposited a MAC
	   * string next -- even if only a NULL string.
	   */

	  if (0 != *(__EtherMAC))
	    {
	      /* the MAC string is not NULL so we'll use it
	       * as our default MAC address.
	       */

	      eth_fill_mac(mac_array,__EtherMAC);
	      got_mac = 1;
	    }
	}
    }
  if (!got_mac)
    {
      /* We didn't get a MAC from the bootloader so
       * use the default hardcoded value as a last resort.
       */

      warn("Mac address normally deposited by bootloader was not found");
#if 1  
      warn("Resorting to internally coded default");
      mac_array[0] = 0x00;
      mac_array[1] = 0xe0;
      mac_array[2] = 0xbe;
      mac_array[3] = 0xef;
      mac_array[4] = 0xfa;
      mac_array[5] = 0xce;
#endif
    }
}

/***********************************************************************
 * eth_set_MAC
 ***********************************************************************/

void eth_set_MAC(unsigned char *mac_array)
{
  int i;
  for (i = 0; i < 6; i++)
    {
      d_MAC[i] = mac_array[i];
    }
}

/***********************************************************************
 * eth_multicast_accept_multicast
 ***********************************************************************/

void eth_multicast_accept_multicast(void)
{
  /* valid Ethernet MAC multicast address range
     01:00:00:00:00:00 - 01:FF:FF:FF:FF:FF

     based on MFV0/MFV1 description on pg 12-39 and
              MFM0/MFM1 description on pg 12-40

     I believe the hardware "ands" the destination address (DA)
     of the incoming packet with MFM0/MFM1 and then compares the
     result with MFV0/MFV1.  If the compare is an exact match, 
     then the packet is accepted as a multicast packet.

     set MFM to 01:00:00:00:00:00
     set MFV to 01:00:00:00:00:00
    
     this should accept all DAs with the low order bit in the 
     high-order octet - bit 40.
  */

  eth_out32(EIM_MFVHI_REG        ,0x00000100);
  eth_out32(EIM_MFVLO_REG        ,0x00000000);

  eth_out32(EIM_MFMHI_REG        ,0x00000100);
  eth_out32(EIM_MFMLO_REG        ,0x00000000);
}

/***********************************************************************
 * eth_multicast_promiscuous
 ***********************************************************************/

void eth_multicast_promiscuous(void)
{
  /* Note: TF 08/21/03
     This probably won't work because the earlier version of the
     driver was configured with the MFM and MFV registers set to
     zero and multicast enabled in the CPU filter register.  See
     the eth_eim_specific_config() routine above.

     Use multicast filter to filter all packets to go though at the
     CPU level.  This is done because I don't know how else to
     enable promiscuous at the CPU level.

     I believe the hardware "ands" the destination address (DA)
     of the incoming packet with MFM0/MFM1 and then compares the
     result with MFV0/MFV1.  If the compare is an exact match, 
     then the packet is accepted.

     set MFM to 00:00:00:00:00:00
     set MFV to 00:00:00:00:00:00
    
     this should accept all DAs.
  */

  eth_out32(EIM_MFVHI_REG        ,0x00000000);
  eth_out32(EIM_MFVLO_REG        ,0x00000000);

  eth_out32(EIM_MFMHI_REG        ,0x00000000);
  eth_out32(EIM_MFMLO_REG        ,0x00000000);
}

/***********************************************************************
 * eth_set_hast_multicast
 ***********************************************************************/

void eth_set_hast_multicast(long hash_table)
{
  int i;
  i = (hash_table & 0xFFFF0000) >> 16;
  eth_out32(ENET0_LARHI_REG, i);
  i = (hash_table & 0xFFFF);
  eth_out32(ENET0_LARLO_REG, i);
}

/***********************************************************************
 * eth_set_addressing
 ***********************************************************************/

void eth_set_addressing(int eim_addr)
{
  eth_out32(ENET0_ADRMODE_EN_REG, eim_addr);
}

/***********************************************************************
 * eth_set_filter
 ***********************************************************************/

void eth_set_filter(int cpu_fltr)
{
  eth_out32(EIM_CPU_FILTER_REG, cpu_fltr);
}

/***********************************************************************
 * eth_turn_on_ethernet
 ***********************************************************************/

void eth_turn_on_ethernet(void)
{
  volatile unsigned long clearbits;

  dbg("Begin Ethernet HW setup");
  dbg("EIM reset");

  eth_eim_reset();

  dbg("PHY init");

  eth_phy_init();

  dbg("EIM config");

  eth_eim_specific_config();

  dbg("MAC assign");

  if (eth_mac_is_null(d_MAC))
    {
      /* load d_MAC with either the default MAC */

      eth_get_default_MAC(d_MAC);

      /* passed to us from the bootloader or
       * possibly uses a special on-board serial 
       * eeprom to located the board's MAC.
       */
    }
  eth_mac_assign(d_MAC);

  /* Next, enable interrupts going from EIM Module to Interrupt Module. */

  /* dummy read of clear-on-read register. */

  clearbits = eth_in32(EIM_STATUS_REG);
  eth_out32(EIM_INT_EN_REG,(eth_in32(EIM_INT_EN_REG) | EIM_CPU_TX_INT | EIM_CPU_RX_INT));

  /* Next, go on-line.
   * According to the C5471 documentation the enables have to occur
   * in this order to insure proper operation; SWITCH first then the
   * ENET.
   */

  dbg("EIM on-line");

  /* enable SWITCH */

  eth_out32(EIM_CTRL_REG,(eth_in32(EIM_CTRL_REG) | EIM_CTRL_SWITCH_EN));

  /* enable ENET */

  eth_out32(ENET0_MODE_REG,(eth_in32(ENET0_MODE_REG) | ENET_MODE_ENABLE));
  dbg("Ethernet HW setup complete");
  return;
}

/***********************************************************************
 * eth_turn_off_ethernet
 ***********************************************************************/

void eth_turn_off_ethernet(void)
{
  /* Next, disable interrupts going from EIM Module to Interrupt Module. */

  eth_out32(EIM_INT_EN_REG,(eth_in32(EIM_INT_EN_REG) & ~EIM_CPU_TX_INT & ~EIM_CPU_RX_INT));

  /* Next, go off-line. */

 /* disable ENET */

  eth_out32(ENET0_MODE_REG,(eth_in32(ENET0_MODE_REG) & ~ENET_MODE_ENABLE));

  /* disable SWITCH */

  eth_out32(EIM_CTRL_REG,(eth_in32(EIM_CTRL_REG) & ~EIM_CTRL_SWITCH_EN));
  return;
}

/***********************************************************************
 * eth_get_received_packet_len
 ***********************************************************************/

#define WRAP 0x40000000

void eth_get_received_packet_len(int *numbytes)
{
  int more, bytelen;
  unsigned int packetlen = 0;
  long *Descriptor_p = current_tx_cpu_desc;

  /* We walk the packet contained within the EIM
   * to add up the number of bytes contained
   * in the packet. We don't actually pull the
   * packet out of HW at this point. We're just
   * looking in to see how long the packet is.
   */

  cached_rx_status = 0;
  more = 1;
  while (more)
    {
      if (EIM_DESC_OWNER_h_w & eth_in32(Descriptor_p))
	{
	  /* words #0 and #1 of descriptor
	   * The incoming packe queue is empty. There are no more
	   * packets for the INET subsystem to process. Indicate this
	   * with a packet len of zero.
	   */

	  *numbytes = 0;
	  return;
	}

      cached_rx_status |= (eth_in32(Descriptor_p)&0x007f0000);
      bytelen = (eth_in32(Descriptor_p)&0x000007ff);
      packetlen += bytelen;

      if (eth_in32(Descriptor_p) & LASTINFRAMEbit)
	{
	  more = 0;
	}

      /* This packet is made up of several descriptors;
       * Find next one in chain.
       */

      if (WRAPbit & eth_in32(Descriptor_p))
	{
	  /* Loop back around to base of descriptor queue. */

	  Descriptor_p = (unsigned long *)(eth_in32(EIM_CPU_TXBA_REG)+EIM_RAM_START);
	}
      else
	{
	  Descriptor_p+=2;
	}
    }

  /* Next, We know that this routine is used by the client to 
   * allocate memory for the actual packet and we know that
   * our packet transfer algorithm in eth_get_received_packet_data()
   * may overshoot by "ROUNDUP" amount of bytes so adjust the
   * length here to insure that the client allocates a buffer
   * big enough to allow us to perform the packet xfer with its
   * potential overshoot.
   */

  *numbytes = packetlen+ROUNDUP;
  return;
}

/***********************************************************************
 * eth_get_received_packet_data
 ***********************************************************************/

void eth_get_received_packet_data(unsigned char *buf, int *numbytes)
{
  unsigned short bytelen, numshorts, more, i, j;
  unsigned int packetlen;
  unsigned short *packetmem;

  j = bytelen = packetlen = 0;

  /* We walk the newly received packet contained within 
   * the EIM and transfer its contents to the client supplied
   * buffer. This frees up the memory contained within
   * the EIM for additional packets that might be received
   * later from the network.
   */

  more = 1;
  while (more)
    {
      if (EIM_DESC_OWNER_h_w & eth_in32(current_tx_cpu_desc))
	{
	  /* words #0 and #1 of descriptor
	   *
	   * We don't ever expect to get here. If we do it means that
	   * the client thought we had an incoming packet to process
	   * and yet we see now that there isn't one. So, construct
	   * a default answer and let the upper layers of the system sort
	   * out the confusion.
	   */

	  err("LOGIC Error! eth_get_received_packet_data()");

	  /* return a null packet. */

	  memset(buf,0,*numbytes);
	  return;
	}
      bytelen = (eth_in32(current_tx_cpu_desc)&0x000007ff);
      packetlen += bytelen;

      /* words #2 and #3 of descriptor */

      packetmem = (unsigned short *)eth_in32(current_tx_cpu_desc+1);

      /* divide by 2 with round up. */

      numshorts = (bytelen+ROUNDUP)>>1;
      for (i=0; i<numshorts; i++, j++)
	{
	  /* 16bits at a time. */

	  ((unsigned short *)buf)[j] = __swap_16(packetmem[i]);
	}
      if (eth_in32(current_tx_cpu_desc) & LASTINFRAMEbit)
	{
	  more = 0;
	}

      /* Next, Clear all bits of words0/1 of the emptied descriptor
       * except preserve the settings of a select few. Can
       * leave descriptor words 2/3 alone.
       */

      eth_out32(current_tx_cpu_desc,(eth_in32(current_tx_cpu_desc)&(EIM_TXDESC_WRAP|CPU_TXDESC_INTEN)));

      /* Next, Give ownership of now emptied descriptor
       * back to the Ether Module's SWITCH.
       */

      eth_out32(current_tx_cpu_desc,(eth_in32(current_tx_cpu_desc)|EIM_DESC_OWNER_h_w));
      eth_incr_tx_cpu_desc(); /* advance; to find our next data buffer. */
    }

  /* Next, adjust the length to remove the crc bytes that we know the
   * client doesn't care about. I suspect the crc bytes would be ignored
   * anyway by the Kernel's INET subsystem but I notice that some eth
   * drivers for other platforms (DSC21) don't deliver the crc bytes
   * so for consistency I'll remove them here too.
   */

  *numbytes = packetlen - 4;
  return;
}

/***********************************************************************
 * eth_send_packet
 ***********************************************************************/

/* Use the HW to send the supplied Ether buffer out the board's network
 * cable. We'll rely on the HW to supply the CRC of the packet as well as
 * any necessary retrys required due to normal network collisions, etc.
 */

void eth_send_packet(unsigned char *buf, int numbytes)
{
  unsigned int i, j, FirstFrame;
  unsigned short bytelen, numshorts;
  unsigned short *packetmem;

  j = 0;
  FirstFrame = 1;
  last_packet_desc_start = current_rx_cpu_desc;

  while (numbytes)
    {
      while (EIM_DESC_OWNER_h_w & eth_in32(current_rx_cpu_desc))
	{
	  /* words #0 and #1 of descriptor
	   * loop until the SWITCH lets go of the descriptor
	   * giving us access rights to submit our new ether
	   * frame to it.
	   * do_delay(1000);
	   */
	}
      if (FirstFrame)
	{
	  eth_out32(current_rx_cpu_desc,
		   (eth_in32(current_rx_cpu_desc)|FIRSTINFRAMEbit));
	}
      else
	{
	  eth_out32(current_rx_cpu_desc,
		   (eth_in32(current_rx_cpu_desc)&~FIRSTINFRAMEbit));
	}
      eth_out32(current_rx_cpu_desc,
	       (eth_in32(current_rx_cpu_desc)&~ENET_DESC_PADCRC));
      if (FirstFrame)
	{
	  eth_out32(current_rx_cpu_desc,
		   (eth_in32(current_rx_cpu_desc)|ENET_DESC_PADCRC));
	}
      if (numbytes >= EIM_PACKET_BYTES)
	{
	  bytelen = EIM_PACKET_BYTES;
	}
      else
	{
	  bytelen = numbytes;
	}

      /* Next, submit ether frame bytes to the C5472 Ether Module packet
       * memory space.
       */

      /* divide by 2 with round up. */

      numshorts = (bytelen+1)>>1;

      /* words #2 and #3 of descriptor */

      packetmem = (unsigned short *)eth_in32(current_rx_cpu_desc+1);
      for (i=0; i<numshorts; i++, j++)
	{
	  /* 16bits at a time. */

	  packetmem[i] = __swap_16(((unsigned short *)buf)[j]);
	}

      eth_out32(current_rx_cpu_desc,
		((eth_in32(current_rx_cpu_desc) & DW1_BYTES_MASK) | bytelen));

      numbytes -= bytelen;
      if (0 == numbytes)
	{
	  eth_out32(current_rx_cpu_desc,
		   (eth_in32(current_rx_cpu_desc)|LASTINFRAMEbit));
	}
      else
	{
	  eth_out32(current_rx_cpu_desc,
		   (eth_in32(current_rx_cpu_desc)&~LASTINFRAMEbit));
	}

      /* Next, We're done with that descriptor;
       * give access rights back to HW. */

      eth_out32(current_rx_cpu_desc,
	       (eth_in32(current_rx_cpu_desc)|EIM_DESC_OWNER_h_w));

      /* Next, tell Ether Module that those submitted bytes are ready
       * for the wire. */

      eth_out32(EIM_CPU_RXRDY_REG,0x00000001);
      last_packet_desc_end = current_rx_cpu_desc;
      incr_rx_cpu_desc(); /* advance to next free descriptor. */
      FirstFrame = 0;
    }
  return;
}
