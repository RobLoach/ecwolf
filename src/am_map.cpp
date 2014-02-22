/*
** am_map.cpp
**
**---------------------------------------------------------------------------
** Copyright 2013 Braden Obrzut
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
**
*/

#include "wl_def.h"
#include "am_map.h"
#include "colormatcher.h"
#include "id_ca.h"
#include "id_in.h"
#include "id_vl.h"
#include "gamemap.h"
#include "r_data/colormaps.h"
#include "v_video.h"
#include "wl_agent.h"
#include "wl_draw.h"
#include "wl_game.h"
#include "wl_main.h"
#include "wl_play.h"

AutoMap AM_Main;
AutoMap AM_Overlay;

enum
{
	AMO_Off,
	AMO_On,
	AMO_Both
};
enum
{
	AMR_Off,
	AMR_On,
	AMR_Overlay
};

unsigned automap = 0;
bool am_cheat = false;
unsigned am_rotate = 0;
bool am_drawtexturedwalls = true;
bool am_drawfloors = false;
unsigned am_overlay = 0;
bool am_pause = true;
bool am_needsrecalc = false;

void AM_ChangeResolution()
{
	if(StatusBar == NULL)
	{
		am_needsrecalc = true;
		return;
	}

	unsigned int y = statusbary1;
	unsigned int height = statusbary2 - statusbary1;

	AM_Main.CalculateDimensions(0, y, screenWidth, height);
	AM_Overlay.CalculateDimensions(viewscreenx, viewscreeny, viewwidth, viewheight);
}

void AM_CheckKeys()
{
	if(ambuttonstate[bt_zoomin])
	{
		AM_Overlay.SetScale(FRACUNIT*1.05, true);
		AM_Main.SetScale(FRACUNIT*1.05, true);
	}
	if(ambuttonstate[bt_zoomout])
	{
		AM_Overlay.SetScale(FRACUNIT*0.95, true);
		AM_Main.SetScale(FRACUNIT*0.95, true);
	}

	if(am_pause)
	{
		static const fixed PAN_AMOUNT = FRACUNIT/4;
		fixed panx = 0, pany = 0;
		if(ambuttonstate[bt_panleft])
			panx += PAN_AMOUNT;
		if(ambuttonstate[bt_panright])
			panx -= PAN_AMOUNT;
		if(ambuttonstate[bt_panup])
			pany += PAN_AMOUNT;
		if(ambuttonstate[bt_pandown])
			pany -= PAN_AMOUNT;
		AM_Main.SetPanning(panx, pany, true);
	}
}

void AM_UpdateFlags()
{
	unsigned int flags = 0;
	unsigned int ovFlags = AutoMap::AMF_Overlay;

	if(am_rotate == AMR_On) flags |= AutoMap::AMF_Rotate;
	if(am_drawtexturedwalls) flags |= AutoMap::AMF_DrawTexturedWalls;
	if(am_drawfloors) flags |= AutoMap::AMF_DrawFloor;

	ovFlags |= flags;
	if(am_rotate == AMR_Overlay) ovFlags |= AutoMap::AMF_Rotate;

	AM_Main.SetFlags(~flags, false);
	AM_Overlay.SetFlags(~ovFlags, false);
	AM_Main.SetFlags(flags, true);
	AM_Overlay.SetFlags(ovFlags, true);
}

void AM_Toggle()
{
	++automap;
	if(automap == AMA_Overlay && am_overlay == AMO_Off)
		++automap;
	else if(automap > AMA_Normal || (automap == AMA_Normal && am_overlay == AMO_On))
	{
		automap = AMA_Off;
		AM_Main.SetPanning(0, 0, false);
		DrawPlayScreen();
	}

	if(am_pause)
	{
		if(automap == AMA_Normal)
			Paused |= 2;
		else
			Paused &= ~2;
	}
}

