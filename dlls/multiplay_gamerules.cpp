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
//
// teamplay_gamerules.cpp
//
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"
#include	"trains.h"
 
#include	"skill.h"
#include	"game.h"
#include	"items.h"
#include	"voice_gamemgr.h"
#include	"hltv.h"
#include	"shake.h"

#include	"pm_shared.h"

#if !defined ( _WIN32 )
#include <ctype.h>
#endif

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;

extern int gmsgDeathMsg;	// client dll messages
extern int gmsgScoreInfo;
extern int gmsgMOTD;
extern int gmsgServerName;
extern int gmsgStatusIcon;
extern int gmsgObjective;

extern int gmsgVoteGameplay;
extern int gmsgVoteMap;
extern int gmsgVoteMutator;
extern int gmsgShowTimer;
extern int gmsgRoundTime;
extern int gmsgAddMutator;

extern DLL_GLOBAL int g_GameMode;
extern int gmsgPlayClientSound;

extern int g_teamplay;

#define ITEM_RESPAWN_TIME	30
#define WEAPON_RESPAWN_TIME	20
#define AMMO_RESPAWN_TIME	20

float g_flIntermissionStartTime = 0;

CVoiceGameMgr	g_VoiceGameMgr;

#ifdef _DEBUG
DLL_GLOBAL float g_iKickSomeone;
#endif

class CMultiplayGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	virtual bool		CanPlayerHearPlayer(CBasePlayer *pListener, CBasePlayer *pTalker)
	{
		if ( g_teamplay )
		{
			if ( g_pGameRules->PlayerRelationship( pListener, pTalker ) != GR_TEAMMATE )
			{
				return false;
			}
		}

		return true;
	}
};
static CMultiplayGameMgrHelper g_GameMgrHelper;

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************

CHalfLifeMultiplay :: CHalfLifeMultiplay()
{
	g_VoiceGameMgr.Init(&g_GameMgrHelper, gpGlobals->maxClients);

	RefreshSkillData();
	m_flIntermissionEndTime = 0;
	m_iFirstBloodDecided = FALSE;
	g_flIntermissionStartTime = 0;
	memset(m_iVoteCount, -1, sizeof(m_iVoteCount));
	m_iVoteUnderway = 0;
	m_iDecidedMapIndex = -1;
	m_fMutatorVoteTime = 0;
	
	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that 
	//  server ops can run multiple game servers, with different server .cfg files,
	//  from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)
	if ( IS_DEDICATED_SERVER() )
	{
		// this code has been moved into engine, to only run server.cfg once
	}
	else
	{
		// listen server
		char *lservercfgfile = (char *)CVAR_GET_STRING( "lservercfgfile" );

		if ( lservercfgfile && lservercfgfile[0] )
		{
			char szCommand[256];
			
			ALERT( at_console, "Executing listen server config file\n" );
			sprintf( szCommand, "exec %s\n", lservercfgfile );
			SERVER_COMMAND( szCommand );
		}
	}

	ResetGameMode();

#ifdef _DEBUG
	g_iKickSomeone = gpGlobals->time + debug_disconnects_time.value;
#endif
}

BOOL CHalfLifeMultiplay::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if(g_VoiceGameMgr.ClientCommand(pPlayer, pcmd))
		return TRUE;

	return CGameRules::ClientCommand(pPlayer, pcmd);
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::RefreshSkillData( void )
{
// load all default values
	CGameRules::RefreshSkillData();

// override some values for multiplay.

	// Snowball
	if (snowballfight.value)
		gSkillData.plrDmgSnowball = 250;

	if (MutatorEnabled(MUTATOR_GOLDENGUNS))
	{
		float damage = 900.0;
		gSkillData.plrDmg9MM = damage;
		gSkillData.plrDmg357 = damage;
		gSkillData.plrDmgSniperRifle = damage;
		gSkillData.plrDmgMP5 = damage;
		gSkillData.plrDmgM203Grenade = damage;
		gSkillData.plrDmgBuckshot = damage;
		gSkillData.plrDmgExpBuckshot = damage;
		gSkillData.plrDmgCrossbowClient = damage;
		gSkillData.plrDmgRPG = damage;
		gSkillData.plrDmgGauss = damage;
		gSkillData.plrDmgEgonNarrow = damage;
		gSkillData.plrDmgEgonWide = damage;
		gSkillData.plrDmgHandGrenade = damage;
		gSkillData.plrDmgSatchel = damage;
		gSkillData.plrDmgTripmine = damage;
		gSkillData.plrDmgVest = damage;
		gSkillData.plrDmgClusterGrenade = damage;
		gSkillData.plrDmgRailgun = damage;
		gSkillData.plrDmgFlak = damage;
		gSkillData.plrDmgFlakBomb = damage;
		gSkillData.plrDmgPlasma = damage;
		gSkillData.plrDmgKnifeSnipe = damage;
		gSkillData.plrDmgNuke = damage;
		gSkillData.plrDmgHornet = damage;
		gSkillData.plrDmgKnife = damage;
		gSkillData.plrDmgFlyingKnife = damage;
		gSkillData.plrDmgFlyingCrowbar = damage;
		gSkillData.chumtoadDmgBite = damage;
		gSkillData.chumtoadDmgPop = damage;
		gSkillData.plrDmgWrench = damage;
		gSkillData.plrDmgFlyingWrench = damage;
		gSkillData.plrDmgSnowball = damage;
		gSkillData.plrDmgChainsaw = damage;
		gSkillData.plrDmgGravityGun = damage;
		gSkillData.plrDmgFlameThrower = damage;
	}
}

// longest the intermission can last, in seconds
#define MAX_INTERMISSION_TIME		120

extern cvar_t timeleft, fragsleft, roundtimeleft;

extern cvar_t mp_chattime;

char *sBuiltInMaps[] =
{
	"bounce2",
	"canyon",
	"catacombs",
	"chillworks",
	"cold_base",
	"coldice",
	"comet",
//	"defroster",
	"datafloe",
	"depot",
	"doublefrost",
	"drift",
	"fences",
	"focus",
	"frostfire",
	"frostmill",
	"frosty",
	"frozen_bunker",
	"frozenwarehouse",
	"furrow",
	"glacialcore",
	"glupshitto",
	"ice_pit",
	"latenightxmas",
	"overflow",
//	"quadfrost",
	"snow_camp",
	"snowcross",
	"snowtransit",
	"snowyard",
	"stalkyard2",
	"storm",
	"thechill",
	"themill",
	"thetemple",
	"training",
	"training2",
	"RANDOM",
};

#define BUILT_IN_MAP_COUNT 35

char *gamePlayModes[] = {
	"Deathmatch",
	"1 vs. 1",
	"Battle Royal",
	"Busters",
	"Chilldemic",
	"Cold Skulls",
	"Cold Spot",
	"Capture The Chumtoad",
	"Capture The Flag",
	"GunGame",
	"Horde",
	"Instagib",
	"Jesus vs. Santa",
	"Prop Hunt",
	"Shidden",
	"Snowballs",
	"Teamplay",
};

extern void Vote( CBasePlayer *pPlayer, int vote );
extern const char *szGameModeList[TOTAL_GAME_MODES];

