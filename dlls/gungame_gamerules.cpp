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
#include "gungame_gamerules.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"

extern DLL_GLOBAL int g_GameMode;
extern DLL_GLOBAL BOOL g_fGameOver;
extern cvar_t timeleft;
extern CVoiceGameMgr g_VoiceGameMgr;
extern int gmsgGameMode;
extern int gmsgPlayClientSound;
extern int gmsgScoreInfo;
extern int gmsgObjective;

#define MAXLEVEL 44
int g_iFrags[MAXLEVEL+1] = { 1, 3, 5, 6, 9, 12, 15, 18, 21, 26, 29, 30, 31, 32, 33, 34, 35,
							36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
							52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63 };
const char *g_WeaponId[MAXLEVEL+1] = 
{
	// hand
	"weapon_9mmhandgun", //level 0
	"weapon_deagle",
	"weapon_dual_deagle",
	"weapon_python",
	"weapon_mag60",
	"weapon_dual_mag60",
	"weapon_smg",
	"weapon_dual_smg",
	"weapon_sawedoff",
	"weapon_dual_sawedoff",
	// long
	"weapon_9mmAR",
	"weapon_12gauge",
	"weapon_shotgun",
	"weapon_crossbow",
	"weapon_sniperrifle",
	"weapon_chaingun",
	"weapon_usas",
	"weapon_dual_usas",
	"weapon_freezegun",
	// heavy
	"weapon_rpg",
	"weapon_dual_rpg",
	"weapon_railgun",
	"weapon_dual_railgun",
	"weapon_cannon",
	"weapon_gauss",
	"weapon_egon",
	"weapon_hornetgun",
	"weapon_glauncher",
	"weapon_flamethrower",
	"weapon_dual_flamethrower",
	"weapon_gravitygun",
	// loose
	"weapon_snowball",
	"weapon_handgrenade",
	"weapon_satchel",
	"weapon_tripmine",
	"weapon_snark",
	"weapon_chumtoad",
	"weapon_vest",
	// hand
	"weapon_chainsaw",
	"weapon_dual_wrench",
	"weapon_wrench",
	"weapon_crowbar",
	"weapon_fists",
	"weapon_knife",
	"Winner"
};

CHalfLifeGunGame::CHalfLifeGunGame()
{
	m_fGoToIntermission = 0;
	m_iTopLevel = 0;
	m_fRefreshStats = 0;
	m_hLeader = NULL;
	m_hVoiceHandle = NULL;
	m_iSuccessfulRounds = 0;
}

void CHalfLifeGunGame::Think( void )
{
	static int last_time;
	int frags_remaining = 0;
	int time_remaining = 0;

	g_VoiceGameMgr.Update(gpGlobals->frametime);

	g_pGameRules->CheckMutators();
	g_pGameRules->CheckGameMode();

	if ( g_fGameOver )
	{
		CHalfLifeMultiplay::Think();
		return;
	}

	float flTimeLimit = CVAR_GET_FLOAT("mp_timelimit") * 60;
	
	time_remaining = (int)(flTimeLimit ? ( flTimeLimit - gpGlobals->time ) : 0);

	if ( flTimeLimit != 0 && gpGlobals->time >= flTimeLimit )
	{
		GoToIntermission();
		return;
	}

	if ( ggstartlevel.value < 0 || ggstartlevel.value > MAXLEVEL-1)
	{
		g_engfuncs.pfnCvar_DirectSet( &ggstartlevel, UTIL_VarArgs( "%i", 0 ) );
	}

	if ( m_fGoToIntermission != 0 && gpGlobals->time >= m_fGoToIntermission )
	{
		if (m_iSuccessfulRounds < roundlimit.value)
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
				if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
					plr->Spawn();
			}

			m_fRefreshStats = gpGlobals->time;
		}
		else
		{
			GoToIntermission();
		}

		m_fGoToIntermission = 0;
		return;
	}

	// Updates once per second
	if ( timeleft.value != last_time )
	{
		g_engfuncs.pfnCvar_DirectSet( &timeleft, UTIL_VarArgs( "%i", time_remaining ) );
	}

	last_time = time_remaining;

	if (m_fRefreshStats > 0 && m_fRefreshStats <= gpGlobals->time)
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
			{
				plr->DisplayHudMessage(UTIL_VarArgs("Round %d of %d\nYour Level: %d | Top Level: %d", 
					m_iSuccessfulRounds+1, (int)roundlimit.value, (int)plr->pev->fuser4, m_iTopLevel),
					TXT_CHANNEL_GAME_INFO, -1, 0.83, 255, 255, 255, 0, 0, 0, 60, 0);
				
				int result = ((plr->pev->fuser4 + 1) / MAXLEVEL) * 100;

				if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
				{
					MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
						WRITE_STRING("Get through your weapon list");
						WRITE_STRING(UTIL_VarArgs("Your progress: %d of %d", (int)plr->pev->fuser4 + 1, MAXLEVEL));
						WRITE_BYTE(result);
						//if (plr->m_iRoundWins > 0)
						//	WRITE_STRING(UTIL_VarArgs("Won %d rounds: %d to go", plr->m_iRoundWins, (int)roundlimit.value - m_iSuccessfulRounds));
						//else
						// WRITE_STRING(UTIL_VarArgs("Round %d: %d to go", m_iSuccessfulRounds + 1, (int)roundlimit.value - 1));
						// WRITE_STRING(UTIL_VarArgs("Next weapon: %s", g_WeaponId[(int)plr->pev->fuser4 + 1]));
					MESSAGE_END();
				}
			}
		}

		// Needed to delay voice over since deploy of weapon mutes client play sound
		if (m_hVoiceHandle != NULL && m_iVoiceId > 0)
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgPlayClientSound, NULL, m_hVoiceHandle->edict() );
				WRITE_BYTE(m_iVoiceId);
			MESSAGE_END();
			m_hVoiceHandle = NULL;
			m_iVoiceId = 0;
		}

		m_fRefreshStats = -1;
	}
}

