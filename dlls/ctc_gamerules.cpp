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
extern int gmsgBanner;

#define SPAWN_TIME 30.0
#define Toad_STAGGER_TIME (SPAWN_TIME * 0.5)

// Throttle capture/drop notification sounds so rapid multi-toad pickup/drop
// churn (especially with mp_ctctoadcount > 1) doesn't spam the client sound
// channel. Print messages still fire every time; only the sound is gated.
#define CtC_PICKDROP_SOUND_COOLDOWN 3.0f

static int CtCConfiguredToadCount()
{
	int configured = (int)ctctoadcount.value;
	if (configured < 1)
		configured = 1;
	else if (configured > 5)
		configured = 5;

	return configured;
}

static int CtCTargetToadCount(int playerCount)
{
	if (playerCount < 2)
		return 0;

	int maxByPlayers = playerCount - 1;
	int target = CtCConfiguredToadCount();
	if (target > maxByPlayers)
		target = maxByPlayers;

	return target;
}

static int CtCCountHolders(CBasePlayer *holders[], int maxHolders, CBasePlayer **ppFirstHolder)
{
	int holderCount = 0;
	if (ppFirstHolder)
		*ppFirstHolder = NULL;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
		if (!plr || !plr->IsPlayer() || plr->HasDisconnected)
			continue;

		if (!plr->m_iHoldingChumtoad)
			continue;

		if (ppFirstHolder && *ppFirstHolder == NULL)
			*ppFirstHolder = plr;

		if (holders && holderCount < maxHolders)
			holders[holderCount] = plr;

		holderCount++;
	}

	return holderCount;
}

static int CtCCountLooseToads(void)
{
	int looseCount = 0;
	edict_t *pEdict = FIND_ENTITY_BY_CLASSNAME(NULL, "monster_ctctoad");
	while (!FNullEnt(pEdict))
	{
		if (pEdict->v.iuser1 == 1)
			looseCount++;
		pEdict = FIND_ENTITY_BY_CLASSNAME(pEdict, "monster_ctctoad");
	}

	return looseCount;
}

static void CtCRemoveAllLooseToads(void)
{
	edict_t *pEdict = FIND_ENTITY_BY_CLASSNAME(NULL, "monster_ctctoad");
	while (!FNullEnt(pEdict))
	{
		edict_t *pNext = FIND_ENTITY_BY_CLASSNAME(pEdict, "monster_ctctoad");
		UTIL_Remove(CBaseEntity::Instance(pEdict));
		pEdict = pNext;
	}
}

static BOOL CtCForceRemoveHolderToad(CHalfLifeCaptureTheChumtoad *pRules, CBasePlayer *pPlayer)
{
	if (!pRules || !pPlayer || !pPlayer->m_iHoldingChumtoad)
		return FALSE;

	pPlayer->m_iHoldingChumtoad = FALSE;
	UTIL_MakeVectors(pPlayer->pev->v_angle);
	CBaseEntity *pDropped = pRules->DropCharm(pPlayer, pPlayer->pev->origin + gpGlobals->v_forward * 64);
	if (pDropped)
		UTIL_Remove(pDropped);
	pPlayer->RemoveNamedItem("weapon_chumtoad");

	return TRUE;
}

static BOOL CtCRemoveOneLooseToad(void)
{
	edict_t *pEdict = FIND_ENTITY_BY_CLASSNAME(NULL, "monster_ctctoad");
	while (!FNullEnt(pEdict))
	{
		if (pEdict->v.iuser1 == 1)
		{
			UTIL_Remove(CBaseEntity::Instance(pEdict));
			return TRUE;
		}

		pEdict = FIND_ENTITY_BY_CLASSNAME(pEdict, "monster_ctctoad");
	}

	return FALSE;
}

