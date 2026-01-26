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
extern int gmsgShowTimer;
extern int gmsgScoreInfo;
extern int gmsgDEraser;

CHalfLifeChilldemic::CHalfLifeChilldemic()
{
	m_iSurvivorsRemain = 0;
	m_iSkeletonsRemain = 0;
	PauseMutators();
}

void CHalfLifeChilldemic::DetermineWinner( void )
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

	if ( highballer )
	{
		if (!IsEqual)
		{
			UTIL_ClientPrintAll(HUD_PRINTCENTER,
				UTIL_VarArgs("%s doled the most frags!\n", STRING(highballer->pev->netname)));
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING("Chilldemic Completed!");
				WRITE_STRING(UTIL_VarArgs("%s win!", m_iSurvivorsRemain ? "Survivors" : "Skeletons"));
				WRITE_BYTE(0);
				WRITE_STRING(UTIL_VarArgs("%s doled the most frags!\n", STRING(highballer->pev->netname)));
			MESSAGE_END();

			DisplayWinnersGoods( highballer );
		}
		else
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && plr->IsInArena )
				{
					if ( plr->pev->frags == highest)
					{
						plr->m_iRoundWins++;
						plr->Celebrate();
					}
				}
			}

			UTIL_ClientPrintAll(HUD_PRINTCENTER, "Numerous victors!");
			UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends with winners!\n");
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING("Chilldemic Completed!");
				WRITE_STRING(UTIL_VarArgs("%s win!", m_iSurvivorsRemain ? "Survivors" : "Skeletons"));
				WRITE_BYTE(0);
				WRITE_STRING("Numerous victors!");
			MESSAGE_END();
		}
	}
	else
	{
		UTIL_ClientPrintAll(HUD_PRINTCENTER, "Round is over!\nNo one has won!\n");
		UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends with no winners!\n");
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("Chilldemic Completed!");
			WRITE_STRING("");
			WRITE_BYTE(0);
			WRITE_STRING("No one has won!");
		MESSAGE_END();
	}
}

