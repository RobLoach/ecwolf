#ifndef __WL_AGENT__
#define __WL_AGENT__

#include "wl_def.h"
#include "a_playerpawn.h"
#include "weaponslots.h"

/*
=============================================================================

							WL_AGENT DEFINITIONS

=============================================================================
*/

extern  int      facecount;
extern  int32_t  thrustspeed;
extern  AActor   *LastAttacker;

void    Cmd_Use ();
void    Thrust (angle_t angle, int32_t speed);
void    SpawnPlayer (int tilex, int tiley, int dir);
void    TakeDamage (int points,AActor *attacker);
void    GivePoints (int32_t points);

//
// player state info
//

void	DrawStatusBar();
void    StatusDrawFace(unsigned picnum);
void    GiveExtraMan (int amount);
void    UpdateFace (bool damageUpdate=false);
void	WeaponGrin ();
void    CheckWeaponChange ();
void    ControlMovement (AActor *self);

////////////////////////////////////////////////////////////////////////////////

class AWeapon;

#define WP_NOCHANGE ((AWeapon*)~0)

extern class player_t
{
	public:
		player_t();

		void	BringUpWeapon();
		size_t	PropagateMark();
		void	Reborn();
		void	Serialize(FArchive &arc);
		void	SetPSprite(const Frame *frame);

		enum State
		{
			PST_ENTER,
			PST_DEAD,
			PST_REBORN,
			PST_LIVE
		};

		enum PlayerFlags
		{
			PF_WEAPONREADY = 0x1
		};

		APlayerPawn	*mo;
		TObjPtr<AActor>	camera;
		TObjPtr<AActor>	killerobj;

		int32_t		oldscore,score,nextextra;
		short		lives;
		int32_t		health;

		FWeaponSlots	weapons;
		AWeapon			*ReadyWeapon;
		AWeapon			*PendingWeapon;
		struct
		{
			const Frame	*frame;
			short		ticcount;
			fixed		sx;
			fixed		sy;
		} psprite;

		// Attackheld is similar to buttonheld[bt_attack] only it only gets set
		// to true when an attack is registered. If the button is released and
		// then pressed again it should initiate a second attack even if
		// NOAUTOFIRE is set.
		bool		attackheld;

		int32_t		flags;
		State		state;
} players[];

FArchive &operator<< (FArchive &arc, player_t *&player);

#endif
