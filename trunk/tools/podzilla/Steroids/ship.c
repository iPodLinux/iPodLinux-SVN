#define MWINCLUDECOLORS
#include <nano-X.h>
#include <stdlib.h>
#include <math.h>

#include "globals.h"
#include "ship.h"
#include "polygon.h"

int initialized = 0;

Steroids_Vector  startPos = {20, 20};
Steroids_Polygon modelShip;

void initializeModels()
{
    if (!initialized)
    {
	modelShip.cog.x = 0;
	modelShip.cog.y = 0;
	modelShip.colour = WHITE;
        modelShip.nPoints = 5;
	modelShip.masterPoint[0].x = 0;
	modelShip.masterPoint[0].y = -5;
	modelShip.masterPoint[1].x = -2;
	modelShip.masterPoint[1].y = 4;
	modelShip.masterPoint[2].x = 0;
	modelShip.masterPoint[2].y = 2;
	modelShip.masterPoint[3].x = 2;
	modelShip.masterPoint[3].y = 4;
	modelShip.masterPoint[4].x = 0;
	modelShip.masterPoint[4].y = -5;
	initialized = 1;
    }
}



void steroids_ship_init (Steroids_Ship *ship)
{
    initializeModels();

    ship->originalShape = &modelShip;

    ship->heading = 0;
    ship->cannon = 0;

    ship->thrust = 0;
    ship->retro = 0;

    // Initialize ship shape object:
    ship->shape.type = STEROIDS_OBJECT_TYPE_POLYGON;
    steroids_polygon_copy (ship->originalShape, &ship->shape.geometry.polygon);
    ship->shape.velocity.x = 0;
    ship->shape.velocity.y = 0;
    ship->shape.accelleration.x = 0;
    ship->shape.accelleration.y = 0;


    // Move to start position:
    startPos.x = steroids_globals.width / 2;
    startPos.y = steroids_globals.height / 2;
    steroids_object_rotate (ship->heading, &ship->shape);
    steroids_object_translate (startPos, &ship->shape);
}


void steroids_ship_draw (Steroids_Ship *ship)
{
    steroids_object_draw (&ship->shape, 0);
}



/** Animates ship according to velocity and accelleration.
 * 
 */
void steroids_ship_animate (Steroids_Ship *ship)
{
    if (ship->retro
	&& abs (ship->shape.velocity.x) < STEROIDS_SHIP_MIN_VELOCITY
	&& abs (ship->shape.velocity.y) < STEROIDS_SHIP_MIN_VELOCITY)
    {
	ship->retro = 0;
	ship->shape.velocity.x = 0;
	ship->shape.velocity.y = 0;
	ship->shape.accelleration.x = 0;
	ship->shape.accelleration.y = 0;
    }

    steroids_object_animate (&ship->shape);
}





/** Rotates the given ship around it's cog.
 *
 */
void steroids_ship_rotate (float angle, Steroids_Ship *ship)
{
    Steroids_Vector v;

    ship->heading += angle;

    v.x = ship->shape.geometry.polygon.cog.x;
    v.y = ship->shape.geometry.polygon.cog.y;

    steroids_object_rotate (ship->heading, &ship->shape);

    ship->shape.geometry.polygon.cog.x = ship->originalShape->cog.x;
    ship->shape.geometry.polygon.cog.y = ship->originalShape->cog.y;
    steroids_object_translate (v, &ship->shape);

    if (ship->thrust)
    {
	ship->thrust = 0;
	steroids_ship_thrustOn (ship);
    }
}


/** Turns ship thruster engine on.
 *
 */
void steroids_ship_thrustOn (Steroids_Ship *ship)
{
    float           vLen;
    Steroids_Vector v;
    if (ship->thrust)
    {
	vLen = STEROIDS_SHIP_RETROTHRUST
	       / sqrt (  ship->shape.velocity.x * ship->shape.velocity.x
		       + ship->shape.velocity.y * ship->shape.velocity.y);

	v.x = ship->shape.velocity.x * vLen;
	v.y = ship->shape.velocity.y * vLen;
	ship->thrust = 0;
	ship->retro = 1;
    }
    else
    {
	steroids_vector_fromPolar (ship->heading, STEROIDS_SHIP_MAINTHRUST, &v);
	ship->thrust = 1;
	ship->retro = 0;
    }
    ship->shape.accelleration.x = v.x;
    ship->shape.accelleration.y = v.y;
}

/** Turns ship retro thrusters on.
 *
 */
void steroids_ship_thrustOff (Steroids_Ship *ship)
{
/*
    Steroids_Vector v;
    steroids_vector_fromPolar (ship->heading, STEROIDS_SHIP_RETROTHRUST, &v);
    ship->shape.accelleration.x = v.x;
    ship->shape.accelleration.y = v.y;
*/
}
