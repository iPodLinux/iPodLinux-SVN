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

#define STYLE_R(style) StyleColors[style][0]
#define STYLE_G(style) StyleColors[style][1]
#define STYLE_B(style) StyleColors[style][2]

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
void SetBrick(int x, int y, int style);     /* Activate a brick */
int  IsBrickSet(int x, int y);              /* Check if a brick is set */

void NewCluster(void);                      /* Create new cluster */
void SetClusterPiece(void);                 /* Set piece for cluster */
void DrawCluster(void);                     /* Draw cluster */
void DrawNextPiece(int posX, int posY);     /* Draw next piece ;) */
void PutCluster(int x, int y);              /* Put cluster into the box */
int  ClusterCollisionTest(int x, int y);    /* Does the cluster collide somewhere? */
int  MoveCluster(int bDown);                /* Move cluster down */
void MoveClusterLeft(void);                 /* Move cluster left */
void MoveClusterRight(void);                /* Move cluster right */
void TurnClusterRight(void);                /* Rotate cluster */

int  FullLine(int y);                       /* Is a certain line full? */
int  CheckFullLine(void);                   /* Remove full lines */

void BrickExplosion(int x, int y, int energy, int density);
void BoxDrawInit(void);
void BoxDrawUpdate(void);
void BoxDrawMove(void);

/* Make the stuff available in every file including this header */
extern CCluster cluster;
extern CBoxDraw boxdraw;

#endif