void CHalfLifeGunGame::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Get through your weapon list");
			WRITE_STRING(UTIL_VarArgs("Your progress: %d of %d", (int)pPlayer->pev->fuser4 + 1, MAXLEVEL));
			WRITE_BYTE(0);
			WRITE_STRING(UTIL_VarArgs("Best of %d", (int)roundlimit.value));
		MESSAGE_END();
	}
}

void CHalfLifeGunGame::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );

	// Go back in levels when killed self or suicide
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );
	if (ktmp)
	{
		int vlevel = (int)pVictim->pev->fuser4;
		if (vlevel > 0 && (pVictim == ktmp || !ktmp->IsPlayer()))
		{
			if ((int)pVictim->pev->frags < g_iFrags[vlevel-1])
			{
				pVictim->pev->fuser4 -= 1;
				m_fRefreshStats = gpGlobals->time;
			}
		}
	}

	// Reconfirm top level
	m_iTopLevel = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
		{
			if (m_iTopLevel < (int)plr->pev->fuser4)
				m_iTopLevel = (int)plr->pev->fuser4;
		}
	}
}

int CHalfLifeGunGame::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	if (m_fGoToIntermission == 0 && pAttacker && pKilled)
	{
		// Attacker
		int currentLevel = (int)pAttacker->pev->fuser4;
		if (currentLevel < MAXLEVEL)
		{
			// Note, frags are increased after this method, so assume +1
			if ((int)pAttacker->pev->frags+1 >= g_iFrags[currentLevel])
			{
				pAttacker->pev->fuser4 += 1;
				int newLevel = (int)pAttacker->pev->fuser4;
				int voiceId = 0;

				ClientPrint(pAttacker->pev, HUD_PRINTTALK,
					UTIL_VarArgs("[GunGame]: You increased your level to %d (obtained %s)!\n",
					newLevel, g_WeaponId[newLevel]));

				// Set highest game level
				if (m_iTopLevel < newLevel) {
					m_iTopLevel = newLevel;

					// Lost the lead!
					if (m_iTopLevel != MAXLEVEL)
					{
						for ( int i = 1; i <= gpGlobals->maxClients; i++ )
						{
							CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
							if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
							{
								if (plr != pAttacker && (int)plr->pev->fuser4 == (m_iTopLevel-1)) {
									// Play immediately since GiveNamedItem is not used in this context
									MESSAGE_BEGIN( MSG_ONE, gmsgPlayClientSound, NULL, plr->edict() );
										WRITE_BYTE(CLIENT_SOUND_LOSTLEAD);
									MESSAGE_END();
									ALERT(at_aiconsole, "play CLIENT_SOUND_LOSTLEAD\n");
								}
							}
						}
					}

					if (m_iTopLevel == MAXLEVEL)
					{
						MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
							WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
						MESSAGE_END();

						m_hLeader = pAttacker;
						m_iSuccessfulRounds++;
						m_fGoToIntermission = gpGlobals->time + 8.0;
						m_iTopLevel = 0;

						for ( int i = 1; i <= gpGlobals->maxClients; i++ )
						{
							CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

							if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
							{
								plr->pev->fuser4 = ggstartlevel.value;
								MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
									WRITE_BYTE( ENTINDEX(plr->edict()) );
									WRITE_SHORT( plr->pev->frags = 0 );
									WRITE_SHORT( plr->m_iDeaths = 0 );
									WRITE_SHORT( 0 );
									WRITE_SHORT( 0 );
								MESSAGE_END();

								if (plr->IsAlive())
									plr->RemoveAllItems(FALSE);

								plr->DisplayHudMessage(UTIL_VarArgs("Winner of Round %d is %s!\n", 
									m_iSuccessfulRounds, STRING(pAttacker->pev->netname)),
									TXT_CHANNEL_GAME_INFO, -1, 0.83, 255, 255, 255, 0, 0, 0, 60, 0);
							}
						}

						m_fRefreshStats = gpGlobals->time + 4.0;

						// No frags awarded when round is complete
						return 0;
					}
					else
					{
						if (m_hLeader != pAttacker)
						{
							m_hLeader = pAttacker;
							m_iVoiceId = CLIENT_SOUND_TAKENLEAD;
						}
					}
				}
				else if (m_iTopLevel == newLevel)
				{
					m_iVoiceId = CLIENT_SOUND_TIEDLEAD;
				}

				if (pAttacker->IsAlive())
				{
					pAttacker->RemoveAllItems(FALSE);
					GiveMutators(pAttacker);
					pAttacker->GiveNamedItem(g_WeaponId[newLevel]);
				}
				m_hVoiceHandle = pAttacker;
				m_fRefreshStats = gpGlobals->time + 1.0;
			}
			else 
			{
				ClientPrint(pAttacker->pev, HUD_PRINTTALK,
					UTIL_VarArgs("[GunGame]: You need %d frags to reach level %s!\n",
					g_iFrags[currentLevel] - ((int)pAttacker->pev->frags+1), g_WeaponId[currentLevel+1]));
			}
		}
	}

	return 1;
}

