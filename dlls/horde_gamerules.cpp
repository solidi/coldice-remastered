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
#include "horde_gamerules.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"

extern int gmsgPlayClientSound;
extern int gmsgObjective;
extern int gmsgShowTimer;
extern int gmsgTeamInfo;
extern int gmsgScoreInfo;
extern int gmsgTeamNames;
extern int gmsgDeathMsg;

// In order of hardness
const char *szMonsters[] = {
	"monster_headcrab",
	"monster_zombie",
	"monster_houndeye",
	"monster_human_grunt",
	"monster_panther",
	"monster_gargantua"
};

#define ENEMY_TOTAL 8

CHalfLifeHorde::CHalfLifeHorde()
{
	m_iSurvivorsRemain = 0;
	m_iWaveNumber = 0;
	pLastSpawnPoint = NULL;

	UTIL_PrecacheOther( "monster_panther" );
	UTIL_PrecacheOther( "monster_headcrab" );
	UTIL_PrecacheOther( "monster_zombie" );
	UTIL_PrecacheOther( "monster_houndeye" );
	UTIL_PrecacheOther( "monster_human_grunt" );
	UTIL_PrecacheOther( "monster_gargantua" );
}

BOOL CHalfLifeHorde::IsSpawnPointValid( CBaseEntity *pSpot )
{
	CBaseEntity *ent = NULL;

	while ( (ent = UTIL_FindEntityInSphere( ent, pSpot->pev->origin, 1024 )) != NULL )
	{
		// Is another base in area
		if (FClassnameIs(ent->pev, "base"))
			return FALSE;
	}

	return TRUE;
}

edict_t *CHalfLifeHorde::EntSelectSpawnPoint( const char *szSpawnPoint )
{
	CBaseEntity *pSpot;

	// choose a point
	if ( g_pGameRules->IsDeathmatch() )
	{
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
				if ( IsSpawnPointValid( pSpot ) )
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
	}

ReturnSpot:
	if ( FNullEnt( pSpot ) )
	{
		ALERT(at_error, "no monster spot on level");
		return INDEXENT(0);
	}

	pLastSpawnPoint = pSpot;
	return pSpot->edict();
}

