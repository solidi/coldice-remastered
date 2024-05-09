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
#include "shidden_gamerules.h"
#include "game.h"

extern int gmsgObjective;
extern int gmsgTeamNames;
extern int gmsgTeamInfo;
extern int gmsgScoreInfo;
extern int gmsgPlayClientSound;
extern int gmsgShowTimer;
extern int gmsgStatusIcon;
extern int gmsgRoundTime;

CHalfLifeShidden::CHalfLifeShidden()
{
	m_iSmeltersRemain = 0;
	m_iDealtersRemain = 0;
}

void CHalfLifeShidden::Think( void )
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
		int smelters_left = 0;
		int dealters_left = 0;

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
					plr->StartObserver(plr->pev->origin, VARS(pentSpawnSpot)->angles);
					plr->m_flForceToObserverTime = 0;
				}

				if ( plr->IsInArena && !plr->IsSpectator() /*&& plr->IsAlive()*/ )
				{
					if (plr->pev->fuser4 > 0)
						dealters_left++;
					else
						smelters_left++;
				}
				/*
				else
				{
					//for clients who connected while game in progress.
					if ( plr->IsSpectator() )
					{
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
							if (m_iSmeltersRemain >= 1 && m_iDealtersRemain <= 0)
								WRITE_STRING("Smelters have won!");
							else if (m_iDealtersRemain >= 1 && m_iSmeltersRemain <= 0)
								WRITE_STRING("Dealters have won!");
							else
							{
								WRITE_STRING(UTIL_VarArgs("Dealters left: %d", m_iDealtersRemain));
								WRITE_STRING(UTIL_VarArgs("Smelters left: %d", m_iSmeltersRemain));
								WRITE_BYTE(float(m_iSmeltersRemain) / (m_iPlayersInGame) * 100);
							}
						MESSAGE_END();
					} else {
						// Send them to observer
						plr->m_flForceToObserverTime = gpGlobals->time;
					}
				}
				*/
			}
		}

		m_iSmeltersRemain = smelters_left;
		m_iDealtersRemain = dealters_left;

		// Skeleton messages
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
			{
				if ( plr->IsInArena && !plr->IsSpectator() /*&& plr->IsAlive()*/ )
				{
					if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
					{
						if (plr->pev->fuser4 > 0)
						{
							if (smelters_left > 1)
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Fart on 'em");
									WRITE_STRING(UTIL_VarArgs("Smelters alive: %d", smelters_left));
									WRITE_BYTE(float(smelters_left) / (m_iPlayersInGame) * 100);
								MESSAGE_END();
							}
							else if (smelters_left == 1)
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Fart on 'em");
									WRITE_STRING("Dispatch the last smelter!");
									WRITE_BYTE(0);
								MESSAGE_END();
							}
							else
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Dealting completed!");
									WRITE_STRING("");
									WRITE_BYTE(0);
									WRITE_STRING(UTIL_VarArgs("Dealters win round %d of %d!", m_iSuccessfulRounds+1, (int)roundlimit.value));
								MESSAGE_END();
							}
						}
						else
						{
							if (dealters_left > 1)
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Smell it");
									WRITE_STRING(UTIL_VarArgs("Dealters remain: %d", dealters_left));
									WRITE_BYTE(float(dealters_left) / (m_iPlayersInGame) * 100);
								MESSAGE_END();
							}
							else
							{
								if (dealters_left > 0)
								{
									MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
										WRITE_STRING("Find that dealter!");
										WRITE_STRING(UTIL_VarArgs("Dealter remain: %d", dealters_left));
										WRITE_BYTE(float(dealters_left) / (m_iPlayersInGame) * 100);
									MESSAGE_END();
								}
								else
								{
									MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
										WRITE_STRING("Dealters eradicated!");
										WRITE_STRING("");
										WRITE_BYTE(0);
										WRITE_STRING(UTIL_VarArgs("Smelters win round %d of %d!", m_iSuccessfulRounds+1, (int)roundlimit.value));
									MESSAGE_END();
								}
							}
						}
					}
				}
				else
				{
					//for clients who connected while game in progress.
					if ( plr->IsSpectator() )
					{
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
							if (m_iSmeltersRemain >= 1 && m_iDealtersRemain <= 0)
								WRITE_STRING("Smelters have won!");
							else if (m_iDealtersRemain >= 1 && m_iSmeltersRemain <= 0)
								WRITE_STRING("Dealters have won!");
							else
							{
								WRITE_STRING(UTIL_VarArgs("Dealters left: %d", m_iDealtersRemain));
								WRITE_STRING(UTIL_VarArgs("Smelters left: %d", m_iSmeltersRemain));
								WRITE_BYTE(float(m_iSmeltersRemain) / (m_iPlayersInGame) * 100);
							}
						MESSAGE_END();
					} else {
						// Send them to observer
						plr->m_flForceToObserverTime = gpGlobals->time;
					}
				}
			}
		}

		// Skeleton icon
		if (m_fSendArmoredManMessage < gpGlobals->time)
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && !plr->HasDisconnected && plr->pev->fuser4 > 0 )
				{
					if (!FBitSet(plr->pev->flags, FL_FAKECLIENT) && !plr->IsSpectator())
					{
						MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, plr->pev );
							WRITE_BYTE(1);
							WRITE_STRING("dealter");
							WRITE_BYTE(0);
							WRITE_BYTE(160);
							WRITE_BYTE(255);
						MESSAGE_END();
					}
				}
			}
		}

		// semlters dead or dealters defeated
		if ( smelters_left < 1 || dealters_left < 1 )
		{
			//stop timer / end game.
			m_flRoundTimeLimit = 0;
			g_GameInProgress = FALSE;
			MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
				WRITE_BYTE(0);
			MESSAGE_END();

			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && !plr->HasDisconnected && plr->pev->fuser4 > 0 )
				{
					if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
					{
						MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, plr->pev );
							WRITE_BYTE(0);
							WRITE_STRING("dealter");
						MESSAGE_END();
					}
				}
			}

			//everyone died.
			if ( smelters_left <= 0 && dealters_left <= 0 )
			{
				UTIL_ClientPrintAll(HUD_PRINTCENTER, "Everyone has been killed!\n");
				UTIL_ClientPrintAll(HUD_PRINTTALK, "* No winners in this round!\n");
				MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
					WRITE_STRING("Everyone died!");
					WRITE_STRING("");
					WRITE_BYTE(0);
					WRITE_STRING("No winners in this round!");
				MESSAGE_END();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
				MESSAGE_END();
			}
			//smelters all dead
			else if ( smelters_left <= 0 )
			{
				//find highest frag amount.
				float highest = 1;
				BOOL IsEqual = FALSE;
				CBasePlayer *highballer = NULL;

				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

					if ( plr && plr->IsPlayer() && plr->IsInArena && plr->pev->fuser4 > 0 )
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
						UTIL_VarArgs("Smelters have been defeated!\n\n%s doled the most gas!\n",
						STRING(highballer->pev->netname)));
					DisplayWinnersGoods( highballer );
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
					MESSAGE_END();
				}
				else
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER, "Smelters have been defeated!\n");
					UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends in a tie!\n");
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
					MESSAGE_END();
				}
			}
			//dealters defeated.
			else if ( dealters_left <= 0 )
			{
				//find highest frag amount.
				float highest = 1;
				BOOL IsEqual = FALSE;
				CBasePlayer *highballer = NULL;

				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

					if ( plr && plr->IsPlayer() && plr->IsInArena && plr->pev->fuser4 == 0 )
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
						UTIL_VarArgs("Dealters have been defeated!\n\n%s doled the most Lysol!\n",
						STRING(highballer->pev->netname)));
					DisplayWinnersGoods( highballer );
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
					MESSAGE_END();
				}
				else
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER, "Dealters have been defeated!\n");
					UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends in a tie!\n");
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
					MESSAGE_END();
				}
			}

			m_iSuccessfulRounds++;
			flUpdateTime = gpGlobals->time + 5.0;
			return;
		}

		flUpdateTime = gpGlobals->time + 3.0;
		return;
	}

	int clients = CheckClients();

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
			if (m_iCountDown == 2) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_PREPAREFORBATTLE);
				MESSAGE_END();
			}
			SuckAllToSpectator(); // in case players join during a countdown.
			UTIL_ClientPrintAll(HUD_PRINTCENTER,
				UTIL_VarArgs("Prepare for Shidden\n\n%i...\n", m_iCountDown));
			m_iCountDown--;
			flUpdateTime = gpGlobals->time + 1.0;
			return;
		}

		//frags + time.
		SetRoundLimits();

		// Balance teams
  		// Implementing Fisher–Yates shuffle
		int i, j, tmp; // create local variables to hold values for shuffle
		int count = 0;
		int player[32];

		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
				player[count++] = i;
		}

		for (i = count - 1; i > 0; i--) { // for loop to shuffle
			j = rand() % (i + 1); //randomise j for shuffle with Fisher Yates
			tmp = player[j];
			player[j] = player[i];
			player[i] = tmp;
		}

		for ( int i = 0; i < count; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( player[i] );

			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
				plr->pev->fuser4 = i % 2;
		}

		g_GameInProgress = TRUE;

		InsertClientsIntoArena(0);

		m_fSendArmoredManMessage = gpGlobals->time + 1.0;

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
			WRITE_BYTE( 2 );
			WRITE_STRING( "smelters" );
			WRITE_STRING( "dealters" );
		MESSAGE_END();

		UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("Shidden has begun!\n"));
		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %d players have entered the arena!\n", clients));
	}
	else
	{
		SuckAllToSpectator();
		m_flRoundTimeLimit = 0;
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("Shidden");
			WRITE_STRING("Waiting for other players");
			WRITE_BYTE(0);
			WRITE_STRING(UTIL_VarArgs("%d Rounds", (int)roundlimit.value));
		MESSAGE_END();
		m_fWaitForPlayersTime = gpGlobals->time + 17.0;
	}

	flUpdateTime = gpGlobals->time + 1.0;
}

