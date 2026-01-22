/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "instagib_gamerules.h"
#include "game.h"
#include "items.h"

extern int gmsgObjective;

class CTombstone : public CBaseEntity
{
public:
	void Precache ( void );
	void Spawn( void );
	void EXPORT TombstoneThink( void );
	virtual int ObjectCaps( void ) { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_PORTAL; }

private:
	float m_flTimeToLive;
};

LINK_ENTITY_TO_CLASS( monster_tombstone, CTombstone );

void CTombstone::Precache( void )
{

}

void CTombstone::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = 0.5;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	pev->health = 40;
	pev->takedamage = DAMAGE_NO;
	pev->classname = MAKE_STRING("monster_tombstone");

	SET_MODEL( edict(), "models/w_items.mdl");
	pev->body = 9;
	pev->sequence = 10;

	UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);

	m_flTimeToLive = gpGlobals->time + 20.0;

	SetThink(&CTombstone::TombstoneThink);
	pev->nextthink = gpGlobals->time + 0.1;
}

void CTombstone::TombstoneThink( void )
{
	if (pev->flags & FL_ONGROUND)
	{
		pev->effects &= ~EF_NODRAW;
		pev->sequence = 11;
		pev->framerate = 1.0;
		pev->animtime = gpGlobals->time;

		SetThink(&CBaseEntity::SUB_StartFadeOut);
		pev->nextthink = gpGlobals->time + 3.0;
		return;
	}

	// In case we never hit ground
	if (m_flTimeToLive < gpGlobals->time)
	{
		SetThink(&CBaseEntity::SUB_Remove);
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	pev->nextthink = gpGlobals->time + 0.25;
}

CHalfLifeInstagib::CHalfLifeInstagib()
{

}

void CHalfLifeInstagib::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Instagib 'em");
			WRITE_STRING("");
			WRITE_BYTE(0);
		MESSAGE_END();
	}
}

void CHalfLifeInstagib::PlayerThink( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerThink(pPlayer);

	if ( flUpdateTime > gpGlobals->time )
		return;

	typedef struct {
		int	clientID = -1;
		int frags = -1;
	} frag_map_t;

	frag_map_t frags[32 + 1];
	int totalPlayers = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		frag_map_t x;
		x.clientID = i;

		if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
		{
			x.frags = plr->pev->frags;
			totalPlayers++;
		}

		frags[i] = x;
	}

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		for (int j = 1; j <= gpGlobals->maxClients - 1; j++)
		{
			if (frags[j].frags < frags[j + 1].frags)
			{
				frag_map_t temp = frags[j];
				frags[j] = frags[j + 1];
				frags[j + 1] = temp;
			}
		}
	}

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		if (frags[i].clientID == pPlayer->entindex())
		{
			if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
			{
				MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer->edict());
					WRITE_STRING("Instagib 'em");
					WRITE_STRING(UTIL_VarArgs("Rank: %d / %d", i, totalPlayers));
					WRITE_BYTE(0);
					int myfrags = frags[i].frags;
					// Calculate spread: difference between your frags and the position above you
					if ( i > 1 ) {
						// There's someone above you, show how far behind/ahead you are
						WRITE_STRING(UTIL_VarArgs("Spread: %+d", (myfrags - frags[i - 1].frags)));
					}
					else {
						// You're in first place, show how far ahead you are from second place
						if (totalPlayers > 1) {
							WRITE_STRING(UTIL_VarArgs("Spread: %+d", (myfrags - frags[i + 1].frags)));
						} else {
							WRITE_STRING("Spread: --");
						}
					}
				MESSAGE_END();
			}
		}
	}

	flUpdateTime = gpGlobals->time + 3.0;
}

BOOL CHalfLifeInstagib::MutatorAllowed(const char *mutator)
{
	if (strstr(mutator, g_szMutators[MUTATOR_999 - 1]) || atoi(mutator) == MUTATOR_999)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_BERSERKER - 1]) || atoi(mutator) == MUTATOR_BERSERKER)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_INSTAGIB - 1]) || atoi(mutator) == MUTATOR_INSTAGIB)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_MAXPACK - 1]) || atoi(mutator) == MUTATOR_MAXPACK)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_NORELOAD - 1]) || atoi(mutator) == MUTATOR_NORELOAD)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_PLUMBER - 1]) || atoi(mutator) == MUTATOR_PLUMBER)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_BUSTERS - 1]) || atoi(mutator) == MUTATOR_BUSTERS)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_COWBOY - 1]) || atoi(mutator) == MUTATOR_COWBOY)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_FIRESTARTER - 1]) || atoi(mutator) == MUTATOR_FIRESTARTER)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_PORTAL - 1]) || atoi(mutator) == MUTATOR_PORTAL)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_RAILGUNS - 1]) || atoi(mutator) == MUTATOR_RAILGUNS)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_RANDOMWEAPON - 1]) || atoi(mutator) == MUTATOR_RANDOMWEAPON)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_ROCKETCROWBAR - 1]) || atoi(mutator) == MUTATOR_ROCKETCROWBAR)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_SLOWBULLETS - 1]) || atoi(mutator) == MUTATOR_SLOWBULLETS)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_VESTED - 1]) || atoi(mutator) == MUTATOR_VESTED)
		return FALSE;

	return CHalfLifeMultiplay::MutatorAllowed(mutator);
}

BOOL CHalfLifeInstagib::IsAllowedToSpawn( CBaseEntity *pEntity )
{
	if (CHalfLifeMultiplay::IsAllowedToSpawn(pEntity))
	{
		if (strcmp(STRING(pEntity->pev->classname), "weapon_hornetgun") == 0)
			return TRUE;

		if (strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0 ||
			strncmp(STRING(pEntity->pev->classname), "ammo_", 5) == 0)
		{
			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

void CHalfLifeInstagib::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);

	pPlayer->GiveNamedItem("weapon_zapgun");
}

void CHalfLifeInstagib::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );

	CBaseEntity::Create( "monster_tombstone", Vector(pVictim->pev->origin.x, pVictim->pev->origin.y, pVictim->pev->origin.z), Vector(pKiller->angles.x, pKiller->angles.y, 0), NULL );
}

int CHalfLifeInstagib::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_GUN_NO;
}

int CHalfLifeInstagib::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_AMMO_NO;
}

BOOL CHalfLifeInstagib::IsAllowedToDropWeapon( CBasePlayer *pPlayer )
{
	return FALSE;
}

BOOL CHalfLifeInstagib::AllowRuneSpawn( const char *szRune )
{
	// Filter out runes that don't make sense with instant-kill gameplay
	if (!strcmp("rune_strength", szRune))
		return FALSE;	// Damage boost is redundant with one-shot kills
	
	if (!strcmp("rune_protect", szRune))
		return FALSE;	// Damage reduction conflicts with one-shot kills
	
	if (!strcmp("rune_vampire", szRune))
		return FALSE;	// Can't drain health from instant kills
	
	if (!strcmp("rune_regen", szRune))
		return FALSE;	// Health regen is pointless when you die in one hit

	return TRUE;
}