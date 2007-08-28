/*
 *	TuxChess - Copyright (c) 2002, Steven J. Merrifield
 *	Chess engine - Copyright 1997 Tom Kerrigan
 *	Modified by djaconil for the iPod
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include "defs.h"
#include "data.h"
#include "protos.h"
#include "../ipod.h"
#include "../pz.h"

#define LASTGAME ".tuxchess"

#define TITLE 		"TuxChess"
#define SCANCODES	64
#if 0
#define BM_WIDTH	394
#define BM_HEIGHT	413
#else
#define BM_WIDTH 	160
#define BM_HEIGHT 	128
#endif

#define WHEEL_EVENT_MOD 3

static GR_WINDOW_ID tuxchess_wid;
static GR_WINDOW_ID historic_wid;
static GR_WINDOW_ID end_wid;
static GR_WINDOW_ID message_wid;
static GR_GC_ID tuxchess_gc;

char cpu_move[8];

/* Horrible code */
char historic_line1[10];
char historic_line2[10];
char historic_line3[10];
char historic_line4[10];
char historic_line5[10];
char historic_line6[10];
int move_nb;
char key_pressed ='\0';

static char xcoord='A', ycoord='1', oldx='A', oldy='1';
static char oldx_curs='M', oldy_curs='9';
int contr=110, right_hit=0, left_hit=0, click=1, end=0;
int is_mini=0, readint = 0, writeint = 0;
char end_type='w';
void draw_cursor(char x, char y);
void draw_end(char col);
static int sel=0;
struct keycolumn {
        short xoffset;
        short scancode;
};

struct keyrow {
        short yoffset;
        short height;
        struct keycolumn columns[9];
};

struct keyrow keyrows[8] = {
        {0, 49, 
         {{0,0}, {49,1}, {98,2}, {147,3}, {196,4}, {245,5}, {294,6}, {343,7}, {999,-1}}},
        {49, 49, 
         {{0,8}, {49,9}, {98,10}, {147,11}, {196,12}, {245,13}, {294,14}, {343,15}, {999,-1}}},
        {98, 49, 
         {{0,16}, {49,17}, {98,18}, {147,19}, {196,20}, {245,21}, {294,22}, {343,23}, {999,-1}} },
        {147, 49, 
         {{0,24}, {49,25}, {98,26}, {147,27}, {196,28}, {245,29}, {294,30}, {343,31}, {999,-1}}},
        {196, 49, 
         {{0,32}, {49,33}, {98,34}, {147,35}, {196,36}, {245,37}, {294,38}, {343,39}, {999,-1}}},
        {245, 49, 
         {{0,40}, {49,41}, {98,42}, {147,43}, {196,44}, {245,45}, {294,46}, {343,47}, {999,-1}}},
        {294, 49, 
         {{0,48}, {49,49}, {98,50}, {147,51}, {196,52}, {245,53}, {294,54}, {343,55}, {999,-1}}},
        {343, 49, 
         {{0,56}, {49,57}, {98,58}, {147,59}, {196,60}, {245,61}, {294,62}, {343,63}, {999,-1}}}
};

static char *board_position[SCANCODES] = {
	"A8","B8","C8","D8","E8","F8","G8","H8",
	"A7","B7","C7","D7","E7","F7","G7","H7",
	"A6","B6","C6","D6","E6","F6","G6","H6",
	"A5","B5","C5","D5","E5","F5","G5","H5",
	"A4","B4","C4","D4","E4","F4","G4","H4",
	"A3","B3","C3","D3","E3","F3","G3","H3",
	"A2","B2","C2","D2","E2","F2","G2","H2",
	"A1","B1","C1","D1","E1","F1","G1","H1"};


int start_square = 1;
char st_sq[3];
int from = 999;
int to = 999;

void last_tuxchess_window(void);

/* Process scancode ?! */
static void process_scancode(int scancode)
{
	char sq[3];
	char fin_sq[3];
	int i;

	strcpy(sq,"");
	for (i=0;i<SCANCODES;i++)
	{
		if (i == scancode)
		{
			strcpy(sq,board_position[i]);
			break;
		}
	}


	if (start_square)
	{
		strcpy(st_sq,sq);
		start_square = 0;
		from = scancode;
		to = 999;
	}
	else
	{
		strcpy(fin_sq,sq);
		start_square = 1;
		to = scancode;
	}
}

