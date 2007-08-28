/*
 *  madgemc.c: Driver for the Madge Smart 16/4 MC16 MCA token ring card.
 *
 *  Written 2000 by Adam Fritzler
 *
 *  This software may be used and distributed according to the terms
 *  of the GNU General Public License, incorporated herein by reference.
 *
 *  This driver module supports the following cards:
 *      - Madge Smart 16/4 Ringnode MC16
 *	- Madge Smart 16/4 Ringnode MC32 (??)
 *
 *  Maintainer(s):
 *    AF	Adam Fritzler		mid@auk.cx
 *
 *  Modification History:
 *	16-Jan-00	AF	Created
 *
 */
static const char version[] = "madgemc.c: v0.91 23/01/2000 by Adam Fritzler\n";

#include <linux/module.h>
#include <linux/mca.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/netdevice.h>
#include <linux/trdevice.h>
#include "tms380tr.h"
#include "madgemc.h"            /* Madge-specific constants */

#define MADGEMC_IO_EXTENT 32
#define MADGEMC_SIF_OFFSET 0x08

struct madgemc_card {
	struct net_device *dev;

	/*
	 * These are read from the BIA ROM.
	 */
	unsigned int manid;
	unsigned int cardtype;
	unsigned int cardrev;
	unsigned int ramsize;
	
	/*
	 * These are read from the MCA POS registers.  
	 */
	unsigned int burstmode:2;
	unsigned int fairness:1; /* 0 = Fair, 1 = Unfair */
	unsigned int arblevel:4;
	unsigned int ringspeed:2; /* 0 = 4mb, 1 = 16, 2 = Auto/none */
	unsigned int cabletype:1; /* 0 = RJ45, 1 = DB9 */

	struct madgemc_card *next;
};
static struct madgemc_card *madgemc_card_list;


int madgemc_probe(void);
static int madgemc_open(struct net_device *dev);
static int madgemc_close(struct net_device *dev);
static int madgemc_chipset_init(struct net_device *dev);
static void madgemc_read_rom(struct madgemc_card *card);
static unsigned short madgemc_setnselout_pins(struct net_device *dev);
static void madgemc_setcabletype(struct net_device *dev, int type);

static int madgemc_mcaproc(char *buf, int slot, void *d);

static void madgemc_setregpage(struct net_device *dev, int page);
static void madgemc_setsifsel(struct net_device *dev, int val);
static void madgemc_setint(struct net_device *dev, int val);

static void madgemc_interrupt(int irq, void *dev_id, struct pt_regs *regs);

/*
 * These work around paging, however they dont guarentee you're on the
 * right page.
 */
#define SIFREADB(reg) (inb(dev->base_addr + ((reg<0x8)?reg:reg-0x8)))
#define SIFWRITEB(val, reg) (outb(val, dev->base_addr + ((reg<0x8)?reg:reg-0x8)))
#define SIFREADW(reg) (inw(dev->base_addr + ((reg<0x8)?reg:reg-0x8)))
#define SIFWRITEW(val, reg) (outw(val, dev->base_addr + ((reg<0x8)?reg:reg-0x8)))

/*
 * Read a byte-length value from the register.
 */
static unsigned short madgemc_sifreadb(struct net_device *dev, unsigned short reg)
{
	unsigned short ret;
	if (reg<0x8)	
		ret = SIFREADB(reg);
	else {
		madgemc_setregpage(dev, 1);	
		ret = SIFREADB(reg);
		madgemc_setregpage(dev, 0);
	}
	return ret;
}

/*
 * Write a byte-length value to a register.
 */
static void madgemc_sifwriteb(struct net_device *dev, unsigned short val, unsigned short reg)
{
	if (reg<0x8)
		SIFWRITEB(val, reg);
	else {
		madgemc_setregpage(dev, 1);
		SIFWRITEB(val, reg);
		madgemc_setregpage(dev, 0);
	}
	return;
}

/*
 * Read a word-length value from a register
 */
