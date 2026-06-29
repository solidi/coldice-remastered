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
#include "busters_gamerules.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"
#include "trains.h"

#define EGON_BUSTING_TIME 10

extern int gmsgObjective;
extern int gmsgTeamNames;
extern int gmsgTeamInfo;
extern int gmsgScoreInfo;
extern int gmsgPlayClientSound;
extern int gmsgBanner;
extern int gmsgStatusIcon;

extern DLL_GLOBAL BOOL g_fGameOver;

bool IsPlayerBusting( CBaseEntity* pPlayer )
{
	if ( !pPlayer || !pPlayer->IsPlayer() || !g_pGameRules->IsBusters() )
		return FALSE;

	return ( (CBasePlayer*)pPlayer )->HasPlayerItemFromID( WEAPON_EGON );
}

BOOL BustingCanHaveItem( CBasePlayer* pPlayer, CBaseEntity* pItem )
{
	BOOL bIsWeaponOrAmmo = FALSE;

	if ( strstr( STRING( pItem->pev->classname ), "weapon_" ) || strstr( STRING( pItem->pev->classname ), "ammo_" ) )
	{
		bIsWeaponOrAmmo = TRUE;
	}

	//Busting players can't have ammo nor weapons
	if ( IsPlayerBusting( pPlayer ) && bIsWeaponOrAmmo )
		return FALSE;

	return TRUE;
}

static int BustersConfiguredCount( void )
{
	int configured = (int)busterscount.value;
	if (configured < 1)
		configured = 1;
	else if (configured > 3)
		configured = 3;

	return configured;
}

static int BustersTargetCount( int playerCount )
{
	// Keep at least a 2-ghosts-to-1-buster ratio.
	if (playerCount < 3)
		return 0;

	int maxByRatio = playerCount / 3;
	int target = BustersConfiguredCount();
	if (target > maxByRatio)
		target = maxByRatio;

	return target;
}

static int BustersCountHolders( CBasePlayer* holders[], int maxHolders )
{
	int holderCount = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsPlayer() || pPlayer->HasDisconnected )
			continue;

		if ( !IsPlayerBusting( pPlayer ) )
			continue;

		if ( holders && holderCount < maxHolders )
			holders[holderCount] = pPlayer;

		holderCount++;
	}

	return holderCount;
}

static int BustersCountLooseEgons( void )
{
	int looseCount = 0;
	CBaseEntity* pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "weaponbox" ) ) != NULL )
	{
		CWeaponBox* pWeaponBox = (CWeaponBox*)pEntity;
		if ( !pWeaponBox )
			continue;

		BOOL hasEgon = FALSE;
		for ( int i = 0; i < MAX_ITEM_TYPES && !hasEgon; i++ )
		{
			CBasePlayerItem* pWeapon = pWeaponBox->m_rgpPlayerItems[i];
			while ( pWeapon )
			{
				if ( pWeapon->m_iId == WEAPON_EGON )
				{
					hasEgon = TRUE;
					break;
				}

				pWeapon = pWeapon->m_pNext;
			}
		}

		if ( hasEgon )
			looseCount++;
	}

	return looseCount;
}

static BOOL BustersRemoveOneLooseEgon( void )
{
	CBaseEntity* pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "weaponbox" ) ) != NULL )
	{
		CWeaponBox* pWeaponBox = (CWeaponBox*)pEntity;
		if ( !pWeaponBox )
			continue;

		for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			CBasePlayerItem* pWeapon = pWeaponBox->m_rgpPlayerItems[i];
			while ( pWeapon )
			{
				if ( pWeapon->m_iId == WEAPON_EGON )
				{
					pWeaponBox->Kill();
					return TRUE;
				}

				pWeapon = pWeapon->m_pNext;
			}
		}
	}

	return FALSE;
}

static BOOL BustersDemoteOneHolder( CMultiplayBusters* pRules )
{
	if ( !pRules )
		return FALSE;

	CBasePlayer* holders[32];
	int holderCount = BustersCountHolders( holders, 32 );
	if ( holderCount < 1 )
		return FALSE;

	CBasePlayer* pRemove = holders[RANDOM_LONG( 0, holderCount - 1 )];
	if ( !pRemove )
		return FALSE;

	pRemove->RemoveNamedItem( "weapon_egon" );
	pRemove->pev->fuser4 = 0;
	pRemove->pev->renderfx = kRenderFxNone;
	pRemove->pev->renderamt = 0;
	pRemove->pev->rendercolor = Vector( 0, 0, 0 );
	pRemove->m_fCameraDelay = gpGlobals->time;
	pRules->SetPlayerModel( pRemove );

	return TRUE;
}

