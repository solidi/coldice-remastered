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
extern int gmsgScoreInfo;
extern int gmsgTeamNames;
extern int gmsgTeamInfo;
extern int gmsgSafeSpot;
extern int gmsgDEraser;

#define TEAM_BLUE 0
#define TEAM_RED 1

class CSafeSpot : public CBaseEntity
{
public:
	static CSafeSpot *CreateSafeSpot( Vector vecOrigin, int body );
	void Precache( void );
	void Spawn( void );
	EXPORT void SafeSpotThink( void );
};

CSafeSpot *CSafeSpot::CreateSafeSpot( Vector vecOrigin, int body )
{
	CSafeSpot *pSpot = GetClassPtr( (CSafeSpot *)NULL );
	UTIL_SetOrigin( pSpot->pev, vecOrigin );
	pSpot->pev->angles = g_vecZero;
	pSpot->Spawn();
	pSpot->pev->body = body; // to set specific size requirement by using fuser4 as id.
	return pSpot;
}

void CSafeSpot::Precache( void )
{
	PRECACHE_MODEL("models/coldspot.mdl");
}

void CSafeSpot::Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), "models/coldspot.mdl");
	pev->classname = MAKE_STRING("coldspot");
	pev->fuser4 = 99; // to determine specific model index to size.
	pev->angles.x = 0;
	pev->angles.z = 0;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CSafeSpot::SafeSpotThink );
	pev->nextthink = gpGlobals->time + 2.0;
}

void CSafeSpot::SafeSpotThink( void )
{
	if (!royaledamage.value)
	{
		pev->nextthink = gpGlobals->time + 2.0;
		return;
	}

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr && plr->IsPlayer() && plr->IsInArena && plr->IsAlive() && !plr->HasDisconnected )
		{
			CBaseEntity *ent = NULL;
			BOOL found = FALSE;
			while ( (ent = UTIL_FindEntityInSphere( ent, pev->origin, 256 * pev->body )) != NULL )
			{
				if ( ent == plr )
				{
					found = TRUE;
					continue;
				}
			}

			if (!found)
			{
				entvars_t *pevWorld = VARS( INDEXENT(0) );
				plr->TakeDamage( pevWorld, plr->pev, 10, DMG_SHOCK );
			}
		}
	}

	pev->nextthink = gpGlobals->time + 2.0;
}

LINK_ENTITY_TO_CLASS( safespot, CSafeSpot );

CHalfLifeLastManStanding::CHalfLifeLastManStanding()
{
	UTIL_PrecacheOther("safespot");
	m_iFirstBloodDecided = TRUE; // no first blood award
	m_TeamBased = royaleteam.value;
	pSafeSpot = NULL;
	pLastSpawnPoint = NULL;
	m_DisableDeathPenalty = FALSE;
	m_fSpawnSafeSpot = gpGlobals->time + 2.0;
}

