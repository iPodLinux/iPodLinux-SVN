
#include "ipod.h"

void toggle_piezo(void)
{
	ipod_set_setting(CLICKER, !ipod_get_setting(CLICKER));
}

void beep(void)
{
	if (ipod_get_setting(CLICKER)) 
		ipod_beep();
}
