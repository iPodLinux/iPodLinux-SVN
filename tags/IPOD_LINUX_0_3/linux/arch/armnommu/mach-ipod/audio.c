/*
 * ipod_audio.c - audio driver for iPod
 *
 * Copyright (c) 2003, Bernard Leach (leachbj@bouncycastle.org)
 */

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
#include <asm/io.h>
#include <asm/arch/irqs.h>


#define D2A_POWER_OFF   1
#define D2A_POWER_SB    2
#define D2A_POWER_ON    3

/* locations for our shared variables */
#define DMA_READ_OFF	0x40000000
#define DMA_WRITE_OFF	0x40000004
#define DMA_ACTIVE	0x40000008
#define DMA_BASE	0x4000000c

/* length of shared buffer in half-words (starting at DMA_BASE) */
#define BUF_LEN		(46*1024)

/* volumes in dB */
#define MAX_VOLUME      6
#define MIN_VOLUME      -73

#define LRHPBOTH        0x100
#define ZERO_DB         0x79

static int ipodaudio_isopen;
static int ipodaudio_power_state;

/** XXX **/

/* get current usec counter */
static int
timer_get_current(void)
{
	return inl(0xcf001110);
}

/* check if number of seconds has past */
static int
timer_check(int clock_start, int usecs)
{
	if ( (inl(0xcf001110) - clock_start) >= usecs ) {
		return 1;
	} else {
		return 0;
	}
}

static void
set_clock_enb(unsigned short clks, int on)
{
	if ( on ) {
		outw(inw(0xcf005000) | clks, 0xcf005000);
	}
	else {
		outw(inw(0xcf005000) & ~clks, 0xcf005000);
	}
}

static int
d2a_send_cmd(unsigned int arg_r0, unsigned int arg_r1, unsigned char *arg_r2)
{
	int start;
	int data_addr;
	int i;
unsigned long flags;

	if ( arg_r1 < 0x1 ) return 0x9;
	if ( arg_r1 > 0x4 ) return 0x9;

save_flags_cli(flags);
	if ( (inb(0xc000801c) & (1<<6)) != 0 ) {
		// get start time
		start = timer_get_current();

		while ( (inb(0xc000801c) & (1<<6)) != 0 ) {
			if ( timer_check(start, 1000) != 0 ) {
				// timeout
				return 0x15;
			}
		}
	}
restore_flags(flags);

	// clear top 15 bits, left shift 1
	outb((arg_r0 << 17) >> 16, 0xc0008004);

	outb(inb(0xc0008000) & ~0x20, 0xc0008000);

	data_addr = 0xc000800c;
	for ( i = 0; i < arg_r1; i++ ) {
		outb(*arg_r2++, data_addr);

		data_addr += 4;
	}

	outb((inb(0xc0008000) & ~0x26) | ((arg_r1-1) << 1), 0xc0008000);

	outb(inb(0xc0008000) | 0x80, 0xc0008000);

	return 0x0;
}

static void
d2a_set_active(int active)
{
	unsigned char cmd[2];

	// set active to 0x0 or 0x1
	if ( active == 0 ) {
		cmd[0] = 0x12;
		cmd[1] = 0x0;

		d2a_send_cmd(0x1a, 0x2, cmd);
	} else {
		cmd[0] = 0x12;
		cmd[1] = 0x1;

		d2a_send_cmd(0x1a, 0x2, cmd);
	}
}