static BOOL BustersGrantEgonToLowestFragger( void )
{
	int bestFrags = 9999;
	CBasePlayer* candidates[32];
	int candidateCount = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( !pPlayer || pPlayer->HasDisconnected || pPlayer->IsSpectator() || !pPlayer->IsAlive() || IsPlayerBusting( pPlayer ) )
			continue;

		int frags = (int)pPlayer->pev->frags;
		if ( frags < bestFrags )
		{
			bestFrags = frags;
			candidateCount = 0;
		}

		if ( frags == bestFrags && candidateCount < 32 )
			candidates[candidateCount++] = pPlayer;
	}

	CBasePlayer* pBestPlayer = ( candidateCount > 0 ) ? candidates[RANDOM_LONG( 0, candidateCount - 1 )] : NULL;
	if ( !pBestPlayer )
		return FALSE;

	pBestPlayer->RemoveAllItems( false );
	pBestPlayer->pev->fuser4 = RADAR_BUSTER;
	pBestPlayer->GiveNamedItem( "weapon_egon" );

	return pBestPlayer->HasNamedPlayerItem( "weapon_egon" );
}

static const char* BustersObjectiveStatusText( int playerCount, int targetBusters, int holderCount, int looseCount, CBasePlayer* pPrimaryHolder )
{
	if ( targetBusters <= 0 || playerCount < 3 )
		return "Busters waiting for players";

	if ( targetBusters <= 1 )
	{
		if ( pPrimaryHolder )
			return UTIL_VarArgs( "%s is busting!", STRING( pPrimaryHolder->pev->netname ) );
		if ( looseCount > 0 )
			return "The egon is free";
		return "Selecting a buster...";
	}

	if ( holderCount > 0 )
		return UTIL_VarArgs( "%d of %d busters are busting.", holderCount, targetBusters );
	if ( looseCount > 0 )
		return UTIL_VarArgs( "%d of %d egons are free.", looseCount, targetBusters );

	return "Selecting busters...";
}

CMultiplayBusters::CMultiplayBusters()
{
	m_flEgonBustingCheckTime = -1;
	m_flObjectiveUpdateTime = -1;
}

void CMultiplayBusters::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Bust 'em");
			WRITE_STRING("");
			WRITE_BYTE(0);
			WRITE_STRING(scorelimit.value > 0 ? UTIL_VarArgs("First to %d wins", (int)scorelimit.value) : "No score limit");
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
			WRITE_BYTE( 2 );
			WRITE_STRING( "ghosts" );
			WRITE_STRING( "busters" );
		MESSAGE_END();

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *plr = UTIL_PlayerByIndex( i );
			if ( plr )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgTeamInfo, NULL, pPlayer->edict() );
					WRITE_BYTE( plr->entindex() );
					WRITE_STRING( plr->TeamID() );
				MESSAGE_END();
			}
		}
	}

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iRoundWins );
		WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
	MESSAGE_END();
}

void CMultiplayBusters::Think()
{
	if (!g_fGameOver)
	{
		CheckForEgons();

		if ( m_flObjectiveUpdateTime < 0.0f || m_flObjectiveUpdateTime <= gpGlobals->time )
		{
			SendObjectiveUpdate();
			m_flObjectiveUpdateTime = gpGlobals->time + 1.0f;
		}

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
			{
				if (plr->m_fCameraDelay && plr->m_fCameraDelay < gpGlobals->time)
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, plr->edict() );
						WRITE_BYTE(0);
						WRITE_STRING("cam_buster");
					MESSAGE_END();
					plr->m_fCameraDelay = 0;
				}

				// End session if hit score limit
				if ( scorelimit.value > 0 && plr->m_iRoundWins >= scorelimit.value )
				{
					GoToIntermission();
					break;
				}

				if (plr->m_iShowGameModeMessage > -1 && plr->m_iShowGameModeMessage < gpGlobals->time && !FBitSet(plr->pev->flags, FL_FAKECLIENT))
				{
					MESSAGE_BEGIN(MSG_ONE, gmsgBanner, NULL, plr->edict());
						WRITE_STRING("You are a Ghost");
						WRITE_STRING("Defeat the buster (a blue hev suit w/Egon)");
						WRITE_BYTE(80);
					MESSAGE_END();
					plr->m_iShowGameModeMessage = -1;
				}
			}
		}
	}

	CHalfLifeMultiplay::Think();
}

