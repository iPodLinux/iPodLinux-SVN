#ifndef _OSS_H_
#define _OSS_H_

#define DSP_LINEIN 0
#define DSP_LINEOUT 1
#define DSP_MIC 2

#define DSP_MAX_VOL 100	/* apparently this isn't obvious */

typedef struct _dsp_st {
	int dsp;	/* /dev/dsp file descriptor */
	int mixer;	/* /dev/mixer file descriptor */
	int volume;	/* pcm volume 0 - 100 (or DSP_MAX_VOL) */
} dsp_st;

/* pcm volume changer: */
int dsp_vol_change(dsp_st *oss, int delta);
/* and for the lazy: */
int dsp_vol_up(dsp_st *oss);
int dsp_vol_down(dsp_st *oss);
int dsp_get_volume(dsp_st *oss);

/* set the dsp.. umm... fmt (whatever that is), channels and rate: */
int dsp_setup(dsp_st *oss, int channels, int rate);

/* dsp read and (blocking) write: */
size_t dsp_read(dsp_st *oss, void *ptr, size_t size);
void dsp_write(dsp_st *oss, void *ptr, size_t size);

/* dsp open and close:  mode takes DSP_LINEIN, DSP_LINEOUT and DSP_MIC */
int dsp_open(dsp_st *oss, int mode);
void dsp_close(dsp_st *oss);

#endif /* _OSS_H_ */
