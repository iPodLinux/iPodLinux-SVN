#include "../pz.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "globals.h"
#include "asteroid.h"
#include "polygon.h"
#include "ship.h"

#define rnd(min,max) (((float)rand()/RAND_MAX)*((max)-(min))+(min))

void steroids_asteroid_initall (Steroids_Asteroid *asteroid)
{
    int i;
    for (i = 0; i < STEROIDS_ASTEROID_NUM; i++)
    {
	if (rand() < RAND_MAX / 3)
	{
	    steroids_asteroid_init (&asteroid[i]);
	}
    }
}



void steroids_asteroid_generateShape(Steroids_Asteroid *asteroid,
				     int                minRadius,
				     int                maxRadius)
{
    int             i;
    float           heading;
    float           step = PI2 / (STEROIDS_ASTEROID_RESOLUTION - 1);
    Steroids_Vector v;

    asteroid->shape.type = STEROIDS_OBJECT_TYPE_POLYGON;
    asteroid->shape.colour = BLACK;
    asteroid->shape.geometry.polygon.nPoints = STEROIDS_ASTEROID_RESOLUTION;
    asteroid->shape.geometry.polygon.radius = maxRadius;

    heading = 0;
    for (i = 0; i < STEROIDS_ASTEROID_RESOLUTION - 1; i++)
    {
	steroids_vector_fromPolar (heading,
				   rnd(minRadius, maxRadius),
				   &v);
	asteroid->shape.geometry.polygon.masterPoint[i].x = (int)(v.x + 0.5);
	asteroid->shape.geometry.polygon.masterPoint[i].y = (int)(v.y + 0.5);
	heading += step;
    }
    asteroid->shape.geometry.polygon.masterPoint[i].x
	= asteroid->shape.geometry.polygon.masterPoint[0].x;
    asteroid->shape.geometry.polygon.masterPoint[i].y
	= asteroid->shape.geometry.polygon.masterPoint[0].y;
}


void steroids_asteroid_init (Steroids_Asteroid *asteroid)
{
    int   maxRadius;
    int   minRadius;

    float heading;
    float force;

    Steroids_Vector v;

    asteroid->active = 1;

    maxRadius = (int)(rnd(STEROIDS_ASTEROID_MIN_RADIUS, 
			  STEROIDS_ASTEROID_MAX_RADIUS) + 0.5);
    minRadius = STEROIDS_ASTEROID_SMOOTHNESS * maxRadius;
    steroids_asteroid_generateShape (asteroid, minRadius, maxRadius);


    force = rnd(STEROIDS_ASTEROID_MIN_FORCE, 
                STEROIDS_ASTEROID_MAX_FORCE);
    heading = rnd(0, PI2);
    steroids_vector_fromPolar (heading, force, &asteroid->shape.velocity);

    asteroid->shape.accelleration.x = 0;
    asteroid->shape.accelleration.y = 0;


    // Move to start position:
    v.x = (int)(rnd(0, steroids_globals.width) + 0.5);
    v.y = (int)(rnd(0, steroids_globals.height) + 0.5);
    steroids_object_rotate (0, &asteroid->shape);
    steroids_object_translate (v, &asteroid->shape);
}