static BOOL CtCTeleportLooseToads(void)
{
	BOOL moved = FALSE;
	edict_t *pEdict = FIND_ENTITY_BY_CLASSNAME(NULL, "monster_ctctoad");
	while (!FNullEnt(pEdict))
	{
		edict_t *pNext = FIND_ENTITY_BY_CLASSNAME(pEdict, "monster_ctctoad");
		if (pEdict->v.iuser1 == 1)
		{
			CBaseEntity *pSpot = NULL;
			for (int i = RANDOM_LONG(1, 8); i > 0; i--)
				pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch");

			if (pSpot)
			{
				UTIL_SetOrigin(VARS(pEdict), pSpot->pev->origin);
				moved = TRUE;
			}
		}

		pEdict = pNext;
	}

	return moved;
}

CHalfLifeCaptureTheChumtoad::CHalfLifeCaptureTheChumtoad()
{
	m_pHolder = NULL; // must initialize, otherwise, GET() crash.
	m_fChumtoadInPlay = FALSE;
	m_fCreateChumtoadTimer = 0;
	m_fRemoveChumtoadTimer = 0;
	m_fMoveChumtoadTimer = 0;
	m_flCtCNextPickDropSoundTime = 0.0f;
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
		int playerCount = UTIL_GetPlayerCount();
		int targetToadCount = CtCTargetToadCount(playerCount);

		if (targetToadCount <= 0)
		{
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
				if (plr && plr->IsPlayer() && !plr->HasDisconnected && plr->m_iHoldingChumtoad)
					CtCForceRemoveHolderToad(this, plr);
			}

			CtCRemoveAllLooseToads();
			m_fCreateChumtoadTimer = 0;
			m_fRemoveChumtoadTimer = 0;
			m_fMoveChumtoadTimer = 0;
			m_fChumtoadInPlay = FALSE;
			m_pHolder = NULL;
		}
		else
		{
			CBasePlayer *firstHolder = NULL;
			int holderCount = CtCCountHolders(NULL, 0, &firstHolder);
			int looseCount = CtCCountLooseToads();
			int totalToads = holderCount + looseCount;

			m_fChumtoadInPlay = holderCount > 0;
			m_pHolder = firstHolder;

			int hardMaxToads = playerCount - 1;
			while (totalToads > hardMaxToads)
			{
				if (CtCRemoveOneLooseToad())
				{
					totalToads--;
					continue;
				}

				CBasePlayer *holders[32];
				int holderCandidates = CtCCountHolders(holders, 32, NULL);
				if (holderCandidates < 1)
					break;

				CBasePlayer *removePlayer = holders[RANDOM_LONG(0, holderCandidates - 1)];
				if (!CtCForceRemoveHolderToad(this, removePlayer))
					break;

				totalToads--;
			}

			if (totalToads > targetToadCount)
			{
				if (m_fRemoveChumtoadTimer <= 0)
					m_fRemoveChumtoadTimer = gpGlobals->time;

				if (m_fRemoveChumtoadTimer <= gpGlobals->time)
				{
					BOOL removed = CtCRemoveOneLooseToad();
					if (!removed)
					{
						CBasePlayer *holders[32];
						int holderCandidates = CtCCountHolders(holders, 32, NULL);
						if (holderCandidates > 0)
						{
							CBasePlayer *removePlayer = holders[RANDOM_LONG(0, holderCandidates - 1)];
							removed = CtCForceRemoveHolderToad(this, removePlayer);
						}
					}

					if (removed)
						totalToads--;

					if (totalToads > targetToadCount)
						m_fRemoveChumtoadTimer = gpGlobals->time + Toad_STAGGER_TIME;
					else
						m_fRemoveChumtoadTimer = 0;
				}
			}
			else
			{
				m_fRemoveChumtoadTimer = 0;
			}

			if (totalToads < targetToadCount)
			{
				if (m_fCreateChumtoadTimer <= 0)
					m_fCreateChumtoadTimer = gpGlobals->time;

				if (m_fCreateChumtoadTimer <= gpGlobals->time)
				{
					if (CreateChumtoad())
					{
						totalToads++;
						UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC] A chumtoad has spawned!\n");
						MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
							WRITE_BYTE(CLIENT_SOUND_CTF_CAPTURE);
						MESSAGE_END();

						if (m_fMoveChumtoadTimer <= 0)
							m_fMoveChumtoadTimer = gpGlobals->time + SPAWN_TIME;

						if (totalToads < targetToadCount)
							m_fCreateChumtoadTimer = gpGlobals->time + Toad_STAGGER_TIME;
						else
							m_fCreateChumtoadTimer = 0;
					}
					else
					{
						m_fCreateChumtoadTimer = gpGlobals->time + 1.0;
					}
				}
			}
			else
			{
				m_fCreateChumtoadTimer = 0;
			}

			holderCount = CtCCountHolders(NULL, 0, &firstHolder);
			looseCount = CtCCountLooseToads();
			m_fChumtoadInPlay = holderCount > 0;
			m_pHolder = firstHolder;

			if (looseCount > 0)
			{
				if (m_fMoveChumtoadTimer <= 0)
					m_fMoveChumtoadTimer = gpGlobals->time + SPAWN_TIME;

				if (m_fMoveChumtoadTimer <= gpGlobals->time)
				{
					if (CtCTeleportLooseToads())
					{
						m_fMoveChumtoadTimer = gpGlobals->time + SPAWN_TIME;
						MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
							WRITE_BYTE(CLIENT_SOUND_EBELL);
						MESSAGE_END();
						UTIL_ClientPrintAll(HUD_PRINTTALK, looseCount > 1 ? "[CtC] The chumtoads have teleported!\n" : "[CtC] The chumtoad has teleported!\n");
					}
					else
					{
						m_fMoveChumtoadTimer = gpGlobals->time + 1.0;
					}
				}
			}
			else
			{
				m_fMoveChumtoadTimer = 0;
			}
		}

		CBasePlayer *firstHolder = NULL;
		int holderCount = CtCCountHolders(NULL, 0, &firstHolder);
		int looseCount = CtCCountLooseToads();
		m_fChumtoadInPlay = holderCount > 0;
		m_pHolder = firstHolder;

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
			if (plr && plr->IsPlayer() && !plr->HasDisconnected && !(plr->pev->flags & FL_FAKECLIENT) && !plr->m_iHoldingChumtoad)
			{
				const char *objectiveText = "";
				if (targetToadCount <= 1)
				{
					if (m_pHolder)
						objectiveText = m_fChumtoadInPlay ? UTIL_VarArgs("%s has it!", STRING(m_pHolder->pev->netname)) : "The chumtoad is free";
					else if (playerCount > 1)
						objectiveText = m_fChumtoadInPlay ? "The chumtoad is held" : "The chumtoad is free";
					else
						objectiveText = "Chumtoad waiting for players";
				}
				else
				{
					if (holderCount > 0)
						objectiveText = UTIL_VarArgs("%d of %d toad holders.", holderCount, targetToadCount);
					else if (looseCount > 0)
						objectiveText = UTIL_VarArgs("%d of %d chumtoads are free.", looseCount, targetToadCount);
					else if (playerCount > 1)
						objectiveText = "Spawning chumtoads...";
					else
						objectiveText = "Chumtoad waiting for players";
				}

				MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
					if (plr->IsSpectator())
						WRITE_STRING("Watching CtC");
					else
						WRITE_STRING("Get the chumtoad");
					WRITE_STRING(objectiveText);
					WRITE_BYTE(0);
					WRITE_STRING(scorelimit.value > 0 ? UTIL_VarArgs("Scorelimit is %d", (int)scorelimit.value) : "No score limit");
				MESSAGE_END();
			}
		}

		m_fChumtoadPlayTimer = gpGlobals->time + 1.0;
	}
}

