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

/* File: box.c === This is the most important file,
                   it includes all the magic behind the "BlueCube" ;) */

#include <stdlib.h>

#include "global.h"
#include "grafix.h"
#include "sound.h"
#include "particle.h"
#include "pieces.h"
#include "box.h"

extern int lines, score;
extern int level;
extern int nextPiece;
const int intervals[11] = {27, 25, 23, 21, 19, 17, 15, 13, 11, 9, 7};

CBoxDraw boxdraw;
CCluster cluster;
CBrick box[BOX_BRICKS_X][BOX_BRICKS_Y];

/* Function prototypes from other source files */
void StartGameOverAnimation();

/*=========================================================================
// Name: InitBox()
// Desc: Clears the box (must be called when starting a new game)
//=======================================================================*/
void InitBox(void)
{
	int x, y;

	for (y=0; y<BOX_BRICKS_Y; y++)
		for (x=0; x<BOX_BRICKS_X; x++)
			SetBrick(x,y,0);
}

/*=========================================================================
// Name: DrawBox()
// Desc: Draws the box onto the screen
//=======================================================================*/
void DrawBox()
{
	int x, y;

	for (y=0; y<BOX_BRICKS_Y; y++)
		for (x=0; x<BOX_BRICKS_X; x++)
		{
			if (box[x][y].style)
			{
				PutRect(
					boxdraw.box_x + x*(boxdraw.brick_width + boxdraw.box_l), 
					boxdraw.box_y + y*(boxdraw.brick_height+ boxdraw.box_l),
					boxdraw.brick_width, boxdraw.brick_height, 
					STYLE_R(box[x][y].style-1),
					STYLE_G(box[x][y].style-1),
					STYLE_B(box[x][y].style-1));
				
				PutRect(
					2+boxdraw.box_x+x*(boxdraw.brick_width + boxdraw.box_l),
					2+boxdraw.box_y+y*(boxdraw.brick_height+ boxdraw.box_l),
					boxdraw.brick_width-4, boxdraw.brick_height-4,0,0,0);
			}
		}
}


/*=========================================================================
// Name: SetBrick()
// Desc: Changes a brick's style
//=======================================================================*/
void SetBrick(int x, int y, int style)
{
	box[x][y].style = style;
}

/*=========================================================================
// Name: IsBrickSet()
// Desc: Checks if a brick is set
//=======================================================================*/
int IsBrickSet(int x, int y)
{
	if (box[x][y].style)
		return 1;
	else
		return 0;
}

/*=========================================================================
// Name: NewCluster()
// Desc: Sets the y position to 0 (top) and the x position to the center
//=======================================================================*/
void NewCluster()
{
	cluster.x = 3;
	cluster.y = 0;
	cluster.turnValue = 0;
	cluster.pieceValue = nextPiece;
	SetClusterPiece();
	cluster.dropCount = intervals[level];

	nextPiece = rand()%7;

	if (ClusterCollisionTest(cluster.x, cluster.y))
		StartGameOverAnimation(); /* No space left for cluster? -> GameOver! */
}


/*=========================================================================
// Name: SetClusterPiece()
// Desc: Fills the datafield of the cluster with a certain piece
//=======================================================================*/
void SetClusterPiece()
{
	int x, y;

	for (y=0; y<CLUSTER_Y; y++)
		for (x=0; x<CLUSTER_X; x++)
			cluster.data[x][y] = 
			Pieces[cluster.pieceValue][cluster.turnValue][x][y];
}

/*=========================================================================
// Name: DrawCluster()
// Desc: Draws the cluster onto the screen
//=======================================================================*/
void DrawCluster()
{
	int x, y;

	for (y=0; y<CLUSTER_Y; y++)
		for (x=0; x<CLUSTER_X; x++)
		{
			if (cluster.data[x][y])
				PutRect(
					boxdraw.box_x + (cluster.x+x)*(boxdraw.brick_width+boxdraw.box_l), 
					boxdraw.box_y + (cluster.y+y)*(boxdraw.brick_height+boxdraw.box_l),
					boxdraw.brick_width, boxdraw.brick_height, 
					STYLE_R(cluster.data[x][y]-1),
					STYLE_G(cluster.data[x][y]-1),
					STYLE_B(cluster.data[x][y]-1));
		}
}

