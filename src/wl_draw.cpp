// WL_DRAW.C

#include "wl_def.h"
#include "id_sd.h"
#include "id_in.h"
#include "id_vl.h"
#include "id_vh.h"
#include "id_us.h"
#include "textures/textures.h"
#include "c_cvars.h"
#include "r_sprites.h"
#include "r_data/colormaps.h"

#include "wl_cloudsky.h"
#include "wl_atmos.h"
#include "wl_shade.h"
#include "actor.h"
#include "id_ca.h"
#include "gamemap.h"
#include "lumpremap.h"
#include "wl_agent.h"
#include "wl_draw.h"
#include "wl_game.h"
#include "wl_play.h"
#include "a_inventory.h"

/*
=============================================================================

							LOCAL CONSTANTS

=============================================================================
*/

#define MINDIST         (0x4000l)

#define mapheight (map->GetHeader().height)
#define mapwidth (map->GetHeader().width)
#define maparea (mapheight*mapwidth)

/*
=============================================================================

							GLOBAL VARIABLES

=============================================================================
*/

void DrawFloorAndCeiling(byte *vbuf, unsigned vbufPitch, int min_wallheight);

const RatioInformation AspectCorrection[] =
{
	/* UNC */	{960,	600,	0x10000,	0,				48,			false},
	/* 16:9 */	{1280,	450,	0x15555,	0,				48*3/4,		true},
	/* 16:10 */	{1152,	500,	0x13333,	0,				48*5/6,		true},
	/* 17:10 */ {1224,	471,	0x14666,	0,				48*40/51,	true},
	/* 4:3 */	{960,	600,	0x10000,	0,				48,			false},
	/* 5:4 */	{960,	640,	0x10000,(fixed)6.5*FRACUNIT,	48*15/16,	false}
};

/*static*/ byte *vbuf = NULL;
unsigned vbufPitch = 0;

int32_t	lasttimecount;
int32_t	frameon;
bool	fpscounter;

int fps_frames=0, fps_time=0, fps=0;

int *wallheight;
int min_wallheight;

//
// math tables
//
short *pixelangle;
fixed finetangent[FINEANGLES/2 + ANG180];
fixed finesine[FINEANGLES+FINEANGLES/4];
fixed *finecosine = finesine+ANG90;

//
// refresh variables
//
fixed   viewx,viewy;                    // the focal point
angle_t viewangle;
fixed   viewsin,viewcos;
int viewshift = 0;
fixed viewz = 32;

fixed gLevelVisibility = VISIBILITY_DEFAULT;
int gLevelLight = LIGHTLEVEL_DEFAULT;

void    TransformActor (AActor *ob);
void    BuildTables (void);
void    ClearScreen (void);
int     CalcRotate (AActor *ob);
void    DrawScaleds (void);
void    CalcTics (void);
void    ThreeDRefresh (void);



//
// wall optimization variables
//
int     lastside;               // true for vertical
int32_t    lastintercept;
MapSpot lasttilehit;
int     lasttexture;

//
// ray tracing variables
//
short    focaltx,focalty,viewtx,viewty;
longword xpartialup,xpartialdown,ypartialup,ypartialdown;

short   midangle;
short   angle;

MapTile::Side hitdir;
MapSpot tilehit;
int     pixx;

short   xtile,ytile;
short   xtilestep,ytilestep;
int32_t    xintercept,yintercept;
word    xstep,ystep;
int     texdelta;
int		texheight;

#define TEXTUREBASE 0x4000000
fixed	texxscale = FRACUNIT;
fixed	texyscale = FRACUNIT;


/*
============================================================================

						3 - D  DEFINITIONS

============================================================================
*/

/*
========================
=
= TransformActor
=
= Takes paramaters:
=   gx,gy               : globalx/globaly of point
=
= globals:
=   viewx,viewy         : point of view
=   viewcos,viewsin     : sin/cos of viewangle
=   scale               : conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
========================
*/


//
// transform actor
//
void TransformActor (AActor *ob)
{
	fixed gx,gy,gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
	gx = ob->x-viewx;
	gy = ob->y-viewy;

//
// calculate newx
//
	gxt = FixedMul(gx,viewcos);
	gyt = FixedMul(gy,viewsin);
	// Wolf4SDL used 0x2000 for statics and 0x4000 for moving actors, but since
	// we no longer tell the difference, use the smaller fudging value since
	// the larger one will just look ugly in general.
	nx = gxt-gyt-0x2000;

//
// calculate newy
//
	gxt = FixedMul(gx,viewsin);
	gyt = FixedMul(gy,viewcos);
	ny = gyt+gxt;

//
// calculate perspective ratio
//
	ob->transx = nx;
	ob->transy = ny;

	if (nx<MINDIST)                 // too close, don't overflow the divide
	{
		ob->viewheight = 0;
		return;
	}

	ob->viewx = (word)(centerx + ny*scale/nx);

//
// calculate height (heightnumerator/(nx>>8))
//
	ob->viewheight = (word)(heightnumerator/(nx>>8));
}

