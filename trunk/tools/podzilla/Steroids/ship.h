#ifndef __STEROIDS_SHIP_H__
#define __STEROIDS_SHIP_H__

#include <nano-X.h>

#include "object.h"
#include "polygon.h"

// 2*PI / 8 = 0.78539816339744825
#define STEROIDS_SHIP_ROTATION 0.78539816339744825
#define STEROIDS_SHIP_MIN_VELOCITY 0.001
#define STEROIDS_SHIP_MAINTHRUST 0.3
#define STEROIDS_SHIP_RETROTHRUST -0.1

typedef struct SteroidsShip
{
    int             thrust; // 1 means main thrusters on, 0 otherwise 
    int             retro; // 1 means retro thrusters on, 0 otherwise 
    Steroids_Object shape;
    Steroids_Object engine;
    float heading;
    int cannon; // Index into point list of shape object

    Steroids_Polygon *originalShape;
    Steroids_Polygon *originalEngine;

} Steroids_Ship;

void steroids_ship_init (Steroids_Ship *ship);

void steroids_ship_draw (Steroids_Ship *ship);

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

#endif
