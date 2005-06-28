/*
 * ipod_audio.c - audio driver for iPod
 *
 * Copyright (c) 2003-2005 Bernard Leach <leachbj@bouncycastle.org>
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
#include <linux/sound.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/irqs.h>
#include <asm/arch/hardware.h>

#define D2A_POWER_OFF   1
#define D2A_POWER_SB    2
#define D2A_POWER_ON    3

/* length of shared buffer in half-words (starting at DMA_BASE) */
#define BUF_LEN		(40*1024)

static int ipodaudio_isopen;
static int ipodaudio_power_state;
static unsigned ipod_hw_ver;
static devfs_handle_t dsp_devfs_handle, mixer_devfs_handle;
static int ipod_sample_rate = 44100;
static volatile int *ipodaudio_stereo = (int *)DMA_STEREO;
static int ipod_mic_boost = 100;
static int ipod_line_level = 0x17;	/* 0dB */
static int ipod_pcm_level = 0x65;	/* -6dB */
static int ipod_active_rec = SOUND_MASK_MIC;

static void
d2a_set_active(int active)
{
	if (ipod_hw_ver > 0x4) return;

	/* set active to 0x0 or 0x1 */
	if (active) {
		ipod_i2c_send(0x1a, 0x12, 0x01);
	} else {
		ipod_i2c_send(0x1a, 0x12, 0x00);
	}
}

static int
d2a_set_sample_rate(int rate)
{
	int sampling_control;

	if (rate <= 8000) {
		if (ipod_hw_ver >= 0x3) {
			/* set CLKIDIV2=1 SR=0011 BOSR=0 USB/NORM=1 (USB) */
			sampling_control = 0x4d;
		}
		else {
			/* set CLKIDIV2=1 SR=0001 BOSR=0 USB/NORM=1 (USB) */
			sampling_control = 0x45;
		}
		rate = 8000;
	}
	else if (rate <= 12000 && ipod_hw_ver >= 0x5) {
		/* set CLKIDIV2=1 SR=1000 BOSR=0 USB/NORM=1 (USB) */
		sampling_control = 0x61;
		rate = 12000;
	}
	else if (rate <= 16000 && ipod_hw_ver >= 0x5) {
		/* set CLKIDIV2=1 SR=1010 BOSR=1 USB/NORM=1 (USB) */
		sampling_control = 0x55;
		rate = 16000;
	}
	else if (rate <= 22050 && ipod_hw_ver >= 0x5) {
		/* set CLKIDIV2=1 SR=1101 BOSR=1 USB/NORM=1 (USB) */
		sampling_control = 0x77;
		rate = 22050;
	}
	else if (rate <= 24000 && ipod_hw_ver >= 0x5) {
		/* set CLKIDIV2=1 SR=1110 BOSR=0 USB/NORM=1 (USB) */
		sampling_control = 0x79;
		rate = 24000;
	}
	else if (rate <= 32000) {
		/* set CLKIDIV2=1 SR=0110 BOSR=0 USB/NORM=1 (USB) */
		sampling_control = 0x59;
		rate = 32000;
	}
	else if (rate <= 44100) {
		/* set CLKIDIV2=1 SR=1000 BOSR=1 USB/NORM=1 (USB) */
		sampling_control = 0x63;
		rate = 44100;
	}
	else if (rate <= 48000) {
		/* set CLKIDIV2=1 SR=0000 BOSR=0 USB/NORM=1 (USB) */
		sampling_control = 0x41;
		rate = 48000;
	}
	else if (rate <= 88200) {
		/* set CLKIDIV2=1 SR=1111 BOSR=1 USB/NORM=1 (USB) */
		sampling_control = 0x7f;
		rate = 88200;
	}
	else {
		/* set for 96kHz */
		/* set CLKIDIV2=1 SR=0111 BOSR=0 USB/NORM=1 (USB) */
		sampling_control = 0x5d;
		rate = 96000;
	}

	if (ipod_hw_ver == 0x7) {
		/* CLKIDIV=0 for the mini2 */
		sampling_control &= ~0x40;
	}

	d2a_set_active(0x0);
	ipod_i2c_send(0x1a, 0x10, sampling_control);
	d2a_set_active(0x1);

	ipod_sample_rate = rate;

	return ipod_sample_rate;
}

