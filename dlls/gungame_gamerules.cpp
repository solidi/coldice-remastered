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
extern int gmsgRoundTime;
extern int gmsgShowTimer;

#define MAXLEVEL 47
int g_iFrags[MAXLEVEL+1] = { 1, 3, 5, 6, 9, 12, 15, 18, 21, 26, 29, 30, 31, 32, 33, 34, 35,
							36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
							52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65 };
const char *g_WeaponId[MAXLEVEL+1] = 
{
	// hand
	"9mmhandgun", //level 0
	"dual_glock",
	"deagle",
	"dual_deagle",
	"python",
	"mag60",
	"dual_mag60",
	"smg",
	"dual_smg",
	"sawedoff",
	"dual_sawedoff",
	// long
	"9mmAR",
	"12gauge",
	"shotgun",
	"crossbow",
	"sniperrifle",
	"chaingun",
	"dual_chaingun",
	"usas",
	"dual_usas",
	"freezegun",
	// heavy
	"rpg",
	"dual_rpg",
	"railgun",
	"dual_railgun",
	"cannon",
	"gauss",
	"egon",
	"hornetgun",
	"dual_hornetgun",
	"glauncher",
	"flamethrower",
	"dual_flamethrower",
	"gravitygun",
	// loose
	"snowball",
	"handgrenade",
	"satchel",
	"tripmine",
	"snark",
	"chumtoad",
	"vest",
	// hand
	"chainsaw",
	"dual_wrench",
	"wrench",
	"crowbar",
	"fists",
	"knife",
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

	m_iFirstBloodDecided = TRUE; // no first blood award
}