int CHalfLifeMultiplay::RandomizeMutator( void )
{
	int attempts = 3, mutatorVote = 1;
	while (attempts > 0)
	{
		mutatorVote = RANDOM_LONG(MUTATOR_CHAOS, MAX_MUTATORS + 1 /*random*/);
		if (mutatorVote - 1 >= MAX_MUTATORS)
			break;
		const char *tryIt = g_szMutators[mutatorVote - 1];

		// If gamerules disallows it
		if (!g_pGameRules->MutatorAllowed(tryIt))
		{
			mutatorVote = 1; // if it fails, default is chaos
			attempts--;
		}
		else if (strlen(chaosfilter.string) > 2 && strstr(chaosfilter.string, tryIt))
		{
			mutatorVote = 1; // if it fails, default is chaos
			attempts--;
		}
		else
		{
			break;
		}
	}
	return mutatorVote;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: Think ( void )
{
	g_VoiceGameMgr.Update(gpGlobals->frametime);

	// No checks during intermission
	if ( !m_flIntermissionEndTime )
	{
		g_pGameRules->MutatorsThink();
		g_pGameRules->CheckGameMode();
	}

	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining = 0;

	if ( m_fMutatorVoteTime )
	{
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if (pPlayer && !FBitSet(pPlayer->pev->flags, FL_FAKECLIENT) && !pPlayer->HasDisconnected &&
				(pPlayer->m_fVoteCoolDown && (pPlayer->m_fVoteCoolDown + 1 <= gpGlobals->time)))
			{
				MESSAGE_BEGIN(MSG_ONE, gmsgVoteMutator, NULL, pPlayer->edict());
					WRITE_BYTE(0);
				MESSAGE_END();
				pPlayer->m_fVoteCoolDown = 0;
			}
		}

		if ( m_fMutatorVoteTime <= gpGlobals->time )
		{
			MESSAGE_BEGIN(MSG_ALL, gmsgVoteMutator);
				WRITE_BYTE(0);
			MESSAGE_END();

			// tally votes
			int vote[MAX_MUTATORS+2]; //+1, +1 RANDOM
			memset(vote, -1, sizeof(vote));

			for (int j = 1; j <= gpGlobals->maxClients; j++)
			{
				int mutatorIndex = g_pGameRules->m_iVoteCount[j-1];
				if ((mutatorIndex-1) >= 0 && (mutatorIndex-1) <= MAX_MUTATORS + 1 /*random*/)
				{
					if (vote[mutatorIndex-1] == -1) vote[mutatorIndex-1] = 0;
					vote[mutatorIndex-1]++;
				}
			}

			int first, second, third;
			third = first = second = -1;
			int fIndex, sIndex, tIndex;
			fIndex = sIndex = tIndex = -1;

			for (int i = 0; i <= MAX_MUTATORS + 1 /*random*/; i++)
			{
				if (vote[i] > first)
				{
					tIndex = sIndex;
					third = second;
					sIndex = fIndex;
					second = first;
					fIndex = i;
					first = vote[i];
				}
				else if (vote[i] > second)
				{
					tIndex = sIndex;
					third = second;
					sIndex = i;
					second = vote[i];
				}
				else if (vote[i] > third)
				{
					tIndex = i;
					third = vote[i];
				}
			}

			memset(m_iVoteCount, -1, sizeof(m_iVoteCount));

			if (first < 0 && second < 0 && third < 0)
			{
				UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Not enough votes received for mutators.\n");
			}
			else
			{
				if (fIndex == MAX_MUTATORS /*random*/)
				{
					UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Randomizing mutator mode #1...\n");
					fIndex = RandomizeMutator() - 1;
				}

				if (sIndex == MAX_MUTATORS /*random*/)
				{
					UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Randomizing mutator mode #2...\n");
					sIndex = RandomizeMutator() - 1;
				}

				if (tIndex == MAX_MUTATORS /*random*/)
				{
					UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Randomizing mutator mode #3...\n");
					tIndex = RandomizeMutator() - 1;
				}

				ALERT(at_aiconsole, "fIndex=%d, sIndex=%d, tIndex=%d\n", fIndex, sIndex, tIndex);

				if (fIndex >= 0 && sIndex >= 0 && tIndex >= 0)
				{
					UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] \"%s\", \"%s\" and \"%s\" are the new mutators!\n", g_szMutators[fIndex], g_szMutators[sIndex], g_szMutators[tIndex]);
					SERVER_COMMAND(UTIL_VarArgs("sv_mutatorlist \"%s253;%s253;%s253\"\n", g_szMutators[fIndex], g_szMutators[sIndex], g_szMutators[tIndex]));
				}
				else if (fIndex >= 0 && sIndex >= 0)
				{
					UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[VOTE] \"%s\" and \"%s\" are the new mutators!\n", g_szMutators[fIndex], g_szMutators[sIndex]));
					SERVER_COMMAND(UTIL_VarArgs("sv_mutatorlist \"%s253;%s253\"\n", g_szMutators[fIndex], g_szMutators[sIndex]));
				}
				else
				{
					UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[VOTE] \"%s\" is the new mutator!\n", g_szMutators[fIndex]));
					SERVER_COMMAND(UTIL_VarArgs("sv_mutatorlist \"%ss253\"\n", g_szMutators[fIndex]));
				}
			}

			m_fMutatorVoteTime = 0;
		}
	}

	if ( g_fGameOver )   // someone else quit the game already
	{
		// bounds check
		int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
		if ( time < 1 )
			CVAR_SET_STRING( "mp_chattime", "1" );
		else if ( time > MAX_INTERMISSION_TIME )
			CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

		int timeLeft = (voting.value * 3) + 12;
		m_flIntermissionEndTime = g_flIntermissionStartTime + mp_chattime.value + timeLeft;

		if (voting.value >= 10)
		{
			if (m_iVoteUnderway == 1 && ((m_flIntermissionEndTime - mp_chattime.value - (timeLeft - 3)) < gpGlobals->time))
			{
				m_iVoteUnderway = 2;

				MESSAGE_BEGIN(MSG_ALL, gmsgVoteGameplay);
					WRITE_BYTE(voting.value);
				MESSAGE_END();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_VOTEGAME);
				MESSAGE_END();

				// Bots get a vote
				for (int i = 1; i <= gpGlobals->maxClients; i++)
				{
					CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
					if (pPlayer && FBitSet(pPlayer->pev->flags, FL_FAKECLIENT) && !pPlayer->HasDisconnected)
					{
						::Vote(pPlayer, RANDOM_LONG(1, TOTAL_GAME_MODES + 2 /*random*/));
					}
				}
			}
			// Game mode vote ended
			else if (m_iVoteUnderway == 2 && ((m_flIntermissionEndTime - mp_chattime.value - (timeLeft - voting.value - 3)) < gpGlobals->time))
			{
				m_iVoteUnderway = 3;

				MESSAGE_BEGIN(MSG_ALL, gmsgVoteGameplay);
					WRITE_BYTE(0);
				MESSAGE_END();

				// tally votes
				int vote[TOTAL_GAME_MODES + 2]; //+1, +1 RANDOM
				memset(vote, 0, sizeof(vote));

				for (int j = 1; j <= gpGlobals->maxClients; j++)
				{
					int gameIndex = g_pGameRules->m_iVoteCount[j-1];
					if ((gameIndex-1) >= GAME_FFA && (gameIndex-1) <= TOTAL_GAME_MODES + 1 /*random*/)
						vote[gameIndex-1]++;
					// ALERT(at_console, "gameIndex=%d vote[%d]=%d\n", gameIndex-1, gameIndex-1, vote[gameIndex-1]);
				}

				int highest = -9999;
				int gameIndex = GAME_FFA;
				for (int i = 0; i <= TOTAL_GAME_MODES + 1 /*random*/; i++)
				{
					if (highest <= vote[i])
					{
						// Tie, determine random if this index should be selected instead.
						if (highest == vote[i]) {
							if (RANDOM_LONG(0, 1)) {
								gameIndex = i;
							}
						}
						else
						{
							highest = vote[i];
							gameIndex = i;
						}
						// ALERT(at_console, "highest=%d, vote[i]=%d, gameIndex=%d\n", highest, vote[i], gameIndex);
					}
				}

				memset(m_iVoteCount, -1, sizeof(m_iVoteCount));

				if (highest <= 0)
				{
					UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Not enough votes received for gameplay.\n");
				}
				else
				{
					if (gameIndex == TOTAL_GAME_MODES + 1 /*random*/)
					{
						UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Randomizing gameplay mode...\n");
						gameIndex = RANDOM_LONG(GAME_FFA, TOTAL_GAME_MODES);
					}

					UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[VOTE] %s is the next gameplay mode!\n", gamePlayModes[gameIndex]));

					if (gameIndex == GAME_TEAMPLAY)
					{
						SERVER_COMMAND("mp_gamemode 0\n");
						SERVER_COMMAND("mp_teamplay 1\n");
					}
					else
					{
						SERVER_COMMAND("mp_teamplay 0\n");
						SERVER_COMMAND(UTIL_VarArgs("mp_gamemode %s\n", szGameModeList[gameIndex]));
					}
				}
			}

			// Mutator vote STARTED
			if (m_iVoteUnderway == 3 && ((m_flIntermissionEndTime - mp_chattime.value - (timeLeft - voting.value - 6)) < gpGlobals->time))
			{
				m_iVoteUnderway = 4;

				MESSAGE_BEGIN(MSG_ALL, gmsgVoteMutator);
					WRITE_BYTE(voting.value);
				MESSAGE_END();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_VOTEMUTATOR);
				MESSAGE_END();

				// Bots get a vote
				for (int i = 1; i <= gpGlobals->maxClients; i++)
				{
					CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
					if (pPlayer && FBitSet(pPlayer->pev->flags, FL_FAKECLIENT) && !pPlayer->HasDisconnected)
					{
						::Vote(pPlayer, RandomizeMutator());
					}
				}
			}

			// Mutator vote ended
			if (m_iVoteUnderway == 4 && ((m_flIntermissionEndTime - mp_chattime.value - (timeLeft - (voting.value * 2) - 6)) < gpGlobals->time))
			{
				m_iVoteUnderway = 5;

				MESSAGE_BEGIN(MSG_ALL, gmsgVoteMutator);
					WRITE_BYTE(0);
				MESSAGE_END();

				// tally votes
				int vote[MAX_MUTATORS+2]; //+1, +1 RANDOM
				memset(vote, -1, sizeof(vote));

				for (int j = 1; j <= gpGlobals->maxClients; j++)
				{
					int mutatorIndex = g_pGameRules->m_iVoteCount[j-1];
					if ((mutatorIndex-1) >= 0 && (mutatorIndex-1) <= MAX_MUTATORS + 1 /*random*/)
					{
						if (vote[mutatorIndex-1] == -1) vote[mutatorIndex-1] = 0;
						vote[mutatorIndex-1]++;
					}
					// ALERT(at_console, "mutatorIndex=%d vote[%d]=%d\n", mutatorIndex-1, mutatorIndex-1, vote[mutatorIndex-1]);
				}

				int first, second, third;
				third = first = second = -1;
				int fIndex, sIndex, tIndex;
				fIndex = sIndex = tIndex = -1;

				for (int i = 0; i <= MAX_MUTATORS + 1 /*random*/; i++)
				{
					if (vote[i] > first)
					{
						tIndex = sIndex;
						third = second;
						sIndex = fIndex;
						second = first;
						fIndex = i;
						first = vote[i];
					}
					else if (vote[i] > second)
					{
						tIndex = sIndex;
						third = second;
						sIndex = i;
						second = vote[i];
					}
					else if (vote[i] > third)
					{
						tIndex = i;
						third = vote[i];
					}
				}

				memset(m_iVoteCount, -1, sizeof(m_iVoteCount));

				ALERT(at_aiconsole, "first=%d, fIndex=%d, second=%d, sIndex=%d, third=%d, tIndex=%d\n", first, fIndex, second, sIndex, third, tIndex);

				if (first < 0 && second < 0 && third < 0)
				{
					UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Not enough votes received for mutators.\n");
				}
				else
				{
					if (fIndex == MAX_MUTATORS /*random*/)
					{
						UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Randomizing mutator mode #1...\n");
						fIndex = RandomizeMutator() - 1;
					}

					if (sIndex == MAX_MUTATORS /*random*/)
					{
						UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Randomizing mutator mode #2...\n");
						sIndex = RandomizeMutator() - 1;
					}

					if (tIndex == MAX_MUTATORS /*random*/)
					{
						UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Randomizing mutator mode #3...\n");
						tIndex = RandomizeMutator() - 1;
					}

					ALERT(at_aiconsole, "fIndex=%d, sIndex=%d, tIndex=%d\n", fIndex, sIndex, tIndex);

					if (fIndex >= 0 && sIndex >= 0 && tIndex >= 0)
					{
						UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] \"%s\", \"%s\" and \"%s\" are the next mutators!\n", g_szMutators[fIndex], g_szMutators[sIndex], g_szMutators[tIndex]);
						SERVER_COMMAND(UTIL_VarArgs("sv_mutatorlist \"%s253;%s253;%s253\"\n", g_szMutators[fIndex], g_szMutators[sIndex], g_szMutators[tIndex]));
					}
					else if (fIndex >= 0 && sIndex >= 0)
					{
						UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[VOTE] \"%s\" and \"%s\" are the next mutators!\n", g_szMutators[fIndex], g_szMutators[sIndex]));
						SERVER_COMMAND(UTIL_VarArgs("sv_mutatorlist \"%s253;%s253\"\n", g_szMutators[fIndex], g_szMutators[sIndex]));
					}
					else
					{
						UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[VOTE] \"%s\" is the next mutator!\n", g_szMutators[fIndex]));
						SERVER_COMMAND(UTIL_VarArgs("sv_mutatorlist \"%ss253\"\n", g_szMutators[fIndex]));
					}
				}
			}

			// Map vote STARTED
			if (m_iVoteUnderway == 5 && ((m_flIntermissionEndTime - mp_chattime.value - (timeLeft - (voting.value * 2) - 9)) < gpGlobals->time))
			{
				m_iVoteUnderway = 6;

				MESSAGE_BEGIN(MSG_ALL, gmsgVoteMap);
					WRITE_BYTE(voting.value);
				MESSAGE_END();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_VOTEMAP);
				MESSAGE_END();

				// Bots get a vote
				for (int i = 1; i <= gpGlobals->maxClients; i++)
				{
					CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
					if (pPlayer && FBitSet(pPlayer->pev->flags, FL_FAKECLIENT) && !pPlayer->HasDisconnected)
					{
						::Vote(pPlayer, RANDOM_LONG(1, BUILT_IN_MAP_COUNT + 1));
					}
				}
			}

			// Map vote ended
			if (m_iVoteUnderway == 6 && (((m_flIntermissionEndTime - mp_chattime.value) - (timeLeft - (voting.value * 3) - 9)) < gpGlobals->time))
			{
				m_iVoteUnderway = 0;

				MESSAGE_BEGIN(MSG_ALL, gmsgVoteMap);
					WRITE_BYTE(0);
				MESSAGE_END();

				// tally votes
				int vote[BUILT_IN_MAP_COUNT + 1 /*random*/];
				memset(vote, 0, sizeof(vote));

				for (int j = 1; j <= gpGlobals->maxClients; j++)
				{
					int mapIndex = g_pGameRules->m_iVoteCount[j-1];
					if ((mapIndex-1) >= 0 && (mapIndex-1) <= BUILT_IN_MAP_COUNT + 1 /*random*/)
						vote[mapIndex-1]++;
					// ALERT(at_console, "mapIndex=%d vote[%d]=%d\n", mapIndex-1, mapIndex-1, vote[mapIndex-1]);
				}

				int highest = -9999;
				int mapIndex = 0;
				for (int i = 0; i < BUILT_IN_MAP_COUNT + 1 /*random*/; i++)
				{
					if (highest <= vote[i])
					{
						// Tie, determine random if this index should be selected instead.
						if (highest == vote[i]) {
							if (RANDOM_LONG(0, 1)) {
								m_iDecidedMapIndex = mapIndex = i;
							}
						}
						else
						{
							highest = vote[i];
							m_iDecidedMapIndex = mapIndex = i;
						}
						// ALERT(at_console, "highest=%d, vote[i]=%d, mapIndex=%d\n", highest, vote[i], mapIndex);
					}
				}

				if (highest <= 0)
				{
					m_iDecidedMapIndex = -1;
					UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Not enough votes received for next map. Using mapcycle.txt.\n");
				}
				else
				{
					ALERT(at_console, "m_iDecidedMapIndex=%d\n", m_iDecidedMapIndex);
					
					if (m_iDecidedMapIndex == BUILT_IN_MAP_COUNT /*random*/)
					{
						UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Randomizing map...\n");
						m_iDecidedMapIndex = RANDOM_LONG(0, BUILT_IN_MAP_COUNT - 1);
					}

					UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[VOTE] %s is the next map!\n", sBuiltInMaps[m_iDecidedMapIndex]));
				}
			}
		}

		// check to see if we should change levels now
		if ( m_flIntermissionEndTime < gpGlobals->time )
		{
			if ( (m_iEndIntermissionButtonHit && !m_iVoteUnderway)  // check that someone has pressed a key, or the max intermission time is over
				|| ( ( g_flIntermissionStartTime + MAX_INTERMISSION_TIME ) < gpGlobals->time) ) 
				ChangeLevel(); // intermission is over
		}

		return;
	}

	float flTimeLimit = timelimit.value * 60;
	float flFragLimit = fraglimit.value;
	if (g_GameMode == GAME_BUSTERS ||
		g_GameMode == GAME_CTC ||
		g_GameMode == GAME_CTF ||
		g_GameMode == GAME_COLDSPOT ||
		g_GameMode == GAME_GUNGAME)
		flFragLimit = 0;

	time_remaining = (int)(flTimeLimit ? ( flTimeLimit - gpGlobals->time ) : 0);
	
	if ( flTimeLimit != 0 && gpGlobals->time >= flTimeLimit )
	{
		GoToIntermission();
		return;
	}

	if ( flFragLimit )
	{
		int bestfrags = 9999;
		int remain;

		// check if any player is over the frag limit
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

			if ( pPlayer && pPlayer->pev->frags >= flFragLimit )
			{
				GoToIntermission();
				return;
			}


			if ( pPlayer )
			{
				remain = flFragLimit - pPlayer->pev->frags;
				if ( remain < bestfrags )
				{
					bestfrags = remain;
				}
			}

		}
		frags_remaining = bestfrags;
	}

	// Updates when frags change
	if ( frags_remaining != last_frags )
	{
		g_engfuncs.pfnCvar_DirectSet( &fragsleft, UTIL_VarArgs( "%i", frags_remaining ) );
	}

	// Updates once per second
	if ( timeleft.value != last_time )
	{
#ifdef _DEBUG
		ALERT(at_console, "[NUMBER_OF_ENTITIES()=%d, gpGlobals->maxEntities=%d]\n", NUMBER_OF_ENTITIES(), gpGlobals->maxEntities);
		ALERT(at_aiconsole, "timeleft.value=%.2f map=%s\n", timeleft.value, STRING(gpGlobals->mapname));
#endif
		g_engfuncs.pfnCvar_DirectSet( &timeleft, UTIL_VarArgs( "%i", time_remaining ) );

		if (!IsRoundBased())
		{
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

			if ((g_GameMode == GAME_FFA || g_GameMode == GAME_SNOWBALL) && m_fShowFrags != fraglimit.value)
			{
				if (fraglimit.value > 0)
				{
					MESSAGE_BEGIN(MSG_ALL, gmsgObjective);
						if (g_GameMode == GAME_FFA)
							WRITE_STRING("Frag 'em");
						else if (g_GameMode == GAME_SNOWBALL)
							WRITE_STRING("Snowball 'em");
						WRITE_STRING(UTIL_VarArgs("Fraglimit %.0f", fraglimit.value));
						WRITE_BYTE(0);
					MESSAGE_END();
				}
				else
				{
					MESSAGE_BEGIN(MSG_ALL, gmsgObjective);
						if (g_GameMode == GAME_SNOWBALL)
							WRITE_STRING("Frag 'em");
						else if (g_GameMode == GAME_SNOWBALL)
							WRITE_STRING("Snowball 'em");
						WRITE_STRING("");
						WRITE_BYTE(0);
					MESSAGE_END();
				}

				m_fShowFrags = timelimit.value;
			}
		}
	}

	last_frags = frags_remaining;
	last_time  = time_remaining;

