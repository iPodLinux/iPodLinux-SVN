#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "hotdog.h"
#include "hotdog_animation.h"

typedef struct 
{
    hd_rect cur, delta, dest;
    int frames;
    void (*done)(hd_object *);
} anim_lineardata;

void HD_DoLinearAnimation (hd_object *obj) 
{
    anim_lineardata *a = (anim_lineardata *)obj->animdata;
    a->cur.x += a->delta.x; a->cur.y += a->delta.y;
    a->cur.w += a->delta.w; a->cur.h += a->delta.h;
    if (--a->frames <= 0) {
        void (*done)(hd_object *) = a->done;
        free (a); // so it works if done() calls HD_Animate*.
        obj->animdata = 0;
        obj->animate = 0;
        obj->animating = 0;
        // and *now* call the done func...
        if (done) (*done)(obj);
    }
}

void HD_AnimateLinear (hd_object *obj, int sx, int sy, int sw, int sh,
                       int dx, int dy, int dw, int dh, int frames, void (*done)(hd_object *))
{
    anim_lineardata *a = malloc (sizeof(anim_lineardata));
    assert (obj != NULL);
    assert (a != NULL);

    obj->animating = 1;
    obj->x = sx; obj->y = sy; obj->w = sw; obj->h = sh;
    obj->animdata = a;
    a->frames = frames;
    a->cur.x = (sx << 16); a->cur.y = (sy << 16);
    a->cur.w = (sw << 16); a->cur.h = (sh << 16);
    a->dest.x = (dx << 16); a->dest.y = (dy << 16);
    a->dest.w = (dw << 16); a->dest.h = (dh << 16);
    a->delta.x = (a->dest.x - a->cur.x) / frames;
    a->delta.y = (a->dest.y - a->cur.y) / frames;
    a->delta.w = (a->dest.w - a->cur.w) / frames;
    a->delta.h = (a->dest.h - a->cur.h) / frames;
    a->done = done;
    obj->animate = HD_DoLinearAnimation;
}

