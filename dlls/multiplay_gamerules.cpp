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
#include	"game.h"
#include	"items.h"
#include	"voice_gamemgr.h"
#include	"hltv.h"
#include	"shake.h"

#if !defined ( _WIN32 )
#include <ctype.h>
#endif

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;

extern DLL_GLOBAL const char *g_MutatorInstaGib;
extern DLL_GLOBAL const char *g_MutatorPlumber;
extern DLL_GLOBAL const char *g_MutatorPaintball;
extern DLL_GLOBAL const char *g_MutatorSuperJump;
extern DLL_GLOBAL const char *g_MutatorLightsOut;
extern DLL_GLOBAL const char *g_MutatorLoopback;
extern DLL_GLOBAL const char *g_MutatorMaxPack;
extern DLL_GLOBAL const char *g_MutatorPushy;

extern int gmsgDeathMsg;	// client dll messages
extern int gmsgScoreInfo;
extern int gmsgMOTD;
extern int gmsgServerName;
extern int gmsgStatusIcon;

extern DLL_GLOBAL int g_GameMode;
extern int gmsgPlayClientSound;

extern int g_teamplay;

#define ITEM_RESPAWN_TIME	30
#define WEAPON_RESPAWN_TIME	20
#define AMMO_RESPAWN_TIME	20

float g_flIntermissionStartTime = 0;

CVoiceGameMgr	g_VoiceGameMgr;

#ifdef _DEBUG
DLL_GLOBAL float g_iKickSomeone;
#endif

class CMultiplayGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	virtual bool		CanPlayerHearPlayer(CBasePlayer *pListener, CBasePlayer *pTalker)
	{
		if ( g_teamplay )
		{
			if ( g_pGameRules->PlayerRelationship( pListener, pTalker ) != GR_TEAMMATE )
			{
				return false;
			}
		}

		return true;
	}
};
static CMultiplayGameMgrHelper g_GameMgrHelper;

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************

CHalfLifeMultiplay :: CHalfLifeMultiplay()
{
	g_VoiceGameMgr.Init(&g_GameMgrHelper, gpGlobals->maxClients);

	RefreshSkillData();
	m_flIntermissionEndTime = 0;
	m_iFirstBloodDecided = FALSE;
	g_flIntermissionStartTime = 0;
	
	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that 
	//  server ops can run multiple game servers, with different server .cfg files,
	//  from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)
	if ( IS_DEDICATED_SERVER() )
	{
		// this code has been moved into engine, to only run server.cfg once
	}
	else
	{
		// listen server
		char *lservercfgfile = (char *)CVAR_GET_STRING( "lservercfgfile" );

		if ( lservercfgfile && lservercfgfile[0] )
		{
			char szCommand[256];
			
			ALERT( at_console, "Executing listen server config file\n" );
			sprintf( szCommand, "exec %s\n", lservercfgfile );
			SERVER_COMMAND( szCommand );
		}
	}

	ResetGameMode();

#ifdef _DEBUG
	g_iKickSomeone = gpGlobals->time + debug_disconnects_time.value;
#endif
}

BOOL CHalfLifeMultiplay::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if(g_VoiceGameMgr.ClientCommand(pPlayer, pcmd))
		return TRUE;

	return CGameRules::ClientCommand(pPlayer, pcmd);
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::RefreshSkillData( void )
{
// load all default values
	CGameRules::RefreshSkillData();

// override some values for multiplay.

	// Snowball
	if (snowballfight.value)
		gSkillData.plrDmgSnowball = 250;
	
	if (strstr(mutators.string, g_MutatorInstaGib) ||
		atoi(mutators.string) == MUTATOR_INSTAGIB)
		gSkillData.plrDmgRailgun = 800;

	if (strstr(mutators.string, g_MutatorPaintball) ||
		atoi(mutators.string) == MUTATOR_PAINTBALL)
	{
		float multiplier = 0.25;
		gSkillData.plrDmg9MM *= multiplier;
		gSkillData.plrDmg357 *= multiplier;
		gSkillData.plrDmgSniperRifle *= multiplier;
		gSkillData.plrDmgMP5 *= multiplier;
		gSkillData.plrDmgM203Grenade *= multiplier;
		gSkillData.plrDmgBuckshot *= multiplier;
		gSkillData.plrDmgExpBuckshot *= multiplier;
		gSkillData.plrDmgCrossbowClient *= multiplier;
		gSkillData.plrDmgRPG *= multiplier;
		gSkillData.plrDmgGauss *= multiplier;
		gSkillData.plrDmgEgonNarrow *= multiplier;
		gSkillData.plrDmgEgonWide *= multiplier;
		gSkillData.plrDmgHandGrenade *= multiplier;
		gSkillData.plrDmgSatchel *= multiplier;
		gSkillData.plrDmgTripmine *= multiplier;
		gSkillData.plrDmgVest *= multiplier;
		gSkillData.plrDmgClusterGrenade *= multiplier;
		gSkillData.plrDmgRailgun *= multiplier;
		gSkillData.plrDmgFlak *= multiplier;
		gSkillData.plrDmgFlakBomb *= multiplier;
		gSkillData.plrDmgPlasma *= multiplier;
	}

	if (strstr(mutators.string, g_MutatorPushy) ||
		atoi(mutators.string) == MUTATOR_PUSHY)
	{
		float multiplier = 0.10;
		gSkillData.plrDmgRPG *= multiplier;
	}
}

// longest the intermission can last, in seconds
#define MAX_INTERMISSION_TIME		120

extern cvar_t timeleft, fragsleft, roundtimeleft;

extern cvar_t mp_chattime;

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: Think ( void )
{
	g_VoiceGameMgr.Update(gpGlobals->frametime);

	g_pGameRules->CheckMutators();
	g_pGameRules->CheckGameMode();

	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining = 0;

	if ( g_fGameOver )   // someone else quit the game already
	{
		// bounds check
		int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
		if ( time < 1 )
			CVAR_SET_STRING( "mp_chattime", "1" );
		else if ( time > MAX_INTERMISSION_TIME )
			CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

		m_flIntermissionEndTime = g_flIntermissionStartTime + mp_chattime.value;

		// check to see if we should change levels now
		if ( m_flIntermissionEndTime < gpGlobals->time )
		{
			if ( m_iEndIntermissionButtonHit  // check that someone has pressed a key, or the max intermission time is over
				|| ( ( g_flIntermissionStartTime + MAX_INTERMISSION_TIME ) < gpGlobals->time) ) 
				ChangeLevel(); // intermission is over
		}

		return;
	}

	float flTimeLimit = timelimit.value * 60;
	float flFragLimit = fraglimit.value;

	time_remaining = (int)(flTimeLimit ? ( flTimeLimit - gpGlobals->time ) : 0);
	
	if ( flTimeLimit != 0 && gpGlobals->time >= flTimeLimit )
	{
		GoToIntermission();
		return;
	}

	if ( flFragLimit )
	{
		int bestfrags = 9999;
		int remain;

		// check if any player is over the frag limit
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

			if ( pPlayer && pPlayer->pev->frags >= flFragLimit )
			{
				GoToIntermission();
				return;
			}


			if ( pPlayer )
			{
				remain = flFragLimit - pPlayer->pev->frags;
				if ( remain < bestfrags )
				{
					bestfrags = remain;
				}
			}

		}
		frags_remaining = bestfrags;
	}

	// Updates when frags change
	if ( frags_remaining != last_frags )
	{
		g_engfuncs.pfnCvar_DirectSet( &fragsleft, UTIL_VarArgs( "%i", frags_remaining ) );
	}

	// Updates once per second
	if ( timeleft.value != last_time )
	{
		g_engfuncs.pfnCvar_DirectSet( &timeleft, UTIL_VarArgs( "%i", time_remaining ) );
	}

	last_frags = frags_remaining;
	last_time  = time_remaining;

	switch ( g_GameMode )
	{
		case GAME_LMS:
			LastManStanding();
			break;
		case GAME_ARENA:
			Arena();
			break;
	}

#ifdef _DEBUG
	if (NUMBER_OF_ENTITIES() > 1024)
		ALERT(at_console, "NUMBER_OF_ENTITIES(): %d | gpGlobals->maxEntities: %d\n", NUMBER_OF_ENTITIES(), gpGlobals->maxEntities);
#endif
}