/*=========================================================================
// Name: DrawNextPiece()
// Desc: Draws the next cluster (see whats coming next...) onto the screen
//=======================================================================*/
void DrawNextPiece(int posX, int posY)
{
	int x, y;

	for (y=0; y<CLUSTER_Y; y++)
		for (x=0; x<CLUSTER_X; x++)
			if (Pieces[nextPiece][0][x][y])
				PutRect(
					posX + x*(boxdraw.brick_width+boxdraw.box_l),
					posY + y*(boxdraw.brick_height+boxdraw.box_l),
					boxdraw.brick_width, boxdraw.brick_height,
					STYLE_R(Pieces[nextPiece][0][x][y]-1),
					STYLE_G(Pieces[nextPiece][0][x][y]-1),
					STYLE_B(Pieces[nextPiece][0][x][y]-1));
}

/*=========================================================================
// Name: PutCluster()
// Desc: Puts the cluster into the box
//=======================================================================*/
void PutCluster(int x, int y)
{
	int cX, cY;

	for (cY=0; cY<CLUSTER_Y; cY++)
		for (cX=0; cX<CLUSTER_X; cX++)
		{
			box[x+cX][y+cY].style |= cluster.data[cX][cY];
		}
}


/*=========================================================================
// Name: ClusterCollisionTest()
// Desc: Checks if cluster collides with bricks/walls
//=======================================================================*/
int ClusterCollisionTest(int x, int y)
{
	int X, Y, cX, cY;

	for (cY=0; cY<CLUSTER_Y; cY++)
		for (cX=0; cX<CLUSTER_X; cX++)
		{
			X = cX+x;
			Y = cY+y;

			if (cluster.data[cX][cY])
			{
				if ((box[X][Y].style) ||
					((X < 0) || (X >= BOX_BRICKS_X) ||
					 (Y < 0) || (Y >= BOX_BRICKS_Y)))
					 return 1;
			}
		}

	return 0;
}

/*=========================================================================
// Name: MoveCluster()
// Desc: Moves the cluster down (or even "drops" down if bDown is true)
//=======================================================================*/
int MoveCluster(int bDown)
{
	int y;

	if (bDown) /* Should be dropped down? */
	{
		for (y=cluster.y; y<BOX_BRICKS_Y; y++)
		{
			if (ClusterCollisionTest(cluster.x, y+1)) /* Search for terrain */
			{
				PutCluster(cluster.x, y);
				if (!CheckFullLine())
					score += 200;
				return 1;
			}
		}
		return 0;
	} /* Move down slowly */
	else {
		if (ClusterCollisionTest(cluster.x, cluster.y+1))
		{
			PutCluster(cluster.x, cluster.y);
			if (!CheckFullLine())
				score += 100;
			return 1;
		}

		cluster.y++; /* Let the cluster "fall" */
		cluster.dropCount = intervals[level];
		return 0;
	}
}

/*=========================================================================
// Name: MoveClusterLeft()
// Desc: Moves the cluster to the left side
//=======================================================================*/
void MoveClusterLeft()
{
	if (!ClusterCollisionTest(cluster.x-1, cluster.y))
		cluster.x--;
}

/*=========================================================================
// Name: MoveClusterRight()
// Desc: Moves the cluster to the right side
//=======================================================================*/
void MoveClusterRight()
{
	if (!ClusterCollisionTest(cluster.x+1, cluster.y))
		cluster.x++;
}

/*=========================================================================
// Name: TurnClusterRight()
// Desc: Rotates the cluster clock-wise
//=======================================================================*/
void TurnClusterRight()
{
	if (cluster.turnValue == 3)
		cluster.turnValue = 0;
	else
		cluster.turnValue++;

	SetClusterPiece();

	if (ClusterCollisionTest(cluster.x, cluster.y))
	{
		if (cluster.turnValue == 0)
			cluster.turnValue = 3;
		else
			cluster.turnValue--;
		SetClusterPiece();
	}
	else
		SetClusterPiece();

}

/*=========================================================================
// Name: FullLine()
// Desc: Checks if a certain line is full
//=======================================================================*/
int FullLine(int y)
{
	int x;

	for (x=0; x<BOX_BRICKS_X; x++) {
		if (!box[x][y].style)
			return 0; 
	}
	
	return 1;
}