void CMultiplayBusters::SendObjectiveUpdate( void )
{
	int playerCount = UTIL_GetPlayerCount();
	int targetBusters = BustersTargetCount( playerCount );
	CBasePlayer* holders[32];
	int holderCount = BustersCountHolders( holders, 32 );
	int looseCount = BustersCountLooseEgons();
	CBasePlayer* pPrimaryHolder = ( holderCount > 0 ) ? holders[0] : NULL;
	const char *objectiveText = BustersObjectiveStatusText( playerCount, targetBusters, holderCount, looseCount, pPrimaryHolder );

	char scoreLimitText[64];
	if ( scorelimit.value > 0 )
		_snprintf( scoreLimitText, sizeof( scoreLimitText ), "Scorelimit is %d", (int)scorelimit.value );
	else
		strcpy( scoreLimitText, "No score limit" );
	scoreLimitText[sizeof( scoreLimitText ) - 1] = '\0';

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !plr || !plr->IsPlayer() || plr->HasDisconnected || FBitSet( plr->pev->flags, FL_FAKECLIENT ) )
			continue;

		MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict() );
			if ( plr->IsSpectator() )
				WRITE_STRING( "Watching Busters" );
			else if ( IsPlayerBusting( plr ) )
				WRITE_STRING( targetBusters > 1 ? "You Are Busting!" : "You Are The Buster!" );
			else
				WRITE_STRING( targetBusters > 1 ? "Defeat the busters" : "Defeat the buster" );
			WRITE_STRING( objectiveText );
			WRITE_BYTE( 0 );
			WRITE_STRING( scoreLimitText );
		MESSAGE_END();
	}
}

int CMultiplayBusters::IPointsForKill( CBasePlayer* pAttacker, CBasePlayer* pKilled )
{
	//If the attacker is busting, they get a point per kill
	if ( IsPlayerBusting( pAttacker ) )
		return 1;

	//If the victim is busting, then the attacker gets two points
	if ( IsPlayerBusting( pKilled ) )
		return 2;

	return 0;
}

void CMultiplayBusters::PlayerKilled( CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor )
{
	BOOL bVictimWasBuster = IsPlayerBusting( pVictim );
	if ( bVictimWasBuster )
	{
		int targetBusters = BustersTargetCount( UTIL_GetPlayerCount() );
		UTIL_ClientPrintAll( HUD_PRINTCENTER, targetBusters > 1 ? "A Buster is dead!!" : "The Buster is dead!!" );

		pVictim->m_fCameraDelay = gpGlobals->time + 4.0;

		MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
			WRITE_BYTE(CLIENT_SOUND_MASSACRE);
		MESSAGE_END();

		//Reset egon check time
		m_flEgonBustingCheckTime = -1;

		CBasePlayer *peKiller = NULL;
		CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );

		if ( ktmp && ( ktmp->Classify() == CLASS_PLAYER ) )
		{
			peKiller = (CBasePlayer*)ktmp;
			if (!IsPlayerBusting(peKiller))
				peKiller->m_iRoundWins++;
		}
		else if ( ktmp && ( ktmp->Classify() == CLASS_VEHICLE ) )
		{
			CBasePlayer *pDriver = (CBasePlayer *)((CFuncVehicle*)ktmp )->m_pDriver;

			if ( pDriver != NULL )
			{
				peKiller = pDriver;
				ktmp = pDriver;
				pKiller = pDriver->pev;
			}
		}

		if ( peKiller )
		{
			if ( pVictim != peKiller )
				UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "[Busters] %s has killed the Buster!\n", STRING( (CBasePlayer*)peKiller->pev->netname ) ) );
			else
				UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "[Busters] %s died!\n", STRING( (CBasePlayer*)peKiller->pev->netname ) ) );
		}

		pVictim->pev->renderfx = kRenderFxNone;
		pVictim->pev->renderamt = 0;
		pVictim->pev->rendercolor = g_vecZero;
	}

	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );

	if ( bVictimWasBuster )
	{
		m_flObjectiveUpdateTime = 0.0f;
		SendObjectiveUpdate();
	}
}