void CHalfLifeMultiplay::LastManStanding( void )
{
	if ( flUpdateTime > gpGlobals->time )
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
						ClientPrint(plr->pev, HUD_PRINTCENTER, "LMS round in progress.\n");
					else {
						// Send them to observer
						if (!plr->IsInArena)
							plr->m_flForceToObserverTime = gpGlobals->time;
					}
				}
			}
		}

		//found victor / or draw.
		if ( clients_alive <= 1 )
		{
			//stop timer / end game.
			m_flRoundTimeLimit = 0;
			g_GameInProgress = FALSE;

			if ( clients_alive == 1 )
			{
				UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("%s is the last man standing!\n", client_name ));

				CheckRounds();

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

			flUpdateTime = gpGlobals->time + 5.0;
			return;
		}

		flUpdateTime = gpGlobals->time;
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

		//frags + time.
		SetRoundLimits();
		InsertClientsIntoArena();
		ALERT(at_console, "\n");

		m_iCountDown = 3;
		
		g_GameInProgress = TRUE;
		UTIL_ClientPrintAll(HUD_PRINTCENTER, "Last man standing has begun!\n");
	}
	else
	{
		SuckAllToSpectator();
		m_flRoundTimeLimit = 0;
		UTIL_ClientPrintAll(HUD_PRINTCENTER, "Waiting for other players to begin\n\n'LMS'\n");
	}

	flUpdateTime = gpGlobals->time + 5.0;
}

extern int gmsgStatusText;

void CHalfLifeMultiplay::Arena ( void )
{
	if ( flUpdateTime > gpGlobals->time )
		return;

	if ( m_flRoundTimeLimit )
	{
		if ( CheckGameTimer() )
			return;
	}

	if ( g_GameInProgress )
	{
		CBasePlayer *pPlayer1 = (CBasePlayer *)UTIL_PlayerByIndex( m_iPlayer1 );
		CBasePlayer *pPlayer2 = (CBasePlayer *)UTIL_PlayerByIndex( m_iPlayer2 );

		// when a player disconnects...
		if ( pPlayer1 == NULL || pPlayer2 == NULL ||
			pPlayer1->HasDisconnected || pPlayer2->HasDisconnected )
		{
			//stop timer / end game.
			m_flRoundTimeLimit = 0;
			g_GameInProgress = FALSE;

			CheckRounds();

			if (pPlayer1->HasDisconnected && pPlayer1->HasDisconnected)
			{
				UTIL_ClientPrintAll(HUD_PRINTCENTER,
					UTIL_VarArgs("No victors, starting another round.\n"));
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
				MESSAGE_END();
			}
			else
			{
				if ( pPlayer1->HasDisconnected )
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER,
						UTIL_VarArgs("%s is the victor!\n",
						STRING(pPlayer2->pev->netname)));
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
					DisplayWinnersGoods( pPlayer1 );
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
					MESSAGE_END();
				}
			}

			flUpdateTime = gpGlobals->time + 5.0;
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

					MESSAGE_BEGIN( MSG_ONE, gmsgStatusText, NULL, plr->edict() );
						WRITE_BYTE( 0 );
						WRITE_STRING( message );
					MESSAGE_END();
				}

				// Force spectate on those that died.
				if ( plr != pPlayer1 && plr != pPlayer2 &&
					plr->m_flForceToObserverTime && plr->m_flForceToObserverTime < gpGlobals->time )
				{
					edict_t *pentSpawnSpot = g_pGameRules->GetPlayerSpawnSpot( plr );
					plr->StartObserver(plr->pev->origin, VARS(pentSpawnSpot)->angles);
					plr->m_flForceToObserverTime = 0;
				}

				// is currently in this game of arena.
				// and frags are >= set server value.
				if ( plr->IsInArena && plr->pev->frags >= roundfraglimit.value )
				{
					//stop timer / end game.
					m_flRoundTimeLimit = 0;
					g_GameInProgress = FALSE;

					UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("%s is the victor!\n", STRING(plr->pev->netname)/*client_name*/ ));

					CheckRounds();

					DisplayWinnersGoods( plr );
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
					MESSAGE_END();
					flUpdateTime = gpGlobals->time + 5.0;
					return;
				}
				else
				{
					//for clients who connected while game in progress.
					if ( plr->IsSpectator() )
						ClientPrint(plr->pev, HUD_PRINTCENTER, 
							UTIL_VarArgs("Arena in progress\n%s (%.0f/%.0f)\nVs.\n%s (%.0f/%.0f)\n",
							STRING(pPlayer1->pev->netname),
							pPlayer1->pev->health,
							pPlayer1->pev->armorvalue,
							STRING(pPlayer2->pev->netname),
							pPlayer2->pev->health,
							pPlayer2->pev->armorvalue));
					else {
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

	ALERT( at_notice, UTIL_VarArgs("CheckClients(): %i\n", clients ));

	if ( clients > 1 )
	{
		if ( m_iCountDown > 0 )
		{
			if (m_iCountDown == 2) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_PREPARETOFIGHT);
				MESSAGE_END();
				RemoveItemsThatDamage();
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

		ALERT( at_notice,
			UTIL_VarArgs("player1: %i | player2: %i \n",
			m_iPlayer1, m_iPlayer2 ));

		CBasePlayer *pPlayer1 = (CBasePlayer *)UTIL_PlayerByIndex( m_iPlayer1 );
		CBasePlayer *pPlayer2 = (CBasePlayer *)UTIL_PlayerByIndex( m_iPlayer2 );

		//frags + time
		SetRoundLimits();

		//Should really be using InsertClientsIntoArena...
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() )
			{ 
				if ( m_iPlayer1 == i || m_iPlayer2 == i )
				{
					MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
					WRITE_BYTE( ENTINDEX(plr->edict()) );
					WRITE_SHORT( plr->pev->frags = 0 );
					WRITE_SHORT( plr->m_iDeaths = 0 );
					WRITE_SHORT( 0 );
					WRITE_SHORT( 0 );
					MESSAGE_END();

					ALERT(at_console, "| %s ", STRING(plr->pev->netname) );

					plr->ExitObserver();
					plr->IsInArena = TRUE;
					plr->m_iGameModePlays++;
				}
				else
				{
					if ( !plr->IsSpectator() )
					{
						//just incase player played previous round
						MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
						WRITE_BYTE( ENTINDEX(plr->edict()) );
						WRITE_SHORT( plr->pev->frags = 0 );
						WRITE_SHORT( plr->m_iDeaths = 0 );
						WRITE_SHORT( 0 );
						WRITE_SHORT( 0 );
						MESSAGE_END();

						edict_t *pentSpawnSpot = g_pGameRules->GetPlayerSpawnSpot(plr);
						plr->StartObserver(plr->pev->origin, VARS(pentSpawnSpot)->angles);
					}
				}
			}
		}

		ALERT(at_console, "\n");

		m_iCountDown = 3;

		g_GameInProgress = TRUE;
		UTIL_ClientPrintAll(HUD_PRINTCENTER,
			UTIL_VarArgs("Arena has begun!\n\n%s Vs. %s",
			STRING(pPlayer1->pev->netname),
			STRING(pPlayer2->pev->netname)));
	}
	else
	{
		SuckAllToSpectator();
		m_flRoundTimeLimit = 0;
		UTIL_ClientPrintAll(HUD_PRINTCENTER, "Waiting for other players to begin\n\n'Arena'\n");
	}


	flUpdateTime = gpGlobals->time + 5.0;
}

int CHalfLifeMultiplay::CheckClients ( void )
{
	int clients = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		m_iPlayersInArena[i-1] = 0;

		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
		{
			ALERT(at_aiconsole, "%s[%d] is accounted for\n", STRING(plr->pev->netname), i);
			clients++;
			plr->IsInArena = FALSE;
			plr->IsArmoredMan = FALSE;
			plr->pev->fuser4 = 0;
			plr->m_fArmoredManHits = 0;
			plr->m_flForceToObserverTime = 0;
			m_iPlayersInArena[clients-1] = i;
		}
	}

	return clients;
}

void CHalfLifeMultiplay::InsertClientsIntoArena ( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr && plr->IsPlayer() )
		{ 
			MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(plr->edict()) );
			if ( g_GameMode == GAME_LMS )
				WRITE_SHORT( plr->pev->frags = startwithlives.value );
			else if ( g_GameMode == GAME_ARENA  )
				WRITE_SHORT( plr->pev->frags = 0 );
			else
				WRITE_SHORT( 0 );
			WRITE_SHORT( plr->m_iDeaths = 0 );
			WRITE_SHORT( 0 );
			WRITE_SHORT( 0 );
			MESSAGE_END();

			ALERT(at_console, "%s\n", STRING(plr->pev->netname));

			plr->ExitObserver();
			plr->IsInArena = TRUE;
			plr->m_iGameModePlays++;
		}
	}
}

