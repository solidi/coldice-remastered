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
#include "chilldemic_gamerules.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"

extern int gmsgPlayClientSound;
extern int gmsgStatusIcon;
extern int gmsgTeamInfo;
extern int gmsgTeamNames;
extern int gmsgObjective;
extern int gmsgShowTime;
extern int gmsgRoundTime;
extern int gmsgShowTimer;
extern int gmsgScoreInfo;

CHalfLifeChilldemic::CHalfLifeChilldemic()
{
	m_iSurvivorsRemain = 0;
}

void CHalfLifeChilldemic::Think( void )
{
	CHalfLifeMultiplay::Think();

	if ( flUpdateTime > gpGlobals->time )
		return;

	if ( m_flRoundTimeLimit )
	{
		if ( CheckGameTimer() )
			return;
	}

	if ( g_GameInProgress )
	{
		int survivors_left = 0;
		int skeletons_left = 0;

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
						skeletons_left++;
					else
						survivors_left++;
				}
				else
				{
					//for clients who connected while game in progress.
					if ( plr->IsSpectator() )
					{
						MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
							WRITE_STRING("Chilldemic in progress");
							WRITE_STRING(UTIL_VarArgs("Survivors left: %d", m_iSurvivorsRemain));
							WRITE_BYTE(float(m_iSurvivorsRemain) / (m_iPlayersInGame) * 100);
						MESSAGE_END();
					} else {
						// Send them to observer
						plr->m_flForceToObserverTime = gpGlobals->time;
					}
				}
			}
		}

		m_iSurvivorsRemain = survivors_left;

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
							if (survivors_left > 1)
							{
								MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Free their bones");
									WRITE_STRING(UTIL_VarArgs("Survivors alive: %d", survivors_left));
									WRITE_BYTE(float(survivors_left) / (m_iPlayersInGame) * 100);
								MESSAGE_END();
							}
							else if (survivors_left == 1)
							{
								MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Free their bones");
									WRITE_STRING("Dispatch the last soul!");
									WRITE_BYTE(0);
								MESSAGE_END();
							}
							else
							{
								MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Virus completed!");
									WRITE_STRING("Skeletons win!");
									WRITE_BYTE(0);
								MESSAGE_END();
							}
						}
						else
						{
							if (survivors_left > 1)
							{
								MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Survive");
									WRITE_STRING(UTIL_VarArgs("Survivors remain: %d", survivors_left));
									WRITE_BYTE(float(survivors_left) / (m_iPlayersInGame) * 100);
								MESSAGE_END();
							}
							else
							{
								if (skeletons_left > 0)
								{
									MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
										WRITE_STRING("You remain! SURVIVE!");
										WRITE_STRING(UTIL_VarArgs("Skeletons remain: %d", skeletons_left));
										WRITE_BYTE(float(skeletons_left) / (m_iPlayersInGame) * 100);
									MESSAGE_END();
								}
								else
								{
									MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
										WRITE_STRING("Virus eradicated!");
										WRITE_STRING("");
										WRITE_BYTE(0);
										WRITE_STRING("Survivors win!");
									MESSAGE_END();
								}
							}
						}
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
							WRITE_STRING("skeleton");
							WRITE_BYTE(0);
							WRITE_BYTE(160);
							WRITE_BYTE(255);
						MESSAGE_END();
					}
				}
			}
		}

		// Survivors dead or skeletons defeated
		if ( survivors_left < 1 || skeletons_left < 1 )
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
							WRITE_STRING("skeleton");
						MESSAGE_END();
					}
				}
			}

			//everyone died.
			if ( survivors_left <= 0 && skeletons_left <= 0 )
			{
				UTIL_ClientPrintAll(HUD_PRINTCENTER, "Everyone has been killed!\n");
				UTIL_ClientPrintAll(HUD_PRINTTALK, "* No winners in this round!");
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
				MESSAGE_END();
			}
			//survivors all dead
			else if ( survivors_left <= 0 )
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
					UTIL_ClientPrintAll(HUD_PRINTCENTER,
						UTIL_VarArgs("Survivors have been defeated!\n\n%s doled the most kills!\n",
						STRING(highballer->pev->netname)));
					DisplayWinnersGoods( highballer );
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
					MESSAGE_END();
				}
				else
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER, "Survivors have been defeated!\n");
					UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends in a tie!");
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
					MESSAGE_END();
				}
			}
			//skeletons defeated.
			else if ( skeletons_left <= 0 )
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
					UTIL_ClientPrintAll(HUD_PRINTCENTER,
						UTIL_VarArgs("Skeletons have been defeated!\n\n%s doled the most kills!\n",
						STRING(highballer->pev->netname)));
					DisplayWinnersGoods( highballer );
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
					MESSAGE_END();
				}
				else
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER, "Skeletons have been defeated!\n");
					UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends in a tie!");
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
					MESSAGE_END();
				}
			}

			flUpdateTime = gpGlobals->time + 5.0;
			return;
		}

		flUpdateTime = gpGlobals->time + 3.0;
		return;
	}

	int clients = CheckClients();

	if ( clients > 1 )
	{
		if ( m_iCountDown > 0 )
		{
			if (m_iCountDown == 2) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_PREPAREFORBATTLE);
				MESSAGE_END();
			}
			SuckAllToSpectator(); // in case players join during a countdown.
			UTIL_ClientPrintAll(HUD_PRINTCENTER,
				UTIL_VarArgs("Prepare for Chilldemic\n\n%i...\n", m_iCountDown));
			m_iCountDown--;
			flUpdateTime = gpGlobals->time + 1.0;
			return;
		}

		//frags + time.
		SetRoundLimits();

		int skeleton = m_iPlayersInArena[RANDOM_LONG(0, clients-1)];
		ALERT(at_console, "clients set to %d, virus set to index=%d\n", clients, skeleton);
		CBasePlayer *pl = (CBasePlayer *)UTIL_PlayerByIndex( skeleton );
		pl->pev->fuser4 = 1;

		g_GameInProgress = TRUE;

		InsertClientsIntoArena();

		m_fSendArmoredManMessage = gpGlobals->time + 1.0;

		m_iCountDown = 3;

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
			WRITE_STRING( "survivors" );
			WRITE_STRING( "skeleton" );
		MESSAGE_END();

		UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("Chilldemic has begun!\n%s is the first skeleton!\n", STRING(pl->pev->netname)));
		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %d players have entered the arena!\n", clients));
	}
	else
	{
		SuckAllToSpectator();
		m_flRoundTimeLimit = 0;
		MESSAGE_BEGIN(MSG_ALL, gmsgObjective);
			WRITE_STRING("Chilldemic");
			WRITE_STRING("Waiting for other players");
			WRITE_BYTE(0);
			WRITE_STRING(UTIL_VarArgs("%d Rounds", (int)roundlimit.value));
		MESSAGE_END();
	}

	flUpdateTime = gpGlobals->time + 1.0;
}

