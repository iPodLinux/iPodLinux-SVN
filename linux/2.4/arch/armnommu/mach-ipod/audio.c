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

/* TODO */
#define PP5002_DMA_OUT_MASK	(1 << PP5002_DMA_OUT_IRQ)

#define D2A_POWER_OFF   1
#define D2A_POWER_SB    2
#define D2A_POWER_ON    3

/* length of shared buffer in half-words (starting at DMA_BASE) */
#define BUF_LEN		(40*1024)

static int ipodaudio_isopen;
// static int ipodaudio_power_state;
static unsigned ipod_hw_ver;
static devfs_handle_t dsp_devfs_handle, mixer_devfs_handle;
static int ipod_sample_rate = 44100;
static volatile int *ipodaudio_stereo = (int *)DMA_STEREO;
static int ipod_mic_boost = 100;
static int ipod_line_level = 0x17;	/* 0dB */
static int ipod_pcm_level = 0x65;	/* -6dB */
static int ipod_active_rec = SOUND_MASK_MIC;
static int codec_chip;

#define WM8721	1
#define WM8731	2
#define WM8975	3
#define WM8758	4






/* WM8758 Regs */

#define WM8758_RESET		0x0
#define WM8758_PWRMGMT1		0x1
#define WM8758_PWRMGMT2		0x2
#define WM8758_PWRMGMT3		0x3
#define WM8758_AUDIOIFCE	0x4
#define WM8758_CLOCKCTRL	0x6
#define WM8758_SRATECTRL	0x7

#define WM8758_DACCTRL		0xA
#define WM8758_LDACVOL		0xB
#define WM8758_RDACVOL		0xC
#define WM8758_JDTCTCTRL1	0x9
#define WM8758_JDTCTCTRL2	0xD
#define WM8758_OUTPUTCTRL	0x31
#define WM8758_LMIXCTRL		0x32
#define WM8758_RMIXCTRL		0x33
#define WM8758_LOUT1VOL		0x34
#define WM8758_ROUT1VOL		0x35
#define WM8758_LOUT2VOL		0x36
#define WM8758_ROUT2VOL		0x37

#define WM8758_PLLN		0x24
#define WM8758_PLLK1		0x25
#define WM8758_PLLK2		0x26
#define WM8758_PLLK3		0x27

static void wm_write(uint8_t reg, uint8_t data_high, uint8_t data_low)
{
	ipod_i2c_send(0x1A, (reg << 1) | (data_high & 0x1), data_low);
}


static void codec_set_active(int active)
{
	if (codec_chip < WM8975) {
		/* set active to 0x0 or 0x1 */
		if (active) {
			ipod_i2c_send(0x1a, 0x12, 0x01);
		} else {
			ipod_i2c_send(0x1a, 0x12, 0x00);
		}
	}
}

