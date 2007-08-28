/***********************************************************************
 *  linux/arch/armnommu/mach-c5471/irq.c
 *
 *   C5471 IRQ mask, unmask & ack routines.
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
 ***********************************************************************/

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/system.h>
#include <asm/irq.h>

#include <asm/mach/irq.h>

#include <asm/arch/irq.h>
#include <asm/arch/hardware.h>

#define EdgeSensitive 0x00000020
#define Priority      0x0000001E

#define REG32(addr) *((volatile unsigned long*)(addr))

/* Acknowlede the IRQ.
 *
 * Bit 0 of the Interrupt Control Rigster ==  New IRQ agreement (NEW_IRQ_AGR).
 * Reset IRQ output. Clear source IRQ register. Enables a new IRQ
 * generation. Reset by internal logic.
 */

static inline void irq_ack(unsigned int irq)
{
  unsigned long reg;
  reg = REG32(SRC_IRQ_REG);          /* Insure appropriate IT_REG bit clears */
  REG32(INT_CTRL_REG) |= 0x00000001; /* write the NEW_IRQ_AGR bit. */
}

/* Acknowledge the FIQ.
 *
 * Bit 1 of the Interrupt Control Rigster ==  New FIQ agreement (NEW_FIQ_AGR).
 * Reset FIQ output. Clear source FIQ register. Enables a new FIQ
 * generation. Reset by internal logic.
 */

static inline void fiq_ack(unsigned int irq)
{
  unsigned long reg;
  reg = REG32(SRC_FIQ_REG);          /* Insure appropriate IT_REG bit clears */
  REG32(INT_CTRL_REG) |= 0x00000002; /* write the NEW_FIQ_AGR bit. */
}

/* Mask the IRQ. */

void c5471_mask_irq(unsigned int irq)
{
  REG32(MASK_IT_REG) |= (1 << irq);
}

/* Unmask the IRQ */

void c5471_unmask_irq(unsigned int irq)
{
  REG32(MASK_IT_REG) &= ~(1 << irq);
}

/* Mask the IRQ and acknowledge it.
 * 
 * Bit 0 of the Interrupt Control Rigster ==  New IRQ agreement (NEW_IRQ_AGR).
 * Reset IRQ output. Clear source IRQ register. Enables a new IRQ
 * generation. Reset by internal logic.
 *
 * IRQ (FIQ) output and SRC_IRQ_REG and SRC_IRQ_BIN_REG
 * (SRC_FIQ_REG) registers are reset only if the bit in the
 * Interrupt register (IT_REG) corresponding to the interrupt
 * having requested MCU action is already cleared or masked.
 *
 * For an edge-sensitive interrupt, the Interrupt register bit is
 * deactivated when reading the SRC_IRQ_REG or SRC_IRQ_BIN_REG
 * (SRC_FIQ_REG) registers.
 */

void c5471_mask_ack_irq(unsigned int irq)
{
  REG32(MASK_IT_REG)  |= (1 << irq); /* Make sure the interrupt is masked. */
  REG32(INT_CTRL_REG) |= 0x00000001; /* write the NEW_IRQ_AGR bit. */
}

void c5471_init_irq(void)
{
  /* Disable all interrupts. */

  REG32(MASK_IT_REG) = 0x0000ffff;

  /* Clear any pending interrupts */

  irq_ack(0);
  fiq_ack(0);
  REG32(IT_REG) = 0x00000000;

  /* Override hardware defaults */

  REG32(ILR_IRQ2_REG)  = EdgeSensitive | Priority;
  REG32(ILR_IRQ4_REG)  = EdgeSensitive | Priority;
  REG32(ILR_IRQ6_REG)  = Priority;
  REG32(ILR_IRQ15_REG) = EdgeSensitive | Priority;

  /* Finally, be a good citizen and do what it takes to insure
   * the user gets accurate info if they type "cat /proc/ioports"
   */

  request_region(INT_FIRST_IO, INT_IO_RANGE, "interrupt controller");
}