//==========================================================================

/*
====================
=
= CalcHeight
=
= Calculates the height of xintercept,yintercept from viewx,viewy
=
====================
*/

int CalcHeight()
{
	fixed z = FixedMul(xintercept - viewx, viewcos)
		- FixedMul(yintercept - viewy, viewsin);
	if(z < MINDIST) z = MINDIST;
	int height = heightnumerator / (z >> 8);
	if(height < min_wallheight) min_wallheight = height;
	return height;
}

//==========================================================================

/*
===================
=
= ScalePost
=
===================
*/

const byte *postsource;
int postx;

void ScalePost()
{
	if(postsource == NULL)
		return;

	int ywcount, yoffs, yw, yd, yendoffs;
	byte col;

	const int shade = LIGHT2SHADE(gLevelLight);
	const int tz = FixedMul(r_depthvisibility<<8, wallheight[postx]);
	BYTE *curshades = &NormalLight.Maps[GETPALOOKUP(MAX(tz, MINZ), shade)<<8];

	ywcount = yd = (wallheight[postx] >> 3);
	if(yd <= 0)
		yd = 100;

	// Calculate starting and ending offsets
	{
		// ywcount can be large enough to cause an overflow if we don't reduce
		// fixed point precision here
		const int topoffset = ywcount*(viewz>>8)/(32<<(FRACBITS-8));
		const int botoffset = ywcount*((viewz - (64<<FRACBITS))>>8)/(32<<(FRACBITS-8));

		yoffs = (viewheight / 2 - topoffset - viewshift) * vbufPitch;
		if(yoffs < 0) yoffs = 0;
		yoffs += postx;

		yendoffs = viewheight / 2 - botoffset - 1 - viewshift;
		yw=texyscale-1;
	}

	while(yendoffs >= viewheight)
	{
		ywcount -= texyscale/2;
		while(ywcount <= 0)
		{
			ywcount += yd;
			yw--;
		}
		yendoffs--;
	}
	if(yw < 0)
		return;

	col = curshades[postsource[yw]];
	yendoffs = yendoffs * vbufPitch + postx;
	while(yoffs <= yendoffs)
	{
		vbuf[yendoffs] = col;
		ywcount -= texyscale/2;
		if(ywcount <= 0)
		{
			do
			{
				ywcount += yd;
				yw--;
			}
			while(ywcount <= 0);
			if(yw < 0) break;
			col = curshades[postsource[yw]];
		}
		yendoffs -= vbufPitch;
	}
}

void GlobalScalePost(byte *vidbuf, unsigned pitch)
{
	vbuf = vidbuf;
	vbufPitch = pitch;
	ScalePost();
}

static void DetermineHitDir(bool vertical)
{
	if(vertical)
	{
		if(xtilestep==-1 && (xintercept>>16)<=xtile)
			hitdir = MapTile::East;
		else
			hitdir = MapTile::West;
	}
	else
	{
		if(ytilestep==-1 && (yintercept>>16)<=ytile)
			hitdir = MapTile::South;
		else
			hitdir = MapTile::North;
	}
}