void CHalfLifeLastManStanding::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );	

	if (m_TeamBased)
	{
		CHalfLifeMultiplay::SavePlayerModel(pPlayer);

		MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
			WRITE_BYTE( 2 );
			WRITE_STRING( "blue" );
			WRITE_STRING( "red" );
		MESSAGE_END();

		if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
				WRITE_STRING("Battle Royale");
				WRITE_STRING(UTIL_VarArgs("You're on %s team", (pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
				WRITE_BYTE(0);
				if (roundlimit.value > 0)
					WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
			MESSAGE_END();
		}
	}
	else
	{
		if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
				WRITE_STRING("Battle Royale");
				if (roundlimit.value > 0)
					WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
				else
					WRITE_STRING("");
				WRITE_BYTE(0);
			MESSAGE_END();

			MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
				WRITE_BYTE( 1 );
				WRITE_STRING( "Active" );
			MESSAGE_END();
		}
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

edict_t *CHalfLifeLastManStanding::EntSelectSpawnPoint( const char *szSpawnPoint )
{
	CBaseEntity *pSpot;

	// choose a point
	pSpot = pLastSpawnPoint;
	// Randomize the start spot
	for ( int i = RANDOM_LONG(1,5); i > 0; i-- )
		pSpot = UTIL_FindEntityByClassname( pSpot, szSpawnPoint );
	if ( FNullEnt( pSpot ) )  // skip over the null point
		pSpot = UTIL_FindEntityByClassname( pSpot, szSpawnPoint );

	CBaseEntity *pFirstSpot = pSpot;

	do
	{
		if ( pSpot )
		{
			// check if pSpot is valid
			if ( 1 /*IsSpawnPointValid( pSpot )*/ )
			{
				// if so, go to pSpot
				goto ReturnSpot;
			}
		}
		// increment pSpot
		pSpot = UTIL_FindEntityByClassname( pSpot, szSpawnPoint );
	} while ( pSpot != pFirstSpot ); // loop if we're not back to the start

	// we haven't found a place to spawn yet
	if ( !FNullEnt( pSpot ) )
	{
		goto ReturnSpot;
	}

ReturnSpot:
	if ( FNullEnt( pSpot ) )
	{
		ALERT(at_error, "no safe spot on level");
		return INDEXENT(0);
	}

	pLastSpawnPoint = pSpot;
	return pSpot->edict();
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
		if ( HasGameTimerExpired() )
			return;
	}

	if (m_fSpawnSafeSpot && m_fSpawnSafeSpot < gpGlobals->time)
	{
		edict_t *pentSpawnSpot = EntSelectSpawnPoint("info_player_start");
		if (!pSafeSpot)
			pSafeSpot = CSafeSpot::CreateSafeSpot(pentSpawnSpot->v.origin, 8);
		else
			UTIL_SetOrigin(pSafeSpot->pev, pentSpawnSpot->v.origin);

		UTIL_ClientPrintAll(HUD_PRINTTALK, "[Royale]: The safe spot has appeared!\n");
		m_fSpawnSafeSpot = 0;
	}

//===================================================

	//access this when game is in progress.
	//we are checking for the last man here.

//===================================================

	if ( g_GameInProgress )
	{
		int clients_alive = 0;
		int client_index = 0;
		const char *client_name = "No one";
		int redTeam = 0;
		int blueTeam = 0;

		if (m_fNextShrinkTime <= gpGlobals->time)
		{
			if (pSafeSpot && pSafeSpot->pev->body > 1)
			{
				pSafeSpot->pev->body -= 1;
				MESSAGE_BEGIN(MSG_ALL, gmsgSafeSpot);
					WRITE_BYTE(pSafeSpot->pev->body);
				MESSAGE_END();
				UTIL_ClientPrintAll(HUD_PRINTTALK, "[Royale]: The safe spot has shrunk!\n");
			}
				
			m_fNextShrinkTime = gpGlobals->time + ((roundtimelimit.value * 60) / 15);
		}

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			//player must exist, and must be alive
			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
			{
				if ( plr->m_flForceToObserverTime && plr->m_flForceToObserverTime < gpGlobals->time )
				{
					edict_t *pentSpawnSpot = g_pGameRules->GetPlayerSpawnSpot( plr );
					plr->StartObserver(pentSpawnSpot->v.origin, VARS(pentSpawnSpot)->angles);
					plr->m_flForceToObserverTime = 0;
				}

				//player cannot be disconnected client
				//and is currently in this game of LMS.
				if ( plr->IsInArena && plr->pev->frags > 0 )
				{
					clients_alive++;
					client_index = i;
					client_name = STRING(plr->pev->netname);

					if (m_TeamBased)
						plr->pev->fuser4 == TEAM_RED ? redTeam++ : blueTeam++;
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

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() && !plr->HasDisconnected && !FBitSet(plr->pev->flags, FL_FAKECLIENT) )
			{
				if (clients_alive > 1)
				{
					MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
						WRITE_STRING("Battle Royale");
						if (plr->pev->iuser1)
						{
							if (m_TeamBased)
							{
								WRITE_STRING(UTIL_VarArgs("Red left: %d", redTeam));
								WRITE_BYTE(0);
								WRITE_STRING(UTIL_VarArgs("Blue left: %d", blueTeam));
							}
							else
							{
								WRITE_STRING(UTIL_VarArgs("Players alive: %d", clients_alive));
								WRITE_BYTE(0);
								if (roundlimit.value > 0)
									WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
							}
						}
						else
						{
							if (m_TeamBased)
								WRITE_STRING(UTIL_VarArgs("%s left: %d", plr->pev->fuser4 == TEAM_RED ? "Blue" : " Red", plr->pev->fuser4 == TEAM_RED ? blueTeam : redTeam));
							else
								WRITE_STRING(UTIL_VarArgs("Players alive: %d", clients_alive));
							if (m_TeamBased)
								WRITE_BYTE(0);
							else
								WRITE_BYTE(float(clients_alive) / (m_iPlayersInGame) * 100);
							WRITE_STRING(UTIL_VarArgs("Lives left: %d", (int)plr->pev->frags));
						}
					MESSAGE_END();
				} 
				else
				{
					MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
						WRITE_STRING("Battle Royale");
						WRITE_STRING("");
						WRITE_BYTE(0);
						WRITE_STRING(UTIL_VarArgs("%s is the winner!", client_name));
					MESSAGE_END();
				}
			}
		}

		BOOL victorFound = clients_alive <= 1;
		if (m_TeamBased)
			victorFound = (redTeam == 0 || blueTeam == 0);

		//found victor / or draw.
		if ( victorFound )
		{
			//stop timer / end game.
			m_flRoundTimeLimit = 0;
			g_GameInProgress = FALSE;
			MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
				WRITE_BYTE(0);
			MESSAGE_END();

			if (m_TeamBased)
			{
				int highest = 1;
				BOOL IsEqual = FALSE;
				CBasePlayer *highballer = NULL;
				int type = TEAM_BLUE;

				if (blueTeam == 0)
				{
					type = TEAM_RED;
					UTIL_ClientPrintAll(HUD_PRINTCENTER, "Red team wins!\n");
				}
				else
					UTIL_ClientPrintAll(HUD_PRINTCENTER, "Blue team wins!\n");

				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

					if ( plr && plr->IsPlayer() && plr->IsInArena )
					{
						if ( plr->pev->fuser4 == type )
						{
							plr->m_iRoundWins++;
							plr->Celebrate();
							MESSAGE_BEGIN( MSG_ONE, gmsgPlayClientSound, NULL, plr->edict() );
								WRITE_BYTE(CLIENT_SOUND_LMS);
							MESSAGE_END();
						}
					}
				}
			}
			else
			{
				if ( clients_alive == 1 )
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("%s\nis standing!\n", client_name ));

					CBasePlayer *pl = (CBasePlayer *)UTIL_PlayerByIndex( client_index );
					MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, pl->edict() );
						WRITE_BYTE(CLIENT_SOUND_LMS);
					MESSAGE_END();
					DisplayWinnersGoods( pl );
				}	
				else
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER, "No player is standing!\n");
				}
			}

			m_iSuccessfulRounds++;
			flUpdateTime = gpGlobals->time + 3.0;
			return;
		}

		flUpdateTime = gpGlobals->time + 1.5;
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
			if (m_iCountDown == 3) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_PICKUPYOURWEAPON);
				MESSAGE_END();
			}
			UTIL_ClientPrintAll(HUD_PRINTCENTER,
				UTIL_VarArgs("Prepare for Battle Royale\n\n%i...\n", m_iCountDown));
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

		m_iCountDown = 5;
		m_fWaitForPlayersTime = -1;

		// spot stuff
		if (pSafeSpot)
		{
			pSafeSpot->pev->body = 8;
			MESSAGE_BEGIN(MSG_ALL, gmsgSafeSpot);
				WRITE_BYTE(pSafeSpot->pev->body);
			MESSAGE_END();
			m_fNextShrinkTime = gpGlobals->time + ((roundtimelimit.value * 60) / 15);
		}

		// Resend team info
		if (!m_TeamBased)
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgTeamNames );
				WRITE_BYTE( 1 );
				WRITE_STRING( "Active" );
			MESSAGE_END();
		}

		UTIL_ClientPrintAll(HUD_PRINTCENTER, "Battle Royale has begun!\n");
	}
	else
	{
		SuckAllToSpectator();
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("Battle Royale");
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

BOOL CHalfLifeLastManStanding::HasGameTimerExpired( void )
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
						if (m_TeamBased && highballer->pev->fuser4 != plr->pev->fuser4)
						{
							IsEqual = TRUE;
							continue;
						}
						else
						{
							IsEqual = TRUE;
							continue;
						}
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
	if ( m_DisableDeathPenalty )
		return;

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
	if ((g_GameInProgress && !pPlayer->IsInArena) || (!g_GameInProgress && IsRoundBased()))
	{
		return;
	}

	if (pPlayer->m_iExitObserver)
	{
		if (m_TeamBased)
		{
			int blueteam = 0, redteam = 0;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
				if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
				{
					if (plr->pev->fuser4 == TEAM_BLUE)
						blueteam++;
					else
						redteam++;
				}
			}

			ALERT(at_aiconsole, "blueteam=%d, redteam=%d\n", blueteam, redteam);
			pPlayer->pev->fuser4 = redteam >= blueteam ? TEAM_BLUE : TEAM_RED;

			if (pPlayer->pev->fuser4 == TEAM_BLUE)
			{
				strncpy( pPlayer->m_szTeamName, "blue", TEAM_NAME_LENGTH );
				g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()),
					g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "iceman");
			}
			else
			{
				strncpy( pPlayer->m_szTeamName, "red", TEAM_NAME_LENGTH );
				g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()),
					g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "santa");
			}

			char text[256];
			sprintf( text, "[Royale]: You're on team \'%s\'\n", pPlayer->m_szTeamName );
			UTIL_SayText( text, pPlayer );
		}
		else
		{
			strncpy( pPlayer->m_szTeamName, "Active", TEAM_NAME_LENGTH );
		}

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
}