/* Draw the message if it's a mini */ 
void draw_message(char *msg1, char *msg2)
{
	int offset;

	/* Clear the window */
	GrClearWindow(message_wid, GR_FALSE);

	/* Put the foreground and background in good shapes */
	GrSetGCForeground(tuxchess_gc, BLACK);
	GrSetGCBackground(tuxchess_gc, WHITE);

	/* Draw the "window" */
	GrLine(message_wid, tuxchess_gc, 1, 0, 34, 0);
	GrLine(message_wid, tuxchess_gc, 1, 16, 34, 16);
	GrLine(message_wid, tuxchess_gc, 1, 0, 1, 110);
	GrLine(message_wid, tuxchess_gc, 34, 0, 34, 110);
	GrLine(message_wid, tuxchess_gc, 1, 110, 34, 110);
	GrText(message_wid, tuxchess_gc, 3,13, 
		"Chess", -1, GR_TFASCII);  /* Title in the text box */

	GrText(message_wid, tuxchess_gc, 3,73, msg1, -1, GR_TFASCII);

	if ((strcmp(msg2, "Play") == 0) || (strcmp(msg2, "     ") == 0))
	{
		offset = 3;
		msg2 = "Play";
	}
	else
	{
		offset = 0;
	}

	GrText(message_wid, tuxchess_gc, 4+offset,53, msg2, -1, GR_TFASCII);
}

/* ***********************************************************/
void init_historic(void)
{
	strcpy(historic_line1,"         ");
	strcpy(historic_line2,"         ");
	strcpy(historic_line3,"         ");
	strcpy(historic_line4,"         ");
	strcpy(historic_line5,"         ");
	strcpy(historic_line6,"         ");
	move_nb = 0;
}

/* ***********************************************************/
void draw_historic(void)
{
	/* Clear the window */
	GrClearWindow (historic_wid, GR_FALSE);

	/* Put the foreground and background in good shapes */
	GrSetGCForeground(tuxchess_gc, BLACK);
	GrSetGCBackground(tuxchess_gc, WHITE);

	/* Draw the "window" */
	GrLine(historic_wid, tuxchess_gc, 1, 1, 55, 1);
	GrLine(historic_wid, tuxchess_gc, 1, 1, 1, 104);
	GrLine(historic_wid, tuxchess_gc, 1, 105, 55, 105);
	GrLine(historic_wid, tuxchess_gc, 55, 1, 55, 105);
	GrText(historic_wid, tuxchess_gc, 14,14, 
		"Moves", -1, GR_TFASCII);	/* Title in the text box */
	GrLine(historic_wid, tuxchess_gc, 1, 17, 56, 17);

	/* So ugly code since char* arrays seem to confuse my iPod - writes 6 lines */
	GrText(historic_wid, tuxchess_gc, 6, 33, historic_line1, -1, GR_TFASCII);
	GrText(historic_wid, tuxchess_gc, 6, 46, historic_line2, -1, GR_TFASCII);
	GrText(historic_wid, tuxchess_gc, 6, 59, historic_line3, -1, GR_TFASCII);
	GrText(historic_wid, tuxchess_gc, 6, 72, historic_line4, -1, GR_TFASCII);
	GrText(historic_wid, tuxchess_gc, 6, 85, historic_line5, -1, GR_TFASCII);
	GrText(historic_wid, tuxchess_gc, 6, 99, historic_line6, -1, GR_TFASCII);
}


/* ***********************************************************/
void reorg_historic(int player)
{
	char s[12];

	if (player == 1) {
		sprintf(s, "j1: %c%c-%c%c",oldx,oldy,xcoord,ycoord);
	}
	else if (player == 2) {
		sprintf(s, "j2: %s", cpu_move);
	}
	else {
		strcpy(s, "         ");
	}

	move_nb++;

	/* So ugly code since char* arrays seem
	to confuse my iPod */
	strcpy(historic_line6, historic_line5);
	strcpy(historic_line5, historic_line4);
	strcpy(historic_line4, historic_line3);
	strcpy(historic_line3, historic_line2);
	strcpy(historic_line2, historic_line1);
	strcpy(historic_line1, s);

	if (!is_mini) {
		draw_historic();
	}
}

/* Set up a new game */
void new_game()
{
	end = 0;
	pz_close_window(end_wid);
	init();
	gen_moves();
	max_time = 100000;//1 << 25;
	max_depth = 1;

	init_historic();
	print_board();
	if (!is_mini) {
		draw_historic();
	}
	else {
		draw_message("", "");
	}
}