#ifdef _DEBUG
	if (NUMBER_OF_ENTITIES() > 1024)
		ALERT(at_console, "NUMBER_OF_ENTITIES(): %d | gpGlobals->maxEntities: %d\n", NUMBER_OF_ENTITIES(), gpGlobals->maxEntities);
#endif
}

void CHalfLifeMultiplay::VoteForMutator( void )
{
	if (m_fMutatorVoteTime > gpGlobals->time)
		return;

	SERVER_COMMAND("sv_addmutator \"clear\"\n");

	MESSAGE_BEGIN( MSG_ALL, gmsgVoteMutator );
		WRITE_BYTE(voting.value);
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
		WRITE_BYTE(CLIENT_SOUND_VOTEMUTATOR);
	MESSAGE_END();

	// Bots get a vote
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if (pPlayer && !pPlayer->HasDisconnected)
		{
			pPlayer->m_fVoteCoolDown = 0;
			if (FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
				::Vote(pPlayer, RandomizeMutator());
		}
	}

	m_fMutatorVoteTime = gpGlobals->time + voting.value;
}

int CHalfLifeMultiplay::CheckClients( void )
{
	int clients = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		m_iPlayersInArena[i-1] = 0;

		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
		{
#ifdef _DEBUG
			ALERT(at_aiconsole, "%s[%d] is accounted for\n", STRING(plr->pev->netname), i);
#endif
			clients++;
			plr->IsInArena = FALSE;
			plr->IsArmoredMan = FALSE;
			plr->pev->fuser4 = 0;
			plr->m_flForceToObserverTime = 0;
			m_iPlayersInArena[clients-1] = i;
		}
	}

	return clients;
}

void CHalfLifeMultiplay::InsertClientsIntoArena(float fragcount)
{
	m_iPlayersInGame = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
		{ 
			// Joining spectators do not get game mode message.
			UpdateGameMode( plr );

			// Must be all, to reset frags.
			MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
				WRITE_BYTE( ENTINDEX(plr->edict()) );
				WRITE_SHORT( plr->pev->frags = fragcount );
				WRITE_SHORT( plr->m_iDeaths = 0 );
				WRITE_SHORT( plr->m_iRoundWins );
				WRITE_SHORT( GetTeamIndex( plr->m_szTeamName ) + 1 );
			MESSAGE_END();
			plr->m_iAssists = 0;

			MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict() );
				WRITE_STRING("");
				WRITE_STRING("");
				WRITE_BYTE(0);
			MESSAGE_END();

			plr->IsInArena = TRUE;
			plr->ExitObserver();
			plr->m_iRoundPlays++;
			m_iPlayersInGame++;
		}
	}
}

BOOL CHalfLifeMultiplay::HasGameTimerExpired( void )
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

	return m_flRoundTimeLimit < gpGlobals->time;
}

void CHalfLifeMultiplay::SetRoundLimits( void )
{
	//enforce a timelimit if given proper value
	if ( roundtimelimit.value > 0 )
	{
		m_flRoundTimeLimit = gpGlobals->time + (roundtimelimit.value * 60.0);

		MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
			WRITE_BYTE(1);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ALL, gmsgRoundTime);
			WRITE_SHORT(roundtimelimit.value * 60.0);
		MESSAGE_END();
	}

	_30secwarning	= FALSE;
	_15secwarning	= FALSE;
	_3secwarning	= FALSE;
}

void CHalfLifeMultiplay::CheckRounds( void )
{
	if ( CVAR_GET_FLOAT("mp_roundlimit") > 0 )
	{
#ifdef _DEBUG
		//ALERT( at_notice, UTIL_VarArgs("SuccessfulRounds = %i\n", m_iSuccessfulRounds ));
#endif
		if ( m_iSuccessfulRounds >= CVAR_GET_FLOAT("mp_roundlimit") )
			GoToIntermission();
	}
}

void CHalfLifeMultiplay::RemoveAndFillItems( void )
{
	const char *pRemoveThese[] =
	{
		"monster_satchel",
		"monster_tripmine",
		"monster_chumtoad",
		"monster_snark",
		"nuke_rocket",
		"rpg_rocket",
		"grenade",
		"gib",
		"weaponbox",
	};

	CBaseEntity *pEntity = NULL;
	for (int itemIndex = 0; itemIndex < ARRAYSIZE(pRemoveThese); itemIndex++)
	{
		while ((pEntity = UTIL_FindEntityByClassname(pEntity, pRemoveThese[itemIndex])) != NULL)
		{
			/*ALERT(at_aiconsole, "Remove %s at [x=%.2f,y=%.2f,z=%.2f]\n", 
				pRemoveThese[itemIndex],
				pEntity->pev->origin.x,
				pEntity->pev->origin.y,
				pEntity->pev->origin.z);*/
			UTIL_Remove(pEntity);
		}
	}

	const char *pRechargeThese[] =
	{
		"func_recharge",
		"func_healthrecharger",
	};

	pEntity = NULL;
	for (int itemIndex = 0; itemIndex < ARRAYSIZE(pRechargeThese); itemIndex++)
	{
		while ((pEntity = UTIL_FindEntityByClassname(pEntity, pRechargeThese[itemIndex])) != NULL)
		{
			/*ALERT(at_aiconsole, "Recharge %s at [x=%.2f,y=%.2f,z=%.2f]\n", 
				pRechargeThese[itemIndex],
				pEntity->pev->origin.x,
				pEntity->pev->origin.y,
				pEntity->pev->origin.z);*/
			pEntity->OverrideReset();
		}
	}
}

void CHalfLifeMultiplay::SuckAllToSpectator( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer && pPlayer->IsPlayer() && !pPlayer->IsSpectator() && !pPlayer->HasDisconnected )
		{
			strcpy(pPlayer->m_szTeamName, "");
			MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
				WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
				WRITE_SHORT( pPlayer->pev->frags = 0 );
				WRITE_SHORT( pPlayer->m_iDeaths = 0 );
				WRITE_SHORT( pPlayer->m_iRoundWins );
				WRITE_SHORT( GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
			MESSAGE_END();

			edict_t *pentSpawnSpot = g_pGameRules->GetPlayerSpawnSpot( pPlayer );
			pPlayer->StartObserver(pentSpawnSpot->v.origin, VARS(pentSpawnSpot)->angles);
		}

		// Spectator fix if client is in eye of another during round end.
		if (!g_GameInProgress && pPlayer && pPlayer->IsSpectator())
		{
			if (pPlayer->m_iObserverLastMode == OBS_IN_EYE)
			{
				pPlayer->m_iObserverLastMode = OBS_ROAMING;
				pPlayer->Observer_SetMode(OBS_ROAMING);
			}
		}
	}

	if (m_iPlayRoundOver != m_iSuccessfulRounds && m_fWaitForPlayersTime < ((gpGlobals->time + roundwaittime.value) - 1))
	{
		MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
			WRITE_BYTE(CLIENT_SOUND_ROUND_OVER);
		MESSAGE_END();
		m_iPlayRoundOver = m_iSuccessfulRounds;
	}
}

void CHalfLifeMultiplay::DisplayWinnersGoods( CBasePlayer *pPlayer )
{
	//increase his win count
	pPlayer->m_iRoundWins++;

	//animate!
	pPlayer->Celebrate();

	//and display to the world what he does best!
	UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %s has won round #%d of %d!\n", STRING(pPlayer->pev->netname), m_iSuccessfulRounds+1, (int)roundlimit.value));
	UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %s record is %i for %i [%.1f%%]\n", STRING(pPlayer->pev->netname),
		pPlayer->m_iRoundWins,
		pPlayer->m_iRoundPlays,
		((float)pPlayer->m_iRoundWins / (float)pPlayer->m_iRoundPlays) * 100 ));
}