// Culling table
static const struct
{
	const struct
	{
		const unsigned short Min, Max;
	} X, Y;
} AM_MinMax[4] = {
	{ {3, 1}, {0, 2} },
	{ {2, 0}, {3, 1} },
	{ {1, 3}, {2, 0} },
	{ {0, 2}, {1, 3} }
};

// #FF9200
struct AMVectorPoint
{
	fixed X, Y;
};
static const AMVectorPoint AM_Arrow[] =
{
	{0, -FRACUNIT/2},
	{FRACUNIT/2, 0},
	{FRACUNIT/4, 0},
	{FRACUNIT/4, FRACUNIT*7/16},
	{-FRACUNIT/4, FRACUNIT*7/16},
	{-FRACUNIT/4, 0},
	{-FRACUNIT/2, 0},
	{0, -FRACUNIT/2},
};

AutoMap::AutoMap(unsigned int flags) :
	fullRefresh(true), amFlags(flags),
	ampanx(0), ampany(0)
{
	amangle = 0;
	minmaxSel = 0;
	amsin = 0;
	amcos = FRACUNIT;
	absscale = FRACUNIT/2;
	rottable[0][0] = 1.0;
	rottable[0][1] = 0.0;
	rottable[1][0] = 1.0;
	rottable[1][1] = 1.0;
}

AutoMap::~AutoMap()
{
}

void AutoMap::CalculateDimensions(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	fullRefresh = true;
	amsizex = width;
	amsizey = height;
	amx = x;
	amy = y;

	// Since the simple polygon fill function seems to be off by one in the y
	// direction, lets shift this up!
	--amy;

	// TODO: Find a better spot for this
	ArrowColor.color = MAKERGB(0xFF,0x80,0x00);
	ArrowColor.palcolor = ColorMatcher.Pick(0xFF, 0x80, 0x00);

	BackgroundColor.color = MAKERGB(0x55,0x55,0x55);
	BackgroundColor.palcolor = ColorMatcher.Pick(0x55, 0x55, 0x55);

	WallColor.color = MAKERGB(0x00, 0x8E, 0x00);
	WallColor.palcolor = ColorMatcher.Pick(0x00, 0x8E, 0x00);

	DoorColor.color = MAKERGB(0x10, 0xC7, 0x10);
	DoorColor.palcolor = ColorMatcher.Pick(0x10, 0xC7, 0x10);
}

// Sutherland–Hodgman algorithm
void AutoMap::ClipTile(TArray<FVector2> &points) const
{
	for(int i = 0;i < 4;++i)
	{
		TArray<FVector2> input(points);
		points.Clear();
		FVector2 *s = &input[0];
		for(unsigned j = input.Size();j-- > 0;)
		{
			FVector2 &e = input[j];

			bool Ein, Sin;
			switch(i)
			{
				case 0: // Left
					Ein = e.X > amx;
					Sin = s->X > amx;
					break;
				case 1: // Top
					Ein = e.Y > amy;
					Sin = s->Y > amy;
					break;
				case 2: // Right
					Ein = e.X < amx+amsizex;
					Sin = s->X < amx+amsizex;
					break;
				case 3: // Bottom
					Ein = e.Y < amy+amsizey;
					Sin = s->Y < amy+amsizey;
					break;
			}
			if(Ein)
			{
				if(!Sin)
					points.Push(GetClipIntersection(e, *s, i));
				points.Push(e);
			}
			else if(Sin)
				points.Push(GetClipIntersection(e, *s, i));
			s = &e;
		}
	}
}

