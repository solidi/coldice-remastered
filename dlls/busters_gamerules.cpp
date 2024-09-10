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

#define EGON_BUSTING_TIME 10

extern int gmsgObjective;
extern int gmsgTeamNames;
extern int gmsgTeamInfo;
extern int gmsgScoreInfo;
extern int gmsgPlayClientSound;

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

CMultiplayBusters::CMultiplayBusters()
{
	m_flEgonBustingCheckTime = -1;
}

void CMultiplayBusters::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Bust 'em");
			WRITE_STRING("");
			WRITE_BYTE(0);
			WRITE_STRING("");
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
			WRITE_BYTE( 2 );
			WRITE_STRING( "ghosts" );
			WRITE_STRING( "busters" );
		MESSAGE_END();
	}
}

void CMultiplayBusters::Think()
{
	if (!g_fGameOver)
	{
		CheckForEgons();
	}

	CHalfLifeMultiplay::Think();
}

int CMultiplayBusters::IPointsForKill( CBasePlayer* pAttacker, CBasePlayer* pKilled )
{
	//If the attacker is busting, they get a point per kill
	if ( IsPlayerBusting( pAttacker ) )
		return 1;

	//If the victim is busting, then the attacker gets a point
	if ( IsPlayerBusting( pKilled ) )
		return 2;

	return 0;
}

void CMultiplayBusters::PlayerKilled( CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor )
{
	if ( IsPlayerBusting( pVictim ) )
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "The Buster is dead!!" );

		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("Bust 'em");
			WRITE_STRING("The Buster is dead!!");
			WRITE_BYTE(0);
			WRITE_STRING("");
		MESSAGE_END();

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
		/*else if ( ktmp && ( ktmp->Classify() == CLASS_VEHICLE ) )
		{
			CBasePlayer *pDriver = ( (CFuncVehicle*)ktmp )->m_pDriver;

			if ( pDriver != NULL )
			{
				peKiller = pDriver;
				ktmp = pDriver;
				pKiller = pDriver->pev;
			}
		}*/

		if ( peKiller )
		{
			UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "[Busters]: %s has killed the Buster!\n", STRING( (CBasePlayer*)peKiller->pev->netname ) ) );
		}

		pVictim->pev->renderfx = kRenderFxNone;
		pVictim->pev->rendercolor = g_vecZero;
		//pVictim->pev->effects &= ~EF_BRIGHTFIELD;
	}

	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );
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
//Check to see if any player has an egon
//If they don't then get the lowest player on the scoreboard and give them one
//Then check to see if any weapon boxes out there has an egon, and delete it
//=========================================================
void CMultiplayBusters::CheckForEgons()
{
	if ( m_flEgonBustingCheckTime <= 0.0f )
	{
		m_flEgonBustingCheckTime = gpGlobals->time + EGON_BUSTING_TIME;
		return;
	}

	if ( m_flEgonBustingCheckTime <= gpGlobals->time )
	{
		m_flEgonBustingCheckTime = -1.0f;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );

			//Someone is busting, no need to continue
			if ( IsPlayerBusting( pPlayer ) )
				return;
		}

		int bBestFrags = 9999;
		CBasePlayer* pBestPlayer = NULL;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer* pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );

			if ( pPlayer && pPlayer->pev->frags <= bBestFrags )
			{
				bBestFrags = pPlayer->pev->frags;
				pBestPlayer = pPlayer;
			}
		}

		if ( pBestPlayer )
		{
			pBestPlayer->RemoveAllItems( false );
			pBestPlayer->pev->fuser4 = 1; // so we can give the named item
			pBestPlayer->GiveNamedItem( "weapon_egon" );

			CBaseEntity* pEntity = NULL;

			//Find a weaponbox that includes an Egon, then destroy it
			while ( ( pEntity = UTIL_FindEntityByClassname( pEntity, "weaponbox" ) ) != NULL )
			{
				CWeaponBox* pWeaponBox = (CWeaponBox*)pEntity;

				if ( pWeaponBox )
				{
					CBasePlayerItem* pWeapon;

					for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
					{
						pWeapon = pWeaponBox->m_rgpPlayerItems[i];

						while ( pWeapon )
						{
							//There you are, bye box
							if ( pWeapon->m_iId == WEAPON_EGON )
							{
								pWeaponBox->Kill();
								break;
							}

							pWeapon = pWeapon->m_pNext;
						}
					}
				}
			}
		}
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
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "Long live the new Buster!" );
		UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "[Busters]: %s is busting!\n", STRING( (CBasePlayer*)pPlayer->pev->netname ) ) );

		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("Bust 'em");
			WRITE_STRING(UTIL_VarArgs("%s is busting!\n", STRING( (CBasePlayer*)pPlayer->pev->netname)));
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
			WRITE_BYTE(CLIENT_SOUND_PICKUPYOURWEAPON);
		MESSAGE_END();

		SetPlayerModel( pPlayer );

		pPlayer->pev->health = pPlayer->pev->max_health;
		pPlayer->pev->armorvalue = 100;

		pPlayer->pev->renderfx = kRenderFxGlowShell;
		pPlayer->pev->renderamt = 25;
		pPlayer->pev->rendercolor = Vector( 0, 75, 250 );

		CBasePlayerWeapon *pEgon = (CBasePlayerWeapon*)pWeapon;

		pEgon->m_iDefaultAmmo = 100;
		pPlayer->m_rgAmmo[pEgon->m_iPrimaryAmmoType] = pEgon->m_iDefaultAmmo;

		g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "frost" );
	}
}

void CMultiplayBusters::ClientUserInfoChanged( CBasePlayer* pPlayer, char* infobuffer )
{
	SetPlayerModel( pPlayer );
}

void CMultiplayBusters::PlayerSpawn( CBasePlayer* pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn( pPlayer );

	CHalfLifeMultiplay::SavePlayerModel(pPlayer);

	SetPlayerModel( pPlayer );
}

void CMultiplayBusters::SetPlayerModel( CBasePlayer* pPlayer )
{
	if ( IsPlayerBusting( pPlayer ) )
	{
		pPlayer->pev->fuser4 = 1;
		strncpy( pPlayer->m_szTeamName, "busters", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "frost" );
		//g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "team", pPlayer->m_szTeamName );
	}
	else
	{
		pPlayer->pev->fuser4 = 0;
		strncpy( pPlayer->m_szTeamName, "ghosts", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "skeleton" );
		//g_engfuncs.pfnSetClientKeyValue( pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "team", pPlayer->m_szTeamName );
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

	return CHalfLifeMultiplay::MutatorAllowed(mutator);
}
