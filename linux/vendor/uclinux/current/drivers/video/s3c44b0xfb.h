/*
 * linux/drivers/video/s3c44b0xfb.c
 *	-- Support for the S3C44B0X LCD/CRT controller as framebuffer device
 *
 * Copyright (C) 2003 Stefan Macher <macher@sympat.de>
 *                    Alexander Assel <assel@sympat.de>
 *
*/
#ifndef __INCLUDE_S3C44B0XFB__
#define __INCLUDE_S3C44B0XFB__

#define S3C44B0X_FB_ADDRESS 0x0C400000 /* FIXME should be done with malloc */


/********************************************************************/
/*! \brief Holds all needed LCD panel configuration values
 *
 *  The driver holds a set of these configuration tables that can be
 *  referenced by giving the LCD panel name as defined in lcd_panel_name
 *  to the driver. For further informations see also the S3C44B0X datasheet.
 *
 *  The six disp_on_... attributes give the possiblity to define a port
 *  that has to be set to enable the display.
 */
struct known_lcd_panels
{
  unsigned char lcd_panel_name[64];   /*!< Holds the lcd panel name */
  u16 clkval; /*!< Determine the rate of CLKVAL. This could also be solved with VCLK and MCLK */
  u8 wlh; /*!< Determine the VLINE pulses high level width [clocks] valid values 4, 8, 12 and 16 */
  u8 wdly; /*!< Determine the delay between VLINE and VCLOCK [clocks] valid values 4, 8, 12 and 16 */
  u8 mmode; /*!< Determine toggle rate of VM 0 -> each frame 1 -> defined by MVAL */
  u8 dismode; /*!< Controls the display mode 0->4-bit dual-scan display, 1->4-bit single scan display, 2->8-bit single-scan display */
  u8 invclk; /*!< Controls the polarity of VCLOCK active edge 0 -> falling edge 1 -> rising edge  */
  u8 invline; /*!< Indicates the line pulse polarity 0 -> normal 1 -> inverted */
  u8 invframe; /*!< Indicates the frame pulse polarity 0 -> normal 1 -> inverted */
  u8 invvd; /*!< Indicates the video data polarity 0 -> normal 1 -> inverted */
  u16 lineblank; /*!< Indicates the blank time in one horizontal line duration */
  u8 selfref; /*!< LCD self refrash mode enable */
  u8 bswp; /*!< Byte swap control */
  u8 mval; /*!< Defines the rate at which the vm signal will toggle */
  u16 width; /*!< display width in pixels */
  u16 height; /*!< Display height in pixels */
  u8 bpp; /*!< Bits per pixel; valid values are 1 (mono), 2 (gray), 4 (gray) and 8 (color) */
  u16 hozval; /*!< hozval */
  u16 lineval; /*!< linevalue */
  u32 *disp_on_port; /*!< disp on port register; set to zero if you do not need it */
  u32 disp_on_mask; /*!< disp on mask; defines the mask of the port bits */
  u32 disp_on_val; /*!< disp on value; defines the value to write to disp_on_port to enable the display */
  u32 *disp_on_ctrl_reg; /*!< disp_on control register; set to zero if you do not need it; the register to control the port (input / output / ...) */
  u32 disp_on_ctrl_reg_mask; /*!< disp_on control register mask to mask port/pin control */
  u32 disp_on_ctrl_reg_value /*!< disp on ctrl reg value */
};	

struct s3c44b0xfb_info {
       
	struct fb_info_gen gen;
	unsigned long fb_virt_start;
	unsigned long fb_size;
	unsigned long fb_phys;
	
};


struct s3c44b0xfb_par {
  
	struct fb_var_screeninfo var;
	int line_length;  /* in bytes */
};


#endif
