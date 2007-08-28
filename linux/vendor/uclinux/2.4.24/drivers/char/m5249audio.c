/****************************************************************************/

/*
 *	m5249audio.c -- audio driver for M5249 ColdFire internal audio.
 *
 *	(C) Copyright 2002, Greg Ungerer (gerg@snapgear.com)
 */

/****************************************************************************/

#include <linux/module.h>
#include <linux/config.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/major.h>
#include <linux/delay.h>
#include <linux/soundcard.h>
#include <asm/uaccess.h>
#include <asm/param.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/mcfdma.h>
#include <asm/m5249audio.h>

/****************************************************************************/

/*
 *	Hardware configuration.
 */
#define	M5249AUDIO_TXIRQ	131	/* Audio TX FIFO IRQ */
#define	M5249AUDIO_DMAIRQ	120	/* Audio TX DMA IRQ */
#define	M5249AUDIO_DMA		0	/* Audio TX DMA channel */

/*
 *	You can choose to use either interrupt or DMA here.
 *	DMA will give you much better CPU utilitization...
 */
//#define CONFIG_AUDIOIRQ	1
#define CONFIG_AUDIODMA	1

/****************************************************************************/

/*
 *	Driver configurable settings.
 */
#define	BUFSIZE		(256*1024)	/* Audio buffer data size */
#define	DMASIZE		56000		/* DMA buffer maximum size */

/*
 *	Driver settings and state.
 */
int		m5249audio_isopen;
int		m5249audio_txbusy;
int		m5249audio_dmaing;
unsigned int	m5249audio_speed;
unsigned int	m5249audio_stereo;
unsigned int	m5249audio_bits;
unsigned int	m5249audio_format;
unsigned long	m5249audio_imrbit;

/*
 *	Circular buffer for audio data. It is broken up into 2 regions.
 *	The first is the current portion of the buffer being DMA'ed right
 *	now. The other is data waiting to be DMA'ed.
 */
unsigned char		*m5249audio_buf;
volatile unsigned int	m5249audio_dmastart;
volatile unsigned int	m5249audio_dmacount;
volatile unsigned int	m5249audio_appstart;
volatile unsigned int	m5249audio_append;

/****************************************************************************/

/*
 *	Quick and easy access to the audio registers. (This optimizes
 *	nicely, since it will almost always be a constant pointer).
 */
#define	reg32p(r)	((volatile unsigned long *) (MCF_MBAR2 + (r)))
#define	reg16w(r)	((volatile unsigned short *) (MCF_MBAR2 + (r)))
#define	reg8p(r)	((volatile unsigned char *) (MCF_MBAR2 + (r)))

/*
 *	Quick and easy access to the SIM registers.
 */
#define	simp(r)	((volatile unsigned char *) (MCF_MBAR + (r)))

/****************************************************************************/

void m5249audio_chipinit(void)
{
#if DEBUG
	printk("m5249audio_chipinit()\n");
#endif

	*reg32p(MCFA_IIS2CONFIG) = MCFA_IIS_CLK8 | MCFA_IIS_TXSRC_PDOR3 |
		MCFA_IIS_16BIT | MCFA_IIS_MODE_IIS | MCFA_IIS_LRCK32BIT;
	*reg32p(MCFA_DATAINCTRL) = 0;
}

/****************************************************************************/

struct m5249audio_format {
	unsigned int	format;
	unsigned int	bits;
} m5249audio_formattable[] = {
	{ AFMT_MU_LAW,		8 },
	{ AFMT_A_LAW,		8 },
	{ AFMT_IMA_ADPCM,	8 },
	{ AFMT_U8,		8 },
	{ AFMT_S16_LE,		16 },
	{ AFMT_S16_BE,		16 },
	{ AFMT_S8,		8 },
	{ AFMT_U16_LE,		16 },
	{ AFMT_U16_BE,		16 },
};

#define	FORMATSIZE	(sizeof(m5249audio_formattable) / sizeof(struct m5249audio_format))


void m5249audio_setsamplesize(int val)
{
	int	i;

	for (i = 0; (i < FORMATSIZE); i++) {
		if (m5249audio_formattable[i].format == val) {
			m5249audio_format = m5249audio_formattable[i].format;
			m5249audio_bits = m5249audio_formattable[i].bits;
			break;
		}
	}
}

/****************************************************************************/

/*
 *	Mute the audio signal output.
 */