/*
====================
=
= HitVertWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

void HitVertWall (void)
{
	if(!tilehit)
		return;

	int texture;

	DetermineHitDir(true);

	texture = (yintercept+texdelta-tilehit->slideAmount[hitdir])&(FRACUNIT-1);
	if (xtilestep == -1 && !tilehit->tile->offsetVertical)
	{
		texture = (FRACUNIT - texture - texxscale)&(FRACUNIT-1);
		xintercept += TILEGLOBAL;
	}

	if(lastside==1 && lastintercept==xtile && lasttilehit==tilehit && !(lasttilehit->tile->offsetVertical))
	{
		texture -= texture%texxscale;

		if((pixx&3) && texture == lasttexture)
		{
			ScalePost();
			postx = pixx;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		ScalePost();
		wallheight[pixx] = CalcHeight();
		if(postsource)
			postsource+=(texture-lasttexture)*texheight/texxscale;
		postx=pixx;
		lasttexture=texture;
		return;
	}

	if(lastside!=-1) ScalePost();

	lastside=1;
	lastintercept=xtile;
	lasttilehit=tilehit;
	wallheight[pixx] = CalcHeight();
	postx = pixx;
	FTexture *source = NULL;

	MapSpot adj = tilehit->GetAdjacent(hitdir);
	if (adj && adj->tile && adj->tile->offsetHorizontal && !adj->tile->offsetVertical) // check for adjacent doors
		source = TexMan(adj->texture[hitdir]);
	else
		source = TexMan(tilehit->texture[hitdir]);

	if(source)
	{
		texheight = source->GetHeight();
		texxscale = TEXTUREBASE/source->xScale;
		texyscale = (64*source->yScale)>>FRACBITS;
		texture -= texture%texxscale;

		postsource = source->GetColumn(texture/texxscale, NULL);
	}
	else
		postsource = NULL;

	lasttexture=texture;
}


/*
====================
=
= HitHorizWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

void HitHorizWall (void)
{
	if(!tilehit)
		return;

	int texture;

	DetermineHitDir(false);

	texture = (xintercept+texdelta-tilehit->slideAmount[hitdir])&(FRACUNIT-1);
	if(!tilehit->tile->offsetHorizontal)
	{
		if (ytilestep == -1)
			yintercept += TILEGLOBAL;
		else
			texture = (FRACUNIT - texture - texxscale)&(FRACUNIT-1);
	}

	if(lastside==0 && lastintercept==ytile && lasttilehit==tilehit && !(lasttilehit->tile->offsetHorizontal))
	{
		texture -= texture%texxscale;

		if((pixx&3) && texture == lasttexture)
		{
			ScalePost();
			postx=pixx;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		ScalePost();
		wallheight[pixx] = CalcHeight();
		if(postsource)
			postsource+=(texture-lasttexture)*texheight/texxscale;
		postx=pixx;
		lasttexture=texture;
		return;
	}

	if(lastside!=-1) ScalePost();

	lastside=0;
	lastintercept=ytile;
	lasttilehit=tilehit;
	wallheight[pixx] = CalcHeight();
	postx = pixx;
	FTexture *source = NULL;

	MapSpot adj = tilehit->GetAdjacent(hitdir);
	if (adj && adj->tile && adj->tile->offsetVertical && !adj->tile->offsetHorizontal) // check for adjacent doors
		source = TexMan(adj->texture[hitdir]);
	else
		source = TexMan(tilehit->texture[hitdir]);

	if(source)
	{
		texheight = source->GetHeight();
		texxscale = TEXTUREBASE/source->xScale;
		texyscale = (64*source->yScale)>>FRACBITS;
		texture -= texture%texxscale;

		postsource = source->GetColumn(texture/texxscale, NULL);
	}
	else
		postsource = NULL;

	lasttexture=texture;
}

//==========================================================================

#define HitHorizBorder HitHorizWall
#define HitVertBorder HitVertWall

//==========================================================================

/*
=====================
=
= CalcRotate
=
=====================
*/

int CalcRotate (AActor *ob)
{
	angle_t angle, viewangle;

	// this isn't exactly correct, as it should vary by a trig value,
	// but it is close enough with only eight rotations

	viewangle = players[0].camera->angle + (centerx - ob->viewx)/8;

	angle = viewangle - ob->angle;

	angle+=ANGLE_45/2;

	return angle/ANGLE_45;
}

/*
=====================
=
= DrawScaleds
=
= Draws all objects that are visable
=
=====================
*/

#define MAXVISABLE 250

typedef struct
{
	AActor *actor;
	short viewheight;
	//short      viewx,
	//		viewheight,
	//		shapenum;
	//short      flags;          // this must be changed to uint32_t, when you
							// you need more than 16-flags for drawing
#ifdef USE_DIR3DSPR
	statobj_t *transsprite;
#endif
} visobj_t;

visobj_t vislist[MAXVISABLE];
visobj_t *visptr,*visstep,*farthest;

void DrawScaleds (void)
{
	int      i,least,numvisable,height;

	visptr = &vislist[0];

//
// place active objects
//
	for(AActor::Iterator *iter = AActor::GetIterator();iter;iter = iter->Next())
	{
		AActor *obj = iter->Item();

		if (obj->sprite == SPR_NONE)
			continue;

		MapSpot spot = map->GetSpot(obj->tilex, obj->tiley, 0);
		MapSpot spots[8];
		spots[0] = spot->GetAdjacent(MapTile::East);
		spots[1] = spots[0] ? spots[0]->GetAdjacent(MapTile::North) : NULL;
		spots[2] = spot->GetAdjacent(MapTile::North);
		spots[3] = spots[2] ? spots[2]->GetAdjacent(MapTile::West) : NULL;
		spots[4] = spot->GetAdjacent(MapTile::West);
		spots[5] = spots[4] ? spots[4]->GetAdjacent(MapTile::South) : NULL;
		spots[6] = spot->GetAdjacent(MapTile::South);
		spots[7] = spots[6] ? spots[6]->GetAdjacent(MapTile::East) : NULL;

		//
		// could be in any of the nine surrounding tiles
		//
		if (spot->visible
			|| ( spots[0] && (spots[0]->visible && !spots[0]->tile) )
			|| ( spots[1] && (spots[1]->visible && !spots[1]->tile) )
			|| ( spots[2] && (spots[2]->visible && !spots[2]->tile) )
			|| ( spots[3] && (spots[3]->visible && !spots[3]->tile) )
			|| ( spots[4] && (spots[4]->visible && !spots[4]->tile) )
			|| ( spots[5] && (spots[5]->visible && !spots[5]->tile) )
			|| ( spots[6] && (spots[6]->visible && !spots[6]->tile) )
			|| ( spots[7] && (spots[7]->visible && !spots[7]->tile) ) )
		{
			TransformActor (obj);
			if (!obj->viewheight || (gamestate.victoryflag && obj == players[0].mo))
				continue;                                               // too close or far away

			visptr->actor = obj;
			visptr->viewheight = obj->viewheight;

			if (visptr < &vislist[MAXVISABLE-1])    // don't let it overflow
			{
#ifdef USE_DIR3DSPR
				visptr->transsprite = NULL;
#endif
				visptr++;
			}
			obj->flags |= FL_VISABLE;
		}
		else
			obj->flags &= ~FL_VISABLE;
	}

//
// draw from back to front
//
	numvisable = (int) (visptr-&vislist[0]);

	if (!numvisable)
		return;                                                                 // no visable objects

	for (i = 0; i<numvisable; i++)
	{
		least = 32000;
		for (visstep=&vislist[0] ; visstep<visptr ; visstep++)
		{
			height = visstep->viewheight;
			if (height < least)
			{
				least = height;
				farthest = visstep;
			}
		}
		//
		// draw farthest
		//
#ifdef USE_DIR3DSPR
		if(farthest->transsprite)
			Scale3DShape(vbuf, vbufPitch, farthest->transsprite);
		else
#endif
			ScaleSprite(farthest->actor, farthest->actor->viewx, farthest->actor->state, farthest->viewheight);

		farthest->viewheight = 32000;
	}
}

