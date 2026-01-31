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
#include "ctc_gamerules.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"

extern int gmsgGameMode;
extern int gmsgScoreInfo;
extern int gmsgPlayClientSound;
extern int gmsgObjective;
extern int gmsgTeamNames;
extern int gmsgTeamInfo;
extern int gmsgStatusIcon;
extern int gmsgSpecialEntity;

#define SPAWN_TIME 30.0

CHalfLifeCaptureTheChumtoad::CHalfLifeCaptureTheChumtoad()
{
	m_pHolder = NULL; // must initialize, otherwise, GET() crash.
	m_fChumtoadInPlay = FALSE;
	m_fCreateChumtoadTimer = m_fMoveChumtoadTimer = 0;
	m_fChumtoadPlayTimer = gpGlobals->time;
}

void CHalfLifeCaptureTheChumtoad::Think( void )
{
	CHalfLifeMultiplay::Think();

	// No loop during intermission
	if ( m_flIntermissionEndTime )
	{
		// Remove any left
		edict_t *pEdict = FIND_ENTITY_BY_CLASSNAME(NULL, "monster_ctctoad");
		while (!FNullEnt(pEdict))
		{
			UTIL_Remove(CBaseEntity::Instance(pEdict));
			pEdict = FIND_ENTITY_BY_CLASSNAME(pEdict, "monster_ctctoad");
		}
		return;
	}

	if (m_fChumtoadPlayTimer < gpGlobals->time)
	{
		edict_t *pEdict = FIND_ENTITY_BY_CLASSNAME(NULL, "monster_ctctoad");
		edict_t *pChumtoad = NULL;
		BOOL foundToad = FALSE;

		int playerCount = UTIL_GetPlayerCount();

		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("Get the chumtoad");
			if (m_pHolder)
				WRITE_STRING(m_fChumtoadInPlay ? UTIL_VarArgs("%s has it!", STRING(m_pHolder->pev->netname)) : "The chumtoad is free");
			else if ( playerCount > 1 )
				WRITE_STRING(m_fChumtoadInPlay ? "The chumtoad is held" : "The chumtoad is free");
			else
				WRITE_STRING("Chumtoad waiting for players");
			WRITE_BYTE(0);
		MESSAGE_END();

		// Find toad, remove extras
		while (!FNullEnt(pEdict))
		{
			// Remove all when not enough players
			if ( playerCount < 2 )
			{
				foundToad = TRUE;
			}

			if (!foundToad)
			{
				if (pEdict->v.iuser1 == 1)
				{
					pChumtoad = pEdict;
					foundToad = TRUE;
				}
			}
			else
			{
				// Uh?
				UTIL_Remove(CBaseEntity::Instance(pEdict));
			}

			pEdict = FIND_ENTITY_BY_CLASSNAME(pEdict, "monster_ctctoad");
		}

		if (playerCount < 2)
		{
			// Player check, only one person should have the toad at any one time
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
				if (plr && plr->IsPlayer())
				{
					if (plr->m_iHoldingChumtoad)
					{
						plr->m_iHoldingChumtoad = FALSE;
						UTIL_MakeVectors(plr->pev->v_angle);
						DropCharm(plr, plr->pev->origin + gpGlobals->v_forward * 64);
						plr->RemoveNamedItem("weapon_chumtoad");
					}
				}
			}
		}

		// No chumtoad, create it
		if (FNullEnt(pChumtoad) && m_fChumtoadInPlay == FALSE)
		{
			if (m_fCreateChumtoadTimer == 0 || playerCount < 2) 
				m_fCreateChumtoadTimer = gpGlobals->time + 3.0;
			
			if (m_fCreateChumtoadTimer > 0 && m_fCreateChumtoadTimer <= gpGlobals->time)
			{
				if (CreateChumtoad())
				{
					UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC]: The chumtoad has spawned!\n");
					m_fCreateChumtoadTimer = 0;
					m_fMoveChumtoadTimer = gpGlobals->time + SPAWN_TIME;
					MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
						WRITE_BYTE(CLIENT_SOUND_CTF_CAPTURE);
					MESSAGE_END();
				}
				else
				{
					// Keep trying
					m_fCreateChumtoadTimer = gpGlobals->time + 1.0;
				}
			}
		}

		// Chumtoad hopping around, move it once and a while
		if (!FNullEnt(pChumtoad) && m_fChumtoadInPlay == FALSE && m_fMoveChumtoadTimer <= gpGlobals->time)
		{
			CBaseEntity *pSpot = NULL;
			for (int i = RANDOM_LONG(1,8); i > 0; i--)
				pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch");
			if (pSpot == NULL)
				m_fMoveChumtoadTimer = gpGlobals->time + 1.0;
			else
			{
				UTIL_SetOrigin(VARS(pChumtoad), pSpot->pev->origin);
				m_fMoveChumtoadTimer = gpGlobals->time + SPAWN_TIME;
				UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC]: The chumtoad has teleported!\n");
			}
		}

		m_fChumtoadPlayTimer = gpGlobals->time + 1.0;
	}
}