void CHalfLifeChilldemic::Think( void )
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
					SuckToSpectator( plr );
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
					if ( plr->IsSpectator() && !plr->HasDisconnected )
					{
						MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
							if (m_iSurvivorsRemain >= 1 && m_iSkeletonsRemain <= 0)
								WRITE_STRING("Suvivors have won!");
							else if (m_iSkeletonsRemain >= 1 && m_iSurvivorsRemain <= 0)
								WRITE_STRING("Skeletons have won!");
							else
							{
								WRITE_STRING(UTIL_VarArgs("Skeletons left: %d", m_iSkeletonsRemain));
								WRITE_STRING(UTIL_VarArgs("Survivors left: %d", m_iSurvivorsRemain));
								WRITE_BYTE(float(m_iSurvivorsRemain) / (m_iPlayersInGame) * 100);
							}
							if (roundlimit.value > 0)
								WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
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
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Free their bones");
									WRITE_STRING(UTIL_VarArgs("Survivors alive: %d", survivors_left));
									WRITE_BYTE(float(survivors_left) / (m_iPlayersInGame) * 100);
									if (roundlimit.value > 0)
										WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
								MESSAGE_END();
							}
							else if (survivors_left == 1)
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Free their bones");
									WRITE_STRING("Dispatch the last soul!");
									WRITE_BYTE(0);
									if (roundlimit.value > 0)
										WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
								MESSAGE_END();
							}
							else
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Virus completed!");
									WRITE_STRING("");
									WRITE_BYTE(0);
									WRITE_STRING(UTIL_VarArgs("Skeletons win round %d of %d!", m_iSuccessfulRounds+1, (int)roundlimit.value));
								MESSAGE_END();
							}
						}
						else
						{
							if (survivors_left > 1)
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Survive");
									WRITE_STRING(UTIL_VarArgs("Survivors remain: %d", survivors_left));
									WRITE_BYTE(float(survivors_left) / (m_iPlayersInGame) * 100);
									if (roundlimit.value > 0)
										WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
								MESSAGE_END();
							}
							else
							{
								if (skeletons_left > 0)
								{
									MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
										WRITE_STRING("You remain! SURVIVE!");
										WRITE_STRING(UTIL_VarArgs("Skeletons remain: %d", skeletons_left));
										WRITE_BYTE(float(skeletons_left) / (m_iPlayersInGame) * 100);
										if (roundlimit.value > 0)
											WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
									MESSAGE_END();
								}
								else
								{
									MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
										WRITE_STRING("Virus eradicated!");
										WRITE_STRING("");
										WRITE_BYTE(0);
										WRITE_STRING(UTIL_VarArgs("Survivors win round %d of %d!", m_iSuccessfulRounds+1, (int)roundlimit.value));
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
			PauseMutators();
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
			//survivors all dead
			else if ( survivors_left <= 0 )
			{
				DetermineWinner();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
				MESSAGE_END();
			}
			//skeletons defeated.
			else if ( skeletons_left <= 0 )
			{
				DetermineWinner();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
				MESSAGE_END();
			}
			else if (survivors_left == 1)
			{
				DetermineWinner();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
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
				UTIL_VarArgs("Prepare for Chilldemic\n\n%i...\n", m_iCountDown));
			m_iCountDown--;
			m_iFirstBloodDecided = FALSE;
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
		
		// Restore mutators when round begins
		RestoreMutators();

		InsertClientsIntoArena(0);

		m_fSendArmoredManMessage = gpGlobals->time + 1.0;

		m_iCountDown = 5;
		m_fWaitForPlayersTime = -1;

		// Resend team info
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamNames );
			WRITE_BYTE( 2 );
			WRITE_STRING( "survivors" );
			WRITE_STRING( "skeleton" );
		MESSAGE_END();

		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %d players have entered the arena!\n", clients));
	}
	else
	{
		SuckAllToSpectator();
		m_flRoundTimeLimit = 0;
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("Chilldemic");
			WRITE_STRING("Waiting for other players");
			WRITE_BYTE(0);
			if (roundlimit.value > 0)
				WRITE_STRING(UTIL_VarArgs("%d Rounds", (int)roundlimit.value));
		MESSAGE_END();
		m_fWaitForPlayersTime = gpGlobals->time + roundwaittime.value;
	}

	flUpdateTime = gpGlobals->time + 1.0;
}

void CHalfLifeChilldemic::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Chilldemic");
			if (roundlimit.value > 0)
				WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
			else
				WRITE_STRING("");
			WRITE_BYTE(0);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
			WRITE_BYTE( 2 );
			WRITE_STRING( "survivors" );
			WRITE_STRING( "skeleton" );
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

BOOL CHalfLifeChilldemic::HasGameTimerExpired( void )
{
	//time is up
	if ( CHalfLifeMultiplay::HasGameTimerExpired() )
	{
		DetermineWinner();
		MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
			WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
		MESSAGE_END();

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

BOOL CHalfLifeChilldemic::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
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

void CHalfLifeChilldemic::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);

	CHalfLifeMultiplay::SavePlayerModel(pPlayer);

	// Place player in spectator mode if joining during a game
	// Or if the game begins that requires spectators
	if ((g_GameInProgress && !pPlayer->IsInArena) || (!g_GameInProgress && IsRoundBased()))
	{
		return;
	}

	// Default model selection
	char *key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict());
	char *mdls = g_engfuncs.pfnInfoKeyValue(key, "model");
	if (strcmp(mdls, "skeleton"))
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "pm", mdls);
	char *pmodel = g_engfuncs.pfnInfoKeyValue(key, "pm");

	if ( pPlayer->pev->fuser4 > 0 )
	{
		pPlayer->RemoveAllItems(FALSE);
		pPlayer->GiveNamedItem("weapon_vest");
		pPlayer->GiveNamedItem("weapon_chainsaw");
		pPlayer->pev->max_health = pPlayer->pev->health = 50;
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "1");

		strncpy( pPlayer->m_szTeamName, "skeleton", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "model", "skeleton");
		//g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "team", "skeleton");
		ClientPrint(pPlayer->pev, HUD_PRINTCENTER, "You are a skeleton, infect others!");
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

		strncpy( pPlayer->m_szTeamName, "survivors", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "model", modelName);
		//g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "team", "survivors");
		ClientPrint(pPlayer->pev, HUD_PRINTCENTER, "You are a survivor, fight skeletons!");
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