static void
d2a_set_power(int new_state)
{
	if ( ipodaudio_power_state == new_state) {
		return;
	}

	if ( new_state == D2A_POWER_ON ) {
		if (ipod_hw_ver > 0x4) {
			ipod_i2c_send(0x1a, 0x1f, 0xff);	/*Reset*/
			ipod_i2c_send(0x1a, 0x1e, 0x0);
			ipod_i2c_send(0x1a, 0x32, 0xfc);	/*Pwr Mgmt(1)*/
			ipod_i2c_send(0x1a, 0x34 | 0x1, 0xf8);	/*Pwr Mgmt(2)*/

			/* set BCLKINV=0(Dont invert BCLK) MS=1(Enable Master) LRSWAP=0 LRP=0 IWL=00(16 bit) FORMAT=10(I2S format) */
			ipod_i2c_send(0x1a, 0xe, 0x42);

			d2a_set_sample_rate(ipod_sample_rate);

			/* set DACMU=0 DEEMPH=0 */
			ipod_i2c_send(0x1a, 0xa, 0x0);

			/* set the volume to -6dB */
			ipod_i2c_send(0x1a, 0x4, ipod_pcm_level);
			ipod_i2c_send(0x1a, 0x6 | 0x1, ipod_pcm_level);
			ipod_i2c_send(0x1a, 0x50, ipod_pcm_level);
			ipod_i2c_send(0x1a, 0x52 | 0x1, ipod_pcm_level);

			ipod_i2c_send(0x1a, 0x45, 0x50);	/* Left out Mix(def) */
			ipod_i2c_send(0x1a, 0x46, 0x50);

			ipod_i2c_send(0x1a, 0x48, 0x50);	/* Right out Mix(def) */
			ipod_i2c_send(0x1a, 0x4b, 0x50);

			ipod_i2c_send(0x1a, 0x4c, 0x0);		/* Mono out Mix */
			ipod_i2c_send(0x1a, 0x4e, 0x0);

			if (ipod_hw_ver == 0x7) {
				ipod_i2c_send(0x1a, 0xC, 0x67);
				ipod_i2c_send(0x1a, 8, 0x10);
				ipod_i2c_send(0x1a, 0x12, 1);
			}
		}
		else {
			/* set power register to POWER_OFF=0 on OUTPD=0, DACPD=0 */
			ipod_i2c_send(0x1a, 0xc, 0x67);

			/* de-activate the d2a */
			d2a_set_active(0x0);

			/* set DACSEL=1 */
			if (ipod_hw_ver >= 0x3) {
				ipod_i2c_send(0x1a, 0x8, 0x18);
			} else {
				ipod_i2c_send(0x1a, 0x8, 0x10);
			}

			/* set DACMU=0 DEEMPH=0 */
			ipod_i2c_send(0x1a, 0xa, 0x00);

			/* set BCLKINV=0(Dont invert BCLK) MS=1(Enable Master) LRSWAP=0 LRP=0 IWL=10(24 bit) FORMAT=10(I2S format) */
			ipod_i2c_send(0x1a, 0xe, 0x4a);

			d2a_set_sample_rate(ipod_sample_rate);

			/* set the volume */
			ipod_i2c_send(0x1a, 0x4 | 0x1, ipod_pcm_level);

			/* activate the d2a */
			d2a_set_active(0x1);
		}
	}
	else {
		/* power off or standby the audio chip */

		/* de-activate d2a */
		d2a_set_active(0x0);

		/* line in mute left & right*/
		ipod_i2c_send(0x1a, 0x0 | 0x1, 0x80);

		/* set DACMU=1 DEEMPH=0 */
		ipod_i2c_send(0x1a, 0xa, 0x8);

		/* set DACSEL=0, MUTEMIC=1 */
		ipod_i2c_send(0x1a, 0x8, 0x2);

		/* set POWEROFF=0 OUTPD=0 DACPD=1 */
		ipod_i2c_send(0x1a, 0xc, 0x6f);

		if ( new_state == D2A_POWER_OFF ) {
			/* power off the chip */

			/* set POWEROFF=1 OUTPD=1 DACPD=1 */
			ipod_i2c_send(0x1a, 0xc, 0xff);
		}
		else {
			/* standby the chip */

			/* set POWEROFF=0 OUTPD=1 DACPD=1 */
			ipod_i2c_send(0x1a, 0xc, 0x7f);
		}
	}

	ipodaudio_power_state = new_state;
}

