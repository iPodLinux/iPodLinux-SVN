#define MWINCLUDECOLORS
#include <nano-X.h>
#include <stdlib.h>

#include "globals.h"
#include "shot.h"
#include "polygon.h"
#include "ship.h"


/** steroids_shot_new
 *
 *  Allocates a new shot fired from the given ship. If no available shot slots
 *  are available, nothing happens.
 *
 */
void steroids_shot_newShip (Steroids_Shot *shot,
			    Steroids_Ship *ship)
{
    int available = 0;
    int i = 0;
    Steroids_Vector v;

    while (i < STEROIDS_SHOT_NUM
	   && !available)
    {
	available = !shot[i].active;
	i++;
    }

    if (available)
    {
	i--;
	shot[i].active = 1;
	shot[i].cycles = 0;
	shot[i].shape.type = STEROIDS_OBJECT_TYPE_POINT;
	shot[i].shape.colour = BLACK;

	steroids_vector_fromPolar (ship->heading,
				   STEROIDS_SHOT_FORCE,
				   &shot[i].shape.velocity);
	shot[i].shape.velocity.x += ship->shape.velocity.x;
	shot[i].shape.velocity.y += ship->shape.velocity.y;

	shot[i].shape.accelleration.x = 0;
	shot[i].shape.accelleration.y = 0;

	// Move to start position:
	v.x = ship->shape.geometry.polygon.point[ship->cannon].x;
	v.y = ship->shape.geometry.polygon.point[ship->cannon].y;

	steroids_object_rotate (0, &shot[i].shape);
	steroids_object_translate (v, &shot[i].shape);
    }
}


void steroids_shot_drawall (Steroids_Shot *shot)
{
    int i;
    for (i = 0; i < STEROIDS_SHOT_NUM; i++)
    {
	if (shot[i].active)
	{
	    steroids_shot_draw (&shot[i]);
	}
    }
}

void steroids_shot_draw (Steroids_Shot *shot)
{
    steroids_object_draw (&shot->shape, 0);
}



/** Animates shot according to velocity and accelleration.
 * 
 */
void steroids_shot_animateall (Steroids_Shot *shot)
{
    int i;
    for (i = 0; i < STEROIDS_SHOT_NUM; i++)
    {
	if (shot[i].active)
	{
	    steroids_shot_animate (&shot[i]);
	}
    }
}
void steroids_shot_animate (Steroids_Shot *shot)
{
    if (shot->cycles > STEROIDS_SHOT_CYCLES)
    {
	shot->active = 0;
    }
    else
    {
	steroids_object_animate (&shot->shape);
	shot->cycles++;
    }
}


/** steroids_shot_collideShip
 *
 *  Returns the index of the shot that collided with the ship or -1 if
 *  none did.
 *
 */
int steroids_shot_collideShip (Steroids_Shot *shot,
			       Steroids_Ship *ship)
{
    int collided = 0;
    int i = 0;
    while (i < STEROIDS_SHOT_NUM && !collided)
    {
	if (shot[i].active)
	{
	    collided = steroids_object_collide (&ship->shape,
						&shot[i].shape);
						
	}
	i++;
    }
    return collided ? i - 1 : -1;
}