//==========================================================================

/*
==============
=
= DrawPlayerWeapon
=
= Draw the player's hands
=
==============
*/

void DrawPlayerWeapon (void)
{
	if(!players[0].psprite.frame)
		return;

	fixed xoffset, yoffset;
	players[0].BobWeapon(&xoffset, &yoffset);

	R_DrawPlayerSprite(players[0].ReadyWeapon, players[0].psprite.frame, players[0].psprite.sx+xoffset, players[0].psprite.sy+yoffset);
}


//==========================================================================


/*
=====================
=
= CalcTics
=
=====================
*/

void CalcTics (void)
{
//
// calculate tics since last refresh for adaptive timing
//
	if (lasttimecount > (int32_t) GetTimeCount())
		lasttimecount = GetTimeCount();    // if the game was paused a LONG time

	uint32_t curtime = SDL_GetTicks();
	tics = (curtime * 7) / 100 - lasttimecount;
	if(!tics)
	{
		// wait until end of current tic
		SDL_Delay(((lasttimecount + 1) * 100) / 7 - curtime);
		tics = 1;
	}
	else if(noadaptive)
		tics = 1;

	lasttimecount += tics;

	if (tics>MAXTICS)
		tics = MAXTICS;
}


//==========================================================================

void AsmRefresh()
{
	static word xspot[2],yspot[2];
	int32_t xstep=0,ystep=0;
	longword xpartial=0,ypartial=0;
	MapSpot focalspot = map->GetSpot(focaltx, focalty, 0);
	bool playerInPushwallBackTile = focalspot->pushAmount != 0;

	for(pixx=0;pixx<viewwidth;pixx++)
	{
		short angl=midangle+pixelangle[pixx];
		if(angl<0) angl+=FINEANGLES;
		if(angl>=ANG360) angl-=FINEANGLES;
		if(angl<ANG90)
		{
			xtilestep=1;
			ytilestep=-1;
			xstep=finetangent[ANG90-1-angl];
			ystep=-finetangent[angl];
			xpartial=xpartialup;
			ypartial=ypartialdown;
		}
		else if(angl<ANG180)
		{
			xtilestep=-1;
			ytilestep=-1;
			xstep=-finetangent[angl-ANG90];
			ystep=-finetangent[ANG180-1-angl];
			xpartial=xpartialdown;
			ypartial=ypartialdown;
		}
		else if(angl<ANG270)
		{
			xtilestep=-1;
			ytilestep=1;
			xstep=-finetangent[ANG270-1-angl];
			ystep=finetangent[angl-ANG180];
			xpartial=xpartialdown;
			ypartial=ypartialup;
		}
		else if(angl<ANG360)
		{
			xtilestep=1;
			ytilestep=1;
			xstep=finetangent[angl-ANG270];
			ystep=finetangent[ANG360-1-angl];
			xpartial=xpartialup;
			ypartial=ypartialup;
		}
		yintercept=FixedMul(ystep,xpartial)+viewy;
		xtile=focaltx+xtilestep;
		xspot[0]=xtile;
		xspot[1]=yintercept>>16;
		xintercept=FixedMul(xstep,ypartial)+viewx;
		ytile=focalty+ytilestep;
		yspot[0]=xintercept>>16;
		yspot[1]=ytile;
		texdelta=0;

		// Special treatment when player is in back tile of pushwall
		if(playerInPushwallBackTile)
		{
			if(focalspot->pushReceptor)
				focalspot = focalspot->pushReceptor;

			if((focalspot->pushDirection == MapTile::East && xtilestep == 1) ||
				(focalspot->pushDirection == MapTile::West && xtilestep == -1))
			{
				int32_t yintbuf = yintercept - ytilestep*(abs(ystep * signed(64 - focalspot->pushAmount)) >> 6);
				if((yintbuf >> 16) == focalty)   // ray hits pushwall back?
				{
					if(focalspot->pushDirection == MapTile::East)
						xintercept = (focaltx << TILESHIFT) + (focalspot->pushAmount << 10);
					else
						xintercept = (focaltx << TILESHIFT) - TILEGLOBAL + ((64 - focalspot->pushAmount) << 10);
					yintercept = yintbuf;
					ytile = (short) (yintercept >> TILESHIFT);
					tilehit = focalspot;
					HitVertWall();
					continue;
				}
			}
			else if((focalspot->pushDirection == MapTile::South && ytilestep == 1) ||
				(focalspot->pushDirection == MapTile::North && ytilestep == -1))
			{
				int32_t xintbuf = xintercept - xtilestep*(abs(xstep * signed(64 - focalspot->pushAmount)) >> 6);
				if((xintbuf >> 16) == focaltx)   // ray hits pushwall back?
				{
					xintercept = xintbuf;
					if(focalspot->pushDirection == MapTile::South)
						yintercept = (focalty << TILESHIFT) + (focalspot->pushAmount << 10);
					else
						yintercept = (focalty << TILESHIFT) - TILEGLOBAL + ((64 - focalspot->pushAmount) << 10);
					xtile = (short) (xintercept >> TILESHIFT);
					tilehit = focalspot;
					HitHorizWall();
					continue;
				}
			}
		}

		do
		{
			if(ytilestep==-1 && (yintercept>>16)<=ytile) goto horizentry;
			if(ytilestep==1 && (yintercept>>16)>=ytile) goto horizentry;
vertentry:

			if((uint32_t)yintercept>mapheight*65536-1 || (word)xtile>=mapwidth)
			{
				if(xtile<0) xintercept=0, xtile=0;
				else if((unsigned)xtile>=mapwidth) xintercept=mapwidth<<TILESHIFT, xtile=mapwidth-1;
				else xtile=(short) (xintercept >> TILESHIFT);
				if(yintercept<0) yintercept=0, ytile=0;
				else if((unsigned)yintercept>=(mapheight<<TILESHIFT)) yintercept=mapheight<<TILESHIFT, ytile=mapheight-1;
				yspot[0]=0xffff;
				tilehit=0;
				HitHorizBorder();
				break;
			}
			if(xspot[0]>=mapwidth || xspot[1]>=mapheight) break;
			tilehit=map->GetSpot(xspot[0], xspot[1], 0);
			if(tilehit && tilehit->tile)
			{
				if(tilehit->tile->offsetVertical)
				{
					DetermineHitDir(true);
					int32_t yintbuf=yintercept+(ystep>>1);
					if((yintbuf>>16)!=(yintercept>>16))
						goto passvert;
					if((word)yintbuf<tilehit->slideAmount[hitdir])
						goto passvert;
					yintercept=yintbuf;
					xintercept=(xtile<<TILESHIFT)|0x8000;
					ytile = (short) (yintercept >> TILESHIFT);
					HitVertWall();
				}
				else
				{
					bool isPushwall = tilehit->pushAmount != 0 || tilehit->pushReceptor;
					if(tilehit->pushReceptor)
						tilehit = tilehit->pushReceptor;

					if(isPushwall)
					{
						if(tilehit->pushDirection==MapTile::West || tilehit->pushDirection==MapTile::East)
						{
							int32_t yintbuf;
							int pwallposnorm;
							int pwallposinv;
							if(tilehit->pushDirection==MapTile::West)
							{
								pwallposnorm = 64-tilehit->pushAmount;
								pwallposinv = tilehit->pushAmount;
							}
							else
							{
								pwallposnorm = tilehit->pushAmount;
								pwallposinv = 64-tilehit->pushAmount;
							}
							if((tilehit->pushDirection==MapTile::East && xtile==(signed)tilehit->GetX() && ((uint32_t)yintercept>>16)==tilehit->GetY())
								|| (tilehit->pushDirection==MapTile::West && !(xtile==(signed)tilehit->GetX() && ((uint32_t)yintercept>>16)==tilehit->GetY())))
							{
								yintbuf=yintercept+((ystep*pwallposnorm)>>6);
								if((yintbuf>>16)!=(yintercept>>16))
									goto passvert;

								xintercept=(xtile<<TILESHIFT)+TILEGLOBAL-(pwallposinv<<10);
								yintercept=yintbuf;
								ytile = (short) (yintercept >> TILESHIFT);
								HitVertWall();
							}
							else
							{
								yintbuf=yintercept+((ystep*pwallposinv)>>6);
								if((yintbuf>>16)!=(yintercept>>16))
									goto passvert;

								xintercept=(xtile<<TILESHIFT)-(pwallposinv<<10);
								yintercept=yintbuf;
								ytile = (short) (yintercept >> TILESHIFT);
								HitVertWall();
							}
						}
						else
						{
							int pwallposi = tilehit->pushAmount;
							if(tilehit->pushDirection==MapTile::North) pwallposi = 64-tilehit->pushAmount;
							if((tilehit->pushDirection==MapTile::South && (word)yintercept<(pwallposi<<10))
								|| (tilehit->pushDirection==MapTile::North && (word)yintercept>(pwallposi<<10)))
							{
								if(((uint32_t)yintercept>>16)==tilehit->GetY() && xtile==(signed)tilehit->GetX())
								{
									if((tilehit->pushDirection==MapTile::South && (int32_t)((word)yintercept)+ystep<(pwallposi<<10))
										|| (tilehit->pushDirection==MapTile::North && (int32_t)((word)yintercept)+ystep>(pwallposi<<10)))
										goto passvert;

									if(tilehit->pushDirection==MapTile::South)
									{
										yintercept=(yintercept&0xffff0000)+(pwallposi<<10);
										xintercept=xintercept-((xstep*(64-pwallposi))>>6);
									}
									else
									{
										yintercept=(yintercept&0xffff0000)-TILEGLOBAL+(pwallposi<<10);
										xintercept=xintercept-((xstep*pwallposi)>>6);
									}
									xtile = (short) (xintercept >> TILESHIFT);
									HitHorizWall();
								}
								else
								{
									texdelta = -(pwallposi<<10);
									xintercept=xtile<<TILESHIFT;
									ytile = (short) (yintercept >> TILESHIFT);
									HitVertWall();
								}
							}
							else
							{
								if(((uint32_t)yintercept>>16)==tilehit->GetY() && xtile==(signed)tilehit->GetX())
								{
									texdelta = -(pwallposi<<10);
									xintercept=xtile<<TILESHIFT;
									ytile = (short) (yintercept >> TILESHIFT);
									HitVertWall();
								}
								else
								{
									if((tilehit->pushDirection==MapTile::South && (int32_t)((word)yintercept)+ystep>(pwallposi<<10))
										|| (tilehit->pushDirection==MapTile::North && (int32_t)((word)yintercept)+ystep<(pwallposi<<10)))
										goto passvert;

									if(tilehit->pushDirection==MapTile::South)
									{
										yintercept=(yintercept&0xffff0000)-TILEGLOBAL+(pwallposi<<10);
										xintercept=xintercept-((xstep*pwallposi)>>6);
									}
									else
									{
										yintercept=(yintercept&0xffff0000)+(pwallposi<<10);
										xintercept=xintercept-((xstep*(64-pwallposi))>>6);
									}
									xtile = (short) (xintercept >> TILESHIFT);
									HitHorizWall();
								}
							}
						}
					}
					else
					{
						xintercept=xtile<<TILESHIFT;
						ytile = (short) (yintercept >> TILESHIFT);
						HitVertWall();
					}
				}
				break;
			}
passvert:
			tilehit->visible=true;
			xtile+=xtilestep;
			yintercept+=ystep;
			xspot[0]=xtile;
			xspot[1]=yintercept>>16;
		}
		while(1);
		continue;

		do
		{
			if(xtilestep==-1 && (xintercept>>16)<=xtile) goto vertentry;
			if(xtilestep==1 && (xintercept>>16)>=xtile) goto vertentry;
horizentry:

			if((uint32_t)xintercept>mapwidth*65536-1 || (word)ytile>=mapheight)
			{
				if(ytile<0) yintercept=0, ytile=0;
				else if((unsigned)ytile>=mapheight) yintercept=mapheight<<TILESHIFT, ytile=mapheight-1;
				else ytile=(short) (yintercept >> TILESHIFT);
				if(xintercept<0) xintercept=0, xtile=0;
				else if((unsigned)xintercept>=(mapwidth<<TILESHIFT)) xintercept=mapwidth<<TILESHIFT, xtile=mapwidth-1;
				xspot[0]=0xffff;
				tilehit=0;
				HitVertBorder();
				break;
			}
			if(yspot[0]>=mapwidth || yspot[1]>=mapheight) break;
			tilehit=map->GetSpot(yspot[0], yspot[1], 0);
			if(tilehit && tilehit->tile)
			{
				if(tilehit->tile->offsetHorizontal)
				{
					DetermineHitDir(false);
					int32_t xintbuf=xintercept+(xstep>>1);
					if((xintbuf>>16)!=(xintercept>>16))
						goto passhoriz;
					if((word)xintbuf<tilehit->slideAmount[hitdir])
						goto passhoriz;
					xintercept=xintbuf;
					yintercept=(ytile<<TILESHIFT)+0x8000;
					xtile = (short) (xintercept >> TILESHIFT);
					HitHorizWall();
				}
				else
				{
					bool isPushwall = tilehit->pushAmount != 0 || tilehit->pushReceptor;
					if(tilehit->pushReceptor)
						tilehit = tilehit->pushReceptor;

					if(isPushwall)
					{
						if(tilehit->pushDirection==MapTile::North || tilehit->pushDirection==MapTile::South)
						{
							int32_t xintbuf;
							int pwallposnorm;
							int pwallposinv;
							if(tilehit->pushDirection==MapTile::North)
							{
								pwallposnorm = 64-tilehit->pushAmount;
								pwallposinv = tilehit->pushAmount;
							}
							else
							{
								pwallposnorm = tilehit->pushAmount;
								pwallposinv = 64-tilehit->pushAmount;
							}
							if((tilehit->pushDirection == MapTile::South && ytile==(signed)tilehit->GetY() && ((uint32_t)xintercept>>16)==tilehit->GetX())
								|| (tilehit->pushDirection == MapTile::North && !(ytile==(signed)tilehit->GetY() && ((uint32_t)xintercept>>16)==tilehit->GetX())))
							{
								xintbuf=xintercept+((xstep*pwallposnorm)>>6);
								if((xintbuf>>16)!=(xintercept>>16))
									goto passhoriz;

								yintercept=(ytile<<TILESHIFT)+TILEGLOBAL-(pwallposinv<<10);
								xintercept=xintbuf;
								xtile = (short) (xintercept >> TILESHIFT);
								HitHorizWall();
							}
							else
							{
								xintbuf=xintercept+((xstep*pwallposinv)>>6);
								if((xintbuf>>16)!=(xintercept>>16))
									goto passhoriz;

								yintercept=(ytile<<TILESHIFT)-(pwallposinv<<10);
								xintercept=xintbuf;
								xtile = (short) (xintercept >> TILESHIFT);
								HitHorizWall();
							}
						}
						else
						{
							int pwallposi = tilehit->pushAmount;
							if(tilehit->pushDirection==MapTile::West) pwallposi = 64-tilehit->pushAmount;
							if((tilehit->pushDirection==MapTile::East && (word)xintercept<(pwallposi<<10))
								|| (tilehit->pushDirection==MapTile::West && (word)xintercept>(pwallposi<<10)))
							{
								if(((uint32_t)xintercept>>16)==tilehit->GetX() && ytile==(signed)tilehit->GetY())
								{
									if((tilehit->pushDirection==MapTile::East && (int32_t)((word)xintercept)+xstep<(pwallposi<<10))
										|| (tilehit->pushDirection==MapTile::West && (int32_t)((word)xintercept)+xstep>(pwallposi<<10)))
										goto passhoriz;

									if(tilehit->pushDirection==MapTile::East)
									{
										xintercept=(xintercept&0xffff0000)+(pwallposi<<10);
										yintercept=yintercept-((ystep*(64-pwallposi))>>6);
									}
									else
									{
										xintercept=(xintercept&0xffff0000)-TILEGLOBAL+(pwallposi<<10);
										yintercept=yintercept-((ystep*pwallposi)>>6);
									}
									ytile = (short) (yintercept >> TILESHIFT);
									HitVertWall();
								}
								else
								{
									texdelta = -(pwallposi<<10);
									yintercept=ytile<<TILESHIFT;
									xtile = (short) (xintercept >> TILESHIFT);
									HitHorizWall();
								}
							}
							else
							{
								if(((uint32_t)xintercept>>16)==tilehit->GetX() && ytile==(signed)tilehit->GetY())
								{
									texdelta = -(pwallposi<<10);
									yintercept=ytile<<TILESHIFT;
									xtile = (short) (xintercept >> TILESHIFT);
									HitHorizWall();
								}
								else
								{
									if((tilehit->pushDirection==MapTile::East && (int32_t)((word)xintercept)+xstep>(pwallposi<<10))
										|| (tilehit->pushDirection==MapTile::West && (int32_t)((word)xintercept)+xstep<(pwallposi<<10)))
										goto passhoriz;

									if(tilehit->pushDirection==MapTile::East)
									{
										xintercept=(xintercept&0xffff0000)-TILEGLOBAL+(pwallposi<<10);
										yintercept=yintercept-((ystep*pwallposi)>>6);
									}
									else
									{
										xintercept=(xintercept&0xffff0000)+(pwallposi<<10);
										yintercept=yintercept-((ystep*(64-pwallposi))>>6);
									}
									ytile = (short) (yintercept >> TILESHIFT);
									HitVertWall();
								}
							}
						}
					}
					else
					{
						yintercept=ytile<<TILESHIFT;
						xtile = (short) (xintercept >> TILESHIFT);
						HitHorizWall();
					}
				}
				break;
			}
passhoriz:
			tilehit->visible=true;
			ytile+=ytilestep;
			xintercept+=xstep;
			yspot[0]=xintercept>>16;
			yspot[1]=ytile;
		}
		while(1);
	}
}

