/*
 * Copyright (C) 2004 Jeffrey Nelson
 * Tunnel is a game who'se objective is to navigate a ball through a tunnel
 * without touching the sides. I think I can call this game original considering
 * I thought of it after waking up from a horrible nightmare.
 *
 * Some improvements that could be done to this game could be:
 * - Game settings screen for chasm width/speed.
 * - Changed linked list implementation to a circular array-like implementation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdio.h> 
#include <time.h>
#include "pz.h"

// Window stuff
static GR_WINDOW_ID tunnel_wid;
static GR_GC_ID tunnel_gc;
static GR_WINDOW_INFO wi;
static GR_WINDOW_ID temp_pixmap;

#ifdef IPOD
#define SAVEFILE "/home/.tunnel"
#else
#define SAVEFILE ".tunnel"
#endif

// Timer stuff
static GR_TIMER_ID timer_id;

// Queue Stuff
typedef struct node
{
    int offset;         
    struct node *prev, *next;  
} QUEUENODE;

static QUEUENODE *head;
static QUEUENODE *middle;
static QUEUENODE *tail;

// Ball Stuff
typedef struct ball
{
	int radius, x;
} GAMEBALL;

static GAMEBALL ball;


// Game stuff
static int running;
static long int score = 0;
static long int highScore = 0;
static int chasmWidth = 25;
static int ballRadius = 5;
static int timerSpeed = 25;

static int hold = 0;

// function stuff
void new_tunnel_window();
static void draw_header();
static void draw_result();
static void reset();
static void advance_and_check();

static int handle_event(GR_EVENT *event);

// High Score reading/writing.
static void readHighScore();
static void writeHighScore();

// queue stuff.
static int new_chasm_queue(int key, int size, QUEUENODE **head,  QUEUENODE **middle, QUEUENODE **tail);
static void cycle_chasm_queue(int key, QUEUENODE **head,  QUEUENODE **middle, QUEUENODE **tail);\
static void reset_chasm_queue(QUEUENODE **head,  QUEUENODE **middle, QUEUENODE **tail);

// Creates a new tunnel "app" window
void new_tunnel_window(void)
{
	
    tunnel_gc = pz_get_gc(1);       /* Get the graphics context */
	
    /* Open the window: */
    tunnel_wid = pz_new_window (0,
								   21,
								   screen_info.cols,
								   screen_info.rows - (HEADER_TOPLINE+1),
								   draw_header,
								   handle_event);
	
	GrGetWindowInfo(tunnel_wid, &wi); /* Get screen info */	
	
    /* Select the types of events you need for your window: */
    GrSelectEvents (tunnel_wid, GR_EVENT_MASK_TIMER|GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN|GR_EVENT_MASK_KEY_UP);
	
	// set up pixmap 
	temp_pixmap = GrNewPixmap(screen_info.cols,
							  (screen_info.rows - (HEADER_TOPLINE + 1)),
							  NULL);
	
    /* Display the window: */
    GrMapWindow (tunnel_wid);
	draw_header();
	readHighScore();
	reset();
}

// Draws the header.
static void draw_header()
{
    pz_draw_header (_("Tunnel"));
}


// Draws a box with the score a person got and their high score.
static void draw_result()
{
	char strScore[30];
	char strHighScore[35];
	int height, width, base;
	sprintf(strScore, "Score:%li", score);
	if (highScore < score)
		highScore = score;
	sprintf(strHighScore, "High Score:%li", highScore);
	GrGetGCTextSize(tunnel_gc, strHighScore, -1, GR_TFASCII, &width, &height, &base);
	
	GrSetGCForeground(tunnel_gc, WHITE);
	GrFillRect(tunnel_wid, tunnel_gc, (wi.width - width) / 2 - 3, wi.height / 2 - height - 3, width + 8, height * 2 + 2);
	GrSetGCForeground(tunnel_gc, BLACK);
	GrRect(tunnel_wid, tunnel_gc, (wi.width - width) / 2 - 3, wi.height / 2 - height - 3, width + 8, height * 2 + 2);
	GrText (tunnel_wid, tunnel_gc, (wi.width - width) / 2, wi.height / 2 - height / 2 + 2, strScore, -1, GR_TFASCII );
	GrText (tunnel_wid, tunnel_gc, (wi.width - width) / 2, wi.height / 2 + height / 2 + 2, strHighScore, -1, GR_TFASCII );
	
}