void CHalfLifeCaptureTheChumtoad::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Capture the chumtoad");
			WRITE_STRING("");
			WRITE_BYTE(0);
			WRITE_STRING(scorelimit.value > 0 ? UTIL_VarArgs("First to %d wins", (int)scorelimit.value) : "No score limit");
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
			WRITE_BYTE( 2 );
			WRITE_STRING( "chaser" );
			WRITE_STRING( "holder" );
		MESSAGE_END();

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *plr = UTIL_PlayerByIndex( i );
			if ( plr )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgTeamInfo, NULL, pPlayer->edict() );
					WRITE_BYTE( plr->entindex() );
					WRITE_STRING( plr->TeamID() );
				MESSAGE_END();
			}
		}
	}

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iRoundWins );
		WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
	MESSAGE_END();
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

	return pToad ? TRUE : FALSE;
}

void CHalfLifeCaptureTheChumtoad::CaptureCharm( CBasePlayer *pPlayer )
{
	if (UTIL_GetPlayerCount() < 2)
		return;

	pPlayer->m_iHoldingChumtoad = TRUE;
	m_fChumtoadInPlay = TRUE;

	if (!pPlayer->m_fHasRune)
	{
		pPlayer->pev->renderfx = kRenderFxGlowShell;
		pPlayer->pev->renderamt = 10;
		pPlayer->pev->rendercolor = Vector(0, 200, 0);
	}

	pPlayer->pev->fuser4 = RADAR_CHUMTOAD;
	pPlayer->m_fCameraDelay = 0;
	m_pHolder = (CBaseEntity *)pPlayer;

	int m_iTrail = g_sModelIndexSmoke2;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( pPlayer->entindex() );	// entity
		WRITE_SHORT( m_iTrail );	// model
		WRITE_BYTE( 50 ); // life
		WRITE_BYTE( 3 );  // width
		WRITE_BYTE( 0 );   // r, g, b
		WRITE_BYTE( 200 );   // r, g, b
		WRITE_BYTE( 0 );   // r, g, b
		WRITE_BYTE( 200 );	// brightness
	MESSAGE_END();

	UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC] %s has captured the chumtoad!\n",
	STRING(pPlayer->pev->netname));

	if (gpGlobals->time >= m_flCtCNextPickDropSoundTime)
	{
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
			WRITE_BYTE(CLIENT_SOUND_BULLSEYE);
		MESSAGE_END();
		m_flCtCNextPickDropSoundTime = gpGlobals->time + CtC_PICKDROP_SOUND_COOLDOWN;
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pPlayer->edict() );
		WRITE_BYTE(1);
		WRITE_STRING("cam_chumtoad");
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
	m_fCreateChumtoadTimer = 0;
	if (m_fMoveChumtoadTimer <= 0)
		m_fMoveChumtoadTimer = gpGlobals->time + SPAWN_TIME;
}