void CHalfLifeShidden::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("The Shidden");
			WRITE_STRING("");
			WRITE_BYTE(0);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
			WRITE_BYTE( 2 );
			WRITE_STRING( "smelters" );
			WRITE_STRING( "dealters" );
		MESSAGE_END();
	}
}

void CHalfLifeShidden::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);

	// Place player in spectator mode if joining during a game
	// Or if the game begins that requires spectators
	if ((g_GameInProgress && !pPlayer->IsInArena) || (!g_GameInProgress && HasSpectators()))
	{
		return;
	}

	char *key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict());

	if ( pPlayer->pev->fuser4 > 0 )
	{
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "1");
		pPlayer->pev->fuser3 = 1; // bots need to identify their team.
		pPlayer->pev->gravity = 0.25;
		pPlayer->MakeInvisible();
		strncpy( pPlayer->m_szTeamName, "dealters", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "team", pPlayer->m_szTeamName);
	}
	else
	{
		strncpy( pPlayer->m_szTeamName, "smelters", TEAM_NAME_LENGTH );
		pPlayer->pev->fuser3 = 0; // bots need to identify their team.
		pPlayer->MakeVisible();
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "team", pPlayer->m_szTeamName);
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

void CHalfLifeShidden::PlayerThink( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerThink(pPlayer);

	if (pPlayer->pev->fuser4 > 0)
	{
		if (pPlayer->pev->rendermode == kRenderNormal)
		{
			pPlayer->MakeInvisible();
		}
	}
}

