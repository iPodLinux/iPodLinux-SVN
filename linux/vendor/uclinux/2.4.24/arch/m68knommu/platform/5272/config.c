/***************************************************************************/

/*
 *	linux/arch/m68knommu/platform/5272/config.c
 *
 *	Copyright (C) 1999-2003, Greg Ungerer (gerg@snapgear.com)
 *	Copyright (C) 2001-2003, SnapGear Inc. (www.snapgear.com)
 *	Copyright (C) 2002,      Arcturus Networks Inc. (www.ArcturusNetworks.com)
 *
 *      Support for Arcturus Networks' uC5272 dimm by
 *        Michael Leslie <mleslie@arcturusnetworks.com>
 */

/***************************************************************************/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <linux/init.h>
#include <linux/ledman.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcftimer.h>
#include <asm/mcfsim.h>
#include <asm/mcfdma.h>
#include <asm/delay.h>

/***************************************************************************/

#if defined (CONFIG_UCBOOTSTRAP)
#include <asm/uCbootstrap.h>

int    ConTTY = 0;
static int errno;
_bsc0(char *, getserialnum)
_bsc1(unsigned char *, gethwaddr, int, a)
_bsc1(char *, getbenv, char *, a)
#endif

#if defined (CONFIG_BOARD_UC5272)
unsigned char cs8900a_hwaddr[6];
unsigned char fec_hwaddr[6];
unsigned char *sn;
#endif

/***************************************************************************/

void	reset_setupbutton(void);

/***************************************************************************/

/*
 *	DMA channel base address table.
 */
unsigned int   dma_base_addr[MAX_DMA_CHANNELS] = {
	MCF_MBAR + MCFDMA_BASE0,
};

unsigned int dma_device_address[MAX_DMA_CHANNELS];

/***************************************************************************/

void coldfire_tick(void)
{
	volatile unsigned char	*timerp;

	/* Reset the ColdFire timer */
	timerp = (volatile unsigned char *) (MCF_MBAR + MCFTIMER_BASE4);
	timerp[MCFTIMER_TER] = MCFTIMER_TER_CAP | MCFTIMER_TER_REF;
}

/***************************************************************************/

void coldfire_timer_init(void (*handler)(int, void *, struct pt_regs *))
{
	volatile unsigned short	*timerp;
	volatile unsigned long	*icrp;

	/* Set up TIMER 4 as poll clock */
	timerp = (volatile unsigned short *) (MCF_MBAR + MCFTIMER_BASE4);
	timerp[MCFTIMER_TMR] = MCFTIMER_TMR_DISABLE;

	timerp[MCFTIMER_TRR] = (unsigned short) ((MCF_CLK / 16) / HZ);
	timerp[MCFTIMER_TMR] = MCFTIMER_TMR_ENORI | MCFTIMER_TMR_CLK16 |
		MCFTIMER_TMR_RESTART | MCFTIMER_TMR_ENABLE;

	icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR1);
	*icrp = 0x0000000d; /* TMR4 with priority 5 */
	request_irq(72, handler, SA_INTERRUPT, "ColdFire Timer", NULL);

#if defined(CONFIG_RESETSWITCH) && !defined(CONFIG_BOARD_UC5272)
	/* This is not really the right place to do this... */
	reset_setupbutton();
#endif
}

/***************************************************************************/

unsigned long coldfire_timer_offset(void)
{
	volatile unsigned short *timerp;
	volatile unsigned long  *icrp;
	unsigned long		trr, tcn, offset;

	timerp = (volatile unsigned short *) (MCF_MBAR + MCFTIMER_BASE4);
	icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR1);

	tcn = timerp[MCFTIMER_TCN];
	trr = timerp[MCFTIMER_TRR];

	/*
	 * If we are still in the first half of the upcount and a
	 * timer interupt is pending, then add on a ticks worth of time.
	 */
	offset = ((tcn * (1000000 / HZ)) / trr);
	if ((offset < (1000000 / HZ / 2)) && ((*icrp & 0x00000008) != 0))
		offset += 1000000 / HZ;
	return offset;	
}

