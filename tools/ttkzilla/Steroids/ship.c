#include <stdlib.h>
#include <math.h>

#include "../pz.h"
#include "grafix.h"

#include "globals.h"
#include "ship.h"
#include "ship_protos.h"
#include "shot_protos.h"
#include "shot.h"
#include "polygon.h"

int initialized = 0;

#define shipNPoints 5
static GR_POINT shipPoint[] = {{ 0, -5},
			       {-2,  4},
			       { 0,  2},
			       { 2,  4},
			       { 0, -5}};

Steroids_Vector  startPos = {20, 20};
Steroids_Polygon modelShip;

static Steroids_Shot     exhaust[STEROIDS_SHIP_EXHAUST_NUM];


void initializeModels()
{
    int i;

    if (!initialized)
    {
	modelShip.cog.x = 0;
	modelShip.cog.y = 0;
        modelShip.nPoints = shipNPoints;
	for (i = 0; i < shipNPoints; i++)
	{
	    modelShip.masterPoint[i].x = shipPoint[i].x;
	    modelShip.masterPoint[i].y = shipPoint[i].y;
	}
	initialized = 1;
    }
}



void steroids_ship_init (Steroids_Ship *ship)
{
    int i;

    initializeModels();

    for (i = 0; i < STEROIDS_SHIP_EXHAUST_NUM; i++)
    {
	exhaust[i].active = 0;
    }

    ship->originalShape = &modelShip;
    ship->heading = 0;
    ship->cannon = 0;
    ship->engine = 2;
    ship->state = STEROIDS_SHIP_STATE_LIVE;

    // Initialize ship shape object:
    ship->shape.type = STEROIDS_OBJECT_TYPE_POLYGON;
    ship->shape.colour = BLACK;
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


void steroids_ship_draw (Steroids_Ship *ship, GR_WINDOW_ID wid)
{
    switch (ship->state)
    {
    case STEROIDS_SHIP_STATE_LIVE:
    case STEROIDS_SHIP_STATE_RETRO:
    case STEROIDS_SHIP_STATE_THRUST:
	steroids_object_draw (&ship->shape, 0, wid);
	break;

    case STEROIDS_SHIP_STATE_DIE:
	steroids_object_draw (&ship->shape, 0, wid);
	break;
    }

    steroids_shot_drawall (exhaust, 5, wid);
}



/** Animates ship according to velocity and accelleration.
 * 
 */
void steroids_ship_animate (Steroids_Ship *ship)
{
    Steroids_Vector v;

    switch (ship->state)
    {
    case STEROIDS_SHIP_STATE_RETRO:
	// Check if we stopped:
	if (   abs (ship->shape.velocity.x) < STEROIDS_SHIP_MIN_VELOCITY
	    && abs (ship->shape.velocity.y) < STEROIDS_SHIP_MIN_VELOCITY)
	{
	    // Disable retro thrusters:
	    ship->state = STEROIDS_SHIP_STATE_LIVE;
	    ship->shape.velocity.x = 0;
	    ship->shape.velocity.y = 0;
	    ship->shape.accelleration.x = 0;
	    ship->shape.accelleration.y = 0;
	}
	break;

    case STEROIDS_SHIP_STATE_THRUST:
	v.x = ship->shape.velocity.x;
	v.y = ship->shape.velocity.y;
	steroids_shot_new (exhaust,
			   STEROIDS_SHIP_EXHAUST_NUM,
			   ship->shape.geometry.polygon.point[ship->engine].x,
			   ship->shape.geometry.polygon.point[ship->engine].y,
			   &v,
			   ship->heading,
			   STEROIDS_SHIP_EXHAUST_THRUST,
			   STEROIDS_SHIP_EXHAUST_LIFECYCLES);

	break;

    case STEROIDS_SHIP_STATE_DIE:
	ship->dieTime--;
	if (ship->dieTime == 0)
	{
	    // Re-spawn:
	    ship->state = STEROIDS_SHIP_STATE_LIVE;
	    ship->shape.colour = BLACK;
	}
	else
	{
	    // Animate pieces:
	    if (ship->dieTime % 2 == 0)
	    {
		ship->shape.colour = (ship->shape.colour == BLACK) ? LTGRAY : BLACK;
	    }
	}
	break;
    }

    steroids_object_animate (&ship->shape);
    steroids_shot_animateall (exhaust, STEROIDS_SHIP_EXHAUST_NUM);
}






void rotate (float angle, Steroids_Ship *ship)
{
    Steroids_Vector v;

    ship->heading += angle;

    v.x = ship->shape.geometry.polygon.cog.x;
    v.y = ship->shape.geometry.polygon.cog.y;
    steroids_object_rotate (ship->heading, &ship->shape);
    ship->shape.geometry.polygon.cog.x = ship->originalShape->cog.x;
    ship->shape.geometry.polygon.cog.y = ship->originalShape->cog.y;
    steroids_object_translate (v, &ship->shape);
}
/** Rotates the given ship around it's cog.
 *
 */
void steroids_ship_rotate (float angle, Steroids_Ship *ship)
{
    switch (ship->state)
    {
    case STEROIDS_SHIP_STATE_LIVE:
    case STEROIDS_SHIP_STATE_RETRO:
	rotate (angle, ship);
	break;

    case STEROIDS_SHIP_STATE_THRUST:
	rotate (angle, ship);

	// Update thrust direction vector:
	ship->state = STEROIDS_SHIP_STATE_LIVE;
	steroids_ship_thrustOn (ship);
	break;
    }
}


/** Turns ship thruster engine on.
 *
 */
void steroids_ship_thrustOn (Steroids_Ship *ship)
{
    float           vLen;
    Steroids_Vector v;

    switch (ship->state)
    {
    case STEROIDS_SHIP_STATE_THRUST:
	// Enable retro thusters:
	vLen = STEROIDS_SHIP_RETROTHRUST
	       / sqrt (  ship->shape.velocity.x * ship->shape.velocity.x
		       + ship->shape.velocity.y * ship->shape.velocity.y);

	v.x = ship->shape.velocity.x * vLen;
	v.y = ship->shape.velocity.y * vLen;
	ship->state = STEROIDS_SHIP_STATE_RETRO;
	break;

    case STEROIDS_SHIP_STATE_LIVE:
    case STEROIDS_SHIP_STATE_RETRO:
	// Enable thrusters:
	steroids_vector_fromPolar (ship->heading, STEROIDS_SHIP_MAINTHRUST, &v);
	ship->state = STEROIDS_SHIP_STATE_THRUST;
	break;
    }
    ship->shape.accelleration.x = v.x;
    ship->shape.accelleration.y = v.y;
}

/** Turns ship retro thrusters off.
 *
 *  Not used, thrust on is now toggle.
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


/** Fires cannon.
 *
 */
void steroids_ship_fire (Steroids_Shot *shots,
			 Steroids_Ship *ship)
{
    switch (ship->state)
    {
    case STEROIDS_SHIP_STATE_LIVE:
    case STEROIDS_SHIP_STATE_RETRO:
    case STEROIDS_SHIP_STATE_THRUST:
	steroids_shot_newShip (shots,
			       STEROIDS_SHOT_NUM,
			       ship);
	break;
    }
}




/** Tell the ship that it has collided.
 *
 * Returns true if the state was changed to DIE, false if the ship was
 * already dying.
 *
 */
int steroids_ship_collided (Steroids_Ship *ship)
{
    int stateChanged = 0;

    switch (ship->state)
    {
    case STEROIDS_SHIP_STATE_LIVE:
    case STEROIDS_SHIP_STATE_THRUST:
    case STEROIDS_SHIP_STATE_RETRO:
	ship->state = STEROIDS_SHIP_STATE_DIE;
	ship->dieTime = STEROIDS_SHIP_DIE_TIME;

	// Kill all engines:
	ship->shape.accelleration.x = 0;
	ship->shape.accelleration.y = 0;

	stateChanged = 1;
	break;
    }
    return stateChanged;
}




void steroids_ship_drawWin (int x, int y, GR_WINDOW_ID wid, GR_GC_ID gc)
{
    static GR_POINT point[shipNPoints];
    int i;
    for (i = 0; i < shipNPoints; i++)
    {
	point[i].x = shipPoint[i].x + x;
	point[i].y = shipPoint[i].y + y;
    }
    GrSetGCForeground (gc, BLACK);
    GrPoly (wid, gc, shipNPoints, point);
}

void steroids_ship_drawReserve (GR_WINDOW_ID wid, GR_GC_ID gc)
{
    int i;
    int x = 7;
    int y = 10;

    GrSetGCForeground (gc, appearance_get_color( CS_TITLEBG ));
    GrFillRect( wid, gc, 0, 0, 
		 STEROIDS_GAME_SHIPS * (STEROIDS_SHIP_WIDTH + 2) + 10,
		 HEADER_TOPLINE );

    GrSetGCForeground (gc, appearance_get_color( CS_TITLEFG ));
    for (i = 0; i < STEROIDS_GAME_SHIPS; i++)
    {
	steroids_ship_drawWin (x, y,
			       wid,
			       gc);
	x += STEROIDS_SHIP_WIDTH + 2;
    }
}

void steroids_ship_eraseReserve (int ships, GR_WINDOW_ID wid, GR_GC_ID gc)
{
    int x = 7;
    int y = 10;

    x += ((ships - 1) * (STEROIDS_SHIP_WIDTH + 2))
	- (STEROIDS_SHIP_WIDTH / 2.0) + 0.5;
    y -= (STEROIDS_SHIP_HEIGHT / 2.0) + 0.5;

    GrSetGCForeground(gc, appearance_get_color( CS_TITLEBG ));
    GrFillRect(wid,
	       gc,
	       x,
	       y,
	       STEROIDS_SHIP_WIDTH + 1,
	       STEROIDS_SHIP_HEIGHT + 1);
}