void CHalfLifeCaptureTheChumtoad::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
		WRITE_BYTE( 2 );
		WRITE_STRING( "chaser" );
		WRITE_STRING( "holder" );
	MESSAGE_END();

	strncpy( pPlayer->m_szTeamName, "chaser", TEAM_NAME_LENGTH );

	// notify everyone's HUD of the team change
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_STRING( pPlayer->m_szTeamName );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iRoundWins );
		WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
	MESSAGE_END();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );
		if ( plr && !FBitSet(pPlayer->pev->flags, FL_FAKECLIENT) )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgTeamInfo, NULL, pPlayer->edict() );
				WRITE_BYTE( plr->entindex() );
				WRITE_STRING( plr->TeamID() );
			MESSAGE_END();
		}
	}

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Capture the chumtoad");
			WRITE_STRING("");
			WRITE_BYTE(0);
		MESSAGE_END();
	}
}

BOOL CHalfLifeCaptureTheChumtoad::CreateChumtoad()
{
	CBaseEntity *pSpot = NULL;
	for (int i = RANDOM_LONG(1,8); i > 0; i--)
		pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch");

	if (pSpot == NULL)
	{
		ALERT(at_console, "Error creating chumtoad. Spawn point not found.\n");
		return FALSE;
	}

	CBaseEntity *pToad = CBaseEntity::Create("monster_ctctoad", pSpot->pev->origin, Vector(0, 0, 0), NULL);

	if (pToad)
	{
		pToad->pev->iuser1 = 1;
		pToad->pev->velocity.x = RANDOM_FLOAT(-300, 300);
		pToad->pev->velocity.y = RANDOM_FLOAT(-300, 300);
		pToad->pev->velocity.z = RANDOM_FLOAT(0, 300);
	}

	return TRUE;
}

void CHalfLifeCaptureTheChumtoad::CaptureCharm( CBasePlayer *pPlayer )
{
	if (UTIL_GetPlayerCount() < 2)
		return;

	pPlayer->m_iHoldingChumtoad = TRUE;
	m_fChumtoadInPlay = TRUE;

	pPlayer->pev->renderfx = kRenderFxGlowShell;
	pPlayer->pev->renderamt = 10;
	pPlayer->pev->rendercolor = Vector(0, 200, 0);

	pPlayer->pev->fuser4 = 1;
	m_pHolder = (CBaseEntity *)pPlayer;

	int m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( pPlayer->entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 50 ); // life
		WRITE_BYTE( 3 );  // width
		if (icesprites.value) {
			WRITE_BYTE( 0 );   // r, g, b
			WRITE_BYTE( 160 );   // r, g, b
			WRITE_BYTE( 255 );   // r, g, b
		} else {
			WRITE_BYTE( 224 );   // r, g, b
			WRITE_BYTE( 224 );   // r, g, b
			WRITE_BYTE( 255 );   // r, g, b
		}
		WRITE_BYTE( 200 );	// brightness
	MESSAGE_END();

	UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC]: %s has captured the chumtoad!\n",
	STRING(pPlayer->pev->netname));

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
		WRITE_BYTE(CLIENT_SOUND_BULLSEYE);
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pPlayer->edict() );
		WRITE_BYTE(1);
		WRITE_STRING("chumtoad");
		WRITE_BYTE(0);
		WRITE_BYTE(160);
		WRITE_BYTE(255);
	MESSAGE_END();

	// notify everyone's HUD of the team change
	strncpy( pPlayer->m_szTeamName, "holder", TEAM_NAME_LENGTH );
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_STRING( pPlayer->m_szTeamName );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iRoundWins );
		WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
	MESSAGE_END();

	pPlayer->m_iChumtoadCounter = pPlayer->m_iCaptureTime = 0;
	m_fCreateChumtoadTimer = -1;
	m_fMoveChumtoadTimer = 0;
}