BOOL CHalfLifeChilldemic::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	// Skeletons can respawn if a survivor is left.
	if ( pPlayer->pev->fuser4 > 0 && m_iSurvivorsRemain >= 1 && !pPlayer->m_flForceToObserverTime )
		return TRUE;

	if ( !pPlayer->m_flForceToObserverTime )
		pPlayer->m_flForceToObserverTime = gpGlobals->time + 3.0;

	return FALSE;
}

void CHalfLifeChilldemic::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	// prevent skin/color/model changes
	char *mdls = g_engfuncs.pfnInfoKeyValue( infobuffer, "model" );
	int clientIndex = pPlayer->entindex();
	if ( pPlayer->pev->fuser4 != 1 && !stricmp( "skeleton", mdls ) )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "[Chilldemic]: Changing to 'skeleton' is not allowed\n");
		g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "iceman" );
		return;
	}

	// Enforce skeleton on infected players
	if ( pPlayer->pev->fuser4 == 1 && stricmp( "skeleton", mdls ) )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "[Chilldemic]: Changing back 'skeleton' due to infection\n");
		g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "skeleton" );
		return;
	}
}

void CHalfLifeChilldemic::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);

	int survivors_left = 0, skeletons_left = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++) {
		if (m_iPlayersInArena[i-1] > 0)
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(m_iPlayersInArena[i-1]);
			if (pPlayer && !pPlayer->IsSpectator() && pVictim != pPlayer && !pPlayer->HasDisconnected)
			{
				if (pPlayer->pev->fuser4 == 0)
					survivors_left++;
				else
					skeletons_left++;
			}
		}
	}

	m_iSurvivorsRemain = survivors_left;
	m_iSkeletonsRemain = skeletons_left;

	// Person was survivor
	if ( pVictim->pev->fuser4 == 0 )
	{
		pVictim->pev->frags = 0; // clear immediately for winner determination
		if (survivors_left >= 1)
		{
			UTIL_ClientPrintAll(HUD_PRINTTALK,
			UTIL_VarArgs("* %s has been infected! %d Survivors remain!\n",
			STRING(pVictim->pev->netname), survivors_left));

			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_MASSACRE);
			MESSAGE_END();
		}
		else if (survivors_left == 0)
		{
			UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* Survivors defeated!\n"));
		}

		pVictim->pev->fuser4 = 1;

		g_engfuncs.pfnSetClientKeyValue( ENTINDEX( pVictim->edict() ),
			g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model", "skeleton" );
		strncpy( pVictim->m_szTeamName, "skeleton", TEAM_NAME_LENGTH );
	}
	else
	{
		// Special case, last survivor, dispatched skeletons sent to observer.
		if (m_iSurvivorsRemain <= 1 && !pVictim->HasDisconnected)
		{
			pVictim->pev->frags = 0; // clear immediately for winner determination
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

	if (pPlayer->pev->fuser4 > 0 &&
		strcmp(STRING(pItem->pev->classname), "weapon_fists") &&
		strcmp(STRING(pItem->pev->classname), "weapon_chainsaw") &&
		strcmp(STRING(pItem->pev->classname), "weapon_knife") &&
		strcmp(STRING(pItem->pev->classname), "weapon_crowbar") &&
		strcmp(STRING(pItem->pev->classname), "weapon_wrench") &&
		strcmp(STRING(pItem->pev->classname), "weapon_dual_wrench"))
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

BOOL CHalfLifeChilldemic::CanRandomizeWeapon( const char *name )
{
	if (strcmp(name, "weapon_nuke") == 0)
		return FALSE;

	return TRUE;
}

int CHalfLifeChilldemic::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() )
		return GR_NOTTEAMMATE;

	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

const char *CHalfLifeChilldemic::GetTeamID( CBaseEntity *pEntity )
{
	if ( pEntity == NULL || pEntity->pev == NULL )
		return "";

	// return their team name
	return pEntity->TeamID();
}

BOOL CHalfLifeChilldemic::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
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

BOOL CHalfLifeChilldemic::IsRoundBased( void )
{
	return TRUE;
}