void CHalfLifeMultiplay::ResetGameMode( void )
{
	g_GameInProgress = FALSE;

	m_iCountDown = 5;
	m_fWaitForPlayersTime = -1;

	_30secwarning = FALSE;
	_15secwarning = FALSE;
	_3secwarning = FALSE;

	m_iSuccessfulRounds = 0;
	m_iPlayRoundOver = 0;
	flUpdateTime = 0;
	m_flRoundTimeLimit = 0;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsMultiplayer( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsDeathmatch( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsCoOp( void )
{
	return gpGlobals->coop;
}

BOOL CHalfLifeMultiplay::IsRoundBased( void )
{
	return FALSE;
}

BOOL CHalfLifeMultiplay::AllowMeleeDrop( void )
{
	return meleedrop.value;
}

float CHalfLifeMultiplay::WeaponMultipler( void )
{
	if (g_pGameRules->MutatorEnabled(MUTATOR_FASTWEAPONS))
		return 0.33;

	if (g_pGameRules->MutatorEnabled(MUTATOR_SLOWWEAPONS))
		return 3;

	return 1;
}

BOOL CHalfLifeMultiplay::AllowRuneSpawn( const char *szRune )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	// Bot patch
	if ( !pWeapon->m_pPlayer ) {
		return FALSE;
	}

	if ( !pWeapon->CanDeploy() )
	{
		// that weapon can't deploy anyway.
		return FALSE;
	}

	if ( !pPlayer->m_pActiveItem )
	{
		// player doesn't have an active item!
		return TRUE;
	}

	if ( !pPlayer->m_pActiveItem->CanHolster() )
	{
		// can't put away the active item.
		return FALSE;
	}

	if ( pPlayer->ShouldWeaponSwitch() && pWeapon->iWeight() > pPlayer->m_pActiveItem->iWeight() )
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CHalfLifeMultiplay :: GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon, BOOL dropBox, BOOL explode )
{

	CBasePlayerItem *pCheck;
	CBasePlayerItem *pBest;// this will be used in the event that we don't find a weapon in the same category.
	int iBestWeight;
	int i;

	iBestWeight = -1;// no weapon lower than -1 can be autoswitched to
	pBest = NULL;

	if ( !pCurrentWeapon->CanHolster() )
	{
		// can't put this gun away right now, so can't switch.
		return FALSE;
	}

	if (!dropBox && explode && pPlayer->ShouldWeaponThrow() && 
		((CBasePlayerWeapon *)pCurrentWeapon)->pszAmmo1() != NULL &&
		!(((CBasePlayerWeapon *)pCurrentWeapon)->iFlags() & ITEM_FLAG_LIMITINWORLD))
	{
		((CBasePlayerWeapon *)pCurrentWeapon)->ThrowWeapon(TRUE);
	}

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pCheck = pPlayer->m_rgpPlayerItems[ i ];

		while ( pCheck )
		{
			if ( pCheck->iWeight() > -1 && pCheck->iWeight() == pCurrentWeapon->iWeight() && pCheck != pCurrentWeapon )
			{
				// this weapon is from the same category. 
				if ( pCheck->CanDeploy() )
				{
					if ( pPlayer->SwitchWeapon( pCheck ) )
					{
						return TRUE;
					}
				}
			}
			else if ( pCheck->iWeight() > iBestWeight && pCheck != pCurrentWeapon )// don't reselect the weapon we're trying to get rid of
			{
				//ALERT ( at_console, "Considering %s\n", STRING( pCheck->pev->classname ) );
				// we keep updating the 'best' weapon just in case we can't find a weapon of the same weight
				// that the player was using. This will end up leaving the player with his heaviest-weighted 
				// weapon. 
				if ( pCheck->CanDeploy() )
				{
					// if this weapon is useable, flag it as the best
					iBestWeight = pCheck->iWeight();
					pBest = pCheck;
				}
			}

			pCheck = pCheck->m_pNext;
		}
	}

	// if we make it here, we've checked all the weapons and found no useable 
	// weapon in the same catagory as the current weapon. 
	
	// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always 
	// at least get the crowbar, but ya never know.
	if ( !pBest )
	{
		return FALSE;
	}

	pPlayer->SwitchWeapon( pBest );

	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] )
{
	if ( pEntity )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pEntity );
		if ( pPlayer )
		{
			pPlayer->IsInArena = FALSE;
			pPlayer->HasDisconnected = FALSE;
		}
	}

	g_VoiceGameMgr.ClientConnected(pEntity);
	return TRUE;
}

extern int gmsgSayText;
extern int gmsgGameMode;

void CHalfLifeMultiplay :: UpdateGameMode( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, NULL, pPlayer->edict() );
		WRITE_BYTE( g_GameMode );  // game mode none
	MESSAGE_END();
}

void CHalfLifeMultiplay :: InitHUD( CBasePlayer *pl )
{
	// notify other clients of player joining the game
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s has joined the game\n", 
		( pl->pev->netname && STRING(pl->pev->netname)[0] != 0 ) ? STRING(pl->pev->netname) : "unconnected" ) );

	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" entered the game\n",  
			STRING( pl->pev->netname ), 
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pl->edict() ), "model" ) );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" entered the game\n",  
			STRING( pl->pev->netname ), 
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			GETPLAYERUSERID( pl->edict() ) );
	}

	if (!FBitSet(pl->pev->flags, FL_FAKECLIENT))
	{
		UpdateGameMode( pl );

		// set or clear timer
		if (!IsRoundBased() && timelimit.value > 0)
		{
			float flTimeLimit = timelimit.value * 60;
			float time_remaining = (int)( flTimeLimit - gpGlobals->time );

			MESSAGE_BEGIN(MSG_ONE, gmsgRoundTime, NULL, pl->edict());
				WRITE_SHORT(time_remaining);
			MESSAGE_END();
		}
		else
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgShowTimer, NULL, pl->edict());
				WRITE_BYTE(0);
			MESSAGE_END();
		}

		// Update mutators
		MESSAGE_BEGIN(MSG_ONE, gmsgAddMutator, NULL, pl->edict());
			mutators_t *t = GetMutators();
			while (t != NULL)
			{
				WRITE_BYTE(t->mutatorId);
				WRITE_BYTE(t->timeToLive);
				t = t->next;
			}
		MESSAGE_END();

		if (!g_GameMode)
		{
			MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pl->edict());
				WRITE_STRING("Frag 'em");
				WRITE_STRING("");
				WRITE_BYTE(0);
			MESSAGE_END();
		}
		else if (g_GameMode == GAME_SNOWBALL)
		{
			MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL,  pl->edict());
				WRITE_STRING("Snowball 'em");
				WRITE_STRING("");
				WRITE_BYTE(0);
			MESSAGE_END();
		}
	}

	// sending just one score makes the hud scoreboard active;  otherwise
	// it is just disabled for single play
	MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
		WRITE_BYTE( ENTINDEX(pl->edict()) );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	SendMOTDToClient( pl->edict() );

	// loop through all active players and send their score info to the new client
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		// FIXME:  Probably don't need to cast this just to read m_iDeaths
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
				WRITE_BYTE( i );	// client number
				WRITE_SHORT( plr->pev->frags );
				WRITE_SHORT( plr->m_iDeaths );
				WRITE_SHORT( g_GameMode != GAME_GUNGAME ? plr->m_iRoundWins : plr->m_iRoundWins + 1 );
				WRITE_SHORT( GetTeamIndex( plr->m_szTeamName ) + 1 );
			MESSAGE_END();
		}
	}

	pl->m_iShownWelcomeMessage = gpGlobals->time + 4.0;
	pl->m_iShowMutatorMessage = gpGlobals->time + 2.0;
	pl->m_iShowGameModeMessage = gpGlobals->time + 2.0;

	if ( g_fGameOver )
	{
		MESSAGE_BEGIN( MSG_ONE, SVC_INTERMISSION, NULL, pl->edict() );
		MESSAGE_END();

		if (pl->pev->flags & FL_FAKECLIENT)
			pl->EnableControl(FALSE);
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: ClientDisconnected( edict_t *pClient )
{
	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		if ( pPlayer )
		{
			FireTargets( "game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0 );

			ResetPlayerSettings(pPlayer);

			// team match?
			if ( g_teamplay )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" disconnected\n",  
					STRING( pPlayer->pev->netname ), 
					GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ),
					g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model" ) );
			}
			else
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%i>\" disconnected\n",  
					STRING( pPlayer->pev->netname ), 
					GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ),
					GETPLAYERUSERID( pPlayer->edict() ) );
			}

			// Cold Ice Remastered Game mode stuff
			pPlayer->HasDisconnected = TRUE;
			if ( g_GameInProgress )
			{
				if ( pPlayer->IsInArena && !pPlayer->IsSpectator() )
					UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("%s has left the round!\n", STRING(pPlayer->pev->netname)));
			}
			pPlayer->IsInArena = FALSE;

			if (pPlayer->pev->flags & FL_FAKECLIENT)
			{
				pPlayer->pev->frags = 0;
				pPlayer->m_iDeaths = 0;
				pPlayer->m_iAssists = 0;
				pPlayer->m_iRoundWins = 0;
				pPlayer->pev->fuser4 = 0;
				strcpy(pPlayer->m_szTeamName, "");
				extern int gmsgTeamInfo;
				MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
					WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
					WRITE_STRING( pPlayer->m_szTeamName );
				MESSAGE_END();

				MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
					WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
					WRITE_SHORT( pPlayer->pev->frags );
					WRITE_SHORT( pPlayer->m_iDeaths );
					WRITE_SHORT( pPlayer->m_iRoundWins );
					WRITE_SHORT( GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
				MESSAGE_END();
			}

			if ( !pPlayer->IsSpectator() )
			{
				MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_TELEPORT	); 
				WRITE_COORD(pPlayer->pev->origin.x);
				WRITE_COORD(pPlayer->pev->origin.y);
				WRITE_COORD(pPlayer->pev->origin.z);
				MESSAGE_END();

				pPlayer->RemoveAllItems( TRUE );// destroy all of the players weapons and items
			}
		}
	}
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	int iFallDamage = (int)falldamage.value;

	// Mutators
	if (g_pGameRules->MutatorEnabled(MUTATOR_SUPERJUMP))
	{
		return 0;
	}

	switch ( iFallDamage )
	{
	case 1://progressive
		pPlayer->m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
		return pPlayer->m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
		break;
	default:
	case 0:// fixed
		return 10;
		break;
	}
} 

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if (pPlayer->m_fLastSpawnTime && pPlayer->m_fLastSpawnTime > gpGlobals->time)
		return FALSE;

	return TRUE;
}

//=========================================================
//=========================================================
#define TO_STRING_AUX( ... ) "" #__VA_ARGS__
#define TO_STRING( x ) TO_STRING_AUX( x )