static int codec_set_sample_rate(int rate)
{
	int sampling_control;
	if (codec_chip < WM8758)
	{
		if (rate <= 8000) {
			if (codec_chip >= WM8731) {
			/* set CLKIDIV2=1 SR=0011 BOSR=0 USB/NORM=1 (USB) */
				sampling_control = 0x4d;
			} else {
			/* set CLKIDIV2=1 SR=0001 BOSR=0 USB/NORM=1 (USB) */
				sampling_control = 0x45;
			}
			rate = 8000;
		} else if (rate <= 12000 && codec_chip >= WM8975) {
			/* set CLKIDIV2=1 SR=1000 BOSR=0 USB/NORM=1 (USB) */
			sampling_control = 0x61;
			rate = 12000;
		} else if (rate <= 16000 && codec_chip >= WM8975) {
			/* set CLKIDIV2=1 SR=1010 BOSR=1 USB/NORM=1 (USB) */
			sampling_control = 0x55;
			rate = 16000;
		} else if (rate <= 22050 && codec_chip >= WM8975) {
			/* set CLKIDIV2=1 SR=1101 BOSR=1 USB/NORM=1 (USB) */
			sampling_control = 0x77;
			rate = 22050;
		} else if (rate <= 24000 && codec_chip >= WM8975) {
			/* set CLKIDIV2=1 SR=1110 BOSR=0 USB/NORM=1 (USB) */
			sampling_control = 0x79;
			rate = 24000;
		} else if (rate <= 32000) {
			/* set CLKIDIV2=1 SR=0110 BOSR=0 USB/NORM=1 (USB) */
			sampling_control = 0x59;
			rate = 32000;
		} else if (rate <= 44100) {
			/* set CLKIDIV2=1 SR=1000 BOSR=1 USB/NORM=1 (USB) */
			sampling_control = 0x63;
			rate = 44100;
		} else if (rate <= 48000) {
			/* set CLKIDIV2=1 SR=0000 BOSR=0 USB/NORM=1 (USB) */
			sampling_control = 0x41;
			rate = 48000;
		} else if (rate <= 88200) {
			/* set CLKIDIV2=1 SR=1111 BOSR=1 USB/NORM=1 (USB) */
			sampling_control = 0x7f;
			rate = 88200;
		} else {
			/* set for 96kHz */
			/* set CLKIDIV2=1 SR=0111 BOSR=0 USB/NORM=1 (USB) */
			sampling_control = 0x5d;
			rate = 96000;
		}
	
		codec_set_active(0x0);
		ipod_i2c_send(0x1a, 0x10, sampling_control);
		codec_set_active(0x1);

		ipod_sample_rate = rate;

		return ipod_sample_rate;
	} else {
		/* 0 = 11.2896  1 = 12.288 */
		uint8_t sysclock = 0;
		uint16_t srate, mclkdiv;	
		
		if (rate <= 8000) {
			rate = 8000;
			mclkdiv = 7;
			srate = 2;
			sysclock=1;	

		} else if (rate <= 12000) {
			rate = 12000;
			mclkdiv = 6;
			srate = 4;
			sysclock=1;
		} else if (rate <= 16000) {
			rate = 16000;
			mclkdiv = 5;
			srate = 3;
			sysclock = 1;
		} else if (rate <= 22050) {
			rate = 22050;
			mclkdiv = 4;
			srate = 2;
			sysclock = 0;
		} else if (rate <= 24000) {
			rate = 24000;
			mclkdiv = 4;	
			srate = 2;
			sysclock = 1;
		} else if (rate <= 32000) {
			rate = 32000;
			mclkdiv = 3;
			sysclock = 1;
			srate = 1;
		} else if (rate <= 44100) {
			rate = 44100;
			mclkdiv = 2;
			sysclock = 0;
			srate = 0;
		} else if (rate <= 48000) {
			rate = 48000;
			mclkdiv = 2;
			sysclock = 1;
			srate =	0;
		} else if (rate <= 88200) {
			rate = 88200;
			mclkdiv = 0;
			sysclock = 0;
			srate = 0;

		} else {
			rate = 96000;
			mclkdiv = 0;
			sysclock = 1;
			srate = 0;

		}
		

		/* set clock div */
		wm_write(WM8758_CLOCKCTRL, 0, 1 | (0 << 2) | (2 << 5));
		if (sysclock == 0) {

			/* setup PLL for MHZ=11.2896 */
			wm_write(WM8758_PLLN, 0, (1 << 4) | 0x7);
			wm_write(WM8758_PLLK1, 0, 0x21);
			wm_write(WM8758_PLLK2, 1, 0x61);
			wm_write(WM8758_PLLK3, 0, 0x26);
		} else {

			/* setup PLL for MHZ=12.288 */
			wm_write(WM8758_PLLN, 0, (1 << 4) | 0x8);
			wm_write(WM8758_PLLK1, 0, 0xC);
			wm_write(WM8758_PLLK2, 0, 0x93);
			wm_write(WM8758_PLLK3, 0, 0xE9);
		}


		/* set clock div */
		wm_write(WM8758_CLOCKCTRL, 1, 1 | (1 << 2) | (mclkdiv << 5));

		/* set srate */
		wm_write(WM8758_SRATECTRL, 0, (srate << 1));
	


		ipod_sample_rate = rate;

		return ipod_sample_rate;

	}
}




/*
 * Init the WM8758 for playback via headphone and line out.
 * I'm using the WM8983 datasheet - seems to be right.
 */
static void codec_wm8758_init_pb(void)
{
	/* reset the chip to a known state */
	wm_write(WM8758_RESET, 0, 0x1);	



	/* Set Master Mode, Clock from pll base for 44100HZ */
	wm_write(WM8758_CLOCKCTRL, 1, 1 | (1 << 2) | (0 << 5));

	/* Enable LOUT1, ROUT1 */
	wm_write(WM8758_PWRMGMT2, 1, (1 << 7));

	/* Enable LOUT2, ROUT2, RMIX, LMIX, DACR, DACL */
	wm_write(WM8758_PWRMGMT3, 0, (0x3 << 5) | (0x3 << 2) | 0x3);

	/* FORMAT=10 (I2S)  WL=00 (16 bit data width) */
	wm_write(WM8758_AUDIOIFCE, 0, (1 << 4));
	
	/* set the sample rate */	
	codec_set_sample_rate(ipod_sample_rate);

	/* VMID=11, BIASEN=1 PLLon */
	wm_write(WM8758_PWRMGMT1, 0, 0x3 | (1 << 3) | (1 << 5));

	/* output control - enable DACs to Mixer paths - enable VREF */
	wm_write(WM8758_OUTPUTCTRL, 0, (0x3 << 5) | 1);

	/* unmute (SOFTMUTE = 0)  
	 * NOTE: This is contradicting in the doc - but SOFTMUTE=0 
	 * disables mute 
	 */
	wm_write(WM8758_DACCTRL, 0, 0x0);

}



/*
 * Initialise the WM8975 for playback via headphone and line out.
 * Note, I'm using the WM8750 datasheet as its apparently close.
 */