int CHalfLifeLastManStanding::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		if (m_TeamBased)
		{
			if (!strcmp(pTeamName, "red"))
				return TEAM_RED;
			else
				return TEAM_BLUE;
		}
		else
		{
			if (!strcmp(pTeamName, "Active"))
				return 0;
		}
	}
	
	return -1;	// No match
}

BOOL CHalfLifeLastManStanding::IsRoundBased( void )
{
	return TRUE;
}

int CHalfLifeLastManStanding::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if ( !pPlayer || (pTarget && !pTarget->IsPlayer()) )
		return GR_NOTTEAMMATE;

	if (!m_TeamBased)
		return GR_NOTTEAMMATE;

	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && 
			!stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

const char *CHalfLifeLastManStanding::GetTeamID( CBaseEntity *pEntity )
{
	if ( pEntity == NULL || pEntity->pev == NULL )
		return "";

	// return their team name
	return pEntity->TeamID();
}

extern DLL_GLOBAL BOOL g_fGameOver;

void CHalfLifeLastManStanding::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	if ( !m_TeamBased )
		return;

	// prevent skin/color/model changes
	char text[1024];
	char *mdls = g_engfuncs.pfnInfoKeyValue( infobuffer, "model" );
	int clientIndex = pPlayer->entindex();

	// Spectator
	if ( !pPlayer->m_szTeamName || !strlen(pPlayer->m_szTeamName) )
		return;

	// Ignore ctf on model changing back.
	if ( g_fGameOver )
		return;

	// prevent skin/color/model changes
	if ( !stricmp( "red", pPlayer->m_szTeamName ) && !stricmp( "santa", mdls ) )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "[Royale]: You're on team '%s' To change, type 'model iceman'\n", pPlayer->m_szTeamName );
		return;
	}
	if ( !stricmp( "blue", pPlayer->m_szTeamName ) && !stricmp( "iceman", mdls ) )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "[Royale]: You're on team '%s' To change, type 'model santa'\n", pPlayer->m_szTeamName );
		return;
	}

	if ( stricmp( mdls, "iceman" ) && stricmp( mdls, "santa" ) )
	{
		g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", (char *)(pPlayer->pev->fuser4 == TEAM_RED ? "santa" : "iceman") );
		sprintf( text, "* Can't change team to \'%s\'\n", mdls );
		UTIL_SayText( text, pPlayer );
		sprintf( text, "* Server limits teams to \'%s\'\n", "iceman (blue), santa (red)" );
		UTIL_SayText( text, pPlayer );
		return;
	}

	m_DisableDeathPenalty = TRUE;

	ClearMultiDamage();
	pPlayer->pev->health = 0; // without this, player can walk as a ghost.
	pPlayer->Killed(pPlayer->pev, VARS(INDEXENT(0)), GIB_ALWAYS);

	m_DisableDeathPenalty = FALSE;

	int id = TEAM_BLUE;
	if ( !stricmp( mdls, "santa" ) )
		id = TEAM_RED;
	pPlayer->pev->fuser4 = id;

	if (pPlayer->pev->fuser4 == TEAM_RED)
	{
		strncpy( pPlayer->m_szTeamName, "red", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "santa" );
	}
	else
	{
		strncpy( pPlayer->m_szTeamName, "blue", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "iceman" );
	}

	// notify everyone of the team change
	sprintf( text, "[Royale]: %s has changed to team \'%s\'\n", STRING(pPlayer->pev->netname), pPlayer->m_szTeamName );
	UTIL_SayTextAll( text, pPlayer );

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

	MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
		WRITE_STRING("Battle Royale");
		WRITE_STRING(UTIL_VarArgs("You're on %s team", (pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
		WRITE_BYTE(0);
		if (roundlimit.value > 0)
			WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
	MESSAGE_END();
}

BOOL CHalfLifeLastManStanding::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
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

BOOL CHalfLifeLastManStanding::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if ( pAttacker && PlayerRelationship( pPlayer, pAttacker ) == GR_TEAMMATE )
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