/* Check the validity of the move */
BOOL check_validity(void)
{
	char s[256];
	int i;
	BOOL found;

	if (to != 999)
	{
		/* loop through the moves to see if it's legal */
		found = FALSE;
		for (i = 0; i < first_move[1]; ++i) {
			if (gen_dat[i].m.b.from == from && gen_dat[i].m.b.to == to) 
			{
				found = TRUE;

				/* get the promotion piece right */
				if (gen_dat[i].m.b.bits & 32) {
					switch (s[4]) 
					{
					case 'N':
						break;
					case 'B':
						i += 1;
						break;
					case 'R':
						i += 2;
						break;
					default:
						i += 3;
						break;
					}
				}
				break;
			}
		}

		if (!found || !makemove(gen_dat[i].m.b)) {
			printf("Illegal move.\n");
			if (is_mini) {
				draw_message("     ","Nope");
			}
			else {
				pz_draw_header("Illegal move !");
			}
			return FALSE;
		}
		else {
			ply = 0;
			gen_moves();
			print_board();

			print_result();
			to = 999;
			return TRUE;
		}
	} /* if to != 999 */
	else {
		return FALSE;
	}
}

/* Let's make the iPod think about ;-) */
void computer_play(void)
{
	if (!is_mini) {
		pz_draw_header("computer play");
	}

	/* think about the move and make it */
	//sleep(1);
	printf("think\n");
	think(2);

	//sleep(1);
	if (!pv[0][0].u) {
		printf("No legal moves\n");
		if (!is_mini) {
			pz_draw_header ("No legal moves");
		}
	}

	sprintf(cpu_move,"%s", move_str(pv[0][0].b));

	makemove(pv[0][0].b);
	ply = 0;
	gen_moves();
	if (in_check(LIGHT)) {
		if (is_mini) {
			draw_message("     ", "Check");
		}
		else {
			pz_draw_header("Check");
		}
	}
	else
	{
		if (is_mini) {
			draw_message("     ", "Play");
		}
		else {
			pz_draw_header("Your Turn");
		}
	}
}

/* When pressing menu - quits*/
void do_menu(void)
{
	int i;
	FILE *writefile;

	readint = 0;

	/* Save last board */
	printf("Save last board \n");
	pz_draw_header("Saving board...");
	if ((writefile = fopen(LASTGAME, "w"))) {
		for (i = 0; i < 64; i++) {
			writeint = piece[i];
			fwrite(&writeint,sizeof(writeint),1,writefile);
		}
		for (i = 0; i < 64; i++) {
			writeint = color[i];
			fwrite(&writeint,sizeof(writeint),1,writefile);
		}
		fwrite(&historic_line1, sizeof(historic_line1), 1, writefile);
		fwrite(&historic_line2, sizeof(historic_line2), 1, writefile);
		fwrite(&historic_line3, sizeof(historic_line3), 1, writefile);
		fwrite(&historic_line4, sizeof(historic_line4), 1, writefile);
		fwrite(&historic_line5, sizeof(historic_line5), 1, writefile);
		fwrite(&historic_line6, sizeof(historic_line6), 1, writefile);
		fclose(writefile);
	}
	else {
		printf("Cant write %s\n", LASTGAME);
	}

	if (end == 2) {
		pz_close_window(end_wid);
	}

	if (is_mini) {
		pz_close_window(message_wid);
	}
	else {
		pz_close_window(historic_wid);
	}

	pz_close_window(tuxchess_wid);

	GrDestroyGC(tuxchess_gc);
}

/* Common function for cursor positionning */
void new_cursor_position(void)
{
	char temp1[5], temp2[12];

	print_board();

	if (in_check(LIGHT))
	{
		if (sel) {
			sprintf(temp2, "Check! %c%c-%c%c",oldx,oldy,xcoord,ycoord);
		}
		else {
			sprintf(temp2, "Check! %c%c-",xcoord,ycoord);
		}
		if (!is_mini) {
			pz_draw_header(temp2);
		}
	}
	else
	{
		if(sel) {
			sprintf(temp1,"%c%c-%c%c",oldx,oldy,xcoord,ycoord);
		}
		else {
			sprintf(temp1,"%c%c-",xcoord,ycoord);
		}
		if (is_mini) {
			draw_message(temp1,"     ");
		}
		else {
			pz_draw_header(temp1);
		}
	}

	print_result();
	printf(temp1);
	printf("\n");
}