static void
d2a_activate_linein(void)
{
	d2a_set_active(0x0);

	if (ipod_line_level == 0) {
		ipod_i2c_send(0x1a, 0x0 | 0x1, 0x80);
	} else {
		ipod_i2c_send(0x1a, 0x0 | 0x1, ipod_line_level);
	}
	ipod_i2c_send(0x1a, 0x4 | 0x1, 0x0);   /* headphone mute left & right */
	ipod_i2c_send(0x1a, 0x8, 0xa);   /* BY PASS, mute mic, INSEL=line in */

	ipod_i2c_send(0x1a, 0xa, 0x9); /* disable ADC high pass filter, mute dac */

	/* power on (PWR_OFF=0) */
	ipod_i2c_send(0x1a, 0xc, 0x7a);  /* MICPD */

	d2a_set_active(0x1);
}

static void
d2a_activate_mic(void)
{
	d2a_set_active(0x0);

	ipod_i2c_send(0x1a, 0x0 | 0x1, 0x80);  /* line in mute left & right */
	ipod_i2c_send(0x1a, 0x4 | 0x1, 0x0);   /* headphone mute left & right */

	if (ipod_mic_boost) {
		ipod_i2c_send(0x1a, 0x8, 0x5);   /* INSEL=mic, MIC_BOOST */
	}
	else {
		ipod_i2c_send(0x1a, 0x8, 0x4);   /* INSEL=mic */
	}

	ipod_i2c_send(0x1a, 0xa, 0x9); /* disable ADC high pass filter, mute dac */

	/* power on (PWR_OFF=0) */
	ipod_i2c_send(0x1a, 0xc, 0x79);  /* CLKOUTPD OSCPD OUTPD DACPD LINEINPD */

	d2a_set_active(0x1);
}

static void ipodaudio_process_pb_dma(void)
{
	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	volatile int *dma_active = (int *)DMA_ACTIVE;
	volatile unsigned short *dma_buf = (unsigned short *)DMA_BASE;
	int stereo = *ipodaudio_stereo;
	unsigned ipod_i2s_fifo_out;

	if (ipod_hw_ver > 0x3) {
		ipod_i2s_fifo_out = 0x70002840;
		outl(inl(0x70002800) & ~0x2, 0x70002800);
	} else {
		ipod_i2s_fifo_out = 0xc0002540;
		inl(0xcf001040);
		outl(inl(0xc000251c) & ~(1<<9), 0xc000251c);
	}

	while (*r_off != *w_off) {
		int free_count;

		if (ipod_hw_ver > 0x3) {
			free_count = (inl(0x7000280c) & 0x3f0000) >> 16;
		} else {
			free_count = (inl(0xc000251c) & 0x7800000) >> 23;
		}

		if (free_count < 2) {
			/* enable interrupt */
			if (ipod_hw_ver > 0x3) {
				outl(inl(0x70002800) | 0x2, 0x70002800);
			} else {
				outl(inl(0xc000251c) | (1<<9), 0xc000251c);
			}

			return;
		}

		outl(((unsigned)dma_buf[*r_off]) << 16, ipod_i2s_fifo_out);
		if (!stereo) {
			outl(((unsigned)dma_buf[*r_off]) << 16, ipod_i2s_fifo_out);
		}

		*r_off = (*r_off + 1) % BUF_LEN;
	}

	*dma_active = 0;
}

static void ipodaudio_process_rec_dma(void)
{
	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	volatile int *dma_active = (int *)DMA_ACTIVE;
	volatile unsigned short *dma_buf = (unsigned short *)DMA_BASE;
	int stereo = *ipodaudio_stereo;

	inl(0xcf001040);
	outl(inl(0xc000251c) & ~(1<<14), 0xc000251c);

	while (*dma_active && ((inl(0xc000251c) & 0x78000000)>>27) < 8) {
		dma_buf[*w_off] = (unsigned short)(inl(0xc0002580) >> 8);
		if ( !stereo ) {
			/* throw away second sample */
			inl(0xc0002580);
		}

		*w_off = (*w_off + 1) % BUF_LEN;

		/* check for buffer over run */
		if ( *r_off == *w_off ) {
			*r_off = (*r_off + 1) % BUF_LEN;
		}
	}

	if (*dma_active) {
		outl(inl(0xc000251c) | (1<<14), 0xc000251c);
	}
}