void steroids_asteroid_split (Steroids_Asteroid *a,
			      Steroids_Asteroid *asteroid)
{
    int             i;
    int             available = 0;

    int             maxRadius;
    int             minRadius;
    Steroids_Vector v;
    Steroids_Vector p;

    maxRadius = a->shape.geometry.polygon.radius / 2;
    if (maxRadius < STEROIDS_ASTEROID_MIN_RADIUS)
    {
	a->active = 0;
    }
    else
    {
	// Initialize asteroid shape object:
	minRadius = STEROIDS_ASTEROID_SMOOTHNESS * maxRadius;
	steroids_asteroid_generateShape (a, minRadius, maxRadius);

	// Set up velocity:
	v.x = a->shape.velocity.y;
	v.y = -a->shape.velocity.x;
	a->shape.velocity.x = v.x;
	a->shape.velocity.y = v.y;

	p.x = a->shape.pos.x;
	p.y = a->shape.pos.y;
	// Move to start position:
	steroids_object_rotate (0, &a->shape);
	steroids_object_translate (p, &a->shape);
	
	// Look for available asteroid:
	i = 0;
	while (i < STEROIDS_ASTEROID_NUM && !available)
	{
	    available = !asteroid[i].active;
	    i++;
	}
	if (available)
	{
	    i--;
	    // Create new asteroid:
	    asteroid[i].active = 1;

	    // Initialize asteroid shape object:
	    steroids_asteroid_generateShape (&asteroid[i],
					     minRadius,
					     maxRadius);

	    // Set up velocity:
	    asteroid[i].shape.velocity.x = -v.x;
	    asteroid[i].shape.velocity.y = -v.y;
	    asteroid[i].shape.accelleration.x = 0;
	    asteroid[i].shape.accelleration.y = 0;

	    // Move to start position:
	    steroids_object_rotate (0, &asteroid[i].shape);
	    steroids_object_translate (p, &asteroid[i].shape);
	}
    }
}







void steroids_asteroid_drawall (Steroids_Asteroid *asteroid, GR_WINDOW_ID wid)
{
    int i;
    for (i = 0; i < STEROIDS_ASTEROID_NUM; i++)
    {
	if (asteroid[i].active)
	{
	    steroids_asteroid_draw (&asteroid[i], wid);
	}
    }
}

void steroids_asteroid_draw (Steroids_Asteroid *asteroid, GR_WINDOW_ID wid)
{
    steroids_object_draw (&asteroid->shape, 0, wid);
}



/** Animates asteroid according to velocity and accelleration.
 * 
 */
void steroids_asteroid_animateall (Steroids_Asteroid *asteroid)
{
    int i;
    for (i = 0; i < STEROIDS_ASTEROID_NUM; i++)
    {
	if (asteroid[i].active)
	{
	    steroids_asteroid_animate (&asteroid[i]);
	}
    }
}
void steroids_asteroid_animate (Steroids_Asteroid *asteroid)
{
    steroids_object_animate (&asteroid->shape);
}


/** steroids_asteroid_collideShip
 *
 *  Returns true if an asteroid collided with the ship.
 *  
 */
int steroids_asteroid_collideShip (Steroids_Asteroid *asteroid,
				   Steroids_Ship     *ship)
{
    int collided = 0;
    int i = 0;
    while (i < STEROIDS_ASTEROID_NUM && !collided)
    {
	if (asteroid[i].active)
	{
	    collided = steroids_object_collide (&asteroid[i].shape,
						&ship->shape);
	}
	i++;
    }
    return collided;
}

/** steroids_asteroid_collideShot
 *
 *  Returns true if an asteroid was hit by any of the shots.
 *
 *  Side effect: The shot that hit is deactivated, the asteroid is split
 *               and a score is added to the player.
 *
 */
int steroids_asteroid_collideShot (Steroids_Asteroid *asteroid,
				   Steroids_Shot     *shot)
{
    int collided = 0;
    int c = 0;
    int a;
    int s;
    for (a = 0; a < STEROIDS_ASTEROID_NUM; a++)
    {
	if (asteroid[a].active)
	{
	    for (s = 0; s < STEROIDS_SHOT_NUM; s++)
	    {
		if (shot[s].active)
		{
		    c = steroids_object_collide (&asteroid[a].shape,
						 &shot[s].shape);
		    if (c)
		    {
			collided = 1;
			shot[s].active = 0;
			steroids_globals.score += (int)(1.0
							/ asteroid[a].shape.geometry.polygon.radius
							* 1000);
			steroids_asteroid_split (&asteroid[a],
						 asteroid);
		    }
		}
	    }
	}
    }
    return collided;
}