/* Move the cursor to the left horizontally */
void do_rev(void)
{
	oldx_curs = xcoord;
	oldy_curs = ycoord;
	if (xcoord>'A') {
		xcoord--;
	}
	else {
		if (ycoord!='1') {
			ycoord--;
			xcoord='H';
		}
	}

	new_cursor_position();
}

/* Move the cursor to the right horizontally */
void do_fwd(void)
{
	oldx_curs = xcoord;
	oldy_curs = ycoord;
	if (xcoord<'H') {
		xcoord++;
	}
	else {
		if (ycoord!='8') {
			ycoord++;
			xcoord='A';
		}
	}
	new_cursor_position();
}

// Select the position of the cursor
void do_action(void)
{
	process_scancode(keyrows[7-(ycoord-'1')].columns[xcoord-'A'].scancode);
	sel=!sel;

	if (sel==0) {
		if (check_validity() == TRUE) {
			reorg_historic(1);
			print_board();
			computer_play();
			reorg_historic(2);
			//print_board();
			new_cursor_position();
		}
	}
	else {
		oldx=xcoord;
		oldy=ycoord;
	}
}

/* Move the cursor down */
void do_right(void)
{
	oldx_curs = xcoord;
	oldy_curs = ycoord;
	if (ycoord>'1') {
		ycoord--;
	}
	else {
		if (xcoord!='A') {
			xcoord--;
			ycoord='8';
		}
	}

	new_cursor_position();
}

/* Move the cursor up */
void do_left(void)
{
	oldx_curs = xcoord;
	oldy_curs = ycoord;
	if (ycoord<'8') {
		ycoord++;
	}
	else {
		if (xcoord!='H') {
			xcoord++;
			ycoord='1';
		}
	}

	new_cursor_position();
}

/* Draw the cursor */
void draw_cursor(char coord1, char coord2)
{
	int x,y;

	x = (coord1-65)*13;
	y = 106-((coord2-48)*13);

	GrSetGCForeground(tuxchess_gc,WHITE);
	GrRect(tuxchess_wid,tuxchess_gc,x,y,13,13);
	GrSetGCForeground(tuxchess_gc,BLACK);
	GrRect(tuxchess_wid,tuxchess_gc,x+1,y+1,11,11);
	GrSetGCForeground(tuxchess_gc,WHITE);
	GrRect(tuxchess_wid,tuxchess_gc,x+2,y+2,9,9);
}

/* Draw the rook */
void draw_rook(int coord_x, int coord_y, char color)
{
	GR_POINT rook[] = {
		{coord_x+1, coord_y+1}, {coord_x+3, coord_y+1},
		{coord_x+3, coord_y+3}, {coord_x+5, coord_y+3},
		{coord_x+5, coord_y+1}, {coord_x+7, coord_y+1},
		{coord_x+7, coord_y+3}, {coord_x+9, coord_y+3},
		{coord_x+9, coord_y+1}, {coord_x+11, coord_y+1}, 
		{coord_x+11, coord_y+4}, {coord_x+9, coord_y+4},
		{coord_x+9, coord_y+8}, {coord_x+11, coord_y+8},
		{coord_x+11, coord_y+11}, {coord_x+1, coord_y+11}, 
		{coord_x+1, coord_y+8}, {coord_x+3, coord_y+8}, 
		{coord_x+3, coord_y+4}, {coord_x+1, coord_y+4},
		{coord_x+1, coord_y+1}
	};

	if(color == 'w')
	{
		GrSetGCForeground(tuxchess_gc,WHITE);
		GrFillPoly(tuxchess_wid,tuxchess_gc,21,rook);
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrPoly(tuxchess_wid,tuxchess_gc,21,rook);
	}
	else
	{
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrFillPoly(tuxchess_wid,tuxchess_gc,21,rook);
	}
}

/* Draw the queen */
void draw_queen(int coord_x, int coord_y, char color)
{
	GR_POINT queen[] = {
		{coord_x+1, coord_y+1}, {coord_x+4, coord_y+4},
		{coord_x+6, coord_y+1}, {coord_x+8, coord_y+4},
		{coord_x+11, coord_y+1}, {coord_x+9, coord_y+10},
		{coord_x+3, coord_y+10}, {coord_x+1, coord_y+1}
	};

	if (color == 'w')
	{
		GrSetGCForeground(tuxchess_gc,WHITE);
		GrFillPoly(tuxchess_wid,tuxchess_gc,8,queen);
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrPoly(tuxchess_wid,tuxchess_gc,8,queen);
	}
	else
	{
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrFillPoly(tuxchess_wid,tuxchess_gc,8,queen);
		GrPoly(tuxchess_wid,tuxchess_gc,8,queen);
	}
}