static unsigned short madgemc_sifreadw(struct net_device *dev, unsigned short reg)
{
	unsigned short ret;
	if (reg<0x8)	
		ret = SIFREADW(reg);
	else {
		madgemc_setregpage(dev, 1);	
		ret = SIFREADW(reg);
		madgemc_setregpage(dev, 0);
	}
	return ret;
}

/*
 * Write a word-length value to a register.
 */
static void madgemc_sifwritew(struct net_device *dev, unsigned short val, unsigned short reg)
{
	if (reg<0x8)
		SIFWRITEW(val, reg);
	else {
		madgemc_setregpage(dev, 1);
		SIFWRITEW(val, reg);
		madgemc_setregpage(dev, 0);
	}
	return;
}



int __init madgemc_probe(void)
{	
	static int versionprinted;
	struct net_device *dev;
	struct net_local *tp;
	struct madgemc_card *card;
	int i,slot = 0;
	__u8 posreg[4];

	if (!MCA_bus)
		return -1;	
 
	while (slot != MCA_NOTFOUND) {
		/*
		 * Currently we only support the MC16/32 (MCA ID 002d)
		 */
		slot = mca_find_unused_adapter(0x002d, slot);
		if (slot == MCA_NOTFOUND)
			break;

		/*
		 * If we get here, we have an adapter.
		 */
		if (versionprinted++ == 0)
			printk("%s", version);

		if ((dev = init_trdev(NULL, 0))==NULL) {
			printk("madgemc: unable to allocate dev space\n");
			if (madgemc_card_list)
				return 0;
			return -1;
		}
		SET_MODULE_OWNER(dev);
		dev->dma = 0;

		/*
		 * Fetch MCA config registers
		 */
		for(i=0;i<4;i++)
			posreg[i] = mca_read_stored_pos(slot, i+2);
		
		card = kmalloc(sizeof(struct madgemc_card), GFP_KERNEL);
		if (card==NULL) {
			printk("madgemc: unable to allocate card struct\n");
			kfree(dev); /* release_trdev? */
			if (madgemc_card_list)
				return 0;
			return -1;
		}
		card->dev = dev;

		/*
		 * Parse configuration information.  This all comes
		 * directly from the publicly available @002d.ADF.
		 * Get it from Madge or your local ADF library.
		 */

		/*
		 * Base address 
		 */
		dev->base_addr = 0x0a20 + 
			((posreg[2] & MC16_POS2_ADDR2)?0x0400:0) +
			((posreg[0] & MC16_POS0_ADDR1)?0x1000:0) +
			((posreg[3] & MC16_POS3_ADDR3)?0x2000:0);

		/*
		 * Interrupt line
		 */
		switch(posreg[0] >> 6) { /* upper two bits */
		case 0x1: dev->irq = 3; break;
		case 0x2: dev->irq = 9; break; /* IRQ 2 = IRQ 9 */
		case 0x3: dev->irq = 10; break;
		default: dev->irq = 0; break;
		}

		if (dev->irq == 0) {
			printk("%s: invalid IRQ\n", dev->name);
			goto getout1;
		}

		if (!request_region(dev->base_addr, MADGEMC_IO_EXTENT, 
				   "madgemc")) {
			printk(KERN_INFO "madgemc: unable to setup Smart MC in slot %d because of I/O base conflict at 0x%04lx\n", slot, dev->base_addr);
			dev->base_addr += MADGEMC_SIF_OFFSET;
			goto getout1;
		}
		dev->base_addr += MADGEMC_SIF_OFFSET;
		
		/*
		 * Arbitration Level
		 */
		card->arblevel = ((posreg[0] >> 1) & 0x7) + 8;

		/*
		 * Burst mode and Fairness
		 */
		card->burstmode = ((posreg[2] >> 6) & 0x3);
		card->fairness = ((posreg[2] >> 4) & 0x1);

		/*
		 * Ring Speed
		 */
		if ((posreg[1] >> 2)&0x1)
			card->ringspeed = 2; /* not selected */
		else if ((posreg[2] >> 5) & 0x1)
			card->ringspeed = 1; /* 16Mb */
		else
			card->ringspeed = 0; /* 4Mb */

		/* 
		 * Cable type
		 */
		if ((posreg[1] >> 6)&0x1)
			card->cabletype = 1; /* STP/DB9 */
		else
			card->cabletype = 0; /* UTP/RJ-45 */


		/* 
		 * ROM Info. This requires us to actually twiddle
		 * bits on the card, so we must ensure above that 
		 * the base address is free of conflict (request_region above).
		 */
		madgemc_read_rom(card);
		
		if (card->manid != 0x4d) { /* something went wrong */
			printk(KERN_INFO "%s: Madge MC ROM read failed (unknown manufacturer ID %02x)\n", dev->name, card->manid);
			goto getout;
		}
		
		if ((card->cardtype != 0x08) && (card->cardtype != 0x0d)) {
			printk(KERN_INFO "%s: Madge MC ROM read failed (unknown card ID %02x)\n", dev->name, card->cardtype);
			goto getout;
		}
	       
		/* All cards except Rev 0 and 1 MC16's have 256kb of RAM */
		if ((card->cardtype == 0x08) && (card->cardrev <= 0x01))
			card->ramsize = 128;
		else
			card->ramsize = 256;

		printk("%s: %s Rev %d at 0x%04lx IRQ %d\n", 
		       dev->name, 
		       (card->cardtype == 0x08)?MADGEMC16_CARDNAME:
		       MADGEMC32_CARDNAME, card->cardrev, 
		       dev->base_addr, dev->irq);

		if (card->cardtype == 0x0d)
			printk("%s:     Warning: MC32 support is experimental and highly untested\n", dev->name);
		
		if (card->ringspeed==2) { /* Unknown */
			printk("%s:     Warning: Ring speed not set in POS -- Please run the reference disk and set it!\n", dev->name);
			card->ringspeed = 1; /* default to 16mb */
		}
		
		printk("%s:     RAM Size: %dKB\n", dev->name, card->ramsize);

		printk("%s:     Ring Speed: %dMb/sec on %s\n", dev->name, 
		       (card->ringspeed)?16:4, 
		       card->cabletype?"STP/DB9":"UTP/RJ-45");
		printk("%s:     Arbitration Level: %d\n", dev->name, 
		       card->arblevel);

		printk("%s:     Burst Mode: ", dev->name);
		switch(card->burstmode) {
		case 0: printk("Cycle steal"); break;
		case 1: printk("Limited burst"); break;
		case 2: printk("Delayed release"); break;
		case 3: printk("Immediate release"); break;
		}
		printk(" (%s)\n", (card->fairness)?"Unfair":"Fair");


		/* 
		 * Enable SIF before we assign the interrupt handler,
		 * just in case we get spurious interrupts that need
		 * handling.
		 */ 
		outb(0, dev->base_addr + MC_CONTROL_REG0); /* sanity */
		madgemc_setsifsel(dev, 1);
		if(request_irq(dev->irq, madgemc_interrupt, SA_SHIRQ,
			       "madgemc", dev)) 
			goto getout;
		
		madgemc_chipset_init(dev); /* enables interrupts! */
		madgemc_setcabletype(dev, card->cabletype);

		/* Setup MCA structures */
		mca_set_adapter_name(slot, (card->cardtype == 0x08)?MADGEMC16_CARDNAME:MADGEMC32_CARDNAME);
		mca_set_adapter_procfn(slot, madgemc_mcaproc, dev);
		mca_mark_as_used(slot);

		printk("%s:     Ring Station Address: ", dev->name);
		printk("%2.2x", dev->dev_addr[0]);
		for (i = 1; i < 6; i++)
			printk(":%2.2x", dev->dev_addr[i]);
		printk("\n");

		/* XXX is ISA_MAX_ADDRESS correct here? */
		if (tmsdev_init(dev, ISA_MAX_ADDRESS, NULL)) {
			printk("%s: unable to get memory for dev->priv.\n", 
			       dev->name);
			release_region(dev->base_addr-MADGEMC_SIF_OFFSET, 
			       MADGEMC_IO_EXTENT); 
			
			kfree(card);
			tmsdev_term(dev);
			kfree(dev);
			if (madgemc_card_list)
				return 0;
			return -1;
		}
		tp = (struct net_local *)dev->priv;

		/* 
		 * The MC16 is physically a 32bit card.  However, Madge
		 * insists on calling it 16bit, so I'll assume here that
		 * they know what they're talking about.  Cut off DMA
		 * at 16mb.
		 */
		tp->setnselout = madgemc_setnselout_pins;
		tp->sifwriteb = madgemc_sifwriteb;
		tp->sifreadb = madgemc_sifreadb;
		tp->sifwritew = madgemc_sifwritew;
		tp->sifreadw = madgemc_sifreadw;
		tp->DataRate = (card->ringspeed)?SPEED_16:SPEED_4;

		memcpy(tp->ProductID, "Madge MCA 16/4    ", PROD_ID_SIZE + 1);

		dev->open = madgemc_open;
		dev->stop = madgemc_close;
		
		if (register_trdev(dev) == 0) {
			/* Enlist in the card list */
			card->next = madgemc_card_list;
			madgemc_card_list = card;
		} else {
			printk("madgemc: register_trdev() returned non-zero.\n");
			release_region(dev->base_addr-MADGEMC_SIF_OFFSET, 
			       MADGEMC_IO_EXTENT); 
			
			kfree(card);
			tmsdev_term(dev);
			kfree(dev);
			if (madgemc_card_list)
				return 0;
			return -1;
		}

		slot++;
		continue; /* successful, try to find another */
		
	getout:
		release_region(dev->base_addr-MADGEMC_SIF_OFFSET, 
			       MADGEMC_IO_EXTENT); 
	getout1:
		kfree(card);
		kfree(dev); /* release_trdev? */
		slot++;
	}

	if (madgemc_card_list)
		return 0;
	return -1;
}

