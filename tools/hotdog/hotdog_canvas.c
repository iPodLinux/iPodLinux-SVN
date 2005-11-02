#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "hotdog.h"
#include "hotdog_canvas.h"

hd_canvas *HD_Canvas_Create(uint32 width,uint32 height) {
  hd_canvas *ret;

  ret = (hd_canvas *)malloc( sizeof(hd_canvas) );
  assert(ret != NULL);

  ret->argb = (uint32 *)malloc( width * height * 4 );
  assert(ret->argb != NULL);

  ret->w = width;
  ret->h = height;

  return(ret);
}

void     HD_Canvas_Destroy(hd_canvas *canvas) {
  if( canvas->argb != NULL) free(canvas->argb);

  free(canvas);
}

void HD_Canvas_Render(hd_engine *eng,hd_object *obj) {
  int32 fp_step_x,fp_step_y,fp_ix,fp_iy,fp_initial_ix,fp_initial_iy;
  int32 x,y;
  int32 startx,starty,endx,endy;
  uint32 buffOff, imgOff;
  
  fp_step_x = (obj->sub.png->w << 16) / obj->w;
  fp_step_y = (obj->sub.png->h << 16) / obj->h;

  // 1st, check if we need to do anything at all
  if( (obj->x > eng->screen.width) || (obj->y > eng->screen.height) ||
	  ((obj->x+obj->w) < 0) || ((obj->y+obj->h) < 0) ) {

	// Nope, we're entirely off-screen, so there's nothing for us to do
	return;

  // 2nd, check if we're completely inside the screen
  } else if( (obj->x >= 0) && ((obj->x+obj->w) < eng->screen.width) &&
             (obj->y >= 0) && ((obj->y+obj->h) < eng->screen.height) ) {
	
    startx = obj->x;
    starty = obj->y;
    endx   = obj->x+obj->w;
    endy   = obj->y+obj->h;

    fp_initial_ix = 0;
    fp_initial_iy = 0;
  } else {
    starty = obj->y;
	startx = obj->x;
    endy   = obj->y+obj->h;
    endx   = obj->x+obj->w;
    fp_initial_ix = 0;
    fp_initial_iy = 0;

    // Let the clipping commence
    if( obj->x < 0 ) {
		startx = 0;
		endx   = obj->w + obj->x;
		fp_initial_ix = -(obj->x) * fp_step_x;
	}
	if( (obj->x+obj->w) > eng->screen.width ) {
		endx = eng->screen.width - 1;
	}
    if( obj->y < 0 ) {
		starty = 0;
		endy   = obj->h + obj->y;
		fp_initial_iy = -(obj->y) * fp_step_y;
	}
	if( (obj->y+obj->h) > eng->screen.height ) {
		endy = eng->screen.height - 1;
	}
	
  }
  
  buffOff = starty * eng->screen.width;// + startx;
  imgOff  = 0;
  
  fp_iy = fp_initial_iy;
  for(y=starty;y<endy;y++) {
    fp_ix = fp_initial_ix;
    imgOff = (fp_iy>>16) * obj->sub.png->w;
    
    for(x=startx;x<endx;x++) {
      
      BLEND_ARGB8888_ON_ARGB8888( eng->buffer[ buffOff + x ], obj->sub.png->argb[ imgOff + (fp_ix>>16) ] );
      
      fp_ix += fp_step_x;
    }
    buffOff += eng->screen.width;
    fp_iy += fp_step_y;
  }
}
