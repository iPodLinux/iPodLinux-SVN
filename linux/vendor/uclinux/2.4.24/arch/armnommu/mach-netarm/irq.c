/*
 * arch/armnommu/mach-netarm/irq.c:
 *
 * NET+ARM  IRQ mask, unmask & init routines.
 *
 * (C) 2001 IMMS gGmbH (http://www.imms.de)
 * author(s): Rolf Peukert (rolf.peukert@imms.de)
 *
 * partially based on work by Gordon McNutt and Joe deBlaquiere
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

#include <linux/module.h>	        // EXPORT_SYMBOL
#include <linux/config.h>
#include <asm/arch/irq.h>
#include <asm/arch/netarm_registers.h>

#ifdef CONFIG_SERIAL_NETARM

/* Mask & Acknowledge function for serial channel 1, RX and TX interrupts */
extern void nas_mask_ack_serial1_irq( unsigned int irq );
/* dto. for serial channel 2, both defined in drivers/char/serial_netarm.c */
extern void nas_mask_ack_serial2_irq( unsigned int irq );

#else
/* minimal Mask & Acknowledge functions */
void nas_mask_ack_serial1_irq( unsigned int irq )
{
	volatile unsigned long int *mask = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_CLR);
	volatile unsigned long int *stat_a =
		(volatile unsigned long *)(NETARM_SER_MODULE_BASE +
		NETARM_SER_CH1_STATUS_A);
	
	if ((irq == IRQ_SER1_TX) || (irq == IRQ_SER1_RX))
	{
		*mask  	= (1 << irq);	/* set mask bit */
		*stat_a	= 0xFFFF;	/* clear all IRQ pending bits */
	}
}
void nas_mask_ack_serial2_irq( unsigned int irq )
{
	volatile unsigned long int *mask = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_CLR);
	volatile unsigned long int *stat_a =
		(volatile unsigned long *)(NETARM_SER_MODULE_BASE +
		NETARM_SER_CH2_STATUS_A);

	if ((irq == IRQ_SER2_TX) || (irq == IRQ_SER2_RX))
	{
		*mask  	= (1 << irq);	/* set mask bit */
		*stat_a	= 0xFFFF;	/* clear all IRQ pending bits */
	}
}
#endif /* if CONFIG_SERIAL_NETARM... */


/* Generic Mask IRQ function */
void netarm_mask_irq(unsigned int irq)
{
	volatile unsigned long int *mask = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_CLR) ;
	
	if (32 > irq) *mask = ( 1 << irq );
}

/* Generic Unmask IRQ function */
void netarm_unmask_irq(unsigned int irq)
{
	volatile unsigned long int *set = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_SET) ;
	
	if (32 > irq) *set = ( 1 << irq );
}

/* Dummy Mask&Ack function */
void netarm_mask_ack_irq(unsigned int irq)
{
	netarm_mask_irq(irq);
        /* The NET+ARM doesn't implement a general IRQ status register
	   like the DSC21 -> we need individual mask_ack_routines */
}

/* Mask & Acknowledge function for Timer 1 IRQ */
void netarm_mask_ack_t1_irq(unsigned int irq)
{
	volatile unsigned long int *mask = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_CLR) ;
	volatile unsigned long int *stat = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_TIMER1_STATUS) ;

	if (irq == IRQ_TIMER1)
	{
		*mask = (1 << IRQ_TIMER1);	/* constant, should be calculated at compile time */
		*stat = NETARM_GEN_TSTAT_INTPEN;/* this clears the IRQ pending bit */
	}
}

/* Mask & Acknowledge function for Timer 2 IRQ */
void netarm_mask_ack_t2_irq(unsigned int irq)
{
	volatile unsigned long int *mask = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_CLR) ;
	volatile unsigned long int *stat = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_TIMER2_STATUS) ;

	if (irq == IRQ_TIMER2)
	{
		*mask = (1 << IRQ_TIMER2);	/* = 0x00000100, bit 4 set */
		*stat = NETARM_GEN_TSTAT_INTPEN;/* clear IRQ pending bit */
	}
}

/* Mask & Acknowledge function for Port C Bit 0..3 IRQs */
void netarm_mask_ack_portc_irq(unsigned int irq)
{
	int 	irq_bit;
	volatile unsigned long int *mask = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_CLR) ;
	volatile unsigned long int *portc = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_PORTC) ;

	if (irq <= IRQ_PORTC3) /* Interrupts 0..3 correspond to Port C bits 0..3 */
	{
		irq_bit = 1 << irq;
		*mask   = irq_bit;		/* set mask bit */
		*portc  = *portc | irq_bit;	/* acknowledge IRQ */
	}
}

/* Mask & Acknowledge function for DMA channel 1 IRQ */
void netarm_mask_ack_dma1_irq(unsigned int irq)
{
	volatile unsigned long int *mask = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_CLR) ;
	volatile unsigned long int *dmastata = 
    get_dma_reg_addr(NETARM_DMA_CHANNEL_1A, NETARM_DMA_STATUS);
	volatile unsigned long int *dmastatb = 
    get_dma_reg_addr(NETARM_DMA_CHANNEL_1B, NETARM_DMA_STATUS);
	volatile unsigned long int *dmastatc = 
    get_dma_reg_addr(NETARM_DMA_CHANNEL_1C, NETARM_DMA_STATUS);
	volatile unsigned long int *dmastatd = 
    get_dma_reg_addr(NETARM_DMA_CHANNEL_1D, NETARM_DMA_STATUS);

	if (IRQ_DMA01 == irq)
	{
		*mask   = (1 << IRQ_DMA01);	/* set mask bit */
		/* A better irq routine should differentiate between the 16 possible
		   IRQ sources. But for now we just acknowledge all IRQ sources */
		*dmastata = NETARM_DMA_STATUS_IP_MA;
		*dmastatb = NETARM_DMA_STATUS_IP_MA;
		*dmastatc = NETARM_DMA_STATUS_IP_MA;
		*dmastatd = NETARM_DMA_STATUS_IP_MA;
	}
}

