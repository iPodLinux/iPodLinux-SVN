#ifndef __STEROIDS_OBJECT_H__
#define __STEROIDS_OBJECT_H__

#include "../pz.h"
#include "polygon.h"

#define STEROIDS_OBJECT_TYPE_POINT    0
#define STEROIDS_OBJECT_TYPE_POLYGON  1

typedef struct SteroidsObject
{
    int type;

    union steroids_geo
    {
	GR_POINT point;
	Steroids_Polygon polygon;
    } geometry;

    int colour;

    Steroids_Vector velocity;
    Steroids_Vector accelleration;

    Steroids_Vector pos;
} Steroids_Object;

void steroids_object_draw (Steroids_Object *o,
			   int clipMode,
			   GR_WINDOW_ID wid);

/** Animates target accoring to velocity and accelleration.
 * 
 */
void steroids_object_animate (Steroids_Object *target);

/** Rotates object to absolute heading.
 *
 */
void steroids_object_rotate (float heading,
			     Steroids_Object *target);

/** Translates target to absolute position.
 *
 */
void steroids_object_translate (Steroids_Vector v,
				Steroids_Object *target);


/** steroids_object_collide 
 * 
 *  Returns true if object o1 collides with object o0.
 *
 *  Note: o0 must be a polygon object, o1 can be any type of object.
 *
 */
int steroids_object_collide (Steroids_Object *o0,
			     Steroids_Object *o1);

#endif