/***************************************************************************/

/*
 *	Program the vector to be an auto-vectored.
 */

void mcf_autovector(unsigned int vec)
{
	/* Everything is auto-vectored on the 5272 */
}

/***************************************************************************/

extern e_vector	*_ramvec;

void set_evector(int vecnum, void (*handler)(void))
{
	if (vecnum >= 0 && vecnum <= 255)
		_ramvec[vecnum] = handler;
}

/***************************************************************************/

/* assembler routines */
asmlinkage void buserr(void);
asmlinkage void trap(void);
asmlinkage void system_call(void);
asmlinkage void inthandler(void);

void coldfire_trap_init(void)
{
	int i;

#ifndef ENABLE_dBUG
	volatile unsigned long	*icrp;

	icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR1);
	icrp[0] = 0x88888888;
	icrp[1] = 0x88888888;
	icrp[2] = 0x88888888;
	icrp[3] = 0x88888888;
#endif

	/*
	 *	There is a common trap handler and common interrupt
	 *	handler that handle almost every vector. We treat
	 *	the system call and bus error special, they get their
	 *	own first level handlers.
	 */
#ifndef ENABLE_dBUG
	for (i = 3; (i <= 23); i++)
		_ramvec[i] = trap;

#if !defined(CONFIG_UCBOOTSTRAP)
        for (i = 33; (i <= 63); i++)
                _ramvec[i] = trap;
#else /* defined(CONFIG_UCBOOTSTRAP) */
        _ramvec[33] = trap;
        /* skip trap #2; this is used as the uCbootloader callback */
        for (i = 35; (i <= 63); i++)
                _ramvec[i] = trap;
#endif

#endif

	for (i = 24; (i <= 30); i++)
		_ramvec[i] = inthandler;
#ifndef ENABLE_dBUG
	_ramvec[31] = inthandler;  // Disables the IRQ7 button
#endif

	for (i = 64; (i < 255); i++)
		_ramvec[i] = inthandler;
	_ramvec[255] = 0;

	_ramvec[2] = buserr;
	_ramvec[32] = system_call;
}

/***************************************************************************/

/*
 *	Generic dumping code. Used for panic and debug.
 */

