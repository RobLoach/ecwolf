/*
** a_playerpawn.cpp
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

#include "a_inventory.h"
#include "a_playerpawn.h"
#include "thingdef/thingdef.h"
#include "wl_agent.h"
#include "wl_game.h"
#include "wl_play.h"

#include <climits>

IMPLEMENT_CLASS(PlayerPawn)

PointerIndexTable<AActor::DropList> APlayerPawn::startInventory;

AWeapon *APlayerPawn::BestWeapon(const ClassDef *ammo)
{
	AWeapon *best = NULL;
	int order = INT_MAX;

	for(AInventory *item = inventory;item != NULL;item = item->inventory)
	{
		if(!item->IsKindOf(NATIVE_CLASS(Weapon)))
			continue;

		const int thisOrder = item->GetClass()->Meta.GetMetaInt(AWMETA_SelectionOrder);
		if(thisOrder > order)
			continue;

		AWeapon *weapon = static_cast<AWeapon *>(item);
		if(ammo && weapon->ammo1->GetClass() != ammo)
			continue;
		if(!weapon->CheckAmmo(AWeapon::PrimaryFire, false))
			continue;

		order = thisOrder;
		best = weapon;
	}

	return best;
}

void APlayerPawn::CheckWeaponSwitch(const ClassDef *ammo)
{
	if(player->PendingWeapon != WP_NOCHANGE)
		return;

	AWeapon *weapon = BestWeapon(ammo);
	if(!weapon)
		return;

	const int selectionOrder = weapon->GetClass()->Meta.GetMetaInt(AWMETA_SelectionOrder);
	const int currentOrder = player->ReadyWeapon ? player->ReadyWeapon->GetClass()->Meta.GetMetaInt(AWMETA_SelectionOrder) : 0;
	if(selectionOrder < currentOrder)
		player->PendingWeapon = weapon;
}

void APlayerPawn::Die()
{
	if(player->ReadyWeapon)
		player->SetPSprite(player->ReadyWeapon->GetDownState());
}

AActor::DropList *APlayerPawn::GetStartInventory()
{
	int index = GetClass()->Meta.GetMetaInt(APMETA_StartInventory);
	if(index >= 0)
		return startInventory[index];
	return NULL;
}

void APlayerPawn::GiveStartingInventory()
{
	if(!GetStartInventory())
		return;

	DropList::Node *item = GetStartInventory()->Head();
	do
	{
		DropItem &inv = item->Item();
		const ClassDef *cls = ClassDef::FindClass(inv.className);
		if(!cls || !cls->IsDescendantOf(NATIVE_CLASS(Inventory)))
			continue;

		AInventory *invItem = (AInventory *)AActor::Spawn(cls, 0, 0, 0, false);
		invItem->RemoveFromWorld();
		invItem->amount = inv.amount;
		if(cls->IsDescendantOf(NATIVE_CLASS(Weapon)))
		{
			player->PendingWeapon = (AWeapon *)invItem;

			// Empty weapon.
			((AWeapon *)invItem)->ammogive1 = 0;
		}
		if(!invItem->TryPickup(this))
			invItem->Destroy();
	}
	while((item = item->Next()));

	SetupWeaponSlots();

	// Bring up weapon
	player->BringUpWeapon();

#if 0
	AInventory *inv = inventory;
	while(inv)
	{
		Printf("%s %d/%d\n", inv->GetClass()->GetName().GetChars(), inv->amount, inv->maxamount);
		inv = inv->inventory;
	}
#endif
}

AWeapon *APlayerPawn::PickNewWeapon()
{
	AWeapon *best = BestWeapon();

	if(best)
	{
		player->PendingWeapon = best;
		player->SetPSprite(player->ReadyWeapon->GetDownState());
	}

	return best;
}

void APlayerPawn::Serialize(FArchive &arc)
{
	arc << maxhealth;

	Super::Serialize(arc);
}

void APlayerPawn::SetupWeaponSlots()
{
	players[0].weapons.StandardSetup(GetClass());
}

void APlayerPawn::Tick()
{
	Super::Tick();

	TickPSprites();

	// Watching BJ
	if(gamestate.victoryflag)
		return;

	UpdateFace();
	CheckWeaponChange();

	if(buttonstate[bt_use])
		Cmd_Use();

	if((player->flags & player_t::PF_WEAPONREADY))
	{
		if(buttonstate[bt_attack] && player->ReadyWeapon->CheckAmmo(AWeapon::PrimaryFire, true))
		{
			if(!buttonheld[bt_attack])
				player->attackheld = false;
			if(!(player->ReadyWeapon->weaponFlags & WF_NOAUTOFIRE) || !player->attackheld)
			{
				player->attackheld = true;
				player->SetPSprite(player->ReadyWeapon->GetAtkState(false));
			}
		}
		else if(player->PendingWeapon != WP_NOCHANGE)
		{
			player->SetPSprite(player->ReadyWeapon->GetDownState());
		}
	}
	else if(player->attackheld)
		player->attackheld = buttonstate[bt_attack];

	ControlMovement(this);

	// [RH] Smooth transitions between bobbing and not-bobbing frames.
	// This also fixes the bug where you can "stick" a weapon off-center by
	// shooting it when it's at the peak of its swing.
	static fixed curbob = 0;
	const fixed movebob = GetClass()->Meta.GetMetaFixed(APMETA_MoveBob);

	if(movebob)
	{
		static const fixed MAXBOB = 0x100000;
		fixed bobtarget = FixedMul(thrustspeed << 8, movebob);
		if(bobtarget > MAXBOB)
			bobtarget = MAXBOB;

		if (curbob != bobtarget)
		{
			if (abs (bobtarget - curbob) <= 1*FRACUNIT)
			{
				curbob = bobtarget;
			}
			else
			{
				fixed_t zoom = MAX<fixed_t> (1*FRACUNIT, abs (curbob - bobtarget) / 40);
				if (curbob > bobtarget)
				{
					curbob -= zoom;
				}
				else
				{
					curbob += zoom;
				}
			}
		}
	}
	else
		curbob = 0;

	player->bob = curbob;
}

void APlayerPawn::TickPSprites()
{
	if(!player->psprite.frame)
		return;

	if(player->psprite.ticcount > 0)
		--player->psprite.ticcount;

	while(player->psprite.frame && player->psprite.ticcount == 0)
		player->SetPSprite(player->psprite.frame->next);

	if(player->psprite.frame)
		player->psprite.frame->thinker(this);
}
