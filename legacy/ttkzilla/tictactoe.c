/*
 * Copyright (C) 2004 Jeffrey Nelson
 * Brain based off minimax algorithm coded by Jacob Donham
 * Newer versions of code could focus on reducing ram and cpu used.
 * A difficulty setting could also be added however then users would 
 * not be as apt to attempt to beat the invincible A.I.
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
#include <time.h>
#include "pz.h"


// drawing stuff
void new_tictactoe_window (void);
static void reset_board();
static void draw_header();
static void drawXO(int pos, int shade, char theChar);
static void output(int stat);

// event handling
static int handle_event(GR_EVENT *event);

// Brains. Yum.
static void playerMadeMove(int pos);
static int board_to_int(char *board);
static int test_won();
static int minimax(int pl, int depth);
static void find_move();

static GR_WINDOW_ID tictactoe_wid;
static GR_GC_ID tictactoe_gc;
static GR_WINDOW_INFO wi;

static int gameRunning = 0;
static int currSquare;
static int difficulty = 6;
static char board[9];
static int vals[19683]; // This should be reduced.
enum { XWINS=1, OWINS=-1, TIE=2, NONE=0 };

void new_tictactoe_window(void)
{
	
    tictactoe_gc = pz_get_gc(1);       /* Get the graphics context */
	
    /* Open the window: */
    tictactoe_wid = pz_new_window (0,
								   21,
								   screen_info.cols,
								   screen_info.rows - (HEADER_TOPLINE+1),
								   draw_header,
								   handle_event);
	
	GrGetWindowInfo(tictactoe_wid, &wi); /* Get screen info */	
	
    /* Select the types of events you need for your window: */
    GrSelectEvents (tictactoe_wid, GR_EVENT_MASK_EXPOSURE|GR_EVENT_MASK_KEY_DOWN|GR_EVENT_MASK_KEY_UP);
	
    /* Display the window: */
    GrMapWindow (tictactoe_wid);
	
	reset_board(); 
}


static void reset_board()
{
	int index;
	for(index = 0; index < 9; index++)
		board[index] = '-';
	
	difficulty = 6;
	gameRunning = 1;
	currSquare = 0;
	draw_header(); 
	
    /* Clear the window */
    GrClearWindow (tictactoe_wid, GR_FALSE);
	
	GrSetGCUseBackground(tictactoe_gc, GR_TRUE);
    GrSetGCBackground(tictactoe_gc, WHITE);
    GrSetGCForeground(tictactoe_gc, BLACK);
	
	GrLine(tictactoe_wid, tictactoe_gc, wi.width * .90, (wi.height / 2.) - (wi.height / 2. * .33), 
		   wi.width - wi.width * .90, (wi.height / 2.) - (wi.height / 2. * .33));
	GrLine(tictactoe_wid, tictactoe_gc, wi.width * .90, (wi.height / 2.) + (wi.height / 2. * .33), 
		   wi.width - wi.width * .90, (wi.height / 2.) + (wi.height / 2. * .33));
	
	GrLine(tictactoe_wid, tictactoe_gc, (wi.width / 2.) - (wi.width / 2. * .33), wi.height * .90, 
		   (wi.width / 2.) - (wi.width / 2. * .33), wi.height - wi.height * .90);
	GrLine(tictactoe_wid, tictactoe_gc, (wi.width / 2.) + (wi.width / 2. * .33), wi.height * .90, 
		   (wi.width / 2.) + (wi.width / 2. * .33), wi.height - wi.height * .90);
	currSquare = 0;
	drawXO(currSquare, GRAY, 'x');
}


static void draw_header()
{
    if (gameRunning)
	pz_draw_header ("Tic-Tac-Toe");
}


void drawXO(int pos, int shade, char theChar)
{	
	int xPos, yPos, i;
	GrSetGCUseBackground(tictactoe_gc, GR_TRUE);
    GrSetGCBackground(tictactoe_gc, WHITE);
    GrSetGCForeground(tictactoe_gc, shade);
	
	xPos = wi.width * .25 * (pos % 3 + 1);
	yPos = wi.height * .25 * (pos / 3 + 1);
	
	if (theChar == 'o') {
		for ( i = 0; i < 4; i++)
			GrEllipse(tictactoe_wid, tictactoe_gc, xPos, yPos, wi.width * .03 + i, wi.height * .03 + i);
	} else if (theChar == 'x') {
		for ( i = 0; i < 4; i++) {
			GrLine(tictactoe_wid, tictactoe_gc, xPos - wi.width * .03 + i, yPos - wi.height * .03, 
				   xPos + wi.width * .03 + i, yPos + wi.height * .03);
			GrLine(tictactoe_wid, tictactoe_gc, xPos - wi.width * .03 + i, yPos+ wi.height * .03, 
				   xPos + wi.width * .03 + i, yPos - wi.height * .03);
		}
	}
}



static void output(int stat)
{
	gameRunning = 0;
	if (stat == XWINS)
		pz_draw_header ("You Wein!");
	else if (stat == OWINS)
		pz_draw_header ("You Loose!");
	else if (stat == TIE)
		pz_draw_header ("Tye!");
}


