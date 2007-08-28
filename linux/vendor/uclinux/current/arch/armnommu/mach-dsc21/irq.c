/*
 * irq.c:
 *         dsc21 IRQ mask, unmask & ack routines.
 * copyright:
 *         (C) 2001 RidgeRun, Inc. (http://www.ridgerun.com)
 * author: Gordon McNutt <gmcnutt@ridgerun.com>
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

#include <linux/types.h>
#include <asm/arch/irq.h>

void dsc21_mask_irq(unsigned int irq)
{
	if (irq > 15) *((u16*)DSC21_IRQ1_ENABLE) &= ~(1 << (irq - 16));
	else *((u16*)DSC21_IRQ0_ENABLE) &= ~(1 << irq);
}	

void dsc21_unmask_irq(unsigned int irq)
{
	if (irq > 15)  *((u16*)DSC21_IRQ1_ENABLE) |= (1 << (irq - 16));
	else *((u16*)DSC21_IRQ0_ENABLE) |= (1 << irq);
}

void dsc21_mask_ack_irq(unsigned int irq)
{
	dsc21_mask_irq(irq);
	if (irq > 15) *((u16*)DSC21_IRQ1_STATUS) |= (0x01 << (irq - 16));
	else *((u16*)DSC21_IRQ0_STATUS) |= (0x01 << irq);
}