void CMultiplayBusters::DeathNotice( CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pevInflictor )
{
	//Only death notices that the Buster was involved in in Busting game mode
	if ( !IsPlayerBusting( pVictim ) && !IsPlayerBusting( CBaseEntity::Instance( pKiller ) ) )
		return;

	CHalfLifeMultiplay::DeathNotice( pVictim, pKiller, pevInflictor );
}

int CMultiplayBusters::WeaponShouldRespawn( CBasePlayerItem* pWeapon )
{
	if ( pWeapon->m_iId == WEAPON_EGON )
		return GR_WEAPON_RESPAWN_NO;

	return CHalfLifeMultiplay::WeaponShouldRespawn( pWeapon );
}


//=========================================================
// CheckForEgons:
// Keep total egons (held + loose) aligned to the dynamic target count.
// Adds are staggered by EGON_BUSTING_TIME; removals trim excess immediately.
//=========================================================
void CMultiplayBusters::CheckForEgons()
{
	// If a weapon is stolen
	if ( m_flCheckForWeapons <= gpGlobals->time )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );

			if (pPlayer && pPlayer->IsAlive() && !pPlayer->HasDisconnected &&
				strcmp(pPlayer->m_szTeamName, "busters") == 0 &&
				!IsPlayerBusting(pPlayer))
			{
				SetPlayerModel( pPlayer );
				pPlayer->pev->renderfx = kRenderFxNone;
				pPlayer->pev->renderamt = 0;
				pPlayer->pev->rendercolor = Vector( 0, 0, 0 );
				pPlayer->m_fCameraDelay = gpGlobals->time;
			}
		}
		m_flCheckForWeapons = gpGlobals->time + 1.0f;
	}

	int playerCount = UTIL_GetPlayerCount();
	int targetBusters = BustersTargetCount( playerCount );
	int holderCount = BustersCountHolders( NULL, 0 );
	int looseCount = BustersCountLooseEgons();
	int totalEgons = holderCount + looseCount;

	if ( targetBusters <= 0 )
	{
		while ( BustersRemoveOneLooseEgon() )
		{
		}

		while ( BustersDemoteOneHolder( this ) )
		{
		}

		m_flEgonBustingCheckTime = -1.0f;
		return;
	}

	while ( totalEgons > targetBusters )
	{
		if ( BustersRemoveOneLooseEgon() )
		{
			totalEgons--;
			continue;
		}

		if ( BustersDemoteOneHolder( this ) )
		{
			totalEgons--;
			continue;
		}

		break;
	}

	if ( totalEgons >= targetBusters )
	{
		m_flEgonBustingCheckTime = -1.0f;
		return;
	}

	if ( m_flEgonBustingCheckTime <= 0.0f )
	{
		m_flEgonBustingCheckTime = gpGlobals->time + EGON_BUSTING_TIME;
		return;
	}

	if ( m_flEgonBustingCheckTime > gpGlobals->time )
		return;

	if ( BustersGrantEgonToLowestFragger() )
	{
		holderCount = BustersCountHolders( NULL, 0 );
		looseCount = BustersCountLooseEgons();
		totalEgons = holderCount + looseCount;

		if ( totalEgons < targetBusters )
			m_flEgonBustingCheckTime = gpGlobals->time + EGON_BUSTING_TIME;
		else
			m_flEgonBustingCheckTime = -1.0f;
	}
	else
	{
		m_flEgonBustingCheckTime = gpGlobals->time + 1.0f;
	}
}

BOOL CMultiplayBusters::CanHavePlayerItem( CBasePlayer* pPlayer, CBasePlayerItem* pItem )
{
	//Buster cannot have more weapons nor ammo
	if ( BustingCanHaveItem( pPlayer, pItem ) == FALSE )
	{
		return FALSE;
	}

	return CHalfLifeMultiplay::CanHavePlayerItem( pPlayer, pItem );
}

BOOL CMultiplayBusters::CanHaveItem( CBasePlayer* pPlayer, CItem* pItem )
{
	//Buster cannot have more weapons nor ammo
	if (BustingCanHaveItem( pPlayer, pItem ) == FALSE )
	{
		return FALSE;
	}

	return CHalfLifeMultiplay::CanHaveItem( pPlayer, pItem );
}