static int ipodaudio_open(struct inode *inode, struct file *filep)
{
	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	volatile int *dma_active = (int *)DMA_ACTIVE;

	if ( ipodaudio_isopen ) {
		return -EBUSY;
	}

	/* initialise shared variables */
	*r_off = 0;
	*w_off = 0;
	*dma_active = 0;

	ipodaudio_isopen = 1;
	ipod_sample_rate = 44100;
	*ipodaudio_stereo = 1;

	if (ipod_hw_ver > 0x3) {
		/* I2S soft reset */
		outl(inl(0x70002800) | 0x80000000, 0x70002800);
		outl(inl(0x70002800) & ~0x80000000, 0x70002800);

		/* Fifo.Format = 32 bit LSB mode */
		outl(inl((0x70002800) & ~70) | 0x30, 0x70002800);
		/* TX.EN = RX.EN = disable */
		outl(inl(0x70002800) & ~0x30000000, 0x70002800);
		/* Clear TX and RX FIFOs */
		outl(inl(0x7000280c) | 0x1100, 0x7000280c);

		/* Data Bit interface Mode == 16 bit */
		outl(inl(0x70002800) & ~0x300, 0x70002800);
		/* Data Bit Interface Format == I2S */
		outl(inl(0x70002800) & ~0xc00, 0x70002800);
	}

	if (filep->f_mode & FMODE_WRITE) {
		d2a_set_power(D2A_POWER_ON);

		if (ipod_hw_ver == 0x3) {
			outl(inl(0xcf000004) & ~0xf, 0xcf000004);
		}

		ipod_set_process_dma(ipodaudio_process_pb_dma);
		if (ipod_hw_ver > 0x3) {
			int i, free_fifo_slots;

			/* flush fifo out */
			for (i = 0; i < 16; i++) {
				outl(0x0, 0x70002840);
			}

			/* enable output fifo */
			outl(inl(0x70002800) | 0x20000000, 0x70002800);

			i = 10000;
			while ((inl(0x70002804) & 0x80000000) == 0 && i--);

			free_fifo_slots = (inl(0x7000280c) & 0x3f0000) >> 16;
			for (i = 1; i < free_fifo_slots; i++) {
				outl(0x0, 0x70002840);
			}

			outl(inl(0x6000403c) | PP5020_I2S_MASK, 0x6000403c);
			outl(PP5020_I2S_MASK, 0x60004034);
		}
		else {
			outl(inl(0xcf00103c) | (1 << PP5002_DMA_OUT_IRQ) , 0xcf00103c);
			outl((1 << PP5002_DMA_OUT_IRQ), 0xcf001034);
		}
	}

	if (filep->f_mode & FMODE_READ) {
		/* 3g recording */
		if (ipod_hw_ver != 0x3) {
			return -ENODEV;
		}

		outl(inl(0xcf000004) & ~0xf, 0xcf000004);
		outl(inl(0xcf004044) & ~0xf, 0xcf004044);

		d2a_set_power(D2A_POWER_ON);

		d2a_set_active(0x0);

		/* set BCLKINV=0(Dont invert BCLK) MS=1(Enable Master) LRSWAP=0
		   LRP=0 IWL=10(24 bit) FORMAT=00(MSB-First,right justified) */
		/* MS IWL=24bit FORMAT=MSB */
		ipod_i2c_send(0x1a, 0xe, 0x48);

		d2a_set_active(0x1);

		if (ipod_active_rec == SOUND_MASK_LINE) {
			d2a_activate_linein();
		}
		else if (ipod_active_rec == SOUND_MASK_MIC) {
			d2a_activate_mic();
		}
	}

	return 0;
}

static void ipodaudio_txdrain(void)
{
	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	volatile int *dma_active = (int *)DMA_ACTIVE;

	if (!*dma_active) {
		return;
	}

	while (*r_off != *w_off) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(2);

		if (signal_pending(current)) {
			break;
		}
	}
}