/*
====================
=
= WallRefresh
=
====================
*/

void WallRefresh (void)
{
	xpartialdown = viewx&(TILEGLOBAL-1);
	xpartialup = TILEGLOBAL-xpartialdown;
	ypartialdown = viewy&(TILEGLOBAL-1);
	ypartialup = TILEGLOBAL-ypartialdown;

	min_wallheight = viewheight;
	lastside = -1;                  // the first pixel is on a new wall
	viewshift = FixedMul(focallengthy, finetangent[(ANGLE_180+players[0].camera->pitch)>>ANGLETOFINESHIFT]);

	
	angle_t bobangle = ((gamestate.TimeCount<<13)/(20*TICRATE/35)) & FINEMASK;
	fixed curbob = gamestate.victoryflag ? 0 : FixedMul(players[0].bob>>1, finesine[bobangle]);

	viewz = (64<<FRACBITS) - players[0].mo->viewheight + curbob;

	AsmRefresh();
	ScalePost ();                   // no more optimization on last post
}

void CalcViewVariables()
{
	viewangle = players[0].camera->angle;
	midangle = viewangle>>ANGLETOFINESHIFT;
	viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
	viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];
	viewx = players[0].camera->x - FixedMul(focallength,viewcos);
	viewy = players[0].camera->y + FixedMul(focallength,viewsin);

	focaltx = (short)(viewx>>TILESHIFT);
	focalty = (short)(viewy>>TILESHIFT);

	viewtx = (short)(players[0].camera->x >> TILESHIFT);
	viewty = (short)(players[0].camera->y >> TILESHIFT);
}