BOOL CHalfLifeMultiplay::CheckGameTimer( void )
{
	g_engfuncs.pfnCvar_DirectSet(&roundtimeleft, UTIL_VarArgs( "%i", int(m_flRoundTimeLimit) - int(gpGlobals->time)));

	if ( !_30secwarning && (m_flRoundTimeLimit - 30) < gpGlobals->time )
	{
		UTIL_ClientPrintAll(HUD_PRINTTALK, "* 30 second warning...\n");
		_30secwarning = TRUE;
	}
	else if ( !_15secwarning && (m_flRoundTimeLimit - 15) < gpGlobals->time )
	{
		UTIL_ClientPrintAll(HUD_PRINTTALK, "* 15 second warning...\n");
		_15secwarning = TRUE;
	}
	else if ( !_3secwarning && (m_flRoundTimeLimit - 3) < gpGlobals->time )
	{
		UTIL_ClientPrintAll(HUD_PRINTTALK, "* 3 second warning...\n");
		_3secwarning = TRUE;
	}

//===================================================

	//time is up for this game.

//===================================================

	//time is up
	if ( m_flRoundTimeLimit < gpGlobals->time )
	{
		int highest = -99999;
		BOOL IsEqual = FALSE;
		CBasePlayer *highballer = NULL;

		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && plr->IsInArena )
				{
					if ( highest <= plr->pev->frags )
					{
						if ( highest == plr->pev->frags )
						{
							IsEqual = TRUE;
							break;
						}

						highest = plr->pev->frags;
						highballer = plr;
					}
				}
			}

			if ( !IsEqual && highballer )
			{
				CheckRounds();
				DisplayWinnersGoods( highballer );
				UTIL_ClientPrintAll(HUD_PRINTCENTER,
					UTIL_VarArgs("Time is Up: %s is the Victor!\n", STRING(highballer->pev->netname)));

				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
				MESSAGE_END();
			}
			else
			{
				UTIL_ClientPrintAll(HUD_PRINTCENTER, "Time is Up: Match ends in a draw!" );
				UTIL_ClientPrintAll(HUD_PRINTTALK, "* No winners in this round!");

				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
				MESSAGE_END();
			}

		}

		g_GameInProgress = FALSE;

		flUpdateTime = gpGlobals->time + 5.0;
		m_flRoundTimeLimit = 0;
		return TRUE;
	}

	return FALSE;
}

void CHalfLifeMultiplay::SetRoundLimits( void )
{
	//enforce a fraglimit always.
	//if ( CVAR_GET_FLOAT("mp_fraglimit") <= 0 )
	//	CVAR_SET_FLOAT("mp_fraglimit", 10);
	
	//enforce a timelimit if given proper value
	if ( CVAR_GET_FLOAT("mp_roundtimelimit") > 0 )
		m_flRoundTimeLimit = gpGlobals->time + (CVAR_GET_FLOAT("mp_roundtimelimit") * 60.0);

	_30secwarning	= FALSE;
	_15secwarning	= FALSE;
	_3secwarning	= FALSE;
}

void CHalfLifeMultiplay::CheckRounds( void )
{
	m_iSuccessfulRounds++;

	if ( CVAR_GET_FLOAT("mp_roundlimit") > 0 )
	{
		ALERT( at_notice, UTIL_VarArgs("SuccessfulRounds = %i\n", m_iSuccessfulRounds ));
		if ( m_iSuccessfulRounds >= CVAR_GET_FLOAT("mp_roundlimit") )
			GoToIntermission();
	}
}

void CHalfLifeMultiplay::RemoveItemsThatDamage( void )
{
	const char *pRemoveThese[] =
	{
		"monster_satchel",
		"monster_tripmine",
		"monster_chumtoad",
		"monster_snark",
	};

	CBaseEntity *pEntity = NULL;
	for (int itemIndex = 0; itemIndex < ARRAYSIZE(pRemoveThese); itemIndex++)
	{
		while ((pEntity = UTIL_FindEntityByClassname(pEntity, pRemoveThese[itemIndex])) != NULL)
		{
			ALERT(at_aiconsole, "Remove %s at [x=%.2f,y=%.2f,z=%.2f]\n", 
				pRemoveThese[itemIndex],
				pEntity->pev->origin.x,
				pEntity->pev->origin.y,
				pEntity->pev->origin.z);
			UTIL_Remove(pEntity);
		}
	}
}

void CHalfLifeMultiplay::SuckAllToSpectator( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer && pPlayer->IsPlayer() && !pPlayer->IsSpectator() )
		{ 
			MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
			WRITE_SHORT( pPlayer->pev->frags = 0 );
			WRITE_SHORT( pPlayer->m_iDeaths = 0 );
			WRITE_SHORT( 0 );
			WRITE_SHORT( 0 );
			MESSAGE_END();

			edict_t *pentSpawnSpot = g_pGameRules->GetPlayerSpawnSpot( pPlayer );
			pPlayer->StartObserver(pPlayer->pev->origin, VARS(pentSpawnSpot)->angles);
		}
	}
}

void CHalfLifeMultiplay::DisplayWinnersGoods( CBasePlayer *pPlayer )
{
	//increase his win count
	pPlayer->m_iGameModeWins++;

	//and display to the world what he does best!
	UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %s has won round #%d!\n", STRING(pPlayer->pev->netname), m_iSuccessfulRounds));
	UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %s record is %i for %i [%.1f%%]\n", STRING(pPlayer->pev->netname),
		pPlayer->m_iGameModeWins,
		pPlayer->m_iGameModePlays,
		((float)pPlayer->m_iGameModeWins / (float)pPlayer->m_iGameModePlays) * 100 ));
}

void CHalfLifeMultiplay::ResetGameMode( void )
{
	g_GameInProgress = FALSE;

	m_iCountDown = 3;

	_30secwarning = FALSE;
	_15secwarning = FALSE;
	_3secwarning = FALSE;

	m_iSuccessfulRounds = 0;
	flUpdateTime = 0;
	m_flRoundTimeLimit = 0;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsMultiplayer( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsDeathmatch( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsCoOp( void )
{
	return gpGlobals->coop;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	// Bot patch
	if ( !pWeapon->m_pPlayer ) {
		return FALSE;
	}

	if ( !pWeapon->CanDeploy() )
	{
		// that weapon can't deploy anyway.
		return FALSE;
	}

	if ( !pPlayer->m_pActiveItem )
	{
		// player doesn't have an active item!
		return TRUE;
	}

	if ( !pPlayer->m_pActiveItem->CanHolster() )
	{
		// can't put away the active item.
		return FALSE;
	}

	if ( pPlayer->ShouldWeaponSwitch() && pWeapon->iWeight() > pPlayer->m_pActiveItem->iWeight() )
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CHalfLifeMultiplay :: GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon )
{

	CBasePlayerItem *pCheck;
	CBasePlayerItem *pBest;// this will be used in the event that we don't find a weapon in the same category.
	int iBestWeight;
	int i;

	iBestWeight = -1;// no weapon lower than -1 can be autoswitched to
	pBest = NULL;

	if ( !pCurrentWeapon->CanHolster() )
	{
		// can't put this gun away right now, so can't switch.
		return FALSE;
	}

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pCheck = pPlayer->m_rgpPlayerItems[ i ];

		while ( pCheck )
		{
			if ( pCheck->iWeight() > -1 && pCheck->iWeight() == pCurrentWeapon->iWeight() && pCheck != pCurrentWeapon )
			{
				// this weapon is from the same category. 
				if ( pCheck->CanDeploy() )
				{
					if ( pPlayer->SwitchWeapon( pCheck ) )
					{
						return TRUE;
					}
				}
			}
			else if ( pCheck->iWeight() > iBestWeight && pCheck != pCurrentWeapon )// don't reselect the weapon we're trying to get rid of
			{
				//ALERT ( at_console, "Considering %s\n", STRING( pCheck->pev->classname ) );
				// we keep updating the 'best' weapon just in case we can't find a weapon of the same weight
				// that the player was using. This will end up leaving the player with his heaviest-weighted 
				// weapon. 
				if ( pCheck->CanDeploy() )
				{
					// if this weapon is useable, flag it as the best
					iBestWeight = pCheck->iWeight();
					pBest = pCheck;
				}
			}

			pCheck = pCheck->m_pNext;
		}
	}

	// if we make it here, we've checked all the weapons and found no useable 
	// weapon in the same catagory as the current weapon. 
	
	// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always 
	// at least get the crowbar, but ya never know.
	if ( !pBest )
	{
		return FALSE;
	}

	pPlayer->SwitchWeapon( pBest );

	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] )
{
	if ( pEntity )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pEntity );
		if ( pPlayer )
		{
			pPlayer->IsInArena = FALSE;
			pPlayer->HasDisconnected = FALSE;
		}
	}

	g_VoiceGameMgr.ClientConnected(pEntity);
	return TRUE;
}

extern int gmsgSayText;
extern int gmsgGameMode;
extern int gmsgMutators;

