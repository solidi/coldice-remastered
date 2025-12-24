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

#define MAXLEVEL 49

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
	"fingergun",
	"zapgun",
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
	"wrench",
	"dual_wrench",
	"crowbar",
	"rocketcrowbar",
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

	CheckMutatorRTV();

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

	if ( ggstartlevel.value < 0 || ggstartlevel.value > MAXLEVEL)
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
						MESSAGE_END();
					}
				}
				else
				{
					int result = ((plr->m_iRoundWins + 1) / MAXLEVEL) * 100;

					if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
					{
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
							WRITE_STRING("Get through your weapon list");
							WRITE_STRING(UTIL_VarArgs("Your progress: %d of %d", plr->m_iRoundWins + 1, MAXLEVEL));
							WRITE_BYTE(result);
							WRITE_STRING(UTIL_VarArgs("Next is %s", g_WeaponId[plr->m_iRoundWins+1]));
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
			WRITE_STRING(UTIL_VarArgs("Your progress: %d of %d", pPlayer->m_iRoundWins + 1, MAXLEVEL));
			WRITE_BYTE(0);
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
		// Suicide
		if (ggsuicide.value > 0 && pVictim->pev == pKiller)
		{
			int roundWins = pVictim->m_iRoundWins;
			if (roundWins > 0)
			{
				pVictim->m_iRoundWins = roundWins - 1;
				pVictim->pev->frags = (pVictim->m_iRoundWins * (int)ggfrags.value);
				MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
					WRITE_BYTE( ENTINDEX(pVictim->edict()) );
					WRITE_SHORT( pVictim->pev->frags );
					WRITE_SHORT( pVictim->m_iDeaths );
					WRITE_SHORT( pVictim->m_iRoundWins + 1 );
					WRITE_SHORT( 0 );
				MESSAGE_END();
				ClientPrint(pVictim->pev, HUD_PRINTTALK, "[GunGame]: You level was lost by suicide!\n");

				m_fRefreshStats = gpGlobals->time;
			}
		}
		// Steals
		else if (ggsteallevel.value > 0)
		{
			if (pInflictor && ktmp->IsPlayer() && FClassnameIs(pInflictor, "weapon_knife"))
			{
				CBasePlayer *plr = ((CBasePlayer *)ktmp);
				int roundWins = pVictim->m_iRoundWins;
				if (roundWins > 0)
				{
					pVictim->m_iRoundWins = roundWins - 1;
					pVictim->pev->frags = (pVictim->m_iRoundWins * (int)ggfrags.value);
					MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
						WRITE_BYTE( ENTINDEX(pVictim->edict()) );
						WRITE_SHORT( pVictim->pev->frags );
						WRITE_SHORT( pVictim->m_iDeaths );
						WRITE_SHORT( pVictim->m_iRoundWins + 1 );
						WRITE_SHORT( 0 );
					MESSAGE_END();
					ClientPrint(pVictim->pev, HUD_PRINTTALK,
						UTIL_VarArgs("[GunGame]: You level was stolen by %s!\n",
						STRING(plr->pev->netname)));
				}

				roundWins = plr->m_iRoundWins;

				if ( roundWins < MAXLEVEL - 1)
				{
					plr->m_iRoundWins = roundWins + 1;
					plr->pev->frags = ((roundWins + 2) * (int)ggfrags.value);
					ClientPrint(plr->pev, HUD_PRINTTALK, UTIL_VarArgs("[GunGame]: You level was increased by a steal!\n"));

					MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
						WRITE_BYTE( ENTINDEX(plr->edict()) );
						WRITE_SHORT( plr->pev->frags );
						WRITE_SHORT( plr->m_iDeaths );
						WRITE_SHORT( plr->m_iRoundWins + 1 );
						WRITE_SHORT( 0 );
					MESSAGE_END();
				}

				m_fRefreshStats = gpGlobals->time;
			}
		}

		if (ktmp->IsPlayer())
		{
			CBasePlayer *plr = ((CBasePlayer *)ktmp);
			char weapon[32];
			sprintf(weapon, "%s%s", "weapon_", g_WeaponId[plr->m_iRoundWins]);

			if (plr->IsAlive() && plr->m_iRoundWins < MAXLEVEL)
			{
				plr->RemoveAllItems(FALSE);
				GiveMutators(plr);
				plr->GiveNamedItem("weapon_fists");
				if (ggsteallevel.value > 0)
					plr->GiveNamedItem("weapon_knife");
				plr->GiveNamedItem(STRING(ALLOC_STRING(weapon)));
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
			if (m_iTopLevel < plr->m_iRoundWins)
				m_iTopLevel = plr->m_iRoundWins;
		}
	}
}

int CHalfLifeGunGame::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	if (m_fGoToIntermission == 0 && pAttacker && pKilled)
	{
		BOOL isOffhand = (gMultiDamage.pEntity == pKilled && (gMultiDamage.type & DMG_PUNCH || gMultiDamage.type & DMG_KICK) && 
						(pAttacker->pev->flags & FL_CLIENT) && gMultiDamage.time > gpGlobals->time - 0.5);
		if (isOffhand)
			return 0;

		BOOL isWeapon = pAttacker->m_pActiveItem && !strcmp(pAttacker->m_pActiveItem->pszName(), "weapon_fists") && gMultiDamage.type & DMG_PUNCH;
		if (isWeapon)
			return 0;

		// Attacker
		int currentLevel = (int)pAttacker->m_iRoundWins;
		if (currentLevel <= MAXLEVEL)
		{
			int inc = 2;
			currentLevel == 0 ? inc = 1 : 0; 
			// Note, frags are increased after this method, so assume +1
			if ((int)pAttacker->pev->frags+1 >= ((currentLevel+inc) * (int)ggfrags.value))
			{
				if (!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "snark"))
					DeactivateItems(pAttacker, "monster_snark");
				else if (!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "chumtoad"))
					DeactivateItems(pAttacker, "monster_chumtoad");
				else if (!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "crossbow"))
					DeactivateItems(pAttacker, "bolt");
				else if (!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "gravitygun"))
					DeactivateItems(pAttacker, "monster_barrel");
				else if (!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "freezegun"))
					DeactivateItems(pAttacker, "plasma");
				else if (!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "handgrenade") ||
						!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "9mmAR") ||
						!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "glauncher"))
					DeactivateItems(pAttacker, "grenade");
				else if (!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "rpg") ||
						!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "dual_rpg"))
					DeactivateItems(pAttacker, "rpg_rocket");
				else if (!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "hornetgun") ||
						!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "dual_hornetgun"))
					DeactivateItems(pAttacker, "hornet");
				else if (!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "flamethrower") ||
						!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "dual_flamethrower"))
					DeactivateItems(pAttacker, "flameball");
				else if (!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "cannon"))
				{
					DeactivateItems(pAttacker, "flak");
					DeactivateItems(pAttacker, "flak_bomb");
				}
				else if (!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "tripmine"))
					DeactivateItems(pAttacker, "monster_tripmine");
				else if (!strcmp(g_WeaponId[pAttacker->m_iRoundWins], "satchel"))
					DeactivateSatchels(pAttacker);

				pAttacker->m_iRoundWins += 1;
				int newLevel = (int)pAttacker->m_iRoundWins;
				int voiceId = 0;

				ClientPrint(pAttacker->pev, HUD_PRINTTALK,
					UTIL_VarArgs("[GunGame]: You increased your level to %d (obtained %s)!\n",
					newLevel+1, g_WeaponId[newLevel]));

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
								if (plr != pAttacker && plr->m_iRoundWins == (m_iTopLevel-1)) {
									// Play immediately since GiveNamedItem is not used in this context
									MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, plr->edict() );
										WRITE_BYTE(CLIENT_SOUND_LOSTLEAD);
									MESSAGE_END();
								}
							}
						}
					}

					if (m_iTopLevel >= MAXLEVEL)
					{
						MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
							WRITE_BYTE(CLIENT_SOUND_ROUND_OVER);
						MESSAGE_END();

						m_hLeader = pAttacker;
						m_iSuccessfulRounds++;
						m_fGoToIntermission = gpGlobals->time + 8.0;
						m_iTopLevel = 0;

						// Update scoreboard for winner
						pAttacker->m_iRoundWins++;

						UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("%s has won GunGame!\n", STRING(pAttacker->pev->netname)));

						for ( int i = 1; i <= gpGlobals->maxClients; i++ )
						{
							CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

							if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
							{
								if (plr->IsAlive())
									plr->RemoveAllItems(FALSE);

								MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("GunGame complete");
									WRITE_STRING("");
									WRITE_BYTE(0);
									WRITE_STRING(UTIL_VarArgs("The winner is %s!\n", STRING(pAttacker->pev->netname)));
								MESSAGE_END();
							}
						}

						m_fRefreshStats = -1;

						return 1;
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

				m_hVoiceHandle = pAttacker;
				m_fRefreshStats = gpGlobals->time + 1.0;
			}
			else 
			{
				int inc = 2;
				currentLevel == 0 ? inc = 1 : 0; 
				ClientPrint(pAttacker->pev, HUD_PRINTTALK,
					UTIL_VarArgs("[GunGame]: You need %d frags to reach level %s!\n",
					((currentLevel+inc) * (int)ggfrags.value) - ((int)pAttacker->pev->frags+1), g_WeaponId[currentLevel+1]));
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
	if (ggsteallevel.value > 0)
		pPlayer->GiveNamedItem("weapon_knife");

	// In full game, go deep with negative deaths but in short game, pin to lowest level
	if (ggstartlevel.value > 1 && pPlayer->m_iRoundWins < ggstartlevel.value) {
		pPlayer->m_iRoundWins = (int)ggstartlevel.value - 1;
		pPlayer->pev->frags = (int)ggfrags.value * (int)ggstartlevel.value;
	}

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iRoundWins + 1 );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	// Game is over, do not give further items
	if (m_iTopLevel >= MAXLEVEL)
		return;

	int currentLevel = pPlayer->m_iRoundWins;
	char weapon[32];
	sprintf(weapon, "%s%s", "weapon_", g_WeaponId[currentLevel]);
	if (!pPlayer->HasNamedPlayerItem(weapon))
		pPlayer->GiveNamedItem(STRING(ALLOC_STRING(weapon)));

	int inc = 2;
	currentLevel == 0 ? inc = 1 : 0; 

	ClientPrint(pPlayer->pev, HUD_PRINTTALK, UTIL_VarArgs("[GunGame]: You need %d frags to reach level %s.\n",
		((currentLevel+inc) * (int)ggfrags.value) - ((int)pPlayer->pev->frags), g_WeaponId[currentLevel+1]));

	g_pGameRules->SpawnMutators(pPlayer);
}

const char *ammoList[] =
{
	"ammo_rpgclip",
	"ammo_9mmAR",
	"ammo_ARgrenades",
	"ammo_357",
	"ammo_buckshot",
	"ammo_9mmclip",
	"ammo_gaussclip",
	"ammo_crossbow",
	"item_healthkit",
	"item_battery",
	"item_longjump",
};

BOOL CHalfLifeGunGame::IsAllowedToSpawn( CBaseEntity *pEntity )
{
	if (!FBitSet(pEntity->pev->spawnflags, SF_GIVEITEM) && strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0)
	{
		CBaseEntity::Create((char *)ammoList[RANDOM_LONG(0, ARRAYSIZE(ammoList)-1)], pEntity->pev->origin, pEntity->pev->angles, pEntity->pev->owner);
		return FALSE;
	}

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
