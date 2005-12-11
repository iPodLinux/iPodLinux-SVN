#ifndef _IPOD_H_
#define _IPOD_H_

/* lcd.c */
typedef struct st_ipod_lcd_info {
	unsigned long base, rtc, busy_mask;
	char gen;
	unsigned int bpp, width, height, type, fblen;
	void *fb;
} ipod_lcd_info;

int ipod_get_contrast(void);
int ipod_set_contrast(int contrast);
int ipod_get_backlight(void);
int ipod_set_backlight(int backlight);
int ipod_set_blank_mode(int blank); /* 0 = on, 1-2 = off, 3 power down */

ipod_lcd_info * ipod_lcd_get_info(void);
void * ipod_lcd_alloc_fb(ipod_lcd_info *lcd);
void ipod_lcd_free_fb(ipod_lcd_info *lcd);
#ifdef IPOD
void ipod_lcd_update(ipod_lcd_info *lcd, int sx, int sy, int mx, int my);
void ipod_fb_video(void);
void ipod_fb_text(void);
#endif

/* disk.c */
/* these should be in the kernel somewhere */
void ipod_set_diskmode(void); /* call a reboot after */
int ipod_usb_is_connected(void); /* 1 = connected, 0 = not */
int ipod_fw_is_connected(void);  /* ''                  '' */

/* ipod.c */
long ipod_get_hw_version(void); /* full ipod revision, use base 16 */
char ipod_get_fs_type(void);    /* H for HFS+, F for FAT, U for Unknown */
void ipod_beep(void);
int ipod_read_apm(int *battery, int *charging); /* random on host */


#endif /* _IPOD_H_ */