/*=========================================================================
// Name: CheckFullLine()
// Desc: Remove all full lines in the box
//=======================================================================*/
int CheckFullLine()
{
	int counter = 0; /* Number of the killed lines */
	int x, y, newX, newY;

	for (y=BOX_BRICKS_Y-1; y>0; y--) /* Create particles and play sound... */
		if (FullLine(y))
		{
			for (x=0; x<BOX_BRICKS_X; x++)
				BrickExplosion(x, y, 1, 15);
			PutSound(&sndLine);
		}

	for (y=BOX_BRICKS_Y-1; y>0; y--)
		while (FullLine(y)) /* Remove lines */
		{
			for (newY=y; newY>0; newY--)
				for (newX=0; newX<BOX_BRICKS_X; newX++)
				{
					box[newX][newY].style = box[newX][newY-1].style;
				}
			counter++;
		}

	if (counter) /* Were some lines killed? */
	{
		lines += counter; /* Increase lines counter */
		switch (counter)  /* Increase score */
		{
		case 1: score += 1000;  break;
		case 2: score += 2500;  break;
		case 3: score += 5000;  break;
		case 4: score += 10000; break;
		}

		return 1;
	}

	return 0;
}

/*=========================================================================
// Name: BrickExplosion()
// Desc: Creates an explosion for a certain brick (particle system)
//=======================================================================*/
void BrickExplosion(int x, int y, int energy, int density)
{
	CreateParticleExplosion(
		boxdraw.box_x + x * (boxdraw.brick_width  + boxdraw.box_l) + boxdraw.brick_width/2,
		boxdraw.box_y + y * (boxdraw.brick_height + boxdraw.box_l) + boxdraw.brick_height/2,
		STYLE_R(box[x][y].style-1),
		STYLE_G(box[x][y].style-1),
		STYLE_B(box[x][y].style-1), energy, density);
}

/*=========================================================================
// Name: BoxDrawInit()
// Desc: Initializes the boxdraw structure
//=======================================================================*/
void BoxDrawInit()
{
	boxdraw.box_l = 2;
	boxdraw.brick_width  = 20;
	boxdraw.brick_height = 20;
	BoxDrawUpdate();
	boxdraw.box_x = (SCREEN_X/2 - boxdraw.box_width/2);
	boxdraw.box_y = (SCREEN_Y/2 - boxdraw.box_height/2);
}

/*=========================================================================
// Name: BoxDrawUpdate()
// Desc: Updates the boxdraw structure
//=======================================================================*/
void BoxDrawUpdate()
{
	boxdraw.box_width  = (BOX_BRICKS_X*boxdraw.brick_width +
						 (BOX_BRICKS_X - 1)*boxdraw.box_l);
	boxdraw.box_height = (BOX_BRICKS_Y*boxdraw.brick_height +
						 (BOX_BRICKS_Y - 1)*boxdraw.box_l);
}

/*=========================================================================
// Name: BoxDrawMove()
// Desc: Moves the box (needed for crazy mode)
//=======================================================================*/
void BoxDrawMove()
{
	static int moveX=3;
	static int bSpace=1;
	static int frameTimer = 0;

	frameTimer++;
	
	if (frameTimer == 1)
	{
		/*
		if (bGrow)
		{
			boxdraw.brick_width++;
			boxdraw.brick_height++;
			BoxDrawUpdate();
			if (boxdraw.brick_width >= 25)
				bGrow = false;
		}
		else
		{
			boxdraw.brick_width--;
			boxdraw.brick_height--;
			BoxDrawUpdate();
			if (boxdraw.brick_width <= 5)
				bGrow = true;
		}
		*/

		if (bSpace)
		{
			boxdraw.box_l++;
			BoxDrawUpdate();
			if (boxdraw.box_l >= 7)
				bSpace = 0;
		}
		else
		{
			boxdraw.box_l--;
			BoxDrawUpdate();
			if (boxdraw.box_l <= 2)
				bSpace = 1;
		}
		
		frameTimer = 0;
	}

	boxdraw.box_x += moveX;	
	/* boxdraw.box_x = (SCREEN_X/2 - boxdraw.box_width/2); */
	boxdraw.box_y = (SCREEN_Y/2 - boxdraw.box_height/2);
	
	if ((boxdraw.box_x + boxdraw.box_width-1) >= SCREEN_X)
		moveX = -3;
	if (boxdraw.box_x < 0)
		moveX = 3;
}