static void codec_wm8975_init_pb(void)
{
	/*
	 * 1. Switch on power supplies.
	 *    By default the WM8750L is in Standby Mode, the DAC is
	 *    digitally muted and the Audio Interface, Line outputs
	 *    and Headphone outputs are all OFF (DACMU = 1 Power
	 *    Management registers 1 and 2 are all zeros).
	 */
	ipod_i2c_send(0x1a, 0x1f, 0xff);	/*Reset*/
	ipod_i2c_send(0x1a, 0x1e, 0x0);

	 /* 2. Enable Vmid and VREF. */
	ipod_i2c_send(0x1a, 0x32, 0xc0);	/*Pwr Mgmt(1)*/

	 /* 3. Enable DACs as required. */
	ipod_i2c_send(0x1a, 0x34 | 0x1, 0x80);	/*Pwr Mgmt(2)*/

	 /* 4. Enable line and / or headphone output buffers as required. */
	ipod_i2c_send(0x1a, 0x34 | 0x1, 0xf8);	/*Pwr Mgmt(2)*/

	/* BCLKINV=0(Dont invert BCLK) MS=1(Enable Master) LRSWAP=0 LRP=0 */
       	/* IWL=00(16 bit) FORMAT=10(I2S format) */
	ipod_i2c_send(0x1a, 0xe, 0x42);


	codec_set_sample_rate(ipod_sample_rate);

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

	/* 5. Set DACMU = 0 to soft-un-mute the audio DACs. */
	ipod_i2c_send(0x1a, 0xa, 0x0);
}

static void codec_wm8975_init_mic(void)
{
	/*
	 * 1. Switch on power supplies.
	 *    By default the WM8750L is in Standby Mode, the DAC is
	 *    digitally muted and the Audio Interface, Line outputs
	 *    and Headphone outputs are all OFF (DACMU = 1 Power
	 *    Management registers 1 and 2 are all zeros).
	 */
	ipod_i2c_send(0x1a, 0x1f, 0xff);	/*Reset*/
	ipod_i2c_send(0x1a, 0x1e, 0x0);

	 /* 2. Enable Vmid and VREF. */
	ipod_i2c_send(0x1a, 0x32, 0xc0);	/*Pwr Mgmt(1)*/

	 /* 3. Enable ADCs as required. */
	ipod_i2c_send(0x1a, 0x32, 0xcc);	/*Pwr Mgmt(1)*/
	ipod_i2c_send(0x1a, 0x34 | 0x1, 0x80);	/*Pwr Mgmt(2)*/

	 /* 4. Enable line and / or headphone output buffers as required. */
	ipod_i2c_send(0x1a, 0x32, 0xfc);	/*Pwr Mgmt(1)*/

	/* BCLKINV=0(Dont invert BCLK) MS=1(Enable Master) LRSWAP=0 LRP=0 */
       	/* IWL=00(16 bit) FORMAT=10(I2S format) */
	ipod_i2c_send(0x1a, 0xe, 0x42);

	codec_set_sample_rate(ipod_sample_rate);

	/* unmute inputs */
	ipod_i2c_send(0x1a, 0x1, 0x17);		/* LINVOL (def 0dB) */
	ipod_i2c_send(0x1a, 0x3, 0x17);		/* RINVOL (def 0dB) */

	ipod_i2c_send(0x1a, 0x2b, 0xd7);	/* LADCVOL max vol x was ff */
	ipod_i2c_send(0x1a, 0x2d, 0xd7);	/* RADCVOL max vol x was ff */

	/* VSEL=10(def) DATSEL=10 (use right ADC only) */
	ipod_i2c_send(0x1a, 0x2e, 0xc8);	/* Additional control(1) */

	/* VROI=1 (sets output resistance to 40kohms) */
	ipod_i2c_send(0x1a, 0x36, 0x40); 	/* Additional control(3) */

	/* LINSEL=1 (LINPUT2) LMICBOOST=10 (20dB boost) */
	ipod_i2c_send(0x1a, 0x40, 0x60);	/* ADCL signal path */
	ipod_i2c_send(0x1a, 0x42, 0x60);	/* ADCR signal path */
}

static void codec_wm8731_init_pb(void)
{
	ipod_i2c_send(0x1a, 0x1e, 0x0);		/*Reset*/

	codec_set_active(0x0);

	/* DACSEL=1 */
	if (codec_chip == WM8721) {
		ipod_i2c_send(0x1a, 0x8, 0x10);
	} else if (codec_chip == WM8731) {
		/* BYPASS=1 */
		ipod_i2c_send(0x1a, 0x8, 0x18);
	}

	/* set power register to POWEROFF=0 on OUTPD=0, DACPD=0 */
	ipod_i2c_send(0x1a, 0xc, 0x67);

	/* BCLKINV=0(Dont invert BCLK) MS=1(Enable Master) LRSWAP=0 LRP=0 */
       	/* IWL=00(16 bit) FORMAT=10(I2S format) */
	ipod_i2c_send(0x1a, 0xe, 0x42);

	codec_set_sample_rate(ipod_sample_rate);

	/* set the volume to -6dB */
	ipod_i2c_send(0x1a, 0x4, ipod_pcm_level);
	ipod_i2c_send(0x1a, 0x6 | 0x1, ipod_pcm_level);

	/* ACTIVE=1 */
	codec_set_active(1);

	/* 5. Set DACMU = 0 to soft-un-mute the audio DACs. */
	ipod_i2c_send(0x1a, 0xa, 0x0);
}