void dump(struct pt_regs *fp)
{
	extern unsigned int sw_usp, sw_ksp;
	unsigned long	*sp;
	unsigned char	*tp;
	int		i;

	printk("\nCURRENT PROCESS:\n\n");
	printk("COMM=%s PID=%d\n", current->comm, current->pid);

	if (current->mm) {
		printk("TEXT=%08x-%08x DATA=%08x-%08x BSS=%08x-%08x\n",
			(int) current->mm->start_code,
			(int) current->mm->end_code,
			(int) current->mm->start_data,
			(int) current->mm->end_data,
			(int) current->mm->end_data,
			(int) current->mm->brk);
		printk("USER-STACK=%08x  KERNEL-STACK=%08x\n\n",
			(int) current->mm->start_stack,
			(int) ((unsigned long) current) + KTHREAD_SIZE);
	}

	printk("PC: %08lx\n", fp->pc);
	printk("SR: %08lx    SP: %08lx\n", (long) fp->sr, (long) fp);
	printk("d0: %08lx    d1: %08lx    d2: %08lx    d3: %08lx\n",
		fp->d0, fp->d1, fp->d2, fp->d3);
	printk("d4: %08lx    d5: %08lx    a0: %08lx    a1: %08lx\n",
		fp->d4, fp->d5, fp->a0, fp->a1);
	printk("\nUSP: %08x   KSP: %08x   TRAPFRAME: %08x\n",
		sw_usp, sw_ksp, (unsigned int) fp);

	printk("\nCODE:");
	tp = ((unsigned char *) fp->pc) - 0x20;
	for (sp = (unsigned long *) tp, i = 0; (i < 0x40);  i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");

	printk("\nKERNEL STACK:");
	tp = ((unsigned char *) fp) - 0x40;
	for (sp = (unsigned long *) tp, i = 0; (i < 0xc0); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");
	printk("\n");

	printk("\nUSER STACK:");
	tp = (unsigned char *) (sw_usp - 0x10);
	for (sp = (unsigned long *) tp, i = 0; (i < 0x80); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n\n");
}

/***************************************************************************/

void coldfire_reset(void)
{
	HARD_RESET_NOW();
}

/***************************************************************************/

#ifdef CONFIG_PPCBOOT

#define PPCBOOT_ENV_START 0xffe04000
#define PPCBOOT_ENV_END   0xffe06000

char *ppcboot_getenv(char* v)
{
	char* p = (char*)PPCBOOT_ENV_START+4;
	while (1) {
		char* e = p;
		while ((*p != '=') && (*p != 0) && (p < (char*)PPCBOOT_ENV_END)) p++;
			if (*p == '=') {
				if (strncmp(v,e,p-e) == 0)
					return p+1;
				else
					while ((*p++ != 0) && (p < (char*)PPCBOOT_ENV_END))
						;
			} else {
				return 0;
			}
	};
};

#endif

/***************************************************************************/

void config_BSP(char *commandp, int size)
{
	unsigned char *p;

#if 0
	volatile unsigned long	*pivrp;

	/* Set base of device vectors to be 64 */
	pivrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_PIVR);
	*pivrp = 0x40;
#endif

#ifdef CONFIG_BOOTPARAM
	strncpy(commandp, CONFIG_BOOTPARAM_STRING, size);
	commandp[size-1] = 0;
#elif defined(CONFIG_NETtel) || defined(CONFIG_eLIA) || \
    defined(CONFIG_DISKtel) || defined(CONFIG_SECUREEDGEMP3) || \
    defined(CONFIG_GILBARCONAP) || defined(CONFIG_SE1100) || \
	 defined(CONFIG_SCALES)
	/* Copy command line from FLASH to local buffer... */
	memcpy(commandp, (char *) 0xf0004000, size);
	commandp[size-1] = 0;
#elif defined(CONFIG_MTD_KeyTechnology)
	/* Copy command line from FLASH to local buffer... */
	memcpy(commandp, (char *) 0xffe06000, size);
	commandp[size-1] = 0;
#elif defined(CONFIG_CANCam)
	/* Copy command line from FLASH to local buffer... */
	memcpy(commandp, (char *) 0xf0010000, size);
	commandp[size-1] = 0;
#elif defined(CONFIG_PPCBOOT)
	do {
		char *bootargs = ppcboot_getenv("bootargs");
		if (bootargs)
			strncpy(commandp, bootargs, size);
	} while(0);
#elif defined (CONFIG_BOARD_UC5272)
	/* printk("\nColdfire uC5272 uCdimm support (c) 2002 Arcturus Networks Inc.\n"); */
#if defined(CONFIG_UC5272_PHY_INT)
	{
		/** ver 1.3 uCdimm and above have PHY interrupt on INT6 **/
		/* set GPIO to be INT6 instead - b30-31 => 01 */
		volatile unsigned int	*temp_ptr;

		/* Reset the ColdFire timer */
		temp_ptr = (volatile unsigned int *) (MCF_MBAR + MCFSIM_PACNT);
		*temp_ptr &= 0x3fffffff;
		*temp_ptr |= 0x40000000;
	}
#endif /* CONFIG_UC5272_PHY_INT */
 
#if defined(CONFIG_UCBOOTSTRAP)
	printk("uC5272 serial string [%s]\n", getserialnum());

	p = gethwaddr(0);
	memcpy (fec_hwaddr, p, 6);
	if (p == NULL) {
		memcpy (fec_hwaddr, "\0x00\0xde\0xad\0xbe\0xef\0x27", 6);
		p = fec_hwaddr;
		printk ("Warning: HWADDR0 not set in uCbootloader. Using default.\n");
	}
	printk("uCbootstrap eth addr 0 %02x:%02x:%02x:%02x:%02x:%02x\n",
		p[0], p[1], p[2], p[3], p[4], p[5]);

	p = gethwaddr(1);
	memcpy (cs8900a_hwaddr, p, 6);
	if (p == NULL) {
		memcpy (cs8900a_hwaddr, "\0x00\0xde\0xad\0xbe\0xef\0x28", 6);
		p = cs8900a_hwaddr;
		printk ("Warning: HWADDR1 not set in uCbootloader. Using default.\n");
	}
	printk("uCbootstrap eth addr 1 %02x:%02x:%02x:%02x:%02x:%02x\n",
		p[0], p[1], p[2], p[3], p[4], p[5]);

	p = getbenv("CONSOLE");
	if (p != NULL)
		sprintf(commandp, " console=%s", p);

	p = getbenv("APPEND");
	if (p != NULL)
		strcat(commandp, p);
#else
	memset(commandp, 0, size);
	sprintf (commandp, "ip=dhcp console=ttyS0");
#endif
#else
	memset(commandp, 0, size);
#endif

	mach_sched_init = coldfire_timer_init;
	mach_tick = coldfire_tick;
	mach_trap_init = coldfire_trap_init;
	mach_reset = coldfire_reset;
	mach_gettimeoffset = coldfire_timer_offset;

#ifdef CONFIG_DS1302
{
	extern int ds1302_set_clock_mmss(unsigned long);
	extern void ds1302_gettod(int *, int *, int *, int *, int *, int *);
	mach_set_clock_mmss = ds1302_set_clock_mmss;
	mach_gettod = ds1302_gettod;
}
#endif
}

/***************************************************************************/
#ifdef CONFIG_RESETSWITCH
/***************************************************************************/

/*
 *	Routines to support the NETtel software reset button.
 */
void reset_button(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile unsigned long	*icrp, *isrp;
	static int		inbutton = 0;

	/*
	 *	IRQ7 is not maskable by the CPU core. It is possible
	 *	that switch bounce mey get us back here before we have
	 *	really serviced the interrupt.
	 */
	if (inbutton)
		return;
	inbutton = 1;

	/* Disable interrupt at SIM - best we can do... */
	icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR1);
	*icrp = (*icrp & 0x07777777) | 0x80000000;

#ifdef CONFIG_LEDMAN
	ledman_signalreset();
#endif

	/* Don't leave here 'till button is no longer pushed! */
	isrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ISR);
	for (;;) {
		if (*isrp & 0x80000000)
			break;
	}

#ifndef CONFIG_LEDMAN
	HARD_RESET_NOW();
	/* Should never get here... */
#endif

	inbutton = 0;
	/* Interrupt service done, acknowledge it */
	icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR1);
	*icrp = (*icrp & 0x07777777) | 0xf0000000;
}