/* Draw the knight */
void draw_knight(int coord_x, int coord_y, char color)
{
	GR_POINT knight[] = {
		{coord_x+5, coord_y+1}, {coord_x+8, coord_y+3},
		{coord_x+11, coord_y+11}, {coord_x+1, coord_y+11},
		{coord_x+5, coord_y+9}, {coord_x+5, coord_y+4},
		{coord_x+3, coord_y+6}, {coord_x+1, coord_y+6},
		{coord_x+5, coord_y+1}
	};

	if(color == 'w')
	{
		GrSetGCForeground(tuxchess_gc,WHITE);
		GrFillPoly(tuxchess_wid,tuxchess_gc,9,knight);
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrPoly(tuxchess_wid,tuxchess_gc,9,knight);
	}
	else 
	{
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrFillPoly(tuxchess_wid,tuxchess_gc,9,knight);
		GrPoly(tuxchess_wid,tuxchess_gc,9,knight);
	}
}

/* Draw the king */
void draw_king(int coord_x, int coord_y, char color)
{
	GrSetGCForeground(tuxchess_gc,BLACK);
	GrLine(tuxchess_wid,tuxchess_gc,coord_x+6,coord_y+1,coord_x+6,coord_y+4);
	GrLine(tuxchess_wid,tuxchess_gc,coord_x+4,coord_y+2,coord_x+8,coord_y+2);
	if(color == 'w')
		GrSetGCForeground(tuxchess_gc,WHITE);

	GrFillEllipse(tuxchess_wid,tuxchess_gc,coord_x+6,coord_y+8,4,4);
	GrSetGCForeground(tuxchess_gc,BLACK);
	GrEllipse(tuxchess_wid,tuxchess_gc,coord_x+6,coord_y+8,4,4);
}

/* Draw the bishop */
void draw_bishop(int coord_x, int coord_y, char color)
{
	GR_POINT bishop1[] = {
		{coord_x+7, coord_y+1}, {coord_x+8, coord_y+4},
		{coord_x+8, coord_y+7}, {coord_x+7, coord_y+10},
		{coord_x+4, coord_y+10}, {coord_x+1, coord_y+7},
		{coord_x+1, coord_y+5}, {coord_x+3, coord_y+3}, 
		{coord_x+7, coord_y+1}
	};

	GR_POINT bishop2[] = {
		{coord_x+10, coord_y+1}, {coord_x+11, coord_y+4},
		{coord_x+11, coord_y+7}, {coord_x+10, coord_y+10},
		{coord_x+7, coord_y+10}, {coord_x+4, coord_y+7},
		{coord_x+4, coord_y+5}, {coord_x+6, coord_y+3}, 
		{coord_x+10, coord_y+1}
	};

	GR_POINT bishop3[] = {
		{coord_x+4, coord_y+10}, {coord_x+9, coord_y+10},
		{coord_x+9, coord_y+12}, {coord_x+4, coord_y+12},
		{coord_x+4, coord_y+10}
	};

	if(color == 'w')
	{
		GrSetGCForeground(tuxchess_gc,WHITE);
		GrFillPoly(tuxchess_wid,tuxchess_gc,9,bishop2);
		GrFillPoly(tuxchess_wid,tuxchess_gc,5,bishop3);
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrPoly(tuxchess_wid,tuxchess_gc,9,bishop2);
		GrPoly(tuxchess_wid,tuxchess_gc,5,bishop3);
		GrSetGCForeground(tuxchess_gc,WHITE);
		GrFillPoly(tuxchess_wid,tuxchess_gc,9,bishop1);
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrPoly(tuxchess_wid,tuxchess_gc,9,bishop1);
	}
	else 
	{
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrFillPoly(tuxchess_wid,tuxchess_gc,9,bishop2);
		GrFillPoly(tuxchess_wid,tuxchess_gc,5,bishop3);
		GrSetGCForeground(tuxchess_gc,LTGRAY);
		GrPoly(tuxchess_wid,tuxchess_gc,9,bishop2);
		GrPoly(tuxchess_wid,tuxchess_gc,5,bishop3);
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrFillPoly(tuxchess_wid,tuxchess_gc,9,bishop1);
		GrSetGCForeground(tuxchess_gc,LTGRAY);
		GrPoly(tuxchess_wid,tuxchess_gc,9,bishop1);
	}
}