CBaseEntity *CHalfLifeCaptureTheChumtoad::DropCharm( CBasePlayer *pPlayer, Vector origin )
{
	pPlayer->m_iHoldingChumtoad = FALSE;

	pPlayer->pev->rendermode = kRenderNormal;
	pPlayer->pev->renderfx = kRenderFxNone;
	pPlayer->pev->renderamt = 0;

	pPlayer->pev->fuser4 = 0;
	pPlayer->m_fCameraDelay = gpGlobals->time + 4.0;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_KILLBEAM );
		WRITE_SHORT( pPlayer->entindex() );	// entity
	MESSAGE_END();

	UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC] %s has dropped the chumtoad!\n",
		STRING(pPlayer->pev->netname));

	if (gpGlobals->time >= m_flCtCNextPickDropSoundTime)
	{
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
			WRITE_BYTE(CLIENT_SOUND_MANIAC);
		MESSAGE_END();
		m_flCtCNextPickDropSoundTime = gpGlobals->time + CtC_PICKDROP_SOUND_COOLDOWN;
	}

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

	m_fCreateChumtoadTimer = 0;
	m_fMoveChumtoadTimer = gpGlobals->time + SPAWN_TIME;
	CBaseEntity *pChumtoad = CBaseEntity::Create("monster_ctctoad", origin, pPlayer->pev->v_angle, NULL);
	if (pChumtoad)
		pChumtoad->pev->iuser1 = 1;
	else
		m_fCreateChumtoadTimer = gpGlobals->time + 1.0; // in case we could not create a chumtoad

	CBasePlayer *firstHolder = NULL;
	int holderCount = CtCCountHolders(NULL, 0, &firstHolder);
	m_fChumtoadInPlay = holderCount > 0;
	m_pHolder = firstHolder;

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
		// End session if hit score limit
		if ( scorelimit.value > 0 && pPlayer->m_iRoundWins >= scorelimit.value )
		{
			GoToIntermission();
			return;
		}
	}

	if (pPlayer->m_fCameraDelay && pPlayer->m_fCameraDelay < gpGlobals->time)
	{
		// Only received if the player is alive.
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pPlayer->edict() );
			WRITE_BYTE(0);
			WRITE_STRING("cam_chumtoad");
		MESSAGE_END();
		pPlayer->m_fCameraDelay = 0;
	}

	if (pPlayer->m_iShowGameModeMessage > -1 && 
		pPlayer->m_iShowGameModeMessage < gpGlobals->time && 
		!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgBanner, NULL, pPlayer->edict());
			WRITE_STRING("Capture The Chumtoad");
			WRITE_STRING("Kind of like Quake's capture the chicken, but with a chumtoad.");
			WRITE_BYTE(80);
		MESSAGE_END();
		pPlayer->m_iShowGameModeMessage = -1;
	}

	// Updates once per second
	int time_remaining = (int)gpGlobals->time;
	BOOL scoringPoints = FALSE;
	char *message;

	if (pPlayer->m_iCaptureTime != time_remaining)
	{
		if (pPlayer->IsAlive() && pPlayer->m_iHoldingChumtoad)
		{
			// Keep running to score points
			if (pPlayer->pev->velocity.Length() > 50)
			{
				pPlayer->m_iChumtoadCounter++;
				message = UTIL_VarArgs("Points: %d | Timer: %d", 
						(int)pPlayer->m_iRoundWins, pPlayer->m_iChumtoadCounter);
				scoringPoints = TRUE;
				pPlayer->m_iChumtoadDropCounter = 10;
			}
			else
			{
				message = UTIL_VarArgs("Keep running to score points\n");

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

			int secondsUntilPoint = ctcsecondsforpoint.value > 0 ? ctcsecondsforpoint.value : 10;
			if (scoringPoints)
			{
				if (pPlayer->m_iChumtoadCounter % secondsUntilPoint == 0)
				{
					MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
						WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
						WRITE_SHORT( pPlayer->pev->frags );
						WRITE_SHORT( pPlayer->m_iDeaths );
						WRITE_SHORT( ++pPlayer->m_iRoundWins );
						WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
					MESSAGE_END();

					UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC] %s has scored a point!\n", 
						STRING(pPlayer->pev->netname));
					
					ClientPrint(pPlayer->pev, HUD_PRINTCENTER, UTIL_VarArgs("You Have Scored a Point!\n"));
					if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
					{
						MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, pPlayer->edict() );
							WRITE_BYTE(CLIENT_SOUND_LEVEL_UP);
						MESSAGE_END();
					}

					pPlayer->m_iChumtoadCounter = 0;
				}

				TraceResult tr;
				Vector vecSpot = pPlayer->pev->origin + Vector(0 , 0, 8); //move up a bit, and trace down.
				UTIL_TraceLine(vecSpot, vecSpot + Vector(0, 0, -64), ignore_monsters, ENT(pPlayer->pev), &tr);
				UTIL_BloodDecalTrace(&tr, BLOOD_COLOR_YELLOW);
			}

			int percent = (int)fmin(fmax(0.0f, ((float)pPlayer->m_iChumtoadCounter / secondsUntilPoint) * 100.0f), 100.0f);
			MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer->edict());
				WRITE_STRING("Carrying the chumtoad!");
				WRITE_STRING(message);
				WRITE_BYTE(percent);
				WRITE_STRING("");
			MESSAGE_END();
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

	// New player
	if (pPlayer->pev->iuser3 > 0)
	{
		// Already set to simple in client.cpp
		return;
	}
	else if (pPlayer->pev->iuser3 == 0) // Spectator now joining
	{
		strncpy( pPlayer->m_szTeamName, "chaser", TEAM_NAME_LENGTH );
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
			WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
			WRITE_STRING( pPlayer->m_szTeamName );
		MESSAGE_END();
		pPlayer->pev->iuser3 = -1;
		pPlayer->m_iObserverWeapon = 0; // Used as the menu option
		pPlayer->m_iShowGameModeMessage = gpGlobals->time + 0.5;
	}

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
	if (!pPlayer)
		return GR_PLR_DROP_GUN_NO;

	// PackDeadPlayerItems runs after CtC drop logic clears m_iHoldingChumtoad.
	// Block gunbox drops if player still has/holds chumtoad in inventory.
	if (pPlayer->m_iHoldingChumtoad ||
		pPlayer->HasNamedPlayerItem("weapon_chumtoad") ||
		(pPlayer->m_pActiveItem && FStrEq("weapon_chumtoad", STRING(pPlayer->m_pActiveItem->pev->classname))))
	{
		return GR_PLR_DROP_GUN_NO;
	}

	return CHalfLifeMultiplay::DeadPlayerWeapons(pPlayer);
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
	if (!strcmp("rune_frag", szRune))
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

	if ( strstr(mutator, g_szMutators[MUTATOR_THIRDPERSON - 1]) || atoi(mutator) == MUTATOR_THIRDPERSON )
		return FALSE;

	return CHalfLifeMultiplay::MutatorAllowed(mutator);
}

