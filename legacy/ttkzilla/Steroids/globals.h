
#ifndef __STEROIDS_GLOBALS_H__
#define __STEROIDS_GLOBALS_H__

#include "../pz.h"

#define HEADLINE "Steroids"

/* Length of one frame */
#define TICK_INTERVAL  30 /* --> 33,33... fps */


/* State definitions */
#define STEROIDS_GAME_STATE_MENU     1
#define STEROIDS_GAME_STATE_PLAY     2
#define STEROIDS_GAME_STATE_PAUSE    3
#define STEROIDS_GAME_STATE_GAMEOVER 4
#define STEROIDS_GAME_STATE_EXIT     5
#define STEROIDS_GAME_STATE_CREDITS  6

#define STEROIDS_GAME_SHIPS  4


typedef struct SteroidsGlobals
{
    int score;
    int level;
    int ships;

    int gameState;

    int done;     // Want to exit?
    int pause;    // Pause?
    int gameOver; // Game over animation?
    int explode;  // Explosion animation?

    int            width;
    int            height;

    GR_TIMER_ID    timer_id;

    GR_WINDOW_ID   topLeft_wid;
    GR_GC_ID       topLeft_gc;

    GR_WINDOW_ID   game_wid;
    GR_WINDOW_ID   temp_wid;
    GR_GC_ID       game_gc;

    TWidget *topLeft;

} Steroids_Globals;



Steroids_Globals steroids_globals;

#endif
