// WL_ACT2.C

#include <stdio.h>
#include <climits>
#include <math.h>
#include "actor.h"
#include "m_random.h"
#include "wl_act.h"
#include "wl_def.h"
#include "wl_menu.h"
#include "id_ca.h"
#include "id_sd.h"
#include "id_vl.h"
#include "id_vh.h"
#include "id_us.h"
#include "language.h"
#include "thingdef/thingdef.h"
#include "thingdef/thingdef_expression.h"
#include "wl_agent.h"
#include "wl_draw.h"
#include "wl_game.h"
#include "wl_state.h"

static const angle_t dirangle[9] = {0,ANGLE_45,2*ANGLE_45,3*ANGLE_45,4*ANGLE_45,
					5*ANGLE_45,6*ANGLE_45,7*ANGLE_45,0};

static inline bool CheckDoorMovement(AActor *actor)
{
	MapTile::Side direction;
	switch(actor->dir)
	{
		default:
			return false;
		case north:
			direction = MapTile::South;
			break;
		case south:
			direction = MapTile::North;
			break;
		case east:
			direction = MapTile::West;
			break;
		case west:
			direction = MapTile::East;
			break;
	}

	if(actor->distance < 0) // Waiting for door?
	{
		MapSpot spot = map->GetSpot(actor->tilex, actor->tiley, 0);
		spot = spot->GetAdjacent(direction, true);
		if(spot->slideAmount[direction] != 0xffff)
			return true;
		// Door is open, go on
		actor->distance = TILEGLOBAL;
		TryWalk(actor);
	}
	return false;
}

void A_Face(AActor *self, AActor *target, angle_t maxturn)
{
	double angle = atan2 ((double) (target->x - self->x), (double) (target->y - self->y));
	if (angle<0)
		angle = (M_PI*2+angle);
	angle_t iangle = (angle_t) (angle*ANGLE_180/M_PI) - ANGLE_90;

	if(maxturn > 0 && maxturn < abs(self->angle - iangle))
	{
		if(self->angle > iangle)
		{
			if(self->angle - iangle < ANGLE_180)
				self->angle -= maxturn;
			else
				self->angle += maxturn;
		}
		else
		{
			if(iangle - self->angle < ANGLE_180)
				self->angle += maxturn;
			else
				self->angle -= maxturn;
		}
	}
	else
		self->angle = iangle;
}

/*
=============================================================================

							LOCAL CONSTANTS

=============================================================================
*/

#define BJRUNSPEED      2048
#define BJJUMPSPEED     680

/*
=============================================================================

							LOCAL VARIABLES

=============================================================================
*/


dirtype dirtable[9] = {northwest,north,northeast,west,nodir,east,
	southwest,south,southeast};

/*
===================
=
= ProjectileTryMove
=
= returns true if move ok
===================
*/

bool ProjectileTryMove (AActor *ob)
{
	int xl,yl,xh,yh,x,y;
	MapSpot check;

	xl = (ob->x-ob->radius) >> TILESHIFT;
	yl = (ob->y-ob->radius) >> TILESHIFT;

	xh = (ob->x+ob->radius) >> TILESHIFT;
	yh = (ob->y+ob->radius) >> TILESHIFT;

	//
	// check for solid walls
	//
	for (y=yl;y<=yh;y++)
		for (x=xl;x<=xh;x++)
		{
			const bool checkLines[4] =
			{
				(ob->x+ob->radius) > ((x+1)<<TILESHIFT),
				(ob->y-ob->radius) < (y<<TILESHIFT),
				(ob->x-ob->radius) < (x<<TILESHIFT),
				(ob->y+ob->radius) > ((y+1)<<TILESHIFT)
			};
			check = map->GetSpot(x, y, 0);
			if (check->tile)
			{
				for(unsigned short i = 0;i < 4;++i)
				{
					if(check->slideAmount[i] != 0xFFFF && checkLines[i])
						return false;
				}
			}
		}

	return true;
}

/*
=================
=
= T_Projectile
=
=================
*/

void T_ExplodeProjectile(AActor *self, AActor *target)
{
	PlaySoundLocActor(self->deathsound, self);

	const Frame *deathstate = NULL;
	if(target && (target->flags & FL_SHOOTABLE)) // Fleshy!
		deathstate = self->FindState(NAME_XDeath);
	if(!deathstate)
		deathstate = self->FindState(NAME_Death);

	if(deathstate)
	{
		self->flags &= ~FL_MISSILE;
		self->SetState(deathstate);
	}
	else
		self->Destroy();
}