/* Draw the pawn */
void draw_pawn(int coord_x, int coord_y, char color)
{
	if(color == 'w')
	{
		GrSetGCForeground(tuxchess_gc,WHITE);
		GrFillEllipse(tuxchess_wid,tuxchess_gc,coord_x+6,coord_y+6,2,4);
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrEllipse(tuxchess_wid,tuxchess_gc,coord_x+6,coord_y+6,2,4);
		GrSetGCForeground(tuxchess_gc,WHITE);
		GrFillRect(tuxchess_wid,tuxchess_gc,coord_x+3,coord_y+9,7,3);
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrRect(tuxchess_wid,tuxchess_gc,coord_x+3,coord_y+9,7,3);
	}
	else 
	{
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrFillEllipse(tuxchess_wid,tuxchess_gc,coord_x+6,coord_y+6,2,4);
		GrSetGCForeground(tuxchess_gc,LTGRAY);
		GrEllipse(tuxchess_wid,tuxchess_gc,coord_x+6,coord_y+6,2,4);
		GrSetGCForeground(tuxchess_gc,BLACK);
		GrFillRect(tuxchess_wid,tuxchess_gc,coord_x+3,coord_y+9,7,3);
		GrSetGCForeground(tuxchess_gc,LTGRAY);
		GrRect(tuxchess_wid,tuxchess_gc,coord_x+3,coord_y+9,7,3);
	}
}


/* Banner drawing */
static void tuxchess_do_draw()
{
	if (!is_mini) {
		pz_draw_header("Tuxchess");
		ttk_draw_window (historic_wid);
	} else {
		ttk_draw_window (message_wid);
	}
}