CBaseEntity *CHalfLifeCaptureTheChumtoad::DropCharm( CBasePlayer *pPlayer, Vector origin )
{
	m_fChumtoadInPlay = FALSE;

	pPlayer->pev->rendermode = kRenderNormal;
	pPlayer->pev->renderfx = kRenderFxNone;
	pPlayer->pev->renderamt = 0;

	pPlayer->pev->fuser4 = 0;
	m_pHolder = NULL;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_KILLBEAM );
		WRITE_SHORT( pPlayer->entindex() );	// entity
	MESSAGE_END();

	UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC]: %s has dropped the chumtoad!\n",
		STRING(pPlayer->pev->netname));

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
		WRITE_BYTE(CLIENT_SOUND_MANIAC);
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pPlayer->edict() );
		WRITE_BYTE(0);
		WRITE_STRING("chumtoad");
	MESSAGE_END();

	strncpy( pPlayer->m_szTeamName, "chaser", TEAM_NAME_LENGTH );
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_STRING( pPlayer->m_szTeamName );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iRoundWins );
		WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
	MESSAGE_END();

	m_fCreateChumtoadTimer = -1;
	m_fMoveChumtoadTimer = gpGlobals->time + SPAWN_TIME;
	CBaseEntity *pChumtoad = CBaseEntity::Create("monster_ctctoad", origin, pPlayer->pev->v_angle, NULL);
	if (pChumtoad)
		pChumtoad->pev->iuser1 = 1;
	else
		m_fCreateChumtoadTimer = gpGlobals->time + 1.0; // in case we could not create a chumtoad

	return pChumtoad;
}

extern DLL_GLOBAL BOOL g_fGameOver;