// Resets the game.
static void reset()
{
	if (running == 1)
		GrDestroyTimer( timer_id );
	reset_chasm_queue(&head, &middle, &tail);
	if(new_chasm_queue((wi.width - chasmWidth) / 2, wi.height / 2, &head, &middle, &tail) == -1)
	{
		pz_error("Error making queue.");
		pz_close_window (tunnel_wid);
		return;
	}
	score = 0;
	running = 0;
	ball.radius = ballRadius;
	ball.x = wi.width / 2;
	timer_id = GrCreateTimer(tunnel_wid, timerSpeed); 
	advance_and_check();
}

// Advances the game, draws the frame, then checks to see if the ball hit a chasm wall.
static void advance_and_check()
{
	
	// Increment game
	time_t t1;
	int i;
	int y;
	QUEUENODE *node;
	GR_COLOR gr = GRAY;
	GR_COLOR wh = WHITE;
	
	GrSetGCForeground(tunnel_gc, WHITE);
	GrFillRect(temp_pixmap, tunnel_gc,
			   0, 0, screen_info.cols, (screen_info.rows - (HEADER_TOPLINE + 1)));
	
	(void) time(&t1);
	srandom((long)t1);
	i = random() % 3;
	if (i == 0 && tail->offset > 0)
		cycle_chasm_queue(tail->offset - 1, &head, &middle, &tail);
	else if (i == 1 && tail->offset + chasmWidth < wi.width)
		cycle_chasm_queue(tail->offset + 1, &head, &middle, &tail);
	else
		cycle_chasm_queue(tail->offset, &head, &middle, &tail);
	
	// Draw chasm
	if( screen_info.bpp >= 16 ) {
		gr = GR_RGB( 255, 128, 0 );
		wh = GR_RGB( 128,  50, 0 );
	}

	node = head;
	y = 0;
	while (node != NULL)
	{
		GrSetGCForeground(tunnel_gc, wh);
		GrFillRect(temp_pixmap, tunnel_gc, 0, y, screen_info.cols, 2);

		GrSetGCForeground(tunnel_gc, WHITE);
		GrFillRect(temp_pixmap, tunnel_gc, 
				node->offset, y, chasmWidth, 2);

		GrSetGCForeground(tunnel_gc, BLACK);
		GrFillRect(temp_pixmap, tunnel_gc, 
				node->offset-5, y, 5, 2);
		GrFillRect(temp_pixmap, tunnel_gc,
				node->offset + chasmWidth, y,
				5, 2);

		GrSetGCForeground(tunnel_gc, gr);
		GrFillRect(temp_pixmap, tunnel_gc, 
				node->offset-8, y, 3, 2);
		GrFillRect(temp_pixmap, tunnel_gc,
				node->offset + chasmWidth+5, y,
				3, 2);

		node = node->next;
		y+=2;
	}
	
	// Draw ball
    GrSetGCForeground(tunnel_gc, BLACK);
	GrFillEllipse(temp_pixmap ,tunnel_gc, ball.x, wi.height / 2, ball.radius, ball.radius);
	
	// Map window 
	GrCopyArea(tunnel_wid, tunnel_gc, 0, 0,
			   screen_info.cols, (screen_info.rows - (HEADER_TOPLINE + 1)),
			   temp_pixmap, 0, 0, MWROP_SRCCOPY);
	
	// Check for collisions. Stop game if one is found.
	if (middle->offset >= ball.x - ball.radius || middle->offset + chasmWidth < ball.x + ball.radius)
	{
		running = 0;
		GrDestroyTimer( timer_id );
		running = 1;
		draw_result();
	}
	
	// increment score
	score++;
}

