#ifndef __STEROIDS_POLYGON_H__
#define __STEROIDS_POLYGON_H__

#include "../pz.h"
#include "vector.h"

#define STEROIDS_POLYGON_MAXPOINTS 10


typedef struct SteroidsPolygon
{
    GR_POINT cog;      // Center of bounding sphere, and center of rotation
    int      radius;   // Boudning sphere
    int      nPoints;
    GR_POINT point[STEROIDS_POLYGON_MAXPOINTS];
    GR_POINT rotPoint[STEROIDS_POLYGON_MAXPOINTS];
    GR_POINT masterPoint[STEROIDS_POLYGON_MAXPOINTS];
} Steroids_Polygon;

void steroids_polygon_draw (Steroids_Polygon *p,
			    int clipMode,
			    GR_WINDOW_ID wid);


/** Copies a polygon from source to target.
 *
 */
void steroids_polygon_copy (Steroids_Polygon *source, 
			    Steroids_Polygon *target);


/** Copies rotated points of source into target.
 *  Warning: Assumes that target is correctly set up as a clone of source.
 *
 */
void steroids_polygon_rotate (float angle,
			      Steroids_Polygon *target);

/** Copies translated points from source into target.
 *  Warning: Assumes that target is correctly set up as a clone of source.
 *
 */
void steroids_polygon_translate (Steroids_Vector v,
				 Steroids_Polygon *target);





/** steroids_polygon_pointInsideConvex
 *
 *  Returns true if the given point is inside the polygon.
 *
 *  Note: Only works on convex polygons without holes.
 *
 */
int steroids_polygon_pointInsideConvex(Steroids_Polygon *polygon,
				       GR_POINT         *p);




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
				         Steroids_Polygon *p1);
#endif