void m5249audio_mute(void)
{
	*reg32p(MCFSIM2_GPIO1WRITE) &= ~0x00020000;
}

/****************************************************************************/

/*
 *	Unmute the audio output (let the sound out :-)
 */

void m5249audio_unmute(void)
{
	*reg32p(MCFSIM2_GPIO1WRITE) |= 0x00020000;
}

/****************************************************************************/

/*
 *	I have coded 2 routines for the interrupt driven case. The first
 *	here is all assembler, and is pretty damn quick. The second is
 *	C based, slower, not as good, really just for reference.
 */

#define	CONFIG_AUDIOIRQASM	1

#ifdef CONFIG_AUDIOIRQ
#ifdef CONFIG_AUDIOIRQASM

extern void m5249audio_isr(int irq, void *dev_id, struct pt_regs *regs);

void m5249audio_irqwrapper(void)
{
	__asm__(
"		.global	m5249audio_isr\n"
"		m5249audio_isr:\n"
"			sub.l	#20, %sp\n"
"			movem.l	%d0-%d2/%a0-%a1, %sp@	/* Save registers */\n"
"\n"
"			move.l	m5249audio_buf, %a0	/* Loop setup */\n"
"			move.l	m5249audio_dmastart, %d0\n"
"			move.l	#0x80000074, %a1\n"
"			moveq	#5, %d2\n"
"\n"
"		m5249audio_isr_writefifo:\n"
"			move.l	%a0@(%d0), %d1		/* Get next word */\n"
"			move.l	%d1, %a1@		/* Write word to FIFO */\n"
"			add.l	#4, %d0			/* Update buffer */\n"
"			cmp.l	#(256*1024-4), %d0\n"
"			blt	m5249audio_isr_nowrap\n"
"			clr.l	%d0			/* Wrap buffer */\n"
"		m5249audio_isr_nowrap:\n"
"			cmp.l	m5249audio_append, %d0\n"
"			beq	m5249audio_isr_txdisable\n"
"			sub.l	#1, %d2			/* Loop counter */\n"
"			bne	m5249audio_isr_writefifo\n"
"\n"
"		m5249audio_isr_done:\n"
"			move.l	%d0, m5249audio_dmastart\n"
"			movem.l	%sp@, %d0-%d2/%a0-%a1	/* Restore registers */\n"
"			add.l	#20, %sp\n"
"			rte\n"
"\n"
"		m5249audio_isr_txdisable:\n"
"			move.l	#0x80000094, %a0	/* Disable interrupt */\n"
"			move.l	%a0@, %d1\n"
"			and.l	#0xfffffff7, %d1\n"
"			move.l	%d1, %a0@\n"
"			clr.l	m5249audio_txbusy\n"
"			bra	m5249audio_isr_done\n"
	);
}

#else

void m5249audio_isr(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long	*bp;

#if DEBUG
	printk("m5249audio_isr(irq=%d)", irq);
#endif

	if (m5249audio_dmastart == m5249audio_append) {
		*reg32p(MCFA_INTENABLE) &= ~0x00000008;
		m5249audio_txbusy = 0;
	} else {
		bp = (unsigned long *) &m5249audio_buf[m5249audio_dmastart];
		*reg32p(MCFA_PDOR3) = *bp;
		m5249audio_dmastart += 4;
		if (m5249audio_dmastart >= BUFSIZE)
			m5249audio_dmastart = 0;
	}
}

#endif  /* CONFIG_AUDIOIRQASM */
#endif	/* CONFIG_AUDIOIRQ */

/****************************************************************************/

#ifdef CONFIG_AUDIODMA

/*
 *	Configure and start DMA engine.
 */
void __inline__ m5249audio_dmarun(void)
{
#if DEBUG
	printk("m5249audio_dmarun(): dma=%x count=%d\n",
		m5249audio_dmastart, m5249audio_dmacount);
#endif

	set_dma_mode(M5249AUDIO_DMA, DMA_MODE_WRITE|DMA_MODE_LONG_BIT);
	set_dma_device_addr(M5249AUDIO_DMA, (MCF_MBAR2+MCFA_PDOR3));
	set_dma_addr(M5249AUDIO_DMA, (int)&m5249audio_buf[m5249audio_dmastart]);
	set_dma_count(M5249AUDIO_DMA, m5249audio_dmacount);
	m5249audio_dmaing = 1;
	m5249audio_txbusy = 1;
	enable_dma(M5249AUDIO_DMA);
}