// Handles timer/key events.
static int handle_event(GR_EVENT *event)
{
    switch (event->type)
    {
		case GR_EVENT_TYPE_TIMER:
			if (running && !hold) 
				advance_and_check();
			break;
		case GR_EVENT_TYPE_KEY_DOWN:
			switch (event->keystroke.ch)
			{
				case 'm': // Menu button.
					GrDestroyTimer( timer_id );
					writeHighScore();
					pz_close_window (tunnel_wid);
					break;
				case 'w': // rewind button
				case 'l': // scroll wheel left
					if (running && ball.radius < ball.x)
						ball.x -= 2;
					break;	
				case 'f': // fast forward button
				case 'r': // scroll wheel right
					if (running && wi.width - 2 - ball.radius  > ball.x)
						ball.x += 2;
					break;						
				case 'd': // play button. Who decided to make it d!
					if (running == 0)
						running = 1;
					else
						reset();
					break;
				case 'h': // Hold button
					hold = 1;
					break;					
				default:
					break;
			}
			break; 
		case GR_EVENT_TYPE_KEY_UP:
			switch( event->keystroke.ch )
			{
				case 'h': // Hold button
					hold = 0;
					break;
			}
			break;
		default:
			break;
    }
	return 0;
}

// Reads high score from file in SAVEFILE
static void readHighScore()
{
	FILE *input;
	if ((input = fopen(SAVEFILE, "r")) == NULL)
	{
		perror(SAVEFILE);
		return;
	}
	fscanf(input, "%ld", &highScore); 
	fclose(input);
}

// Writes high score to file in SAVEFILE
static void writeHighScore()
{
	FILE *output;
	if ((output = fopen(SAVEFILE, "w")) == NULL)
	{
		perror(SAVEFILE);
		return;
	}
	fprintf(output, "%ld", highScore);
	fclose(output);
}

// Sets up a linked list
static int new_chasm_queue(int key, int size, QUEUENODE **head,  QUEUENODE **middle, QUEUENODE **tail)
{
	int i;
	QUEUENODE *newNode;

	(*head) = NULL;
	(*middle) = NULL;
	(*tail) = NULL;
	
	if (key <= 0) {
		return -1;
	}

	if((newNode=(QUEUENODE *)malloc(sizeof(QUEUENODE))) == NULL)
		return -1;
	
	newNode->prev = NULL;
	newNode->next = NULL;
	newNode->offset = key;
	(*head) = newNode;
	(*tail) = newNode;
	
	for (i = 1; i < size; i++)
	{
		if((newNode=(QUEUENODE *)malloc(sizeof(QUEUENODE))) == NULL)
			return -1;
		newNode->offset = key;
		newNode->prev = (*tail);
		(*tail)->next = newNode;
		(*tail) = newNode;
		if ((*middle) == NULL && i >= size / 2)
			(*middle) = newNode;
	}
	
	(*tail)->next = NULL;
	return 0;
}


// Moves node at top of list to bottom of list.
// This function assumes there are more than say 3 defined nodes in a healthy queue
static void cycle_chasm_queue(int key, QUEUENODE **head,  QUEUENODE **middle, QUEUENODE **tail) 
{
	QUEUENODE *tmpNode;
	if ((*head) == NULL)
		return;
	
	
	/*
	 requires removing top node, adding it to the bottom, then cleaning up.
	 */
	
	tmpNode = (*head);
	(*head) = (*head)->next;
	(*head)->prev = NULL;

	tmpNode->prev = (*tail);
	(*tail)->next = tmpNode;
	tmpNode->next = NULL;
	(*tail) = tmpNode;
	
	(*middle) = (*middle)->next;
	tmpNode->offset = key;
}

// Destroys a linked list.
static void reset_chasm_queue(QUEUENODE **head,  QUEUENODE **middle, QUEUENODE **tail)
{
	QUEUENODE *curNode;
	while ((*head) != NULL) {
		curNode = (*head);
		(*head) = curNode->next;
		free(curNode);
	}
	
	(*head) = NULL;
	(*middle) = NULL;
	(*tail) = NULL;
}
