#ifndef __STEROIDS_SHOT_PROTOS_H__
#define __STEROIDS_SHOT_PROTOS_H__

#include "shot.h"
#include "ship.h"


/** steroids_shot_new
 *
 *  Allocates a new shot based on the given parameters. If no available shot
 *  slots are available, nothing happens.
 *
 */
void steroids_shot_new (Steroids_Shot *shot,
			int            n,
			int x, int y,
			Steroids_Vector *initialV,
			float heading,
			float velocity,
			int lifeCycles);

/** steroids_shot_new
 *
 *  Allocates a new shot fired from the given ship. If no available shot slots
 *  are available, nothing happens.
 *
 */
void steroids_shot_newShip (Steroids_Shot *shot,
			    int            n,
			    Steroids_Ship *ship);


void steroids_shot_drawall (Steroids_Shot *shot, int n, GR_WINDOW_ID wid);
void steroids_shot_draw (Steroids_Shot *shot, GR_WINDOW_ID wid);

/** Animates shot according to velocity and accelleration.
 * 
 */
void steroids_shot_animateall (Steroids_Shot *shot, int n);
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
