#ifndef __STEROIDS_ASTEROID_H__
#define __STEROIDS_ASTEROID_H__

#include "grafix.h"

#include "object.h"
#include "polygon.h"
#include "ship.h"
#include "shot.h"

#define STEROIDS_ASTEROID_RESOLUTION 6
#define STEROIDS_ASTEROID_NUM 10

#define STEROIDS_ASTEROID_MIN_RADIUS 5
#define STEROIDS_ASTEROID_MAX_RADIUS 30
#define STEROIDS_ASTEROID_SMOOTHNESS 0.5

#define STEROIDS_ASTEROID_MIN_FORCE 0.9
#define STEROIDS_ASTEROID_MAX_FORCE 2


typedef struct SteroidsAsteroid
{
    int             active;
    Steroids_Object shape;

} Steroids_Asteroid;

void steroids_asteroid_initall (Steroids_Asteroid *asteroid);
void steroids_asteroid_init (Steroids_Asteroid *asteroid);

void steroids_asteroid_drawall (Steroids_Asteroid *asteroid, GR_WINDOW_ID wid);
void steroids_asteroid_draw (Steroids_Asteroid *asteroid, GR_WINDOW_ID wid);

/** Animates asteroid according to velocity and accelleration.
 * 
 */
void steroids_asteroid_animateall (Steroids_Asteroid *asteroid);
void steroids_asteroid_animate (Steroids_Asteroid *asteroid);


/** steroids_asteroid_collideShip
 *
 *  Returns the index of the asteroid that collided with the ship or -1 if
 *  none did.
 *
 */
int steroids_asteroid_collideShip (Steroids_Asteroid *asteroid,
				   Steroids_Ship     *ship);



/** steroids_asteroid_collideShot
 *
 *  Returns true if an asteroid was hit by any of the shots.
 *
 *  Side effect: The shot that hit is deactivated, the asteroid is split
 *               and a score is added to the player.
 *
 */
int steroids_asteroid_collideShot (Steroids_Asteroid *asteroid,
				   Steroids_Shot     *shot);

#endif