/* Event handler in Podzilla compiling style */
static int tuxchess_handle_event(GR_EVENT *event)
{
	int ret = 0;
	switch (event->type)
	{
	case GR_EVENT_TYPE_KEY_DOWN:
		switch (event->keystroke.ch)
		{
		case'm':
			do_menu();
			ret |= KEY_CLICK;
			break;

		case'w':
			if (end == 0) {
				do_rev();
				ret |= KEY_CLICK;
			}
			break;

		case'f':
			if (end == 0) {
				do_fwd();
				ret |= KEY_CLICK;
			}
			break;

		case'\r':
			if (end == 0) {
				do_action();
			}
			else if (end == 1) {
				draw_end(end_type);
			}
			else {
				new_game();
			}
			ret |= KEY_CLICK;
			break;

		case'l':
			if (end == 0) {
				do_left();
				ret |= KEY_CLICK;
			}
			break;

		case'r':
			if (end == 0) {
				do_right();
				ret |= KEY_CLICK;
			}
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

	return ret;
}

/* End of the game */
void draw_end(char col)
{
	int offset, off_x, off_y;

	if (end == 0) {
		end = 1;
		end_type = col;
	}
	else {
		end = 2;
		if (is_mini) {
			offset = 0;
			off_x = 11;
			off_y = 9;
		}
		else {
			offset = HEADER_TOPLINE+1;
			off_x = 0;
			off_y = 0;
		}

		end_wid = pz_new_window (0, offset, screen_info.cols,
			screen_info.rows - offset,
			tuxchess_do_draw, tuxchess_handle_event);
		GrSelectEvents(end_wid, GR_EVENT_MASK_KEY_DOWN |
				GR_EVENT_MASK_KEY_UP);

		GrMapWindow(end_wid);

		/* Put the foreground and background in good shapes */
		GrSetGCForeground(tuxchess_gc, BLACK);
		GrSetGCBackground(tuxchess_gc, WHITE);

		/* Clear the window */
		GrClearWindow(end_wid, GR_FALSE);

		if (col=='b') {
			GrText(end_wid, tuxchess_gc, 57-off_x,40-off_y, "You Lost", -1, GR_TFASCII);
		}
		else if (col=='w') {
			GrText(end_wid, tuxchess_gc, 54-off_x,40-off_y, "Well Done", -1, GR_TFASCII);
		}
		else if (col=='d') {
			GrText(end_wid, tuxchess_gc, 67-off_x,40-off_y, "Draw", -1, GR_TFASCII);
		}

		GrText(end_wid, tuxchess_gc, 52-off_x,65-off_y, 
			"Menu : Quit", -1, GR_TFASCII);
		GrText(end_wid, tuxchess_gc, 33-off_x,80-off_y, 
			"Action : New Game", -1, GR_TFASCII);
	}
}


/* ***********************************************************/
void open_tuxchess_window (void)
{
	tuxchess_gc = pz_get_gc(1);	/* Get the graphics context */

	is_mini = (screen_info.cols == 138);

	if (is_mini) {
		/* Open the window for the board: */
		tuxchess_wid = pz_new_window(0, 2,
			104,
			screen_info.rows,
			tuxchess_do_draw,
			tuxchess_handle_event);

		/* Open the window for the message on the left : */
		message_wid = pz_new_window(104, 0,
			screen_info.cols,
			screen_info.rows,
			tuxchess_do_draw,
			tuxchess_handle_event); 

		GrSelectEvents(tuxchess_wid, GR_EVENT_MASK_KEY_DOWN |
				GR_EVENT_MASK_KEY_UP);
		GrSelectEvents(message_wid, GR_EVENT_MASK_KEY_DOWN |
				GR_EVENT_MASK_KEY_UP);
	} 
	else {
		/* Open the window for the board: */
		tuxchess_wid = pz_new_window(0, HEADER_TOPLINE + 1,
			104,
			screen_info.rows - HEADER_TOPLINE - 1,
			tuxchess_do_draw,
			tuxchess_handle_event);

		/* Open the window for the historic : */
		historic_wid = pz_new_window(104, HEADER_TOPLINE + 1,
			screen_info.cols,
			screen_info.rows - HEADER_TOPLINE - 1,
			tuxchess_do_draw,
			tuxchess_handle_event); 

		GrSelectEvents(tuxchess_wid, GR_EVENT_MASK_KEY_DOWN |
				GR_EVENT_MASK_KEY_UP);
		GrSelectEvents(historic_wid, GR_EVENT_MASK_KEY_DOWN |
				GR_EVENT_MASK_KEY_UP);
	}

	/* Display the windows : */
	if (is_mini) {
		GrMapWindow(message_wid);
		draw_message("A1-","Play");
	}
	else {
		GrMapWindow(historic_wid);
	}
	GrMapWindow(tuxchess_wid);

	tuxchess_do_draw();

	/* Clear the window */
	GrClearWindow(tuxchess_wid, GR_FALSE);

	gen_moves();
	max_time = 100000;//1 << 25;
	max_depth = 1;
	end = 0;

	print_board();
	if (!is_mini) {
		draw_historic();
	}

	/* make sure the right window has focus so we get input events */
	// GrSetFocus(tuxchess_wid);
}

void new_tuxchess_window(void)
{
	init();
	init_historic();
	open_tuxchess_window();
}

/* ***********************************************************/
/* Should load last game board */
void last_tuxchess_window(void)
{
	int i;
	FILE *readfile;
	readint=0;

	// Load last board
	printf("Load last board \n");
	pz_draw_header("Saving board...");
	if ((readfile = fopen(LASTGAME, "r"))) {
		for(i = 0; i < 64; i++) {
			fread(&readint,sizeof(readint),1,readfile);
			piece[i] = readint;
			piece_avt[i] = 9;
		}

		for(i = 0; i < 64; i++) {
			fread(&readint,sizeof(readint),1,readfile);
			color[i] = readint;
			color_avt[i] = 9;
		}

		fread(&historic_line1, sizeof(historic_line1), 1, readfile);
		fread(&historic_line2, sizeof(historic_line2), 1, readfile);
		fread(&historic_line3, sizeof(historic_line3), 1, readfile);
		fread(&historic_line4, sizeof(historic_line4), 1, readfile);
		fread(&historic_line5, sizeof(historic_line5), 1, readfile);
		fread(&historic_line6, sizeof(historic_line6), 1, readfile);

		fclose(readfile);
	}
	else {
		printf("Cant read %s\n", LASTGAME);
		init();
	}

	open_tuxchess_window();
}

/* ***********************************************************/
/* move_str returns a string with move m in coordinate notation */
char *move_str(move_bytes m)
{
	static char str[8];
	char c;

	if (m.bits & 32) {
		switch (m.promote) {
		case KNIGHT:
			c = 'n';
			break;
		case BISHOP:
			c = 'b';
			break;
		case ROOK:
			c = 'r';
			break;
		default:
			c = 'q';
			break;
		}
		sprintf(str, "%c%d-%c%d%c",
			COL(m.from) + 'A',
			8 - ROW(m.from),
			COL(m.to) + 'A',
			8 - ROW(m.to),
			c);
	}
	else {
		sprintf(str, "%c%d-%c%d",
			COL(m.from) + 'A',
			8 - ROW(m.from),
			COL(m.to) + 'A',
			8 - ROW(m.to));
	}

	return str;
}

/*************************************************************/
void print_board()
{
	int row,column,i,x,y,x_curs,y_curs,x_curs_old,y_curs_old;
	static int color_type=1; //gray color

	GR_GC_ID tuxchess_gc;

	tuxchess_gc = GrNewGC();	/* Get the graphics context */

	if (end == 1) {
		reorg_historic(3);
		end = 0;
	}

	x_curs = xcoord-65;
	y_curs = ycoord-49;
	x_curs_old = oldx_curs-65;
	y_curs_old = oldy_curs-49;

	/* Draw the pieces */
	for (row = 0; row < 8; row++) {
		for (column = 0; column < 8; column++) {
			i = keyrows[row].columns[column].scancode;
			color_type=!color_type;
			if (((piece_avt[i] != piece[i])
				|| (color_avt[i] != color[i]))
				|| (x_curs_old==column && y_curs_old==7-row)
				|| (x_curs==column && y_curs==7-row)) {
				if(color_type==1) {
					GrSetGCForeground(tuxchess_gc,GRAY);
				}
				else {
					GrSetGCForeground(tuxchess_gc,WHITE);
				}

				GrFillRect(tuxchess_wid,tuxchess_gc,0+column*13,2+row*13,13,13);
				x = keyrows[row].columns[column].xoffset/3.75;
				y = 2+keyrows[row].yoffset/3.75;
				switch(color[i]) {
				case LIGHT:
					switch(piece_char[piece[i]]) {
					case 'P': 
						draw_pawn(x,y,'w');
						break;
					case 'N': 
						draw_knight(x,y,'w');
						break;
					case 'B': 
						draw_bishop(x,y,'w');
						break;
					case 'R': 
						draw_rook(x,y,'w');
						break;
					case 'Q': 
						draw_queen(x,y,'w');
						break;
					case 'K':
						draw_king(x,y,'w');
						break;
					}
					break;

				case DARK:
					switch(piece_char[piece[i]]) {
					case 'P':
						draw_pawn(x,y,'b');
						break;
					case 'N': 
						draw_knight(x,y,'b');
						break;
					case 'B': 
						draw_bishop(x,y,'b');
						break;
					case 'R': 
						draw_rook(x,y,'b');
						break;
					case 'Q': 
						draw_queen(x,y,'b');
						break;
					case 'K':
						draw_king(x,y,'b');
						break;
					}
					break;
				} /* switch */
			}
			piece_avt[i] = piece[i];
			color_avt[i] = color[i];

			if (x_curs==column && y_curs==7-row) {
				draw_cursor(xcoord, ycoord);
			}
		} /* for col */
		color_type=!color_type;
	} /* for row */

	print_result();
}

/* ***********************************************************/
/* print_result() checks to see if the game is over */
void print_result()
{
	int i;

	/* is there a legal move? */
	for (i = 0; i < first_move[1]; ++i) {
		if (makemove(gen_dat[i].m.b)) {
			takeback();
			break;
		}
	}

	if (i == first_move[1]) {
		if (in_check(side)) {
			if (side == LIGHT) {
				printf("Black mates");
				if (is_mini) {
					draw_message("Black","Mates");
				}
				else {
					pz_draw_header("Black mates");
				}
				draw_end('b');
			}
			else {
				printf("White mates");
				if (is_mini) {
					draw_message("White","Mates");
				}
				else {
					pz_draw_header("White mates");
				}
				draw_end('w');
			}
		}
		else {
			printf("Stalemate");
			if (is_mini) {
				draw_message("Stale","Mate");
			}
			else {
				pz_draw_header("Stalemate");
			}
			draw_end('d');
		}
	}
/*
	else if (reps() == 3)
	{
		printf("Draw by repetition");
		if (is_mini == 0)
			pz_draw_header("Draw by repetition");
		draw_end('d');
	}
*/
	else if (fifty >= 100) {
		printf("Draw by fifty move rule");
		if (is_mini == 0) {
			pz_draw_header("Draw : fifty moves");
		}
		draw_end('d');
	}
}