static int handle_event(GR_EVENT *event)
{
	int i;
    switch (event->type)
    {
		case GR_EVENT_TYPE_KEY_DOWN:
			switch (event->keystroke.ch)
			{
				case 'm': /* Menu button */
					pz_close_window (tictactoe_wid);
					break;
					
				case 'l': 
					if (gameRunning == 1) {
						for (i = currSquare - 1; board[i] != '-' && i != currSquare; i--)
							if (i <= -1)
								i = 9;
						if (board[currSquare] == '-')
							drawXO(currSquare, WHITE, 'x');
						drawXO(i, GRAY, 'x');
						currSquare = i;
					}
					break;	
				case 'r': 
					if (gameRunning == 1) {
						for (i = currSquare + 1; board[i] != '-' && i != currSquare; i++)
							if (i >= 9)
								i = -1;
						if (board[currSquare] == '-')
							drawXO(currSquare, WHITE, 'x');
						drawXO(i, GRAY, 'x');
						currSquare = i;
					}
					break;						
				case 'f':
					reset_board();
					break;	
				case 'w':
					if (!gameRunning)
						reset_board();
					break;
				case '\r':
					if (gameRunning)
						playerMadeMove(currSquare);
					else
						reset_board();
					break;
				default:
					break;
			}
			break; 
    }
	return 0;
}


static void playerMadeMove(int pos)
{
	int won;
	
	if (board[pos] == '-') {
		board[pos] = 'x';
		drawXO(currSquare, BLACK , 'x');
		if ((won = test_won()) != NONE) { 
			output(won);
		} else {
			find_move();
			if ((won = test_won()) != NONE)
				output(won);
		}	
	}
}


static int board_to_int(char *board)
{
	int i, out =0, exp=1;
	
	for (i=0; i<9; i++, exp *= 3) {
		switch(board[i]) {
			case '-': /* zero */
				break;
			case 'x': /* one */
				out += exp * 1;
				break;
			case 'o': /* two */
				out += exp * 2;
		}
	}
	
	return out;
}


static int test_won()
{
	int i, flag=0;
	char *b = board;
	
	if ((b[0] == 'x' && b[1] == 'x' && b[2] == 'x') ||
		(b[3] == 'x' && b[4] == 'x' && b[5] == 'x') ||
		(b[6] == 'x' && b[7] == 'x' && b[8] == 'x') || 
		(b[0] == 'x' && b[3] == 'x' && b[6] == 'x') || 
		(b[1] == 'x' && b[4] == 'x' && b[7] == 'x') || 
		(b[2] == 'x' && b[5] == 'x' && b[8] == 'x') || 
		(b[0] == 'x' && b[4] == 'x' && b[8] == 'x') || 
		(b[2] == 'x' && b[4] == 'x' && b[6] == 'x'))
		return XWINS;
	
	if ((b[0] == 'o' && b[1] == 'o' && b[2] == 'o') ||
		(b[3] == 'o' && b[4] == 'o' && b[5] == 'o') ||
		(b[6] == 'o' && b[7] == 'o' && b[8] == 'o') || 
		(b[0] == 'o' && b[3] == 'o' && b[6] == 'o') || 
		(b[1] == 'o' && b[4] == 'o' && b[7] == 'o') || 
		(b[2] == 'o' && b[5] == 'o' && b[8] == 'o') || 
		(b[0] == 'o' && b[4] == 'o' && b[8] == 'o') || 
		(b[2] == 'o' && b[4] == 'o' && b[6] == 'o'))
		return OWINS;
	
	for (i=0; i<9; i++) {
		if (b[i] == '-')
			flag=1;
	}
	if (!flag)
		return TIE;
	
	return NONE;
}


static int minimax(int pl, int depth)
{
	int best, val;
	int i, index;
	
	index = board_to_int(board);

	if ((vals[index] % 10) >= depth)
		return vals[index] - (vals[index] % 10); 
	/* since 0 is a valid value we don't want use the memorized value if it 
	 *is shallower than
	 * we're allowed to go--in fact, this doesn't matter since the memoization
	 * goes away with each move, but otherwise it would.
	 */
	
	if (depth > difficulty)
		return 0;
	
	if ((val = test_won(board)) != NONE) {
		switch(val) {
			case XWINS:
			case OWINS:
				return 1000 * val;
			case TIE:
				return 0;
		}
	}
	
	best = -pl * 1000000;
	for (i=0; i<9; i++) {
		if (board[i] == '-') {
			board[i] = (pl == 1) ? 'x' : 'o';
			val = minimax(-pl, depth+1);
			if (val * pl > best * pl)
				best = val;
			board[i] = '-';
		}
	}
	
	vals[index] = best + depth;
	return best;
}


static void find_move()
{
	int best, val, besti[9], i, bestcount = 0;
	time_t t1;
		
	best = 1000000;
	for (i=0; i<9; i++) {
		if (board[i] == '-') {
			board[i] = 'o';
			val = minimax(1, 1);
			if (val == best) {
				best = val;
				besti[bestcount++] = i;
			}
			else if (val < best) {
				best = val;
				bestcount = 0;
				besti[bestcount++] = i;
			}
			board[i] = '-';
		}
	}
	
	(void) time(&t1);
	srandom((long)t1);
	i = random()%bestcount;
	
	difficulty--;
	board[besti[i]] = 'o';
	drawXO(besti[i], BLACK,'o');
}