/*
 * Handle interrupts generated by the card
 *
 * The MicroChannel Madge cards need slightly more handling
 * after an interrupt than other TMS380 cards do.
 *
 * First we must make sure it was this card that generated the
 * interrupt (since interrupt sharing is allowed).  Then,
 * because we're using level-triggered interrupts (as is
 * standard on MCA), we must toggle the interrupt line
 * on the card in order to claim and acknowledge the interrupt.
 * Once that is done, the interrupt should be handlable in
 * the normal tms380tr_interrupt() routine.
 *
 * There's two ways we can check to see if the interrupt is ours,
 * both with their own disadvantages...
 *
 * 1)  	Read in the SIFSTS register from the TMS controller.  This
 *	is guarenteed to be accurate, however, there's a fairly
 *	large performance penalty for doing so: the Madge chips
 *	must request the register from the Eagle, the Eagle must
 *	read them from its internal bus, and then take the route
 *	back out again, for a 16bit read.  
 *
 * 2)	Use the MC_CONTROL_REG0_SINTR bit from the Madge ASICs.
 *	The major disadvantage here is that the accuracy of the
 *	bit is in question.  However, it cuts out the extra read
 *	cycles it takes to read the Eagle's SIF, as its only an
 *	8bit read, and theoretically the Madge bit is directly
 *	connected to the interrupt latch coming out of the Eagle
 *	hardware (that statement is not verified).  
 *
 * I can't determine which of these methods has the best win.  For now,
 * we make a compromise.  Use the Madge way for the first interrupt,
 * which should be the fast-path, and then once we hit the first 
 * interrupt, keep on trying using the SIF method until we've
 * exhausted all contiguous interrupts.
 *
 */
