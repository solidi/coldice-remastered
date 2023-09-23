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

#define SPAWN_TIME 30.0

CHalfLifeCaptureTheChumtoad::CHalfLifeCaptureTheChumtoad()
{
	m_fChumtoadInPlay = FALSE;
	m_fCreateChumtoadTimer = m_fMoveChumtoadTimer = 0;
	m_fChumtoadPlayTimer = gpGlobals->time;
}

void CHalfLifeCaptureTheChumtoad::Think( void )
{
	CHalfLifeMultiplay::Think();

	if (m_fChumtoadPlayTimer < gpGlobals->time)
	{
		edict_t *pEdict = FIND_ENTITY_BY_CLASSNAME(NULL, "monster_ctctoad");
		edict_t *pChumtoad = NULL;
		BOOL foundToad = FALSE;

		MESSAGE_BEGIN(MSG_ALL, gmsgObjective, NULL);
			WRITE_STRING("Capture and hold the chumtoad to score points");
			WRITE_STRING(m_fChumtoadInPlay ? "The chumtoad is being held" : "The chumtoad is loose");
			WRITE_BYTE(0);
		MESSAGE_END();

		// Find toad, remove extras
		while (!FNullEnt(pEdict))
		{
			if (!foundToad)
			{
				if (pEdict->v.iuser1 == 1)
				{
					pChumtoad = pEdict;
					foundToad = TRUE;
				}
			}
			else
			{
				// Uh?
				UTIL_Remove(CBaseEntity::Instance(pEdict));
			}

			pEdict = FIND_ENTITY_BY_CLASSNAME(pEdict, "monster_ctctoad");
		}

/*
		if (m_fChumtoadInPlay)
		{
			foundToad = FALSE;
			// Player check, only one person should have the toad at any one time
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
				if (plr && plr->IsPlayer())
				{
					if (plr->m_iHoldingChumtoad)
					{
						if (foundToad)
						{
							plr->m_iHoldingChumtoad = FALSE;
							if (plr->HasNamedPlayerItem("weapon_chumtoad"))
							{
								if (plr->m_pActiveItem)
									plr->RemovePlayerItem(plr->m_pActiveItem);
							}
						}
						else
							foundToad = TRUE;
					}
				}
			}
		}
*/
		// No chumtoad, create it
		if (FNullEnt(pChumtoad) && m_fChumtoadInPlay == FALSE)
		{
			if (m_fCreateChumtoadTimer == 0) 
				m_fCreateChumtoadTimer = gpGlobals->time + 3.0;
			
			if (m_fCreateChumtoadTimer > 0 && m_fCreateChumtoadTimer <= gpGlobals->time)
			{
				if (CreateChumtoad())
				{
					UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC]: The chumtoad has appeared!\n");
					m_fCreateChumtoadTimer = -1;
					m_fMoveChumtoadTimer = gpGlobals->time + SPAWN_TIME;
				}
				else
				{
					// Keep trying
					m_fCreateChumtoadTimer = gpGlobals->time + 1.0;
				}
			}
		}

		// Chumtoad hopping around, move it once and a while
		if (!FNullEnt(pChumtoad) && m_fChumtoadInPlay == FALSE && m_fMoveChumtoadTimer <= gpGlobals->time)
		{
			CBaseEntity *pSpot = NULL;
			for (int i = RANDOM_LONG(1,8); i > 0; i--)
				pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch");
			if (pSpot == NULL)
				m_fMoveChumtoadTimer = gpGlobals->time + 1.0;
			else
			{
				UTIL_SetOrigin(VARS(pChumtoad), pSpot->pev->origin);
				m_fMoveChumtoadTimer = gpGlobals->time + SPAWN_TIME;
				UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC]: The chumtoad has teleported!\n");
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
		MESSAGE_END();
	}
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

	return TRUE;
}

void CHalfLifeCaptureTheChumtoad::CaptureCharm( CBasePlayer *pPlayer )
{
	pPlayer->m_iHoldingChumtoad = TRUE;
	m_fChumtoadInPlay = TRUE;

	pPlayer->pev->renderfx = kRenderFxGlowShell;
	pPlayer->pev->renderamt = 10;
	pPlayer->pev->rendercolor = Vector(0, 200, 0);

	pPlayer->pev->fuser4 = 1;

	UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC]: %s has captured the chumtoad!\n",
	STRING(pPlayer->pev->netname));

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
		WRITE_BYTE(CLIENT_SOUND_BULLSEYE);
	MESSAGE_END();

	pPlayer->m_iChumtoadCounter = pPlayer->m_iCaptureTime = 0;
	m_fCreateChumtoadTimer = -1;
	m_fMoveChumtoadTimer = 0;
}