static int ipodaudio_close(struct inode *inode, struct file *filep)
{
	if (filep->f_mode & FMODE_WRITE) {
		ipodaudio_txdrain();

		if (ipod_hw_ver > 0x3) {
			outl(inl(0x70002800) & ~0x20000000, 0x70002800);
			outl(inl(0x70002800) & ~0x2, 0x70002800);

			outl(PP5020_I2S_MASK, 0x60004038);
		}
		else {
			outl((1 << PP5002_DMA_OUT_IRQ), 0xcf001038);
		}

		ipod_set_process_dma(0);
	}

	if (filep->f_mode & FMODE_READ) {
		volatile int *dma_active = (int *)DMA_ACTIVE;

		*dma_active = 0;	/* tell COP dma to exit */
		outl(inl(0xc000251c) & ~(1<<14), 0xc000251c);
		outl((1 << PP5002_DMA_IN_IRQ), 0xcf001038);
		ipod_set_process_dma(0);
	}


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

		write_off_next = (write_off_current + 1) % BUF_LEN;

		read_off_current = *r_off;

		/* buffer full? */
		if ( write_off_next == read_off_current ) {
			/* buffer is full */
			set_current_state(TASK_INTERRUPTIBLE);

			/* sleep a little */
			schedule_timeout(2);
		}

		if ( read_off_current <= write_off_current ) {
			/* room at end of buffer? */
			cnt = BUF_LEN - 1 - write_off_current;
			if ( read_off_current > 0 ) cnt++;

			if ( cnt > 0 )  {
				if ( cnt > rem ) cnt = rem;

				memcpy((void*)&dma_buf[write_off_current], bufsp, cnt<<1);

				rem -= cnt;
				bufsp += cnt;

				write_off_current += cnt;
			}

			/* room at start of buffer (and more data)? */
			if ( read_off_current > 0 && rem > 0 ) {
				int n;

				if ( rem >= read_off_current ) {
					n = read_off_current - 1;
				}
				else {
					n = rem;
				}

				memcpy((void*)&dma_buf[0], bufsp, n<<1);

				rem -= n;
				bufsp += n;

				write_off_current = n;
			}
		}
		else if ( read_off_current > write_off_current ) {
			cnt = read_off_current - 1 - write_off_current;
			if ( cnt > rem ) cnt = rem;

			memcpy((void*)&dma_buf[write_off_current], bufsp, cnt<<1);

			bufsp += cnt;
			rem -= cnt;

			write_off_current += cnt;
		}

		*w_off = write_off_current;

		if ( !*dma_active ) {
			*dma_active = 1;

			if (ipod_hw_ver > 0x3) {
				outl(inl(0x70002800) | 0x2, 0x70002800);
			}
			else {
				outl(inl(0xc000251c) | (1<<9), 0xc000251c);
			}
		}
	}

	return count;
}

static ssize_t ipodaudio_read(struct file *filp, char *buf, size_t count, loff_t *ppos)
{
	unsigned short *bufsp;
	size_t rem;

	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	volatile int *dma_active = (int *)DMA_ACTIVE;
	volatile unsigned short *dma_buf = (unsigned short *)DMA_BASE;

	if ( !*dma_active ) {
		*dma_active = 1;

		*r_off = 0;
		*w_off = 0;

		ipod_set_process_dma(ipodaudio_process_rec_dma);
		outl(inl(0xcf00103c) | (1 << PP5002_DMA_IN_IRQ) , 0xcf00103c);
		outl((1 << PP5002_DMA_IN_IRQ), 0xcf001034);

		outl(inl(0xc000251c) | 0x20000, 0xc000251c);
		outl(inl(0xc000251c) & ~0x20000, 0xc000251c);

		outl(inl(0xc000251c) | (1<<14), 0xc000251c);
	}

	bufsp = (unsigned short *)buf;
	rem = count/2;

	while ( rem > 0 ) {
		int write_pos = *w_off;
		int read_pos = *r_off;
		int len = 0;

		if ( read_pos < write_pos ) {
			/* read data between read pos and write pos */
			len = write_pos - read_pos;
		}
		else if ( write_pos < read_pos ) {
			/* read data to end of buffer */
			/* next loop iteration will read the rest */
			len = BUF_LEN - read_pos;
		}
		else {
			/* buffer is empty */
			set_current_state(TASK_INTERRUPTIBLE);

			/* sleep a little */
			schedule_timeout(2);
		}

		if ( len > rem ) {
			len = rem;
		}

		if (len) {
			memcpy(bufsp, (void*)&dma_buf[read_pos], len<<1);

			bufsp += len;
			rem -= len;

			/* check for buffer over run */
			if ( read_pos == *r_off ) {
				*r_off = (*r_off + len) % BUF_LEN;
			}
			else {
				printk(KERN_ERR "ADC buffer overrun\n");
			}
		}

		if (signal_pending(current)) {
			set_current_state(TASK_RUNNING);
			return count - (rem * 2);
		}
	}

	set_current_state(TASK_RUNNING);

	return count;
}

static int ipod_getspace_ioctl(void *arg)
{
	audio_buf_info abinfo;
	int read_pos = *(int *)DMA_READ_OFF;
	int write_pos = *(int *)DMA_WRITE_OFF;
	int len;

	if (read_pos == write_pos) {
		len = BUF_LEN - 1;	/* ring buffer empty */
	}
	else if (read_pos < write_pos) {
		len = BUF_LEN - 1 - (write_pos - read_pos);
	} else {
		int next_write_pos = (write_pos + 1) % BUF_LEN;

		len = read_pos - next_write_pos;
	}

	abinfo.bytes = len * 2;
	abinfo.fragsize = BUF_LEN / 2;
	abinfo.fragments = abinfo.bytes / abinfo.fragsize;

	return copy_to_user(arg, &abinfo, sizeof(abinfo)) ? -EFAULT : 0;
}