void CHalfLifeMultiplay :: PlayerThink( CBasePlayer *pPlayer )
{
	if (g_pGameRules->MutatorEnabled(MUTATOR_LIGHTSOUT))
	{
		// Everready
		if (pPlayer->IsAlive())
			pPlayer->m_iFlashBattery = 100;
	}

	if (pPlayer->m_fHasRune == RUNE_VAMPIRE && pPlayer->m_fVampireHealth > 0)
	{
		//under limit, increase damage / 2.
		if ( pPlayer->pev->health < pPlayer->pev->max_health )
			pPlayer->pev->health += pPlayer->m_fVampireHealth;

		//over the limit, go back to max.
		if ( pPlayer->pev->health > pPlayer->pev->max_health )
			pPlayer->pev->health = pPlayer->pev->max_health;

		UTIL_ScreenFade(pPlayer, Vector(200, 0, 0), .5, .5, 32, FFADE_IN);

		pPlayer->m_fVampireHealth = 0;
	}

	if ( pPlayer->m_fHasRune == RUNE_REGEN )
	{
		if ( pPlayer->m_flRuneHealTime < gpGlobals->time )
		{
			if (pPlayer->pev->health < pPlayer->pev->max_health) {
				pPlayer->pev->health += 1;
				UTIL_ScreenFade( pPlayer, Vector(200,0,200), .5, .5, 32, FFADE_IN);
				pPlayer->m_flRuneHealTime = gpGlobals->time + 1.0;
			} else if (pPlayer->pev->armorvalue < pPlayer->pev->max_health) {
				pPlayer->pev->armorvalue += 1;
				UTIL_ScreenFade( pPlayer, Vector(200,0,200), .5, .5, 32, FFADE_IN);
				pPlayer->m_flRuneHealTime = gpGlobals->time + 1.0;
			}
		}
	}
	else if ( pPlayer->m_fHasRune == RUNE_AMMO )
	{
		if ( pPlayer->m_flRuneHealTime < gpGlobals->time )
		{
			if (pPlayer->m_pActiveItem)
			{
				CBasePlayerWeapon *pWeapon = (CBasePlayerWeapon*)pPlayer->m_pActiveItem;

				if (pPlayer->m_rgAmmo[pWeapon->m_iPrimaryAmmoType] < pWeapon->iMaxAmmo1()) {
					pPlayer->m_rgAmmo[pWeapon->m_iPrimaryAmmoType] += 1;
					UTIL_ScreenFade( pPlayer, Vector(200,200,0), .5, .5, 32, FFADE_IN);
					pPlayer->m_flRuneHealTime = gpGlobals->time + 1.0;
				} else if (pPlayer->m_rgAmmo[pWeapon->m_iSecondaryAmmoType] < pWeapon->iMaxAmmo2()) {
					pPlayer->m_rgAmmo[pWeapon->m_iSecondaryAmmoType] += 1;
					UTIL_ScreenFade( pPlayer, Vector(200,200,0), .5, .5, 32, FFADE_IN);
					pPlayer->m_flRuneHealTime = gpGlobals->time + 1.0;
				}
			}
		}
	}

	if ( g_fGameOver )
	{
		// check for button presses
		if ( pPlayer->m_afButtonPressed & ( IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP ) )
			m_iEndIntermissionButtonHit = TRUE;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed = 0;
		pPlayer->pev->button = 0;
		pPlayer->m_afButtonReleased = 0;
	}

	if (pPlayer->m_iShownWelcomeMessage != -1 && pPlayer->m_iShownWelcomeMessage < gpGlobals->time) {
#ifdef GIT
		ClientPrint(pPlayer->pev, HUD_PRINTTALK, "Welcome to Cold Ice Remastered Beta 5 (%s). For commands, type \"help\" in the console.\n", TO_STRING(GIT));
#else
		ClientPrint(pPlayer->pev, HUD_PRINTTALK, "Welcome to Cold Ice Remastered Beta 5. For commands, type \"help\" in the console.\n");
#endif

		// Play music
		CBaseEntity *pT = UTIL_FindEntityByClassname( NULL, "trigger_mp3audio");
		if ( pT && pT->edict() && pPlayer->m_iPlayMusic )
		{
			pT->Use(pPlayer, pPlayer, USE_ON, 0);
		}

		pPlayer->m_iShownWelcomeMessage = -1;
	}

	if (pPlayer->m_fLastSpawnTime && pPlayer->m_fLastSpawnTime <= gpGlobals->time)
	{
		if (!pPlayer->IsObserver())
		{
			if (!MutatorEnabled(MUTATOR_GODMODE))
				pPlayer->pev->flags &= ~FL_GODMODE;
			pPlayer->pev->rendermode = kRenderNormal;
			pPlayer->pev->renderfx = kRenderFxNone;
			pPlayer->pev->renderamt = 0;
			pPlayer->m_fLastSpawnTime = 0;

			if (MutatorEnabled(MUTATOR_INVISIBLE))
				pPlayer->MakeInvisible();
		}
	}

	if (pPlayer->m_fEffectTime && pPlayer->m_fEffectTime <= gpGlobals->time)
	{
		pPlayer->pev->rendermode = kRenderTransAdd;
		pPlayer->pev->renderfx = kRenderFxStrobeFaster;
		pPlayer->pev->renderamt = 125;
		pPlayer->m_fEffectTime = 0;
	}

	g_pGameRules->UpdateMutatorMessage(pPlayer);
	g_pGameRules->UpdateGameModeMessage(pPlayer);

#ifdef _DEBUG
	if (debug_disconnects.value && g_iKickSomeone < gpGlobals->time) {

		int bot_clients = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( plr && plr->pev->flags & FL_FAKECLIENT )
			{
				bot_clients++;
			}
		}

		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( RANDOM_LONG(1, bot_clients) );

		if (RANDOM_LONG(0,1) == 1)
		{
			if ( plr && plr->pev->flags & FL_FAKECLIENT )
			{
				if (RANDOM_LONG(0,9) == 0)
				{
					SERVER_COMMAND("kickall\n");
				}
				else
				{
					char cmd[80];
					strcpy(cmd, "");
					if (RANDOM_LONG(0,1))
					{
						sprintf(cmd, "kick \"%s\"\n", STRING(plr->pev->netname));
						SERVER_COMMAND(cmd);
						ALERT(at_aiconsole, "rotate client: %s", cmd);
					}
					else
					{
						ClearMultiDamage();
						plr->pev->health = 0;
						plr->Killed( plr->pev, GIB_NEVER );
					}
				}
			}
		}
		else
		{
			ALERT(at_aiconsole, "adding bot...\n");
			SERVER_COMMAND("addbot\n");
		}

		g_iKickSomeone = gpGlobals->time + debug_disconnects_time.value;
	}
#endif
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerSpawn( CBasePlayer *pPlayer )
{
	// Place player in spectator mode if joining during a game
	// Or if the game begins that requires spectators
	if ((g_GameInProgress && !pPlayer->IsInArena) || (!g_GameInProgress && IsRoundBased()))
	{
		return;
	}

	// Pause player during voting
	if (m_iVoteUnderway)
		pPlayer->EnableControl(FALSE);

	BOOL		addDefault;
	CBaseEntity	*pWeaponEntity = NULL;

	pPlayer->pev->weapons |= (1<<WEAPON_SUIT);
	
	addDefault = TRUE;

	pPlayer->GiveNamedItem("weapon_fists");

	while ( pWeaponEntity = UTIL_FindEntityByClassname( pWeaponEntity, "game_player_equip" ))
	{
		pWeaponEntity->Touch( pPlayer );
		addDefault = FALSE;
	}

	if (startwithall.value) {
		pPlayer->CheatImpulseCommands(101, FALSE);
	}
	else if ( addDefault )
	{
		if (snowballfight.value)
		{
			pPlayer->GiveNamedItem("weapon_snowball");
		}
		else
		{
			// Give a random melee
			char *meleeWeapon = "weapon_crowbar";
			if (g_GameMode == GAME_FFA) {
				int whichWeapon = RANDOM_LONG(0,3);
				if (!whichWeapon) {
					meleeWeapon = "weapon_chainsaw";
				} else if (whichWeapon == 1) {
					meleeWeapon = "weapon_knife";
				} else if (whichWeapon == 2 && !g_pGameRules->MutatorEnabled(MUTATOR_PLUMBER)) {
					meleeWeapon = "weapon_wrench";
				}
			}
			if (!pPlayer->HasNamedPlayerItem(meleeWeapon))
				pPlayer->GiveNamedItem(STRING(ALLOC_STRING(meleeWeapon)));
		}

		char *pWeaponName;
		char list[1024];
		strcpy(list, spawnweaponlist.string);
		pWeaponName = list;
		pWeaponName = strtok( pWeaponName, ";" );
		while ( pWeaponName != NULL && *pWeaponName )
		{
			const char *newWeapon = STRING(ALLOC_STRING(pWeaponName));
			if (!pPlayer->HasNamedPlayerItem(newWeapon))
				pPlayer->GiveNamedItem(newWeapon);
			pWeaponName = strtok( NULL, ";" );
		}
		pPlayer->GiveAmmo( 68, "9mm", _9MM_MAX_CARRY );// 4 full reloads
	}

	g_pGameRules->SpawnMutators(pPlayer);

	if (spawnprotectiontime.value > 0)
	{
		pPlayer->pev->flags |= FL_GODMODE;
		// pPlayer->pev->solid = SOLID_NOT;
		pPlayer->m_fEffectTime = gpGlobals->time + 0.25;
	}
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	return TRUE;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerSpawnTime( CBasePlayer *pPlayer )
{
	return gpGlobals->time;//now!
}

BOOL CHalfLifeMultiplay :: AllowAutoTargetCrosshair( void )
{
	return ( aimcrosshair.value != 0 );
}

BOOL CHalfLifeMultiplay::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
{
	// players, only
	CBaseEntity *pTgt = CBaseEntity::Instance( target );
	if ( pTgt && pTgt->IsPlayer() )
		return TRUE;

	return FALSE;
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
int CHalfLifeMultiplay :: IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	if ( pAttacker->m_fHasRune == RUNE_FRAG )
		return 2;
	else
		return 1;
}


//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
#define	HITGROUP_HEAD 1

void CHalfLifeMultiplay :: PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	DeathNotice( pVictim, pKiller, pInflictor );

	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );

	pVictim->m_iDeaths += 1;


	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );

	if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayer*)ktmp;
	else if ( ktmp && ktmp->Classify() == CLASS_VEHICLE )
	{
		CBasePlayer *pDriver = (CBasePlayer *)( (CFuncVehicle *)ktmp )->m_pDriver;

		if ( pDriver != NULL )
		{
			pKiller = pDriver->pev;
			ktmp = peKiller = (CBasePlayer *)pDriver;
		}
	}

	if ( pVictim->pev == pKiller )  
	{  // killed self
		int fragsToRemove = 1;
		if ((pInflictor && FClassnameIs(pInflictor, "weapon_vest")) || g_GameMode == GAME_GUNGAME)
			fragsToRemove = 0;
		pKiller->frags -= fragsToRemove;
	}
	else if ( ktmp && ktmp->IsPlayer() )
	{
		// if a player dies in a deathmatch game and the killer is a client, award the killer some points
		pKiller->frags += IPointsForKill( peKiller, pVictim );
		if (peKiller->m_iAssists && (peKiller->m_iAssists % 3 == 0))
			pKiller->frags += IPointsForKill( peKiller, pVictim );

		if (!UTIL_GetAlivePlayersInSphere(peKiller, 1024) &&
			peKiller->m_iAutoTaunt)
			peKiller->m_fTauntTime = gpGlobals->time + 0.75;

		if (!m_iFirstBloodDecided && PlayerRelationship( pVictim, peKiller ) != GR_TEAMMATE)
		{
			UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("%s achieves first blood!\n", STRING(pKiller->netname) ));
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_FIRSTBLOOD);
			MESSAGE_END();
			pKiller->frags += IPointsForKill( peKiller, pVictim );
			m_iFirstBloodDecided = TRUE;
		}
		else if (pVictim->m_LastHitGroup == HITGROUP_HEAD)
		{
			if (!FBitSet(pKiller->flags, FL_FAKECLIENT))
			{
				MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, pKiller );
					WRITE_BYTE(CLIENT_SOUND_HEADSHOT);
				MESSAGE_END();
			}
			pKiller->health += 5;
		}

		if (g_pGameRules->MutatorEnabled(MUTATOR_LOOPBACK))
		{
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_TELEPORT	); 
				WRITE_COORD(pKiller->origin.x);
				WRITE_COORD(pKiller->origin.y);
				WRITE_COORD(pKiller->origin.z);
			MESSAGE_END();
			UTIL_ScreenFade(peKiller, Vector(200,200,200), .15, .15, 200, FFADE_IN);

			pKiller->origin = pVictim->pev->origin;

			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_TELEPORT	); 
				WRITE_COORD(pKiller->origin.x);
				WRITE_COORD(pKiller->origin.y);
				WRITE_COORD(pKiller->origin.z);
			MESSAGE_END();
		}

		FireTargets( "game_playerkill", ktmp, ktmp, USE_TOGGLE, 0 );
	}
	else
	{  // killed by the world
		if (g_GameMode != GAME_GUNGAME)
			pKiller->frags -= 1;
	}

	// update the scores
	// killed scores
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );
		WRITE_SHORT( pVictim->pev->frags );
		WRITE_SHORT( pVictim->m_iDeaths );
		WRITE_SHORT( pVictim->m_iRoundWins );
		WRITE_SHORT( GetTeamIndex( pVictim->m_szTeamName ) + 1 );
	MESSAGE_END();

	// killers score, if it's a player
	CBaseEntity *ep = CBaseEntity::Instance( pKiller );
	if ( ep && ep->Classify() == CLASS_PLAYER )
	{
		CBasePlayer *PK = (CBasePlayer*)ep;

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(PK->edict()) );
			WRITE_SHORT( PK->pev->frags );
			WRITE_SHORT( PK->m_iDeaths );
			WRITE_SHORT( PK->m_iRoundWins );
			WRITE_SHORT( GetTeamIndex( PK->m_szTeamName) + 1 );
		MESSAGE_END();

		// let the killer paint another decal as soon as he'd like.
		PK->m_flNextDecalTime = gpGlobals->time;
	}