static void codec_init_pb(void)
{
	switch (codec_chip) {
	case WM8721:
	case WM8731:
		codec_wm8731_init_pb();
		break;
	case WM8975:
		codec_wm8975_init_pb();
		break;
	case WM8758:
		codec_wm8758_init_pb();
		break;
	}
}

static void codec_wm8731_init_mic(void)
{
	codec_set_active(0x0);

	/* set BCLKINV=0(Dont invert BCLK) MS=1(Enable Master) LRSWAP=0
	 * LRP=0 IWL=10(24 bit) FORMAT=00(MSB-First,right justified) */
	ipod_i2c_send(0x1a, 0xe, 0x48);

	ipod_i2c_send(0x1a, 0x0 | 0x1, 0x80);  /* line in mute left & right */
	ipod_i2c_send(0x1a, 0x4 | 0x1, 0x0);   /* headphone mute left & right */

	if (ipod_mic_boost) {
		ipod_i2c_send(0x1a, 0x8, 0x5);   /* INSEL=mic, MIC_BOOST */
	} else {
		ipod_i2c_send(0x1a, 0x8, 0x4);   /* INSEL=mic */
	}

	/* disable ADC high pass filter, mute dac */
	ipod_i2c_send(0x1a, 0xa, 0x9);

	/* power on (PWR_OFF=0) */
       	/* CLKOUTPD OSCPD OUTPD DACPD LINEINPD */
	ipod_i2c_send(0x1a, 0xc, 0x79);

	codec_set_active(0x1);
}

static void codec_activate_mic(void)
{
	switch (codec_chip) {
	case WM8721:
		/* no recording with this device */
		return;
	case WM8731:
		codec_wm8731_init_mic();
		break;
	case WM8975:
		codec_wm8975_init_mic();
		break;
	case WM8758:
		/* TODO: implement */
		break;
	}
}

static void codec_wm8731_init_linein(void)
{
	codec_set_active(0x0);

	/* set BCLKINV=0(Dont invert BCLK) MS=1(Enable Master) LRSWAP=0
	 * LRP=0 IWL=10(24 bit) FORMAT=00(MSB-First,right justified) */
	ipod_i2c_send(0x1a, 0xe, 0x48);

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

	codec_set_active(0x1);
}

static void codec_activate_linein(void)
{
	switch (codec_chip) {
	case WM8721:
		/* no recording with this device */
		return;
	case WM8731:
		codec_wm8731_init_linein();
		break;
	case WM8975:
		// TODO
		codec_wm8975_init_mic();
		break;
	case WM8758:
		/* TODO: implement */
		break;
	}
}

static void codec_wm8758_deinit(void)
{

	/* Softmute DAC */
	wm_write(WM8758_DACCTRL, 0, (1 << 6));

	/* output + dac disables */
	wm_write(WM8758_PWRMGMT3, 0, 0x00);

	/* output diable */
	wm_write(WM8758_PWRMGMT1, 0, 0x00);

	/* output + disable + SLEEP MODE */
	wm_write(WM8758_PWRMGMT2, 0, (1 << 6));

}
static void codec_wm8975_deinit(void)
{
	/* 1. Set DACMU = 1 to soft-mute the audio DACs. */
	ipod_i2c_send(0x1a, 0xa, 0x8);

	/* 2. Disable all output buffers. */
	ipod_i2c_send(0x1a, 0x34, 0x0);	/*Pwr Mgmt(2)*/

	/* 3. Switch off the power supplies. */
	ipod_i2c_send(0x1a, 0x32, 0x0);	/*Pwr Mgmt(1)*/
}

static void codec_wm8731_deinit(void)
{
	/* set DACMU=1 DEEMPH=0 */
	ipod_i2c_send(0x1a, 0xa, 0x8);

	/* ACTIVE=0 */
	codec_set_active(0x0);

	/* line in mute left & right*/
	ipod_i2c_send(0x1a, 0x0 | 0x1, 0x80);

	/* set DACSEL=0, MUTEMIC=1 */
	ipod_i2c_send(0x1a, 0x8, 0x2);

	/* set POWEROFF=0 OUTPD=0 DACPD=1 */
	ipod_i2c_send(0x1a, 0xc, 0x6f);

	/* set POWEROFF=1 OUTPD=1 DACPD=1 */
	ipod_i2c_send(0x1a, 0xc, 0xff);
}

static void codec_deinit(void)
{
	switch (codec_chip) {
	case WM8721:
	case WM8731:
		codec_wm8731_deinit();
		break;
	case WM8975:
		codec_wm8975_deinit();
		break;
	case WM8758:
		codec_wm8758_deinit();
		break;
	}
}

