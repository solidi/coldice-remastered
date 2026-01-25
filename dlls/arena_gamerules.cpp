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
#include "arena_gamerules.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"

extern int gmsgShowTimer;
extern int gmsgStatusText;
extern int gmsgObjective;
extern int gmsgPlayClientSound;
extern int gmsgScoreInfo;
extern int gmsgTeamNames;
extern int gmsgTeamInfo;
extern int gmsgDEraser;

CHalfLifeArena::CHalfLifeArena()
{
	m_iFirstBloodDecided = TRUE; // no first blood award
	PauseMutators();
}

void CHalfLifeArena::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("1 vs. 1");
			if (roundlimit.value > 0)
				WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
			else
				WRITE_STRING("");
			WRITE_BYTE(0);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
			WRITE_BYTE( 2 );
			WRITE_STRING( "blue" );
			WRITE_STRING( "red" );
		MESSAGE_END();
	}

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
}

void CHalfLifeArena::Think( void )
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
		if ( HasGameTimerExpired() )
			return;
	}

	if ( g_GameInProgress )
	{
		CBasePlayer *pPlayer1 = (CBasePlayer *)UTIL_PlayerByIndex( m_iPlayer1 );
		CBasePlayer *pPlayer2 = (CBasePlayer *)UTIL_PlayerByIndex( m_iPlayer2 );

		// when a player disconnects...
		bool p1Disconnected = !pPlayer1 || pPlayer1->HasDisconnected;
		bool p2Disconnected = !pPlayer2 || pPlayer2->HasDisconnected;

		if ( p1Disconnected || p2Disconnected )
		{
			//stop timer / end game.
			m_flRoundTimeLimit = 0;
			g_GameInProgress = FALSE;
			PauseMutators();
			MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
				WRITE_BYTE(0);
			MESSAGE_END();

			if (p1Disconnected && p2Disconnected)
			{
				UTIL_ClientPrintAll(HUD_PRINTCENTER,
					UTIL_VarArgs("No victors, starting another round.\n"));
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
				MESSAGE_END();
			}
			else
			{
				if ( p1Disconnected )
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER,
						UTIL_VarArgs("%s is the victor!\n",
						STRING(pPlayer2->pev->netname)));
					MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
						WRITE_STRING("1 vs. 1");
						WRITE_STRING("");
						WRITE_BYTE(0);
						WRITE_STRING(UTIL_VarArgs("%s is the victor!\n", STRING(pPlayer2->pev->netname)));
					MESSAGE_END();
					DisplayWinnersGoods( pPlayer2 );
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
					MESSAGE_END();
				}
				else
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER,
						UTIL_VarArgs("%s is the victor!\n",
						STRING(pPlayer1->pev->netname)));
					MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
						WRITE_STRING("1 vs. 1");
						WRITE_STRING("");
						WRITE_BYTE(0);
						WRITE_STRING(UTIL_VarArgs("%s is the victor!\n", STRING(pPlayer1->pev->netname)));
					MESSAGE_END();
					DisplayWinnersGoods( pPlayer1 );
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
					MESSAGE_END();
				}
			}

			m_iSuccessfulRounds++;
			flUpdateTime = gpGlobals->time + 3.0;
			return;
		}

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			//player must exist
			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
			{
				if (plr == pPlayer1 || plr == pPlayer2)
				{
					char message[64];
					if (plr == pPlayer1)
						sprintf(message, "Fighting %s", STRING(pPlayer2->pev->netname));
					else
						sprintf(message, "Fighting %s", STRING(pPlayer1->pev->netname));

					if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
					{
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgStatusText, NULL, plr->edict() );
							WRITE_BYTE( 0 );
							WRITE_BYTE( ENTINDEX(plr->edict()) );
							WRITE_STRING( message );
						MESSAGE_END();
					}
				}

				// Force spectate on those that died.
				if ( plr != pPlayer1 && plr != pPlayer2 &&
					plr->m_flForceToObserverTime && plr->m_flForceToObserverTime < gpGlobals->time )
				{
					SuckToSpectator( plr );
					plr->m_flForceToObserverTime = 0;
				}

				// is currently in this game of arena.
				// and frags are >= set server value.
				if ( plr->IsInArena && plr->pev->frags >= roundfraglimit.value )
				{
					//stop timer / end game.
					m_flRoundTimeLimit = 0;
					g_GameInProgress = FALSE;
					PauseMutators();
					MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
						WRITE_BYTE(0);
					MESSAGE_END();

					MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective );
						WRITE_STRING("Arena finished");
						WRITE_STRING("");
						WRITE_BYTE(0);
						WRITE_STRING(UTIL_VarArgs("%s is the victor!\n", STRING(plr->pev->netname)));
					MESSAGE_END();

					DisplayWinnersGoods( plr );
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
					MESSAGE_END();

					m_iSuccessfulRounds++;
					flUpdateTime = gpGlobals->time + 3.0;
					return;
				}
				else
				{
					//for clients who connected while game in progress.
					if ( plr->IsSpectator() )
					{
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict() );
							WRITE_STRING(UTIL_VarArgs("1 vs. 1: Round %d of %.0f", m_iSuccessfulRounds + 1, roundlimit.value ));
							WRITE_STRING(UTIL_VarArgs("%s (%.0f/%.0f) vs. %s (%.0f/%.0f)\n",
							STRING(pPlayer1->pev->netname),
							pPlayer1->pev->health,
							pPlayer1->pev->armorvalue,
							STRING(pPlayer2->pev->netname),
							pPlayer2->pev->health,
							pPlayer2->pev->armorvalue));
							WRITE_BYTE(0);
						MESSAGE_END();
					} else {
						// Send them to observer
						if (!plr->IsInArena)
							plr->m_flForceToObserverTime = gpGlobals->time;
					}
				}
			}
		}

		flUpdateTime = gpGlobals->time + 1.5;
		return;
	}

