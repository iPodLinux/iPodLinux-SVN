/*
 * BlueCube - just another tetris clone 
 * Copyright (C) 2003  Sebastian Falbesoner
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

/* File: box.h === Interface for the "BlueCube" ;) */

#ifndef __BOX_H__
#define __BOX_H__

/* Brick structure */
typedef struct
{
	int style; /* Brickstyle (color) */
} CBrick;

/* Cluster (falling piece) structure */
typedef struct
{
	int x, y;       /* Position */
	int pieceValue; /* Cluster value */
	int turnValue;  /* Rotation value */
	int data[CLUSTER_X][CLUSTER_Y]; /* Data */
	unsigned dropCount;
} CCluster;

/* Drawing attributes for the box */
typedef struct
{
	int box_x;          /* X-Position */ 
	int box_y;          /* Y-Position */
	int box_l;          /* Space between the bricks */
	int brick_width;    /* Brick's width */
	int brick_height;   /* Brick's height */

	int box_width;      /* Box' width */
	int box_height;     /* Box' height */
} CBoxDraw;

void InitBox(void);                         /* Clear box */
void DrawBox(void);                         /* Draw box */

void NewCluster(void);                      /* Create new cluster */
void DrawCluster(int);                      /* Draw cluster */
void DrawNextPiece(int posX, int posY);     /* Draw next piece ;) */

int  MoveCluster(int bDown);                /* Move cluster down */
void MoveClusterLeft(void);                 /* Move cluster left */
void MoveClusterRight(void);                /* Move cluster right */
void TurnClusterRight(void);                /* Rotate cluster */

void BoxDrawInit(void);

/* Make the stuff available in every file including this header */
extern CCluster cluster;
extern CBoxDraw boxdraw;

#endif