void CMultiplayBusters::PlayerGotWeapon( CBasePlayer* pPlayer, CBasePlayerItem* pWeapon )
{
	if ( pWeapon->m_iId == WEAPON_EGON )
	{
		int targetBusters = BustersTargetCount( UTIL_GetPlayerCount() );
		int holderCount = BustersCountHolders( NULL, 0 );

		if ( targetBusters <= 0 || holderCount >= targetBusters )
		{
			// Cap reached: remove this egon pickup.
			pPlayer->RemovePlayerItem( pWeapon );
			pWeapon->Kill();
			return;
		}

		// Reset timer when egon is picked up.
		m_flEgonBustingCheckTime = -1.0f;

		UTIL_ClientPrintAll( HUD_PRINTCENTER, "Long live the new Buster!" );
		UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "[Busters] %s is busting!\n", STRING( pPlayer->pev->netname ) ) );

		if (pPlayer->m_iShowGameModeMessage == -1 && !FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgBanner, NULL, pPlayer->edict());
				WRITE_STRING("You Are Busting!");
				WRITE_STRING("Blast those ghosts (really, skeletons)");
				WRITE_BYTE(80);
			MESSAGE_END();
			pPlayer->m_iShowGameModeMessage = -2;
		}

		MESSAGE_BEGIN(MSG_ONE, gmsgStatusIcon, NULL, pPlayer->edict());
			WRITE_BYTE(1);
			WRITE_STRING("cam_buster");
		MESSAGE_END();
		pPlayer->m_fCameraDelay = 0;

		// AddPlayerItem calls this before the egon is fully committed to inventory,
		// so defer objective recompute to the next Think tick.
		m_flObjectiveUpdateTime = 0.0f;

		MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
			WRITE_BYTE(CLIENT_SOUND_PICKUPYOURWEAPON);
		MESSAGE_END();

		// ---------------------------------------------------------------
		// Radial purge: when a player picks up the egon, gib every other
		// player within range and play a fireball+ring shockwave at the
		// pickup origin.  The new buster is excluded via ignoreAttacker.
		// This breaks up the tight cluster that forms when the previous
		// buster dies surrounded by ghosts, giving the new buster a beat
		// of breathing room and visually cueing the role transition.
		//
		// Must run BEFORE SetPlayerModel(): PlayerGotWeapon is called
		// from CBasePlayer::AddPlayerItem before the egon is inserted
		// into m_rgpPlayerItems, so IsPlayerBusting() still reports
		// FALSE and SetPlayerModel() would (re)assign fuser4 = 0.  We
		// promote fuser4 to RADAR_BUSTER here so the blast's victims
		// (ghosts, fuser4 == 0) don't share the attacker's team flag
		// and get rejected by FPlayerCanTakeDamage as friendly fire.
		// SetPlayerModel() below will set fuser4 again to the same
		// value once IsPlayerBusting() flips true on the next frame
		// via PlayerSpawn / ClientUserInfoChanged paths.
		// ---------------------------------------------------------------
		{
			pPlayer->pev->fuser4 = RADAR_BUSTER;

			Vector vecBlast = pPlayer->pev->origin;
			int iContents = UTIL_PointContents ( vecBlast );

			// Fireball + dynamic light + sound (PAS so distant players hear it)
			MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecBlast );
				WRITE_BYTE( TE_EXPLOSION );
				WRITE_COORD( vecBlast.x );
				WRITE_COORD( vecBlast.y );
				WRITE_COORD( vecBlast.z );
				if (iContents != CONTENTS_WATER)
				{
					if (icesprites.value) {
						WRITE_SHORT( g_sModelIndexIceFireball );
					} else {
						WRITE_SHORT( g_sModelIndexFireball );
					}
				}
				else
				{
					WRITE_SHORT( g_sModelIndexWExplosion );
				}
				WRITE_BYTE( 30 );                 // scale * 10
				WRITE_BYTE( 15 );                 // framerate
				WRITE_BYTE( TE_EXPLFLAG_NONE );
			MESSAGE_END();

			// Expanding ring shockwave (vertical cylinder reads as a halo)
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecBlast );
				WRITE_BYTE( TE_BEAMCYLINDER );
				WRITE_COORD( vecBlast.x );
				WRITE_COORD( vecBlast.y );
				WRITE_COORD( vecBlast.z );
				WRITE_COORD( vecBlast.x );
				WRITE_COORD( vecBlast.y );
				WRITE_COORD( vecBlast.z + 256 );  // expansion radius
				WRITE_SHORT( g_sModelLightning );
				WRITE_BYTE( 0 );                  // start frame
				WRITE_BYTE( 0 );                  // framerate
				WRITE_BYTE( 12 );                 // life (1.2s)
				WRITE_BYTE( 16 );                 // width
				WRITE_BYTE( 0 );                  // noise
				WRITE_BYTE( 100 );                // r
				WRITE_BYTE( 180 );                // g
				WRITE_BYTE( 255 );                // b — buster blue tint
				WRITE_BYTE( 200 );                // brightness
				WRITE_BYTE( 0 );                  // speed
			MESSAGE_END();

			// Gib every other player inside the radius.  ignoreAttacker=TRUE
			// so the new buster (the attacker) is skipped; DMG_ALWAYSGIB
			// guarantees a clean gib regardless of damage rolls.
			::RadiusDamage( vecBlast, pPlayer->pev, pPlayer->pev,
				999.0f, 256.0f, CLASS_NONE,
				DMG_BLAST | DMG_ALWAYSGIB, TRUE );
		}

		SetPlayerModel( pPlayer );

		pPlayer->pev->health = pPlayer->pev->max_health;
		pPlayer->pev->armorvalue = 100;

		if (!pPlayer->m_fHasRune)
		{
			pPlayer->pev->renderfx = kRenderFxGlowShell;
			pPlayer->pev->renderamt = 25;
			pPlayer->pev->rendercolor = Vector( 0, 75, 250 );
		}

		CBasePlayerWeapon *pEgon = (CBasePlayerWeapon*)pWeapon;

		pEgon->m_iDefaultAmmo = 100;
		pPlayer->m_rgAmmo[pEgon->m_iPrimaryAmmoType] = pEgon->m_iDefaultAmmo;

		g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "frost" );
	}
}

