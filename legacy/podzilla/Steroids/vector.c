
#include <math.h>

#include "vector.h"

void steroids_vector_fromPolar (float angle, float radius, Steroids_Vector *v)
{
    v->x = radius * sin (angle);
    v->y = radius * -cos (angle);
}

