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
#include "id_ca.h"
#include "id_vl.h"
#include "gamemap.h"
#include "r_data/colormaps.h"
#include "v_video.h"
#include "wl_agent.h"
#include "wl_draw.h"

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

AutoMap::AutoMap()
{
	amangle = 0;
	minmaxSel = 0;
	amsin = 0;
	amcos = FRACUNIT;
	rottable[0][0] = 1.0;
	rottable[0][1] = 0.0;
	rottable[1][0] = 1.0;
	rottable[1][1] = 1.0;
}

AutoMap::~AutoMap()
{
}

void AutoMap::CalculateDimensions()
{
	static const int windowsize = 16;

	amsizex = screenWidth*2/3;
	amsizey = screenHeight*2/3;
	amx = (screenWidth - amsizex)/2;
	amy = (screenHeight - amsizey)/2;
	SetScale((amsizey<<FRACBITS)/windowsize);

	// Since the simple polygon fill function seems to be off by one in the y
	// direction, lets shift this up!
	--amy;
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
	MapSpot spot;
	float shiftx, shifty;
};
void AutoMap::Draw()
{
	TArray<AMPWall> pwalls;
	TArray<FVector2> points;
	const unsigned int mapwidth = map->GetHeader().width;
	const unsigned int mapheight = map->GetHeader().height;

	screen->Clear(amx, amy+1, amx+amsizex, amy+amsizey, GPalette.BlackIndex, 0);

	const fixed playerx = players[0].mo->x;
	const fixed playery = players[0].mo->y;

	if(amangle != players[0].mo->angle-ANGLE_90)
	{
		amangle = players[0].mo->angle-ANGLE_90;
		minmaxSel = amangle/ANGLE_90;
		amsin = finesine[amangle>>ANGLETOFINESHIFT];
		amcos = finecosine[amangle>>ANGLETOFINESHIFT];

		// For rotating the tiles, this table includes the point offsets based on the current scale
		rottable[0][0] = FIXED2FLOAT(FixedMul(scale, amcos)); rottable[0][1] = FIXED2FLOAT(FixedMul(scale, amsin));
		rottable[1][0] = FIXED2FLOAT(FixedMul(scale, amcos) - FixedMul(scale, amsin)); rottable[1][1] = FIXED2FLOAT(FixedMul(scale, amsin) + FixedMul(scale, amcos));
	}

	const double originx = (amx+amsizex/2) - (FIXED2FLOAT(FixedMul(FixedMul(scale, playerx&0xFFFF), amcos) - FixedMul(FixedMul(scale, playery&0xFFFF), amsin)));
	const double originy = (amy+amsizey/2) - (FIXED2FLOAT(FixedMul(FixedMul(scale, playerx&0xFFFF), amsin) + FixedMul(FixedMul(scale, playery&0xFFFF), amcos)));
	const double texScale = FIXED2FLOAT(scale>>6);

	for(unsigned int my = 0;my < mapheight;++my)
	{
		MapSpot spot = map->GetSpot(0, my, 0);
		for(unsigned int mx = 0;mx < mapwidth;++mx, ++spot)
		{
			if(!(spot->amFlags & AM_Visible))
				continue;

			if(TransformTile(spot, FixedMul((mx<<FRACBITS)-playerx, scale), FixedMul((my<<FRACBITS)-playery, scale), points))
			{
				FTexture *tex;
				int brightness;
				if(spot->tile && !spot->pushAmount && !spot->pushReceptor)
				{
					brightness = 256;
					tex = TexMan(spot->texture[0]);
				}
				else if(spot->sector)
				{
					brightness = 128;
					tex = TexMan(spot->sector->texture[MapSector::Floor]);
				}
				else
					tex = NULL;

				if(tex)
					screen->FillSimplePoly(tex, &points[0], points.Size(), originx, originy, texScale, texScale, ~amangle, &NormalLight, brightness);
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
					pwall.spot = spot;
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
		FTexture *tex = TexMan(pwall.spot->texture[0]);
		if(tex)
			screen->FillSimplePoly(tex, &pwall.points[0], pwall.points.Size(), originx + pwall.shiftx, originy + pwall.shifty, texScale, texScale, ~amangle, &NormalLight, 256);
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

void AutoMap::SetScale(fixed scale)
{
	this->scale = scale;

	if(amangle == 0)
	{
		rottable[0][0] = FIXED2FLOAT(scale);
		rottable[0][1] = 0.0;
		rottable[1][0] = FIXED2FLOAT(scale);
		rottable[1][1] = FIXED2FLOAT(scale);
	}
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
	static AutoMap am;
	am.CalculateDimensions();
	am.Draw();
}
