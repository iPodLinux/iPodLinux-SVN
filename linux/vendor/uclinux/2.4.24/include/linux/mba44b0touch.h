#ifndef __MBA44B0TOUCH_INC__
#define __MBA44B0TOUCH_INC__

/*
 * Copyright (C) 2003 Stefan Macher <macher@sympat.de>
 *                    Alexander Assel <assel@sympat.de>
 */

/* debugging stuff */
#define MBA44B0_TS_DEBUG_ERR    (1 << 0)
#define MBA44B0_TS_DEBUG_INFO   (1 << 1)
#define MBA44B0_TS_DEBUG_ADC    (1 << 2)
#define MBA44B0_TS_DEBUG_POS    (1 << 3)
#define MBA44B0_TS_DEBUG_PRESS  (1 << 4)
#define MBA44B0_TS_DEBUG_CONFIG (1 << 5)
#define MBA44B0_TS_DEBUG_IRQ    (1 << 6)
#define MBA44B0_TS_DEBUG(x,args...) if(x&mba44b0_ts_debug_mask) printk("mba44b0_touch: " args);

//------------------------------
//IOCTL COMMANDS und structures
//------------------------------
#define MBA44B0_TS_SET_CAL_PARAMS 0
#define MBA44B0_TS_GET_CAL_PARAMS 1
#define MBA44B0_TS_SET_X_RES      2
#define MBA44B0_TS_SET_Y_RES      3

struct mba44b0_cal_settings {
  int cal_x_off;
  unsigned int cal_x_scale_num;
  unsigned int cal_x_scale_denom;
  int cal_y_off;
  unsigned int cal_y_scale_num;
  unsigned int cal_y_scale_denom;
};

//------------------------------
// PRESS THRESHOLD
//------------------------------
#define MBA44B0_TS_PRESS_THRESHOLD 512
//------------------------------

//------------------------------
// Device definintions
//------------------------------
#define TS_DEVICE "MBA44B0"
#define TS_DEVICE_FILE "/dev/touchscreen/mba44b0"


/*! \brief Interface struct used for reading from touchscreen device
 */
struct ts_event 
{
  unsigned char pressure; //!< zero -> not pressed, all other values indicate that the touch is pressed
  short x; //!< The x position
  short y; //!< The y position
  short pad; //!< No meaning for now
};

#endif
