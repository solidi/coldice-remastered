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
#include "jvs_gamerules.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"

extern int gmsgPlayClientSound;
extern int gmsgStatusIcon;
extern int gmsgTeamInfo;
extern int gmsgTeamNames;
extern int gmsgScoreInfo;
extern int gmsgObjective;
extern int gmsgShowTimer;
extern int gmsgDEraser;
extern int gmsgBanner;

CHalfLifeJesusVsSanta::CHalfLifeJesusVsSanta()
{
	pArmoredMan = NULL;
	PauseMutators();
}

void CHalfLifeJesusVsSanta::DetermineWinner( void )
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
				WRITE_STRING("JVS Completed!");
				WRITE_STRING(UTIL_VarArgs("%s win!", pArmoredMan && pArmoredMan->IsAlive() ? "Jesus" : "Santas"));
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
				WRITE_STRING("JVS Completed!");
				WRITE_STRING(UTIL_VarArgs("%s win!", pArmoredMan && pArmoredMan->IsAlive() ? "Jesus" : "Santas"));
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
			WRITE_STRING("JVS Completed!");
			WRITE_STRING("");
			WRITE_BYTE(0);
			WRITE_STRING("No one has won!");
		MESSAGE_END();
	}
}

void CHalfLifeJesusVsSanta::Think( void )
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
		int clients_alive = 0;

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

				if ( plr->IsInArena && plr->IsAlive() )
				{
					clients_alive++;
				}
				else
				{
					//for clients who connected while game in progress.
					if ( !plr->IsSpectator() )
					{
						// Send them to observer
						plr->m_flForceToObserverTime = gpGlobals->time;
					}
				}
			}
		}

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( plr && plr->IsPlayer() && !plr->HasDisconnected && !FBitSet(plr->pev->flags, FL_FAKECLIENT))
			{
				if (pArmoredMan && plr == pArmoredMan)
				{
					if ((clients_alive - 1) >= 1)
					{
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
							WRITE_STRING("Defeat all Santas");
							WRITE_STRING(UTIL_VarArgs("Santas alive: %d", clients_alive - 1));
							WRITE_BYTE(float(clients_alive - 1) / (m_iPlayersInGame - 1) * 100);
							if (roundlimit.value > 0)
								WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
						MESSAGE_END();
					}
				}
				else if ( plr->IsSpectator() )
				{
					if (pArmoredMan && (clients_alive - 1) >= 1)
					{
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
							WRITE_STRING("Jesus vs Santa in progress");
							WRITE_STRING(UTIL_VarArgs("Jesus: %s (%.0f/%.0f)\n",
								STRING(pArmoredMan->pev->netname),
								pArmoredMan->pev->health,
								pArmoredMan->pev->armorvalue ));
							WRITE_BYTE((pArmoredMan->pev->health / pArmoredMan->pev->max_health) * 100);
							WRITE_STRING(UTIL_VarArgs("Santas remain: %d", clients_alive - 1));
						MESSAGE_END();
					}
				}
				else
				{
					if ((clients_alive - 1) >= 1)
					{
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
							WRITE_STRING(UTIL_VarArgs("Defeat %s as Jesus", STRING(pArmoredMan->pev->netname)));
							WRITE_STRING(UTIL_VarArgs("Santas remain: %d", clients_alive - 1));
							WRITE_BYTE(float(clients_alive - 1) / (m_iPlayersInGame - 1) * 100);
							if (roundlimit.value > 0)
								WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
						MESSAGE_END();
					}
				}
			}
		}

		if (m_fSendArmoredManMessage != -1 && m_fSendArmoredManMessage < gpGlobals->time)
		{
			if (pArmoredMan && !FBitSet(pArmoredMan->pev->flags, FL_FAKECLIENT))
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pArmoredMan->edict() );
					WRITE_BYTE(1);
					WRITE_STRING("jesus");
					WRITE_BYTE(0);
					WRITE_BYTE(160);
					WRITE_BYTE(255);
				MESSAGE_END();

				MESSAGE_BEGIN(MSG_ONE, gmsgBanner, NULL, pArmoredMan->edict());
					WRITE_STRING("You Are Jesus");
					WRITE_STRING("Convert every single Santa you find!");
					WRITE_BYTE(80);
				MESSAGE_END();
			}

			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && !plr->HasDisconnected && !FBitSet(plr->pev->flags, FL_FAKECLIENT) && !plr->IsSpectator() )
				{
					if (plr->pev->fuser4 > 0)
					{
						MESSAGE_BEGIN(MSG_ONE, gmsgBanner, NULL, plr->edict());
							WRITE_STRING("You Are On Team Santa");
							WRITE_STRING("Find Jesus and convert him into commercialism!");
							WRITE_BYTE(80);
						MESSAGE_END();
					}
				}
			}
			m_fSendArmoredManMessage = -1;
		}

		//santas all dead or armored man defeated.
		if ( clients_alive <= 1 || !pArmoredMan || !pArmoredMan->IsAlive() || pArmoredMan->HasDisconnected )
		{
			//stop timer / end game.
			m_flRoundTimeLimit = 0;
			g_GameInProgress = FALSE;
			PauseMutators();
			MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
				WRITE_BYTE(0);
			MESSAGE_END();

			//hack to allow for logical code below.
			if ( pArmoredMan && pArmoredMan->HasDisconnected )
				pArmoredMan->pev->health = 0;

			if (pArmoredMan && !FBitSet(pArmoredMan->pev->flags, FL_FAKECLIENT))
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pArmoredMan->edict() );
					WRITE_BYTE(0);
					WRITE_STRING("jesus");
				MESSAGE_END();
			}

			//armored man is alive.
			if ( pArmoredMan && pArmoredMan->IsAlive() && clients_alive == 1 )
			{
				DetermineWinner();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_KILLINGMACHINE);
				MESSAGE_END();
			}
			//the man has been killed.
			else if ( pArmoredMan && !pArmoredMan->IsAlive() )
			{
				DetermineWinner();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
				MESSAGE_END();
			}
			//everyone died.
			else
			{
				DetermineWinner();
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
				UTIL_VarArgs("Prepare for Jesus vs Santa\n\n%i...\n", m_iCountDown));
			m_iCountDown--;
			m_iFirstBloodDecided = FALSE;
			flUpdateTime = gpGlobals->time + 1.0;
			return;
		}

		ALERT(at_console, "Players in arena:\n");

		//frags + time.
		SetRoundLimits();

		int armoredman = m_iPlayersInArena[RANDOM_LONG(0, clients-1)];
		ALERT(at_console, "clients set to %d, armor man set to index=%d\n", clients, armoredman);
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
				plr->pev->fuser4 = 1;
		}
		pArmoredMan = (CBasePlayer *)UTIL_PlayerByIndex( armoredman );
		if (pArmoredMan)
		{
			pArmoredMan->IsArmoredMan = TRUE;
			pArmoredMan->pev->fuser4 = 0;
		}

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
			WRITE_STRING( "jesus" );
			WRITE_STRING( "santa" );
		MESSAGE_END();

		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %d players have entered the arena!\n", clients));
	}
	else
	{
		SuckAllToSpectator();
		m_flRoundTimeLimit = 0;
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("Jesus vs Santa");
			WRITE_STRING("Waiting for other players");
			WRITE_BYTE(0);
			if (roundlimit.value > 0)
				WRITE_STRING(UTIL_VarArgs("%d Rounds", (int)roundlimit.value));
		MESSAGE_END();
		m_fWaitForPlayersTime = gpGlobals->time + roundwaittime.value;
	}

	flUpdateTime = gpGlobals->time + 1.0;
}

