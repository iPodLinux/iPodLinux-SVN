
#include "ipod.h"

static int piezo = 0;

void toggle_piezo(void)
{
	piezo = !piezo;
}

void beep(void)
{
	if (piezo) 
		ipod_beep();
}
