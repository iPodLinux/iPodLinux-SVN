/****************************************************************************/

/*
 *      m5272audio.c (c) 2003 Frieder Ferlemann
 *
 *      Derived from:
 *      m5249audio.c by Greg Ungerer and
 *      pwm328.c by David Beckemeyer
 */

/****************************************************************************/

#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/soundcard.h>
#include <asm/param.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/mcftimer.h>

/****************************************************************************/

MODULE_AUTHOR("Frieder Ferlemann <f.ferlemann@identecsolutions.de>");
MODULE_DESCRIPTION("Driver for 8 bit PWM audio");
MODULE_SUPPORTED_DEVICE("Motorola MCF5272");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;

/****************************************************************************/

#define NAME "m5272audio"

/* register defines (should have been available from m5272sim.h) */
#define MCFSIM_PWCR0	(0xc0)
#define MCFSIM_PWWD0	(0xd0)
#define MCFSIM_TMR0	(0x200)
#define MCFSIM_TRR0	(0x204)
#define MCFSIM_TCN0	(0x20C)
#define MCFSIM_TER0	(0x210)

/* register defaults */
#define TMR_RESET	(0)
#define TMR_DEFAULT	(((125-1)<<8) | 0x0b)
#define PWCR_DEFAULT	(0x21)

//#define DEBUG_M5272AUDIO_ISR
//#define DEBUG_M5272AUDIO_ISR2
//#define DEBUG_M5272AUDIO_OPEN_CLOSE
//#define DEBUG_M5272AUDIO_TXDRAIN
//#define DEBUG_M5272AUDIO_WRITE
//#define DEBUG_M5272AUDIO_BUF
//#define DEBUG_IOCTL

/****************************************************************************/
/*
 * 	Resources used
 */

#define	M5272AUDIO_BUFSIZE	(0x1000)
#define PWM_CHANNEL	(1)
#define TIMER_NUMBER	(0)

#define IRQ		(69+TIMER_NUMBER)
/* you'll probably need 7 for glitchless audio */
#define IRQ_PRIORITY	(6)

/****************************************************************************/
/*
 *      Quick and easy access to the registers. (This optimizes
 *      nicely, since it will almost always be a constant pointer).
 */
#define reg32p(r)       ((volatile unsigned long *) (MCF_MBAR + (r)))
#define reg16p(r)       ((volatile unsigned short *) (MCF_MBAR + (r)))
#define reg8p(r)        ((volatile unsigned char *) (MCF_MBAR + (r)))

/****************************************************************************/

unsigned char		m5272audio_buf[M5272AUDIO_BUFSIZE];

volatile unsigned int	m5272audio_start;
volatile unsigned int	m5272audio_append;
volatile unsigned int	m5272audio_txbusy;

unsigned int		m5272audio_isopen;
unsigned int	  	resource_count;

/****************************************************************************/

DECLARE_WAIT_QUEUE_HEAD( m5272audio_waitchan );

/****************************************************************************/

struct m5272audio_format {
        unsigned int    format;
        unsigned int    bits;
} m5272audio_formattable[] = {
        { AFMT_U8, 	8 }
};

#define FORMATSIZE      (sizeof(m5272audio_formattable) / sizeof(struct m5272audio_format))

int m5272audio_setsamplesize(int val)
{
        int     i;

        for (i = 0; (i < FORMATSIZE); i++) {
                if (m5272audio_formattable[i].format == val) {
                        return 0;
                }
        }
	return -EINVAL;
}

/****************************************************************************/

void inline timer_init(void)
{
	/* Initialize timer */
        *reg16p( MCFSIM_TRR0 + TIMER_NUMBER * 0x20 ) = 32; 	// 66MHz/(33*125) = 16kHz
        *reg16p( MCFSIM_TMR0 + TIMER_NUMBER * 0x20 ) = TMR_DEFAULT; 
}

/****************************************************************************/

void inline timer_interrupt_init(void)
{
        int flags;
      	uint16_t t;
	
	save_flags(flags); cli();
        
	t= *reg16p( 2 + MCFSIM_ICR1 ); 
	t = (t & 0x7777 & ~(0xf000>>TIMER_NUMBER*4)) | ( (0x8000 | (IRQ_PRIORITY<<12) )>>TIMER_NUMBER*4);
	*reg16p( 2 + MCFSIM_ICR1 ) = t; 
	
	restore_flags(flags);
}

/****************************************************************************/

void inline timer_interrupt_exit(void)
{
        int flags;
      	uint16_t t;
	
	save_flags(flags); cli();
        
	t = *reg16p( 2 + MCFSIM_ICR1 ); 
	t = (t & 0x7777 & ~(0xf000>>TIMER_NUMBER*4)) | (0x8000>>TIMER_NUMBER*4);
	*reg16p( 2 + MCFSIM_ICR1 ) = t; 
	
	restore_flags(flags);
}