/***************************************************************************/

void reset_setupbutton(void)
{
	volatile unsigned long	*icrp;

	icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR1);
	*icrp = (*icrp & 0x07777777) | 0xf0000000;
	request_irq(65, reset_button, (SA_INTERRUPT | IRQ_FLG_FAST),
		"Reset Button", NULL);
}

/***************************************************************************/
#endif /* CONFIG_RESETSWITCH */
/***************************************************************************/
#if defined(CONFIG_UCBOOTSTRAP)
/***************************************************************************/

unsigned char *get_MAC_address(char *devname)
{
	char *devnum  = devname + (strlen(devname)-1);
	char *varname = "HWADDR0";
	char *varnum  = varname + (strlen(varname)-1);
	static unsigned char tmphwaddr[6] = {0, 0, 0, 0, 0, 0};
	char *ret;
	int i;

	strcpy ( varnum, devnum );

	ret = getbenv(varname);

	memset(tmphwaddr, 0, 6);
	if (ret && strlen(ret) == 17) {
		for (i=0; i<6; i++) {
			ret[(i+1) * 3] = 0;
			tmphwaddr[i] = simple_strtoul(ret + i*3, 0, 16);
		}
	}
	return(tmphwaddr);
}

/***************************************************************************/
#endif /* CONFIG_UCBOOTSTRAP */
/***************************************************************************/