static void
d2a_set_power(int new_state)
{
	unsigned char cmd[2];

	if ( ipodaudio_power_state == new_state) {
		return;
	}

	if ( new_state != D2A_POWER_OFF ) {
		set_clock_enb((1<<1), 0x1);
	}

	if ( new_state == D2A_POWER_ON ) {
		// set power register to POWER_OFF=0 on OUTPD=0, DACPD=0
		cmd[0] = 0xc; cmd[1] = 0x67;
		d2a_send_cmd(0x1a, 0x2, cmd);

		// de-activate the d2a
		d2a_set_active(0x0);

		// set DACSEL=1
		cmd[0] = 0x8; cmd[1] = 0x10;
		d2a_send_cmd(0x1a, 0x2, cmd);

		// set DACMU=0 DEEMPH=0
		cmd[0] = 0xa; cmd[1] = 0x0;
		d2a_send_cmd(0x1a, 0x2, cmd);

		// set BCLKINV=0(Dont invert BCLK) MS=1(Enable Master) LRSWAP=0 LRP=0 IWL=10(24 bit) FORMAT=10(I2S format)
		cmd[0] = 0xe; cmd[1] = 0x4a;
		d2a_send_cmd(0x1a, 0x2, cmd);

		// set CLKIDIV2=0 SR=1000 BOSR=1 USB/NORM=1
		//cmd[0] = 0x10; cmd[1] = 0x23;

		// set CLKIDIV2=1 SR=1000 BOSR=1 (272fs) USB/NORM=1 (USB)
		cmd[0] = 0x10; cmd[1] = 0x63;

		// set CLKIDIV2=1 SR=0011 BOSR=0 (fs) USB/NORM=1 (USB)
		//cmd[0] = 0x10; cmd[1] = 0x4d;
		d2a_send_cmd(0x1a, 0x2, cmd);

		// activate the d2a
		d2a_set_active(0x1);
	}
	else {
		/* power off or standby the audio chip */

		// set DACMU=1 DEEMPH=0
		cmd[0] = 0xa; cmd[1] = 0x8;
		d2a_send_cmd(0x1a, 0x2, cmd);

		// set DACSEL=0
		cmd[0] = 0x8; cmd[1] = 0x0;
		d2a_send_cmd(0x1a, 0x2, cmd);

		// set POWEROFF=0 OUTPD=0 DACPD=1
		cmd[0] = 0xc; cmd[1] = 0x6f;
		d2a_send_cmd(0x1a, 0x2, cmd);

		if ( new_state == D2A_POWER_OFF ) {
			/* power off the chip */

			// set POWEROFF=1 OUTPD=1 DACPD=1
			cmd[0] = 0xc; cmd[1] = 0xff;
			d2a_send_cmd(0x1a, 0x2, cmd);

			set_clock_enb((1<<1), 0x0);
		}
		else {
			/* standby the chip */

			// set POWEROFF=0 OUTPD=1 DACPD=1
			cmd[0] = 0xc; cmd[1] = 0x7f;
			d2a_send_cmd(0x1a, 0x2, cmd);
		}
	}

	ipodaudio_power_state = new_state;
}

static void d2a_set_vol(int vol)
{
	unsigned char cmd[2];
	unsigned int v;

	if ( vol > MAX_VOLUME ) {
		vol = MAX_VOLUME;
	}

	if ( vol < MIN_VOLUME ) {
		vol = MIN_VOLUME;
	}

	v = (vol + ZERO_DB) | LRHPBOTH;

	// set volume
	cmd[0] = 4 | (v >> 8); cmd[1] = (unsigned char)v;
	d2a_send_cmd(0x1a, 0x2, cmd);
}

static int ipodaudio_open(struct inode *inode, struct file *filep)
{
	if ( ipodaudio_isopen ) {
		return -EBUSY;
	}

	ipodaudio_isopen = 1;

	d2a_set_power(D2A_POWER_ON);

	// set the volument to -6dB
	d2a_set_vol(-20);

	return 0;
}

static void ipodaudio_txdrain()
{
	while ( (inl(0xc000251c) & (1<<0)) == 0 ) {
		// empty
	}
}

static int ipodaudio_close(struct inode *inode, struct file *filp)
{
	ipodaudio_txdrain();

	d2a_set_power(D2A_POWER_OFF);

	ipodaudio_isopen = 0;

	return 0;
}


static ssize_t ipodaudio_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
	unsigned short *bufsp;
	size_t rem;

	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	int write_off_current, write_off_next, read_off_current;
	volatile int *dma_active = (int *)DMA_ACTIVE;
	volatile unsigned short *dma_buf = (unsigned short *)DMA_BASE;

	if ( count <= 0 ) {
		return 0;
	}

	bufsp = (unsigned short *)buf;
	rem = count/2;

	write_off_current = *w_off;

	while ( rem > 0 ) {
		int cnt;

		write_off_next = (write_off_current + 1);
		if ( write_off_next > BUF_LEN ) write_off_next = 0;

		read_off_current = *r_off;

		if ( read_off_current < write_off_current ) {
			cnt = BUF_LEN - 1 - write_off_current;

			if ( cnt > 0 )  {
				if ( cnt > rem ) cnt = rem;

				memcpy(&dma_buf[write_off_next], bufsp, cnt<<1);

				rem -= cnt;
				bufsp += cnt;

				if ( read_off_current > 0 && rem > 0 ) {
					int n;

					if ( rem > read_off_current ) {
						n = read_off_current;
					}
					else {
						n = rem;
					}

					memcpy(&dma_buf[0], bufsp, n<<1);

					rem -= n;
					bufsp += n;

					write_off_current = n - 1;
				}
				else {
					write_off_current += cnt;
				}
			}
		}
		else if ( read_off_current > write_off_current ) {
			cnt = read_off_current - 1 - write_off_current;
			if ( cnt > rem ) cnt = rem;

			memcpy(&dma_buf[write_off_next], bufsp, cnt<<1);

			bufsp += cnt;
			rem -= cnt;

			write_off_current += cnt;
		}
		else {
			/* buffer is empty */
			if ( rem < BUF_LEN ) {
				cnt = rem;
			}
			else {
				cnt = BUF_LEN;
			}
			memcpy(&dma_buf[0], bufsp, cnt<<1);

			bufsp += cnt;
			rem -= cnt;

			write_off_current = cnt - 1;

			/* we have copied to the start of the buffer */
			*r_off = 0;
		}

		if ( !*dma_active ) {
			*dma_active = 1;

			*w_off = write_off_current;

			outl(inl(0xc000251c)|(1<<9), 0xc000251c);

			/* dummy write to start things */
			outl(0x0, 0xc0002540);
		}
	}

	*w_off = write_off_current;

	return count;
}

