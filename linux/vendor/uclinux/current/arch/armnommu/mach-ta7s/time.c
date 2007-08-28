/*
 * time.c  Timer functions for the Triscend A7S.
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
#include <linux/autoconf.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <asm/arch/arch.h>

#ifndef CONFIG_OSC_FREQ
  #define CONFIG_OSC_FREQ	25000000
#endif

#ifdef CONFIG_USE_A7HAL
#include <asm/arch/a7hal/clock.h>
#include <asm/arch/a7hal/timer.h>
#else

/*
 * Calculate the cpu speed based on the PLL, if the PLL
 * is not being used, this function will return 0
 */
unsigned long
a7hal_clock_getFreq (int noRAM)
{
  unsigned int freq = 0;
#if defined(A7VE) || defined(A7VC) || defined(A7VT) || defined(A7VL)
  unsigned long feedbackDivider;
  unsigned long outputDivider;
  unsigned long referenceDivider;
#else
  unsigned int scale;
#endif

  if (GET_BIT (SYS_CLOCK_CONTROL_REG, CLK_SEL_BIT))
    {
      if (GET_BIT (SYS_CLOCK_CONTROL_REG, PLL_SEL_BIT))
	{
#if defined(A7VE) || defined(A7VC) || defined(A7VT) || defined(A7VL)
	  feedbackDivider = GET_FIELD (SYS_PLL_CONTROL_REG,
				       PLL_CLKF_FIELD, NBITS_PLL_CLKF) + 1;

	  outputDivider = GET_FIELD (SYS_PLL_CONTROL_REG,
				     PLL_CLKOD_FIELD, NBITS_PLL_CLKOD) + 1;

	  referenceDivider = GET_FIELD (SYS_PLL_CONTROL_REG,
					PLL_CLKR_FIELD, NBITS_PLL_CLKR) + 1;


          /*
           * Is the input to the PLL the 32KHz xtal or the external osc.
           */
          if ( GET_BIT( SYS_PLL_CONTROL_REG, PLL_REFSEL_BIT ) )
          {
            freq = ( CONFIG_OSC_FREQ * feedbackDivider ) /
                ( referenceDivider * outputDivider );
          }
          else
          {
              freq = ( 32768 * feedbackDivider ) /
                  ( referenceDivider * outputDivider );
          }

#else
	  freq = 32768 * GET_FIELD (SYS_CLOCK_CONTROL_REG,
				    PLL_DIV_FIELD, NBITS_PLL_DIV);
	  scale =
	    GET_FIELD (SYS_CLOCK_CONTROL_REG, PLL_SCALE_FIELD,
		       NBITS_PLL_SCALE);
	  while (scale)
	    {
	      freq >>= 1;
	      scale >>= 1;
	    }
#endif
	}
      else
	{
	  freq = CONFIG_OSC_FREQ;
	}
    }

  return freq;
}

void
a7hal_timer_start (unsigned short count, int whichTimer,
		   unsigned long control)
{
  A7_REG (TIMER0_LOAD_REG) = count;
  A7_REG (TIMER0_CONTROL_REG) = (1L << TIM_ENABLE_BIT) | control;
}

void
a7hal_timer_clearInt (int whichTimer)
{
  A7_REG (TIMER0_CLEAR_REG) = (1L << TIM_INT_CLEAR_BIT);
}

unsigned short
a7hal_timer_read (int whichTimer)
{
  return (A7_REG (TIMER0_VALUE_REG));
}

#endif

unsigned long
ta7_gettimeoffset (void)
{
  return (a7hal_timer_read (0) * (1000000) / (a7hal_clock_getFreq (0) / 16));
}

void
ta7_timer_interrupt (int irq, void *dev_id, struct pt_regs *regs)
{
  do_timer (regs);
  a7hal_timer_clearInt (0);
}