//=======================================================
// execute below if we are waiting for players to join
//=======================================================

	int clients = CheckClients();

#ifdef _DEBUG
	ALERT( at_notice, UTIL_VarArgs("CheckClients(): %i\n", clients ));
#endif

	if ( clients > 1 )
	{
		if ( m_fWaitForPlayersTime == -1 )
		{
			m_fWaitForPlayersTime = gpGlobals->time + roundwaittime.value;
			RemoveAndFillItems();
			extern void ClearBodyQue();
			ClearBodyQue();
			MESSAGE_BEGIN( MSG_ALL, gmsgDEraser );
			MESSAGE_END();
		}

		if ( m_fWaitForPlayersTime > gpGlobals->time )
		{
			SuckAllToSpectator();
			flUpdateTime = gpGlobals->time + 1.0;
			UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("Battle will begin in %.0f\n", (m_fWaitForPlayersTime + 5) - gpGlobals->time));
			return;
		}

		if ( m_iCountDown > 0 )
		{
			if (m_iCountDown == 2) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_PREPARETOFIGHT);
				MESSAGE_END();
			} else if (m_iCountDown == 3) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_THREE);
				MESSAGE_END();
			} else if (m_iCountDown == 4) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_FOUR);
				MESSAGE_END();
			} else if (m_iCountDown == 5) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_FIVE);
				MESSAGE_END();
			}
			SuckAllToSpectator(); // in case players join during a countdown.
			UTIL_ClientPrintAll(HUD_PRINTCENTER,
				UTIL_VarArgs("Prepare for Arena battle\n\n%i...\n", m_iCountDown));
			m_iCountDown--;
			flUpdateTime = gpGlobals->time + 1.0;
			return;
		}

		ALERT(at_console, "Players in Arena: ");

		m_iPlayer1 = m_iPlayersInArena[RANDOM_LONG( 0, clients-1 )];
		m_iPlayer2 = m_iPlayersInArena[RANDOM_LONG( 0, clients-1 )];

		while ( m_iPlayer1 == m_iPlayer2 )
		{
			m_iPlayer2 = RANDOM_LONG( 1, m_iPlayersInArena[RANDOM_LONG( 0, clients-1 )] );
		}

#ifdef _DEBUG
		ALERT( at_notice,
			UTIL_VarArgs("player1: %i | player2: %i \n",
			m_iPlayer1, m_iPlayer2 ));