/*
 *	Start DMA'ing a new buffer of data if any available.
 */
void m5249audio_dmabuf(void)
{
#if DEBUG
	printk("m5249audio_dmabuf()\n");
#endif

	/* If already running then nothing to do... */
	if (m5249audio_dmaing)
		return;

	/* Set DMA buffer size */
	m5249audio_dmacount = (m5249audio_append >= m5249audio_appstart) ?
		(m5249audio_append - m5249audio_appstart) :
		(BUFSIZE - m5249audio_appstart);
	if (m5249audio_dmacount > DMASIZE)
		m5249audio_dmacount = DMASIZE;

	/* Adjust pointers and counters accordingly */
	m5249audio_appstart += m5249audio_dmacount;
	if (m5249audio_appstart >= BUFSIZE)
		m5249audio_appstart = 0;

	if (m5249audio_dmacount > 0)
		m5249audio_dmarun();
	else
		m5249audio_txbusy = 0;
}


/*
 *	Interrupt from DMA engine.
 */
void m5249audio_dmaisr(int irq, void *dev_id, struct pt_regs *regs)
{
#if DEBUG
	printk("m5249audio_dmaisr(irq=%d)", irq);
#endif

	/* Clear DMA interrupt */
	disable_dma(M5249AUDIO_DMA);
	m5249audio_dmaing = 0;

	/* Update data pointers and counts */
	m5249audio_dmastart += m5249audio_dmacount;
	if (m5249audio_dmastart >= BUFSIZE)
		m5249audio_dmastart = 0;
	m5249audio_dmacount = 0;

	/* Start new DMA buffer if we can */
	m5249audio_dmabuf();
}

#endif	/* CONFIG_AUDIODMA */

/****************************************************************************/

void m5249audio_txdrain(void)
{
#ifdef DEBUG
	printk("m5249audio_txdrain()\n");
#endif

	while (!signal_pending(current)) {
		if (m5249audio_txbusy == 0)
			break;
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(1);
	}
}

/****************************************************************************/

int m5249audio_open(struct inode *inode, struct file *filp)
{
#if DEBUG
	printk("m5249audio_open()\n");
#endif

	if (m5249audio_isopen)
		return(-EBUSY);

	/* Set initial driver playback defaults. */
	m5249audio_speed = 44100;
	m5249audio_format = AFMT_S16_BE;
	m5249audio_bits = 16;
	m5249audio_stereo = 1;
	m5249audio_isopen = 1;
	m5249audio_unmute();

	return(0);
}

/****************************************************************************/

int m5249audio_close(struct inode *inode, struct file *filp)
{
#if DEBUG
	printk("m5249audio_close()\n");
#endif

	m5249audio_txdrain();
	m5249audio_mute();

#ifdef CONFIG_AUDIOIRQ
	*reg32p(MCFA_INTENABLE) &= ~0x00000008;
#endif
#ifdef CONFIG_AUDIODMA
	disable_dma(M5249AUDIO_DMA);
	m5249audio_dmaing = 0;
#endif

	m5249audio_txbusy = 0;
	m5249audio_dmastart = 0;
	m5249audio_dmacount = 0;
	m5249audio_appstart = 0;
	m5249audio_append = 0;
	m5249audio_isopen = 0;
	return(0);
}

/****************************************************************************/

ssize_t m5249audio_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
	unsigned long	*dp, *buflp;
	unsigned short	*bufwp;
	unsigned char	*bufbp;
	unsigned int	slen, bufcnt, i, s, e;

#ifdef DEBUG
	printk("m5249_write(buf=%x,count=%d)\n", (int) buf, count);
#endif

	if (count <= 0)
		return(0);

	buflp = (unsigned long *) buf;
	bufwp = (unsigned short *) buf;
	bufbp = (unsigned char *) buf;

	bufcnt = count & ~0x3;
	if (m5249audio_stereo == 0)
		bufcnt <<= 1;
	if (m5249audio_bits == 8)
		bufcnt <<= 1;