static void i2s_pp5002_pb_dma(void)
{
	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	volatile int *dma_active = (int *)DMA_ACTIVE;
	volatile unsigned short *dma_buf = (unsigned short *)DMA_BASE;
	int stereo = *ipodaudio_stereo;
	int free_count;

	inl(0xcf001040);
	outl(inl(0xc000251c) & ~(1<<9), 0xc000251c);

repeat:
	while (*r_off != *w_off) {
		free_count = (inl(0xc000251c) & 0x7800000) >> 23;

		if (free_count < 2) {
			/* enable interrupt */
			outl(inl(0xc000251c) | (1<<9), 0xc000251c);

			return;
		}

		outl(((unsigned)dma_buf[*r_off]) << 16, 0xc0002540);
		if (!stereo) {
			outl(((unsigned)dma_buf[*r_off]) << 16, 0xc0002540);
		}

		/* enable playback fifo */
		outl(inl(0xc0002500) | 0x4, 0xc0002500);

		*r_off = (*r_off + 1) % BUF_LEN;
	}

	/* wait for fifo to empty */
	while ((inl(0xc000251c) & 0x1) == 0) {
		if (*r_off != *w_off) {
			goto repeat;
		}
	}

	/* disable playback fifo */
	outl(inl(0xc0002500) & ~0x4, 0xc0002500);

	*dma_active = 0;
}

static void i2s_pp5020_pb_dma(void)
{
	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	volatile int *dma_active = (int *)DMA_ACTIVE;
	volatile unsigned short *dma_buf = (unsigned short *)DMA_BASE;
	int stereo = *ipodaudio_stereo;
	int free_count;

	outl(inl(0x70002800) & ~0x2, 0x70002800);

repeat:
	while (*r_off != *w_off) {
		free_count = (inl(0x7000280c) & 0x3f0000) >> 16;

		if (free_count < 2) {
			/* enable interrupt */
			outl(inl(0x70002800) | 0x2, 0x70002800);

			return;
		}

		outl(((unsigned)dma_buf[*r_off]) << 16, 0x70002840);
		if (!stereo) {
			outl(((unsigned)dma_buf[*r_off]) << 16, 0x70002840);
		}

		/* enable playback fifo */
		outl(inl(0x70002800) | 0x20000000, 0x70002800);

		*r_off = (*r_off + 1) % BUF_LEN;
	}

	/* wait for fifo to empty */
	while ((inl(0x70002804) & 0x80000000) == 0) {
		if (*r_off != *w_off) {
			goto repeat;
		}
	}

	/* disable playback fifo */
	outl(inl(0x70002800) & ~0x20000000, 0x70002800);

	*dma_active = 0;
}

static void i2s_pp5002_rec_dma(void)
{
	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	volatile int *dma_active = (int *)DMA_ACTIVE;
	volatile unsigned short *dma_buf = (unsigned short *)DMA_BASE;
	int stereo = *ipodaudio_stereo;

	inl(0xcf001040);
	outl(inl(0xc000251c) & ~(1<<14), 0xc000251c);

	while (*dma_active != 2) {
	       	int empty_count = (inl(0xc000251c) & 0x78000000) >> 27;
		if (empty_count > 14) {
			/* enable interrupt */
			outl(inl(0xc000251c) | (1<<14), 0xc000251c);
			return;
		}

		dma_buf[*w_off] = (unsigned short)(inl(0xc0002580) >> 8);
		if (!stereo) {
			/* throw away second sample */
			inl(0xc0002580);
		}

		*w_off = (*w_off + 1) % BUF_LEN;

		/* check for buffer over run */
		if (*r_off == *w_off) {
			*r_off = (*r_off + 1) % BUF_LEN;
		}
	}

	/* disable fifo */
	outl(inl(0xc0002500) & ~0x8, 0xc0002500);

	/* tell the cpu we are no longer active */
	*dma_active = 3;
}

static void i2s_pp5020_rec_dma(void)
{
	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	volatile int *dma_active = (int *)DMA_ACTIVE;
	volatile unsigned short *dma_buf = (unsigned short *)DMA_BASE;
	int stereo = *ipodaudio_stereo;

	outl(inl(0x70002800) & ~0x1, 0x70002800);

	while (*dma_active != 2) {
		int full_count = (inl(0x7000280c) & 0x3f000000) >> 24;
		if (full_count < 2) {
			/* enable interrupt */
			outl(inl(0x70002800) | 0x1, 0x70002800);
			return;
		}

		dma_buf[*w_off] = (unsigned short)(inl(0x70002880) >> 16);
		if (!stereo) {
			/* throw away second sample */
			inl(0x70002880);
		}

		*w_off = (*w_off + 1) % BUF_LEN;

		/* check for buffer over run */
		if (*r_off == *w_off) {
			*r_off = (*r_off + 1) % BUF_LEN;
		}
	}

	/* disable fifo */
	outl(inl(0x70002800) & ~0x10000000, 0x70002800);

	/* tell the cpu we are no longer active */
	*dma_active = 3;
}