static void madgemc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int pending,reg1;
	struct net_device *dev;

	if (!dev_id) {
		printk("madgemc_interrupt: was not passed a dev_id!\n");
		return;
	}

	dev = (struct net_device *)dev_id;

	/* Make sure its really us. -- the Madge way */
	pending = inb(dev->base_addr + MC_CONTROL_REG0);
	if (!(pending & MC_CONTROL_REG0_SINTR))
		return; /* not our interrupt */

	/*
	 * Since we're level-triggered, we may miss the rising edge
	 * of the next interrupt while we're off handling this one,
	 * so keep checking until the SIF verifies that it has nothing
	 * left for us to do.
	 */
	pending = STS_SYSTEM_IRQ;
	do {
		if (pending & STS_SYSTEM_IRQ) {

			/* Toggle the interrupt to reset the latch on card */
			reg1 = inb(dev->base_addr + MC_CONTROL_REG1);
			outb(reg1 ^ MC_CONTROL_REG1_SINTEN, 
			     dev->base_addr + MC_CONTROL_REG1);
			outb(reg1, dev->base_addr + MC_CONTROL_REG1);

			/* Continue handling as normal */
			tms380tr_interrupt(irq, dev_id, regs);

			pending = SIFREADW(SIFSTS); /* restart - the SIF way */

		} else
			return; 
	} while (1);

	return; /* not reachable */
}

