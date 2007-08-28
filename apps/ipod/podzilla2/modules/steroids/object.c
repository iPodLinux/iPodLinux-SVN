
#include "globals.h"
#include "object.h"

void steroids_object_draw (Steroids_Object *o,
			   int clipMode,
			   GR_WINDOW_ID wid)
{
    GrSetGCForeground (steroids_globals.game_gc, o->colour);
    switch (o->type)
    {
    case STEROIDS_OBJECT_TYPE_POINT:
	GrPoint (wid,
		 steroids_globals.game_gc,
		 o->geometry.point.x,
		 o->geometry.point.y);
	break;
    case STEROIDS_OBJECT_TYPE_POLYGON:
	steroids_polygon_draw (&o->geometry.polygon, clipMode, wid);
	break;
    }
}



/** Animates target accoring to velocity and accelleration.
 * 
 */
void steroids_object_animate (Steroids_Object *target)
{
    Steroids_Vector oldPos;
    Steroids_Vector delta;

    oldPos.x = target->pos.x;
    oldPos.y = target->pos.y;

    target->velocity.x += target->accelleration.x;
    target->velocity.y += target->accelleration.y;

    target->pos.x += target->velocity.x;
    target->pos.y += target->velocity.y;

    if (target->pos.x < 0)
    {
	target->pos.x += steroids_globals.width;
    }
    else if (target->pos.x >= steroids_globals.width)
    {
	target->pos.x -= steroids_globals.width;
    }
    if (target->pos.y < 0)
    {
	target->pos.y += steroids_globals.height;
    }
    else if (target->pos.y >= steroids_globals.height)
    {
	target->pos.y -= steroids_globals.height;
    }

    delta.x = (int)(target->pos.x - oldPos.x + 0.5);
    delta.y = (int)(target->pos.y - oldPos.y + 0.5);
    if (delta.x != 0 || delta.y != 0)
    {
	steroids_object_translate (target->pos, target);
    }
}





/** Rotates object to absolute heading.
 *
 */
void steroids_object_rotate (float heading,
			     Steroids_Object *target)
{
    switch (target->type)
    {
    case STEROIDS_OBJECT_TYPE_POINT:
	break;
    case STEROIDS_OBJECT_TYPE_POLYGON:
	steroids_polygon_rotate (heading,
				 &target->geometry.polygon);
	break;
    }
}

/** Translates target to absolute position.
 *
 */
void steroids_object_translate (Steroids_Vector v,
				Steroids_Object *target)
{
    target->pos.x = v.x;
    target->pos.y = v.y;
    switch (target->type)
    {
    case STEROIDS_OBJECT_TYPE_POINT:
	target->geometry.point.x = (int)(v.x + 0.5);
	target->geometry.point.y = (int)(v.y + 0.5);
	break;
    case STEROIDS_OBJECT_TYPE_POLYGON:
	steroids_polygon_translate (v,
				    &target->geometry.polygon);
	break;
    }
}


/** steroids_object_collide 
 * 
 *  Returns true if object o1 collides with object o0.
 *
 *  Note: o0 must be a polygon object, o1 can be any type of object.
 *
 */
int steroids_object_collide (Steroids_Object *o0,
			     Steroids_Object *o1)
{
    int collide = 0;
    switch (o1->type)
    {
    case STEROIDS_OBJECT_TYPE_POINT:
	collide = steroids_polygon_pointInsideConvex (&o0->geometry.polygon, 
						      &o1->geometry.point);
	break;
    case STEROIDS_OBJECT_TYPE_POLYGON:
	collide = steroids_polygon_polygonInsideConvex (&o0->geometry.polygon, 
							&o1->geometry.polygon);
	break;
    }
    return collide;
}