#ifndef HLDEMO_BUILD
	if ( pVictim->HasNamedPlayerItem("weapon_satchel") )
	{
		DeactivateSatchels( pVictim );
		if (g_pGameRules->FAllowMonsters())
			DeactivateAssassins( pVictim );
	}
	DeactivatePortals( pVictim );
#endif

	if (m_iVolatile ||
		(pVictim->m_pActiveItem && pVictim->m_pActiveItem->m_iId == WEAPON_VEST)) {
		// No echo boom
		if (pInflictor && FClassnameIs(pInflictor, "weapon_vest"))
			return;
		CGrenade::Vest( pVictim->pev, pVictim->pev->origin, gSkillData.plrDmgVest );
		pVictim->pev->solid = SOLID_NOT;
		pVictim->GibMonster();
		pVictim->pev->effects |= EF_NODRAW;
	}

	if (m_iNotTheBees)
	{
		Vector angles;
		BOOL isOtherPlayer = peKiller && pKiller != pVictim->pev;
		if (isOtherPlayer) 
			angles = UTIL_VecToAngles((pKiller->origin + pKiller->view_ofs ) - pVictim->pev->origin);

		CBaseEntity *hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16, isOtherPlayer ? angles : gpGlobals->v_up, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isOtherPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				hornet->pev->velocity.z += 50;
			}
			else
				hornet->pev->velocity = gpGlobals->v_up * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * 8, isOtherPlayer ? angles : gpGlobals->v_up, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isOtherPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				hornet->pev->velocity.z += 50;
			}
			else
				hornet->pev->velocity = gpGlobals->v_up * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16 + gpGlobals->v_forward * 24, isOtherPlayer ? angles : gpGlobals->v_forward, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isOtherPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				hornet->pev->velocity.z += 50;
			}
			else
				hornet->pev->velocity = gpGlobals->v_forward * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16 + gpGlobals->v_forward * -24, isOtherPlayer ? angles : gpGlobals->v_forward * -1, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isOtherPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				hornet->pev->velocity.z += 50;
			}
			else
				hornet->pev->velocity = (gpGlobals->v_forward * -1) * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16 + gpGlobals->v_right * 24, isOtherPlayer ? angles : gpGlobals->v_right, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isOtherPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				hornet->pev->velocity.z += 50;
			}
			else
				hornet->pev->velocity = gpGlobals->v_right * RANDOM_LONG(100, 200);
		}

		hornet = CBaseEntity::Create( "hornet", pVictim->GetGunPosition( ) + gpGlobals->v_up * -16 + gpGlobals->v_right * -24, isOtherPlayer ? angles : gpGlobals->v_right * -1, pVictim->edict() );
		if (hornet != NULL)
		{
			if (isOtherPlayer)
			{
				hornet->pev->velocity = ( ( pKiller->origin + pKiller->view_ofs ) - hornet->pev->origin ).Normalize() * RANDOM_LONG(100, 200);
				hornet->pev->velocity.z += 50;
			}
			else
				hornet->pev->velocity = (gpGlobals->v_right * -1) * RANDOM_LONG(100, 200);
		}
	}
}

