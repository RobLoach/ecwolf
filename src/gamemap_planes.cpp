/*
** gamemap_planes.cpp
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

#include "id_ca.h"
#include "g_mapinfo.h"
#include "gamemap.h"
#include "gamemap_common.h"
#include "lnspec.h"
#include "scanner.h"
#include "w_wad.h"
#include "wl_game.h"

class Xlat : public TextMapParser
{
public:
	struct ThingXlat
	{
	public:
		static int SortCompare(const void *t1, const void *t2)
		{
			unsigned short old1 = static_cast<const ThingXlat *>(t1)->oldnum;
			unsigned short old2 = static_cast<const ThingXlat *>(t2)->oldnum;
			if(old1 < old2)
				return -1;
			else if(old1 > old2)
				return 1;
			return 0;
		}

		unsigned short	oldnum;
		bool			isTrigger;


		unsigned short	newnum;
		unsigned char	angles;
		bool			patrol;
		unsigned char	minskill;

		MapTrigger		templateTrigger;
	};

	Xlat() : lump(0)
	{
	}

	void LoadXlat(int lump)
	{
		if(this->lump == lump)
			return;
		this->lump = lump;

		FMemLump data = Wads.ReadLump(lump);
		Scanner sc((const char*)data.GetMem(), data.GetSize());
		sc.SetScriptIdentifier(Wads.GetLumpFullName(lump));

		while(sc.TokensLeft())
		{
			sc.MustGetToken(TK_Identifier);

			if(sc->str.CompareNoCase("things") == 0)
				LoadThingTable(sc);
			else if(sc->str.CompareNoCase("flats") == 0)
				LoadFlatsTable(sc);
			else
				sc.ScriptMessage(Scanner::ERROR, "Unknown xlat property '%s'.", sc->str.GetChars());
		}
	}

	FTextureID TranslateFlat(unsigned int index, bool ceiling)
	{
		if(flatTable[index][ceiling].isValid())
			return flatTable[index][ceiling];
		return levelInfo->DefaultTexture[ceiling];
	}

	bool TranslateThing(MapThing &thing, MapTrigger &trigger, bool &isTrigger, unsigned short oldnum) const
	{
		bool valid = false;
		unsigned int type = thingTable.Size()/2;
		unsigned int max = thingTable.Size()-1;
		unsigned int min = 0;
		do
		{
			if(thingTable[type].oldnum == oldnum ||
				(thingTable[type].angles && unsigned(oldnum - thingTable[type].oldnum) < thingTable[type].angles))
			{
				valid = true;
				break;
			}

			if(thingTable[type].oldnum > oldnum)
				max = type-1;
			else if(thingTable[type].oldnum < oldnum)
				min = type+1;

			type = (max+min)/2;
		}
		while(max >= min && max < thingTable.Size());

		if(valid)
		{
			isTrigger = thingTable[type].isTrigger;
			if(isTrigger)
			{
				trigger = thingTable[type].templateTrigger;
			}
			else
			{
				thing.type = thingTable[type].newnum;

				if(thingTable[type].angles)
					thing.angle = (oldnum - thingTable[type].oldnum)*(360/thingTable[type].angles);
				else
					thing.angle = 0;

				thing.patrol = thingTable[type].patrol;
				thing.skill[0] = thing.skill[1] = thingTable[type].minskill <= 1;
				thing.skill[2] = thingTable[type].minskill <= 2;
				thing.skill[3] = thingTable[type].minskill <= 3;
			}
		}

		return valid;
	}

protected:
	void LoadFlatsTable(Scanner &sc)
	{
		// Clear out old data
		for(unsigned int i = 0;i < 256;++i)
		{
			flatTable[i][0].SetInvalid();
			flatTable[i][1].SetInvalid();
		}

		sc.MustGetToken('{');
		while(!sc.CheckToken('}'))
		{
			sc.MustGetToken(TK_Identifier);
			bool ceiling = sc->str.CompareNoCase("ceiling") == 0;
			if(!ceiling && sc->str.CompareNoCase("floor") != 0)
				sc.ScriptMessage(Scanner::ERROR, "Unknown flat section '%s'.", sc->str.GetChars());

			sc.MustGetToken('{');
			unsigned int index = 0;
			do
			{
				sc.MustGetToken(TK_StringConst);
				FTextureID texID = TexMan.GetTexture(sc->str, FTexture::TEX_Flat);
				if(sc.CheckToken('='))
				{
					sc.MustGetToken(TK_IntConst);
					index = sc->number;
					if(index > 255)
						index = 255;
				}
				flatTable[index++][ceiling] = texID;

				if(index == 256)
					break;
			}
			while(sc.CheckToken(','));
			sc.MustGetToken('}');
		}
	}

	void LoadThingTable(Scanner &sc)
	{
		// Always start with an empty table
		thingTable.Clear();

		sc.MustGetToken('{');
		while(!sc.CheckToken('}'))
		{
			ThingXlat thing;

			// Property
			if(sc.CheckToken(TK_Identifier))
			{
				if(sc->str.CompareNoCase("trigger") == 0)
				{
					sc.MustGetToken(TK_IntConst);
					thing.isTrigger = true;
					thing.oldnum = sc->number;

					sc.MustGetToken('{');
					TextMapParser::ParseTrigger(sc, thing.templateTrigger);
				}
				else
					sc.ScriptMessage(Scanner::ERROR, "Unknown thing table block '%s'.", sc->str.GetChars());
			}
			else
			{
				// Handle thing translation
				thing.isTrigger = false;
				sc.MustGetToken('{');
				sc.MustGetToken(TK_IntConst);
				thing.oldnum = sc->number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				thing.newnum = sc->number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				thing.angles = sc->number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_BoolConst);
				thing.patrol = sc->boolean;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				thing.minskill = sc->number;
				sc.MustGetToken('}');
			}

			thingTable.Push(thing);
		}

		qsort(&thingTable[0], thingTable.Size(), sizeof(thingTable[0]), ThingXlat::SortCompare);
	}

private:
	int lump;

	WORD pushwall;
	WORD patrolpoint;
	TArray<ThingXlat> thingTable;
	FTextureID flatTable[256][2]; // Floor/ceiling textures
};

// Reads old format maps
void GameMap::ReadPlanesData()
{
	static Xlat xlat;
	static const unsigned short UNIT = 64;
	enum OldPlanes { Plane_Tiles, Plane_Object, Plane_Flats, NUM_USABLE_PLANES };

	xlat.LoadXlat(Wads.GetNumForFullName(levelInfo->Translator));

	valid = true;

	// Old format maps always have a tile size of 64
	header.tileSize = UNIT;

	FileReader *lump = lumps[0];

	// Read plane count
	lump->Seek(10, SEEK_SET);
	WORD numPlanes;
	lump->Read(&numPlanes, 2);
	numPlanes = LittleShort(numPlanes);

	lump->Seek(14, SEEK_SET);
	char name[17];
	lump->Read(name, 16);
	name[16] = 0;
	header.name = name;

	lump->Seek(30, SEEK_SET);
	WORD dimensions[2];
	lump->Read(dimensions, 4);
	dimensions[0] = LittleShort(dimensions[0]);
	dimensions[1] = LittleShort(dimensions[1]);
	DWORD size = dimensions[0]*dimensions[1];
	header.width = dimensions[0];
	header.height = dimensions[1];

	Plane &mapPlane = NewPlane();
	mapPlane.depth = UNIT;

	// We need to store the spots marked for ambush since it's stored in the
	// tiles plane instead of the objects plane.
	TArray<WORD> ambushSpots;

	for(int plane = 0;plane < numPlanes && plane < NUM_USABLE_PLANES;++plane)
	{
		WORD* oldplane = new WORD[size];
		lump->Read(oldplane, size*2);

		switch(plane)
		{
			default:
				break;

			case Plane_Tiles:
			{
				// Yay for magic numbers!
				static const WORD
					NUM_WALLS = 64,
					EXIT_TILE = 21,
					DOOR_START = 90,
					DOOR_END = DOOR_START + 6*2,
					AMBUSH_TILE = 106,
					ALT_EXIT = 107,
					ZONE_START = 108,
					ZONE_END = ZONE_START + 36;

				tilePalette.Resize(NUM_WALLS+(DOOR_END-DOOR_START));
				zonePalette.Resize(ZONE_END-ZONE_START);
				for(unsigned int i = 0;i < zonePalette.Size();++i)
					zonePalette[i].index = i;
				TArray<WORD> altExitSpots;

				// Import tiles
				for(unsigned int i = 0;i < NUM_WALLS;++i)
				{
					Tile &tile = tilePalette[i];
					tile.texture[Tile::North] = tile.texture[Tile::South] = TexMan.GetTile(i, false);
					tile.texture[Tile::East] = tile.texture[Tile::West] = TexMan.GetTile(i, true);
				}
				for(unsigned int i = 0;i < DOOR_END-DOOR_START;i++)
				{
					Tile &tile = tilePalette[i+NUM_WALLS];
					tile.offsetVertical = i%2 == 0;
					tile.offsetHorizontal = i%2 == 1;
					if(i%2 == 0)
					{
						tile.texture[Tile::North] = tile.texture[Tile::South] = TexMan.GetDoor(i/2, true, true);
						tile.texture[Tile::East] = tile.texture[Tile::West] = TexMan.GetDoor(i/2, true, false);
					}
					else
					{
						tile.texture[Tile::North] = tile.texture[Tile::South] = TexMan.GetDoor(i/2, false, false);
						tile.texture[Tile::East] = tile.texture[Tile::West] = TexMan.GetDoor(i/2, false, true);
					}
				}
				

				for(unsigned int i = 0;i < size;++i)
				{
					if(oldplane[i] < NUM_WALLS)
						mapPlane.map[i].SetTile(&tilePalette[oldplane[i]]);
					else if(oldplane[i] >= DOOR_START && oldplane[i] < DOOR_END)
					{
						const bool vertical = oldplane[i]%2 == 0;
						const unsigned int doorType = (oldplane[i]-DOOR_START)/2;
						Trigger &trig = NewTrigger(i%header.width, i/header.width, 0);
						trig.action = Specials::Door_Open;
						trig.arg[0] = 4;
						trig.arg[1] = doorType >= 1 && doorType <= 4 ? doorType : 0;
						trig.playerUse = true;
						trig.monsterUse = true;
						trig.repeatable = true;
						if(vertical)
							trig.activate[Trigger::North] = trig.activate[Trigger::South] = false;
						else
							trig.activate[Trigger::East] = trig.activate[Trigger::West] = false;

						mapPlane.map[i].SetTile(&tilePalette[oldplane[i]-DOOR_START+NUM_WALLS]);
					}
					else
						mapPlane.map[i].SetTile(NULL);

					// We'll be moving the ambush flag to the actor itself.
					if(oldplane[i] == AMBUSH_TILE)
						ambushSpots.Push(i);
					// For the alt exit we need to change the triggers after we
					// are done with this pass.
					else if(oldplane[i] == ALT_EXIT)
						altExitSpots.Push(i);
					else if(oldplane[i] == EXIT_TILE)
					{
						// Add exit trigger
						Trigger &trig = NewTrigger(i%header.width, i/header.width, 0);
						trig.action = Specials::Exit_Normal;
						trig.activate[Trigger::North] = trig.activate[Trigger::South] = false;
						trig.playerUse = true;
					}

					if(oldplane[i] >= ZONE_START && oldplane[i] < ZONE_END)
						mapPlane.map[i].zone = &zonePalette[oldplane[i]-ZONE_START];
					else
						mapPlane.map[i].zone = NULL;
				}

				// Get a sound zone for ambush spots if possible
				for(unsigned int i = 0;i < ambushSpots.Size();++i)
				{
					const int candidates[4] = {
						ambushSpots[i] + 1,
						ambushSpots[i] - header.width,
						ambushSpots[i] - 1,
						ambushSpots[i] + header.width
					};
					for(unsigned int j = 0;j < 4;++j)
					{
						// Ensure that all candidates are valid locations.
						// In addition for moving left/right check to see that
						// the new location is indeed in the same row.
						if((candidates[j] < 0 || (unsigned)candidates[j] > size) ||
							((j == Tile::East || j == Tile::West) &&
							(candidates[j]/header.width != ambushSpots[i]/header.width)))
							continue;

						// First adjacent zone wins
						if(mapPlane.map[candidates[j]].zone != NULL)
						{
							mapPlane.map[ambushSpots[i]].zone = mapPlane.map[candidates[j]].zone;
							break;
						}
					}
				}

				for(unsigned int i = 0;i < altExitSpots.Size();++i)
				{
					// Look for and switch exit triggers.
					const int candidates[4] = {
						altExitSpots[i] + 1,
						altExitSpots[i] - header.width,
						altExitSpots[i] - 1,
						altExitSpots[i] + header.width
					};
					for(unsigned int j = 0;j < 4;++j)
					{
						// Same as before
						if((candidates[j] < 0 || (unsigned)candidates[j] > size) ||
							((j == Trigger::East || j == Trigger::West) &&
							(candidates[j]/header.width != altExitSpots[i]/header.width)))
							continue;

						if(oldplane[candidates[j]] == EXIT_TILE)
						{
							TArray<Trigger> &triggers = mapPlane.map[candidates[j]].triggers;
							// Look for any triggers matching the candidate
							for(unsigned int k = 0;k < triggers.Size();++k)
							{
								if((triggers[k].x == (unsigned)candidates[j]%header.width) &&
									(triggers[k].y == (unsigned)candidates[j]/header.width) &&
									(triggers[k].action == Specials::Exit_Normal))
									triggers[k].action = Specials::Exit_Secret;
							}
						}
					}
				}
				break;
			}

			case Plane_Object:
			{
				unsigned int ambushSpot = 0;
				ambushSpots.Push(0xFFFF); // Prevent uninitialized value errors. 

				for(unsigned int i = 0;i < size;++i)
				{
					if(oldplane[i] == 0)
					{
						// In case of malformed maps we need to always check this.
						if(ambushSpots[ambushSpot] == i)
							++ambushSpot;
						continue;
					}

					Thing thing;
					Trigger trigger;
					bool isTrigger;

					if(!xlat.TranslateThing(thing, trigger, isTrigger, oldplane[i]))
						printf("Unknown old type %d @ (%d,%d)\n", oldplane[i], i%header.width, i/header.width);
					else
					{
						if(isTrigger)
						{
							trigger.x = i%header.width;
							trigger.y = i/header.width;
							trigger.z = 0;

							Trigger &mapTrigger = NewTrigger(trigger.x, trigger.y, trigger.z);
							mapTrigger = trigger;
						}
						else
						{
							thing.x = ((i%header.width)<<FRACBITS)+(FRACUNIT/2);
							thing.y = ((i/header.width)<<FRACBITS)+(FRACUNIT/2);
							thing.z = 0;
							thing.ambush = ambushSpots[ambushSpot] == i;
							things.Push(thing);
						}
					}

					if(ambushSpots[ambushSpot] == i)
						++ambushSpot;
				}
				break;
			}

			case Plane_Flats:
			{
				// Look for all unique floor/ceiling texture combinations.
				WORD type = 0;
				TMap<WORD, WORD> flatMap;
				for(unsigned int i = 0;i < size;++i)
				{
					if(!flatMap.CheckKey(oldplane[i]))
						flatMap[oldplane[i]] = type++;
				}

				// Build the palette.
				sectorPalette.Resize(type);
				TMap<WORD, WORD>::ConstIterator iter(flatMap);
				TMap<WORD, WORD>::ConstPair *pair;
				while(iter.NextPair(pair))
				{
					Sector &sect = sectorPalette[pair->Value];
					sect.texture[Sector::Floor] = xlat.TranslateFlat(pair->Key&0xFF, Sector::Floor);
					sect.texture[Sector::Ceiling] = xlat.TranslateFlat(pair->Key>>8, Sector::Ceiling);
				}

				// Now link the sector data to map points!
				for(unsigned int i = 0;i < size;++i)
					mapPlane.map[i].sector = &sectorPalette[flatMap[oldplane[i]]];
				break;
			}
		}
		delete[] oldplane;
	}
	SetupLinks();
}