void CHalfLifeGunGame::Think( void )
{
	static int last_time;
	int frags_remaining = 0;
	int time_remaining = 0;

	g_VoiceGameMgr.Update(gpGlobals->frametime);

	// No checks during intermission
	if ( !m_flIntermissionEndTime )
	{
		g_pGameRules->MutatorsThink();
		g_pGameRules->CheckGameMode();
	}

	if ( g_fGameOver )
	{
		CHalfLifeMultiplay::Think();
		return;
	}

	float flTimeLimit = CVAR_GET_FLOAT("mp_timelimit") * 60;
	
	time_remaining = (int)(flTimeLimit ? ( flTimeLimit - gpGlobals->time ) : 0);

	if (m_fShowTimer != timelimit.value)
	{
		if (time_remaining > 0)
		{
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgRoundTime);
				WRITE_SHORT(time_remaining);
			MESSAGE_END();
		}
		else
		{
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgShowTimer);
				WRITE_BYTE(0);
			MESSAGE_END();
		}

		m_fShowTimer = timelimit.value;
	}

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
		if (m_iSuccessfulRounds < 1)
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
				if (plr->IsSpectator())
				{
					if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
					{
						int result = ((m_iTopLevel + 1) / MAXLEVEL) * 100;
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
							WRITE_STRING("GunGame in progress");
							WRITE_STRING(UTIL_VarArgs("Top level is %s [%d of %d]", g_WeaponId[m_iTopLevel], m_iTopLevel + 1, MAXLEVEL));
							WRITE_BYTE(result);
							//WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
						MESSAGE_END();
					}
				}
				else
				{
					/*
					plr->DisplayHudMessage(UTIL_VarArgs("Round %d of %d\nYour Level: %d | Top Level: %d", 
						m_iSuccessfulRounds+1, (int)roundlimit.value, (int)plr->pev->fuser4, m_iTopLevel),
						TXT_CHANNEL_GAME_INFO, -1, 0.83, 255, 255, 255, 0, 0, 0, 60, 0);
					*/
					
					int result = ((plr->pev->fuser4 + 1) / MAXLEVEL) * 100;

					if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
					{
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
							WRITE_STRING("Get through your weapon list");
							WRITE_STRING(UTIL_VarArgs("Your progress: %d of %d", (int)plr->pev->fuser4 + 1, MAXLEVEL));
							WRITE_BYTE(result);
							WRITE_STRING(UTIL_VarArgs("Next is %s", g_WeaponId[(int)plr->pev->fuser4+1]));
							//if (plr->m_iRoundWins > 0)
							//	WRITE_STRING(UTIL_VarArgs("Won %d rounds: %d to go", plr->m_iRoundWins, (int)roundlimit.value - m_iSuccessfulRounds));
							//else
							// WRITE_STRING(UTIL_VarArgs("Round %d: %d to go", m_iSuccessfulRounds + 1, (int)roundlimit.value - 1));
							// WRITE_STRING(UTIL_VarArgs("Next weapon: %s", g_WeaponId[(int)plr->pev->fuser4 + 1]));
						MESSAGE_END();
					}
				}
			}
		}

		// Needed to delay voice over since deploy of weapon mutes client play sound
		if (m_hVoiceHandle != NULL && m_iVoiceId > 0)
		{
			MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, m_hVoiceHandle->edict() );
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
			//WRITE_STRING(UTIL_VarArgs("Best of %d", (int)roundlimit.value));
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
									MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, plr->edict() );
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
								if (ggstartlevel.value > 0 && ggstartlevel.value < MAXLEVEL)
								{
									plr->pev->fuser4 = ggstartlevel.value;
									plr->pev->frags = g_iFrags[(int)ggstartlevel.value - 1];
								}
								else
								{
									plr->pev->frags = plr->pev->fuser4 = 0;
								}

								MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
									WRITE_BYTE( ENTINDEX(plr->edict()) );
									WRITE_SHORT( plr->pev->frags );
									WRITE_SHORT( plr->m_iDeaths = 0 );
									WRITE_SHORT( 0 );
									WRITE_SHORT( 0 );
								MESSAGE_END();
								plr->m_iAssists = 0;

								if (plr->IsAlive())
									plr->RemoveAllItems(FALSE);

								MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("GunGame complete");
									WRITE_STRING("");
									WRITE_BYTE(0);
									WRITE_STRING(UTIL_VarArgs("Winner of round %d is %s!\n", m_iSuccessfulRounds+1, STRING(pAttacker->pev->netname)));
								MESSAGE_END();
							}
						}

						m_fRefreshStats = -1;

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
					if (!pAttacker->HasNamedPlayerItem("weapon_fists"))
						pAttacker->GiveNamedItem("weapon_fists");
					char weapon[32];
					sprintf(weapon, "%s%s", "weapon_", g_WeaponId[newLevel]);
					if (!pAttacker->HasNamedPlayerItem(weapon))
						pAttacker->GiveNamedItem(STRING(ALLOC_STRING(weapon)));
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
	if (!pPlayer->HasNamedPlayerItem("weapon_fists"))
		pPlayer->GiveNamedItem("weapon_fists");

	// In full game, go deep with negative deaths but in short game, pin to lowest level
	if (ggstartlevel.value > 0 && pPlayer->pev->fuser4 < ggstartlevel.value) {
		pPlayer->pev->fuser4 = ggstartlevel.value;
		pPlayer->pev->frags = g_iFrags[(int)ggstartlevel.value - 1];
		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
			WRITE_SHORT( pPlayer->pev->frags );
			WRITE_SHORT( pPlayer->m_iDeaths );
			WRITE_SHORT( 0 );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}
	
	int currentLevel = (int)pPlayer->pev->fuser4;
	char weapon[32];
	sprintf(weapon, "%s%s", "weapon_", g_WeaponId[currentLevel]);
	if (!pPlayer->HasNamedPlayerItem(weapon))
		pPlayer->GiveNamedItem(STRING(ALLOC_STRING(weapon)));
	pPlayer->GiveAmmo(AMMO_GLOCKCLIP_GIVE * 4, "9mm", _9MM_MAX_CARRY);
	pPlayer->GiveAmmo(AMMO_357BOX_GIVE * 4, "357", _357_MAX_CARRY);
	pPlayer->GiveAmmo(AMMO_BUCKSHOTBOX_GIVE * 4, "buckshot", BUCKSHOT_MAX_CARRY);
	pPlayer->GiveAmmo(AMMO_CROSSBOWCLIP_GIVE * 4, "bolts", BOLT_MAX_CARRY);
	pPlayer->GiveAmmo(AMMO_M203BOX_GIVE * 2, "ARgrenades", M203_GRENADE_MAX_CARRY);
	pPlayer->GiveAmmo(AMMO_RPGCLIP_GIVE * 2, "rockets", ROCKET_MAX_CARRY);
	pPlayer->GiveAmmo(AMMO_URANIUMBOX_GIVE * 4, "uranium", URANIUM_MAX_CARRY);

	ClientPrint(pPlayer->pev, HUD_PRINTTALK, UTIL_VarArgs("[GunGame]: You need %d frags to reach level %s.\n",
		g_iFrags[currentLevel] - ((int)pPlayer->pev->frags), g_WeaponId[currentLevel+1]));

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

