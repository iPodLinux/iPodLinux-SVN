/*
 * Steroids - just another asteroids clone 
 * Copyright (C) 2004  Fredrik Bergholtz
 *
 * Loosely based on the init code of bluecube by Sebastian Falbesoner.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../pz.h"

#include "globals.h"
#include "grafix.h"

#include "ship.h"
#include "ship_protos.h"
#include "asteroid.h"
#include "shot.h"
#include "shot_protos.h"


static void steroids_NewGame(void);
static void steroids_Game_Loop(void);
static void steroids_DrawScene(void);
void steroids_StartGameOverAnimation(void);
static void steroids_GameOverAnimation(void);

void steroids_ClearRect(int x, int y, int w, int h);


Steroids_Globals  steroids_globals;
Steroids_Ship     steroids_ship;
Steroids_Asteroid steroids_asteroid[STEROIDS_ASTEROID_NUM];
Steroids_Shot     steroids_shotShip[STEROIDS_SHOT_NUM];
Steroids_Shot     steroids_shotUFO;

static void steroids_do_draw()
{
    pz_draw_header("Steroids");
}

static void steroids_drawTopLeft (TWidget *this, ttk_surface srf)
{
    ttk_blit_image (steroids_globals.topLeft_wid->srf, srf, this->x, this->y);
}

static int steroids_handle_event (GR_EVENT *event)
{
    int ret = 0;

    switch (steroids_globals.gameState)
    {
    case STEROIDS_GAME_STATE_PLAY:
	switch (event->type)
	{
	case GR_EVENT_TYPE_TIMER:
	    steroids_Game_Loop();
	    break;

	case GR_EVENT_TYPE_KEY_DOWN:
	    switch (event->keystroke.ch)
	    {
	    case '\r': // Wheel button
		// Fire:
		steroids_ship_fire (steroids_shotShip,
				    &steroids_ship);
		break;

	    case 'h': // Play/pause button
		steroids_globals.gameState = STEROIDS_GAME_STATE_PAUSE;
		break;

	    case 'w': // Rewind button
		break;

	    case 'f': // Fast forward button
		// Engine thrust:
		steroids_ship_thrustOn (&steroids_ship);
		break;

	    case 'l': // Wheel left
		// Rotate ship clockwise
		steroids_ship_rotate (-STEROIDS_SHIP_ROTATION, &steroids_ship);
		break;

	    case 'r': // Wheel right
		// Rotate ship counter clockwise
		steroids_ship_rotate (STEROIDS_SHIP_ROTATION, &steroids_ship);
		break;

	    case 'm': // Menu button
		steroids_globals.gameState = STEROIDS_GAME_STATE_EXIT;
		break;

	    default:
		ret |= KEY_UNUSED;
		break;
	    } // keystroke
	    break;   // key down

	default:
	    ret |= EVENT_UNUSED;
	    break;
	} // event type
	break; // GAME_STATE_PLAY



    case STEROIDS_GAME_STATE_PAUSE:
	switch (event->type)
	{
	case GR_EVENT_TYPE_KEY_DOWN:
	    switch (event->keystroke.ch)
	    {
	    case 'm': // Menu button
		steroids_globals.gameState = STEROIDS_GAME_STATE_EXIT;
		break;

	    default:
		ret |= KEY_UNUSED;
		break;
	    }
	    break;

	case GR_EVENT_TYPE_KEY_UP:
	    switch (event->keystroke.ch)
	    {
	    case 'h': // Play/pause button
		steroids_globals.gameState = STEROIDS_GAME_STATE_PLAY;
		break;
	    }
	    break;

	default:
	    ret |= EVENT_UNUSED;
	    break;
	}
	break;
	


    case STEROIDS_GAME_STATE_GAMEOVER:
	switch (event->type)
	{
	case GR_EVENT_TYPE_KEY_DOWN:
	    switch (event->keystroke.ch)
	    {
	    case '\r': // Wheel button
		steroids_NewGame();
		break;

	    case 'm': // Menu button
		steroids_globals.gameState = STEROIDS_GAME_STATE_EXIT;
		break;

	    default:
		ret |= KEY_UNUSED;
		break;
	    }
	    break;

	default:
	    ret |= EVENT_UNUSED;
	    break;
	}
	break;


    case STEROIDS_GAME_STATE_EXIT:
	pz_header_unset_local();
	ttk_free_widget (steroids_globals.topLeft);
	GrDestroyGC (steroids_globals.topLeft_gc);
	GrDestroyGC (steroids_globals.game_gc);
	GrDestroyWindow(steroids_globals.topLeft_wid);
	pz_close_window(steroids_globals.game_wid);
	GrDestroyTimer(steroids_globals.timer_id);
	break;
    }




    return 1;
}

void new_steroids_window(void)
{
    /* Init randomizer */
    srand(time(NULL));

    steroids_globals.game_gc = pz_get_gc(1);
    steroids_globals.topLeft_gc = pz_get_gc(1);

    steroids_globals.width = screen_info.cols;
    steroids_globals.height = screen_info.rows
	                      - (HEADER_TOPLINE + 1);

    GrSetGCUseBackground(steroids_globals.topLeft_gc, GR_FALSE);

    steroids_globals.temp_wid = GrNewPixmap(screen_info.cols,
					    (screen_info.rows - (HEADER_TOPLINE + 1)),
					    NULL);

    steroids_globals.topLeft_wid = GrNewPixmap (STEROIDS_GAME_SHIPS * (STEROIDS_SHIP_WIDTH + 2) + 10, 18, 0);

    steroids_globals.game_wid = pz_new_window(0,
					 HEADER_TOPLINE + 1,
					 steroids_globals.width,
					 steroids_globals.height,
					 steroids_do_draw,
					 steroids_handle_event);
    GrSelectEvents(steroids_globals.game_wid,
		     GR_EVENT_MASK_EXPOSURE
		   | GR_EVENT_MASK_KEY_DOWN
		   | GR_EVENT_MASK_KEY_UP
		   | GR_EVENT_MASK_TIMER);
    GrMapWindow(steroids_globals.game_wid);

    /* set ships thingy to display in UL corner */
    steroids_globals.topLeft = ttk_new_widget (0, 0);
    steroids_globals.topLeft->w = STEROIDS_GAME_SHIPS * (STEROIDS_SHIP_WIDTH + 2) + 10;
    steroids_globals.topLeft->h = 18;
    steroids_globals.topLeft->draw = steroids_drawTopLeft;
    pz_header_set_local (steroids_globals.topLeft, 0);

    /* intialise game state */
    steroids_NewGame();

    steroids_globals.timer_id = GrCreateTimer(steroids_globals.game_wid, 150);
}





