#ifndef __STEROIDS_SHOT_H__
#define __STEROIDS_SHOT_H__

#include <nano-X.h>

#include "object.h"
#include "polygon.h"
#include "ship.h"

#define STEROIDS_SHOT_NUM     10
#define STEROIDS_SHOT_FORCE    3
#define STEROIDS_SHOT_CYCLES  30

typedef struct SteroidsShot
{
    int             active;
    long            cycles;    // The number of cycles the shot has been active
    Steroids_Object shape;

} Steroids_Shot;

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