/*
 * Set the card to the prefered ring speed.
 *
 * Unlike newer cards, the MC16/32 have their speed selection
 * circuit connected to the Madge ASICs and not to the TMS380
 * NSELOUT pins. Set the ASIC bits correctly here, and return 
 * zero to leave the TMS NSELOUT bits unaffected.
 *
 */
unsigned short madgemc_setnselout_pins(struct net_device *dev)
{
	unsigned char reg1;
	struct net_local *tp = (struct net_local *)dev->priv;
	
	reg1 = inb(dev->base_addr + MC_CONTROL_REG1);

	if(tp->DataRate == SPEED_16)
		reg1 |= MC_CONTROL_REG1_SPEED_SEL; /* add for 16mb */
	else if (reg1 & MC_CONTROL_REG1_SPEED_SEL)
		reg1 ^= MC_CONTROL_REG1_SPEED_SEL; /* remove for 4mb */
	outb(reg1, dev->base_addr + MC_CONTROL_REG1);

	return 0; /* no change */
}

/*
 * Set the register page.  This equates to the SRSX line
 * on the TMS380Cx6.
 *
 * Register selection is normally done via three contiguous
 * bits.  However, some boards (such as the MC16/32) use only
 * two bits, plus a separate bit in the glue chip.  This
 * sets the SRSX bit (the top bit).  See page 4-17 in the
 * Yellow Book for which registers are affected.
 *
 */
static void madgemc_setregpage(struct net_device *dev, int page)
{	
	static int reg1;

	reg1 = inb(dev->base_addr + MC_CONTROL_REG1);
	if ((page == 0) && (reg1 & MC_CONTROL_REG1_SRSX)) {
		outb(reg1 ^ MC_CONTROL_REG1_SRSX, 
		     dev->base_addr + MC_CONTROL_REG1);
	}
	else if (page == 1) {
		outb(reg1 | MC_CONTROL_REG1_SRSX, 
		     dev->base_addr + MC_CONTROL_REG1);
	}
	reg1 = inb(dev->base_addr + MC_CONTROL_REG1);

	return;
}

/*
 * The SIF registers are not mapped into register space by default
 * Set this to 1 to map them, 0 to map the BIA ROM.
 *
 */
static void madgemc_setsifsel(struct net_device *dev, int val)
{
	unsigned int reg0;

	reg0 = inb(dev->base_addr + MC_CONTROL_REG0);
	if ((val == 0) && (reg0 & MC_CONTROL_REG0_SIFSEL)) {
		outb(reg0 ^ MC_CONTROL_REG0_SIFSEL, 
		     dev->base_addr + MC_CONTROL_REG0);
	} else if (val == 1) {
		outb(reg0 | MC_CONTROL_REG0_SIFSEL, 
		     dev->base_addr + MC_CONTROL_REG0);
	}	
	reg0 = inb(dev->base_addr + MC_CONTROL_REG0);

	return;
}

/*
 * Enable SIF interrupts
 *
 * This does not enable interrupts in the SIF, but rather
 * enables SIF interrupts to be passed onto the host.
 *
 */
static void madgemc_setint(struct net_device *dev, int val)
{
	unsigned int reg1;

	reg1 = inb(dev->base_addr + MC_CONTROL_REG1);
	if ((val == 0) && (reg1 & MC_CONTROL_REG1_SINTEN)) {
		outb(reg1 ^ MC_CONTROL_REG1_SINTEN, 
		     dev->base_addr + MC_CONTROL_REG1);
	} else if (val == 1) {
		outb(reg1 | MC_CONTROL_REG1_SINTEN, 
		     dev->base_addr + MC_CONTROL_REG1);
	}

	return;
}