CBaseEntity *CHalfLifeCaptureTheChumtoad::DropCharm( CBasePlayer *pPlayer, Vector origin )
{
	m_fChumtoadInPlay = FALSE;

	pPlayer->pev->rendermode = kRenderNormal;
	pPlayer->pev->renderfx = kRenderFxNone;
	pPlayer->pev->renderamt = 0;

	pPlayer->pev->fuser4 = 0;

	UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC]: %s has dropped the chumtoad!\n",
		STRING(pPlayer->pev->netname));

	MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
		WRITE_BYTE(CLIENT_SOUND_MANIAC);
	MESSAGE_END();

	m_fCreateChumtoadTimer = -1;
	m_fMoveChumtoadTimer = gpGlobals->time + SPAWN_TIME;
	CBaseEntity *pChumtoad = CBaseEntity::Create("monster_ctctoad", origin, pPlayer->pev->v_angle, pPlayer->edict());
	if (pChumtoad)
		pChumtoad->pev->iuser1 = 1;
	else
		m_fCreateChumtoadTimer = gpGlobals->time + 1.0; // in case we could not create a chumtoad

	return pChumtoad;
}

void CHalfLifeCaptureTheChumtoad::PlayerThink( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerThink(pPlayer);

	// Updates once per second
	int time_remaining = (int)gpGlobals->time;
	BOOL foundChumtoad = FALSE, scoringPoints = FALSE;
	char *message;

	if (pPlayer->m_iCaptureTime != time_remaining)
	{
		if (pPlayer->IsAlive() && pPlayer->m_iHoldingChumtoad)
		{
			// Keep running to score points
			if (pPlayer->pev->velocity.Length() > 50)
			{
				pPlayer->m_iChumtoadCounter++;
				message = UTIL_VarArgs("Running with the Chumtoad!\nPoints: %d | Timer: %d", 
						(int)pPlayer->pev->frags, pPlayer->m_iChumtoadCounter);
				scoringPoints = TRUE;
			}
			else
			{
				message = UTIL_VarArgs("Keep running to score points\nor the chumtoad will slip away!");

				// 15% chance on every score point, drop chumtoad!
				if (RANDOM_LONG(0,7) == 0 && pPlayer->m_pActiveItem)
				{
					ClientPrint(pPlayer->pev, HUD_PRINTCENTER, "The chumtoad slipped away!\n");
					CBasePlayerWeapon *weapon = (CBasePlayerWeapon *)pPlayer->m_pActiveItem;
					weapon->SendWeaponAnim( 5 );
					weapon->PrimaryAttack();
				}
			}

			if (scoringPoints)
			{
				int secondsUntilPoint = ctcsecondsforpoint.value > 0 ? ctcsecondsforpoint.value : 10;
				if (pPlayer->m_iChumtoadCounter % secondsUntilPoint == 0)
				{
					pPlayer->pev->frags++;
					MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
						WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
						WRITE_SHORT( pPlayer->pev->frags );
						WRITE_SHORT( pPlayer->m_iDeaths );
						WRITE_SHORT( 0 );
						WRITE_SHORT( 0 );
					MESSAGE_END();

					UTIL_ClientPrintAll(HUD_PRINTTALK, "[CtC]: %s has scored a point!\n", 
						STRING(pPlayer->pev->netname));

					MESSAGE_BEGIN( MSG_ONE, gmsgPlayClientSound, NULL, pPlayer->edict() );
					switch (RANDOM_LONG(1,6))
					{
						case 1: WRITE_BYTE(CLIENT_SOUND_WHICKEDSICK); break;
						case 2: WRITE_BYTE(CLIENT_SOUND_EXCELLENT); break;
						case 3: WRITE_BYTE(CLIENT_SOUND_IMPRESSIVE); break;
						case 4: WRITE_BYTE(CLIENT_SOUND_UNSTOPPABLE); break;
						default: WRITE_BYTE(0); break;
					}
					MESSAGE_END();

					pPlayer->m_iChumtoadCounter = 0;
				}

				TraceResult tr;
				Vector vecSpot = pPlayer->pev->origin + Vector(0 , 0, 8); //move up a bit, and trace down.
				UTIL_TraceLine(vecSpot, vecSpot + Vector(0, 0, -64), ignore_monsters, ENT(pPlayer->pev), &tr);
				UTIL_BloodDecalTrace(&tr, BLOOD_COLOR_YELLOW);
			}

			pPlayer->DisplayHudMessage(message,
			TXT_CHANNEL_GAME_INFO, -1, 0.83, 255, 255, 255, 0, 0.25, 0.25, 2, 0);
		}
	}

	pPlayer->m_iCaptureTime = time_remaining;
}

int CHalfLifeCaptureTheChumtoad::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return 0;
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
	return pPlayer->m_iHoldingChumtoad ? GR_PLR_DROP_GUN_NO : CHalfLifeMultiplay::DeadPlayerWeapons(pPlayer);
}

BOOL CHalfLifeCaptureTheChumtoad::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
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