/*
 * Reset the I2S BIT.FORMAT I2S, 16bit, FIFO.FORMAT 32bit
 */
static void i2s_reset(void)
{
	if (ipod_hw_ver > 0x3) {
		/* PP502x */

		/* I2S soft reset */
		outl(inl(0x70002800) | 0x80000000, 0x70002800);
		outl(inl(0x70002800) & ~0x80000000, 0x70002800);

		/* BIT.FORMAT [11:10] = I2S (default) */
		outl(inl(0x70002800) & ~0xc00, 0x70002800);
		/* BIT.SIZE [9:8] = 16bit (default) */
		outl(inl(0x70002800) & ~0x300, 0x70002800);

		/* FIFO.FORMAT [6:4] = 32 bit LSB */
		/* since BIT.SIZ < FIFO.FORMAT low 16 bits will be 0 */
		outl(inl(0x70002800) | 0x30, 0x70002800);

		/* RX_ATN_LVL=1 == when 12 slots full */
		/* TX_ATN_LVL=1 == when 12 slots empty */
		outl(inl(0x7000280c) | 0x33, 0x7000280c);

		/* Rx.CLR = 1, TX.CLR = 1 */
		outl(inl(0x7000280c) | 0x1100, 0x7000280c);
	} else {
		/* PP500x */

		/* I2S device reset */
		outl(inl(0xcf005030) | 0x80, 0xcf005030);
		outl(inl(0xcf005030) & ~0x80, 0xcf005030);

		/* I2S controller enable */
		outl(inl(0xc0002500) | 0x1, 0xc0002500);

		/* BIT.FORMAT [11:10] = I2S (default) */
		/* BIT.SIZE [9:8] = 24bit */
		/* FIFO.FORMAT = 24 bit LSB */

		/* reset DAC and ADC fifo */
		outl(inl(0xc000251c) | 0x30000, 0xc000251c);
	}
}


