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
//
// teamplay_gamerules.cpp
//
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"
#include	"skill.h"
#include	"items.h"
#include	"game.h"

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgScoreInfo;
extern int gmsgMOTD;
extern int gmsgObjective;

//=========================================================
//=========================================================
CHalfLifeRules::CHalfLifeRules( void )
{
	RefreshSkillData();
}

//=========================================================
//=========================================================
void CHalfLifeRules::Think ( void )
{
	g_pGameRules->MutatorsThink();
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules::IsMultiplayer( void )
{
	return FALSE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules::IsDeathmatch ( void )
{
	return FALSE;
}

BOOL CHalfLifeRules::IsRoundBased( void )
{
	return FALSE;
}

BOOL CHalfLifeRules::AllowMeleeDrop( void )
{
	return FALSE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules::IsCoOp( void )
{
	return FALSE;
}

float CHalfLifeRules::WeaponMultipler( void )
{
	if (g_pGameRules->MutatorEnabled(MUTATOR_FASTWEAPONS))
		return 0.33;

	if (g_pGameRules->MutatorEnabled(MUTATOR_SLOWWEAPONS))
		return 6;

	return 1;
}

BOOL CHalfLifeRules::AllowRuneSpawn( const char *szRune )
{
	return FALSE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	if ( !pPlayer->m_pActiveItem )
	{
		// player doesn't have an active item!
		return TRUE;
	}

	if ( !pPlayer->m_pActiveItem->CanHolster() )
	{
		return FALSE;
	}

	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules :: GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon, BOOL dropBox )
{
	return FALSE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules :: ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] )
{
	return TRUE;
}

void CHalfLifeRules :: InitHUD( CBasePlayer *pl )
{
	if (!FBitSet(pl->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pl->edict());
			WRITE_STRING("");
			WRITE_STRING("");
			WRITE_BYTE(0);
			WRITE_STRING("");
		MESSAGE_END();
	}
}

void CHalfLifeRules :: UpdateGameMode( CBasePlayer *pl )
{
}

//=========================================================
//=========================================================
void CHalfLifeRules :: ClientDisconnected( edict_t *pClient )
{
}

//=========================================================
//=========================================================
float CHalfLifeRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	// subtract off the speed at which a player is allowed to fall without being hurt,
	// so damage will be based on speed beyond that, not the entire fall
	pPlayer->m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
	return pPlayer->m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
}

//=========================================================
//=========================================================
void CHalfLifeRules :: PlayerSpawn( CBasePlayer *pPlayer )
{
	pPlayer->GiveNamedItem("weapon_fists");

	// Elevator unstick
	if (!stricmp(STRING(gpGlobals->mapname), "c1a2"))
	{
		edict_t *pentTarget = NULL;

		while ((pentTarget = FIND_ENTITY_BY_TARGETNAME(pentTarget, "elestartmm")) != NULL)
		{
			if (FNullEnt(pentTarget))
				break;

			CBaseEntity *pTarget = CBaseEntity::Instance(pentTarget);

			if (pTarget)
				pTarget->Use(pPlayer->m_hActivator, pPlayer, USE_TOGGLE, 0);
		}
	}

	g_pGameRules->SpawnMutators(pPlayer);
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules :: AllowAutoTargetCrosshair( void )
{
	return ( g_iSkillLevel == SKILL_EASY );
}

//=========================================================
//=========================================================
void CHalfLifeRules :: PlayerThink( CBasePlayer *pPlayer )
{
	g_pGameRules->UpdateMutatorMessage(pPlayer);
}


//=========================================================
//=========================================================
BOOL CHalfLifeRules :: FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	return TRUE;
}

//=========================================================
//=========================================================
float CHalfLifeRules :: FlPlayerSpawnTime( CBasePlayer *pPlayer )
{
	return gpGlobals->time;//now!
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
int CHalfLifeRules :: IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return 1;
}

//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
void CHalfLifeRules :: PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
}