struct AMPWall
{
public:
	TArray<FVector2> points;
	FTextureID texid;
	float shiftx, shifty;
};
void AutoMap::Draw()
{
	TArray<AMPWall> pwalls;
	TArray<FVector2> points;
	const unsigned int mapwidth = map->GetHeader().width;
	const unsigned int mapheight = map->GetHeader().height;

	// Some magic, min scale is approximately small enough so that a rotated automap will fit on screen (22/32 ~ 1/sqrt(2))
	const fixed minscale = ((screenHeight*22)<<(FRACBITS-5))/mapheight;
	scale = minscale + FixedMul(absscale, (screenHeight<<(FRACBITS-4)) - minscale);

	if(!(amFlags & AMF_Overlay))
		screen->Clear(amx, amy+1, amx+amsizex, amy+amsizey+1, BackgroundColor.palcolor, BackgroundColor.color);

	const fixed playerx = players[0].mo->x;
	const fixed playery = players[0].mo->y;

	if(fullRefresh || amangle != ((amFlags & AMF_Rotate) ? players[0].mo->angle-ANGLE_90 : 0))
	{
		amangle = (amFlags & AMF_Rotate) ? players[0].mo->angle-ANGLE_90 : 0;
		minmaxSel = amangle/ANGLE_90;
		amsin = finesine[amangle>>ANGLETOFINESHIFT];
		amcos = finecosine[amangle>>ANGLETOFINESHIFT];

		// For rotating the tiles, this table includes the point offsets based on the current scale
		rottable[0][0] = FIXED2FLOAT(FixedMul(scale, amcos)); rottable[0][1] = FIXED2FLOAT(FixedMul(scale, amsin));
		rottable[1][0] = FIXED2FLOAT(FixedMul(scale, amcos) - FixedMul(scale, amsin)); rottable[1][1] = FIXED2FLOAT(FixedMul(scale, amsin) + FixedMul(scale, amcos));

		fullRefresh = false;
	}

	const fixed pany = FixedMul(ampany, amcos) - FixedMul(ampanx, amsin);
	const fixed panx = FixedMul(ampany, amsin) + FixedMul(ampanx, amcos);
	const fixed ofsx = playerx - panx;
	const fixed ofsy = playery - pany;

	const double originx = (amx+amsizex/2) - (FIXED2FLOAT(FixedMul(FixedMul(scale, ofsx&0xFFFF), amcos) - FixedMul(FixedMul(scale, ofsy&0xFFFF), amsin)));
	const double originy = (amy+amsizey/2) - (FIXED2FLOAT(FixedMul(FixedMul(scale, ofsx&0xFFFF), amsin) + FixedMul(FixedMul(scale, ofsy&0xFFFF), amcos)));
	const double origTexScale = FIXED2FLOAT(scale>>6);

	for(unsigned int my = 0;my < mapheight;++my)
	{
		MapSpot spot = map->GetSpot(0, my, 0);
		for(unsigned int mx = 0;mx < mapwidth;++mx, ++spot)
		{
			if(!(spot->amFlags & AM_Visible) && !am_cheat)
				continue;

			if(TransformTile(spot, FixedMul((mx<<FRACBITS)-ofsx, scale), FixedMul((my<<FRACBITS)-ofsy, scale), points))
			{
				double texScale = origTexScale;

				FTexture *tex;
				Color *color = NULL;
				int brightness;
				if(spot->tile && !spot->pushAmount && !spot->pushReceptor)
				{
					brightness = 256;
					if((amFlags & AMF_DrawTexturedWalls))
					{
						if(spot->tile->overhead.isValid())
						{
							tex = TexMan(spot->tile->overhead);
							texScale *= 8;
						}
						else if(spot->tile->offsetHorizontal)
							tex = TexMan(spot->texture[MapTile::North]);
						else
							tex = TexMan(spot->texture[MapTile::East]);
					}
					else
					{
						tex = NULL;
						if(spot->tile->offsetHorizontal || spot->tile->offsetVertical)
							color = &DoorColor;
						else
							color = &WallColor;
					}
				}
				else if((amFlags & AMF_DrawFloor) && spot->sector)
				{
					brightness = 128;
					tex = TexMan(spot->sector->texture[MapSector::Floor]);
				}
				else
					tex = NULL;

				if(tex)
					screen->FillSimplePoly(tex, &points[0], points.Size(), originx, originy, texScale, texScale, ~amangle, &NormalLight, brightness);
				else if(color)
					screen->FillSimplePoly(NULL, &points[0], points.Size(), originx, originy, texScale, texScale, ~amangle, &NormalLight, brightness, color->palcolor, color->color);
			}

			// We need to check this even if the origin tile isn't visible since
			// the destination spot may be!
			if(spot->pushAmount)
			{
				fixed tx = (mx<<FRACBITS), ty = my<<FRACBITS;
				switch(spot->pushDirection)
				{
					default:
					case MapTile::East:
						tx += (spot->pushAmount<<10);
						break;
					case MapTile::West:
						tx -= (spot->pushAmount<<10);
						break;
					case MapTile::South:
						ty += (spot->pushAmount<<10);
						break;
					case MapTile::North:
						ty -= (spot->pushAmount<<10);
						break;
				}
				if(TransformTile(spot, FixedMul(tx-playerx, scale), FixedMul(ty-playery, scale), points))
				{
					AMPWall pwall;
					pwall.points = points;
					pwall.texid = spot->texture[0];
					pwall.shiftx = (FIXED2FLOAT(FixedMul(FixedMul(scale, tx&0xFFFF), amcos) - FixedMul(FixedMul(scale, ty&0xFFFF), amsin)));
					pwall.shifty = (FIXED2FLOAT(FixedMul(FixedMul(scale, tx&0xFFFF), amsin) + FixedMul(FixedMul(scale, ty&0xFFFF), amcos)));
					pwalls.Push(pwall);
				}
			}
		}
	}

	for(unsigned int pw = pwalls.Size();pw-- > 0;)
	{
		AMPWall &pwall = pwalls[pw];
		if((amFlags & AMF_DrawTexturedWalls))
		{
			FTexture *tex = TexMan(pwall.texid);
			if(tex)
				screen->FillSimplePoly(tex, &pwall.points[0], pwall.points.Size(), originx + pwall.shiftx, originy + pwall.shifty, origTexScale, origTexScale, ~amangle, &NormalLight, 256);
		}
		else
			screen->FillSimplePoly(NULL, &pwall.points[0], pwall.points.Size(), originx + pwall.shiftx, originy + pwall.shifty, origTexScale, origTexScale, ~amangle, &NormalLight, 256, WallColor.palcolor, WallColor.color);
	}

	DrawVector(AM_Arrow, 8, amx + (amsizex>>1), amy + (amsizey>>1), (amFlags & AMF_Rotate) ? 0 : ANGLE_90-players[0].mo->angle, ArrowColor);
}

