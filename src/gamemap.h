/*
** gamemap.h
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

#ifndef __GAMEMAP_H__
#define __GAMEMAP_H__

#include "tarray.h"
#include "zstring.h"
#include "textures/textures.h"
#include "dobject.h"

class Thinker;
class UWMFParser;

class GameMap
{
	public:
		struct Header
		{
			FString			name;
			unsigned int	width;
			unsigned int	height;
			unsigned int	tileSize;
		};
		struct Thing
		{
			Thing() : x(0), y(0), z(0), angle(0), type(0), ambush(false), 
				patrol(false)
			{
				skill[0] = skill[1] = skill[2] = skill[3] = false;
			}

			fixed			x, y, z;
			unsigned short	angle;
			unsigned int	type;
			bool			ambush;
			bool			patrol;
			bool			skill[4];
		};
		struct Trigger
		{
			Trigger() : x(0), y(0), z(0), active(true), action(0),
				playerUse(false), monsterUse(false), isSecret(false),
				repeatable(false)
			{
				activate[0] = activate[1] = activate[2] = activate[3] = true;
				arg[0] = arg[1] = arg[2] = arg[3] = arg[4] = arg[5] = 0;
			}

			unsigned int	x, y, z;
			// Stores if the trigger hasn't been used yet.
			// Note that this is set to false on for repeatable actions as well so that secrets are only accounted for once.
			bool			active;

			enum Side { East, North, West, South };
			unsigned int	action;
			bool			activate[4];
			int				arg[5];

			bool			playerUse;
			bool			monsterUse;
			bool			isSecret;
			bool			repeatable;
		};
		struct Tile
		{
			Tile() : offsetVertical(false), offsetHorizontal(false) {}

			enum Side { East, North, West, South };
			FTextureID		texture[4];
			bool			offsetVertical;
			bool			offsetHorizontal;
		};
		struct Sector
		{
			enum Flat { Floor, Ceiling };
			FTextureID	texture[2];
		};
		struct Zone
		{
			unsigned short	index;
		};
		struct Plane
		{
			const GameMap	*gm;

			unsigned int	depth;
			struct Map
			{
				Map() : tile(NULL), sector(NULL), zone(NULL), thinker(NULL),
					pushAmount(0), pushReceptor(NULL)
				{
					slideAmount[0] = slideAmount[1] = slideAmount[2] = slideAmount[3] = 0;
				}

				unsigned int	GetX() const;
				unsigned int	GetY() const;
				Map				*GetAdjacent(Tile::Side dir, bool opposite=false) const;

				const Plane		*plane;

				const Tile		*tile;
				const Sector	*sector;
				const Zone		*zone;

				bool			visible;
				TObjPtr<Thinker> thinker;
				unsigned int	slideAmount[4];
				TArray<Trigger>	triggers;
				Tile::Side		pushDirection;
				unsigned int	pushAmount;
				Map				*pushReceptor;
			}*	map;
		};

		GameMap(const FString &map);
		~GameMap();

		bool			ActivateTrigger(Trigger &trig, Trigger::Side direction, AActor *activator);
		void			ClearVisibility();
		const Header	&GetHeader() const { return header; }
		void			GetHitlist(BYTE* hitlist) const;
		Plane::Map		*GetSpot(unsigned int x, unsigned int y, unsigned int z) { return &GetPlane(z).map[y*header.width+x]; }
		bool			IsValid() const { return valid; }
		unsigned int	NumPlanes() const { return planes.Size(); }
		const Plane		&GetPlane(unsigned int index) const { return planes[index]; }
		void			SpawnThings() const;

		// Sound functions
		bool			CheckLink(const Zone *zone1, const Zone *zone2, bool recurse);
		void			LinkZones(const Zone *zone1, const Zone *zone2, bool open);

	private:
		friend class UWMFParser;

		Plane	&NewPlane();
		Trigger	&NewTrigger(unsigned int x, unsigned int y, unsigned int z);
		void	ReadPlanesData();
		void	ReadUWMFData();
		void	SetupLinks();
		void	UnloadLinks();

		FString	map;

		bool	valid;
		bool	isWad;
		int		markerLump;
		int		numLumps;

		class FResourceFile	*file;
		class FileReader	*lumps[1];

		// Actual map data
		Header			header;
		TArray<Tile>	tilePalette;
		TArray<Sector>	sectorPalette;
		TArray<Zone>	zonePalette;
		TArray<Thing>	things;
		TArray<Trigger>	triggers;
		TArray<Plane>	planes;

		unsigned short***	zoneLinks;
};

typedef GameMap::Plane::Map *	MapSpot;

// The following are mainly for easy access to enums
typedef GameMap::Plane			MapPlane;
typedef GameMap::Sector			MapSector;
typedef GameMap::Thing			MapThing;
typedef GameMap::Tile			MapTile;
typedef GameMap::Trigger		MapTrigger;
typedef GameMap::Zone			MapZone;

#endif