void CMultiplayBusters::ClientUserInfoChanged( CBasePlayer* pPlayer, char* infobuffer )
{
	if (pPlayer->IsSpectator())
		return;

	SetPlayerModel( pPlayer );
}

void CMultiplayBusters::PlayerSpawn( CBasePlayer* pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn( pPlayer );

	CHalfLifeMultiplay::SavePlayerModel(pPlayer);

	// New player
	if (pPlayer->pev->iuser3 > 0)
	{
		// Already set to simple in client.cpp
		return;
	}
	else if (pPlayer->pev->iuser3 == 0) // Spectator now joining
	{
		strncpy( pPlayer->m_szTeamName, "ghosts", TEAM_NAME_LENGTH );
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
			WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
			WRITE_STRING( pPlayer->m_szTeamName );
		MESSAGE_END();
		pPlayer->pev->iuser3 = -1;
		pPlayer->m_iObserverWeapon = 0; // Used as the menu option
		pPlayer->m_iShowGameModeMessage = gpGlobals->time + 0.5;
	}

	SetPlayerModel( pPlayer );
}

void CMultiplayBusters::SetPlayerModel( CBasePlayer* pPlayer )
{
	if ( IsPlayerBusting( pPlayer ) )
	{
		pPlayer->pev->fuser4 = RADAR_BUSTER;
		strncpy( pPlayer->m_szTeamName, "busters", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "frost" );
	}
	else
	{
		pPlayer->pev->fuser4 = 0;
		strncpy( pPlayer->m_szTeamName, "ghosts", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "skeleton" );
	}

	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_STRING( pPlayer->m_szTeamName );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( pPlayer->entindex() );	// client number
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iRoundWins );
		WRITE_SHORT( GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
	MESSAGE_END();
}

int CMultiplayBusters::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		if (!strcmp(pTeamName, "busters"))
			return 1;
		else
			return 0; // ghosts
	}
	
	return -1;	// No match
}


int CMultiplayBusters::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() )
		return GR_NOTTEAMMATE;

	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