/*=========================================================================
// Name: NewGame()
// Desc: Starts a new game
//=======================================================================*/
static void steroids_NewGame()
{
    steroids_asteroid_initall (steroids_asteroid);
    steroids_ship_init (&steroids_ship);

    steroids_globals.score = 0; /* Reset score */
    steroids_globals.level = 0; /* Reset level */
    steroids_globals.ships = STEROIDS_GAME_SHIPS; /* Reset ship counter */

    steroids_ship_drawReserve(steroids_globals.topLeft_wid,
			      steroids_globals.topLeft_gc);
    steroids_globals.topLeft->dirty = 1;

    steroids_globals.gameState = STEROIDS_GAME_STATE_PLAY; /* Set playstate */
}


/*=========================================================================
// Name: Game_Loop()
// Desc: The main loop for the game
//=======================================================================*/
static void steroids_Game_Loop()
{
    int collide;
    int i;

    switch (steroids_globals.gameState)
    {
    case STEROIDS_GAME_STATE_PLAY:
	// Animate
	steroids_asteroid_animateall (steroids_asteroid);
	steroids_shot_animateall (steroids_shotShip, STEROIDS_SHOT_NUM);
	steroids_ship_animate (&steroids_ship);

	// Check ship collisions:
	collide = steroids_asteroid_collideShip (steroids_asteroid,
						 &steroids_ship);
// My own shots are lethal:
//      collide |= steroids_shot_collideShip (steroids_shot,
//    					      &steroids_ship);
	if (collide)
	{
	    if (steroids_ship_collided (&steroids_ship))
	    {
		steroids_globals.ships--;
		if (steroids_globals.ships < 0)
		{
		    steroids_globals.gameState = STEROIDS_GAME_STATE_GAMEOVER;
		}
		else
		{
		    steroids_ship_eraseReserve (steroids_globals.ships + 1,
						steroids_globals.topLeft_wid,
						steroids_globals.topLeft_gc);
		    steroids_globals.topLeft->dirty++;
		}
	    }
	}

	// Check asteroid collisions:
	collide = steroids_asteroid_collideShot (steroids_asteroid,
						 steroids_shotShip);
	if (collide)
	{
	    i = 0;
	    while (i < STEROIDS_ASTEROID_NUM
		   && !steroids_asteroid[i].active)
	    {
		i++;
	    }
	    if (i == STEROIDS_ASTEROID_NUM)
	    {
		steroids_globals.level++;
		steroids_asteroid_initall (steroids_asteroid);
	    }
	}
	break;

    case STEROIDS_GAME_STATE_GAMEOVER:
	steroids_GameOverAnimation();
	break;
    }

    steroids_DrawScene();
}