/* Mask & Acknowledge function for DMA channels 2..10 IRQs */
void netarm_mask_ack_dmax_irq(unsigned int irq)
{
	int	channel;
	volatile unsigned long int *mask = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_CLR) ;
	volatile unsigned long int *dmastat;

	/* Interrupts 30..22 correspond to DMA channels 2..10
	   Note the "reverse" numbering scheme (HW Reference 4.4) */
	if ((IRQ_DMA02 >= irq) && (irq >= IRQ_DMA10))
	{
		*mask   = (1 << irq);		/* set mask bit */
		channel = 32 - irq;		/* DMA channel, 1..10 */
		dmastat = get_dma_reg_addr(NETARM_DMA_CHANNEL_2 + ((channel - 2) * 0x20), NETARM_DMA_STATUS);
		/* A better irq routine should differentiate between the four possible
		   IRQ sources. But as a quick workaround, we acknowledge all sources */
		*dmastat = NETARM_DMA_STATUS_IP_MA;
	}
}

/* setup IRQ descriptor array */
void irq_init_irq(void)
{
	int irq;
	unsigned long flags;
	volatile unsigned long int *mask = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_CLR) ;
	volatile unsigned long int *set = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE_SET) ;
	volatile unsigned long int *ie = 
		(volatile unsigned long *)(NETARM_GEN_MODULE_BASE + 
		NETARM_GEN_INTR_ENABLE) ;

	for (irq = 0; irq < NR_IRQS; irq++)
	{
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;

		switch (irq)
		{
			case IRQ_PORTC0:
			case IRQ_PORTC1:
			case IRQ_PORTC2:
			case IRQ_PORTC3:
					irq_desc[irq].mask_ack = netarm_mask_ack_portc_irq;
					break;
			case IRQ_TIMER2:
					irq_desc[irq].mask_ack = netarm_mask_ack_t2_irq;
					break;
			case IRQ_TIMER1:
					irq_desc[irq].mask_ack = netarm_mask_ack_t1_irq;
					break;
			case IRQ_SER1_TX:
			case IRQ_SER1_RX:
					irq_desc[irq].mask_ack = nas_mask_ack_serial1_irq;
					break;			
			case IRQ_SER2_TX:
			case IRQ_SER2_RX:
					irq_desc[irq].mask_ack = nas_mask_ack_serial2_irq;
					break;			
			case IRQ_DMA10:
			case IRQ_DMA09:
			case IRQ_DMA08:
			case IRQ_DMA07:
			case IRQ_DMA06:
			case IRQ_DMA05:
			case IRQ_DMA04:
			case IRQ_DMA03:
					irq_desc[irq].mask_ack = netarm_mask_ack_dmax_irq;
					break;
#ifdef CONFIG_ETHER_NETARM
			/* The IRQ routines in netarmeth.c do their own acknowledge,
			   so don't clear the pending bits here */
			case IRQ_DMA02:
					irq_desc[irq].mask_ack = netarm_mask_irq;
					break;
			case IRQ_DMA01:
					irq_desc[irq].mask_ack = netarm_mask_irq;
					break;
#else
			/* Use our Mask-Ack functions, just for the case... */
			case IRQ_DMA02:
					irq_desc[irq].mask_ack = netarm_mask_ack_dmax_irq;
					break;
			case IRQ_DMA01:
					irq_desc[irq].mask_ack = netarm_mask_ack_dma1_irq;
					break;
#endif /* CONFIG_ETHER_NETARM... */
			default:
					irq_desc[irq].mask_ack = netarm_mask_ack_irq;
					break;
		};
		irq_desc[irq].mask	= netarm_mask_irq;
		irq_desc[irq].unmask	= netarm_unmask_irq;
		irq_desc[irq].action	= NULL;
	}
	/* clear all status * disable all */
	save_flags_cli (flags);
	*mask = 0xffffffff;
	*set = 0x5aa55aa5;
	*set = 0x00000000;
	if (*ie != 0x5aa55aa5)
		 _netarm_led_FAIL();
	*mask = 0xffffffff;		/* clear all interrupt enables */
	restore_flags (flags);
}

EXPORT_SYMBOL( netarm_mask_irq );
EXPORT_SYMBOL( netarm_unmask_irq );
EXPORT_SYMBOL( netarm_mask_ack_irq );
EXPORT_SYMBOL( netarm_mask_ack_t1_irq );
EXPORT_SYMBOL( netarm_mask_ack_t2_irq );
EXPORT_SYMBOL( netarm_mask_ack_portc_irq );
EXPORT_SYMBOL( netarm_mask_ack_dma1_irq );
EXPORT_SYMBOL( netarm_mask_ack_dmax_irq );
EXPORT_SYMBOL( irq_init_irq );

