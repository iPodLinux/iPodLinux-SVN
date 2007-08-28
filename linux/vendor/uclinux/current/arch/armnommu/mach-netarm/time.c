/*
 * time.c  Timer functions for the NET+ARM.
 *         
 * copyright:
 *         (C) 2001 RidgeRun, Inc. (http://www.ridgerun.com)
 * authors: Gordon McNutt <gmcnutt@ridgerun.com>
 *          Joe deBlaquiere
 *          Rolf Peukert
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

#include <linux/time.h>
#include <linux/timex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/netarm_registers.h>

unsigned long netarm_gettimeoffset (void)
{
	volatile unsigned int *timer_status = 
		(volatile unsigned int *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_TIMER2_STATUS) ;

	/* convert milliseconds to microseconds */
	return  ( NETARM_GEN_TIMER_MSEC_P(*timer_status) * 1000 ) ;
}

void netarm_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	do_timer(regs);

/* in version 2.0.38:
	if (reset_timer ())
		do_timer(regs);
	update_rtc ();
*/
}