/*
 * Cable type is set via control register 7. Bit zero high
 * for UTP, low for STP.
 */
static void madgemc_setcabletype(struct net_device *dev, int type)
{
	outb((type==0)?MC_CONTROL_REG7_CABLEUTP:MC_CONTROL_REG7_CABLESTP,
	     dev->base_addr + MC_CONTROL_REG7);
}

/*
 * Enable the functions of the Madge chipset needed for
 * full working order. 
 */
static int madgemc_chipset_init(struct net_device *dev)
{
	outb(0, dev->base_addr + MC_CONTROL_REG1); /* pull SRESET low */
	tms380tr_wait(100); /* wait for card to reset */

	/* bring back into normal operating mode */
	outb(MC_CONTROL_REG1_NSRESET, dev->base_addr + MC_CONTROL_REG1);

	/* map SIF registers */
	madgemc_setsifsel(dev, 1);

	/* enable SIF interrupts */
	madgemc_setint(dev, 1); 

	return 0;
}

/*
 * Disable the board, and put back into power-up state.
 */
void madgemc_chipset_close(struct net_device *dev)
{
	/* disable interrupts */
	madgemc_setint(dev, 0);
	/* unmap SIF registers */
	madgemc_setsifsel(dev, 0);

	return;
}

/*
 * Read the card type (MC16 or MC32) from the card.
 *
 * The configuration registers are stored in two separate
 * pages.  Pages are flipped by clearing bit 3 of CONTROL_REG0 (PAGE)
 * for page zero, or setting bit 3 for page one.
 *
 * Page zero contains the following data:
 *	Byte 0: Manufacturer ID (0x4D -- ASCII "M")
 *	Byte 1: Card type:
 *			0x08 for MC16
 *			0x0D for MC32
 *	Byte 2: Card revision
 *	Byte 3: Mirror of POS config register 0
 *	Byte 4: Mirror of POS 1
 *	Byte 5: Mirror of POS 2
 *
 * Page one contains the following data:
 *	Byte 0: Unused
 *	Byte 1-6: BIA, MSB to LSB.
 *
 * Note that to read the BIA, we must unmap the SIF registers
 * by clearing bit 2 of CONTROL_REG0 (SIFSEL), as the data
 * will reside in the same logical location.  For this reason,
 * _never_ read the BIA while the Eagle processor is running!
 * The SIF will be completely inaccessible until the BIA operation
 * is complete.
 *
 */
static void madgemc_read_rom(struct madgemc_card *card)
{
	unsigned long ioaddr;
	unsigned char reg0, reg1, tmpreg0, i;

	ioaddr = card->dev->base_addr;

	reg0 = inb(ioaddr + MC_CONTROL_REG0);
	reg1 = inb(ioaddr + MC_CONTROL_REG1);

	/* Switch to page zero and unmap SIF */
	tmpreg0 = reg0 & ~(MC_CONTROL_REG0_PAGE + MC_CONTROL_REG0_SIFSEL);
	outb(tmpreg0, ioaddr + MC_CONTROL_REG0);
	
	card->manid = inb(ioaddr + MC_ROM_MANUFACTURERID);
	card->cardtype = inb(ioaddr + MC_ROM_ADAPTERID);
	card->cardrev = inb(ioaddr + MC_ROM_REVISION);

	/* Switch to rom page one */
	outb(tmpreg0 | MC_CONTROL_REG0_PAGE, ioaddr + MC_CONTROL_REG0);

	/* Read BIA */
	card->dev->addr_len = 6;
	for (i = 0; i < 6; i++)
		card->dev->dev_addr[i] = inb(ioaddr + MC_ROM_BIA_START + i);
	
	/* Restore original register values */
	outb(reg0, ioaddr + MC_CONTROL_REG0);
	outb(reg1, ioaddr + MC_CONTROL_REG1);
	
	return;
}