void T_Projectile (AActor *self)
{
	self->x += self->velx;
	self->y += self->vely;

	if (!ProjectileTryMove (self))
	{
		T_ExplodeProjectile(self, NULL);
		return;
	}

	if(!(self->flags & FL_PLAYERMISSILE))
	{
		fixed deltax = LABS(self->x - players[0].mo->x);
		fixed deltay = LABS(self->y - players[0].mo->y);
		fixed radius = players[0].mo->radius + self->radius;
		if (deltax < radius && deltay < radius)
		{
			TakeDamage (self->GetDamage(),self);
			T_ExplodeProjectile(self, players[0].mo);
			return;
		}
	}
	else
	{
		AActor::Iterator *iter = AActor::GetIterator();
		while(iter)
		{
			AActor *check = iter->Item();
			if((check->flags & FL_SHOOTABLE))
			{
				fixed deltax = LABS(self->x - check->x);
				fixed deltay = LABS(self->y - check->y);
				fixed radius = check->radius + self->radius;
				if(deltax < radius && deltay < radius)
				{
					DamageActor(check, self->GetDamage());
					T_ExplodeProjectile(self, check);
					return;
				}
			}
			iter = iter->Next();
		}
	}
}

/*
==================
=
= A_Scream
=
==================
*/

ACTION_FUNCTION(A_Scream)
{
	PlaySoundLocActor(self->deathsound, self);
}

/*
==================
=
= A_CustomMissile
=
==================
*/

ACTION_FUNCTION(A_CustomMissile)
{
	enum
	{
		CMF_AIMOFFSET = 1
	};

	ACTION_PARAM_STRING(missiletype, 0);
	ACTION_PARAM_DOUBLE(spawnheight, 1);
	ACTION_PARAM_INT(spawnoffset, 2);
	ACTION_PARAM_DOUBLE(angleOffset, 3);
	ACTION_PARAM_INT(flags, 4);

	fixed newx = self->x + spawnoffset*finesine[self->angle>>ANGLETOFINESHIFT]/64;
	fixed newy = self->y + spawnoffset*finecosine[self->angle>>ANGLETOFINESHIFT]/64;

	double angle = (flags & CMF_AIMOFFSET) ?
		atan2 ((double) (self->y - players[0].mo->y), (double) (players[0].mo->x - self->x)) :
		atan2 ((double) (newy - players[0].mo->y), (double) (players[0].mo->x - newx));
	if (angle<0)
		angle = (M_PI*2+angle);
	angle_t iangle = (angle_t) (angle*ANGLE_180/M_PI) + (angle_t) ((angleOffset*ANGLE_45)/45);

	const ClassDef *cls = ClassDef::FindClass(missiletype);
	if(!cls)
		return;
	AActor *newobj = AActor::Spawn(cls, newx, newy, 0, true);
	newobj->angle = iangle;

	newobj->velx = FixedMul(newobj->speed,finecosine[iangle>>ANGLETOFINESHIFT]);
	newobj->vely = -FixedMul(newobj->speed,finesine[iangle>>ANGLETOFINESHIFT]);
}

//
// spectre
//


/*
===============
=
= A_Dormant
=
===============
*/

ACTION_FUNCTION(A_Dormant)
{
	AActor::Iterator *iter = AActor::GetIterator();
	AActor *actor;
	while(iter)
	{
		actor = iter->Item();
		iter = iter->Next();
		if(actor == self || !(actor->flags&(FL_SHOOTABLE|FL_SOLID)))
			continue;

		fixed radius = self->radius + actor->radius;
		if(abs(self->x - actor->x) < radius &&
			abs(self->y - actor->y) < radius)
			return;
	}

	self->flags |= FL_AMBUSH | FL_SHOOTABLE | FL_SOLID;
	self->flags &= ~FL_ATTACKMODE;
	self->dir = nodir;
	self->SetState(self->SeeState);
}

/*
============================================================================

STAND

============================================================================
*/


/*
===============
=
= A_Look
=
===============
*/

ACTION_FUNCTION(A_Look)
{
	ACTION_PARAM_INT(flags, 0);
	ACTION_PARAM_DOUBLE(minseedist, 1);
	ACTION_PARAM_DOUBLE(maxseedist, 2);
	ACTION_PARAM_DOUBLE(maxheardist, 3);
	ACTION_PARAM_DOUBLE(fov, 4);

	// FOV of 0 indicates default
	if(fov < 0.00001)
		fov = 180;

	SightPlayer(self, minseedist, maxseedist, maxheardist, fov);
}
// Create A_LookEx as an alias to A_Look since we're technically emulating this
// ZDoom function with A_Look.
ACTION_ALIAS(A_Look, A_LookEx)


/*
============================================================================

CHASE

============================================================================
*/

/*
=================
=
= T_Chase
=
=================
*/

bool CheckMeleeRange(AActor *actor1, AActor *actor2, fixed range)
{
	fixed r = actor1->radius + actor2->radius + range;
	return abs(actor2->x - actor1->x) <= r && abs(actor2->y - actor1->y) <= r;
}

/*
===============
=
= SelectPathDir
=
===============
*/

void SelectPathDir (AActor *ob)
{
	ob->distance = TILEGLOBAL;

	if (!TryWalk (ob))
		ob->dir = nodir;
}

