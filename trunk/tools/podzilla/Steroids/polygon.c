#include <stdio.h>
#include <string.h>
#include <math.h>

#include "globals.h"
#include "polygon.h"

void steroids_polygon_draw (Steroids_Polygon *p,
			    int clipMode,
			    GR_WINDOW_ID wid)
{
    // Draw polygon:
    GrPoly (wid, steroids_globals.game_gc, p->nPoints, p->point);
}


/** Copies polygon from source to target.
 *
 */
void steroids_polygon_copy (Steroids_Polygon *source, 
			    Steroids_Polygon *target)
{
    memcpy (target, source, sizeof (Steroids_Polygon));
}



/** Copies rotated points of source into target.
 *  Warning: Assumes that target is correctly set up as a clone of source.
 *
 */
void steroids_polygon_rotate (float angle,
			      Steroids_Polygon *target)
{
    int i;
    float cosA = cos (angle); 
    float sinA = sin (angle);
 
    for (i = 0; i < target->nPoints; i++)
    {
	target->rotPoint[i].x =   target->masterPoint[i].x * cosA
	                        - target->masterPoint[i].y * sinA;
	target->rotPoint[i].y =   target->masterPoint[i].x * sinA
	                        + target->masterPoint[i].y * cosA;
    }
}

/** Copies translated points from source into target.
 *  Warning: Assumes that target is correctly set up as a clone of source.
 *
 */
void steroids_polygon_translate (Steroids_Vector v,
				 Steroids_Polygon *target)
{
    int i;

    target->cog.x = (int)(v.x + 0.5);
    target->cog.y = (int)(v.y + 0.5);
    for (i = 0; i < target->nPoints; i++)
    {
	target->point[i].x = (int)(target->rotPoint[i].x + v.x + 0.5);
	target->point[i].y = (int)(target->rotPoint[i].y + v.y + 0.5);
    }
}



#define MIN(x,y) (x < y ? x : y)
#define MAX(x,y) (x > y ? x : y)

/** steroids_polygon_pointInsideConvex
 *
 *  Returns true if the given point is inside the polygon.
 *
 *  Note: Only works on convex polygons without holes.
 *
 */
int steroids_polygon_pointInsideConvex(Steroids_Polygon *polygon,
				       GR_POINT         *p)
{
  int counter = 0;
  int i;
  float xinters;
  GR_POINT p1, p2;

  p1 = polygon->point[0];
  for (i = 1; i <= polygon->nPoints; i++)
  {
    p2 = polygon->point[i % polygon->nPoints];
    if (p->y > MIN(p1.y, p2.y))
    {
      if (p->y <= MAX(p1.y, p2.y))
      {
        if (p->x <= MAX(p1.x, p2.x))
	{
          if (p1.y != p2.y)
	  {
            xinters = (p->y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y) + p1.x;
            if (p1.x == p2.x || p->x <= xinters)
	    {
              counter++;
	    }
          }
        }
      }
    }
    p1 = p2;
  }

  return (counter % 2 != 0); // If odd, then inside.
}


/** steroids_polygon_polygonInsideConvex
 *
 *  Returns true if any of the points in the given polygon p1 is inside
 *  the convex polygon p0.
 *
 *  Note: Only works on convex polygons p0 without holes, p1 may be an
 *        arbitrary polygon.
 *
 *  Note: If polygon p1 entirely contain polygon p0, all points are outside,
 *        hence this method will not consider this a collision.
 *
 */
int steroids_polygon_polygonInsideConvex(Steroids_Polygon *p0,
				         Steroids_Polygon *p1)
{
    int inside = 0;
    int i = 0;
    while (i < p1->nPoints && !inside)
    {
	inside = steroids_polygon_pointInsideConvex (p0, &p1->point[i]);
	i++;
    }
    return inside;
}