static int ipodaudio_open(struct inode *inode, struct file *filep)
{
	volatile int *r_off = (int *)DMA_READ_OFF;
	volatile int *w_off = (int *)DMA_WRITE_OFF;
	volatile int *dma_active = (int *)DMA_ACTIVE;

	if (ipodaudio_isopen) {
		return -EBUSY;
	}

	/* initialise shared variables */
	*r_off = 0;
	*w_off = 0;
	*dma_active = 0;

	ipodaudio_isopen = 1;
	ipod_sample_rate = 44100;
	*ipodaudio_stereo = 0;

	/* reset the I2S controller into known state */
	i2s_reset();

	if (filep->f_mode & FMODE_WRITE) {
		codec_init_pb();

		if (ipod_hw_ver > 3) {
			/* PP502x */

			/* setup I2S FIQ handler */
			ipod_set_process_dma(i2s_pp5020_pb_dma);

			/* setup I2S interrupt for FIQ */
			outl(inl(0x6000403c) | PP5020_I2S_MASK, 0x6000403c);
			outl(PP5020_I2S_MASK, 0x60004034);
		} else {
			/* PP500x */

			/* setup I2S FIQ handler */
			ipod_set_process_dma(i2s_pp5002_pb_dma);

			/* setup I2S interrupt for FIQ */
			outl(inl(0xcf00103c) | PP5002_DMA_OUT_MASK, 0xcf00103c);
			outl(PP5002_DMA_OUT_MASK, 0xcf001034);
		}
	}

	if (filep->f_mode & FMODE_READ) {
		/* no recording with this device */
		if (codec_chip == WM8721) {
			return -ENODEV;
		}

		if (ipod_active_rec == SOUND_MASK_LINE) {
			codec_activate_linein();
		} else if (ipod_active_rec == SOUND_MASK_MIC) {
			codec_activate_mic();
		}

		if (ipod_hw_ver == 0x6) {
			outl(inl(0x6000d120) & ~0x40, 0x6000d120);
			outl(inl(0x6000d020) & ~0x4, 0x6000d020);
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
		printk(KERN_ERR "dma not active\n");
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

		/* mask the interrupt */
		if (ipod_hw_ver > 3) {
			/* PP502x */
			outl(PP5020_I2S_MASK, 0x60004038);
		} else {
			/* PP500x */
			outl(PP5002_DMA_OUT_MASK, 0xcf001038);
		}
	}

	if (filep->f_mode & FMODE_READ) {
		volatile int *dma_active = (int *)DMA_ACTIVE;

		if (*dma_active) {
			/* tell COP dma to exit */
			*dma_active = 2;

			/* wait for the COP to signal its done */
			while (*dma_active != 3) {
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(2);

				if (signal_pending(current)) {
					break;
				}
			}
		}

		if (ipod_hw_ver == 0x6) {
			outl(inl(0x6000d120) | 0x40, 0x6000d120);
			outl(inl(0x6000d020) | 0x4, 0x6000d020);
		}
	}

	/* clear the handler */
	ipod_set_process_dma(0);

	codec_deinit();

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

	if (count <= 0) {
		return 0;
	}

	bufsp = (unsigned short *)buf;
	rem = count/2;

	write_off_current = *w_off;

	while (rem > 0) {
		int cnt;

		write_off_next = (write_off_current + 1) % BUF_LEN;

		read_off_current = *r_off;

		/* buffer full? */
		if (write_off_next == read_off_current) {
			/* buffer is full */
			set_current_state(TASK_INTERRUPTIBLE);

			/* sleep a little */
			schedule_timeout(2);
		}

		if (read_off_current <= write_off_current) {
			/* room at end of buffer? */
			cnt = BUF_LEN - 1 - write_off_current;
			if (read_off_current > 0) cnt++;

			if (cnt > 0)  {
				if (cnt > rem) cnt = rem;

				memcpy((void*)&dma_buf[write_off_current], bufsp, cnt<<1);

				rem -= cnt;
				bufsp += cnt;

				write_off_current = (write_off_current + cnt) % BUF_LEN;
			}

			/* room at start of buffer (and more data)? */
			if (read_off_current > 0 && rem > 0) {
				int n;

				if (rem >= read_off_current) {
					n = read_off_current - 1;
				} else {
					n = rem;
				}

				memcpy((void*)&dma_buf[0], bufsp, n<<1);

				rem -= n;
				bufsp += n;

				write_off_current = n;
			}
		} else if (read_off_current > write_off_current) {
			cnt = read_off_current - 1 - write_off_current;
			if (cnt > rem) cnt = rem;

			memcpy((void*)&dma_buf[write_off_current], bufsp, cnt<<1);

			bufsp += cnt;
			rem -= cnt;

			write_off_current += cnt;
		}

		*w_off = write_off_current;

		if (!*dma_active) {
			*dma_active = 1;

			if (ipod_hw_ver > 0x3) {
				outl(inl(0x70002800) | 0x2, 0x70002800);
				outl(inl(0x70002800) | 0x20000000, 0x70002800);
			} else {
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

	if (!*dma_active) {
		*dma_active = 1;

		*r_off = 0;
		*w_off = 0;

		if (ipod_hw_ver > 0x3) {
			/* setup I2S FIQ handler */
			ipod_set_process_dma(i2s_pp5020_rec_dma);

			/* setup FIQ */
			outl(inl(0x6000403c) | PP5020_I2S_MASK, 0x6000403c);
			outl(PP5020_I2S_MASK, 0x60004034);

			/* interrupt on full fifo */
			outl(inl(0x70002800) | 0x1, 0x70002800);

			/* enable record fifo */
			outl(inl(0x70002800) | 0x10000000, 0x70002800);
		} else {
			/* setup I2S FIQ handler */
			ipod_set_process_dma(i2s_pp5002_rec_dma);

			/* setup FIQ */
			outl(inl(0xcf00103c) | (1 << PP5002_DMA_IN_IRQ) , 0xcf00103c);
			outl((1 << PP5002_DMA_IN_IRQ), 0xcf001034);

			/* interrupt on full fifo */
			outl(inl(0xc000251c) | (1<<14), 0xc000251c);

			/* enable record fifo */
			outl(inl(0xc0002500) | 0x8, 0xc0002500);
		}
	}

	bufsp = (unsigned short *)buf;
	rem = count/2;

	while (rem > 0) {
		int write_pos = *w_off;
		int read_pos = *r_off;
		int len = 0;

		if (read_pos < write_pos) {
			/* read data between read pos and write pos */
			len = write_pos - read_pos;
		} else if (write_pos < read_pos) {
			/* read data to end of buffer */
			/* next loop iteration will read the rest */
			len = BUF_LEN - read_pos;
		} else {
			/* buffer is empty */
			set_current_state(TASK_INTERRUPTIBLE);

			/* sleep a little */
			schedule_timeout(2);
		}

		if (len > rem) {
			len = rem;
		}

		if (len) {
			memcpy(bufsp, (void*)&dma_buf[read_pos], len<<1);

			bufsp += len;
			rem -= len;

			/* check for buffer over run */
			if (read_pos == *r_off) {
				*r_off = (*r_off + len) % BUF_LEN;
			} else {
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

static int ipodaudio_getspace_ioctl(void *arg)
{
	audio_buf_info abinfo;
	int read_pos = *(int *)DMA_READ_OFF;
	int write_pos = *(int *)DMA_WRITE_OFF;
	int len;

	if (read_pos == write_pos) {
		len = BUF_LEN - 1;	/* ring buffer empty */
	} else if (read_pos < write_pos) {
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
		if (rc == 0) {
			get_user(val, (int *) arg);

			val = codec_set_sample_rate(val);

			put_user(val, (int *) arg);
		}
		break;

	case SNDCTL_DSP_GETFMTS:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if (rc == 0) {
			put_user(AFMT_S16_LE, (int *) arg);
		}
		break;

	case SNDCTL_DSP_SETFMT:
	/* case SNDCTL_DSP_SAMPLESIZE: */
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if (rc == 0) {
			get_user(val, (int *) arg);
			if (val != AFMT_S16_LE) {
				put_user(AFMT_S16_LE, (int *) arg);
			}
		}
		break;

	case SNDCTL_DSP_STEREO:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if (rc == 0) {
			get_user(val, (int *) arg);
			if (val != 0 && val != 1) {
				put_user(1, (int *) arg);
			} else {
				*ipodaudio_stereo = val;
			}
		}
		break;

	case SNDCTL_DSP_CHANNELS:
		rc = verify_area(VERIFY_READ, (void *) arg, sizeof(val));
		if (rc == 0) {
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
		if (rc == 0) {
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
		return ipodaudio_getspace_ioctl((void *)arg);
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
		unsigned char *codec;
	       
		codec = (codec_chip == WM8731 ? "WM8731" : (codec_chip == WM8721 ? "WM8721" : (codec_chip==WM8975 ? "WM8975" : "WM8758")));

		strncpy(info.id, codec, sizeof(info.id));
		strncpy(info.name, "Wolfson WM", sizeof(info.name));
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
					codec_activate_linein();
				} else if (val == SOUND_MASK_MIC) {
					codec_activate_mic();
				} else {
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
			if (codec_chip >= WM8758) {
				ipod_pcm_level = ((left * 63) / 100);
				right = ((right * 63) / 100);

				/* OUT1 */
				wm_write(WM8758_LOUT1VOL, 0, ipod_pcm_level);
				wm_write(WM8758_ROUT1VOL, 1, right);

				/* OUT2 */
				wm_write(WM8758_LOUT2VOL, 0, ipod_pcm_level);
				wm_write(WM8758_ROUT2VOL, 1, right);

			} else if (codec_chip >= WM8975) {
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
				} else {
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
			} else {
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
			} else {
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

static void __init ipodaudio_hw_init(void)
{
	ipod_hw_ver = ipod_get_hw_version() >> 16;

	switch (ipod_hw_ver) {
	case 1:		/* 1g */
	case 2:		/* 2g */
	case 4:		/* mini - nb these are really 31s but no recording */
	case 7:		/* mini2 - nb these are really 31s but no recording */
		codec_chip = WM8721;
		break;
	case 3:		/* 3g */
		codec_chip = WM8731;
		break;
	case 5:		/* 4g */
	case 6:		/* photo */
	case 0xc:	/* nano */
		codec_chip = WM8975;
		break;
	case 0xb:	/* 5g */
		codec_chip = WM8758;
		break;
	}

	printk("codec %s\n", (codec_chip == WM8731 ? "WM8731" : (codec_chip == WM8721 ? "WM8721" : (codec_chip==WM8975 ? "WM8975" : "WM8758"))));

	/* reset I2C */
	ipod_i2c_init();

	if (ipod_hw_ver > 3) {
		/* PP502x */

		/* normal outputs for CDI and I2S pin groups */
		outl(inl(0x70000020) & ~0x300, 0x70000020);

		/*mini2?*/
		outl(inl(0x70000010) & ~0x3000000, 0x70000010);
		/*mini2?*/

		/* device reset */
		outl(inl(0x60006004) | 0x800, 0x60006004);
		outl(inl(0x60006004) & ~0x800, 0x60006004);

		/* device enable */
		outl(inl(0x6000600C) | 0x807, 0x6000600C);

		/* enable external dev clock clocks */
		outl(inl(0x6000600c) | 0x2, 0x6000600c);

		/* external dev clock to 24MHz */
		outl(inl(0x70000018) & ~0xc, 0x70000018);

	} else {
		/* PP500x */

		/* device reset */
		outl(inl(0xcf005030) | 0x80, 0xcf005030);
		outl(inl(0xcf005030) & ~0x80, 0xcf005030);

		/* device enable */
		outl(inl(0xcf005000) | 0x80, 0xcf005000);

		/* GPIO D06 enable for output */
		outl(inl(0xcf00000c) | 0x40, 0xcf00000c);
		outl(inl(0xcf00001c) & ~0x40, 0xcf00001c);

		if (ipod_hw_ver < 0x3) {
			/* nb this is different to 3g!? */
			/* bits 11,10 == 10 */
			outl(inl(0xcf004040) & ~0x400, 0xcf004040);
			outl(inl(0xcf004040) | 0x800, 0xcf004040);
		} else {
			/* bits 11,10 == 01 */
			outl(inl(0xcf004040) | 0x400, 0xcf004040);
			outl(inl(0xcf004040) & ~0x800, 0xcf004040);

			outl(inl(0xcf004048) & ~0x1, 0xcf004048);

			outl(inl(0xcf000004) & ~0xf, 0xcf000004);
			outl(inl(0xcf004044) & ~0xf, 0xcf004044);

			/* C03 = 0 */
			outl(inl(0xcf000008) | 0x8, 0xcf000008);
			outl(inl(0xcf000018) | 0x8, 0xcf000018);
			outl(inl(0xcf000028) & ~0x8, 0xcf000028);
		}
	}
}

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

	ipodaudio_hw_init();

	return 0;
}

static void __exit ipodaudio_exit(void)
{
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