void CHalfLifeChilldemic::InitHUD( CBasePlayer *pl )
{
	CHalfLifeMultiplay::InitHUD( pl );

	if (!FBitSet(pl->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pl->edict());
			WRITE_STRING("Survive");
			WRITE_STRING("");
			WRITE_BYTE(0);
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgTeamNames, NULL, pl->edict() );
			WRITE_BYTE( 2 );
			WRITE_STRING( "survivors" );
			WRITE_STRING( "skeleton" );
		MESSAGE_END();
	}
}

BOOL CHalfLifeChilldemic::CheckGameTimer( void )
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
			UTIL_ClientPrintAll(HUD_PRINTCENTER, 
				UTIL_VarArgs("Time is up!\n\nSurvivor %s doled the most frags!\n",
				STRING(highballer->pev->netname)));
			DisplayWinnersGoods( highballer );

			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
			MESSAGE_END();
		}
		else
		{
			UTIL_ClientPrintAll(HUD_PRINTCENTER, "Time is up!\nNo one has won!\n");
			UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends in a tie!");
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
			MESSAGE_END();
		}

		g_GameInProgress = FALSE;
		MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
			WRITE_BYTE(0);
		MESSAGE_END();

		flUpdateTime = gpGlobals->time + 5.0;
		m_flRoundTimeLimit = 0;
		return TRUE;
	}

	return FALSE;
}

void CHalfLifeChilldemic::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	if ( pPlayer->pev->fuser4 > 0 )
	{
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ),
			g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "skeleton" );
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ),
			g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "team", "skeleton" );
		
		strncpy( pPlayer->m_szTeamName, "skeleton", TEAM_NAME_LENGTH );
	}
	else
	{
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pPlayer->edict() ),
			g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "team", "survivors" );
		strncpy( pPlayer->m_szTeamName, "survivors", TEAM_NAME_LENGTH );
	}

	// notify everyone's HUD of the team change
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_STRING( pPlayer->IsSpectator() ? "" : pPlayer->m_szTeamName );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( 0 );
		WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
	MESSAGE_END();
}

