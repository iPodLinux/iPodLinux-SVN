#ifndef __STEROIDS_SHOT_PROTOS_H__
#define __STEROIDS_SHOT_PROTOS_H__

#include "shot.h"
#include "ship.h"


/** steroids_shot_new
 *
 *  Allocates a new shot fired from the given ship. If no available shot slots
 *  are available, nothing happens.
 *
 */
void steroids_shot_newShip (Steroids_Shot *shot,
			    Steroids_Ship *ship);


void steroids_shot_drawall (Steroids_Shot *shot);
void steroids_shot_draw (Steroids_Shot *shot);

/** Animates shot according to velocity and accelleration.
 * 
 */
void steroids_shot_animateall (Steroids_Shot *shot);
void steroids_shot_animate (Steroids_Shot *shot);


/** steroids_shot_collideShip
 *
 *  Returns the index of the shot that collided with the ship or -1 if
 *  none did.
 *
 */
int steroids_shot_collideShip (Steroids_Shot *shot,
			       Steroids_Ship *ship);


#endif