int CHalfLifeJesusVsSanta::IPointsForKill( CBasePlayer* pAttacker, CBasePlayer* pKilled )
{
	// No credit for team kills
	if ( !pAttacker->IsArmoredMan && !pKilled->IsArmoredMan )
		return 0;

	return CHalfLifeMultiplay::IPointsForKill( pAttacker, pKilled );
}

void CHalfLifeJesusVsSanta::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Jesus vs Santa");
			if (roundlimit.value > 0)
				WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
			else
				WRITE_STRING("");
			WRITE_BYTE(0);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
			WRITE_BYTE( 2 );
			WRITE_STRING( "jesus" );
			WRITE_STRING( "santa" );
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

BOOL CHalfLifeJesusVsSanta::HasGameTimerExpired( void )
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

BOOL CHalfLifeJesusVsSanta::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
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

void CHalfLifeJesusVsSanta::FPlayerTookDamage( float flDamage, CBasePlayer *pVictim, CBaseEntity *pKiller)
{
	CBasePlayer *pPlayerAttacker = NULL;

	if (pKiller && pKiller->IsPlayer())
	{
		pPlayerAttacker = (CBasePlayer *)pKiller;
		if ( pPlayerAttacker != pVictim && !pPlayerAttacker->IsArmoredMan && !pVictim->IsArmoredMan )
		{
			ClientPrint(pPlayerAttacker->pev, HUD_PRINTCENTER, "Destroy Jesus!\nNot your teammate!");
		}
	}
}