void AutoMap::DrawVector(const AMVectorPoint *points, unsigned int numPoints, int x, int y, angle_t angle, const Color &c) const
{
	int x1, y1, x2, y2;

	x1 = FixedMul(points[numPoints-1].X, scale)>>FRACBITS;
	y1 = FixedMul(points[numPoints-1].Y, scale)>>FRACBITS;
	if(angle)
	{
		const fixed rsin = finesine[angle>>ANGLETOFINESHIFT];
		const fixed rcos = finecosine[angle>>ANGLETOFINESHIFT];
		int tmp;
		tmp = FixedMul(x1, rcos) - FixedMul(y1, rsin);
		y1 = FixedMul(x1, rsin) + FixedMul(y1, rcos);
		x1 = tmp;

		for(unsigned int i = numPoints-1;i-- > 0;x1 = x2, y1 = y2)
		{
			x2 = FixedMul(points[i].X, scale)>>FRACBITS;
			y2 = FixedMul(points[i].Y, scale)>>FRACBITS;

			tmp = FixedMul(x2, rcos) - FixedMul(y2, rsin);
			y2 = FixedMul(x2, rsin) + FixedMul(y2, rcos);
			x2 = tmp;

			screen->DrawLine(x + x1, y + y1, x + x2, y + y2, c.palcolor, c.color);
		}
	}
	else
	{
		for(unsigned int i = numPoints-1;i-- > 0;x1 = x2, y1 = y2)
		{
			x2 = FixedMul(points[i].X, scale)>>FRACBITS;
			y2 = FixedMul(points[i].Y, scale)>>FRACBITS;

			screen->DrawLine(x + x1, y + y1, x + x2, y + y2, c.palcolor, c.color);
		}
	}
}