static int ipodaudio_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	int val = 0;

	switch (cmd) {
	case SNDCTL_DSP_SPEED:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if ( rc == 0 ) {
			get_user(val, (int *) arg);

			val = d2a_set_sample_rate(val);

			put_user(val, (int *) arg);
		}
		break;

	case SNDCTL_DSP_GETFMTS:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if ( rc == 0 ) {
			put_user(AFMT_S16_LE, (int *) arg);
		}
		break;

	case SNDCTL_DSP_SETFMT:
	/* case SNDCTL_DSP_SAMPLESIZE: */
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if ( rc == 0 ) {
			get_user(val, (int *) arg);
			if ( val != AFMT_S16_LE ) {
				put_user(AFMT_S16_LE, (int *) arg);
			}
		}
		break;

	case SNDCTL_DSP_STEREO:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if ( rc == 0 ) {
			get_user(val, (int *) arg);
			if ( val != 0 && val != 1 ) {
				put_user(1, (int *) arg);
			}
			else {
				*ipodaudio_stereo = val;
			}
		}
		break;

	case SNDCTL_DSP_CHANNELS:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if ( rc == 0 ) {
			get_user(val, (int *) arg);
			if (val > 2) {
				val = 2;
			}
			*ipodaudio_stereo = (val == 2);
			put_user(val, (int *) arg);
		}
		break;

	case SNDCTL_DSP_GETBLKSIZE:
		rc = verify_area(VERIFY_WRITE, (void *) arg, sizeof(long));
		if ( rc == 0 ) {
			put_user(BUF_LEN/2, (int *) arg);
		}
		break;

	case SNDCTL_DSP_SYNC:
		rc = 0;
		ipodaudio_txdrain();
		break;

	case SNDCTL_DSP_RESET:
		rc = 0;
		break;

	case SNDCTL_DSP_GETOSPACE:
		return ipod_getspace_ioctl((void *)arg);
	}

	return rc;
}

static struct file_operations ipod_dsp_fops = {
	owner: THIS_MODULE,
	llseek:	no_llseek,
	open: ipodaudio_open,
	release: ipodaudio_close,
	write: ipodaudio_write,
	read: ipodaudio_read,
	ioctl: ipodaudio_ioctl,
};

static int ipod_mixer_open(struct inode *inode, struct file *filep)
{
	return 0;
}

static int ipod_mixer_close(struct inode *inode, struct file *filep)
{
	return 0;
}