//==========================================================================

void R_RenderView()
{
	CalcViewVariables();

//
// follow the walls from there to the right, drawing as we go
//
#if defined(USE_FEATUREFLAGS) && defined(USE_STARSKY)
	if(GetFeatureFlags() & FF_STARSKY)
		DrawStarSky(vbuf, vbufPitch);
#endif

	WallRefresh ();

#if defined(USE_FEATUREFLAGS) && defined(USE_PARALLAX)
	if(GetFeatureFlags() & FF_PARALLAXSKY)
		DrawParallax(vbuf, vbufPitch);
#endif
#if defined(USE_FEATUREFLAGS) && defined(USE_CLOUDSKY)
	if(GetFeatureFlags() & FF_CLOUDSKY)
		DrawClouds(vbuf, vbufPitch, min_wallheight);
#endif
	DrawFloorAndCeiling(vbuf, vbufPitch, min_wallheight);

//
// draw all the scaled images
//
	DrawScaleds();                  // draw scaled stuff

#if defined(USE_FEATUREFLAGS) && defined(USE_RAIN)
	if(GetFeatureFlags() & FF_RAIN)
		DrawRain(vbuf, vbufPitch);
#endif
#if defined(USE_FEATUREFLAGS) && defined(USE_SNOW)
	if(GetFeatureFlags() & FF_SNOW)
		DrawSnow(vbuf, vbufPitch);
#endif

	DrawPlayerWeapon ();    // draw players[0].mo's hands

	if(Keyboard[sc_Tab] && viewsize == 21)
		ShowActStatus();
}