//=========================================================
// Deathnotice. 
//=========================================================
void CHalfLifeMultiplay::DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor )
{
	// Work out what killed the player, and send a message to all clients about it
	CBaseEntity *Killer = CBaseEntity::Instance( pKiller );

	const char *killer_weapon_name = "world";		// by default, the player is killed by the world
	int killer_index = 0;
	int assist_index = 0;
	
	// Hack to fix name change
	char *tau = "tau_cannon";
	char *gluon = "gluon gun";

	// For assassin kills
	if (pevInflictor != NULL 
		&& strncmp(STRING( pevInflictor->classname ), "monster_human_assassin", 22) == 0)
	{
		if (VARS(Killer->pev->owner))
			pKiller = VARS(Killer->pev->owner);
	}

	if ( pKiller->flags & FL_CLIENT )
	{
		killer_index = ENTINDEX(ENT(pKiller));
		if (pVictim->pLastAssist)
		{
			assist_index = pVictim->pLastAssist->entindex();
			if (assist_index != killer_index)
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(assist_index);
				if (plr)
					plr->m_iAssists += 1;
				else
					assist_index = -1;
			}
			pVictim->pLastAssist = NULL;
		}
		
		if ( pevInflictor )
		{
			if ( pevInflictor == pKiller )
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				CBasePlayer *pPlayer = (CBasePlayer*)CBaseEntity::Instance( pKiller );
				
				if ( pPlayer->m_pActiveItem )
				{
					killer_weapon_name = pPlayer->m_pActiveItem->pszName();
				}
			}
			else
			{
				killer_weapon_name = STRING( pevInflictor->classname );  // it's just that easy
			}
		}
	}
	else
	{
		if (pevInflictor)
			killer_weapon_name = STRING( pevInflictor->classname );
	}

	if (gMultiDamage.pEntity == pVictim &&
		(gMultiDamage.type & DMG_KICK) &&
		(pKiller->flags & FL_CLIENT) && 
		gMultiDamage.time > gpGlobals->time - 0.5)
	{
		killer_weapon_name = "kick";
	}

	if (gMultiDamage.pEntity == pVictim &&
		(gMultiDamage.type & DMG_PUNCH) &&
		(pKiller->flags & FL_CLIENT) && 
		gMultiDamage.time > gpGlobals->time - 0.5)
	{
		killer_weapon_name = "fists";
	}

	// Tracer weapon id
	if (pevInflictor && pevInflictor->noise3 > 0)
	{
		killer_weapon_name = STRING(pevInflictor->noise3);
		pevInflictor->noise3 = 0;
	}

	// strip the monster_* or weapon_* from the inflictor's classname
	if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
		killer_weapon_name += 7;
	else if ( strncmp( killer_weapon_name, "monster_", 8 ) == 0 )
		killer_weapon_name += 8;
	else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		killer_weapon_name += 5;
	else if ( strncmp( killer_weapon_name, "player", 6 ) == 0 )
		killer_weapon_name = "flamethrower";

	if ( strncmp( killer_weapon_name, "human_assassin", 14 ) == 0 )
		killer_weapon_name = "vest";

	MESSAGE_BEGIN( MSG_ALL, gmsgDeathMsg );
		WRITE_BYTE( killer_index );						// the killer
		WRITE_BYTE( assist_index != killer_index ? assist_index : -1 );						// the assist
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );		// the victim
		WRITE_STRING( killer_weapon_name );		// what they were killed by (should this be a string?)
		WRITE_BYTE( (killer_index && pVictim->m_LastHitGroup == HITGROUP_HEAD) ? 1 : -1);
	MESSAGE_END();

	// replace the code names with the 'real' names
	if ( !strcmp( killer_weapon_name, "egon" ) )
		killer_weapon_name = gluon;
	else if ( !strcmp( killer_weapon_name, "gauss" ) )
		killer_weapon_name = tau;

	if ( pVictim->pev == pKiller )  
	{
		// killed self

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",  
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\"\n",  
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",  
				STRING( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( ENT(pKiller) ), "model" ),
				STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" killed \"%s<%i><%s><%i>\" with \"%s\"\n",  
				STRING( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				GETPLAYERUSERID( ENT(pKiller) ),
				STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );
		}
	}
	else
	{ 
		// killed by the world

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\" (world)\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\" (world)\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}

	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE ( 9 );	// command length in bytes
		WRITE_BYTE ( DRC_CMD_EVENT );	// player killed
		WRITE_SHORT( ENTINDEX(pVictim->edict()) );	// index number of primary entity
		if (pevInflictor)
			WRITE_SHORT( ENTINDEX(ENT(pevInflictor)) );	// index number of secondary entity
		else
			WRITE_SHORT( ENTINDEX(ENT(pKiller)) );	// index number of secondary entity
		WRITE_LONG( 7 | DRC_FLAG_DRAMATIC);   // eventflags (priority and flags)
	MESSAGE_END();

//  Print a standard message
	// TODO: make this go direct to console
	return; // just remove for now
/*
	char	szText[ 128 ];

	if ( pKiller->flags & FL_MONSTER )
	{
		// killed by a monster
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was killed by a monster.\n" );
		return;
	}

	if ( pKiller == pVictim->pev )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " commited suicide.\n" );
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		strcpy ( szText, STRING( pKiller->netname ) );

		strcat( szText, " : " );
		strcat( szText, killer_weapon_name );
		strcat( szText, " : " );

		strcat ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, "\n" );
	}
	else if ( FClassnameIs ( pKiller, "worldspawn" ) )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " fell or drowned or something.\n" );
	}
	else if ( pKiller->solid == SOLID_BSP )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was mooshed.\n" );
	}
	else
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " died mysteriously.\n" );
	}

	UTIL_ClientPrintAll( szText );
*/
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
void CHalfLifeMultiplay :: PlayerGotWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHalfLifeMultiplay :: FlWeaponRespawnTime( CBasePlayerItem *pWeapon )
{
	if ( weaponstay.value > 0 )
	{
		// make sure it's only certain weapons
		if ( !(pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
		{
			return gpGlobals->time + 0;		// weapon respawns almost instantly
		}
	}

	return gpGlobals->time + WEAPON_RESPAWN_TIME;
}

// when we are within this close to running out of entities,  items 
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE	100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHalfLifeMultiplay :: FlWeaponTryRespawn( CBasePlayerItem *pWeapon )
{
	if ( pWeapon && (pWeapon->m_iId > 0 && pWeapon->m_iId < MAX_WEAPONS) && (pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
	{
		if ( NUMBER_OF_ENTITIES() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE) )
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime( pWeapon );
	}

	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay :: VecWeaponRespawnSpot( CBasePlayerItem *pWeapon )
{
	return pWeapon->pev->origin;
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHalfLifeMultiplay :: WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	if ( pWeapon->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_WEAPON_RESPAWN_NO;
	}

	return GR_WEAPON_RESPAWN_YES;
}

//=========================================================
// CanHaveWeapon - returns FALSE if the player is not allowed
// to pick up this weapon
//=========================================================
BOOL CHalfLifeMultiplay::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if ( weaponstay.value > 0 )
	{
		if ( pItem->iFlags() & ITEM_FLAG_LIMITINWORLD )
			return CGameRules::CanHavePlayerItem( pPlayer, pItem );

		// check if the player already has this weapon
		for ( int i = 0 ; i < MAX_ITEM_TYPES ; i++ )
		{
			CBasePlayerItem *it = pPlayer->m_rgpPlayerItems[i];

			while ( it != NULL )
			{
				if ( it->m_iId == pItem->m_iId )
				{
					return FALSE;
				}

				it = it->m_pNext;
			}
		}
	}

	return CGameRules::CanHavePlayerItem( pPlayer, pItem );
}

BOOL CHalfLifeMultiplay::CanHavePlayerAmmo( CBasePlayer *pPlayer, CBasePlayerAmmo *pAmmo )
{
	return CGameRules::CanHavePlayerAmmo( pPlayer, pAmmo );
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem )
{
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::ItemShouldRespawn( CItem *pItem )
{
	if ( pItem->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_ITEM_RESPAWN_NO;
	}

	return GR_ITEM_RESPAWN_YES;
}


//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHalfLifeMultiplay::FlItemRespawnTime( CItem *pItem )
{
	return gpGlobals->time + ITEM_RESPAWN_TIME;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay::VecItemRespawnSpot( CItem *pItem )
{
	return pItem->pev->origin;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotAmmo( CBasePlayer *pPlayer, char *szName, int iCount )
{
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsAllowedToSpawn( CBaseEntity *pEntity )
{
//	if ( pEntity->pev->flags & FL_MONSTER )
//		return FALSE;

	if (g_GameInProgress == TRUE && pEntity->IsPlayer()) {
		return FALSE;
	}

	if (!spawnweapons.value &&
		(strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0 || strncmp(STRING(pEntity->pev->classname), "ammo_", 5) == 0))
	{
		return FALSE;
	}

	if (!spawnitems.value &&
		strncmp(STRING(pEntity->pev->classname), "item_", 5) == 0)
	{
		return FALSE;
	}

	if (disallowlist.string && strstr(disallowlist.string, STRING(pEntity->pev->classname))) {
		ALERT(at_aiconsole, "%s has been disallowed on the server.\n", STRING(pEntity->pev->classname));
		return FALSE;
	}

/*
	if (g_pGameRules->MutatorEnabled(MUTATOR_INSTAGIB) &&
		(strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0 || strncmp(STRING(pEntity->pev->classname), "ammo_", 5) == 0))
	{	
		if (stricmp(STRING(pEntity->pev->classname), "weapon_railgun") == 0 ||
			stricmp(STRING(pEntity->pev->classname), "weapon_dual_railgun") == 0 ||
			stricmp(STRING(pEntity->pev->classname), "weapon_fists") == 0) {
			return TRUE;
		}

		if (stricmp(STRING(pEntity->pev->classname), "ammo_gaussclip") != 0)
		{
			CBaseEntity::Create("ammo_gaussclip", pEntity->pev->origin, pEntity->pev->angles, pEntity->pev->owner);
			return FALSE;
		}
	}
*/

/*
	if (g_pGameRules->MutatorEnabled(MUTATOR_PLUMBER) &&
		(strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0 || strncmp(STRING(pEntity->pev->classname), "ammo_", 5) == 0))
	{	
		if (stricmp(STRING(pEntity->pev->classname), "weapon_fists") == 0 ||
			stricmp(STRING(pEntity->pev->classname), "weapon_wrench") == 0 ||
			stricmp(STRING(pEntity->pev->classname), "weapon_dual_wrench") == 0) {
			return TRUE;
		}

		if (stricmp(STRING(pEntity->pev->classname), "weapon_wrench") != 0)
		{
			CBaseEntity::Create("weapon_wrench", pEntity->pev->origin, pEntity->pev->angles, pEntity->pev->owner);
			return FALSE;
		}
	}
*/

	const char* dualWeaponList[] = {
		"weapon_dual_wrench",
		"weapon_dual_glock",
		"weapon_dual_deagle",
		"weapon_dual_mag60",
		"weapon_dual_smg",
		"weapon_dual_usas",
		"weapon_dual_rpg",
		"weapon_dual_flamethrower"
	};

	if (dualsonly.value && strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0)
	{
		// Allow addition weapons that are thrown
		if (strncmp(STRING(pEntity->pev->classname), "weapon_fists", 12) == 0 ||
			strncmp(STRING(pEntity->pev->classname), "weapon_wrench", 12) == 0 ||
			strncmp(STRING(pEntity->pev->classname), "weapon_crowbar", 14) == 0 ||
			strncmp(STRING(pEntity->pev->classname), "weapon_knife", 12) == 0)
			return TRUE;

		if (strncmp(STRING(pEntity->pev->classname), "weapon_dual_", 12) != 0)
		{
			CBaseEntity::Create((char *)dualWeaponList[RANDOM_LONG(0,ARRAYSIZE(dualWeaponList)-1)], pEntity->pev->origin, pEntity->pev->angles, pEntity->pev->owner);
			return FALSE;
		}

		return TRUE;
	}

	if (snowballfight.value && 
		(strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0 ||
		strncmp(STRING(pEntity->pev->classname), "ammo_", 5) == 0))
	{
		if (!stricmp(STRING(pEntity->pev->classname), "weapon_fists")) {
			return TRUE;
		}

		if (strncmp(STRING(pEntity->pev->classname), "weapon_snowball", 15) != 0)
		{
			CBaseEntity::Create("weapon_snowball", pEntity->pev->origin, pEntity->pev->angles, pEntity->pev->owner);
			return FALSE;
		}
	}

	return TRUE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::AmmoShouldRespawn( CBasePlayerAmmo *pAmmo )
{
	if ( pAmmo->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_AMMO_RESPAWN_NO;
	}

	return GR_AMMO_RESPAWN_YES;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlAmmoRespawnTime( CBasePlayerAmmo *pAmmo )
{
	return gpGlobals->time + AMMO_RESPAWN_TIME;
}

//=========================================================
//=========================================================
Vector CHalfLifeMultiplay::VecAmmoRespawnSpot( CBasePlayerAmmo *pAmmo )
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlHealthChargerRechargeTime( void )
{
	return 60;
}


float CHalfLifeMultiplay::FlHEVChargerRechargeTime( void )
{
	return 30;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	if (g_pGameRules->MutatorEnabled(MUTATOR_MAXPACK))
		return GR_PLR_DROP_GUN_ALL;
	else
		return GR_PLR_DROP_GUN_ACTIVE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	if (g_pGameRules->MutatorEnabled(MUTATOR_MAXPACK))
		return GR_PLR_DROP_AMMO_ALL;
	else
		return GR_PLR_DROP_AMMO_ACTIVE;
}

BOOL CHalfLifeMultiplay::IsAllowedSingleWeapon( CBaseEntity *pEntity )
{
	return TRUE;
}

BOOL CHalfLifeMultiplay::IsAllowedToDropWeapon( CBasePlayer *pPlayer )
{
	return TRUE;
}

BOOL CHalfLifeMultiplay::IsAllowedToHolsterWeapon( void )
{
	return holsterweapons.value;
}

edict_t *CHalfLifeMultiplay::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	edict_t *pentSpawnSpot = CGameRules::GetPlayerSpawnSpot( pPlayer );	
	if ( IsMultiplayer() && pentSpawnSpot->v.target )
	{
		FireTargets( STRING(pentSpawnSpot->v.target), pPlayer, pPlayer, USE_TOGGLE, 0 );
	}

	return pentSpawnSpot;
}


//=========================================================
//=========================================================
int CHalfLifeMultiplay::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	// half life deathmatch has only enemies
	return GR_NOTTEAMMATE;
}

BOOL CHalfLifeMultiplay :: PlayFootstepSounds( CBasePlayer *pl, float fvol )
{
	if ( g_footsteps && g_footsteps->value == 0 )
		return FALSE;

	if ( pl->IsOnLadder() || pl->pev->velocity.Length2D() > 220 )
		return TRUE;  // only make step sounds in multiplayer if the player is moving fast enough

	return FALSE;
}

BOOL CHalfLifeMultiplay :: FAllowFlashlight( void ) 
{ 
	return flashlight.value != 0; 
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FAllowMonsters( void )
{
	return ( allowmonsters.value != 0 );
}

//=========================================================
//=========================================================
#if defined( GRAPPLING_HOOK )
BOOL CHalfLifeMultiplay :: AllowGrapplingHook( CBasePlayer *pPlayer )
{
	return ( grapplinghook.value );
}
# endif

//=========================================================
//======== CHalfLifeMultiplay private functions ===========
#define INTERMISSION_TIME		6

void CHalfLifeMultiplay :: GoToIntermission( void )
{
	if ( g_fGameOver )
		return;  // intermission has already been triggered, so ignore.

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr )
		{
			if ( plr->pev->flags & FL_FAKECLIENT )
				plr->EnableControl(FALSE);
			
			plr->m_fHasRune = 0;
			ResetPlayerSettings(plr);
		}
	}

	// bounds check
	int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
	if ( time < 1 )
		CVAR_SET_STRING( "mp_chattime", "1" );
	else if ( time > MAX_INTERMISSION_TIME )
		CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

	m_flIntermissionEndTime = gpGlobals->time + ( (int)mp_chattime.value );
	g_flIntermissionStartTime = gpGlobals->time;

	if (voting.value >= 10)
	{
		m_flIntermissionEndTime += (voting.value * 3) + 12; 
		m_iVoteUnderway = 1;
	}

	// Clear previous message at intermission
	UTIL_ClientPrintAll(HUD_PRINTCENTER, "");

	g_fGameOver = TRUE;
	m_iEndIntermissionButtonHit = FALSE;
}

#define MAX_RULE_BUFFER 1024

typedef struct mapcycle_item_s
{
	struct mapcycle_item_s *next;

	char mapname[ 32 ];
	int  minplayers, maxplayers;
	char rulebuffer[ MAX_RULE_BUFFER ];
} mapcycle_item_t;

typedef struct mapcycle_s
{
	struct mapcycle_item_s *items;
	struct mapcycle_item_s *next_item;
} mapcycle_t;

/*
==============
DestroyMapCycle

Clean up memory used by mapcycle when switching it
==============
*/
void DestroyMapCycle( mapcycle_t *cycle )
{
	mapcycle_item_t *p, *n, *start;
	p = cycle->items;
	if ( p )
	{
		start = p;
		p = p->next;
		while ( p != start )
		{
			n = p->next;
			delete p;
			p = n;
		}
		
		delete cycle->items;
	}
	cycle->items = NULL;
	cycle->next_item = NULL;
}

static char com_token[ 1500 ];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int             c;
	int             len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
			break;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}

/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
int COM_TokenWaiting( char *buffer )
{
	char *p;

	p = buffer;
	while ( *p && *p!='\n')
	{
		if ( !isspace( *p ) || isalnum( *p ) )
			return 1;

		p++;
	}

	return 0;
}



/*
==============
ReloadMapCycleFile


Parses mapcycle.txt file into mapcycle_t structure
==============
*/
int ReloadMapCycleFile( char *filename, mapcycle_t *cycle )
{
	char szBuffer[ MAX_RULE_BUFFER ];
	char szMap[ 32 ];
	int length;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE_FOR_ME( filename, &length );
	int hasbuffer;
	mapcycle_item_s *item, *newlist = NULL, *next;

	if ( pFileList && length )
	{
		// the first map name in the file becomes the default
		while ( 1 )
		{
			hasbuffer = 0;
			memset( szBuffer, 0, MAX_RULE_BUFFER );

			pFileList = COM_Parse( pFileList );
			if ( strlen( com_token ) <= 0 )
				break;

			strcpy( szMap, com_token );

			// Any more tokens on this line?
			if ( COM_TokenWaiting( pFileList ) )
			{
				pFileList = COM_Parse( pFileList );
				if ( strlen( com_token ) > 0 )
				{
					hasbuffer = 1;
					strcpy( szBuffer, com_token );
				}
			}

			// Check map
			if ( IS_MAP_VALID( szMap ) )
			{
				// Create entry
				char *s;

				item = new mapcycle_item_s;

				strcpy( item->mapname, szMap );

				item->minplayers = 0;
				item->maxplayers = 0;

				memset( item->rulebuffer, 0, MAX_RULE_BUFFER );

				if ( hasbuffer )
				{
					s = g_engfuncs.pfnInfoKeyValue( szBuffer, "minplayers" );
					if ( s && s[0] )
					{
						item->minplayers = atoi( s );
						item->minplayers = fmax( item->minplayers, 0 );
						item->minplayers = fmin( item->minplayers, gpGlobals->maxClients );
					}
					s = g_engfuncs.pfnInfoKeyValue( szBuffer, "maxplayers" );
					if ( s && s[0] )
					{
						item->maxplayers = atoi( s );
						item->maxplayers = fmax( item->maxplayers, 0 );
						item->maxplayers = fmin( item->maxplayers, gpGlobals->maxClients );
					}

					// Remove keys
					//
					g_engfuncs.pfnInfo_RemoveKey( szBuffer, "minplayers" );
					g_engfuncs.pfnInfo_RemoveKey( szBuffer, "maxplayers" );

					strcpy( item->rulebuffer, szBuffer );
				}

				item->next = cycle->items;
				cycle->items = item;
			}
			else
			{
				ALERT( at_console, "Skipping %s from mapcycle, not a valid map\n", szMap );
			}

		}

		FREE_FILE( aFileList );
	}

	// Fixup circular list pointer
	item = cycle->items;

	// Reverse it to get original order
	while ( item )
	{
		next = item->next;
		item->next = newlist;
		newlist = item;
		item = next;
	}
	cycle->items = newlist;
	item = cycle->items;

	// Didn't parse anything
	if ( !item )
	{
		return 0;
	}

	while ( item->next )
	{
		item = item->next;
	}
	item->next = cycle->items;
	
	cycle->next_item = item->next;

	return 1;
}

/*
==============
CountPlayers

Determine the current # of active players on the server for map cycling logic
==============
*/
int CountPlayers( void )
{
	int	num = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

		if ( pEnt )
		{
			num = num + 1;
		}
	}

	return num;
}

/*
==============
ExtractCommandString

Parse commands/key value pairs to issue right after map xxx command is issued on server
 level transition
==============
*/
void ExtractCommandString( char *s, char *szCommand )
{
	// Now make rules happen
	char	pkey[512];
	char	value[512];	// use two buffers so compares
								// work without stomping on each other
	char	*o;
	
	if ( *s == '\\' )
		s++;

	while (1)
	{
		o = pkey;
		while ( *s != '\\' )
		{
			if ( !*s )
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		strcat( szCommand, pkey );
		if ( strlen( value ) > 0 )
		{
			strcat( szCommand, " " );
			strcat( szCommand, value );
		}
		strcat( szCommand, "\n" );

		if (!*s)
			return;
		s++;
	}
}

/*
==============
ChangeLevel

Server is changing to a new level, check mapcycle.txt for map name and setup info
==============
*/
void CHalfLifeMultiplay :: ChangeLevel( void )
{
	static char szPreviousMapCycleFile[ 256 ];
	static mapcycle_t mapcycle;

	char szNextMap[32];
	char szFirstMapInList[32];
	char szCommands[ 1500 ];
	char szRules[ 1500 ];
	int minplayers = 0, maxplayers = 0;
	strcpy( szFirstMapInList, "hldm1" );  // the absolute default level is hldm1

	int	curplayers;
	BOOL do_cycle = TRUE;

	// find the map to change to
	char *mapcfile = (char*)CVAR_GET_STRING( "mapcyclefile" );
	ASSERT( mapcfile != NULL );

	szCommands[ 0 ] = '\0';
	szRules[ 0 ] = '\0';

	curplayers = CountPlayers();

#ifdef _DEBUG
	g_iKickSomeone = 0;
#endif

	// Has the map cycle filename changed?
	if ( stricmp( mapcfile, szPreviousMapCycleFile ) )
	{
		strcpy( szPreviousMapCycleFile, mapcfile );

		DestroyMapCycle( &mapcycle );

		if ( !ReloadMapCycleFile( mapcfile, &mapcycle ) || ( !mapcycle.items ) )
		{
			ALERT( at_console, "Unable to load map cycle file %s\n", mapcfile );
			do_cycle = FALSE;
		}
	}

	if ( do_cycle && mapcycle.items )
	{
		BOOL keeplooking = FALSE;
		BOOL found = FALSE;
		mapcycle_item_s *item;

		// Assume current map
		strcpy( szNextMap, STRING(gpGlobals->mapname) );
		strcpy( szFirstMapInList, STRING(gpGlobals->mapname) );

		// Traverse list
		for ( item = mapcycle.next_item; item->next != mapcycle.next_item; item = item->next )
		{
			keeplooking = FALSE;

			ASSERT( item != NULL );

			if ( item->minplayers != 0 )
			{
				if ( curplayers >= item->minplayers )
				{
					found = TRUE;
					minplayers = item->minplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( item->maxplayers != 0 )
			{
				if ( curplayers <= item->maxplayers )
				{
					found = TRUE;
					maxplayers = item->maxplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( keeplooking )
				continue;

			found = TRUE;
			break;
		}

		if ( !found )
		{
			item = mapcycle.next_item;
		}			
		
		// Increment next item pointer
		mapcycle.next_item = item->next;

		// Perform logic on current item
		strcpy( szNextMap, item->mapname );

		ExtractCommandString( item->rulebuffer, szCommands );
		strcpy( szRules, item->rulebuffer );
	}

	if ( !IS_MAP_VALID(szNextMap) )
	{
		strcpy( szNextMap, szFirstMapInList );
	}

	g_fGameOver = TRUE;

	// Intercept map vote
	if (m_iDecidedMapIndex > -1)
	{
		strcpy(szNextMap, sBuiltInMaps[m_iDecidedMapIndex]);
	}

	ALERT( at_console, "CHANGE LEVEL: %s\n", szNextMap );
	if ( minplayers || maxplayers )
	{
		ALERT( at_console, "PLAYER COUNT:  min %i max %i current %i\n", minplayers, maxplayers, curplayers );
	}
	if ( strlen( szRules ) > 0 )
	{
		ALERT( at_console, "RULES:  %s\n", szRules );
	}
	
	CHANGE_LEVEL( szNextMap, NULL );
	if ( strlen( szCommands ) > 0 )
	{
		SERVER_COMMAND( szCommands );
	}
}

#define MAX_MOTD_CHUNK	  60
#define MAX_MOTD_LENGTH   1536 // (MAX_MOTD_CHUNK * 4)

void CHalfLifeMultiplay :: SendMOTDToClient( edict_t *client )
{
	// read from the MOTD.txt file
	int length, char_count = 0;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE_FOR_ME( (char *)CVAR_GET_STRING( "motdfile" ), &length );

	// send the server name
	MESSAGE_BEGIN( MSG_ONE, gmsgServerName, NULL, client );
		WRITE_STRING( CVAR_GET_STRING("hostname") );
	MESSAGE_END();

	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts

	while ( pFileList && *pFileList && char_count < MAX_MOTD_LENGTH )
	{
		char chunk[MAX_MOTD_CHUNK+1];
		
		if ( strlen( pFileList ) < MAX_MOTD_CHUNK )
		{
			strcpy( chunk, pFileList );
		}
		else
		{
			strncpy( chunk, pFileList, MAX_MOTD_CHUNK );
			chunk[MAX_MOTD_CHUNK] = 0;		// strncpy doesn't always append the null terminator
		}

		char_count += strlen( chunk );
		if ( char_count < MAX_MOTD_LENGTH )
			pFileList = aFileList + char_count; 
		else
			*pFileList = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgMOTD, NULL, client );
			WRITE_BYTE( *pFileList ? FALSE : TRUE );	// FALSE means there is still more message to come
			WRITE_STRING( chunk );
		MESSAGE_END();
	}

	FREE_FILE( aFileList );
}

BOOL CHalfLifeMultiplay::MutatorAllowed(const char *mutator)
{
	if ((strstr(mutator, g_szMutators[MUTATOR_SLOWMO - 1]) || atoi(mutator) == MUTATOR_SLOWMO) ||
		(strstr(mutator, g_szMutators[MUTATOR_SPEEDUP - 1]) || atoi(mutator) == MUTATOR_SPEEDUP) ||
		(strstr(mutator, g_szMutators[MUTATOR_TOPSYTURVY - 1]) || atoi(mutator) == MUTATOR_TOPSYTURVY) ||
		(strstr(mutator, g_szMutators[MUTATOR_EXPLOSIVEAI - 1]) || atoi(mutator) == MUTATOR_EXPLOSIVEAI))
		return FALSE;

	// Snowball
	if (strstr(mutator, g_szMutators[MUTATOR_MAXPACK - 1]) || atoi(mutator) == MUTATOR_MAXPACK ||
		strstr(mutator, g_szMutators[MUTATOR_BERSERKER - 1]) || atoi(mutator) == MUTATOR_BERSERKER ||
		strstr(mutator, g_szMutators[MUTATOR_PLUMBER - 1]) || atoi(mutator) == MUTATOR_PLUMBER ||
		strstr(mutator, g_szMutators[MUTATOR_COWBOY - 1]) || atoi(mutator) == MUTATOR_COWBOY ||
		strstr(mutator, g_szMutators[MUTATOR_BUSTERS - 1]) || atoi(mutator) == MUTATOR_BUSTERS ||
		strstr(mutator, g_szMutators[MUTATOR_FIRESTARTER - 1]) || atoi(mutator) == MUTATOR_FIRESTARTER ||
		strstr(mutator, g_szMutators[MUTATOR_PORTAL - 1]) || atoi(mutator) == MUTATOR_PORTAL ||
		strstr(mutator, g_szMutators[MUTATOR_RANDOMWEAPON - 1]) || atoi(mutator) == MUTATOR_RANDOMWEAPON ||
		strstr(mutator, g_szMutators[MUTATOR_ROCKETCROWBAR - 1]) || atoi(mutator) == MUTATOR_ROCKETCROWBAR ||
		strstr(mutator, g_szMutators[MUTATOR_VESTED - 1]) || atoi(mutator) == MUTATOR_VESTED)
		return !(g_GameMode == GAME_SNOWBALL);
	
	return TRUE;
}

// For gameplay that changes player models, save their current model
void CHalfLifeMultiplay::SavePlayerModel(CBasePlayer *pPlayer)
{
	char *key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict());
	char *om = g_engfuncs.pfnInfoKeyValue(key, "om");
	if (!om || !strlen(om))
	{
		char model[64];
		strcpy(model, g_engfuncs.pfnInfoKeyValue(key, "model"));
		if (model && strlen(model))
			g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), key, "om", model);
	}
}

void CHalfLifeMultiplay::ResetPlayerSettings(CBasePlayer *pPlayer)
{
	// Reset Jope name.
	char *name = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "j");
	if (name && strlen(name))
	{
		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "name", name);
		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "j", "");
	}

	// Reset model if available.
	char *model = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "om");
	if (model && strlen(model))
	{
		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", model);
		g_engfuncs.pfnSetClientKeyValue(pPlayer->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "om", "");
	}
}