/****************************************************************************/

void inline timer_interrupt_on(void)
{	
#ifdef DEBUG_M5272AUDIO_ISR
	printk( KERN_DEBUG NAME ": " __FUNCTION__ "\n");
#endif
        *reg16p( MCFSIM_TMR0 + TIMER_NUMBER * 0x20 ) =  MCFTIMER_TMR_ENORI | TMR_DEFAULT; 
}

/****************************************************************************/

short inline timer_interrupt_is_on(void)
{	
        return 0 != (*reg16p( MCFSIM_TMR0 + TIMER_NUMBER * 0x20 ) & 0x10); 
}

/****************************************************************************/

void inline timer_interrupt_off(void)
{
        *reg16p( MCFSIM_TMR0 + TIMER_NUMBER * 0x20 ) = TMR_DEFAULT; 
}

/****************************************************************************/

void inline pwm_on(void)
{
        *reg8p( 3 + MCFSIM_PWCR0 + PWM_CHANNEL * 4 ) = PWCR_DEFAULT | 0x80;
}

/****************************************************************************/

void inline pwm_off(void)
{
        *reg8p( 3 + MCFSIM_PWCR0 + PWM_CHANNEL * 4 ) = PWCR_DEFAULT; 
}

/****************************************************************************/

void inline pwm_set(uint8_t t)
{
        *reg8p( 3 + MCFSIM_PWWD0 + PWM_CHANNEL * 4 ) = t;
}

/****************************************************************************/

void pwm_configure_pin(void)
{
        pwm_off();
	pwm_set( 0xff );

	/* switch pin for use with PWM. (Only the two upper channels share their output pin) */
	if( PWM_CHANNEL == 1 ){
		int flags;
		uint16_t t;

		save_flags(flags); cli();

         	t = *reg16p( 2 + MCFSIM_PDCNT ); 
		t = (t &  ~0x3000) | 0x1000;
		*reg16p( 2 + MCFSIM_PDCNT ) = t; 

		restore_flags(flags);
	}
	if( PWM_CHANNEL == 2 ){
		int flags;
		uint16_t t;

		save_flags(flags); cli();

         	t = *reg16p( 2 + MCFSIM_PDCNT ); 
		t = (t & ~0xc000) | 0x4000;
		*reg16p( 2 + MCFSIM_PDCNT ) = t; 

		restore_flags(flags);
	}
}


/****************************************************************************/

void m5272audio_isr(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned char	*bp;
	
#ifdef DEBUG_M5272AUDIO_ISR2
	int flags;
	
 	save_flags(flags); cli();
	*reg16p( MCFSIM_PADAT ) |= 1;
	restore_flags(flags);
#endif	

	/* clear IRQ */
        *reg16p( MCFSIM_TER0 + TIMER_NUMBER * 0x20 ) = 0x0003; 

	if (m5272audio_start == m5272audio_append) {
		m5272audio_txbusy = 0;
		timer_interrupt_off();
		pwm_off();	
#ifdef DEBUG_M5272AUDIO_ISR
		printk( KERN_DEBUG NAME ": " __FUNCTION__ " done\n");
#endif
	} else {
		bp = &m5272audio_buf[ m5272audio_start++ ];
		pwm_set( *bp );
		if (m5272audio_start >= M5272AUDIO_BUFSIZE)
			m5272audio_start = 0;
	}

#ifdef DEBUG_M5272AUDIO_ISR2
 	save_flags(flags); cli();
	*reg16p( MCFSIM_PADAT ) &= ~1;
	restore_flags(flags);
#endif	


}


/****************************************************************************/

void m5272audio_txdrain(void)
{
#ifdef DEBUG_M5272AUDIO_TXDRAIN
	printk(KERN_DEBUG NAME ": " __FUNCTION__ "\n");
#endif
	while ( m5272audio_txbusy ) {
	    interruptible_sleep_on_timeout( &m5272audio_waitchan, 1 );
	}
}

/****************************************************************************/

int m5272audio_open(struct inode *inode, struct file *filp)
{
#ifdef DEBUG_M5272AUDIO_OPEN_CLOSE
	printk(KERN_DEBUG NAME ": " __FUNCTION__ "\n" );
#endif

	if (m5272audio_isopen)
		return -EBUSY;
	m5272audio_isopen++;
	return 0;
}

/****************************************************************************/

int m5272audio_release(struct inode *inode, struct file *filp)
{
#ifdef DEBUG_M5272AUDIO_OPEN_CLOSE
	printk(KERN_DEBUG NAME ": " __FUNCTION__ "\n" );
#endif
	m5272audio_txdrain ();
	m5272audio_isopen = 0;
	return 0;
}