tryagain:
	/*
	 *	Get a snapshot of buffer, so we can figure out how
	 *	much data we can fit in...
	 */
	s = m5249audio_dmastart;
	e = m5249audio_append;
	dp = (unsigned long *) &m5249audio_buf[e];

	slen = ((s > e) ? (s - e) : (BUFSIZE - (e - s))) - 4;
	if (slen > bufcnt)
		slen = bufcnt;
	if ((BUFSIZE - e) < slen)
		slen = BUFSIZE - e;

	if (slen == 0) {
		if (signal_pending(current))
			return(-ERESTARTSYS);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
		goto tryagain;
	}

	if (m5249audio_stereo) {
		if (m5249audio_bits == 16) {
			for (i = 0; (i < slen); i += 4)
				*dp++ = *buflp++;
		} else {
			for (i = 0; (i < slen); i += 4) {
				*dp    = (((unsigned long) *bufbp++) << 24);
				*dp++ |= (((unsigned long) *bufbp++) << 8);
			}
		}
	} else {
		if (m5249audio_bits == 16) {
			for (i = 0; (i < slen); i += 4) {
				*dp++ = (((unsigned long)*bufwp)<<16) | *bufwp;
				bufwp++;
			}
		} else {
			for (i = 0; (i < slen); i += 4) {
				*dp++ = (((unsigned long) *bufbp) << 24) | 
					(((unsigned long) *bufbp) << 8);
				bufbp++;
			}
		}
	}

	e += slen;
	if (e >= BUFSIZE)
		e = 0;
	m5249audio_append = e;

#ifdef CONFIG_AUDIOIRQ
	/* If not outputing audio, then start now */
	if (m5249audio_txbusy == 0) {
		m5249audio_txbusy++;
		*reg32p(MCFA_INTENABLE) |= 0x00000008;
		/* Dummy write to start output interrupts */
		*reg32p(MCFA_PDOR3) = 0;
	}
#endif
#ifdef CONFIG_AUDIODMA
	m5249audio_dmabuf();
#endif

	bufcnt -= slen;
	if (bufcnt > 0)
		goto tryagain;

	return(count);
}

/****************************************************************************/

int m5249audio_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	long	val;
	int	rc = 0;

#ifdef DEBUG
        printk("m5249audio_ioctl(cmd=%x,arg=%x)\n", (int) cmd, (int) arg);
#endif

	switch (cmd) {

	case SNDCTL_DSP_SPEED:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if (rc == 0) {
			get_user(val, (unsigned long *) arg);
			m5249audio_txdrain();
			m5249audio_speed = val;
			/* FIXME: adjust replay speed?? */
		}
		break;

	case SNDCTL_DSP_SAMPLESIZE:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if (rc == 0) {
			get_user(val, (unsigned long *) arg);
			m5249audio_txdrain();
			m5249audio_setsamplesize(val);
		}
		break;

	case SNDCTL_DSP_STEREO:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if (rc == 0) {
			get_user(val, (unsigned long *) arg);
			m5249audio_txdrain();
			m5249audio_stereo = val;
		}
		break;

	case SNDCTL_DSP_GETBLKSIZE:
		rc = verify_area(VERIFY_WRITE, (void *) arg, sizeof(long));
		if (rc == 0)
			put_user(DMASIZE, (long *) arg);
		break;

	case SNDCTL_DSP_SYNC:
		m5249audio_txdrain();
		break;

	default:
		rc = -EINVAL;
		break;
	}

	return(rc);
}

/****************************************************************************/

struct file_operations	m5249audio_fops = {
	open: m5249audio_open,		/* open */
	release: m5249audio_close,	/* release */
	write: m5249audio_write,	/* write */
	ioctl: m5249audio_ioctl,	/* ioctl */
};

/****************************************************************************/

#ifdef CONFIG_AUDIOIRQ

void m5249audio_getallintrs(void)
{
	int	i, rc;

	for (i = 128; (i < 160); i++) {
		rc = request_irq(i, m5249audio_isr, SA_INTERRUPT, "audio", NULL);
		if (rc)
			printk("M5249AUDIO: IRQ %d already in use?\n", i);
	}

	/* Enable all audio interrupts... */
	*((volatile unsigned long *) (MCF_MBAR2 + 0x140)) = 0x33333333;
	*((volatile unsigned long *) (MCF_MBAR2 + 0x144)) = 0x33333333;
	*((volatile unsigned long *) (MCF_MBAR2 + 0x148)) = 0x33333333;
	*((volatile unsigned long *) (MCF_MBAR2 + 0x14c)) = 0x33333333;
}

#endif

/****************************************************************************/