void CHalfLifeHorde::Think( void )
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
		{
			edict_t *pEdict = FIND_ENTITY_BY_STRING(NULL, "message", "horde");
			while (!FNullEnt(pEdict))
			{
				UTIL_Remove(CBaseEntity::Instance(pEdict));
				pEdict = FIND_ENTITY_BY_STRING(pEdict, "message", "horde");
			}
			return;
		}
	}

	if ( g_GameInProgress )
	{
		int survivors_left = 0;

		// Player accountings
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
			{
				// Force spectate on those that died.
				if ( plr->m_flForceToObserverTime && plr->m_flForceToObserverTime < gpGlobals->time )
				{
					edict_t *pentSpawnSpot = g_pGameRules->GetPlayerSpawnSpot( plr );
					plr->StartObserver(pentSpawnSpot->v.origin, VARS(pentSpawnSpot)->angles);
					plr->m_flForceToObserverTime = 0;
				}

				if ( plr->IsInArena && !plr->IsSpectator() )
				{
					survivors_left++;
				}
			}
		}

		m_iSurvivorsRemain = survivors_left;

		// spectator messages
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
			{
				// for clients who connected while game in progress.
				if ( !plr->IsInArena )
				{
					if (plr->IsSpectator() )
					{
						if (m_iTotalEnemies)
						{
							MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
								WRITE_STRING(UTIL_VarArgs("Wave #%d", m_iWaveNumber));
								WRITE_STRING(UTIL_VarArgs("Survivors remain: %d", m_iSurvivorsRemain));
								WRITE_BYTE(float(m_iEnemiesRemain) / (m_iTotalEnemies) * 100);
								WRITE_STRING(UTIL_VarArgs("Enemies remain: %d", m_iEnemiesRemain));
							MESSAGE_END();
						}
					} else {
						// Send them to observer
						plr->m_flForceToObserverTime = gpGlobals->time;
					}
				}
			}
		}

		// wave
		if (m_fBeginWaveTime && m_fBeginWaveTime < gpGlobals->time)
		{
			// Spawn enemies
			m_iTotalEnemies = 0;
			m_iEnemiesRemain = ENEMY_TOTAL;
			BOOL pickedUpPerson = FALSE;

			//spawn those dead, restock.
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
				{
					// Get back in
					if (plr->IsObserver())
					{
						plr->IsInArena = TRUE;
						plr->ExitObserver();
						pickedUpPerson = TRUE;
					}
					else if (plr->pev->health > 0)
					{
						// restock for those alive
						plr->pev->health = 100;
					}

					MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
						WRITE_BYTE( ENTINDEX(plr->edict()) );
						WRITE_SHORT( plr->pev->frags = 0 );
						WRITE_SHORT( plr->m_iDeaths = 0 );
						WRITE_SHORT( plr->m_iRoundWins );
						WRITE_SHORT( GetTeamIndex( plr->m_szTeamName ) + 1 );
					MESSAGE_END();
				}
			}

			// So monsters and incoming players dont clash
			if (pickedUpPerson)
			{
				m_fBeginWaveTime = gpGlobals->time + 3.0;
				flUpdateTime = gpGlobals->time + 1.5;
				UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("Players joining. Wave #%d begins soon.", m_iWaveNumber + 1));
				return;
			}

			SetRoundLimits();

			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_WAVE_BEGINS);
			MESSAGE_END();

			for (int i = 0; i < m_iEnemiesRemain; i++)
			{
				edict_t *m_pSpot = EntSelectSpawnPoint("info_player_deathmatch");

				if (m_pSpot == NULL)
				{
					ALERT(at_console, "Error creating monster. Spawn point not found.\n");
					continue;
				}

				int monsterRound = (m_iWaveNumber / ENEMY_TOTAL) % ARRAYSIZE(szMonsters);
				int index = monsterRound;
				char monster[64];

				// New enemies appear per wave.
				if (m_iWaveNumber % ENEMY_TOTAL > i)
					index = index + 1;

				index = fmin(fmax(0, index), ARRAYSIZE(szMonsters) - 1);
				strcpy(monster, szMonsters[index]);
				CBaseEntity *pEntity = CBaseEntity::Create(monster, m_pSpot->v.origin, m_pSpot->v.angles);

				CBaseEntity *ent = NULL;
				while ( (ent = UTIL_FindEntityInSphere( ent, m_pSpot->v.origin, 64 )) != NULL )
				{
					// if ent is a client, kill em (unless they are ourselves)
					if ( ent->IsAlive() && pEntity != ent )
					{
						ClearMultiDamage();
						if (ent->IsPlayer())
						{
							CBasePlayer *pl = (CBasePlayer *)ent;
							if (!pl->IsObserver())
							{
								ent->pev->health = 0; // without this, player can walk as a ghost.
								pl->Killed(pEntity->pev, VARS(INDEXENT(0)), GIB_ALWAYS);
							}
						}
						else if (strcmp(STRING(ent->pev->message), "horde"))
						{
							ent->pev->health = 0; // without this, player can walk as a ghost.
							ent->Killed(VARS(INDEXENT(0)), GIB_ALWAYS);
						}
					}
				}

				// Health increases all monsters.
				int hardness = (m_iWaveNumber / float(ARRAYSIZE(szMonsters) * ENEMY_TOTAL)) * 10;

				ALERT(at_aiconsole, ">>> [Horde] m_iWaveNumber=%d, index=%d, monsterRound=%d, i=%d, hardness=%d\n", m_iWaveNumber, index, monsterRound, i, hardness);

				if (pEntity)
				{
					m_iTotalEnemies++;
					ALERT(at_aiconsole, ">>> [Horde] created %s\n", monster);

					// Radar mark
					pEntity->pev->fuser4 = 2;
					pEntity->pev->message = MAKE_STRING("horde");

					if (hardness > 1)
					{
						ALERT(at_aiconsole, ">>> [Horde] hardness at %d, health was %.0f, now is %.0f\n", hardness, pEntity->pev->max_health, pEntity->pev->max_health * hardness);
						pEntity->pev->max_health = pEntity->pev->max_health * hardness;
						pEntity->pev->health = pEntity->pev->max_health;
					}
				}
			}

			m_iEnemiesRemain = m_iTotalEnemies;
			m_iWaveNumber++;

			if (m_iTotalEnemies)
			{
				UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("Wave %d begins!\n", m_iWaveNumber));
				MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
					WRITE_STRING(UTIL_VarArgs("Wave #%d", m_iWaveNumber));
					WRITE_STRING(UTIL_VarArgs("Enemies remain: %d", m_iEnemiesRemain));
					WRITE_BYTE(float(m_iEnemiesRemain) / (m_iTotalEnemies) * 100);
				MESSAGE_END();
			}

			m_fBeginWaveTime = 0;
		}

		// detect enemies
		edict_t *pEdict = FIND_ENTITY_BY_STRING(NULL, "message", "horde");
		m_iEnemiesRemain = 0;
		while (!FNullEnt(pEdict))
		{
			m_iEnemiesRemain++;

			if (pEdict->v.deadflag == DEAD_DEAD || pEdict->v.flags & EF_NODRAW)
				UTIL_Remove(CBaseEntity::Instance(pEdict));

			pEdict = FIND_ENTITY_BY_STRING(pEdict, "message", "horde");
		}

		if (m_iEnemiesRemain >= 1 && m_iTotalEnemies >= 1)
		{
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING(UTIL_VarArgs("Wave #%d", m_iWaveNumber));
				WRITE_STRING(UTIL_VarArgs("Enemies remain: %d", m_iEnemiesRemain));
				WRITE_BYTE(float(m_iEnemiesRemain) / (m_iTotalEnemies) * 100);
			MESSAGE_END();
		}

		// Enemies all dead, new round.
		if (m_iEnemiesRemain <= 0 && m_fBeginWaveTime < gpGlobals->time)
		{
			m_flRoundTimeLimit = 0;

			int highest = 1;
			BOOL IsEqual = FALSE;
			CBasePlayer *highballer = NULL;

			//find highest damage amount.
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
				UTIL_ClientPrintAll(HUD_PRINTCENTER,
					UTIL_VarArgs("%s doled the most enemy frags!\n", STRING(highballer->pev->netname)));
				MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
					WRITE_STRING(UTIL_VarArgs("Wave #%d Completed!", m_iWaveNumber));
					WRITE_STRING("");
					WRITE_BYTE(0);
					WRITE_STRING(UTIL_VarArgs("%s doled the most frags!\n", STRING(highballer->pev->netname)));
				MESSAGE_END();

				DisplayWinnersGoods( highballer );
			}
			else
			{
				UTIL_ClientPrintAll(HUD_PRINTCENTER, "Horde is dead!\nNo one has won!\n");
				UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends with no winners!\n");
				MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
					WRITE_STRING(UTIL_VarArgs("Wave #%d Completed!", m_iWaveNumber));
					WRITE_STRING("");
					WRITE_BYTE(0);
					WRITE_STRING("No one has won!");
				MESSAGE_END();
			}

			MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
				WRITE_BYTE(CLIENT_SOUND_WAVE_ENDED);
			MESSAGE_END();

			MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
				WRITE_BYTE(0);
			MESSAGE_END();

			// Reset frags, publish scores
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
				{
					MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
						WRITE_BYTE( ENTINDEX(plr->edict()) );
						WRITE_SHORT( plr->pev->frags = 0 );
						WRITE_SHORT( plr->m_iDeaths = 0 );
						WRITE_SHORT( plr->m_iRoundWins );
						WRITE_SHORT( GetTeamIndex( plr->m_szTeamName ) + 1 );
					MESSAGE_END();
					plr->m_iAssists = 0;
					
					plr->m_iRoundPlays++;
				}
			}

			m_iSuccessfulRounds++;

			m_fBeginWaveTime = gpGlobals->time + 3.0;
		}

		// survivors dead
		if ( survivors_left < 1 )
		{
			//stop timer / end game.
			m_flRoundTimeLimit = 0;
			g_GameInProgress = FALSE;
			MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
				WRITE_BYTE(0);
			MESSAGE_END();

			// remove enemies
			edict_t *pEdict = FIND_ENTITY_BY_STRING(NULL, "message", "horde");
			while (!FNullEnt(pEdict))
			{
				UTIL_Remove(CBaseEntity::Instance(pEdict));
				pEdict = FIND_ENTITY_BY_STRING(pEdict, "message", "horde");
			}

			//find highest frag amount.
			float highest = 1;
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
				UTIL_ClientPrintAll(HUD_PRINTCENTER,
					UTIL_VarArgs("Survivors have been defeated!\n\n%s doled the most enemy frags!\n",
					STRING(highballer->pev->netname)));
				MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
					WRITE_STRING(UTIL_VarArgs("Wave #%d Completed!", m_iWaveNumber));
					WRITE_STRING("");
					WRITE_BYTE(0);
					WRITE_STRING(UTIL_VarArgs("%s scored highest!", STRING(highballer->pev->netname)));
				MESSAGE_END();
				DisplayWinnersGoods( highballer );
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
				MESSAGE_END();
			}
			else
			{
				UTIL_ClientPrintAll(HUD_PRINTCENTER, "Survivors have been defeated!\n");
				UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends with no winners!\n");
				MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
					WRITE_STRING(UTIL_VarArgs("Wave #%d Completed!", m_iWaveNumber));
					WRITE_STRING("");
					WRITE_BYTE(0);
					WRITE_STRING("No one has won!");
				MESSAGE_END();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
				MESSAGE_END();
			}

			m_iSuccessfulRounds++;
			flUpdateTime = gpGlobals->time + 3.0;
			return;
		}

		flUpdateTime = gpGlobals->time + 1.5;
		return;
	}

	int clients = CheckClients();

	// Any persons
	if ( clients > 0 )
	{
		if ( m_fWaitForPlayersTime == -1 )
			m_fWaitForPlayersTime = gpGlobals->time + 15.0;

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
					WRITE_BYTE(CLIENT_SOUND_PREPAREFORBATTLE);
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
				UTIL_VarArgs("Prepare for Horde\n\n%i...\n", m_iCountDown));
			m_iCountDown--;
			flUpdateTime = gpGlobals->time + 1.0;
			return;
		}

		//frags + time.
		SetRoundLimits();

		g_GameInProgress = TRUE;

		InsertClientsIntoArena(0);

		m_fBeginWaveTime = gpGlobals->time + 3.0;

		m_iCountDown = 5;
		m_fWaitForPlayersTime = -1;

		// Disable timer for now
		MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
			WRITE_BYTE(0);
		MESSAGE_END();

		// Resend team info
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamNames );
			WRITE_BYTE( 2 );
			WRITE_STRING( "other" );
			WRITE_STRING( "survivors" );
		MESSAGE_END();

		UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("Horde has begun!\n"));
		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %d players have entered the arena!\n", clients));
	}
	else
	{
		SuckAllToSpectator();
		m_flRoundTimeLimit = 0;
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("Horde Mode");
			WRITE_STRING("Waiting for other players");
			WRITE_BYTE(0);
			WRITE_STRING(UTIL_VarArgs("%d Rounds", (int)roundlimit.value));
		MESSAGE_END();
		m_fWaitForPlayersTime = gpGlobals->time + 15.0;
	}

	flUpdateTime = gpGlobals->time + 1.0;
}