/*
========================
=
= ThreeDRefresh
=
========================
*/

void    ThreeDRefresh (void)
{
	if (fizzlein)
		FizzleFadeStart();

//
// clear out the traced array
//
	map->ClearVisibility();

	vbuf = VL_LockSurface(screenBuffer);
	if(vbuf == NULL) return;

	vbuf += screenofs;
	vbufPitch = bufferPitch;

	R_RenderView();

	VL_UnlockSurface(screenBuffer);
	vbuf = NULL;

//
// show screen and time last cycle
//

	if (fizzlein)
	{
		FizzleFade(screenBuffer, 0, 0, screenWidth, screenHeight, 20, false);
		fizzlein = false;

		lasttimecount = GetTimeCount();          // don't make a big tic count
	}
	else
	{
		if (fpscounter)
		{
			FString fpsDisplay;
			fpsDisplay.Format("%2u fps", fps);

			word x = 0;
			word y = 0;
			word width, height;
			VW_MeasurePropString(ConFont, fpsDisplay, width, height);
			MenuToRealCoords(x, y, width, height, MENU_TOP);
			VWB_Clear(GPalette.BlackIndex, x, y, x+width+1, y+height+1);
			px = 0;
			py = 0;
			pa = MENU_TOP;
			VWB_DrawPropString(ConFont, fpsDisplay, CR_WHITE);
			pa = MENU_CENTER;
		}
		VH_UpdateScreen();
	}

	if (fpscounter)
	{
		fps_frames++;
		fps_time+=tics;

		if(fps_time>35)
		{
			fps_time-=35;
			fps=fps_frames<<1;
			fps_frames=0;
		}
	}
}