FRandom pr_chase("Chase");
ACTION_FUNCTION(A_Chase)
{
	enum
	{
		CHF_DONTDODGE = 1,
		CHF_BACKOFF = 2,
		CHF_NOSIGHTCHECK = 4
	};

	ACTION_PARAM_STATE(melee, 0, self->MeleeState);
	ACTION_PARAM_STATE(missile, 1, self->MissileState);
	ACTION_PARAM_INT(flags, 2);

	int32_t	move,target;
	int		dx,dy,dist = INT_MAX,chance;
	bool	dodge = !(flags & CHF_DONTDODGE);
	bool	pathing = (self->flags & FL_PATHING) ? true : false;

	if(!pathing)
	{
		if(missile)
		{
			dodge = false;
			if (CheckLine(self)) // got a shot at players[0].mo?
			{
				self->hidden = false;
				dx = abs(self->tilex - players[0].mo->tilex);
				dy = abs(self->tiley - players[0].mo->tiley);
				dist = dx>dy ? dx : dy;

				if(!(flags & CHF_BACKOFF))
				{
					if (dist)
						chance = self->missilechance/dist;
					else
						chance = 300;

					if (dist == 1)
					{
						target = abs(self->x - players[0].mo->x);
						if (target < 0x14000l)
						{
							target = abs(self->y - players[0].mo->y);
							if (target < 0x14000l)
								chance = 300;
						}
					}
				}
				else
					chance = self->missilechance;

				if ( pr_chase()<chance)
				{
					if(missile)
						self->SetState(missile);
					return;
				}
				dodge = !(flags & CHF_DONTDODGE);
			}
			else
				self->hidden = true;
		}
		else
			self->hidden = !CheckMeleeRange(self, players[0].mo, self->speed);
	}
	else
	{
		if (!(flags & CHF_NOSIGHTCHECK) && SightPlayer (self, 0, 0, 0, 180))
			return;
	}

	if (self->dir == nodir)
	{
		if (pathing)
			SelectPathDir (self);
		else if (dodge)
			SelectDodgeDir (self);
		else
			SelectChaseDir (self);
		if (self->dir == nodir)
			return; // object is blocked in
	}

	self->angle = dirangle[self->dir];
	move = self->speed;

	while (move)
	{
		if (CheckDoorMovement(self))
			return;

		if(!pathing)
		{
			//
			// check for melee range
			//
			if(melee && CheckMeleeRange(self, players[0].mo, self->speed))
			{
				PlaySoundLocActor(self->attacksound, self);
				self->SetState(melee);
				return;
			}
		}

		if (move < self->distance)
		{
			MoveObj (self,move);
			break;
		}

		//
		// reached goal tile, so select another one
		//

		//
		// fix position to account for round off during moving
		//
		self->x = ((int32_t)self->tilex<<TILESHIFT)+TILEGLOBAL/2;
		self->y = ((int32_t)self->tiley<<TILESHIFT)+TILEGLOBAL/2;

		move -= self->distance;

		if (pathing)
			SelectPathDir (self);
		else if ((flags & CHF_BACKOFF) && dist < 4)
			SelectRunDir (self);
		else if (dodge)
			SelectDodgeDir (self);
		else
			SelectChaseDir (self);

		if (self->dir == nodir)
			return; // object is blocked in
	}
}

/*
=============================================================================

									FIGHT

=============================================================================
*/


/*
===============
=
= A_WolfAttack
=
= Try to damage the players[0].mo, based on skill level and players[0].mo's speed
=
===============
*/

static FRandom pr_cabullet("CustomBullet");
ACTION_FUNCTION(A_WolfAttack)
{
	enum
	{
		WAF_NORANDOM = 1
	};

	ACTION_PARAM_INT(flags, 0);
	ACTION_PARAM_STRING(sound, 1);
	ACTION_PARAM_DOUBLE(snipe, 2);
	ACTION_PARAM_INT(maxdamage, 3);
	ACTION_PARAM_INT(blocksize, 4);
	ACTION_PARAM_INT(pointblank, 5);
	ACTION_PARAM_INT(longrange, 6);
	ACTION_PARAM_DOUBLE(runspeed, 7);

	int     dx,dy,dist;
	int     hitchance;

	runspeed *= 37.5;

	A_Face(self, players[0].mo);

	if (CheckLine (self))                    // players[0].mo is not behind a wall
	{
		dx = abs(self->x - players[0].mo->x);
		dy = abs(self->y - players[0].mo->y);
		dist = dx>dy ? dx:dy;

		dist = FixedMul(dist, snipe*FRACUNIT);
		dist /= blocksize<<9;

		if (thrustspeed >= runspeed)
		{
			if (self->flags&FL_VISABLE)
				hitchance = 160-dist*16;                // players[0].mo can see to dodge
			else
				hitchance = 160-dist*8;
		}
		else
		{
			if (self->flags&FL_VISABLE)
				hitchance = 256-dist*16;                // players[0].mo can see to dodge
			else
				hitchance = 256-dist*8;
		}

		// see if the shot was a hit

		if (pr_cabullet()<hitchance)
		{
			int damage = flags & WAF_NORANDOM ? maxdamage : (1 + (pr_cabullet()%maxdamage));
			if (dist>=pointblank)
				damage >>= 1;
			if (dist>=longrange)
				damage >>= 1;

			TakeDamage (damage,self);
		}
	}

	if(sound.Len() == 1 && sound[0] == '*')
		PlaySoundLocActor(self->attacksound, self);
	else
		PlaySoundLocActor(sound, self);
}