static int ipodaudio_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	unsigned long val;

	switch (cmd) {
	case SNDCTL_DSP_SPEED:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if ( rc == 0 ) {
			get_user(val, (unsigned long *) arg);
			if ( val != 44100 ) {
				put_user(44100, (long *) arg);
			}
		}
		break;

	case SNDCTL_DSP_GETFMTS:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if ( rc == 0 ) {
			put_user(AFMT_S16_LE, (long *) arg);
		}

	case SNDCTL_DSP_SETFMT:
	/* case SNDCTL_DSP_SAMPLESIZE: */
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if ( rc == 0 ) {
			get_user(val, (unsigned long *) arg);
			if ( val != AFMT_S16_LE ) {
				put_user(AFMT_S16_LE, (long *) arg);
			}
		}
		break;

	case SNDCTL_DSP_STEREO:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if ( rc == 0 ) {
			get_user(val, (unsigned long *) arg);
			if ( val != 1 ) {
				put_user(1, (long *) arg);
			}
		}
		break;

	case SNDCTL_DSP_GETBLKSIZE:
		rc = verify_area(VERIFY_WRITE, (void *) arg, sizeof(long));
		if ( rc == 0 ) {
			put_user(BUF_LEN/2, (long *) arg);
		}
		break;

	case SNDCTL_DSP_SYNC:
		ipodaudio_txdrain();
		break;
	}

	return rc;
}

static struct file_operations  ipodaudio_fops = {
	open: ipodaudio_open,
	release: ipodaudio_close,
	write: ipodaudio_write,
	ioctl: ipodaudio_ioctl,
};

void ipod_process_dma(void)
{
	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	volatile int *dma_active = (int *)DMA_ACTIVE;
	volatile unsigned short *dma_buf = (unsigned short *)DMA_BASE;

	inl(0xcf001040);
	outl(inl(0xc000251c) & ~(1<<9), 0xc000251c);

	while ( *r_off != *w_off ) {
		int i;

		while ( (inl(0xc000251c) & 0x7800000) == 0 ) {
		}

		outl(((unsigned)dma_buf[*r_off]) << 16, 0xc0002540);

		*r_off = (*r_off + 1) % BUF_LEN;
	}

	*dma_active = 0;
}

static int __init ipodaudio_init(void)
{
	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	volatile int *dma_active = (int *)DMA_ACTIVE;

	printk("ipodaudio: (c) Copyright 2003, Bernard Leach (leachbj@bouncycastle.org\n\n");

	if ( register_chrdev(SOUND_MAJOR, "sound", &ipodaudio_fops) < 0 ) {
		printk(KERN_WARNING "SOUND: failed to register major %d\n",
			SOUND_MAJOR);
		return 0;
	}

	/* initialise shared variables */
	*r_off = 0;
	*w_off = 0;
	*dma_active = 0;

	outl(inl(0xcf00103c) | (1 << DMA_OUT_IRQ) , 0xcf00103c);
	outl((1 << DMA_OUT_IRQ), 0xcf001034);

#if 0
	/* intialise the hardware */
	outl((inl(0xcf004040) & ~0xc00) | 0x800, 0xcf004040);

	outl(inl(0xcf005030) | (1<<7), 0xcf005030);
	outl(inl(0xcf005030) & ~(1<<7), 0xcf005030);

	outl(inl(0xcf005030) | (1<<8), 0xcf005030);
	outl(inl(0xcf005030) & ~(1<<8), 0xcf005030);

	outl((1<<0) | (1<<2), 0xc0002500);

	outl(inl(0xc000251c) | (1<<17), 0xc000251c);
	outl(inl(0xc000251c) & ~(1<<17), 0xc000251c);

	outl(inl(0xc000251c) | (1<<16), 0xc000251c);
	outl(inl(0xc000251c) & ~(1<<16), 0xc000251c);

	outb(0x0, 0xc0008000);

	outb(0x20, 0xc0008034);
	outb(0x0, 0xc0008038);

	outb((1<<1) | (1<<0), 0xc0008020);

	outb(0x55, 0xc000802c);
	outb(0x54, 0xc0008030);

	d2a_set_power(D2A_POWER_SB);
#endif

	return 0;
}

module_init(ipodaudio_init);