const char *CMultiplayBusters::GetTeamID( CBaseEntity *pEntity )
{
	if ( pEntity == NULL || pEntity->pev == NULL )
		return "";

	// return their team name
	return pEntity->TeamID();
}

BOOL CMultiplayBusters::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
{
	// always autoaim, unless target is a teammate
	CBaseEntity *pTgt = CBaseEntity::Instance( target );
	if ( pTgt && pTgt->IsPlayer() )
	{
		if ( PlayerRelationship( pPlayer, pTgt ) == GR_TEAMMATE )
			return FALSE; // don't autoaim at teammates
	}

	return CHalfLifeMultiplay::ShouldAutoAim( pPlayer, target );
}

BOOL CMultiplayBusters::MutatorAllowed(const char *mutator)
{
	if (strstr(mutator, g_szMutators[MUTATOR_RANDOMWEAPON - 1]) || atoi(mutator) == MUTATOR_RANDOMWEAPON)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_INVISIBLE - 1]) || atoi(mutator) == MUTATOR_INVISIBLE)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_BUSTERS - 1]) || atoi(mutator) == MUTATOR_BUSTERS)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_THIRDPERSON - 1]) || atoi(mutator) == MUTATOR_THIRDPERSON)
		return FALSE;

	return CHalfLifeMultiplay::MutatorAllowed(mutator);
}

BOOL CMultiplayBusters::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if ( pAttacker && pAttacker->IsPlayer() && pPlayer->pev->fuser4 == pAttacker->pev->fuser4 )
	{
		// my teammate hit me.
		if ( (friendlyfire.value == 0) && (pAttacker != pPlayer) )
		{
			// friendly fire is off, and this hit came from someone other than myself,  then don't get hurt
			return FALSE;
		}
	}

	return CHalfLifeMultiplay::FPlayerCanTakeDamage( pPlayer, pAttacker );
}

BOOL CMultiplayBusters::IsTeamplay( void )
{
	return TRUE;
}

BOOL CMultiplayBusters::CanHaveNamedItem( CBasePlayer *pPlayer, const char *pszItemName )
{
	if (strcmp(pszItemName, "weapon_egon") == 0)
	{
		int targetBusters = BustersTargetCount( UTIL_GetPlayerCount() );
		if ( targetBusters <= 0 )
			return FALSE;

		int holderCount = BustersCountHolders( NULL, 0 );
		int looseCount = BustersCountLooseEgons();
		if ( holderCount + looseCount >= targetBusters )
			return FALSE;
	}

	return CHalfLifeMultiplay::CanHaveNamedItem( pPlayer, pszItemName );
}

BOOL CMultiplayBusters::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	if (pWeapon && pWeapon->m_iId == WEAPON_EGON)
	{
		// If the player is picking up an Egon, always switch to it
		return TRUE;
	}

	return CHalfLifeMultiplay::FShouldSwitchWeapon( pPlayer, pWeapon );
}

BOOL CMultiplayBusters::CanRandomizeWeapon( const char *name )
{
	if ( !strcmp(name, "weapon_egon") )
		return FALSE;
	return TRUE;
}

BOOL CMultiplayBusters::CanHavePlayerAmmo( CBasePlayer *pPlayer, CBasePlayerAmmo *pAmmo )
{
	if (pPlayer->pev->fuser4 == RADAR_BUSTER)
		return FALSE;

	return CHalfLifeMultiplay::CanHavePlayerAmmo( pPlayer, pAmmo );
}

BOOL CMultiplayBusters::IsAllowedToDropWeapon( CBasePlayer *pPlayer )
{
	if (pPlayer->pev->fuser4 == RADAR_BUSTER)
		return FALSE;

	return CHalfLifeMultiplay::IsAllowedToDropWeapon( pPlayer );
}

void CMultiplayBusters::ClientDisconnected( edict_t *pClient )
{
	if (pClient)
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance(pClient);

		if (pPlayer)
		{
			if (IsPlayerBusting( pPlayer ) )
			{
				//Reset egon check time so a new buster will be chosen immediately
				m_flEgonBustingCheckTime = -1.0f;

				MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
					WRITE_STRING("Bust 'em");
					WRITE_STRING("The Buster disconnected!!");
					WRITE_BYTE(0);
					WRITE_STRING("");
				MESSAGE_END();
			}
		}
	}

	CHalfLifeMultiplay::ClientDisconnected( pClient );
}