BOOL CHalfLifeChilldemic::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
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

void CHalfLifeChilldemic::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);

	// Place player in spectator mode if joining during a game
	// Or if the game begins that requires spectators
	if ((g_GameInProgress && !pPlayer->IsInArena) || (!g_GameInProgress && HasSpectators()))
	{
		return;
	}

	// Default model selection
	char *key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict());
	char *mdls = g_engfuncs.pfnInfoKeyValue(key, "model");
	if (strcmp(mdls, "skeleton"))
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "pmodel", mdls);
	char *pmodel = g_engfuncs.pfnInfoKeyValue(key, "pmodel");

	if ( pPlayer->pev->fuser4 > 0 )
	{
		pPlayer->RemoveAllItems(FALSE);
		pPlayer->GiveNamedItem("weapon_vest");
		pPlayer->GiveNamedItem("weapon_chainsaw");
		pPlayer->pev->max_health = pPlayer->pev->health = 50;
		pPlayer->pev->maxspeed = CVAR_GET_FLOAT("sv_maxspeed");
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "1");
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "model", "skeleton");
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "team", "skeleton");
		strncpy( pPlayer->m_szTeamName, "skeleton", TEAM_NAME_LENGTH );
	}
	else
	{
		char *defaultPlayerModels[4] = { "iceman", "commando", "jesus", "hhev", };
		char modelName[32];
		if (pmodel && (strlen(pmodel) && strlen(modelName) < 31))
			strcpy(modelName, pmodel);
		else
			strcpy(modelName, defaultPlayerModels[RANDOM_LONG(0,3)]);
		pPlayer->GiveRandomWeapon("weapon_nuke");
		pPlayer->pev->maxspeed = CVAR_GET_FLOAT("sv_maxspeed") * .5;
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "model", modelName);
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "team", "survivors");
		strncpy( pPlayer->m_szTeamName, "survivors", TEAM_NAME_LENGTH );
	}
}

BOOL CHalfLifeChilldemic::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	// Skeletons can respawn if a survivor is left.
	if ( pPlayer->pev->fuser4 > 0 && m_iSurvivorsRemain >= 1 && !pPlayer->m_flForceToObserverTime )
		return TRUE;

	if ( !pPlayer->m_flForceToObserverTime )
		pPlayer->m_flForceToObserverTime = gpGlobals->time + 3.0;

	return FALSE;
}

void CHalfLifeChilldemic::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);

	int survivors_left = 0;
	int skeletons_left = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++) {
		if (m_iPlayersInArena[i-1] > 0)
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(m_iPlayersInArena[i-1]);
			if (pPlayer && !pPlayer->IsSpectator() /*pPlayer->IsAlive()*/ && !pPlayer->HasDisconnected)
			{
				if (pPlayer->pev->fuser4 == 0)
					survivors_left++;
				else
					skeletons_left++;
			}
		}
	}

	m_iSurvivorsRemain = survivors_left;

	// Person was survivor
	if ( pVictim->pev->fuser4 == 0 )
	{
		UTIL_ClientPrintAll(HUD_PRINTTALK,
			UTIL_VarArgs("* %s has been infected! %d Survivors remain!\n",
			STRING(pVictim->pev->netname), survivors_left - 1));
		if (survivors_left >= 1)
		{
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_MASSACRE);
			MESSAGE_END();
		}

		pVictim->pev->fuser4 = 1;

		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pVictim->edict() ),
			g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model", "skeleton" );
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pVictim->edict() ),
			g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "team", "skeleton" );
		
		strncpy( pVictim->m_szTeamName, "skeleton", TEAM_NAME_LENGTH );
	}
	else
	{
		// Special case, last survivor, dispatched skeletons sent to observer.
		if (m_iSurvivorsRemain <= 1)
		{
			pVictim->m_flForceToObserverTime = gpGlobals->time + 2.0;
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pVictim->pev );
				WRITE_BYTE(0);
				WRITE_STRING("skeleton");
			MESSAGE_END();
		}
	}
}

BOOL CHalfLifeChilldemic::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if (!strcmp(STRING(pItem->pev->classname), "weapon_nuke"))
		return FALSE;

	return CHalfLifeMultiplay::CanHavePlayerItem( pPlayer, pItem );
}

int CHalfLifeChilldemic::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		if (!strcmp(pTeamName, "skeleton"))
			return 1;
		else
			return 0; // survivors
	}
	
	return -1;	// No match
}
