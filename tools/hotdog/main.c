/*
 * Copyright (c) 2005, James Jacobsson
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this list
 * of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 * Neither the name of the organization nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior
 * written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "main.h"
#include "hotdog.h"
#include "hotdog_png.h"
#include "tinyptc_pub.h"

#define WIDTH  320
#define HEIGHT 240

void keyDown(int iKey) { 

	switch( iKey ) {
	case( 8 ): // Backspace
		break;
	case( 32 ): // Space
		break;
	case( 37 ): // Left
		break;
	case( 39 ): // Right
		break;
	case( 38 ): // Up
		break;
	case( 40 ): // Down
		break;
	default: 
		return;
	}
}

int main(void) {
	uint32 bIsPtcOpen = 0;
	uint16* pFB=0;
	hd_engine *engine;
	hd_object obj,obj2,obj3;
	hd_primitive pri,pri2;
	double f = 0.0;


	pFB = malloc( WIDTH*HEIGHT*2 );
	if( !ptc_open( "HGL Test", WIDTH, HEIGHT, 0 ) ) assert(0);
	bIsPtcOpen = 1;
	ptc_set_keymessage_callback( keyDown, 0 );

	engine = HD_Initialize(WIDTH,HEIGHT,pFB);

	obj.x = 10;
	obj.y = 10;
	obj.w = 50;
	obj.h = 50;
	obj.depth    = 1;
	obj.type     = HD_TYPE_PRIMITIVE;
	obj.sub.prim = &pri;
	obj.opacity  = 0xFF;
	pri.type  = HD_PRIM_RECTANGLE;
	pri.color = 0x80800000;

	obj2.x = 30;
	obj2.y = 30;
	obj2.w = 50;
	obj2.h = 50;
	obj2.depth    = 2;
	obj2.type     = HD_TYPE_PRIMITIVE;
	obj2.sub.prim = &pri2;
	pri2.type  = HD_PRIM_RECTANGLE;
	pri2.color = 0x80008000;

	obj3.x = 40;
	obj3.y = 10;
	obj3.w = 50;
	obj3.h = 50;
	obj3.depth    = 3;
	obj3.type     = HD_TYPE_PNG;
	obj3.sub.png  = HD_PNG_Create("C:/Projects/svn/K3/applications/iPodFrontRow/Mirrortest2/data/skin001/images/photos.png");

	HD_Register(engine,&obj);
	HD_Register(engine,&obj2);
	HD_Register(engine,&obj3);

	while( ptc_is_running() ) {
		HD_Render(engine);

		obj2.x = (int32)(30.0 + 20.0 * cos(f));
		obj2.y = (int32)(30.0 + 20.0 * sin(f));

		ptc_update_rgb565( pFB,0 );

		f += 0.01;
	}

	if( bIsPtcOpen ) 
		ptc_close( 0 );
}