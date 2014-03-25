/*
** r_sprites.cpp
**
**---------------------------------------------------------------------------
** Copyright 2011 Braden Obrzut
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

#include "textures/textures.h"
#include "c_cvars.h"
#include "r_sprites.h"
#include "linkedlist.h"
#include "tarray.h"
#include "templates.h"
#include "actor.h"
#include "thingdef/thingdef.h"
#include "v_palette.h"
#include "wl_agent.h"
#include "wl_draw.h"
#include "wl_main.h"
#include "wl_play.h"
#include "wl_shade.h"
#include "zstring.h"
#include "r_data/colormaps.h"
#include "a_inventory.h"

struct SpriteInfo
{
	union
	{
		char 		name[5];
		uint32_t	iname;
	};
	unsigned int	frames;
	unsigned int	numFrames;
};

struct Sprite
{
	static const uint8_t NO_FRAMES = 255; // If rotations == NO_FRAMES

	FTextureID	texture[8];
	uint8_t		rotations;
	uint16_t	mirror; // Mirroring bitfield
};

static TArray<Sprite> spriteFrames;
static TArray<SpriteInfo> loadedSprites;

bool R_CheckSpriteValid(unsigned int spr)
{
	if(spr < NUM_SPECIAL_SPRITES)
		return true;

	SpriteInfo &sprite = loadedSprites[spr];
	if(sprite.numFrames == 0)
		return false;
	return true;
}

uint32_t R_GetNameForSprite(unsigned int index)
{
	return loadedSprites[index].iname;
}

// Cache sprite name lookups
unsigned int R_GetSprite(const char* spr)
{
	static unsigned int mid = 0;

	union
	{
		char 		name[4];
		uint32_t	iname;
	} tmp;
	memcpy(tmp.name, spr, 4);

	if(tmp.iname == loadedSprites[mid].iname)
		return mid;

	for(mid = 0;mid < NUM_SPECIAL_SPRITES;++mid)
	{
		if(tmp.iname == loadedSprites[mid].iname)
			return mid;
	}

	unsigned int max = loadedSprites.Size()-1;
	unsigned int min = NUM_SPECIAL_SPRITES;
	mid = (min+max)/2;
	do
	{
		if(tmp.iname == loadedSprites[mid].iname)
			return mid;

		if(tmp.iname < loadedSprites[mid].iname)
			max = mid-1;
		else if(tmp.iname > loadedSprites[mid].iname)
			min = mid+1;
		mid = (min+max)/2;
	}
	while(max >= min);

	// I don't think this should ever happen, but if it does return no sprite.
	return 0;
}

FTexture *R_GetAMSprite(AActor *actor, angle_t rotangle, bool &flip)
{
	if(actor->sprite == SPR_NONE || loadedSprites[actor->sprite].numFrames == 0)
		return NULL;

	const Sprite &spr = spriteFrames[loadedSprites[actor->sprite].frames+actor->state->frame];
	FTexture *tex;
	if(spr.rotations == 0)
	{
		tex = TexMan[spr.texture[0]];
		flip = false;
	}
	else
	{
		int rot = (rotangle-actor->angle-(ANGLE_90-ANGLE_45/2))/ANGLE_45;
		tex = TexMan[spr.texture[rot]];
		flip = (spr.mirror>>rot)&1;
	}
	return tex;
}

void R_InstallSprite(Sprite &frame, FTexture *tex, int dir, bool mirror)
{
	if(dir < -1 || dir >= 8)
	{
		printf("Invalid frame data for '%s'.\n", tex->Name);
		return;
	}

	if(dir == -1)
	{
		frame.rotations = 0;
		dir = 0;
	}
	else
		frame.rotations = 8;

	frame.texture[dir] = tex->GetID();
	if(mirror)
		frame.mirror |= 1<<dir;
}

unsigned int R_GetNumLoadedSprites()
{
	return loadedSprites.Size();
}

void R_GetSpriteHitlist(BYTE* hitlist)
{
	// Start by getting a list of currently in use sprites and then tell the
	// precacher to load them.

	BYTE* sprites = new BYTE[loadedSprites.Size()];
	memset(sprites, 0, loadedSprites.Size());

	for(AActor::Iterator iter = AActor::GetIterator();iter.Next();)
	{
		sprites[iter->state->spriteInf] = 1;
	}

	for(unsigned int i = loadedSprites.Size();i-- > NUM_SPECIAL_SPRITES;)
	{
		if(!sprites[i])
			continue;

		SpriteInfo &sprInf = loadedSprites[i];
		Sprite *frame = &spriteFrames[sprInf.frames];
		for(unsigned int j = sprInf.numFrames;j-- > 0;++frame)
		{
			if(frame->rotations == Sprite::NO_FRAMES)
				continue;

			for(unsigned int k = frame->rotations;k-- > 0;)
			{
				if(frame->texture[k].isValid())
					hitlist[frame->texture[k].GetIndex()] |= 1;
			}
		}
	}

	delete[] sprites;
}

int SpriteCompare(const void *s1, const void *s2)
{
	uint32_t n1 = static_cast<const SpriteInfo *>(s1)->iname;
	uint32_t n2 = static_cast<const SpriteInfo *>(s2)->iname;
	if(n1 < n2)
		return -1;
	else if(n1 > n2)
		return 1;
	return 0;
}

void R_InitSprites()
{
	static const uint8_t MAX_SPRITE_FRAMES = 29; // A-Z, [, \, ]

	// First sort the loaded sprites list
	qsort(&loadedSprites[NUM_SPECIAL_SPRITES], loadedSprites.Size()-NUM_SPECIAL_SPRITES, sizeof(loadedSprites[0]), SpriteCompare);

	typedef LinkedList<FTexture*> SpritesList;
	typedef TMap<uint32_t, SpritesList> SpritesMap;

	SpritesMap spritesMap;

	// Collect potential sprite list (linked list of sprites by name)
	for(unsigned int i = TexMan.NumTextures();i-- > 0;)
	{
		FTexture *tex = TexMan.ByIndex(i);
		if(tex->UseType == FTexture::TEX_Sprite && strlen(tex->Name) >= 6)
		{
			SpritesList &list = spritesMap[tex->dwName];
			list.Push(tex);
		}
	}

	// Now process the sprites if we need to load them
	for(unsigned int i = NUM_SPECIAL_SPRITES;i < loadedSprites.Size();++i)
	{
		SpritesList &list = spritesMap[loadedSprites[i].iname];
		if(list.Size() == 0)
			continue;
		loadedSprites[i].frames = spriteFrames.Size();

		Sprite frames[MAX_SPRITE_FRAMES];
		uint8_t maxframes = 0;
		for(unsigned int j = 0;j < MAX_SPRITE_FRAMES;++j)
		{
			frames[j].rotations = Sprite::NO_FRAMES;
			frames[j].mirror = 0;
		}

		for(SpritesList::Iterator iter = list.Head();iter;++iter)
		{
			FTexture *tex = iter;
			unsigned char frame = tex->Name[4] - 'A';
			if(frame < MAX_SPRITE_FRAMES)
			{
				if(frame > maxframes)
					maxframes = frame;
				R_InstallSprite(frames[frame], tex, tex->Name[5] - '1', false);

				if(strlen(tex->Name) == 8)
				{
					frame = tex->Name[6] - 'A';
					if(frame < MAX_SPRITE_FRAMES)
					{
						if(frame > maxframes)
							maxframes = frame;
						R_InstallSprite(frames[frame], tex, tex->Name[7] - '1', true);
					}
				}
			}
		}

		++maxframes;
		for(unsigned int j = 0;j < maxframes;++j)
		{
			// Check rotations
			if(frames[j].rotations == 8)
			{
				for(unsigned int r = 0;r < 8;++r)
				{
					if(!frames[j].texture[r].isValid())
					{
						printf("Sprite %s is missing rotations for frame %c.\n", loadedSprites[i].name, j);
						break;
					}
				}
			}

			spriteFrames.Push(frames[j]);
		}

		loadedSprites[i].numFrames = maxframes;
	}
}

void R_LoadSprite(const FString &name)
{
	if(loadedSprites.Size() == 0)
	{
		// Make sure the special sprites are loaded
		SpriteInfo sprInf;
		sprInf.frames = 0;
		strcpy(sprInf.name, "TNT1");
		loadedSprites.Push(sprInf);
	}

	if(name.Len() != 4)
	{
		printf("Sprite name invalid.\n");
		return;
	}

	static uint32_t lastSprite = 0;
	SpriteInfo sprInf;
	sprInf.frames = 0;
	sprInf.numFrames = 0;

	strcpy(sprInf.name, name.GetChars());
	if(loadedSprites.Size() > 0)
	{
		if(sprInf.iname == lastSprite)
			return;

		for(unsigned int i = 0;i < loadedSprites.Size();++i)
		{
			if(loadedSprites[i].iname == sprInf.iname)
			{
				sprInf = loadedSprites[i];
				lastSprite = sprInf.iname;
				return;
			}
		}
	}
	lastSprite = sprInf.iname;

	loadedSprites.Push(sprInf);
}

////////////////////////////////////////////////////////////////////////////////

// From wl_draw.cpp
int CalcRotate(AActor *ob);
extern byte* vbuf;
extern unsigned vbufPitch;
extern fixed viewshift;
extern fixed viewz;

void ScaleSprite(AActor *actor, int xcenter, const Frame *frame, unsigned height)
{
	if(actor->sprite == SPR_NONE || loadedSprites[actor->sprite].numFrames == 0)
		return;

	bool flip = false;
	const Sprite &spr = spriteFrames[loadedSprites[actor->sprite].frames+frame->frame];
	FTexture *tex;
	if(spr.rotations == 0)
		tex = TexMan[spr.texture[0]];
	else
	{
		int rot = (CalcRotate(actor)+4)%8;
		tex = TexMan[spr.texture[rot]];
		flip = (spr.mirror>>rot)&1;
	}
	if(tex == NULL)
		return;

	const int scale = height>>3; // Integer part of the height
	const int topoffset = (scale*(viewz-(32<<FRACBITS))/(32<<FRACBITS));
	if(scale == 0 || -(viewheight/2 - viewshift - topoffset) >= scale)
		return;

	const double dyScale = (height/256.0)*(actor->scaleY/65536.);
	const int upperedge = static_cast<int>((viewheight/2 - viewshift - topoffset)+scale - tex->GetScaledTopOffsetDouble()*dyScale);

	const double dxScale = (height/256.0)*(FixedDiv(actor->scaleX, yaspect)/65536.);
	const int actx = static_cast<int>(xcenter - tex->GetScaledLeftOffsetDouble()*dxScale);

	const unsigned int texWidth = tex->GetWidth();
	const unsigned int startX = -MIN(actx, 0);
	const unsigned int startY = -MIN(upperedge, 0);
	const fixed xStep = static_cast<fixed>(tex->xScale/dxScale);
	const fixed yStep = static_cast<fixed>(tex->yScale/dyScale);
	const fixed xRun = MIN<fixed>(texWidth<<FRACBITS, xStep*(viewwidth-actx));
	const fixed yRun = MIN<fixed>(tex->GetHeight()<<FRACBITS, yStep*(viewheight-upperedge));

	const BYTE *colormap;
	if((actor->flags & FL_BRIGHT) || frame->fullbright)
		colormap = NormalLight.Maps;
	else
	{
		const int shade = LIGHT2SHADE(gLevelLight + r_extralight);
		const int tz = FixedMul(r_depthvisibility<<8, height);
		colormap = &NormalLight.Maps[GETPALOOKUP(MAX(tz, MINZ), shade)<<8];
	}
	const BYTE *src;
	byte *destBase = vbuf + actx + startX + (upperedge > 0 ? vbufPitch*upperedge : 0);
	byte *dest = destBase;
	unsigned int i;
	fixed x, y;
	for(i = actx+startX, x = startX*xStep;x < xRun;x += xStep, ++i, dest = ++destBase)
	{
		if(wallheight[i] > (signed)height)
			continue;

		src = tex->GetColumn(flip ? texWidth - (x>>FRACBITS) - 1 : (x>>FRACBITS), NULL);

		for(y = startY*yStep;y < yRun;y += yStep)
		{
			if(src[y>>FRACBITS])
				*dest = colormap[src[y>>FRACBITS]];
			dest += vbufPitch;
		}
	}
}

void R_DrawPlayerSprite(AActor *actor, const Frame *frame, fixed offsetX, fixed offsetY)
{
	if(frame->spriteInf == SPR_NONE || loadedSprites[frame->spriteInf].numFrames == 0)
		return;

	const Sprite &spr = spriteFrames[loadedSprites[frame->spriteInf].frames+frame->frame];
	FTexture *tex;
	if(spr.rotations == 0)
		tex = TexMan[spr.texture[0]];
	else
		tex = TexMan[spr.texture[(CalcRotate(actor)+4)%8]];
	if(tex == NULL)
		return;

	const BYTE *colormap;
	if(frame->fullbright)
		colormap = NormalLight.Maps;
	else
	{
		const int shade = LIGHT2SHADE(gLevelLight) - (gLevelMaxLightVis/LIGHTVISIBILITY_FACTOR);
		colormap = &NormalLight.Maps[GETPALOOKUP(0, shade)<<8];
	}

	const fixed scale = viewheight<<(FRACBITS-1);

	const fixed centeringOffset = (centerx - 2*centerxwide)<<FRACBITS;
	const fixed leftedge = FixedMul((160<<FRACBITS) - fixed(tex->GetScaledLeftOffsetDouble()*FRACUNIT) + offsetX, pspritexscale) + centeringOffset;
	fixed upperedge = ((100-32)<<FRACBITS) + fixed(tex->GetScaledTopOffsetDouble()*FRACUNIT) - offsetY - AspectCorrection[r_ratio].tallscreen;
	if(viewsize == 21 && players[0].ReadyWeapon)
	{
		upperedge -= players[0].ReadyWeapon->yadjust;
	}
	upperedge = scale - FixedMul(upperedge, pspriteyscale);

	// startX and startY indicate where the sprite becomes visible, we only
	// need to calculate the start since the end will be determined when we hit
	// the view during drawing.
	const unsigned int startX = -MIN(leftedge>>FRACBITS, 0);
	const unsigned int startY = -MIN(upperedge>>FRACBITS, 0);
	const fixed xStep = FixedDiv(tex->xScale, pspritexscale);
	const fixed yStep = FixedDiv(tex->yScale, pspriteyscale);

	const int x1 = leftedge>>FRACBITS;
	const int y1 = upperedge>>FRACBITS;
	const fixed xRun = MIN<fixed>(tex->GetWidth()<<FRACBITS, xStep*(viewwidth-x1-startX));
	const fixed yRun = MIN<fixed>(tex->GetHeight()<<FRACBITS, yStep*(viewheight-y1));
	const BYTE *src;
	byte *destBase = vbuf+x1+startX + (y1 > 0 ? vbufPitch*y1 : 0);
	byte *dest = destBase;
	fixed x, y;
	for(x = startX*xStep;x < xRun;x += xStep)
	{
		src = tex->GetColumn(x>>FRACBITS, NULL);

		for(y = startY*yStep;y < yRun;y += yStep)
		{
			if(src[y>>FRACBITS] != 0)
				*dest = colormap[src[y>>FRACBITS]];
			dest += vbufPitch;
		}

		dest = ++destBase;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// S3DNA Zoomer
//

IMPLEMENT_INTERNAL_CLASS(SpriteZoomer)

SpriteZoomer::SpriteZoomer(FTextureID texID) : Thinker(ThinkerList::VICTORY), texID(texID), count(0)
{
}

void SpriteZoomer::Draw()
{
	FTexture *gmoverTex = TexMan(texID);

	// What we're trying to do is zoom in a 160x160 player sprite to
	// fill the viewheight.  S3DNA use the player sprite rendering
	// function and passed count as the height. We won't do it like that
	// since that method didn't account for the view window size
	// (vanilla could crash) and our player sprite renderer may take
	// into account things we would rather not have here.
	const double yscale = double(viewheight*count)/(160.*192.);
	const double xscale = yscale/FIXED2FLOAT(yaspect);

	screen->DrawTexture(gmoverTex, viewscreenx + (viewwidth>>1) - xscale*160, viewscreeny + (viewheight>>1) - yscale*80,
		DTA_DestWidthF, gmoverTex->GetScaledWidthDouble()*xscale,
		DTA_DestHeightF, gmoverTex->GetScaledHeightDouble()*yscale,
		TAG_DONE);
}

void SpriteZoomer::Tick()
{
	assert(count <= 192);
	if(++count > 192)
		Destroy();
}