void CHalfLifeMultiplay :: UpdateGameMode( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, NULL, pPlayer->edict() );
		WRITE_BYTE( g_GameMode );  // game mode none
	MESSAGE_END();
}

void CHalfLifeMultiplay :: InitHUD( CBasePlayer *pl )
{
	// notify other clients of player joining the game
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s has joined the game\n", 
		( pl->pev->netname && STRING(pl->pev->netname)[0] != 0 ) ? STRING(pl->pev->netname) : "unconnected" ) );

	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" entered the game\n",  
			STRING( pl->pev->netname ), 
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pl->edict() ), "model" ) );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" entered the game\n",  
			STRING( pl->pev->netname ), 
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			GETPLAYERUSERID( pl->edict() ) );
	}

	UpdateGameMode( pl );

	char szMutators[64];
	strncpy(szMutators, mutators.string, sizeof(szMutators));
	MESSAGE_BEGIN( MSG_ALL, gmsgMutators );
		WRITE_STRING(szMutators);
	MESSAGE_END();

	// sending just one score makes the hud scoreboard active;  otherwise
	// it is just disabled for single play
	MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
		WRITE_BYTE( ENTINDEX(pl->edict()) );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	SendMOTDToClient( pl->edict() );

	// loop through all active players and send their score info to the new client
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		// FIXME:  Probably don't need to cast this just to read m_iDeaths
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
				WRITE_BYTE( i );	// client number
				WRITE_SHORT( plr->pev->frags );
				WRITE_SHORT( plr->m_iDeaths );
				WRITE_SHORT( 0 );
				WRITE_SHORT( GetTeamIndex( plr->m_szTeamName ) + 1 );
			MESSAGE_END();
		}
	}

	pl->m_iShownWelcomeMessage = gpGlobals->time + 4.0;
	pl->m_iShowMutatorMessage = gpGlobals->time + 2.0;
	pl->m_iShowGameModeMessage = gpGlobals->time + 2.0;

	if ( g_fGameOver )
	{
		MESSAGE_BEGIN( MSG_ONE, SVC_INTERMISSION, NULL, pl->edict() );
		MESSAGE_END();
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: ClientDisconnected( edict_t *pClient )
{
	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		if ( pPlayer )
		{
			FireTargets( "game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0 );

			// team match?
			if ( g_teamplay )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" disconnected\n",  
					STRING( pPlayer->pev->netname ), 
					GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ),
					g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model" ) );
			}
			else
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%i>\" disconnected\n",  
					STRING( pPlayer->pev->netname ), 
					GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ),
					GETPLAYERUSERID( pPlayer->edict() ) );
			}

			// Cold Ice Remastered Game mode stuff
			pPlayer->HasDisconnected = TRUE;
			pPlayer->IsInArena = FALSE;

			if (pPlayer->pev->flags & FL_FAKECLIENT)
			{
				pPlayer->pev->frags = 0;
				pPlayer->m_iDeaths = 0;
			}

			if ( !pPlayer->IsSpectator() )
			{
				MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_TELEPORT	); 
				WRITE_COORD(pPlayer->pev->origin.x);
				WRITE_COORD(pPlayer->pev->origin.y);
				WRITE_COORD(pPlayer->pev->origin.z);
				MESSAGE_END();
			}

			if ( g_GameInProgress )
			{
				if ( pPlayer->IsInArena && !pPlayer->IsSpectator() )
				{
					pPlayer->IsInArena = FALSE;

					flUpdateTime = gpGlobals->time + 3.0;
					UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("%s has left the arena!\n", STRING(pPlayer->pev->netname)));
				}
			}

			pPlayer->RemoveAllItems( TRUE );// destroy all of the players weapons and items
		}
	}
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	int iFallDamage = (int)falldamage.value;

	// Mutators
	if ((strstr(mutators.string, g_MutatorSuperJump) ||
		atoi(mutators.string) == MUTATOR_SUPERJUMP))
	{
		return 0;
	}

	switch ( iFallDamage )
	{
	case 1://progressive
		pPlayer->m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
		return pPlayer->m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
		break;
	default:
	case 0:// fixed
		return 10;
		break;
	}
} 

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerThink( CBasePlayer *pPlayer )
{
	if (strstr(mutators.string, g_MutatorLightsOut) ||
		atoi(mutators.string) == MUTATOR_LIGHTSOUT)
	{
		if (pPlayer->IsAlive())
			pPlayer->m_iFlashBattery = 100;
		//else
		//	pPlayer->FlashlightTurnOff();
	}

	if ( pPlayer->m_fHasRune == RUNE_REGEN )
	{
		if ( pPlayer->m_flRuneHealTime < gpGlobals->time )
		{
			if (pPlayer->pev->health < pPlayer->pev->max_health) {
				pPlayer->pev->health += 1;
				UTIL_ScreenFade( pPlayer, Vector(200,0,200), .5, .5, 32, FFADE_IN);
				pPlayer->m_flRuneHealTime = gpGlobals->time + 1.0;
			} else if (pPlayer->pev->armorvalue < pPlayer->pev->max_health) {
				pPlayer->pev->armorvalue += 1;
				UTIL_ScreenFade( pPlayer, Vector(200,0,200), .5, .5, 32, FFADE_IN);
				pPlayer->m_flRuneHealTime = gpGlobals->time + 1.0;
			}
		}
	}
	else if ( pPlayer->m_fHasRune == RUNE_AMMO )
	{
		if ( pPlayer->m_flRuneHealTime < gpGlobals->time )
		{
			if (pPlayer->m_pActiveItem)
			{
				CBasePlayerWeapon *pWeapon = (CBasePlayerWeapon*)pPlayer->m_pActiveItem;

				if (pPlayer->m_rgAmmo[pWeapon->m_iPrimaryAmmoType] < pWeapon->iMaxAmmo1()) {
					pPlayer->m_rgAmmo[pWeapon->m_iPrimaryAmmoType] += 1;
					UTIL_ScreenFade( pPlayer, Vector(200,200,0), .5, .5, 32, FFADE_IN);
					pPlayer->m_flRuneHealTime = gpGlobals->time + 1.0;
				} else if (pPlayer->m_rgAmmo[pWeapon->m_iSecondaryAmmoType] < pWeapon->iMaxAmmo2()) {
					pPlayer->m_rgAmmo[pWeapon->m_iSecondaryAmmoType] += 1;
					UTIL_ScreenFade( pPlayer, Vector(200,200,0), .5, .5, 32, FFADE_IN);
					pPlayer->m_flRuneHealTime = gpGlobals->time + 1.0;
				}
			}
		}
	}

	if ( g_fGameOver )
	{
		// check for button presses
		if ( pPlayer->m_afButtonPressed & ( IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP ) )
			m_iEndIntermissionButtonHit = TRUE;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed = 0;
		pPlayer->pev->button = 0;
		pPlayer->m_afButtonReleased = 0;
	}

	if (pPlayer->m_iShownWelcomeMessage != -1 && pPlayer->m_iShownWelcomeMessage < gpGlobals->time) {
#ifdef GIT
		ClientPrint(pPlayer->pev, HUD_PRINTTALK, "Welcome to Cold Ice Remastered Beta 4 (%s). For commands, type \"help\" in the console.\n", GIT);
#else
		ClientPrint(pPlayer->pev, HUD_PRINTTALK, "Welcome to Cold Ice Remastered Beta 4. For commands, type \"help\" in the console.\n");
#endif
		pPlayer->m_iShownWelcomeMessage = -1;
	}

	g_pGameRules->UpdateMutatorMessage(pPlayer);
	g_pGameRules->UpdateGameModeMessage(pPlayer);

#ifdef _DEBUG
	if (debug_disconnects.value && g_iKickSomeone < gpGlobals->time) {

		int bot_clients = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( plr && plr->pev->flags & FL_FAKECLIENT )
			{
				bot_clients++;
			}
		}

		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( RANDOM_LONG(1, bot_clients) );

		if (RANDOM_LONG(0,1) == 1)
		{
			if ( plr && plr->pev->flags & FL_FAKECLIENT )
			{
				char cmd[80];
				strcpy(cmd, "");
				if (RANDOM_LONG(0,1))
				{
					sprintf(cmd, "kick \"%s\"\n", STRING(plr->pev->netname));
					SERVER_COMMAND(cmd);
					ALERT(at_aiconsole, "rotate client: %s", cmd);
				}
				else
				{
					ClearMultiDamage();
					plr->pev->health = 0;
					plr->Killed( plr->pev, GIB_NEVER );
				}
			}
		}
		else
		{
			ALERT(at_aiconsole, "adding bot...\n");
			SERVER_COMMAND("addbot\n");
		}

		g_iKickSomeone = gpGlobals->time + debug_disconnects_time.value;
	}