void CHalfLifeJesusVsSanta::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);

	CHalfLifeMultiplay::SavePlayerModel(pPlayer);

	// Place player in spectator mode if joining during a game
	// Or if the game begins that requires spectators
	if ((g_GameInProgress && !pPlayer->IsInArena) || (!g_GameInProgress && IsRoundBased()))
	{
		return;
	}

	if ( pPlayer->IsArmoredMan )
	{
		pPlayer->GiveMelees();
		pPlayer->GiveExplosives();
		pPlayer->pev->max_health = pPlayer->pev->health = pPlayer->pev->armorvalue = 750;
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "1");
		pPlayer->GiveNamedItem("rune_cloak");
		strncpy( pPlayer->m_szTeamName, "jesus", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()),
			g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "jesus");
	}
	else
	{
		pPlayer->GiveRandomWeapon("weapon_nuke");
		strncpy( pPlayer->m_szTeamName, "santa", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()),
			g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "santa");
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

BOOL CHalfLifeJesusVsSanta::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	if ( !pPlayer->m_flForceToObserverTime )
		pPlayer->m_flForceToObserverTime = gpGlobals->time + 3.0;

	return FALSE;
}

void CHalfLifeJesusVsSanta::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	pVictim->pev->frags = 0; // clear immediately for winner determination

	CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);

	if ( !pVictim->IsArmoredMan )
	{
		int clientsLeft = 0;
		for (int i = 1; i <= gpGlobals->maxClients; i++) {
			if (m_iPlayersInArena[i-1] > 0)
			{
				CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(m_iPlayersInArena[i-1]);
				if (pPlayer && pPlayer->IsAlive() && !pPlayer->HasDisconnected)
					clientsLeft++;
			}
		}
		UTIL_ClientPrintAll(HUD_PRINTTALK,
			UTIL_VarArgs("* %s has been eliminated! %d Santas remain!\n",
			STRING(pVictim->pev->netname), clientsLeft > 0 ? clientsLeft - 1 : 0));
		if (clientsLeft > 1)
		{
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_MASSACRE);
			MESSAGE_END();
		}
	}
}

BOOL CHalfLifeJesusVsSanta::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if (pPlayer->IsArmoredMan)
	{
		if (!strcmp(STRING(pItem->pev->classname), "weapon_vest"))
			return FALSE;
	}

	if (!strcmp(STRING(pItem->pev->classname), "weapon_nuke"))
		return FALSE;

	return CHalfLifeMultiplay::CanHavePlayerItem( pPlayer, pItem );
}

int CHalfLifeJesusVsSanta::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		if (!strcmp(pTeamName, "jesus"))
			return 0;
		else
			return 1; // santa
	}
	
	return -1;	// No match
}

BOOL CHalfLifeJesusVsSanta::CanRandomizeWeapon( const char *name )
{
	if (strcmp(name, "weapon_nuke") == 0)
		return FALSE;

	return TRUE;
}

void CHalfLifeJesusVsSanta::PlayerThink( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerThink(pPlayer);

	// Jesus walks on water (kind of)
	if (pPlayer->IsArmoredMan)
	{
		if (pPlayer->m_fHallelujahTime < gpGlobals->time)
		{
			if (pPlayer->pev->waterlevel > 0)
			{
				pPlayer->pev->velocity.z += 45;
				UTIL_MakeVectors(pPlayer->pev->v_angle);

				if ( pPlayer->m_afButtonPressed & IN_FORWARD )
					pPlayer->pev->velocity = pPlayer->pev->velocity + gpGlobals->v_forward * 300;
				else if ( pPlayer->m_afButtonPressed & IN_BACK )
					pPlayer->pev->velocity = pPlayer->pev->velocity + gpGlobals->v_forward * -300;
				else if ( pPlayer->m_afButtonPressed & IN_MOVERIGHT )
					pPlayer->pev->velocity = pPlayer->pev->velocity + gpGlobals->v_right * 300;
				else if ( pPlayer->m_afButtonPressed & IN_MOVELEFT )
					pPlayer->pev->velocity = pPlayer->pev->velocity + gpGlobals->v_right * -300;
				else if ( pPlayer->m_afButtonPressed & IN_JUMP)
					pPlayer->pev->velocity = pPlayer->pev->velocity + gpGlobals->v_up * 300;

				// Allow Jesus to check on the fish
				if ( pPlayer->m_afButtonPressed & IN_DUCK)
					pPlayer->m_fHallelujahTime = gpGlobals->time + 15.0;
				else
					pPlayer->m_fHallelujahTime = gpGlobals->time + 0.01;
			}
		}
	}
}

int CHalfLifeJesusVsSanta::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() )
		return GR_NOTTEAMMATE;

	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

const char *CHalfLifeJesusVsSanta::GetTeamID( CBaseEntity *pEntity )
{
	if ( pEntity == NULL || pEntity->pev == NULL )
		return "";

	// return their team name
	return pEntity->TeamID();
}

BOOL CHalfLifeJesusVsSanta::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
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

BOOL CHalfLifeJesusVsSanta::IsRoundBased( void )
{
	return TRUE;
}

BOOL CHalfLifeJesusVsSanta::IsTeamplay( void )
{
	return TRUE;
}