static int madgemc_open(struct net_device *dev)
{  
	/*
	 * Go ahead and reinitialize the chipset again, just to 
	 * make sure we didn't get left in a bad state.
	 */
	madgemc_chipset_init(dev);
	tms380tr_open(dev);
	return 0;
}

static int madgemc_close(struct net_device *dev)
{
	tms380tr_close(dev);
	madgemc_chipset_close(dev);
	return 0;
}

/*
 * Give some details available from /proc/mca/slotX
 */
static int madgemc_mcaproc(char *buf, int slot, void *d) 
{	
	struct net_device *dev = (struct net_device *)d;
	struct madgemc_card *curcard = madgemc_card_list;
	int len = 0;
	
	while (curcard) { /* search for card struct */
		if (curcard->dev == dev)
			break;
		curcard = curcard->next;
	}
	len += sprintf(buf+len, "-------\n");
	if (curcard) {
		struct net_local *tp = (struct net_local *)dev->priv;
		int i;
		
		len += sprintf(buf+len, "Card Revision: %d\n", curcard->cardrev);
		len += sprintf(buf+len, "RAM Size: %dkb\n", curcard->ramsize);
		len += sprintf(buf+len, "Cable type: %s\n", (curcard->cabletype)?"STP/DB9":"UTP/RJ-45");
		len += sprintf(buf+len, "Configured ring speed: %dMb/sec\n", (curcard->ringspeed)?16:4);
		len += sprintf(buf+len, "Running ring speed: %dMb/sec\n", (tp->DataRate==SPEED_16)?16:4);
		len += sprintf(buf+len, "Device: %s\n", dev->name);
		len += sprintf(buf+len, "IO Port: 0x%04lx\n", dev->base_addr);
		len += sprintf(buf+len, "IRQ: %d\n", dev->irq);
		len += sprintf(buf+len, "Arbitration Level: %d\n", curcard->arblevel);
		len += sprintf(buf+len, "Burst Mode: ");
		switch(curcard->burstmode) {
		case 0: len += sprintf(buf+len, "Cycle steal"); break;
		case 1: len += sprintf(buf+len, "Limited burst"); break;
		case 2: len += sprintf(buf+len, "Delayed release"); break;
		case 3: len += sprintf(buf+len, "Immediate release"); break;
		}
		len += sprintf(buf+len, " (%s)\n", (curcard->fairness)?"Unfair":"Fair");
		
		len += sprintf(buf+len, "Ring Station Address: ");
		len += sprintf(buf+len, "%2.2x", dev->dev_addr[0]);
		for (i = 1; i < 6; i++)
			len += sprintf(buf+len, " %2.2x", dev->dev_addr[i]);
		len += sprintf(buf+len, "\n");
	} else 
		len += sprintf(buf+len, "Card not configured\n");

	return len;
}

#ifdef MODULE

int init_module(void)
{
	/* Probe for cards. */
	if (madgemc_probe()) {
		printk(KERN_NOTICE "madgemc.c: No cards found.\n");
	}
	/* lock_tms380_module(); */
	return (0);
}

void cleanup_module(void)
{
	struct net_device *dev;
	struct madgemc_card *this_card;
	
	while (madgemc_card_list) {
		dev = madgemc_card_list->dev;
		unregister_trdev(dev);
		release_region(dev->base_addr-MADGEMC_SIF_OFFSET, MADGEMC_IO_EXTENT);
		free_irq(dev->irq, dev);
		tmsdev_term(dev);
		kfree(dev);
		this_card = madgemc_card_list;
		madgemc_card_list = this_card->next;
		kfree(this_card);
	}
	/* unlock_tms380_module(); */
}
#endif /* MODULE */

MODULE_LICENSE("GPL");


/*
 * Local variables:
 *  compile-command: "gcc -DMODVERSIONS  -DMODULE -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer -I/usr/src/linux/drivers/net/tokenring/ -c madgemc.c"
 *  alt-compile-command: "gcc -DMODULE -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer -I/usr/src/linux/drivers/net/tokenring/ -c madgemc.c"
 *  c-set-style "K&R"
 *  c-indent-level: 8
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */

