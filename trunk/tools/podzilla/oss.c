#include <stdlib.h>
#include <sys/types.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "pz.h"
#include "oss.h"

int dsp_vol_change(dsp_st *oss, int delta)
{
	int vol, ret;
	if (oss->mixer < 0) {
		return -1;
	}
	if ((oss->volume == DSP_MAX_VOL && delta > 0) ||
	    (oss->volume == 0 && delta < 0) ||
	     delta == 0) {
		return 0;
	}
	oss->volume += delta;
	if (oss->volume > DSP_MAX_VOL) {
		oss->volume = DSP_MAX_VOL;
	}
	else if (oss->volume < 0) {
		oss->volume = 0;
	}
	vol = oss->volume << 8 | oss->volume;
	ret = ioctl(oss->mixer, SOUND_MIXER_WRITE_PCM, &vol);
	oss->volume = vol & 0xff;
	return ret;
}

int dsp_vol_up(dsp_st *oss)
{
	return dsp_vol_change(oss, 1);
}

int dsp_vol_down(dsp_st *oss)
{
	return dsp_vol_change(oss, -1);	
}

int dsp_get_volume(dsp_st *oss)
{
	return oss->volume;
}

int dsp_setup(dsp_st *oss, int channels, int rate)
{
	int ret;
	int fmt = AFMT_S16_LE;
	ret = ioctl(oss->dsp, SNDCTL_DSP_SETFMT, &fmt);
	if (ret < 0) {
		return ret;
	}
	/* mono or stereo */
	ret = ioctl(oss->dsp, SNDCTL_DSP_CHANNELS, &channels);
	if (ret < 0) {
		return ret;
	}
	/* sample rate */
	return ioctl(oss->dsp, SNDCTL_DSP_SPEED, &rate);
}

static int dsp_set_recsrc(dsp_st *oss, int rec_src)
{
	return ioctl(oss->mixer, SOUND_MIXER_WRITE_RECSRC, &rec_src);
}

void dsp_write(dsp_st *oss, void *ptr, size_t size)
{
	size_t rem, n;
	void *p = ptr;
	if ((rem = size) > 0) {
		while (rem) {
			n = write(oss->dsp, p, rem);
			if (n > 0) {
				rem -= n;
				p += n;
			}
		}
	}
}

size_t dsp_read(dsp_st *oss, void *ptr, size_t size)
{
	return read(oss->dsp, ptr, size);
}

int dsp_open(dsp_st *oss, int mode)
{
	oss->dsp = open("/dev/dsp", (mode == DSP_LINEOUT)? O_WRONLY : O_RDONLY);
	if (oss->dsp < 0) {
		return -1;
	}

	oss->mixer = open("/dev/mixer", O_RDWR);
	if (oss->mixer >= 0) {
		if (mode == DSP_LINEOUT) {
			ioctl(oss->mixer, SOUND_MIXER_READ_PCM, &oss->volume);
			oss->volume = oss->volume & 0xff;
		}
		else {
			if (mode == DSP_LINEIN) {
				dsp_set_recsrc(oss, SOUND_MASK_LINE);
			}
			else if (mode == DSP_MIC) {
				dsp_set_recsrc(oss, SOUND_MASK_MIC);
			}
			close(oss->mixer);
			oss->mixer = -1;
		}
	}
	return 0;
}

void dsp_close(dsp_st *oss)
{
	close(oss->dsp);
	if (oss->mixer >= 0) {
		close(oss->mixer);
	}
}	