void CHalfLifeHorde::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Horde Mode");
			WRITE_STRING("");
			WRITE_BYTE(0);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
			WRITE_BYTE( 2 );
			WRITE_STRING( "other" );
			WRITE_STRING( "survivors" );
		MESSAGE_END();
	}
}

void CHalfLifeHorde::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);

	// Place player in spectator mode if joining during a game
	// Or if the game begins that requires spectators
	if ((g_GameInProgress && !pPlayer->IsInArena) || (!g_GameInProgress && IsRoundBased()))
	{
		return;
	}

	char *key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict());

	pPlayer->pev->fuser3 = 1; // bots need to identify their team.
	strncpy( pPlayer->m_szTeamName, "survivors", TEAM_NAME_LENGTH );
	//g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "team", pPlayer->m_szTeamName);

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

BOOL CHalfLifeHorde::HasGameTimerExpired( void )
{
	//time is up
	if ( CHalfLifeMultiplay::HasGameTimerExpired() )
	{
		int highest = 1;
		BOOL IsEqual = FALSE;
		CBasePlayer *highballer = NULL;

		//find highest damage amount.
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
			UTIL_ClientPrintAll(HUD_PRINTCENTER, 
				UTIL_VarArgs("Time is up!\n\n%s doled the most frags!\n",
				STRING(highballer->pev->netname)));
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING("Time is up!");
				WRITE_STRING("");
				WRITE_BYTE(0);
				WRITE_STRING(UTIL_VarArgs("%s doled the most frags!\n", STRING(highballer->pev->netname)));
			MESSAGE_END();
			DisplayWinnersGoods( highballer );

			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
			MESSAGE_END();
		}
		else
		{
			UTIL_ClientPrintAll(HUD_PRINTCENTER, "Time is up!\nNo one has won!\n");
			UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends with no winners!\n");
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING("Time is up!");
				WRITE_STRING("");
				WRITE_BYTE(0);
				WRITE_STRING("No one has won!");
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

BOOL CHalfLifeHorde::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if ( pPlayer->pev->fuser3 == pAttacker->pev->fuser3 )
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

void CHalfLifeHorde::FPlayerTookDamage( float flDamage, CBasePlayer *pVictim, CBaseEntity *pKiller)
{
	CBasePlayer *pPlayerAttacker = NULL;

	if (pKiller && pKiller->IsPlayer())
	{
		pPlayerAttacker = (CBasePlayer *)pKiller;
		if ( pPlayerAttacker != pVictim && pPlayerAttacker->pev->fuser3 && pVictim->pev->fuser3 )
		{
			ClientPrint(pPlayerAttacker->pev, HUD_PRINTCENTER, "Destroy the horde!\nNot your teammate!");
		}
	}
}

BOOL CHalfLifeHorde::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	if ( !pPlayer->m_flForceToObserverTime )
		pPlayer->m_flForceToObserverTime = gpGlobals->time + 3.0;

	return FALSE;
}

void CHalfLifeHorde::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);

	int survivors_left = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++) {
		//if (m_iPlayersInArena[i-1] > 0)
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(i);
			if (pPlayer && !pPlayer->IsSpectator() && pPlayer != pVictim && !pPlayer->HasDisconnected)
			{
				survivors_left++;
			}
		}
	}

	m_iSurvivorsRemain = survivors_left;

	// Person was survivor
	if (survivors_left >= 1)
	{
		UTIL_ClientPrintAll(HUD_PRINTTALK,
		UTIL_VarArgs("* %s has been defeated! %d survivors remain!\n",
		STRING(pVictim->pev->netname), survivors_left));

		MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
			WRITE_BYTE(CLIENT_SOUND_MASSACRE);
		MESSAGE_END();
	}
	else if (survivors_left == 0)
	{
		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* Survivors defeated by horde!\n"));
	}
}