/****************************************************************************/

ssize_t m5272audio_write(struct file *filp, const char *buf, size_t count, loff_t *offset )
{
	unsigned char *bufp;
	unsigned int slen, bufcnt, i, s, e;

#ifdef DEBUG_M5272AUDIO_WRITE
	printk(KERN_DEBUG NAME ": "  __FUNCTION__ " (buf=%x,count=%d)\n", (int) buf, count);
#endif

	if (count <= 0)
		return(0);

	bufp = (unsigned char *) buf;
	bufcnt = count;

tryagain:
	/*
	 * Get a snapshot of buffer, so we can figure out how
	 * much data we can fit in...
	 */
	s = m5272audio_start;
	e = m5272audio_append;

	slen = ((s > e) ? (s - e) : (M5272AUDIO_BUFSIZE - (e - s))) - 4;
	if (slen > bufcnt)
		slen = bufcnt;
	if ((M5272AUDIO_BUFSIZE - e) < slen)
		slen = M5272AUDIO_BUFSIZE - e;

	if (slen == 0) {
		if (signal_pending(current))
			return(-ERESTARTSYS);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
		goto tryagain;
	}

#ifdef DEBUG_M5272AUDIO_BUF
	printk(KERN_DEBUG NAME ": " __FUNCTION__ " filling buf[%d] %d\n", e, slen);
#endif

	/* Copy the buffer local and start sending it. */
	copy_from_user( &m5272audio_buf[e], bufp, slen );
	bufp += slen;

	e += slen;
	if (e >= M5272AUDIO_BUFSIZE)
		e = 0;
	m5272audio_append = e;

	pwm_on();
	if ( !timer_interrupt_is_on() ){
		timer_interrupt_on();
		m5272audio_txbusy++;
	}

	bufcnt -= slen;
	if (bufcnt > 0)
		goto tryagain;

	return(count);
}

/****************************************************************************/

int m5272audio_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
        long    val;
        int     rc = 0;

#ifdef DEBUG_IOCTL
        printk(KERN_DEBUG NAME ": " __FUNCTION__ " (cmd=0x%x, arg=0x%x)\n", (int) cmd, (int) arg);
#endif

        switch (cmd) {
	
        case SNDCTL_DSP_SAMPLESIZE:
                rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
                if (rc == 0) {
                        get_user(val, (unsigned long *) arg);
                        m5272audio_txdrain();
                        rc = m5272audio_setsamplesize(val);
                }
                break;
       
        case SNDCTL_DSP_GETBLKSIZE:
                rc = verify_area(VERIFY_WRITE, (void *) arg, sizeof(long));
                if (rc == 0)
                        put_user(M5272AUDIO_BUFSIZE-16, (long *) arg);
                break;

        case SNDCTL_DSP_SYNC:
                m5272audio_txdrain();
                break;

        default:
                rc = -EINVAL;
                break;
        }

        return(rc);
}


/****************************************************************************/
/*
 *	Exported file operations structure for driver...
 */

struct file_operations	m5272audio_fops = {
	.write = m5272audio_write,
	.ioctl = m5272audio_ioctl,
	.open = m5272audio_open,
	.release = m5272audio_release
};

/****************************************************************************/

static int __init m5272audio_init(void)
{
	int			result;

	printk (NAME ": MCF5272 audio driver, (C) 2003 F. Ferlemann\n" );

	/* Register pwm as character device */
	result = register_chrdev(SOUND_MAJOR, NAME, &m5272audio_fops);
	if (result < 0) {
		printk(KERN_ERR NAME ": could not register major %d\n", SOUND_MAJOR);
		return -ENODEV;
	}
        resource_count++;

	/* Install ISR (interrupt service routine) */
	result = request_irq(IRQ, m5272audio_isr, SA_INTERRUPT | IRQ_FLG_FAST, NAME, NULL);
	if (result) {
		printk (KERN_ERR NAME ": could not register IRQ %d\n", IRQ);
		return -ENODEV;
	}

	printk (NAME ": TMR%d PWM%d IRQ %d IRQ_PRIO %d\n", TIMER_NUMBER, PWM_CHANNEL, IRQ, IRQ_PRIORITY);
	
	timer_init();
	timer_interrupt_init();
	pwm_configure_pin();
	resource_count++;
	
	return 0;
}

/****************************************************************************/

static void __exit m5272audio_exit(void)
{
	int 	result;
	
	printk (NAME ": exiting\n");

        if(!resource_count--) return;
	timer_interrupt_exit();
	free_irq(IRQ, NULL);

        if(!resource_count--) return;
	result = unregister_chrdev(SOUND_MAJOR, NAME);
	
}

/****************************************************************************/

module_init(m5272audio_init);
module_exit(m5272audio_exit);
