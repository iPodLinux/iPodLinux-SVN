#ifndef __STEROIDS_SHOT_H__
#define __STEROIDS_SHOT_H__

#include "../pz.h"
#include "object.h"
#include "polygon.h"
#include "ship.h"

#define STEROIDS_SHOT_NUM     10
#define STEROIDS_SHOT_FORCE    3
#define STEROIDS_SHOT_CYCLES  30

typedef struct SteroidsShot
{
    int             active;
    long            cycles;    // The number of cycles the shot has been active
    Steroids_Object shape;

} Steroids_Shot;

#endif