#endif

		CBasePlayer *pPlayer1 = (CBasePlayer *)UTIL_PlayerByIndex( m_iPlayer1 );
		CBasePlayer *pPlayer2 = (CBasePlayer *)UTIL_PlayerByIndex( m_iPlayer2 );

		char *key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer1->edict());
		strncpy( pPlayer1->m_szTeamName, "blue", TEAM_NAME_LENGTH );
		//g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer1->edict()), key, "team", pPlayer1->m_szTeamName);

		key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer2->edict());
		strncpy( pPlayer2->m_szTeamName, "red", TEAM_NAME_LENGTH );
		//g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer2->edict()), key, "team", pPlayer2->m_szTeamName);

		// notify everyone's HUD of the team change
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
			WRITE_BYTE( ENTINDEX(pPlayer1->edict()) );
			WRITE_STRING( pPlayer1->m_szTeamName );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
			WRITE_BYTE( ENTINDEX(pPlayer2->edict()) );
			WRITE_STRING( pPlayer2->m_szTeamName );
		MESSAGE_END();

		//frags + time
		SetRoundLimits();

		g_GameInProgress = TRUE;
		
		// Restore mutators when round begins
		RestoreMutators();

		//Should really be using InsertClientsIntoArena...
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
			{ 
				if ( m_iPlayer1 == i || m_iPlayer2 == i )
				{
					MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
						WRITE_BYTE( ENTINDEX(plr->edict()) );
						WRITE_SHORT( plr->pev->frags = 0 );
						WRITE_SHORT( plr->m_iDeaths = 0 );
						WRITE_SHORT( plr->m_iRoundWins );
						WRITE_SHORT( g_pGameRules->GetTeamIndex( plr->m_szTeamName ) + 1 );
					MESSAGE_END();
					plr->m_iAssists = 0;

					ALERT(at_console, "| %s ", STRING(plr->pev->netname) );

					plr->IsInArena = TRUE;
					plr->ExitObserver();
					plr->m_iRoundPlays++;
				}
				else
				{
					if ( !plr->IsSpectator() )
					{
						//just incase player played previous round
						plr->m_iAssists = 0;
						SuckToSpectator( plr );
					}
				}
			}
		}

		ALERT(at_console, "\n");

		m_iCountDown = 5;
		m_fWaitForPlayersTime = -1;

		UTIL_ClientPrintAll(HUD_PRINTCENTER,
			UTIL_VarArgs("Arena has begun!\n\n%s Vs. %s",
			STRING(pPlayer1->pev->netname),
			STRING(pPlayer2->pev->netname)));

		if (!FBitSet(pPlayer1->pev->flags, FL_FAKECLIENT))
		{
			MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer1->edict());
				WRITE_STRING(UTIL_VarArgs("Defeat %s", STRING(pPlayer2->pev->netname)));
				WRITE_STRING(UTIL_VarArgs("Frags to go: %d", int(roundfraglimit.value - pPlayer1->pev->frags)));
				WRITE_BYTE(0);
				if (roundlimit.value > 0)
					WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
			MESSAGE_END();
		}

		if (!FBitSet(pPlayer2->pev->flags, FL_FAKECLIENT))
		{
			MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer2->edict());
				WRITE_STRING(UTIL_VarArgs("Defeat %s", STRING(pPlayer1->pev->netname)));
				WRITE_STRING(UTIL_VarArgs("Frags to go: %d", int(roundfraglimit.value - pPlayer2->pev->frags)));
				WRITE_BYTE(0);
				if (roundlimit.value > 0)
					WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
			MESSAGE_END();
		}
	}
	else
	{
		SuckAllToSpectator();
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("1 vs. 1");
			WRITE_STRING("Waiting for other players");
			WRITE_BYTE(0);
			if (roundlimit.value > 0)
				WRITE_STRING(UTIL_VarArgs("%d Rounds", (int)roundlimit.value));
		MESSAGE_END();
		m_flRoundTimeLimit = 0;
		m_fWaitForPlayersTime = gpGlobals->time + roundwaittime.value;
	}

	flUpdateTime = gpGlobals->time + 1.0;
}