static int __init m5249audio_init(void)
{
	printk("M5249AUDIO: (C) Copyright 2002, "
		"Greg Ungerer (gerg@snapgear.com)\n");

	if (register_chrdev(SOUND_MAJOR, "sound", &m5249audio_fops) < 0) {
		printk(KERN_WARNING "SOUND: failed to register major %d\n",
			SOUND_MAJOR);
		return(0);
	}

	m5249audio_buf = kmalloc(BUFSIZE, GFP_KERNEL);
	if (m5249audio_buf == NULL) {
		printk("M5249AUDIO: failed to allocate DMA[%d] buffer\n",
			BUFSIZE);
	}

#ifdef CONFIG_AUDIOIRQ
#ifdef CONFIG_AUDIOIRQASM
	/* Install fast interrupt handler */
	printk("M5249AUDIO: fast interrupt handler irq=%d\n",
		M5249AUDIO_TXIRQ);
	*((unsigned long *) (M5249AUDIO_TXIRQ * 4)) =
		(unsigned long) m5249audio_isr;
#else
	/* Re-direct TX empty FIFO audio interrupt. */
	printk("M5249AUDIO: standard interrupt handler, irq=%d\n",
		M5249AUDIO_TXIRQ);
	if (request_irq(M5249AUDIO_TXIRQ, m5249audio_isr,
	    (SA_INTERRUPT | IRQ_FLG_FAST), "audio", NULL)) {
		printk("M5249AUDIO: IRQ %d already in use?\n",
			M5249AUDIO_TXIRQ);
	}
#endif
	*reg32p(0x140) = 0x00007000;
#endif
#ifdef CONFIG_AUDIODMA
{
	volatile unsigned char *dmap;
	unsigned int		icr;

	printk("M5249AUDIO: DMA channel=%d, irq=%d\n",
		M5249AUDIO_DMA, M5249AUDIO_DMAIRQ);
	if (request_irq(M5249AUDIO_DMAIRQ, m5249audio_dmaisr,
	    (SA_INTERRUPT | IRQ_FLG_FAST), "audio(DMA)", NULL)) {
		printk("M5249AUDIO: DMA IRQ %d already in use?\n", 
			M5249AUDIO_DMAIRQ);
	}

	dmap = (volatile unsigned char *) dma_base_addr[M5249AUDIO_DMA];
	dmap[MCFDMA_DIVR] = M5249AUDIO_DMAIRQ;

	/* Set interrupt level and priority */
	switch (M5249AUDIO_DMA) {
	case 1:  icr=MCFSIM_DMA1ICR; m5249audio_imrbit=MCFSIM_IMR_DMA1; break;
	case 2:  icr=MCFSIM_DMA2ICR; m5249audio_imrbit=MCFSIM_IMR_DMA2; break;
	case 3:  icr=MCFSIM_DMA3ICR; m5249audio_imrbit=MCFSIM_IMR_DMA3; break;
	default: icr=MCFSIM_DMA0ICR; m5249audio_imrbit=MCFSIM_IMR_DMA0; break;
	}

	*simp(icr) = MCFSIM_ICR_LEVEL7 | MCFSIM_ICR_PRI3;
	mcf_setimr(mcf_getimr() & ~m5249audio_imrbit);

	/* Set DMA to use channel 0 for audio */
	*reg8p(MCFA_DMACONF) = MCFA_DMA_0REQ;
	*reg32p(MCFSIM2_DMAROUTE) = 0x00000080;

	if (request_dma(M5249AUDIO_DMA, "audio")) {
		printk("M5249AUDIO: DMA channel %d already in use?\n",
			M5249AUDIO_DMA);
	}
}
#endif

	/* Unmute the DAC, set GPIO49 */
	*reg32p(MCFSIM2_GPIO1FUNC) |= 0x00020000;
	*reg32p(MCFSIM2_GPIO1ENABLE) |= 0x00020000;
	m5249audio_unmute();

	/* Power up the DAC, set GPIO39 */
	*reg32p(MCFSIM2_GPIO1FUNC) |= 0x00000080;
	*reg32p(MCFSIM2_GPIO1ENABLE) |= 0x00000080;
	*reg32p(MCFSIM2_GPIO1WRITE) |= 0x00000080;

	m5249audio_chipinit();

	/* Dummy write to start outputing */
	*reg32p(MCFA_PDOR3) = 0;

	return(0);
}

module_init(m5249audio_init);

/****************************************************************************/
