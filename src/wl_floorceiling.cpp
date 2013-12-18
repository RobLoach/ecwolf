#include "textures/textures.h"
#include "c_cvars.h"
#include "id_ca.h"
#include "gamemap.h"
#include "wl_def.h"
#include "wl_draw.h"
#include "wl_main.h"
#include "wl_shade.h"
#include "r_data/colormaps.h"

#include <climits>

extern fixed viewshift;
extern fixed viewz;

static void R_DrawPlane(byte *vbuf, unsigned vbufPitch, int min_wallheight, int halfheight, fixed planeheight)
{
	fixed dist;                                // distance to row projection
	fixed tex_step;                            // global step per one screen pixel
	fixed gu, gv, du, dv;                      // global texture coordinates
	const byte *tex = NULL;
	int texwidth, texheight;
	fixed texxscale, texyscale;
	FTextureID lasttex;
	byte *tex_offset;
	bool useOptimized = false;

	const fixed heightFactor = abs(planeheight/32);
	int y0 = (((min_wallheight >> 3)*heightFactor)>>FRACBITS) - abs(viewshift);
	if(y0 > halfheight)
		return; // view obscured by walls
	if(y0 <= 0) y0 = 1; // don't let division by zero

	lasttex.SetInvalid();

	const unsigned int mapwidth = map->GetHeader().width;
	const unsigned int mapheight = map->GetHeader().height;

	fixed planenumerator = FixedMul(heightnumerator, planeheight);
	const bool floor = planenumerator < 0;
	int tex_offsetPitch;
	if(floor)
	{
		tex_offset = vbuf + (signed)vbufPitch * (halfheight + y0);
		tex_offsetPitch = vbufPitch-viewwidth;
		planenumerator *= -1;
	}
	else
	{
		tex_offset = vbuf + (signed)vbufPitch * (halfheight - y0 - 1);
		tex_offsetPitch = -viewwidth-vbufPitch;
	}

	unsigned int oldmapx = INT_MAX, oldmapy = INT_MAX;
	const byte* curshades = NormalLight.Maps;
	// draw horizontal lines
	for(int y = y0;floor ? y+halfheight < viewheight : y < halfheight; ++y, tex_offset += tex_offsetPitch)
	{
		if(floor ? (y+halfheight < 0) : (y < halfheight - viewheight))
		{
			tex_offset += viewwidth;
			continue;
		}

		dist = (planenumerator / (y + 1));
		gu =  viewx + FixedMul(dist, viewcos);
		gv = -viewy + FixedMul(dist, viewsin);
		tex_step = dist / scale;
		du =  FixedMul(tex_step, viewsin);
		dv = -FixedMul(tex_step, viewcos);
		gu -= (viewwidth >> 1) * du;
		gv -= (viewwidth >> 1) * dv; // starting point (leftmost)

		// Depth fog
		const int shade = LIGHT2SHADE(gLevelLight + r_extralight);
		const int tz = FixedMul(FixedDiv(r_depthvisibility, abs(planeheight)), abs(((halfheight)<<16) - ((halfheight-y)<<16)));
		curshades = &NormalLight.Maps[GETPALOOKUP(tz, shade)<<8];

		for(unsigned int x = 0;x < (unsigned)viewwidth; ++x, ++tex_offset)
		{
			if(((wallheight[x] >> 3)*heightFactor)>>FRACBITS <= y)
			{
				unsigned int curx = (gu >> TILESHIFT);
				unsigned int cury = (-(gv >> TILESHIFT) - 1);

				if(curx != oldmapx || cury != oldmapy)
				{
					oldmapx = curx;
					oldmapy = cury;
					const MapSpot spot = map->GetSpot(oldmapx%mapwidth, oldmapy%mapheight, 0);

					if(spot->sector)
					{
						FTextureID curtex = spot->sector->texture[floor ? MapSector::Floor : MapSector::Ceiling];
						if (curtex != lasttex && curtex.isValid())
						{
							FTexture * const texture = TexMan(curtex);
							lasttex = curtex;
							tex = texture->GetPixels();
							texwidth = texture->GetWidth();
							texheight = texture->GetHeight();
							texxscale = texture->xScale>>10;
							texyscale = -texture->yScale>>10;

							useOptimized = texwidth == 64 && texheight == 64 && texxscale == FRACUNIT>>10 && texyscale == -FRACUNIT>>10;
						}
					}
					else
						tex = NULL;
				}

				if(tex)
				{
					if(useOptimized)
					{
						const int u = (gu>>10) & 63;
						const int v = (-gv>>10) & 63;
						const unsigned texoffs = (u * 64) + v;
						*tex_offset = curshades[tex[texoffs]];
					}
					else
					{
						const int u = (FixedMul(gu-512, texxscale)) & (texwidth-1);
						const int v = (FixedMul(gv+512, texyscale)) & (texheight-1);
						const unsigned texoffs = (u * texheight) + v;
						*tex_offset = curshades[tex[texoffs]];
					}
				}
			}
			gu += du;
			gv += dv;
		}
	}
}

// Textured Floor and Ceiling by DarkOne
// With multi-textured floors and ceilings stored in lower and upper bytes of
// according tile in third mapplane, respectively.
void DrawFloorAndCeiling(byte *vbuf, unsigned vbufPitch, int min_wallheight)
{
	const int halfheight = (viewheight >> 1) - viewshift;

	R_DrawPlane(vbuf, vbufPitch, min_wallheight, halfheight, viewz-(64<<FRACBITS));
	R_DrawPlane(vbuf, vbufPitch, min_wallheight, halfheight, viewz);
}