void CHalfLifeCaptureTheChumtoad::PlayerThink( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerThink(pPlayer);

	if (m_flIntermissionEndTime)
		return;

	if (!g_fGameOver)
	{
		// End session if hit round limit
		if ( pPlayer->m_iRoundWins >= scorelimit.value )
		{
			GoToIntermission();
			return;
		}
	}

	// Updates once per second
	int time_remaining = (int)gpGlobals->time;
	BOOL foundChumtoad = FALSE, scoringPoints = FALSE;
	char *message;

	if (pPlayer->m_iCaptureTime != time_remaining)
	{
		if (pPlayer->IsAlive() && pPlayer->m_iHoldingChumtoad)
		{
			// Keep running to score points
			if (pPlayer->pev->velocity.Length() > 50)
			{
				pPlayer->m_iChumtoadCounter++;
				message = UTIL_VarArgs("Running with the Chumtoad!\nPoints: %d | Timer: %d", 
						(int)pPlayer->m_iRoundWins, pPlayer->m_iChumtoadCounter);
				scoringPoints = TRUE;
				pPlayer->m_iChumtoadDropCounter = 10;
			}
			else
			{
				message = UTIL_VarArgs("Keep running to score points\nor the chumtoad will slip away!");

				if (RANDOM_LONG(0, pPlayer->m_iChumtoadDropCounter) == 0 && pPlayer->m_pActiveItem)
				{
					ClientPrint(pPlayer->pev, HUD_PRINTCENTER, "The chumtoad slipped away!\n");
					CBasePlayerWeapon *weapon = (CBasePlayerWeapon *)pPlayer->m_pActiveItem;
					weapon->SendWeaponAnim( 5 );
					weapon->PrimaryAttack();
				}

				if (pPlayer->m_iChumtoadDropCounter > 0)
					pPlayer->m_iChumtoadDropCounter--;
			}

			if (scoringPoints)
			{
				int secondsUntilPoint = ctcsecondsforpoint.value > 0 ? ctcsecondsforpoint.value : 10;
				if (pPlayer->m_iChumtoadCounter % secondsUntilPoint == 0)
				{
					MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
						WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
						WRITE_SHORT( pPlayer->pev->frags );
						WRITE_SHORT( pPlayer->m_iDeaths );
						WRITE_SHORT( ++pPlayer->m_iRoundWins );
						WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
					MESSAGE_END();

					UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC]: %s has scored a point!\n", 
						STRING(pPlayer->pev->netname));
					
					ClientPrint(pPlayer->pev, HUD_PRINTCENTER, UTIL_VarArgs("You Have Scored a Point!\n"));

					MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, pPlayer->edict() );
						WRITE_BYTE(CLIENT_SOUND_LEVEL_UP);
					MESSAGE_END();

					pPlayer->m_iChumtoadCounter = 0;
				}

				TraceResult tr;
				Vector vecSpot = pPlayer->pev->origin + Vector(0 , 0, 8); //move up a bit, and trace down.
				UTIL_TraceLine(vecSpot, vecSpot + Vector(0, 0, -64), ignore_monsters, ENT(pPlayer->pev), &tr);
				UTIL_BloodDecalTrace(&tr, BLOOD_COLOR_YELLOW);
			}

			pPlayer->DisplayHudMessage(message,
			TXT_CHANNEL_GAME_INFO, -1, 0.83, 255, 255, 255, 0, 0.25, 0.25, 2, 0);
		}
	}

	pPlayer->m_iCaptureTime = time_remaining;
}

void CHalfLifeCaptureTheChumtoad::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );

	if (pVictim->m_iHoldingChumtoad)
	{
		// Not here, on spawn so that no weapon is dropped.
		// pVictim->m_iHoldingChumtoad = FALSE;
		UTIL_MakeVectors(pVictim->pev->v_angle);
		DropCharm(pVictim, pVictim->pev->origin + gpGlobals->v_forward * 64);
	}
}

#if defined( GRAPPLING_HOOK )
BOOL CHalfLifeCaptureTheChumtoad::AllowGrapplingHook( CBasePlayer *pPlayer )
{
	return ( grapplinghook.value && !pPlayer->m_iHoldingChumtoad );
}
# endif

void CHalfLifeCaptureTheChumtoad::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);

	// Needed for dropping weapons on kill
	pPlayer->m_iHoldingChumtoad = FALSE;
}

void CHalfLifeCaptureTheChumtoad::ClientDisconnected( edict_t *pClient )
{
	CHalfLifeMultiplay::ClientDisconnected( pClient );

	if (pClient)
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance(pClient);

		if (pPlayer)
		{
			if (pPlayer->m_iHoldingChumtoad)
			{
				pPlayer->m_iHoldingChumtoad = FALSE;
				UTIL_MakeVectors(pPlayer->pev->v_angle);
				DropCharm(pPlayer, pPlayer->pev->origin + gpGlobals->v_forward * 64);
			}
		}
	}
}

int CHalfLifeCaptureTheChumtoad::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	return pPlayer->m_iHoldingChumtoad ? GR_PLR_DROP_GUN_NO : CHalfLifeMultiplay::DeadPlayerWeapons(pPlayer);
}

