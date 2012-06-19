/*
** gamemap.cpp
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

#include "gamemap.h"
#include "tarray.h"
#include "w_wad.h"
#include "wl_def.h"
#include "lnspec.h"
#include "actor.h"
#include "thingdef/thingdef.h"
#include "wl_agent.h"
#include "wl_game.h"
#include "r_sprites.h"
#include "resourcefiles/resourcefile.h"

GameMap::GameMap(const FString &map) : map(map), valid(false), zoneLinks(NULL), file(NULL)
{
	lumps[0] = NULL;

	// Find the map
	markerLump = Wads.CheckNumForName(map);

	// PK3 format maps
	FString mapWad;
	mapWad.Format("maps/%s.wad", map.GetChars());

	int wadLump = Wads.CheckNumForFullName(mapWad);
	if(wadLump > markerLump)
	{
		isWad = true;
		markerLump = wadLump;
	}
	else
		isWad = false;

	if(markerLump == -1)
	{
		Quit("Could not find map %s!", map.GetChars());
		return;
	}

	// Hmm... What follows is some massive copy and paste, but I can't really
	// think of a cleaner way to do this.
	// Anyways, if we have a wad we need to open a resource file for it.
	// Otherwise we open the relevent lumps.
	if(isWad)
	{
		file = FResourceFile::OpenResourceFile(mapWad.GetChars(), Wads.ReopenLumpNum(markerLump), true);
		if(!file || file->LumpCount() < 2) // Maps must be 2 lumps in size
			return;

		// First lump is assumed marker
		FResourceLump *lump = file->GetLump(1);
		if(stricmp(lump->Name, "PLANES") == 0)
		{
			numLumps = 1;
			lumps[0] = lump->NewReader();
			ReadPlanesData();
		}
		else
		{
			if(stricmp(lump->Name, "TEXTMAP") != 0)
			{
				Quit("Invalid map format for %s!", map.GetChars());
				return;
			}
			else
			{
				lumps[0] = lump->NewReader();
			}

			for(unsigned int i = 2;i < file->LumpCount();++i)
			{
				lump = file->GetLump(i);
				if(stricmp(lump->Name, "ENDMAP") == 0)
				{
					valid = true;
					break;
				}
				numLumps++;
			}
			if(!valid)
			{
				Quit("ENDMAP not found for map %s!", map.GetChars());
				return;
			}
			ReadUWMFData();
		}
	}
	else
	{
		if(strcmp(Wads.GetLumpFullName(markerLump+1), "PLANES") == 0)
		{
			numLumps = 1;
			lumps[0] = Wads.ReopenLumpNum(markerLump+1);
			ReadPlanesData();
		}
		else
		{
			// Expect UWMF formatted map.
			if(strcmp(Wads.GetLumpFullName(markerLump+1), "TEXTMAP") != 0)
			{
				Quit("Invalid map format for %s!", map.GetChars());
				return;
			}
			else
			{
				lumps[0] = Wads.ReopenLumpNum(markerLump+1);
			}

			for(int i = 2;i < Wads.GetNumLumps();i++)
			{
				if(strcmp(Wads.GetLumpFullName(markerLump+i), "ENDMAP") == 0)
				{
					valid = true;
					break;
				}
				numLumps++;
			}
			if(!valid)
			{
				Quit("ENDMAP not found for map %s!", map.GetChars());
				return;
			}
			ReadUWMFData();
		}
	}
}

GameMap::~GameMap()
{
	if(isWad)
		delete file;
	delete lumps[0];

	for(unsigned int i = 0;i < planes.Size();++i)
		delete[] planes[i].map;
	UnloadLinks();
}

bool GameMap::ActivateTrigger(Trigger &trig, Trigger::Side direction, AActor *activator)
{
	if(!trig.repeatable && !trig.active)
		return false;

	MapSpot spot = GetSpot(trig.x, trig.y, trig.z);

	Specials::LineSpecialFunction func = Specials::LookupFunction(Specials::LineSpecials(trig.action));
	bool ret = func(spot, trig.arg, direction, activator);
	if(ret)
	{
		if(trig.active && trig.isSecret)
			++gamestate.secretcount;
		trig.active = false;
	}
	return ret;
}

void GameMap::ClearVisibility()
{
	for(unsigned int i = 0;i < header.width*header.height;++i)
	{
		for(unsigned int p = 0;p < planes.Size();++p)
			planes[p].map[i].visible = false;
	}
	if(players[0].mo)
		GetSpot(players[0].mo->tilex, players[0].mo->tiley, 0)->visible = true;
}

bool GameMap::CheckLink(const Zone *zone1, const Zone *zone2, bool recurse)
{
	if(zone1 == NULL || zone2 == NULL)
		return false;

	// If we're doing a recursive check and the straight check passes use that
	bool straightCheck = *zoneLinks[zone1->index][zone2->index] > 0;
	if(!recurse || straightCheck)
		return straightCheck;

	// If doing a recursive check we need to make zone 1 the lower number that
	// way we can check the top half of the table.
	if(zone2->index < zone1->index)
	{
		const Zone *tmp = zone1;
		zone1 = zone2;
		zone2 = tmp;
	}

	for(unsigned int i = zone1->index+1;i < zonePalette.Size();++i)
	{
		if(*zoneLinks[zone1->index][i] > 0 &&
			(i == zone2->index || CheckLink(&zonePalette[i], zone2, true)))
				return true;
	}
	return false;
}

// Get a list of textures to precache
void GameMap::GetHitlist(BYTE* hitlist) const
{
	R_GetSpriteHitlist(hitlist);

	for(unsigned int i = planes.Size();i-- > 0;)
	{
		Plane &plane = planes[i];
		for(unsigned int j = GetHeader().width*GetHeader().height;j-- > 0;)
		{
			Plane::Map &spot = plane.map[j];

			if(spot.tile)
			{
				hitlist[spot.tile->texture[Tile::East].GetIndex()] =
					hitlist[spot.tile->texture[Tile::North].GetIndex()] =
					hitlist[spot.tile->texture[Tile::West].GetIndex()] =
					hitlist[spot.tile->texture[Tile::South].GetIndex()] |= 1;
			}

			if(spot.sector)
			{
				hitlist[spot.sector->texture[Sector::Floor].GetIndex()] =
					hitlist[spot.sector->texture[Sector::Ceiling].GetIndex()] |= 2;
			}
		}
	}
}

void GameMap::LinkZones(const Zone *zone1, const Zone *zone2, bool open)
{
	if(zone1 == zone2 || zone1 == NULL || zone2 == NULL)
		return;

	unsigned short &value = *zoneLinks[zone1->index][zone2->index];
	if(!open)
	{
		if(value > 0)
			--value;
	}
	else
		++value;
}

GameMap::Plane &GameMap::NewPlane()
{
	planes.Reserve(1);
	Plane &newPlane = planes[planes.Size()-1];
	newPlane.gm = this;
	newPlane.map = new Plane::Map[header.width*header.height];
	for(unsigned int i = 0;i < header.width*header.height;++i)
		newPlane.map[i].plane = &newPlane;
	return newPlane;
}

GameMap::Trigger &GameMap::NewTrigger(unsigned int x, unsigned int y, unsigned int z)
{
	MapSpot spot = GetSpot(x, y, z);
	Trigger newTrig;
	newTrig.x = x;
	newTrig.y = y;
	newTrig.z = z;
	spot->triggers.Push(newTrig);
	return spot->triggers[spot->triggers.Size()-1];
}

void GameMap::SetupLinks()
{
	// Might as well use the same pointer for each time x == y
	unsigned short *one = new unsigned short;
	*one = 1;

	// Set up the table
	zoneLinks = new unsigned short**[zonePalette.Size()];
	for(unsigned int i = 0;i < zonePalette.Size();++i)
	{
		zoneLinks[i] = new unsigned short*[zonePalette.Size()];
		zoneLinks[i][i] = one;
		for(unsigned int j = i+1;j < zonePalette.Size();++j)
		{
			zoneLinks[i][j] = new unsigned short;
			*zoneLinks[i][j] = (i == j);
		}
	}

	// Second half should just point to the first set.
	for(unsigned int i = 1;i < zonePalette.Size();++i)
	{
		for(unsigned int j = 0;j < i;++j)
		{
			zoneLinks[i][j] = zoneLinks[j][i];
		}
	}
}

void GameMap::SpawnThings() const
{
	printf("Spawning %d things\n", things.Size());
	for(unsigned int i = 0;i < things.Size();++i)
	{
		Thing &thing = things[i];
		if(!thing.skill[gamestate.difficulty])
			continue;

		if(thing.type == 1)
			SpawnPlayer(thing.x>>FRACBITS, thing.y>>FRACBITS, thing.angle);
		else
		{
			// Spawn object
			const ClassDef *cls = ClassDef::FindClass(thing.type);
			if(cls == NULL)
			{
				printf("Unknown thing %d @ (%d, %d)\n", thing.type, thing.x>>FRACBITS, thing.y>>FRACBITS);
				continue;
			}

			AActor *actor = AActor::Spawn(cls, thing.x, thing.y, 0);
			// This forumla helps us to avoid errors in roundoffs.
			actor->angle = (thing.angle/45)*ANGLE_45 + (thing.angle%45)*ANGLE_1;
			actor->dir = dirtype(thing.angle/45);
			if(thing.ambush)
				actor->flags |= FL_AMBUSH;
			if(thing.patrol)
			{
				actor->flags |= FL_PATHING;
				if(actor->PathState)
					actor->SetState(actor->PathState, true);
			}

			// Check for valid frames
			if(!actor->state || !R_CheckSpriteValid(actor->sprite))
			{
				printf("%s at (%d, %d) has no frames\n", cls->GetName().GetChars(), thing.x>>FRACBITS, thing.y>>FRACBITS);
			}
		}
	}
}

void GameMap::UnloadLinks()
{
	// Make sure there's stuff to unload.
	if(!zoneLinks)
		return;

	delete zoneLinks[0][0];
	for(unsigned int i = 0;i < zonePalette.Size();++i)
	{
		for(unsigned int j = i+1;j < zonePalette.Size();++j)
			delete zoneLinks[i][j];
		delete[] zoneLinks[i];
	}
	delete[] zoneLinks;
	zoneLinks = NULL;
}

////////////////////////////////////////////////////////////////////////////////

unsigned int GameMap::Plane::Map::GetX() const
{
	return static_cast<unsigned int>(this - plane->map)%plane->gm->GetHeader().width;
}

unsigned int GameMap::Plane::Map::GetY() const
{
	return static_cast<unsigned int>(this - plane->map)/plane->gm->GetHeader().width;
}

MapSpot GameMap::Plane::Map::GetAdjacent(MapTile::Side dir, bool opposite) const
{
	if(opposite) // Rotate the dir 180 degrees.
		dir = MapTile::Side((dir+2)%4);

	const int pos = static_cast<int>(this - plane->map);
	int x = GetX();
	int y = GetY();
	switch(dir)
	{
		case MapTile::South:
			++y;
			break;
		case MapTile::North:
			--y;
			break;
		case MapTile::West:
			--x;
			break;
		case MapTile::East:
			++x;
			break;
	}
	if(y < 0 || y >= plane->gm->GetHeader().height || x < 0 || x >= plane->gm->GetHeader().width)
		return NULL;
	return &plane->map[y*plane->gm->GetHeader().width+x];
}