BOOL CHalfLifeShidden::HasGameTimerExpired( void )
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

			if ( plr && plr->IsPlayer() && plr->IsInArena && plr->pev->fuser4 == 0)
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
				UTIL_VarArgs("Time is up!\n\nSurvivor %s doled the most frags!\n",
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
			UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends in a tie!\n");
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
		flUpdateTime = gpGlobals->time + 5.0;
		m_flRoundTimeLimit = 0;
		return TRUE;
	}

	return FALSE;
}

BOOL CHalfLifeShidden::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if ( pPlayer->pev->fuser4 == pAttacker->pev->fuser4 )
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

void CHalfLifeShidden::FPlayerTookDamage( float flDamage, CBasePlayer *pVictim, CBaseEntity *pKiller)
{
	CBasePlayer *pPlayerAttacker = NULL;

	if (pKiller && pKiller->IsPlayer())
	{
		pPlayerAttacker = (CBasePlayer *)pKiller;
		if ( pPlayerAttacker != pVictim && !pPlayerAttacker->pev->fuser4 && !pVictim->pev->fuser4 )
		{
			ClientPrint(pPlayerAttacker->pev, HUD_PRINTCENTER, "Destroy the Shidden!\nNot your teammate!");
		}
	}
}

BOOL CHalfLifeShidden::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	if ( !pPlayer->m_flForceToObserverTime )
		pPlayer->m_flForceToObserverTime = gpGlobals->time + 3.0;

	return FALSE;
}

void CHalfLifeShidden::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);

	int smelters_left = 0;
	int dealters_left = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++) {
		if (m_iPlayersInArena[i-1] > 0)
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(m_iPlayersInArena[i-1]);
			if (pPlayer && !pPlayer->IsSpectator() && pPlayer != pVictim && !pPlayer->HasDisconnected)
			{
				if (pPlayer->pev->fuser4 == 0)
					smelters_left++;
				else
					dealters_left++;
			}
		}
	}

	m_iSmeltersRemain = smelters_left;
	m_iDealtersRemain = dealters_left;

	// Person was survivor
	if ( pVictim->pev->fuser4 == 0 )
	{
		if (smelters_left >= 1)
		{
			UTIL_ClientPrintAll(HUD_PRINTTALK,
			UTIL_VarArgs("* %s has been dealted! %d Smelters remain!\n",
			STRING(pVictim->pev->netname), smelters_left));

			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_MASSACRE);
			MESSAGE_END();
		}
		else if (smelters_left == 0)
		{
			UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* Smelters defeated!\n"));
		}
	}
}

BOOL CHalfLifeShidden::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if (!strcmp(STRING(pItem->pev->classname), "weapon_nuke"))
		return FALSE;

	return CHalfLifeMultiplay::CanHavePlayerItem( pPlayer, pItem );
}

int CHalfLifeShidden::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		if (!strcmp(pTeamName, "dealters"))
			return 1;
		else
			return 0; // smelters
	}
	
	return -1;	// No match
}

BOOL CHalfLifeShidden::CanRandomizeWeapon( const char *name )
{
	if (strcmp(name, "weapon_nuke") == 0)
		return FALSE;

	return TRUE;
}

int CHalfLifeShidden::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() )
		return GR_NOTTEAMMATE;

	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

const char *CHalfLifeShidden::GetTeamID( CBaseEntity *pEntity )
{
	if ( pEntity == NULL || pEntity->pev == NULL )
		return "";

	// return their team name
	return pEntity->TeamID();
}