BOOL CHalfLifeCaptureTheChumtoad::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if (pAttacker && strcmp(STRING(pAttacker->pev->classname), "trigger_hurt") == 0)
		return TRUE;

	return pPlayer->m_iHoldingChumtoad ? TRUE : FALSE;
}

BOOL CHalfLifeCaptureTheChumtoad::IsAllowedToSpawn( CBaseEntity *pEntity )
{
	if (strcmp(STRING(pEntity->pev->classname), "weapon_chumtoad") == 0 ||
		strcmp(STRING(pEntity->pev->classname), "weapon_snark") == 0 ||
		strcmp(STRING(pEntity->pev->classname), "rune_ammo") == 0)
		return FALSE;

	return TRUE;
}

int CHalfLifeCaptureTheChumtoad::WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	if (strcmp(STRING(pWeapon->pev->classname), "weapon_chumtoad") == 0 ||
		strcmp(STRING(pWeapon->pev->classname), "weapon_snark") == 0)
		return GR_WEAPON_RESPAWN_NO;

	return CHalfLifeMultiplay::WeaponShouldRespawn(pWeapon);
}

BOOL CHalfLifeCaptureTheChumtoad::CanRandomizeWeapon( const char *name )
{
	if (strcmp(name, "weapon_chumtoad") == 0 ||
		strcmp(name, "weapon_snark") == 0)
		return FALSE;

	return TRUE;
}

BOOL CHalfLifeCaptureTheChumtoad::IsAllowedToDropWeapon( CBasePlayer *pPlayer )
{
	return FALSE;
}

BOOL CHalfLifeCaptureTheChumtoad::AllowRuneSpawn( const char *szRune )
{
	if (!strcmp("rune_frag", szRune));
		return FALSE;

	return TRUE;
}

int CHalfLifeCaptureTheChumtoad::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() )
		return GR_TEAMMATE;

	CBasePlayer *pl = (CBasePlayer *)pPlayer;
	CBasePlayer *pt = (CBasePlayer *)pTarget;

	if ( pl->m_iHoldingChumtoad || pt->m_iHoldingChumtoad )
	{
		return GR_NOTTEAMMATE;
	}

	return GR_TEAMMATE;
}

const char *CHalfLifeCaptureTheChumtoad::GetTeamID( CBaseEntity *pEntity )
{
	if ( pEntity == NULL || pEntity->pev == NULL )
		return "";

	if (!pEntity->IsPlayer())
		return "";

	CBasePlayer *pl = (CBasePlayer *)pEntity;
	if ( pl->m_iHoldingChumtoad )
		return "holder";

	// return their team name
	return pEntity->TeamID();
}

BOOL CHalfLifeCaptureTheChumtoad::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
{
	CBaseEntity *pTgt = CBaseEntity::Instance( target );
	if ( pTgt && pTgt->IsPlayer() )
	{
		if ( ((CBasePlayer *)pTgt)->m_iHoldingChumtoad )
			return TRUE;
	}

	return FALSE;
}

BOOL CHalfLifeCaptureTheChumtoad::MutatorAllowed(const char *mutator)
{
	if (strstr(mutator, g_szMutators[MUTATOR_999 - 1]) || atoi(mutator) == MUTATOR_999)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_CHUMXPLODE - 1]) || atoi(mutator) == MUTATOR_CHUMXPLODE)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_NOCLIP - 1]) || atoi(mutator) == MUTATOR_NOCLIP)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_DONTSHOOT - 1]) || atoi(mutator) == MUTATOR_DONTSHOOT)
		return FALSE;

	return CHalfLifeMultiplay::MutatorAllowed(mutator);
}

int CHalfLifeCaptureTheChumtoad::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		if (!strcmp(pTeamName, "holder"))
			return 1;
		else
			return 0;
	}
	
	return -1;	// No match
}

BOOL CHalfLifeCaptureTheChumtoad::IsTeamplay( void )
{
	return TRUE;
}