void CHalfLifeGunGame::PlayerSpawn( CBasePlayer *pPlayer )
{
	pPlayer->pev->weapons |= (1<<WEAPON_SUIT);

	if (pPlayer)
	{
		// In full game, go deep with negative deaths but in short game, pin to lowest level
		if (ggstartlevel.value > 0 && pPlayer->pev->fuser4 < ggstartlevel.value) {
			pPlayer->pev->fuser4 = ggstartlevel.value;
			pPlayer->pev->frags = g_iFrags[(int)ggstartlevel.value - 1];
		}
		
		int currentLevel = (int)pPlayer->pev->fuser4;
		pPlayer->GiveNamedItem(g_WeaponId[currentLevel]);
		pPlayer->GiveAmmo(AMMO_GLOCKCLIP_GIVE * 4, "9mm", _9MM_MAX_CARRY);
		pPlayer->GiveAmmo(AMMO_357BOX_GIVE * 4, "357", _357_MAX_CARRY);
		pPlayer->GiveAmmo(AMMO_BUCKSHOTBOX_GIVE * 4, "buckshot", BUCKSHOT_MAX_CARRY);
		pPlayer->GiveAmmo(AMMO_CROSSBOWCLIP_GIVE * 4, "bolts", BOLT_MAX_CARRY);
		pPlayer->GiveAmmo(AMMO_M203BOX_GIVE * 2, "ARgrenades", M203_GRENADE_MAX_CARRY);
		pPlayer->GiveAmmo(AMMO_RPGCLIP_GIVE * 2, "rockets", ROCKET_MAX_CARRY);
		pPlayer->GiveAmmo(AMMO_URANIUMBOX_GIVE * 4, "uranium", URANIUM_MAX_CARRY);

		ClientPrint(pPlayer->pev, HUD_PRINTTALK, UTIL_VarArgs("[GunGame]: You need %d frags to reach level %s.\n",
			g_iFrags[currentLevel] - ((int)pPlayer->pev->frags), g_WeaponId[currentLevel+1]));
	}

	g_pGameRules->SpawnMutators(pPlayer);
}

BOOL CHalfLifeGunGame::IsAllowedToSpawn( CBaseEntity *pEntity )
{
	if (strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0)
		return FALSE;

	return TRUE;
}

int CHalfLifeGunGame::WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	return GR_WEAPON_RESPAWN_NO;
}

int CHalfLifeGunGame::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_GUN_NO;
}

int CHalfLifeGunGame::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_AMMO_NO;
}

BOOL CHalfLifeGunGame::IsAllowedSingleWeapon( CBaseEntity *pEntity )
{
	// Do not allow dual weapons to automatically dole single weapon variant
	return FALSE;
}

BOOL CHalfLifeGunGame::IsAllowedToDropWeapon( void )
{
	return FALSE;
}

BOOL CHalfLifeGunGame::IsAllowedToHolsterWeapon( void )
{
	return FALSE;
}