int CHalfLifeCaptureTheChumtoad::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		if (!strcmp(pTeamName, "holder"))
			return 3; // green, not red.
		else
			return 0;
	}
	
	return -1;	// No match
}

BOOL CHalfLifeCaptureTheChumtoad::IsTeamplay( void )
{
	return TRUE;
}

BOOL CHalfLifeCaptureTheChumtoad::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if (pPlayer->pev->fuser4 == RADAR_CHUMTOAD &&
		strcmp(STRING(pItem->pev->classname), "weapon_chumtoad"))
		return FALSE;

	return CHalfLifeMultiplay::CanHavePlayerItem(pPlayer, pItem);
}

BOOL CHalfLifeCaptureTheChumtoad::CanHaveNamedItem( CBasePlayer *pPlayer, const char *pszItemName )
{
	if (pPlayer->pev->fuser4 == 0)
	{
		if (strcmp(pszItemName, "weapon_chumtoad") == 0) {
			return FALSE;
		}
	}

	if (pPlayer->pev->fuser4 == RADAR_CHUMTOAD)
	{
		if (strcmp(pszItemName, "weapon_chumtoad") != 0) {
			return FALSE;
		}
	}

	return CHalfLifeMultiplay::CanHaveNamedItem( pPlayer, pszItemName );
}

BOOL CHalfLifeCaptureTheChumtoad::CanHavePlayerAmmo( CBasePlayer *pPlayer, CBasePlayerAmmo *pAmmo )
{
	if (pPlayer->pev->fuser4 == RADAR_CHUMTOAD)
		return FALSE;

	return CHalfLifeMultiplay::CanHavePlayerAmmo( pPlayer, pAmmo );
}