void CHalfLifeRules :: MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller )
{
	if (m_iNotTheBees && strcmp("hornet", STRING(pVictim->pev->classname)) != 0)
	{
		CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );
		BOOL isPlayer = FALSE;
		if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
			isPlayer = TRUE;
		Vector angles;

		if (isPlayer) 
			angles = UTIL_VecToAngles((pKiller->origin + pKiller->view_ofs ) - pVictim->pev->origin);

		CBaseEntity *hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16, isPlayer ? angles : gpGlobals->v_up, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				//hornet->pev->velocity.z += 100;
			}
			else
				hornet->pev->velocity = gpGlobals->v_up * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * 8, isPlayer ? angles : gpGlobals->v_up, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				//hornet->pev->velocity.z += 100;
			}
			else
				hornet->pev->velocity = gpGlobals->v_up * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16 + gpGlobals->v_forward * 24, isPlayer ? angles : gpGlobals->v_forward, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				//hornet->pev->velocity.z += 100;
			}
			else
				hornet->pev->velocity = gpGlobals->v_forward * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16 + gpGlobals->v_forward * -24, isPlayer ? angles : gpGlobals->v_forward * -1, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				//hornet->pev->velocity.z += 100;
			}
			else
				hornet->pev->velocity = (gpGlobals->v_forward * -1) * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16 + gpGlobals->v_right * 24, isPlayer ? angles : gpGlobals->v_right, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				//hornet->pev->velocity.z += 100;
			}
			else
				hornet->pev->velocity = gpGlobals->v_right * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16 + gpGlobals->v_right * -24, isPlayer ? angles : gpGlobals->v_right * -1, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				//hornet->pev->velocity.z += 100;
			}
			else
				hornet->pev->velocity = (gpGlobals->v_right * -1) * RANDOM_LONG(100, 200);
		}
	}

	if (m_iVolatile) {
		CGrenade::Vest( pVictim->pev, pVictim->pev->origin, gSkillData.plrDmgVest );
		pVictim->pev->solid = SOLID_NOT;
		pVictim->GibMonster();
		pVictim->pev->effects |= EF_NODRAW;
	}
}

//=========================================================
// Deathnotice
//=========================================================
void CHalfLifeRules::DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
void CHalfLifeRules :: PlayerGotWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHalfLifeRules :: FlWeaponRespawnTime( CBasePlayerItem *pWeapon )
{
	return -1;
}

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHalfLifeRules :: FlWeaponTryRespawn( CBasePlayerItem *pWeapon )
{
	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeRules :: VecWeaponRespawnSpot( CBasePlayerItem *pWeapon )
{
	return pWeapon->pev->origin;
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHalfLifeRules :: WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	return GR_WEAPON_RESPAWN_NO;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeRules::PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem )
{
}

//=========================================================
//=========================================================
int CHalfLifeRules::ItemShouldRespawn( CItem *pItem )
{
	return GR_ITEM_RESPAWN_NO;
}


//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHalfLifeRules::FlItemRespawnTime( CItem *pItem )
{
	return -1;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeRules::VecItemRespawnSpot( CItem *pItem )
{
	return pItem->pev->origin;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules::IsAllowedToSpawn( CBaseEntity *pEntity )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeRules::PlayerGotAmmo( CBasePlayer *pPlayer, char *szName, int iCount )
{
}

//=========================================================
//=========================================================
int CHalfLifeRules::AmmoShouldRespawn( CBasePlayerAmmo *pAmmo )
{
	return GR_AMMO_RESPAWN_NO;
}

//=========================================================
//=========================================================
float CHalfLifeRules::FlAmmoRespawnTime( CBasePlayerAmmo *pAmmo )
{
	return -1;
}

//=========================================================
//=========================================================
Vector CHalfLifeRules::VecAmmoRespawnSpot( CBasePlayerAmmo *pAmmo )
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
float CHalfLifeRules::FlHealthChargerRechargeTime( void )
{
	return 0;// don't recharge
}

//=========================================================
//=========================================================
int CHalfLifeRules::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_GUN_NO;
}

//=========================================================
//=========================================================
int CHalfLifeRules::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_AMMO_NO;
}

BOOL CHalfLifeRules::IsAllowedSingleWeapon( CBaseEntity *pEntity )
{
	return TRUE;
}

BOOL CHalfLifeRules::IsAllowedToDropWeapon( CBasePlayer *pPlayer )
{
	return FALSE;
}

BOOL CHalfLifeRules::IsAllowedToHolsterWeapon( void )
{
	return holsterweapons.value;
}

//=========================================================
//=========================================================
int CHalfLifeRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	// why would a single player in half life need this? 
	return GR_NOTTEAMMATE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeRules :: FAllowMonsters( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
#if defined( GRAPPLING_HOOK )
BOOL CHalfLifeRules :: AllowGrapplingHook( CBasePlayer *pPlayer )
{
	return FALSE;
}
#endif

BOOL CHalfLifeRules::MutatorAllowed(const char *mutator)
{
	if (strstr(mutator, g_szMutators[MUTATOR_MAXPACK - 1]) || atoi(mutator) == MUTATOR_MAXPACK)
		return FALSE;
	
	return TRUE;
}
