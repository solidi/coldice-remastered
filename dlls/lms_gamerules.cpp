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
#include "lms_gamerules.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"

extern int gmsgObjective;
extern int gmsgShowTimer;
extern int gmsgPlayClientSound;
extern int gmsgRoundTime;
extern int gmsgScoreInfo;
extern int gmsgTeamNames;
extern int gmsgTeamInfo;

CHalfLifeLastManStanding::CHalfLifeLastManStanding()
{
	m_iFirstBloodDecided = TRUE; // no first blood award
}

void CHalfLifeLastManStanding::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Last man standing");
			WRITE_STRING("");
			WRITE_BYTE(0);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgTeamNames, NULL, pPlayer->edict());
			WRITE_BYTE( 1 );
			WRITE_STRING( "Active" );
		MESSAGE_END();
	}
}

void CHalfLifeLastManStanding::Think( void )
{
	CHalfLifeMultiplay::Think();

	if ( flUpdateTime > gpGlobals->time )
		return;

	CheckRounds();

	// No loop during intermission
	if ( m_flIntermissionEndTime )
		return;

	if ( m_flRoundTimeLimit )
	{
		if ( CheckGameTimer() )
			return;
	}

//===================================================

	//access this when game is in progress.
	//we are checking for the last man here.

//===================================================

	if ( g_GameInProgress )
	{
		int clients_alive = 0;
		int client_index = 0;
		const char *client_name;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			//player must exist, and must be alive
			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
			{
				if ( plr->m_flForceToObserverTime && plr->m_flForceToObserverTime < gpGlobals->time )
				{
					edict_t *pentSpawnSpot = g_pGameRules->GetPlayerSpawnSpot( plr );
					plr->StartObserver(plr->pev->origin, VARS(pentSpawnSpot)->angles);
					plr->m_flForceToObserverTime = 0;
				}

				//player cannot be disconnected client
				//and is currently in this game of LMS.
				if ( plr->IsInArena && plr->pev->frags > 0 )
				{
					clients_alive++;
					client_index = i;
					client_name = STRING(plr->pev->netname);
				}
				else
				{
					//for clients who connected while game in progress.
					if ( plr->IsSpectator() )
					{
						//ClientPrint(plr->pev, HUD_PRINTCENTER, "LMS round in progress.\n");
					} else {
						// Send them to observer
						if (!plr->IsInArena)
							plr->m_flForceToObserverTime = gpGlobals->time;
					}
				}
			}
		}

		if (clients_alive > 1)
		{
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING("Last man standing");
				WRITE_STRING(UTIL_VarArgs("Players alive: %d", clients_alive));
				WRITE_BYTE(float(clients_alive) / (m_iPlayersInGame) * 100);
			MESSAGE_END();
		} 
		else
		{
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING("Last man standing");
				WRITE_STRING("");
				WRITE_BYTE(0);
				WRITE_STRING(UTIL_VarArgs("%s is the winner!", client_name));
			MESSAGE_END();
		}

		//found victor / or draw.
		if ( clients_alive <= 1 )
		{
			//stop timer / end game.
			m_flRoundTimeLimit = 0;
			g_GameInProgress = FALSE;
			MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
				WRITE_BYTE(0);
			MESSAGE_END();

			if ( clients_alive == 1 )
			{
				UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("%s\nis the last man standing!\n", client_name ));

				CBasePlayer *pl = (CBasePlayer *)UTIL_PlayerByIndex( client_index );
				MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, pl->edict() );
					WRITE_BYTE(CLIENT_SOUND_LMS);
				MESSAGE_END();
				DisplayWinnersGoods( pl );
			}	
			else
			{
				UTIL_ClientPrintAll(HUD_PRINTCENTER, "No man is left standing!\n");
			}

			m_iSuccessfulRounds++;
			flUpdateTime = gpGlobals->time + 5.0;
			return;
		}

		flUpdateTime = gpGlobals->time + 3.0;
		return;
	}

//===================================================

	//if the game is not in progress
	//make sure there is more than one player
	//dub and recheck then spawn the 
	//players out of observer.