BOOL CHalfLifeArena::HasGameTimerExpired( void )
{
	//time is up
	if ( CHalfLifeMultiplay::HasGameTimerExpired() )
	{
		int highest = 1;
		BOOL IsEqual = FALSE;
		CBasePlayer *highballer = NULL;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() && plr->IsInArena )
			{
				if ( highest <= plr->pev->frags )
				{
					if ( highballer && highest == plr->pev->frags )
					{
						IsEqual = TRUE;
						continue;
					}

					IsEqual = FALSE;
					highest = plr->pev->frags;
					highballer = plr;
				}
			}
		}

		if ( !IsEqual && highballer )
		{
			DisplayWinnersGoods( highballer );
			UTIL_ClientPrintAll(HUD_PRINTCENTER,
				UTIL_VarArgs("Time is Up: %s is the Victor!\n", STRING(highballer->pev->netname)));

			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING("Time is up!");
				WRITE_STRING("");
				WRITE_BYTE(0);
				WRITE_STRING(UTIL_VarArgs("%s is the victor!\n", STRING(highballer->pev->netname)));
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
			MESSAGE_END();
		}
		else
		{
			UTIL_ClientPrintAll(HUD_PRINTCENTER, "Time is Up: Match ends in a draw!\n" );
			UTIL_ClientPrintAll(HUD_PRINTTALK, "* No winners in this round!\n");

			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING("Time is up!");
				WRITE_STRING("");
				WRITE_BYTE(0);
				WRITE_STRING("Match ends in a draw!");
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
			MESSAGE_END();
		}

		g_GameInProgress = FALSE;
		MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
			WRITE_BYTE(0);
		MESSAGE_END();

		m_iSuccessfulRounds++;
		flUpdateTime = gpGlobals->time + 3.0;
		m_flRoundTimeLimit = 0;
		return TRUE;
	}

	return FALSE;
}

BOOL CHalfLifeArena::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	if ( !g_GameInProgress )
		return FALSE; 

	CBasePlayer *pPlayer1 = (CBasePlayer *)UTIL_PlayerByIndex( m_iPlayer1 );
	CBasePlayer *pPlayer2 = (CBasePlayer *)UTIL_PlayerByIndex( m_iPlayer2 );

	if (pPlayer1 && pPlayer1 == pPlayer)
		return TRUE;
	if (pPlayer2 && pPlayer2 == pPlayer)
		return TRUE;

	if ( !pPlayer->m_flForceToObserverTime )
		pPlayer->m_flForceToObserverTime = gpGlobals->time + 3.0;

	return FALSE;
}

void CHalfLifeArena::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);

	if ( g_GameInProgress )
	{
		CBasePlayer *pPlayer1 = (CBasePlayer *)UTIL_PlayerByIndex( m_iPlayer1 );
		CBasePlayer *pPlayer2 = (CBasePlayer *)UTIL_PlayerByIndex( m_iPlayer2 );

		if (pPlayer1 && pPlayer2)
		{
			int fragsToGo = int(roundfraglimit.value - pPlayer1->pev->frags);
			MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer1->edict());
				if (fragsToGo >= 1)
					WRITE_STRING(UTIL_VarArgs("Defeat %s", STRING(pPlayer2->pev->netname)));
				else
					WRITE_STRING(UTIL_VarArgs("You Defeated %s!", STRING(pPlayer2->pev->netname)));
				if (fragsToGo >= 1)
					WRITE_STRING(UTIL_VarArgs("They need: %d", int(roundfraglimit.value - pPlayer2->pev->frags)));
				else
					WRITE_STRING("");
				WRITE_BYTE(fmax(0, (pPlayer1->pev->frags / roundfraglimit.value) * 100));
				if (fragsToGo < 1)
					WRITE_STRING("You are the WINNER!");
				else
					WRITE_STRING(UTIL_VarArgs("You need: %d", fragsToGo));
			MESSAGE_END();

			fragsToGo = int(roundfraglimit.value - pPlayer2->pev->frags);
			MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer2->edict());
				if (fragsToGo >= 1)
					WRITE_STRING(UTIL_VarArgs("Defeat %s", STRING(pPlayer1->pev->netname)));
				else
					WRITE_STRING(UTIL_VarArgs("You Defeated %s!", STRING(pPlayer1->pev->netname)));
				if (fragsToGo >= 1)
					WRITE_STRING(UTIL_VarArgs("They need: %d", int(roundfraglimit.value - pPlayer1->pev->frags)));
				else
					WRITE_STRING("");
				WRITE_BYTE(fmax(0, (pPlayer2->pev->frags / roundfraglimit.value) * 100));
				if (fragsToGo < 1)
					WRITE_STRING("You are the WINNER!");
				else
					WRITE_STRING(UTIL_VarArgs("You need: %d", fragsToGo));
			MESSAGE_END();
		}
	}
}

int CHalfLifeArena::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		if (!strcmp(pTeamName, "red"))
			return 1;
		else
			return 0;
	}
	
	return -1;	// No match
}

const char *CHalfLifeArena::GetTeamID( CBaseEntity *pEntity )
{
	if ( pEntity == NULL || pEntity->pev == NULL )
		return "";

	// return their team name
	return pEntity->TeamID();
}

BOOL CHalfLifeArena::IsRoundBased( void )
{
	return TRUE;
}