#endif
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerSpawn( CBasePlayer *pPlayer )
{
	BOOL		addDefault;
	CBaseEntity	*pWeaponEntity = NULL;

	pPlayer->pev->weapons |= (1<<WEAPON_SUIT);
	
	addDefault = TRUE;

	pPlayer->GiveNamedItem("weapon_fists");

	while ( pWeaponEntity = UTIL_FindEntityByClassname( pWeaponEntity, "game_player_equip" ))
	{
		pWeaponEntity->Touch( pPlayer );
		addDefault = FALSE;
	}

	if (startwithall.value) {
		pPlayer->CheatImpulseCommands(101, FALSE);
	}
	else if ( addDefault )
	{
		if (snowballfight.value)
		{
			pPlayer->GiveNamedItem("weapon_snowball");
		}
		else
		{
			// Give a random melee
			char *meleeWeapon = "weapon_crowbar";
			if (g_GameMode == GAME_FFA) {
				int whichWeapon = RANDOM_LONG(0,3);
				if (!whichWeapon) {
					meleeWeapon = "weapon_chainsaw";
				} else if (whichWeapon == 1) {
					meleeWeapon = "weapon_knife";
				} else if (whichWeapon == 2 && (!strstr(mutators.string, g_MutatorPlumber) &&
					atoi(mutators.string) != MUTATOR_PLUMBER)) {
					meleeWeapon = "weapon_wrench";
				}
			}
			pPlayer->GiveNamedItem(STRING(ALLOC_STRING(meleeWeapon)));
		}

		char *pWeaponName;
		char list[1024];
		strcpy(list, spawnweaponlist.string);
		pWeaponName = list;
		pWeaponName = strtok( pWeaponName, ";" );
		while ( pWeaponName != NULL && *pWeaponName )
		{
			const char *newWeapon = STRING(ALLOC_STRING(pWeaponName));
			if (!pPlayer->HasNamedPlayerItem(newWeapon))
				pPlayer->GiveNamedItem(newWeapon);
			pWeaponName = strtok( NULL, ";" );
		}
		pPlayer->GiveAmmo( 68, "9mm", _9MM_MAX_CARRY );// 4 full reloads
	}

	g_pGameRules->SpawnMutators(pPlayer);
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	switch (g_GameMode)
	{
	case GAME_LMS:
		if ( pPlayer->pev->frags <= 0 )
		{
			if ( !pPlayer->m_flForceToObserverTime )
				pPlayer->m_flForceToObserverTime = gpGlobals->time + 3.0;

			return FALSE;
		}
		break;

	case GAME_ARENA:
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
		break;
	}

	return TRUE;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerSpawnTime( CBasePlayer *pPlayer )
{
	return gpGlobals->time;//now!
}

BOOL CHalfLifeMultiplay :: AllowAutoTargetCrosshair( void )
{
	return ( aimcrosshair.value != 0 );
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
int CHalfLifeMultiplay :: IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	if ( pAttacker->m_fHasRune == RUNE_FRAG )
		return 2;
	else if ( g_GameMode == GAME_LMS )
		return 0;
	else
		return 1;
}


//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
#define	HITGROUP_HEAD 1

void CHalfLifeMultiplay :: PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	DeathNotice( pVictim, pKiller, pInflictor );

	if ( g_GameInProgress )
	{
		switch( g_GameMode )
		{
			case GAME_LMS:
				pVictim->pev->frags -= 1;

				if ( !pVictim->pev->frags ) {
					UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %s has been eliminated from the round!\n", STRING(pVictim->pev->netname)));
					MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, pVictim->edict() );
						WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
					MESSAGE_END();
				}
				break;
		}
	}

	pVictim->m_iDeaths += 1;


	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );
	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );
	if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayer*)ktmp;

	if ( pVictim->pev == pKiller )  
	{  // killed self
		pKiller->frags -= 1;
	}
	else if ( ktmp && ktmp->IsPlayer() )
	{
		// if a player dies in a deathmatch game and the killer is a client, award the killer some points
		pKiller->frags += IPointsForKill( peKiller, pVictim );

		if (!m_iFirstBloodDecided)
		{
			UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("%s achieves first blood!\n", STRING(pKiller->netname) ));
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_FIRSTBLOOD);
			MESSAGE_END();
			pKiller->health = 100;
			pKiller->frags += 1;
			m_iFirstBloodDecided = TRUE;
		}
		else if (pVictim->m_LastHitGroup == HITGROUP_HEAD)
		{
			MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, pKiller );
				WRITE_BYTE(CLIENT_SOUND_HEADSHOT);
			MESSAGE_END();
			pKiller->health += 5;
		}

		if (strstr(mutators.string, g_MutatorLoopback) ||
			atoi(mutators.string) == MUTATOR_LOOPBACK)
		{
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_TELEPORT	); 
				WRITE_COORD(pKiller->origin.x);
				WRITE_COORD(pKiller->origin.y);
				WRITE_COORD(pKiller->origin.z);
			MESSAGE_END();
			UTIL_ScreenFade(peKiller, Vector(200,200,200), .15, .15, 200, FFADE_IN);

			pKiller->origin = pVictim->pev->origin;

			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_TELEPORT	); 
				WRITE_COORD(pKiller->origin.x);
				WRITE_COORD(pKiller->origin.y);
				WRITE_COORD(pKiller->origin.z);
			MESSAGE_END();
		}

		FireTargets( "game_playerkill", ktmp, ktmp, USE_TOGGLE, 0 );
	}
	else
	{  // killed by the world
		pKiller->frags -= 1;
	}

	// update the scores
	// killed scores
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );
		WRITE_SHORT( pVictim->pev->frags );
		WRITE_SHORT( pVictim->m_iDeaths );
		WRITE_SHORT( 0 );
		WRITE_SHORT( GetTeamIndex( pVictim->m_szTeamName ) + 1 );
	MESSAGE_END();

	// killers score, if it's a player
	CBaseEntity *ep = CBaseEntity::Instance( pKiller );
	if ( ep && ep->Classify() == CLASS_PLAYER )
	{
		CBasePlayer *PK = (CBasePlayer*)ep;

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(PK->edict()) );
			WRITE_SHORT( PK->pev->frags );
			WRITE_SHORT( PK->m_iDeaths );
			WRITE_SHORT( 0 );
			WRITE_SHORT( GetTeamIndex( PK->m_szTeamName) + 1 );
		MESSAGE_END();

		// let the killer paint another decal as soon as he'd like.
		PK->m_flNextDecalTime = gpGlobals->time;
	}
#ifndef HLDEMO_BUILD
	if ( pVictim->HasNamedPlayerItem("weapon_satchel") )
	{
		DeactivateSatchels( pVictim );
		if (g_pGameRules->FAllowMonsters())
			DeactivateAssassins( pVictim );
	}
	DeactivatePortals( pVictim );
#endif

	if (m_iVolatile) {
		// No echo boom
		if (pInflictor && FClassnameIs(pInflictor, "vest"))
			return;
		CGrenade::Vest( pVictim->pev, pVictim->pev->origin );
		pVictim->pev->solid = SOLID_NOT;
		pVictim->GibMonster();
		pVictim->pev->effects |= EF_NODRAW;
	}

	if (m_iNotTheBees)
	{
		Vector angles;
		BOOL isOtherPlayer = peKiller && pKiller != pVictim->pev;
		if (isOtherPlayer) 
			angles = UTIL_VecToAngles((pKiller->origin + pKiller->view_ofs ) - pVictim->pev->origin);

		CBaseEntity *hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16, isOtherPlayer ? angles : gpGlobals->v_up, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isOtherPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				hornet->pev->velocity.z += 50;
			}
			else
				hornet->pev->velocity = gpGlobals->v_up * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * 8, isOtherPlayer ? angles : gpGlobals->v_up, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isOtherPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				hornet->pev->velocity.z += 50;
			}
			else
				hornet->pev->velocity = gpGlobals->v_up * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16 + gpGlobals->v_forward * 24, isOtherPlayer ? angles : gpGlobals->v_forward, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isOtherPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				hornet->pev->velocity.z += 50;
			}
			else
				hornet->pev->velocity = gpGlobals->v_forward * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16 + gpGlobals->v_forward * -24, isOtherPlayer ? angles : gpGlobals->v_forward * -1, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isOtherPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				hornet->pev->velocity.z += 50;
			}
			else
				hornet->pev->velocity = (gpGlobals->v_forward * -1) * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16 + gpGlobals->v_right * 24, isOtherPlayer ? angles : gpGlobals->v_right, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isOtherPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				hornet->pev->velocity.z += 50;
			}
			else
				hornet->pev->velocity = gpGlobals->v_right * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16 + gpGlobals->v_right * -24, isOtherPlayer ? angles : gpGlobals->v_right * -1, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isOtherPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				hornet->pev->velocity.z += 50;
			}
			else
				hornet->pev->velocity = (gpGlobals->v_right * -1) * RANDOM_LONG(100, 200);
		}
	}
}

