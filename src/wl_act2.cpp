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

	if(maxturn > 0 && maxturn < self->angle - iangle)
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

//==========================================================================
//
// P_AproxDistance
//
// Gives an estimation of distance (not exact)
// From Doom.
// 
//==========================================================================

fixed P_AproxDistance (fixed dx, fixed dy)
{
	dx = abs(dx);
	dy = abs(dy);
	return (dx < dy) ? dx+dy-(dx>>1) : dx+dy-(dy>>1);
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

static FRandom pr_explodemissile("ExplodeMissile");
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

		if((self->flags & FL_RANDOMIZE) && self->ticcount > 0)
		{
			self->ticcount -= pr_explodemissile() & 7;
			if(self->ticcount < 1)
				self->ticcount = 1;
		}
	}
	else
		self->Destroy();
}

void T_Projectile (AActor *self)
{
	int steps = 1;
	fixed movex = self->velx;
	fixed movey = self->vely;

	// Projectiles can't move faster than their radius in a tic or collision
	// detection can be off.
	{
		fixed maxmove = self->radius - FRACUNIT/64;
		if(maxmove <= 0) // Really small projectile? Prevent problems with division
			maxmove = FRACUNIT/2;

		fixed vel = MAX(abs(movex), abs(movey));
		if(vel > maxmove)
			steps = 1 + vel / maxmove;

		movex /= steps;
		movey /= steps;
	}

	do
	{
		self->x += movex;
		self->y += movey;

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
				if(check != players[0].mo && (check->flags & FL_SHOOTABLE))
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
	while(--steps);
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

bool CheckMeleeRange(AActor *inflictor, AActor *inflictee, fixed range)
{
	fixed r = inflictor->meleerange + inflictee->radius + range;
	return abs(inflictee->x - inflictor->x) <= r && abs(inflictee->y - inflictor->y) <= r;
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
		CHF_NOSIGHTCHECK = 4,
		CHF_NOPLAYACTIVE = 8
	};

	ACTION_PARAM_STATE(melee, 0, self->MeleeState);
	ACTION_PARAM_STATE(missile, 1, self->MissileState);
	ACTION_PARAM_INT(flags, 2);

	int32_t	move,target;
	int		dx,dy,dist = INT_MAX,chance;
	bool	dodge = !(flags & CHF_DONTDODGE);
	bool	pathing = (self->flags & FL_PATHING) ? true : false;

	if (self->dir == nodir)
	{
		if (pathing)
			SelectPathDir (self);
		else if (dodge)
			SelectDodgeDir (self);
		else
			SelectChaseDir (self);

		self->movecount = pr_chase.RandomOld(false) & 15;
	}
	// Movecount is an approximation of Doom's movecount which would keep the
	// monster moving in some direction for a random amount of time (contrarily
	// to wolfensteins block based movement). This is simulated since it also
	// determines when a monster attempts to attack
	else if(--self->movecount < 0)
		self->movecount = pr_chase.RandomOld(false) & 15;

	if(!pathing)
	{
		bool inMeleeRange = melee ? CheckMeleeRange(self, players[0].mo, self->speed) : false;

		if(!inMeleeRange && missile)
		{
			dodge = false;
			if (((self->flags & FL_ALWAYSFAST) || self->movecount == 0) && CheckLine(self)) // got a shot at players[0].mo?
			{
				self->hidden = false;
				dx = abs(self->tilex + dirdeltax[self->dir] - players[0].mo->tilex);
				dy = abs(self->tiley + dirdeltay[self->dir] - players[0].mo->tiley);
				dist = dx>dy ? dx : dy;
				// If we only do ranged attacks, be more aggressive
				if(!melee)
				{
					if(self->missilefrequency >= FRACUNIT)
						dist -= 2;
					else
						// For frequencies less than 1.0 scale back the boost
						// in aggressiveness. Through the magic that is integer
						// math, this will become 0 at wolfensteins's frequency
						// This allows us to approximate Doom's aggressiveness
						// while not tampering the Wolf probability
						dist -= (2*self->missilefrequency)>>FRACBITS;
				}

				if(!(flags & CHF_BACKOFF))
				{
					if (dist > 0)
						chance = (208*self->missilefrequency/dist)>>FRACBITS;
					else
						chance = 256;

					// If we have a combo attack monster, we want to skip this
					// check as the monster should try to get melee in.
					if (dist == 1 && !melee)
					{
						target = abs(self->x - players[0].mo->x);
						if (target < 0x14000l) //  < 1.25 tiles or 80 units
						{
							target = abs(self->y - players[0].mo->y);
							if (target < 0x14000l)
								chance = 256;
						}
					}
				}
				else
					chance = (208*self->missilefrequency)>>FRACBITS;

				if ( pr_chase.RandomOld(self->flags & FL_OLDRANDOMCHASE) < MAX<int>(chance, self->minmissilechance))
				{
					self->SetState(missile);
					return;
				}
				dodge = !(flags & CHF_DONTDODGE);
			}
			else
				self->hidden = true;
		}
		else
			self->hidden = !inMeleeRange;
	}
	else
	{
		if (!(flags & CHF_NOSIGHTCHECK) && SightPlayer (self, 0, 0, 0, 180))
			return;
	}

	if(self->dir == nodir)
		return; // object is blocked in

	self->angle = dirangle[self->dir];
	move = self->speed;

	do
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
			if(!MoveObj (self,move) && !(flags & CHF_DONTDODGE) && !(self->flags & FL_PATHING))
			{
				// Touched the player so turn around!
				self->dir = dirtype((self->dir+4)%8);
				self->distance = FRACUNIT-self->distance;
			}
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

		dx = abs(self->tilex - players[0].mo->tilex);
		dy = abs(self->tiley - players[0].mo->tiley);
		dist = dx>dy ? dx : dy;
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
	while(move);

	if(!(flags & CHF_NOPLAYACTIVE) &&
		self->activesound != NAME_None && pr_chase.RandomOld(false) < 3)
	{
		PlaySoundLocActor(self->activesound, self);
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
	ACTION_PARAM_FIXED(snipe, 2);
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

		dist = FixedMul(dist, snipe);
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
