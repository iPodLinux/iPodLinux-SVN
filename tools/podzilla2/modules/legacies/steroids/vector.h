#ifndef __STEROIDS_VECTOR_H__
#define __STEROIDS_VECTOR_H__

#define PZ_COMPAT
#include "pz.h"

typedef struct SteroidsVector
{
    float x;
    float y;
} Steroids_Vector;

void steroids_vector_fromPolar (float angle, float radius, Steroids_Vector *v);


#endif