//=========================================================
// Deathnotice. 
//=========================================================
void CHalfLifeMultiplay::DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor )
{
	// Work out what killed the player, and send a message to all clients about it
	CBaseEntity *Killer = CBaseEntity::Instance( pKiller );

	const char *killer_weapon_name = "world";		// by default, the player is killed by the world
	int killer_index = 0;
	
	// Hack to fix name change
	char *tau = "tau_cannon";
	char *gluon = "gluon gun";

	// For assassin kills
	if (pevInflictor != NULL 
		&& strncmp(STRING( pevInflictor->classname ), "monster_human_assassin", 22) == 0)
	{
		pKiller = VARS(Killer->pev->owner);
	}

	if ( pKiller->flags & FL_CLIENT )
	{
		killer_index = ENTINDEX(ENT(pKiller));
		
		if ( pevInflictor )
		{
			if ( pevInflictor == pKiller )
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				CBasePlayer *pPlayer = (CBasePlayer*)CBaseEntity::Instance( pKiller );
				
				if ( pPlayer->m_pActiveItem )
				{
					killer_weapon_name = pPlayer->m_pActiveItem->pszName();
				}
			}
			else
			{
				killer_weapon_name = STRING( pevInflictor->classname );  // it's just that easy
			}
		}
	}
	else
	{
		killer_weapon_name = STRING( pevInflictor->classname );
	}

	if (gMultiDamage.pEntity == pVictim && (gMultiDamage.type & DMG_KICK))
	{
		killer_weapon_name = "kick";
	}

	if (gMultiDamage.pEntity == pVictim && (gMultiDamage.type & DMG_PUNCH))
	{
		killer_weapon_name = "fists";
	}

	// Tracer weapon id
	if (pevInflictor && pevInflictor->iuser3 > 0)
	{
		killer_weapon_name = STRING(pevInflictor->iuser3);
		pevInflictor->iuser3 = 0;
	}

	// strip the monster_* or weapon_* from the inflictor's classname
	if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
		killer_weapon_name += 7;
	else if ( strncmp( killer_weapon_name, "monster_", 8 ) == 0 )
		killer_weapon_name += 8;
	else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		killer_weapon_name += 5;
	else if ( strncmp( killer_weapon_name, "player", 6 ) == 0 )
		killer_weapon_name = "flamethrower";

	if ( strncmp( killer_weapon_name, "human_assassin", 14 ) == 0 )
		killer_weapon_name = "vest";

	MESSAGE_BEGIN( MSG_ALL, gmsgDeathMsg );
		WRITE_BYTE( killer_index );						// the killer
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );		// the victim
		WRITE_STRING( killer_weapon_name );		// what they were killed by (should this be a string?)
	MESSAGE_END();

	// replace the code names with the 'real' names
	if ( !strcmp( killer_weapon_name, "egon" ) )
		killer_weapon_name = gluon;
	else if ( !strcmp( killer_weapon_name, "gauss" ) )
		killer_weapon_name = tau;

	if ( pVictim->pev == pKiller )  
	{
		// killed self

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",  
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\"\n",  
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",  
				STRING( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( ENT(pKiller) ), "model" ),
				STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" killed \"%s<%i><%s><%i>\" with \"%s\"\n",  
				STRING( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				GETPLAYERUSERID( ENT(pKiller) ),
				STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );
		}
	}
	else
	{ 
		// killed by the world

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\" (world)\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\" (world)\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}

	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE ( 9 );	// command length in bytes
		WRITE_BYTE ( DRC_CMD_EVENT );	// player killed
		WRITE_SHORT( ENTINDEX(pVictim->edict()) );	// index number of primary entity
		if (pevInflictor)
			WRITE_SHORT( ENTINDEX(ENT(pevInflictor)) );	// index number of secondary entity
		else
			WRITE_SHORT( ENTINDEX(ENT(pKiller)) );	// index number of secondary entity
		WRITE_LONG( 7 | DRC_FLAG_DRAMATIC);   // eventflags (priority and flags)
	MESSAGE_END();

//  Print a standard message
	// TODO: make this go direct to console
	return; // just remove for now
/*
	char	szText[ 128 ];

	if ( pKiller->flags & FL_MONSTER )
	{
		// killed by a monster
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was killed by a monster.\n" );
		return;
	}

	if ( pKiller == pVictim->pev )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " commited suicide.\n" );
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		strcpy ( szText, STRING( pKiller->netname ) );

		strcat( szText, " : " );
		strcat( szText, killer_weapon_name );
		strcat( szText, " : " );

		strcat ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, "\n" );
	}
	else if ( FClassnameIs ( pKiller, "worldspawn" ) )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " fell or drowned or something.\n" );
	}
	else if ( pKiller->solid == SOLID_BSP )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was mooshed.\n" );
	}
	else
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " died mysteriously.\n" );
	}

	UTIL_ClientPrintAll( szText );
*/
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
void CHalfLifeMultiplay :: PlayerGotWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHalfLifeMultiplay :: FlWeaponRespawnTime( CBasePlayerItem *pWeapon )
{
	if ( weaponstay.value > 0 )
	{
		// make sure it's only certain weapons
		if ( !(pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
		{
			return gpGlobals->time + 0;		// weapon respawns almost instantly
		}
	}

	return gpGlobals->time + WEAPON_RESPAWN_TIME;
}

// when we are within this close to running out of entities,  items 
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE	100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHalfLifeMultiplay :: FlWeaponTryRespawn( CBasePlayerItem *pWeapon )
{
	if ( pWeapon && (pWeapon->m_iId > 0 && pWeapon->m_iId < MAX_WEAPONS) && (pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
	{
		if ( NUMBER_OF_ENTITIES() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE) )
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime( pWeapon );
	}

	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay :: VecWeaponRespawnSpot( CBasePlayerItem *pWeapon )
{
	return pWeapon->pev->origin;
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHalfLifeMultiplay :: WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	if ( pWeapon->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_WEAPON_RESPAWN_NO;
	}

	return GR_WEAPON_RESPAWN_YES;
}

//=========================================================
// CanHaveWeapon - returns FALSE if the player is not allowed
// to pick up this weapon
//=========================================================
BOOL CHalfLifeMultiplay::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if ( weaponstay.value > 0 )
	{
		if ( pItem->iFlags() & ITEM_FLAG_LIMITINWORLD )
			return CGameRules::CanHavePlayerItem( pPlayer, pItem );

		// check if the player already has this weapon
		for ( int i = 0 ; i < MAX_ITEM_TYPES ; i++ )
		{
			CBasePlayerItem *it = pPlayer->m_rgpPlayerItems[i];

			while ( it != NULL )
			{
				if ( it->m_iId == pItem->m_iId )
				{
					return FALSE;
				}

				it = it->m_pNext;
			}
		}
	}

	return CGameRules::CanHavePlayerItem( pPlayer, pItem );
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem )
{
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::ItemShouldRespawn( CItem *pItem )
{
	if ( pItem->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_ITEM_RESPAWN_NO;
	}

	return GR_ITEM_RESPAWN_YES;
}


//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHalfLifeMultiplay::FlItemRespawnTime( CItem *pItem )
{
	return gpGlobals->time + ITEM_RESPAWN_TIME;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay::VecItemRespawnSpot( CItem *pItem )
{
	return pItem->pev->origin;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotAmmo( CBasePlayer *pPlayer, char *szName, int iCount )
{
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsAllowedToSpawn( CBaseEntity *pEntity )
{
//	if ( pEntity->pev->flags & FL_MONSTER )
//		return FALSE;

	if (g_GameInProgress == TRUE && pEntity->IsPlayer()) {
		return FALSE;
	}

	if (!spawnweapons.value &&
		(strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0 || strncmp(STRING(pEntity->pev->classname), "ammo_", 5) == 0))
	{
		return FALSE;
	}

	if (!spawnitems.value &&
		strncmp(STRING(pEntity->pev->classname), "item_", 5) == 0)
	{
		return FALSE;
	}

	if (disallowlist.string && strstr(disallowlist.string, STRING(pEntity->pev->classname))) {
		ALERT(at_aiconsole, "%s has been disallowed on the server.\n", STRING(pEntity->pev->classname));
		return FALSE;
	}

/*
	if ((strstr(mutators.string, g_MutatorInstaGib) ||
		atoi(mutators.string) == MUTATOR_INSTAGIB) &&
		(strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0 || strncmp(STRING(pEntity->pev->classname), "ammo_", 5) == 0))
	{	
		if (stricmp(STRING(pEntity->pev->classname), "weapon_railgun") == 0 ||
			stricmp(STRING(pEntity->pev->classname), "weapon_dual_railgun") == 0 ||
			stricmp(STRING(pEntity->pev->classname), "weapon_fists") == 0) {
			return TRUE;
		}

		if (stricmp(STRING(pEntity->pev->classname), "ammo_gaussclip") != 0)
		{
			CBaseEntity::Create("ammo_gaussclip", pEntity->pev->origin, pEntity->pev->angles, pEntity->pev->owner);
			return FALSE;
		}
	}
*/

/*
	if ((strstr(mutators.string, g_MutatorPlumber) ||
		atoi(mutators.string) == MUTATOR_PLUMBER) &&
		(strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0 || strncmp(STRING(pEntity->pev->classname), "ammo_", 5) == 0))
	{	
		if (stricmp(STRING(pEntity->pev->classname), "weapon_fists") == 0 ||
			stricmp(STRING(pEntity->pev->classname), "weapon_wrench") == 0 ||
			stricmp(STRING(pEntity->pev->classname), "weapon_dual_wrench") == 0) {
			return TRUE;
		}

		if (stricmp(STRING(pEntity->pev->classname), "weapon_wrench") != 0)
		{
			CBaseEntity::Create("weapon_wrench", pEntity->pev->origin, pEntity->pev->angles, pEntity->pev->owner);
			return FALSE;
		}
	}
*/

	const char* dualWeaponList[7] = {
		"weapon_dual_wrench",
		"weapon_dual_deagle",
		"weapon_dual_mag60",
		"weapon_dual_smg",
		"weapon_dual_usas",
		"weapon_dual_rpg",
		"weapon_dual_flamethrower"
	};

	if (dualsonly.value && strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0)
	{
		// Allow addition weapons that are thrown
		if (strncmp(STRING(pEntity->pev->classname), "weapon_fists", 12) == 0 ||
			strncmp(STRING(pEntity->pev->classname), "weapon_wrench", 12) == 0 ||
			strncmp(STRING(pEntity->pev->classname), "weapon_crowbar", 14) == 0 ||
			strncmp(STRING(pEntity->pev->classname), "weapon_knife", 12) == 0)
			return TRUE;

		if (strncmp(STRING(pEntity->pev->classname), "weapon_dual_", 12) != 0)
		{
			CBaseEntity::Create((char *)dualWeaponList[RANDOM_LONG(0,ARRAYSIZE(dualWeaponList)-1)], pEntity->pev->origin, pEntity->pev->angles, pEntity->pev->owner);
			return FALSE;
		}
	}

	if (snowballfight.value && 
		(strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0 ||
		strncmp(STRING(pEntity->pev->classname), "ammo_", 5) == 0))
	{
		if (!stricmp(STRING(pEntity->pev->classname), "weapon_fists")) {
			return TRUE;
		}

		if (strncmp(STRING(pEntity->pev->classname), "weapon_snowball", 15) != 0)
		{
			CBaseEntity::Create("weapon_snowball", pEntity->pev->origin, pEntity->pev->angles, pEntity->pev->owner);
			return FALSE;
		}
	}

	return TRUE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::AmmoShouldRespawn( CBasePlayerAmmo *pAmmo )
{
	if ( pAmmo->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_AMMO_RESPAWN_NO;
	}

	return GR_AMMO_RESPAWN_YES;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlAmmoRespawnTime( CBasePlayerAmmo *pAmmo )
{
	return gpGlobals->time + AMMO_RESPAWN_TIME;
}

//=========================================================
//=========================================================
Vector CHalfLifeMultiplay::VecAmmoRespawnSpot( CBasePlayerAmmo *pAmmo )
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlHealthChargerRechargeTime( void )
{
	return 60;
}


float CHalfLifeMultiplay::FlHEVChargerRechargeTime( void )
{
	return 30;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	if (strstr(mutators.string, g_MutatorMaxPack) ||
		atoi(mutators.string) == MUTATOR_MAXPACK)
		return GR_PLR_DROP_GUN_ALL;
	else
		return GR_PLR_DROP_GUN_ACTIVE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	if (strstr(mutators.string, g_MutatorMaxPack) ||
		atoi(mutators.string) == MUTATOR_MAXPACK)
		return GR_PLR_DROP_AMMO_ALL;
	else
		return GR_PLR_DROP_AMMO_ACTIVE;
}

BOOL CHalfLifeMultiplay::IsAllowedSingleWeapon( CBaseEntity *pEntity )
{
	return TRUE;
}

BOOL CHalfLifeMultiplay::IsAllowedToDropWeapon( void )
{
	return TRUE;
}

BOOL CHalfLifeMultiplay::IsAllowedToHolsterWeapon( void )
{
	return holsterweapons.value;
}

edict_t *CHalfLifeMultiplay::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	edict_t *pentSpawnSpot = CGameRules::GetPlayerSpawnSpot( pPlayer );	
	if ( IsMultiplayer() && pentSpawnSpot->v.target )
	{
		FireTargets( STRING(pentSpawnSpot->v.target), pPlayer, pPlayer, USE_TOGGLE, 0 );
	}

	return pentSpawnSpot;
}


//=========================================================
//=========================================================
int CHalfLifeMultiplay::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	// half life deathmatch has only enemies
	return GR_NOTTEAMMATE;
}

BOOL CHalfLifeMultiplay :: PlayFootstepSounds( CBasePlayer *pl, float fvol )
{
	if ( g_footsteps && g_footsteps->value == 0 )
		return FALSE;

	if ( pl->IsOnLadder() || pl->pev->velocity.Length2D() > 220 )
		return TRUE;  // only make step sounds in multiplayer if the player is moving fast enough

	return FALSE;
}

BOOL CHalfLifeMultiplay :: FAllowFlashlight( void ) 
{ 
	return flashlight.value != 0; 
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FAllowMonsters( void )
{
	return ( allowmonsters.value != 0 );
}

//=========================================================
//=========================================================
#if defined( GRAPPLING_HOOK )
BOOL CHalfLifeMultiplay :: AllowGrapplingHook( CBasePlayer *pPlayer )
{
	return ( grapplinghook.value );
}
# endif

//=========================================================
//======== CHalfLifeMultiplay private functions ===========
#define INTERMISSION_TIME		6

void CHalfLifeMultiplay :: GoToIntermission( void )
{
	if ( g_fGameOver )
		return;  // intermission has already been triggered, so ignore.

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();

	// bounds check
	int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
	if ( time < 1 )
		CVAR_SET_STRING( "mp_chattime", "1" );
	else if ( time > MAX_INTERMISSION_TIME )
		CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

	m_flIntermissionEndTime = gpGlobals->time + ( (int)mp_chattime.value );
	g_flIntermissionStartTime = gpGlobals->time;

	// Clear previous message at intermission
	if (g_GameMode != GAME_FFA)
		UTIL_ClientPrintAll(HUD_PRINTCENTER, "");
	g_fGameOver = TRUE;
	m_iEndIntermissionButtonHit = FALSE;
}

#define MAX_RULE_BUFFER 1024

typedef struct mapcycle_item_s
{
	struct mapcycle_item_s *next;

	char mapname[ 32 ];
	int  minplayers, maxplayers;
	char rulebuffer[ MAX_RULE_BUFFER ];
} mapcycle_item_t;

typedef struct mapcycle_s
{
	struct mapcycle_item_s *items;
	struct mapcycle_item_s *next_item;
} mapcycle_t;

/*
==============
DestroyMapCycle

Clean up memory used by mapcycle when switching it
==============
*/
void DestroyMapCycle( mapcycle_t *cycle )
{
	mapcycle_item_t *p, *n, *start;
	p = cycle->items;
	if ( p )
	{
		start = p;
		p = p->next;
		while ( p != start )
		{
			n = p->next;
			delete p;
			p = n;
		}
		
		delete cycle->items;
	}
	cycle->items = NULL;
	cycle->next_item = NULL;
}

static char com_token[ 1500 ];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int             c;
	int             len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
			break;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}

/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
int COM_TokenWaiting( char *buffer )
{
	char *p;

	p = buffer;
	while ( *p && *p!='\n')
	{
		if ( !isspace( *p ) || isalnum( *p ) )
			return 1;

		p++;
	}

	return 0;
}



/*
==============
ReloadMapCycleFile


Parses mapcycle.txt file into mapcycle_t structure
==============
*/
int ReloadMapCycleFile( char *filename, mapcycle_t *cycle )
{
	char szBuffer[ MAX_RULE_BUFFER ];
	char szMap[ 32 ];
	int length;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE_FOR_ME( filename, &length );
	int hasbuffer;
	mapcycle_item_s *item, *newlist = NULL, *next;

	if ( pFileList && length )
	{
		// the first map name in the file becomes the default
		while ( 1 )
		{
			hasbuffer = 0;
			memset( szBuffer, 0, MAX_RULE_BUFFER );

			pFileList = COM_Parse( pFileList );
			if ( strlen( com_token ) <= 0 )
				break;

			strcpy( szMap, com_token );

			// Any more tokens on this line?
			if ( COM_TokenWaiting( pFileList ) )
			{
				pFileList = COM_Parse( pFileList );
				if ( strlen( com_token ) > 0 )
				{
					hasbuffer = 1;
					strcpy( szBuffer, com_token );
				}
			}

			// Check map
			if ( IS_MAP_VALID( szMap ) )
			{
				// Create entry
				char *s;

				item = new mapcycle_item_s;

				strcpy( item->mapname, szMap );

				item->minplayers = 0;
				item->maxplayers = 0;

				memset( item->rulebuffer, 0, MAX_RULE_BUFFER );

				if ( hasbuffer )
				{
					s = g_engfuncs.pfnInfoKeyValue( szBuffer, "minplayers" );
					if ( s && s[0] )
					{
						item->minplayers = atoi( s );
						item->minplayers = max( item->minplayers, 0 );
						item->minplayers = min( item->minplayers, gpGlobals->maxClients );
					}
					s = g_engfuncs.pfnInfoKeyValue( szBuffer, "maxplayers" );
					if ( s && s[0] )
					{
						item->maxplayers = atoi( s );
						item->maxplayers = max( item->maxplayers, 0 );
						item->maxplayers = min( item->maxplayers, gpGlobals->maxClients );
					}

					// Remove keys
					//
					g_engfuncs.pfnInfo_RemoveKey( szBuffer, "minplayers" );
					g_engfuncs.pfnInfo_RemoveKey( szBuffer, "maxplayers" );

					strcpy( item->rulebuffer, szBuffer );
				}

				item->next = cycle->items;
				cycle->items = item;
			}
			else
			{
				ALERT( at_console, "Skipping %s from mapcycle, not a valid map\n", szMap );
			}

		}

		FREE_FILE( aFileList );
	}

	// Fixup circular list pointer
	item = cycle->items;

	// Reverse it to get original order
	while ( item )
	{
		next = item->next;
		item->next = newlist;
		newlist = item;
		item = next;
	}
	cycle->items = newlist;
	item = cycle->items;

	// Didn't parse anything
	if ( !item )
	{
		return 0;
	}

	while ( item->next )
	{
		item = item->next;
	}
	item->next = cycle->items;
	
	cycle->next_item = item->next;

	return 1;
}

/*
==============
CountPlayers

Determine the current # of active players on the server for map cycling logic
==============
*/
int CountPlayers( void )
{
	int	num = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

		if ( pEnt )
		{
			num = num + 1;
		}
	}

	return num;
}

/*
==============
ExtractCommandString

Parse commands/key value pairs to issue right after map xxx command is issued on server
 level transition
==============
*/
void ExtractCommandString( char *s, char *szCommand )
{
	// Now make rules happen
	char	pkey[512];
	char	value[512];	// use two buffers so compares
								// work without stomping on each other
	char	*o;
	
	if ( *s == '\\' )
		s++;

	while (1)
	{
		o = pkey;
		while ( *s != '\\' )
		{
			if ( !*s )
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		strcat( szCommand, pkey );
		if ( strlen( value ) > 0 )
		{
			strcat( szCommand, " " );
			strcat( szCommand, value );
		}
		strcat( szCommand, "\n" );

		if (!*s)
			return;
		s++;
	}
}

/*
==============
ChangeLevel

Server is changing to a new level, check mapcycle.txt for map name and setup info
==============
*/
void CHalfLifeMultiplay :: ChangeLevel( void )
{
	static char szPreviousMapCycleFile[ 256 ];
	static mapcycle_t mapcycle;

	char szNextMap[32];
	char szFirstMapInList[32];
	char szCommands[ 1500 ];
	char szRules[ 1500 ];
	int minplayers = 0, maxplayers = 0;
	strcpy( szFirstMapInList, "hldm1" );  // the absolute default level is hldm1

	int	curplayers;
	BOOL do_cycle = TRUE;

	// find the map to change to
	char *mapcfile = (char*)CVAR_GET_STRING( "mapcyclefile" );
	ASSERT( mapcfile != NULL );

	szCommands[ 0 ] = '\0';
	szRules[ 0 ] = '\0';

	curplayers = CountPlayers();

#ifdef _DEBUG
	g_iKickSomeone = 0;
#endif

	// Has the map cycle filename changed?
	if ( stricmp( mapcfile, szPreviousMapCycleFile ) )
	{
		strcpy( szPreviousMapCycleFile, mapcfile );

		DestroyMapCycle( &mapcycle );

		if ( !ReloadMapCycleFile( mapcfile, &mapcycle ) || ( !mapcycle.items ) )
		{
			ALERT( at_console, "Unable to load map cycle file %s\n", mapcfile );
			do_cycle = FALSE;
		}
	}

	if ( do_cycle && mapcycle.items )
	{
		BOOL keeplooking = FALSE;
		BOOL found = FALSE;
		mapcycle_item_s *item;

		// Assume current map
		strcpy( szNextMap, STRING(gpGlobals->mapname) );
		strcpy( szFirstMapInList, STRING(gpGlobals->mapname) );

		// Traverse list
		for ( item = mapcycle.next_item; item->next != mapcycle.next_item; item = item->next )
		{
			keeplooking = FALSE;

			ASSERT( item != NULL );

			if ( item->minplayers != 0 )
			{
				if ( curplayers >= item->minplayers )
				{
					found = TRUE;
					minplayers = item->minplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( item->maxplayers != 0 )
			{
				if ( curplayers <= item->maxplayers )
				{
					found = TRUE;
					maxplayers = item->maxplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( keeplooking )
				continue;

			found = TRUE;
			break;
		}

		if ( !found )
		{
			item = mapcycle.next_item;
		}			
		
		// Increment next item pointer
		mapcycle.next_item = item->next;

		// Perform logic on current item
		strcpy( szNextMap, item->mapname );

		ExtractCommandString( item->rulebuffer, szCommands );
		strcpy( szRules, item->rulebuffer );
	}

	if ( !IS_MAP_VALID(szNextMap) )
	{
		strcpy( szNextMap, szFirstMapInList );
	}

	g_fGameOver = TRUE;

	ALERT( at_console, "CHANGE LEVEL: %s\n", szNextMap );
	if ( minplayers || maxplayers )
	{
		ALERT( at_console, "PLAYER COUNT:  min %i max %i current %i\n", minplayers, maxplayers, curplayers );
	}
	if ( strlen( szRules ) > 0 )
	{
		ALERT( at_console, "RULES:  %s\n", szRules );
	}
	
	CHANGE_LEVEL( szNextMap, NULL );
	if ( strlen( szCommands ) > 0 )
	{
		SERVER_COMMAND( szCommands );
	}
}

#define MAX_MOTD_CHUNK	  60
#define MAX_MOTD_LENGTH   1536 // (MAX_MOTD_CHUNK * 4)

void CHalfLifeMultiplay :: SendMOTDToClient( edict_t *client )
{
	// read from the MOTD.txt file
	int length, char_count = 0;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE_FOR_ME( (char *)CVAR_GET_STRING( "motdfile" ), &length );

	// send the server name
	MESSAGE_BEGIN( MSG_ONE, gmsgServerName, NULL, client );
		WRITE_STRING( CVAR_GET_STRING("hostname") );
	MESSAGE_END();

	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts

	while ( pFileList && *pFileList && char_count < MAX_MOTD_LENGTH )
	{
		char chunk[MAX_MOTD_CHUNK+1];
		
		if ( strlen( pFileList ) < MAX_MOTD_CHUNK )
		{
			strcpy( chunk, pFileList );
		}
		else
		{
			strncpy( chunk, pFileList, MAX_MOTD_CHUNK );
			chunk[MAX_MOTD_CHUNK] = 0;		// strncpy doesn't always append the null terminator
		}

		char_count += strlen( chunk );
		if ( char_count < MAX_MOTD_LENGTH )
			pFileList = aFileList + char_count; 
		else
			*pFileList = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgMOTD, NULL, client );
			WRITE_BYTE( *pFileList ? FALSE : TRUE );	// FALSE means there is still more message to come
			WRITE_STRING( chunk );
		MESSAGE_END();
	}

	FREE_FILE( aFileList );
}
	

