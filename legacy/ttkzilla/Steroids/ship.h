#ifndef __STEROIDS_SHIP_H__
#define __STEROIDS_SHIP_H__

#include "../pz.h"
#include "object.h"
#include "polygon.h"

// 2*PI / 8 = 0.78539816339744825
#define STEROIDS_SHIP_ROTATION 0.78539816339744825
#define STEROIDS_SHIP_MIN_VELOCITY 0.001
#define STEROIDS_SHIP_MAINTHRUST 0.3
#define STEROIDS_SHIP_RETROTHRUST -0.1

#define STEROIDS_SHIP_EXHAUST_NUM        5
#define STEROIDS_SHIP_EXHAUST_THRUST     STEROIDS_SHIP_MAINTHRUST * -5
#define STEROIDS_SHIP_EXHAUST_LIFECYCLES 4

#define STEROIDS_SHIP_STATE_LIVE   0
#define STEROIDS_SHIP_STATE_THRUST 1
#define STEROIDS_SHIP_STATE_RETRO  2
#define STEROIDS_SHIP_STATE_DIE    3

#define STEROIDS_SHIP_DIE_TIME    15

#define STEROIDS_SHIP_WIDTH  4
#define STEROIDS_SHIP_HEIGHT 9


typedef struct SteroidsShip
{
    int state;
    Steroids_Object shape;
    float heading;
    int cannon; // Index into point list of shape object
    int engine; // Index into point list of shape object

    Steroids_Polygon *originalShape;

    int dieTime; // Counter for number of frames for the dying sequence
} Steroids_Ship;


#endif