/*=========================================================================
// Name: DrawScene()
// Desc: Draws the whole scene!
//=======================================================================*/
static void steroids_DrawScene()
{
    char chScore[30];
    char chLevel[30];
//    int i;

    switch (steroids_globals.gameState)
    {
    case STEROIDS_GAME_STATE_PLAY:
	// Clear playfield:
	//
	GrSetGCForeground(steroids_globals.game_gc, WHITE);
	GrFillRect(steroids_globals.temp_wid,
		   steroids_globals.game_gc,
		   0, 0,
		   screen_info.cols, (screen_info.rows - (HEADER_TOPLINE + 1)));

	GrSetGCForeground(steroids_globals.game_gc, BLACK);
	/*
	for (i = 0; i < STEROIDS_ASTEROID_NUM; i++)
	{
	    if (steroids_asteroid[i].active)
	    {
		sprintf (chScore,
			 "(%0.2f, %0.2f)",
			 steroids_asteroid[i].shape.velocity.x,
			 steroids_asteroid[i].shape.velocity.y);
	    }
	    else
	    {
		sprintf (chScore, "Inactive");
	    }
	    GrText(steroids_globals.temp_wid,
		   steroids_globals.game_gc,
		   1, i * 11 + 11,
		   chScore,
		   -1,
		   GR_TFASCII);
	}
	*/
/*
	chScore[0] = '\0';
	for (i = 0; i < STEROIDS_SHOT_NUM; i++)
	{
	    sprintf (chScore, "%s%d", chScore, steroids_shotShip[i].active);
	}
	GrText(steroids_globals.temp_wid,
	       steroids_globals.game_gc,
	       1, 103,
	       chScore,
	       -1,
	       GR_TFASCII);
*/
/*
	sprintf (chScore, "%.2f, %.2f",
		 steroids_asteroid[0].shape.pos.x,
		 steroids_asteroid[0].shape.pos.y);
	GrText(steroids_globals.temp_wid,
	       steroids_globals.game_gc,
	       1, 90,
	       chScore,
	       -1,
	       GR_TFASCII);
	sprintf (chScore, "%d, %d",
		 steroids_ship.shape.geometry.polygon.cog.x,
		 steroids_ship.shape.geometry.polygon.cog.y);
	GrText(steroids_globals.temp_wid,
	       steroids_globals.game_gc,
	       1, 103,
	       chScore,
	       -1,
	       GR_TFASCII);
*/
	steroids_asteroid_drawall (steroids_asteroid,
				   steroids_globals.temp_wid);
	steroids_shot_drawall (steroids_shotShip,
			       STEROIDS_SHOT_NUM,
			       steroids_globals.temp_wid);
	steroids_ship_draw (&steroids_ship,
			    steroids_globals.temp_wid);

	GrCopyArea(steroids_globals.game_wid,
		   steroids_globals.game_gc,
		   0, 0,
		   screen_info.cols, (screen_info.rows - (HEADER_TOPLINE + 1)),
		   steroids_globals.temp_wid,
		   0, 0,
		   MWROP_SRCCOPY);
	break;


    case STEROIDS_GAME_STATE_GAMEOVER:
	GrSetGCForeground(steroids_globals.game_gc, WHITE);
	GrFillRect(steroids_globals.game_wid, steroids_globals.game_gc, 0, 0, 168, 128);
	GrSetGCForeground(steroids_globals.game_gc, BLACK);
	GrText(steroids_globals.game_wid, steroids_globals.game_gc,
	       45, 35,
	       "- Game Over -",
	       -1, GR_TFASCII);
	sprintf(chScore, "Your Score: %d", steroids_globals.score);
	GrText(steroids_globals.game_wid, steroids_globals.game_gc,
	       35, 65,
	       chScore,
	       -1,
	       GR_TFASCII);

	sprintf(chLevel, "Your Level: %d", steroids_globals.level + 1);
	GrText(steroids_globals.game_wid, steroids_globals.game_gc,
	       35, 95,
	       chLevel,
	       -1,
	       GR_TFASCII);
	break;
    }
}

/*=========================================================================
// Name: StartGameOverAnimation()
// Desc: Starts the gameover animation
//=======================================================================*/
void steroids_StartGameOverAnimation()
{
    steroids_globals.gameOver = 1;
}

/*=========================================================================
// Name: GameOverAnimation()
// Desc: Gameover Animation!
//=======================================================================*/
static void steroids_GameOverAnimation()
{
}