BOOL CHalfLifeGunGame::IsAllowedToDropWeapon( CBasePlayer *pPlayer )
{
	return FALSE;
}

BOOL CHalfLifeGunGame::IsAllowedToHolsterWeapon( void )
{
	return FALSE;
}

BOOL CHalfLifeGunGame::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if (!strcmp(STRING(pItem->pev->classname), "weapon_nuke"))
		return FALSE;

	return CHalfLifeMultiplay::CanHavePlayerItem( pPlayer, pItem );
}

BOOL CHalfLifeGunGame::CanRandomizeWeapon( const char *name )
{
	if (strcmp(name, "weapon_nuke") == 0)
		return FALSE;

	return TRUE;
}

BOOL CHalfLifeGunGame::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	// No damage after round ends as we walk around
	if (m_fGoToIntermission > 0)
		return FALSE;

	return TRUE;
}

BOOL CHalfLifeGunGame::MutatorAllowed(const char *mutator)
{
	if (strstr(mutator, g_szMutators[MUTATOR_BARRELS - 1]) || atoi(mutator) == MUTATOR_BARRELS)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_BERSERKER - 1]) || atoi(mutator) == MUTATOR_BERSERKER)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_DEALTER - 1]) || atoi(mutator) == MUTATOR_DEALTER)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_DONTSHOOT - 1]) || atoi(mutator) == MUTATOR_DONTSHOOT)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_INSTAGIB - 1]) || atoi(mutator) == MUTATOR_INSTAGIB)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_PLUMBER - 1]) || atoi(mutator) == MUTATOR_PLUMBER)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_COWBOY - 1]) || atoi(mutator) == MUTATOR_COWBOY)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_BUSTERS - 1]) || atoi(mutator) == MUTATOR_BUSTERS)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_FIRESTARTER - 1]) || atoi(mutator) == MUTATOR_FIRESTARTER)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_PORTAL - 1]) || atoi(mutator) == MUTATOR_PORTAL)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_RAILGUNS - 1]) || atoi(mutator) == MUTATOR_RAILGUNS)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_RICOCHET - 1]) || atoi(mutator) == MUTATOR_RICOCHET)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_ROCKETBEES - 1]) || atoi(mutator) == MUTATOR_ROCKETBEES)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_ROCKETCROWBAR - 1]) || atoi(mutator) == MUTATOR_ROCKETCROWBAR)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_VESTED - 1]) || atoi(mutator) == MUTATOR_VESTED)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_RANDOMWEAPON - 1]) || atoi(mutator) == MUTATOR_RANDOMWEAPON)
		return FALSE;
	
	return CHalfLifeMultiplay::MutatorAllowed(mutator);
}
