#ifndef _IPOD_H_
#define _IPOD_H_

/* lcd.c */
typedef struct st_ipod_lcd_info {
	unsigned long base, rtc;
	long generation;
	int bpp, width, height, type;
	unsigned short *fb;
} ipod_lcd_info;

int ipod_get_contrast(void);
int ipod_set_contrast(int contrast);
int ipod_get_backlight(void);
int ipod_set_backlight(int backlight);
int ipod_set_blank_mode(int blank); /* 0 = on, 1-2 = off, 3 power down */

ipod_lcd_info * ipod_get_lcd_info(void);
unsigned short * ipod_alloc_fb(ipod_lcd_info *lcd);
void ipod_free_fb(ipod_lcd_info *lcd);
void ipod_update_mono_lcd(ipod_lcd_info *lcd, int sx, int sy, int mx, int my);
void ipod_update_colour_lcd(ipod_lcd_info *lcd, int sx, int sy, int mx, int my);

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