//===================================================

	int clients = 0;
	clients = CheckClients();

	if ( clients > 1 )
	{
		if ( m_fWaitForPlayersTime == -1 )
			m_fWaitForPlayersTime = gpGlobals->time + 17.0;

		if ( m_fWaitForPlayersTime > gpGlobals->time )
		{
			SuckAllToSpectator();
			flUpdateTime = gpGlobals->time + 1.0;
			UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("Battle will begin in %.0f\n", (m_fWaitForPlayersTime + 3) - gpGlobals->time));
			return;
		}

		if ( m_iCountDown > 0 )
		{
			if (m_iCountDown == 3) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_PICKUPYOURWEAPON);
				MESSAGE_END();
			}
			UTIL_ClientPrintAll(HUD_PRINTCENTER,
				UTIL_VarArgs("Prepare for LMS battle\n\n%i...\n", m_iCountDown));
			SuckAllToSpectator(); // in case players join during a countdown.
			m_iCountDown--;
			flUpdateTime = gpGlobals->time + 1.0;
			return;
		}

		ALERT(at_console, "Players in LMS: ");

		g_GameInProgress = TRUE;

		//frags + time.
		SetRoundLimits();
		InsertClientsIntoArena(startwithlives.value);
		ALERT(at_console, "\n");

		m_iCountDown = 3;
		m_fWaitForPlayersTime = -1;

		if (roundtimelimit.value > 0)
		{
			MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
				WRITE_BYTE(1);
			MESSAGE_END();

			MESSAGE_BEGIN(MSG_ALL, gmsgRoundTime);
				WRITE_SHORT(roundtimelimit.value * 60.0);
			MESSAGE_END();
		}

		// Resend team info
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamNames );
			WRITE_BYTE( 1 );
			WRITE_STRING( "Active" );
		MESSAGE_END();

		UTIL_ClientPrintAll(HUD_PRINTCENTER, "Last man standing has begun!\n");
	}
	else
	{
		SuckAllToSpectator();
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("Battle Royale");
			WRITE_STRING("Waiting for other players");
			WRITE_BYTE(0);
			WRITE_STRING(UTIL_VarArgs("%d Rounds", (int)roundlimit.value));
		MESSAGE_END();
		m_flRoundTimeLimit = 0;
		m_fWaitForPlayersTime = gpGlobals->time + 17.0;
	}

	flUpdateTime = gpGlobals->time + 1.0;
}

BOOL CHalfLifeLastManStanding::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	if ( pPlayer->pev->frags <= 0 )
	{
		if ( !pPlayer->m_flForceToObserverTime )
		pPlayer->m_flForceToObserverTime = gpGlobals->time + 3.0;

		return FALSE;
	}

	return TRUE;
}

int CHalfLifeLastManStanding::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return 0;
}

void CHalfLifeLastManStanding::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);

	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );

	if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayer*)ktmp;

	if ( g_GameInProgress )
	{
		// Reduce frags only if killed by another, multiplayer rules handle environment frag reduction.
		if ( pVictim->pev != pKiller && ktmp && ktmp->IsPlayer() )
		{
			pVictim->pev->frags -= 1;
			MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
				WRITE_BYTE( ENTINDEX(pVictim->edict()) );
				WRITE_SHORT( pVictim->pev->frags );
				WRITE_SHORT( pVictim->m_iDeaths );
				WRITE_SHORT( pVictim->m_iRoundWins );
				WRITE_SHORT( GetTeamIndex( pVictim->m_szTeamName ) + 1 );
			MESSAGE_END();
		}

		if ( !pVictim->pev->frags )
		{
			UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %s has been eliminated from the round!\n", STRING(pVictim->pev->netname)));
			MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, pVictim->edict() );
				WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
			MESSAGE_END();
		}
	}
}

BOOL CHalfLifeLastManStanding::AllowRuneSpawn( const char *szRune )
{
	if (!strcmp("rune_frag", szRune))
		return FALSE;

	return TRUE;
}

void CHalfLifeLastManStanding::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);

	// Place player in spectator mode if joining during a game
	// Or if the game begins that requires spectators
	if ((g_GameInProgress && !pPlayer->IsInArena) || (!g_GameInProgress && HasSpectators()))
	{
		return;
	}

	char *key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict());
	strncpy( pPlayer->m_szTeamName, "Active", TEAM_NAME_LENGTH );
	g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "team", "Active");

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
}

int CHalfLifeLastManStanding::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		if (!strcmp(pTeamName, "Active"))
			return 0;
	}
	
	return -1;	// No match
}