static int ipod_mixer_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (cmd == SOUND_MIXER_INFO) {
		mixer_info info;

		strncpy(info.id, "WM8731", sizeof(info.id));
		strncpy(info.name, "Wolfson WM8731", sizeof(info.name));
		if (copy_to_user((void *) arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}

	if (_SIOC_DIR(cmd) == _SIOC_READ) {
		int val;

		switch (_IOC_NR(cmd)) {
		/* the devices which can be used as recording devices */
		case SOUND_MIXER_RECMASK:
			return put_user(SOUND_MASK_LINE | SOUND_MASK_MIC, (int *)arg);

		/* bit mask for each of the stereo channels */
		case SOUND_MIXER_STEREODEVS:
			return put_user(SOUND_MASK_PCM | SOUND_MASK_LINE, (int *)arg);

		/* bit mask for each of the supported channels */
		case SOUND_MIXER_DEVMASK:
			return put_user(SOUND_MASK_PCM | SOUND_MASK_LINE | SOUND_MASK_MIC, (int *)arg);

		/* bit mask which describes general capabilities of the mixer */
		case SOUND_MIXER_CAPS:
			/* only one mixer channel can be selected as a
			   recording source at any one time */
			return put_user(SOUND_CAP_EXCL_INPUT, (int *)arg);

		/* bit mask for each of the currently active recording sources */
		case SOUND_MIXER_RECSRC:
			return put_user(ipod_active_rec, (int *)arg);

		case SOUND_MIXER_PCM:		/* codec output level */
			val = (ipod_pcm_level - 0x2f) * 100 / 80;
			val = val << 8 | val;
			return put_user(val, (int *)arg);

		case SOUND_MIXER_LINE:	/* line-in jack */
			val = ipod_line_level * 100 / 31;
			val = val << 8 | val;
			return put_user(val, (int *)arg);

		case SOUND_MIXER_MIC:		/* microphone */
			/* 0 or +20dB (mic boost) plus mute */
			return put_user(ipod_mic_boost, (int *)arg);
		}
	}
	else {
		int val, left, right;

		if (get_user(val, (int *)arg)) {
			return -EFAULT;
		}

		switch (_IOC_NR(cmd)) {
		/* select the active recording sources 0 == mic */
		case SOUND_MIXER_RECSRC:
			if (val != ipod_active_rec) {
				if (val == SOUND_MASK_LINE) {
					d2a_activate_linein();
				}
				else if (val == SOUND_MASK_MIC) {
					d2a_activate_mic();
				}
				else {
					val = ipod_active_rec;
				}

				ipod_active_rec = val;
			}

			return put_user(val, (int *)arg);

		case SOUND_MIXER_PCM:		/* codec output level */
			/* +6 to -73dB 1dB steps (plus mute == 80levels) 7bits */
			/* 1111001 == 0dB */
			/* 0110000 == -73dB */
			/* 0101111 == mute (0x2f) */
			left = val & 0xff;
			right = (val >> 8) & 0xff;

			if (left > 100) left = 100;
			if (right > 100) right = 100;

			ipod_pcm_level = (left * 80 / 100) + 0x2f;
			if (ipod_hw_ver > 0x4) {
				right = (right * 80 / 100) + 0x2f;

				/* OUT1 */
				ipod_i2c_send(0x1a, 0x4, ipod_pcm_level);
				ipod_i2c_send(0x1a, 0x6 | 0x1, right);

				/* OUT2 */
				ipod_i2c_send(0x1a, 0x50, ipod_pcm_level);
				ipod_i2c_send(0x1a, 0x52 | 0x1, right);
			} else {
				if (left == right) {
					ipod_i2c_send(0x1a, 0x4 | 0x1, ipod_pcm_level);
				}
				else {
					right = (right * 80 / 100) + 0x2f;

					ipod_i2c_send(0x1a, 0x4, ipod_pcm_level);
					ipod_i2c_send(0x1a, 0x6, right);
				}
			}

			return put_user(val, (int *)arg);

		case SOUND_MIXER_LINE:	/* line-in jack */
			/* +12 to -34.5dB 1.5dB steps (plus mute == 32levels) 5bits + mute */
			/* 10111 == 0dB */
			left = val & 0xff;
			right = (val >> 8) & 0xff;

			if (left > 100) left = 100;
			if (right > 100) right = 100;

			if (left == right) {
				ipod_line_level = left * 31 / 100;
				if (ipod_line_level == 0) {
					ipod_i2c_send(0x1a, 0x0 | 0x1, 0x80);
				} else {
					ipod_i2c_send(0x1a, 0x0 | 0x1, ipod_line_level);
				}
			}
			else {
				ipod_line_level = left * 31 / 100;
				if (ipod_line_level == 0) {
					ipod_i2c_send(0x1a, 0x0, 0x80);
				} else {
					ipod_i2c_send(0x1a, 0x0, ipod_line_level);
				}
				right = right * 31 / 100;
				if (right == 0) {
					ipod_i2c_send(0x1a, 0x2, 0x80);
				} else {
					ipod_i2c_send(0x1a, 0x2, right);
				}
			}

			return put_user(val, (int *)arg);

		case SOUND_MIXER_MIC:		/* microphone */
			/* 0 or +20dB (mic boost) plus mute */
			if (val == 100) {
				/* enable mic boost */
				if (ipod_active_rec == SOUND_MASK_MIC) {
					ipod_i2c_send(0x1a, 0x8, 0x5);   /* INSEL=mic, MIC_BOOST */
				}
			}
			else {
				/* disable mic boost */
				val = 0;
				if (ipod_active_rec == SOUND_MASK_MIC) {
					ipod_i2c_send(0x1a, 0x8, 0x4);   /* INSEL=mic */
				}
			}
			ipod_mic_boost = val;

			return put_user(val, (int *)arg);
		}
	}

	return -EINVAL;
}


static struct file_operations ipod_mixer_fops = {
	owner: THIS_MODULE,
	llseek:	no_llseek,
	open: ipod_mixer_open,
	release: ipod_mixer_close,
	ioctl: ipod_mixer_ioctl,
};

static int __init ipodaudio_init(void)
{
	printk("ipodaudio: (c) Copyright 2003-2005 Bernard Leach <leachbj@bouncycastle.org>\n");

	dsp_devfs_handle = devfs_register(NULL, "dsp", DEVFS_FL_DEFAULT,
			SOUND_MAJOR, SND_DEV_DSP,
			S_IFCHR | S_IWUSR | S_IRUSR,
			&ipod_dsp_fops, NULL);
	if (dsp_devfs_handle < 0) {
		printk(KERN_WARNING "SOUND: failed to register major %d, minor %d\n",
			SOUND_MAJOR, SND_DEV_DSP);
		return 0;
	}
	mixer_devfs_handle = devfs_register(NULL, "mixer", DEVFS_FL_DEFAULT,
			SOUND_MAJOR, SND_DEV_CTL,
			S_IFCHR | S_IWUSR | S_IRUSR,
			&ipod_mixer_fops, NULL);
	if (mixer_devfs_handle < 0) {
		printk(KERN_WARNING "SOUND: failed to register major %d, minor %d\n",
			SOUND_MAJOR, SND_DEV_DSP);
		return 0;
	}

	ipod_hw_ver = ipod_get_hw_version() >> 16;
	if (ipod_hw_ver > 0x3) {
		/* reset I2C */
		ipod_i2c_init();

		/* device enable */
		outl(inl(0x6000600c) | 0x7, 0x6000600c);

		/* i2s_init */
		outl(inl(0x70000020) & ~0x300, 0x70000020);
		outl(inl(0x70000010) & ~0x3000000, 0x70000010);

		/* device enable */
		outl(inl(0x6000600c) | 0x800, 0x6000600c);

		/* device reset */
		outl(inl(0x60006004) | 0x800, 0x60006004);
		outl(inl(0x60006004) & ~0x800, 0x60006004);

		/* I2S soft reset */
		outl(inl(0x70002800) | 0x80000000, 0x70002800);
		outl(inl(0x70002800) & ~0x80000000, 0x70002800);

		/* disable ADC/DAC */
		outl(inl(0x70002800) | 0x30, 0x70002800);
		outl(inl(0x70002800) & ~0x30000000, 0x70002800);
		outl(inl(0x7000280c) | 0x1100, 0x7000280c);

		/* i2s_init end */
	}
	else if (ipod_hw_ver == 0x3) {
		/* reset I2C */
		ipod_i2c_init();

		/* reset DAC and ADC fifo */
		outl(inl(0xc000251c) | 0x10000, 0xc000251c);
		outl(inl(0xc000251c) | 0x20000, 0xc000251c);
		outl(inl(0xc000251c) & ~0x30000, 0xc000251c);

		/* enable ADC/DAC */
		outl(0xd, 0xc0002500);

		/* bits 11,10 == 01 */
		outl(inl(0xcf004040) | 0x400, 0xcf004040);
		outl(inl(0xcf004040) & ~0x800, 0xcf004040);

		outl(inl(0xcf004048) & ~0x1, 0xcf004048);
	}
	else if (ipod_hw_ver < 0x3) {
		/* reset I2C */
		ipod_i2c_init();

		/* reset DAC fifo */
		outl(inl(0xc000251c) | 0x10000, 0xc000251c);
		outl(inl(0xc000251c) & ~0x10000, 0xc000251c);

		/* enable DAC */
		outl(0x5, 0xc0002500);

		/* nb this is different to 3g!? */
		/* bits 11,10 == 10 */
		outl(inl(0xcf004040) & ~0x400, 0xcf004040);
		outl(inl(0xcf004040) | 0x800, 0xcf004040);
	}

	if (ipod_hw_ver <= 0x3) {
		/* GPIO D bit 6 enable for output */
		outl(inl(0xcf00000c) | 0x40, 0xcf00000c);
		outl(inl(0xcf00001c) & ~0x40, 0xcf00001c);
	}

	return 0;
}

static void __exit ipodaudio_exit(void)
{
	ipod_set_process_dma(0);

	devfs_unregister_chrdev(SOUND_MAJOR, "mixer");
	devfs_unregister(mixer_devfs_handle);

	devfs_unregister_chrdev(SOUND_MAJOR, "dsp");
	devfs_unregister(dsp_devfs_handle);
}

module_init(ipodaudio_init);
module_exit(ipodaudio_exit);

MODULE_AUTHOR("Bernard Leach <leachbj@bouncycastle.org>");
MODULE_DESCRIPTION("Audio driver for IPod");
MODULE_LICENSE("GPL");
