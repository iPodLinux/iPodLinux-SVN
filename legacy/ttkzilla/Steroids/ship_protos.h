#ifndef __STEROIDS_SHIP_PROTOS_H__
#define __STEROIDS_SHIP_PROTOS_H__

#include "ship.h"
#include "shot.h"

void steroids_ship_init (Steroids_Ship *ship);

void steroids_ship_draw (Steroids_Ship *ship, GR_WINDOW_ID wid);

void steroids_ship_drawReserve (GR_WINDOW_ID wid, GR_GC_ID gc);
void steroids_ship_eraseReserve (int ships, GR_WINDOW_ID wid, GR_GC_ID gc);


/** Animates ship according to velocity and accelleration.
 * 
 */
void steroids_ship_animate (Steroids_Ship *ship);



/** Rotates the given ship around it's cog.
 *
 */
void steroids_ship_rotate (float angle, Steroids_Ship *ship); 


/** Turns ship thruster engine on.
 *
 * Note: Since I can't get the KEY_UP events from Microwindows, this now
 *       toggles thruster/retro.
 */
void steroids_ship_thrustOn (Steroids_Ship *ship); 

/** Turns ship retro thrusters on.
 *
 */
void steroids_ship_thrustOff (Steroids_Ship *ship); 



/** Fires cannon.
 *
 */
void steroids_ship_fire (Steroids_Shot *shots,
			 Steroids_Ship *ship);


/** Tell the ship that it has collided.
 *
 * Returns true if the state was changed to DIE, false if the ship was
 * already dying.
 *
 */
int steroids_ship_collided (Steroids_Ship *ship);

#endif