int CHalfLifeHorde::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		if (!strcmp(pTeamName, "survivors"))
			return 1;
		else
			return 0; // other
	}
	
	return -1;	// No match
}

int CHalfLifeHorde::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() )
		return GR_NOTTEAMMATE;

	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

const char *CHalfLifeHorde::GetTeamID( CBaseEntity *pEntity )
{
	if ( pEntity == NULL || pEntity->pev == NULL )
		return "";

	// return their team name
	return pEntity->TeamID();
}

void CHalfLifeHorde::MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller )
{
	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );

	// Monster must be of the horde
	if (strcmp(STRING(pVictim->pev->message), "horde"))
		return;

	if ( pKiller && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayer*)ktmp;

	if (peKiller)
	{
		pKiller->frags += 1;

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(peKiller->edict()) );
			WRITE_SHORT( peKiller->pev->frags );
			WRITE_SHORT( peKiller->m_iDeaths );
			WRITE_SHORT( peKiller->m_iRoundWins );
			WRITE_SHORT( GetTeamIndex( peKiller->m_szTeamName) + 1 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgDeathMsg );
			WRITE_BYTE( ENTINDEX(ENT(pKiller)) );		// the killer
			WRITE_BYTE( -1 );							// the assist
			WRITE_BYTE( -1 );							// the victim
			WRITE_STRING( STRING(pVictim->pev->classname + 8) );			// what they were killed by (should this be a string?)
		MESSAGE_END();
	}
}

BOOL CHalfLifeHorde::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
{
	CBaseEntity *pTgt = CBaseEntity::Instance( target );
	if ( pTgt )
	{
		if ( strstr(STRING(pTgt->pev->classname), "monster_") )
			return TRUE;
	}

	return FALSE;
}

BOOL CHalfLifeHorde::IsRoundBased( void )
{
	return TRUE;
}
