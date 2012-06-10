#include "textures/textures.h"
#include "c_cvars.h"
#include "id_ca.h"
#include "gamemap.h"
#include "wl_def.h"
#include "wl_draw.h"
#include "wl_main.h"
#include "wl_shade.h"
#include "r_data/colormaps.h"

// Textured Floor and Ceiling by DarkOne
// With multi-textured floors and ceilings stored in lower and upper bytes of
// according tile in third mapplane, respectively.
void DrawFloorAndCeiling(byte *vbuf, unsigned vbufPitch, int min_wallheight)
{
	fixed dist;                                // distance to row projection
	fixed tex_step;                            // global step per one screen pixel
	fixed gu, gv, du, dv;                      // global texture coordinates
	int u, v;                                  // local texture coordinates
	static const byte *toptex, *bottex;
	static FTextureID lasttoptex, lastbottex;

	int halfheight = viewheight >> 1;
	int y0 = min_wallheight >> 3;              // starting y value
	if(y0 > halfheight)
		return;                                // view obscured by walls
	if(!y0) y0 = 1;                            // don't let division by zero
	unsigned bot_offset0 = vbufPitch * (halfheight + y0);
	unsigned top_offset0 = vbufPitch * (halfheight - y0 - 1);

	const unsigned mapwidth = map->GetHeader().width;
	const unsigned mapheight = map->GetHeader().height;

	// draw horizontal lines
	for(int y = y0, bot_offset = bot_offset0, top_offset = top_offset0;
		y <= halfheight; y++, bot_offset += vbufPitch, top_offset -= vbufPitch)
	{
		dist = (heightnumerator / (y + 1)) << 5;
		gu =  viewx + FixedMul(dist, viewcos);
		gv = -viewy + FixedMul(dist, viewsin);
		tex_step = (dist << 8) / viewwidth / 175;
		du =  FixedMul(tex_step, viewsin);
		dv = -FixedMul(tex_step, viewcos);
		gu -= (viewwidth >> 1) * du;
		gv -= (viewwidth >> 1) * dv; // starting point (leftmost)
		const BYTE *curshades;
		if(r_depthfog)
			curshades = &NormalLight.Maps[256*GetShade(y << 3)];
		else
			curshades = NormalLight.Maps;
		for(int x = 0, bot_add = bot_offset, top_add = top_offset;
			x < viewwidth; x++, bot_add++, top_add++)
		{
			if(wallheight[x] >> 3 <= y)
			{
				int curx = (gu >> TILESHIFT)%mapwidth;
				int cury = (-(gv >> TILESHIFT) - 1)%mapheight;
				MapSpot spot = map->GetSpot(curx, cury, 0);
				if(spot->sector)
				{
					FTextureID curtoptex = spot->sector->texture[MapSector::Ceiling];
					if (curtoptex != lasttoptex && curtoptex.isValid())
					{
						lasttoptex = curtoptex;
						toptex = TexMan(curtoptex)->GetPixels();
					}
					FTextureID curbottex = spot->sector->texture[MapSector::Floor];
					if (curbottex != lastbottex && curbottex.isValid())
					{
						lastbottex = curbottex;
						bottex = TexMan(curbottex)->GetPixels();
					}
					u = (gu >> (TILESHIFT - TEXTURESHIFT)) & (TEXTURESIZE - 1);
					v = (gv >> (TILESHIFT - TEXTURESHIFT)) & (TEXTURESIZE - 1);
					unsigned texoffs = (u << TEXTURESHIFT) + (TEXTURESIZE - 1) - v;
					if(curtoptex.isValid() && y < halfheight)
						vbuf[top_add] = curshades[toptex[texoffs]];
					if(curbottex.isValid() && y+halfheight < viewheight)
						vbuf[bot_add] = curshades[bottex[texoffs]];
				}
			}
			gu += du;
			gv += dv;
		}
	}
}