FVector2 AutoMap::GetClipIntersection(const FVector2 &p1, const FVector2 &p2, unsigned edge) const
{
	// If we are clipping a vertical line, it should be because our angle is
	// 90 degrees and it's clip against the top or bottom
	if((edge&1) == 1 && ((amangle>>ANGLETOFINESHIFT)&(ANG90-1)) == 0)
	{
		if(edge == 1)
			return FVector2(p1.X, amy);
		return FVector2(p1.X, amy+amsizey);
	}
	else
	{
		const float A = p2.Y - p1.Y;
		const float B = p1.X - p2.X;
		const float C = A*p1.X + B*p1.Y;
		switch(edge)
		{
			default:
			case 0: // Left
				return FVector2(amx, (C - A*amx)/B);
			case 1: // Top
				return FVector2((C - B*amy)/A, amy);
			case 2: // Right
				return FVector2(amx+amsizex, (C - A*(amx+amsizex))/B);
			case 3: // Bottom
				return FVector2((C - B*(amy+amsizey))/A, amy+amsizey);
		}
	}
}

void AutoMap::SetFlags(unsigned int flags, bool set)
{
	if(set)
		amFlags |= flags;
	else
		amFlags &= ~flags;
}

void AutoMap::SetPanning(fixed x, fixed y, bool relative)
{
	if(relative)
	{
		ampanx += x;
		ampany += y;
	}
	else
	{
		ampanx = 0;
		ampany = 0;
	}
}

void AutoMap::SetScale(fixed scale, bool relative)
{
	if(relative)
		absscale = FixedMul(absscale, scale);
	else
		absscale = scale;
	absscale = clamp<fixed>(absscale, FRACUNIT/50, FRACUNIT);

	fullRefresh = true;
}

bool AutoMap::TransformTile(MapSpot spot, fixed x, fixed y, TArray<FVector2> &points) const
{
	fixed rotx = FixedMul(x, amcos) - FixedMul(y, amsin) + (amsizex<<(FRACBITS-1));
	fixed roty = FixedMul(x, amsin) + FixedMul(y, amcos) + (amsizey<<(FRACBITS-1));
	double x1 = amx + FIXED2FLOAT(rotx);
	double y1 = amy + FIXED2FLOAT(roty);
	points.Resize(4);
	points[0] = FVector2(x1, y1);
	points[1] = FVector2(x1 + rottable[0][0], y1 + rottable[0][1]);
	points[2] = FVector2(x1 + rottable[1][0], y1 + rottable[1][1]);
	points[3] = FVector2(x1 - rottable[0][1], y1 + rottable[0][0]);


	const float xmin = points[AM_MinMax[minmaxSel].X.Min].X;
	const float xmax = points[AM_MinMax[minmaxSel].X.Max].X;
	const float ymin = points[AM_MinMax[minmaxSel].Y.Min].Y;
	const float ymax = points[AM_MinMax[minmaxSel].Y.Max].Y;

	// Cull out tiles which never touch the automap area
	if((xmax < amx || xmin > amx+amsizex) || (ymax < amy || ymin > amy+amsizey))
		return false;

	// If the tile is partially in the automap area, clip
	if((ymax > amy+amsizey || ymin < amy) || (xmax > amx+amsizex || xmin < amx))
		ClipTile(points);
	return true;
}

void BasicOverhead()
{
	if(am_needsrecalc)
		AM_ChangeResolution();

	switch(automap)
	{
		case AMA_Overlay:
			AM_Overlay.Draw();
			break;
		case AMA_Normal:
		{
			int oldviewsize = viewsize;
			viewsize = 20;
			DrawPlayScreen();
			viewsize = oldviewsize;
			AM_Main.Draw();
			break;
		}
		default: break;
	}
}
