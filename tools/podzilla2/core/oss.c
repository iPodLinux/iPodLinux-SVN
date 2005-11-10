/*
 * Copyright (C) 2005 Courtney Cavin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <sys/types.h>
#ifdef __linux__
#include <linux/soundcard.h>
#endif
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "pz.h"

int pz_dsp_vol_change(pz_dsp_st *oss, int delta)
{
	int vol, ret;
	if (oss->mixer < 0) {
		return -1;
	}
	if ((oss->volume == PZ_DSP_MAX_VOL && delta > 0) ||
	    (oss->volume == 0 && delta < 0) ||
	     delta == 0) {
		return 0;
	}
	oss->volume += delta;
	if (oss->volume > PZ_DSP_MAX_VOL) {
		oss->volume = PZ_DSP_MAX_VOL;
	}
	else if (oss->volume < 0) {
		oss->volume = 0;
	}
	vol = oss->volume << 8 | oss->volume;
#ifdef __linux__
	ret = ioctl(oss->mixer, SOUND_MIXER_WRITE_PCM, &vol);
#else
	ret = -1;
#endif
	oss->volume = vol & 0xff;
	return ret;
}

int pz_dsp_vol_up(pz_dsp_st *oss)
{
	return pz_dsp_vol_change(oss, 1);
}

int pz_dsp_vol_down(pz_dsp_st *oss)
{
	return pz_dsp_vol_change(oss, -1);	
}

int pz_dsp_get_volume(pz_dsp_st *oss)
{
	return oss->volume;
}

int pz_dsp_setup(pz_dsp_st *oss, int channels, int rate)
{
#ifdef __linux__
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
#else
	return -1;
#endif
}

static int pz_dsp_set_recsrc(pz_dsp_st *oss, int rec_src)
{
#ifdef __linux__
	return ioctl(oss->mixer, SOUND_MIXER_WRITE_RECSRC, &rec_src);
#else
	return -1;
#endif
}

void pz_dsp_write(pz_dsp_st *oss, void *ptr, size_t size)
{
#ifdef __linux__
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
#endif
}

size_t pz_dsp_read(pz_dsp_st *oss, void *ptr, size_t size)
{
	return read(oss->dsp, ptr, size);
}

int pz_dsp_open(pz_dsp_st *oss, int mode)
{
#ifdef __linux__
	oss->dsp = open("/dev/dsp", (mode == PZ_DSP_LINEOUT)? O_WRONLY : O_RDONLY);
	if (oss->dsp < 0) {
		return -1;
	}

	oss->mixer = open("/dev/mixer", O_RDWR);
	if (oss->mixer >= 0) {
		if (mode == PZ_DSP_LINEOUT) {
			ioctl(oss->mixer, SOUND_MIXER_READ_PCM, &oss->volume);
			oss->volume = oss->volume & 0xff;
		}
		else {
			if (mode == PZ_DSP_LINEIN) {
				pz_dsp_set_recsrc(oss, SOUND_MASK_LINE);
			}
			else if (mode == PZ_DSP_MIC) {
				pz_dsp_set_recsrc(oss, SOUND_MASK_MIC);
			}
			close(oss->mixer);
			oss->mixer = -1;
		}
	}
	return 0;
#else
	return -1;
#endif
}

void pz_dsp_close(pz_dsp_st *oss)
{
	close(oss->dsp);
	if (oss->mixer >= 0) {
		close(oss->mixer);
	}
}	

