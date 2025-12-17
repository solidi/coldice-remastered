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
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to 
//				   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "com_model.h"
#include "player.h"
#include "spectator.h"
#include "client.h"
#include "soundent.h"
#include "gamerules.h"
#include "game.h"
#include "customentity.h"
#include "weapons.h"
#include "weaponinfo.h"
#include "usercmd.h"
#include "netadr.h"
#include "pm_shared.h"
#include "items.h"
#include "func_break.h"
#include "const.h"
#include "effects.h"

#if defined( GRAPPLING_HOOK )
#include "grapplinghook.h"
#endif

#if !defined ( _WIN32 )
#include <ctype.h>
#endif

extern DLL_GLOBAL ULONG		g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL		g_fGameOver;
extern DLL_GLOBAL int		g_iSkillLevel;
extern DLL_GLOBAL ULONG		g_ulFrameCount;

extern void CopyToBodyQue(entvars_t* pev);
extern int giPrecacheGrunt;
extern int gmsgSayText;
extern int gmsgStatusIcon;

extern cvar_t allow_spectators;

extern int g_teamplay;

void LinkUserMessages( void );

/*
 * used by kill command and disconnect command
 * ROBIN: Moved here from player.cpp, to allow multiple player models
 */
void set_suicide_frame(entvars_t* pev)
{       
	if (!FStrEq(STRING(pev->model), "models/player.mdl"))
		return; // allready gibbed

//	pev->frame		= $deatha11;
	pev->solid		= SOLID_NOT;
	pev->movetype	= MOVETYPE_TOSS;
	pev->deadflag	= DEAD_DEAD;
	pev->nextthink	= -1;
}


/*
===========
ClientConnect

called when a player connects to a server
============
*/
BOOL ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ]  )
{
	char text[256] = "" ;
	if ( pEntity->v.netname )
		_snprintf( text, sizeof(text), "+ %s has entered the game\n", STRING(pEntity->v.netname) );
	text[ sizeof(text) - 1 ] = 0;
	MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
		WRITE_BYTE( ENTINDEX(pEntity) );
		WRITE_STRING( text );
	MESSAGE_END();

	return g_pGameRules->ClientConnected( pEntity, pszName, pszAddress, szRejectReason );

// a client connecting during an intermission can cause problems
//	if (intermission_running)
//		ExitIntermission ();

}


/*
===========
ClientDisconnect

called when a player disconnects from a server

GLOBALS ASSUMED SET:  g_fGameOver
============
*/
extern int gmsgFlameKill;
extern int gmsgDelPart;
void ClientDisconnect( edict_t *pEntity )
{
	if (g_fGameOver)
		return;

	pEntity->v.playerclass = 0;
	/*MESSAGE_BEGIN( MSG_ALL, gmsgDelPart );
		WRITE_SHORT( ENTINDEX(pEntity) );
		WRITE_BYTE( 10 );
	MESSAGE_END();*/
	MESSAGE_BEGIN( MSG_ALL, gmsgFlameKill );
		WRITE_SHORT( ENTINDEX(pEntity) );
	MESSAGE_END();

	char text[256] = "" ;
	if ( pEntity->v.netname )
		_snprintf( text, sizeof(text), "- %s has left the game\n", STRING(pEntity->v.netname) );
	text[ sizeof(text) - 1 ] = 0;
	MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
		WRITE_BYTE( ENTINDEX(pEntity) );
		WRITE_STRING( text );
	MESSAGE_END();

	CSound *pSound;
	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( pEntity ) );
	{
		// since this client isn't around to think anymore, reset their sound. 
		if ( pSound )
		{
			pSound->Reset();
		}
	}

// since the edict doesn't get deleted, fix it so it doesn't interfere.
	pEntity->v.takedamage = DAMAGE_NO;// don't attract autoaim
	pEntity->v.solid = SOLID_NOT;// nonsolid
	UTIL_SetOrigin ( &pEntity->v, pEntity->v.origin );

	g_pGameRules->ClientDisconnected( pEntity );
}


// called by ClientKill and DeadThink
void respawn(entvars_t* pev, BOOL fCopyCorpse)
{
	if (gpGlobals->coop || gpGlobals->deathmatch)
	{
		if ( fCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			CopyToBodyQue(pev);
		}

		// respawn player
		GetClassPtr( (CBasePlayer *)pev)->Spawn( );
	}
	else
	{       // restart the entire server
		SERVER_COMMAND("reload\n");
	}
}

/*
============
ClientKill

Player entered the suicide command

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
============
*/
void ClientKill( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;

	CBasePlayer *pl = (CBasePlayer*) CBasePlayer::Instance( pev );

	if ( pl->m_fNextSuicideTime > gpGlobals->time )
		return;  // prevent suiciding too ofter

	if ( g_pGameRules->IsPropHunt() && pl->pev->fuser4 > 0 )
		return; // props cannot suicide

	if ( pl->IsSpectator() )
		return; // neither can spectators

	pl->m_fNextSuicideTime = gpGlobals->time + 1;  // don't let them suicide for 5 seconds after suiciding

	// have the player kill themself
	ClearMultiDamage();
	pev->health = 0;
	pl->Killed( pev, GIB_NEVER );

//	pev->modelindex = g_ulModelIndexPlayer;
//	pev->frags -= 2;		// extra penalty
//	respawn( pev );
}

/*
===========
ClientPutInServer

called each time a player is spawned
============
*/
void ClientPutInServer( edict_t *pEntity )
{
	CBasePlayer *pPlayer;

	entvars_t *pev = &pEntity->v;

	pPlayer = GetClassPtr((CBasePlayer *)pev);
	pPlayer->SetCustomDecalFrames(-1); // Assume none;

	// Cold Ice client defaults
	pPlayer->m_iAutoWepSwitch = 1;
	pPlayer->m_iAutoWepThrow = 1;
	pPlayer->m_iAutoMelee = 1;
	pPlayer->m_iAutoTaunt = 1;
	pPlayer->m_iPlayMusic = 1;
	pPlayer->m_iDisplayInfoMessage = 1;
	pPlayer->m_iKeyboardAcrobatics = 1;

	pPlayer->pev->iuser1 = 0;	// disable any spec modes
	pPlayer->pev->iuser2 = 0; 

	// Allocate a CBasePlayer for pev, and call spawn
	pPlayer->Spawn();

	if (g_pGameRules->IsRoundBased())
	{
		pPlayer->pev->iuser1 = 1;
		pPlayer->m_iObserverLastMode = 1;
		pPlayer->m_flForceToObserverTime = 0;
		pPlayer->pev->effects |= EF_NODRAW;
		pPlayer->pev->solid = SOLID_NOT;
		pPlayer->pev->movetype = MOVETYPE_NOCLIP;
		pPlayer->pev->takedamage = DAMAGE_NO;
		pPlayer->m_afPhysicsFlags |= PFLAG_OBSERVER;
	}

	// Reset interpolation during first frame
	pPlayer->pev->effects |= EF_NOINTERP;
}

#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;



#if defined( _MSC_VER ) || defined( WIN32 )
typedef wchar_t	uchar16;
typedef unsigned int uchar32;
#else
typedef unsigned short uchar16;
typedef wchar_t uchar32;
#endif

//-----------------------------------------------------------------------------
// Purpose: determine if a uchar32 represents a valid Unicode code point
//-----------------------------------------------------------------------------
bool Q_IsValidUChar32( uchar32 uVal )
{
	// Values > 0x10FFFF are explicitly invalid; ditto for UTF-16 surrogate halves,
	// values ending in FFFE or FFFF, or values in the 0x00FDD0-0x00FDEF reserved range
	return ( uVal < 0x110000u ) && ( (uVal - 0x00D800u) > 0x7FFu ) && ( (uVal & 0xFFFFu) < 0xFFFEu ) && ( ( uVal - 0x00FDD0u ) > 0x1Fu );
}


// Decode one character from a UTF-8 encoded string. Treats 6-byte CESU-8 sequences
// as a single character, as if they were a correctly-encoded 4-byte UTF-8 sequence.
int Q_UTF8ToUChar32( const char *pUTF8_, uchar32 &uValueOut, bool &bErrorOut )
{
	const uint8 *pUTF8 = (const uint8 *)pUTF8_;
	
	int nBytes = 1;
	uint32 uValue = pUTF8[0];
	uint32 uMinValue = 0;
	
	// 0....... single byte
	if ( uValue < 0x80 )
		goto decodeFinishedNoCheck;
	
	// Expecting at least a two-byte sequence with 0xC0 <= first <= 0xF7 (110...... and 11110...)
	if ( (uValue - 0xC0u) > 0x37u || ( pUTF8[1] & 0xC0 ) != 0x80 )
		goto decodeError;
	
	uValue = (uValue << 6) - (0xC0 << 6) + pUTF8[1] - 0x80;
	nBytes = 2;
	uMinValue = 0x80;
	
	// 110..... two-byte lead byte
	if ( !( uValue & (0x20 << 6) ) )
		goto decodeFinished;
	
	// Expecting at least a three-byte sequence
	if ( ( pUTF8[2] & 0xC0 ) != 0x80 )
		goto decodeError;
	
	uValue = (uValue << 6) - (0x20 << 12) + pUTF8[2] - 0x80;
	nBytes = 3;
	uMinValue = 0x800;
	
	// 1110.... three-byte lead byte
	if ( !( uValue & (0x10 << 12) ) )
		goto decodeFinishedMaybeCESU8;
	
	// Expecting a four-byte sequence, longest permissible in UTF-8
	if ( ( pUTF8[3] & 0xC0 ) != 0x80 )
		goto decodeError;
	
	uValue = (uValue << 6) - (0x10 << 18) + pUTF8[3] - 0x80;
	nBytes = 4;
	uMinValue = 0x10000;
	
	// 11110... four-byte lead byte. fall through to finished.
	
decodeFinished:
	if ( uValue >= uMinValue && Q_IsValidUChar32( uValue ) )
	{
decodeFinishedNoCheck:
	uValueOut = uValue;
	bErrorOut = false;
	return nBytes;
	}
decodeError:
	uValueOut = '?';
	bErrorOut = true;
	return nBytes;
	
decodeFinishedMaybeCESU8:
	// Do we have a full UTF-16 surrogate pair that's been UTF-8 encoded afterwards?
	// That is, do we have 0xD800-0xDBFF followed by 0xDC00-0xDFFF? If so, decode it all.
	if ( ( uValue - 0xD800u ) < 0x400u && pUTF8[3] == 0xED && (uint8)( pUTF8[4] - 0xB0 ) < 0x10 && ( pUTF8[5] & 0xC0 ) == 0x80 )
	{
		uValue = 0x10000 + ( ( uValue - 0xD800u ) << 10 ) + ( (uint8)( pUTF8[4] - 0xB0 ) << 6 ) + pUTF8[5] - 0x80;
		nBytes = 6;
		uMinValue = 0x10000;
	}
	goto decodeFinished;
}



//-----------------------------------------------------------------------------
// Purpose: Returns true if UTF-8 string contains invalid sequences.
//-----------------------------------------------------------------------------
bool Q_UnicodeValidate( const char *pUTF8 )
{
	bool bError = false;
	while ( *pUTF8 )
	{
		uchar32 uVal;
		// Our UTF-8 decoder silently fixes up 6-byte CESU-8 (improperly re-encoded UTF-16) sequences.
		// However, these are technically not valid UTF-8. So if we eat 6 bytes at once, it's an error.
		int nCharSize = Q_UTF8ToUChar32( pUTF8, uVal, bError );
		if ( bError || nCharSize == 6 )
			return false;
		pUTF8 += nCharSize;
	}
	return true;
}

extern int gmsgPlayClientSound;

void GameplayVote(edict_t *pEntity, const char *text)
{
	static float m_fVoteTime = 0;
	static int m_iNeedsVotes = 0;
	static int m_iVotes[32];

	if (voting.value && UTIL_stristr(text, "vote"))
	{
		// Start vote, capture player count for majority count
		if (m_fVoteTime < gpGlobals->time)
		{
			int players = 0;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(i);
				if (pPlayer && !FBitSet(pPlayer->pev->flags, FL_FAKECLIENT) && !pPlayer->HasDisconnected)
					players++;
			}

			m_fVoteTime = gpGlobals->time + 30;
			m_iNeedsVotes = (players / 2) + 1;

			// Person who calls, also votes.
			memset(m_iVotes, 0, sizeof(m_iVotes));
			m_iVotes[ENTINDEX(pEntity)] = 1;

			MESSAGE_BEGIN(MSG_ALL, gmsgPlayClientSound);
				WRITE_BYTE(CLIENT_SOUND_VOTESTARTED);
			MESSAGE_END();

			if (m_iNeedsVotes <= 1)
			{
				// Sole person can call an immediate end.
				m_iNeedsVotes = m_fVoteTime = 0;
				UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Single person gets the vote.\n");
				g_pGameRules->EndMultiplayerGame();
			}
			else
			{
				UTIL_ClientPrintAll(HUD_PRINTTALK,
					UTIL_VarArgs("[VOTE] We need %.0f vote(s) in 30 seconds. Others, type \"vote\" to rock the vote.\n", fmax(1, m_iNeedsVotes - 1)));
			}
		}
		else
		{
			// Hasn't voted.
			if (m_iVotes[ENTINDEX(pEntity)] <= 0)
			{
				m_iVotes[ENTINDEX(pEntity)] = 1;

				// Tally
				int votes = 0;
				for (int i = 1; i <= gpGlobals->maxClients; i++)
				{
					CBaseEntity *p = UTIL_PlayerByIndex(i);
					if (p && m_iVotes[p->entindex()] > 0)
						votes++;
				}

				// Confirm
				if (votes >= m_iNeedsVotes)
				{
					m_iNeedsVotes = m_fVoteTime = 0;
					UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Vote success!\n");
					g_pGameRules->EndMultiplayerGame();
				}
				else
				{
					UTIL_ClientPrintAll(HUD_PRINTTALK,
						UTIL_VarArgs("[VOTE] %s voted (%d / %d)\n", STRING(pEntity->v.netname), votes, m_iNeedsVotes));
				}
			}
		}
	}
}

void MutatorVote(edict_t *pEntity, const char *text)
{
	static float m_fVoteTime = 0;
	static int m_iNeedsVotes = 0;
	static int m_iVotes[32];

	if (voting.value && UTIL_stristr(text, "mutator"))
	{
		// Start vote, capture player count for majority count
		if (m_fVoteTime < gpGlobals->time)
		{
			int players = 0;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex(i);
				if (pPlayer && !FBitSet(pPlayer->pev->flags, FL_FAKECLIENT) && !pPlayer->HasDisconnected)
					players++;
			}

			m_fVoteTime = gpGlobals->time + 30;
			m_iNeedsVotes = (players / 2) + 1;

			// Person who calls, also votes.
			memset(m_iVotes, 0, sizeof(m_iVotes));
			m_iVotes[ENTINDEX(pEntity)] = 1;

			MESSAGE_BEGIN(MSG_ALL, gmsgPlayClientSound);
				WRITE_BYTE(CLIENT_SOUND_VOTESTARTED);
			MESSAGE_END();

			if (m_iNeedsVotes <= 1)
			{
				// Sole person can call an immediate end.
				m_iNeedsVotes = m_fVoteTime = 0;
				UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Single person gets the vote.\n");
				g_pGameRules->VoteForMutator();
			}
			else
			{
				UTIL_ClientPrintAll(HUD_PRINTTALK,
					UTIL_VarArgs("[VOTE] We need %.0f vote(s) in 30 seconds. Others, type \"mutator\" to vote.\n", fmax(1, m_iNeedsVotes - 1)));
			}
		}
		else
		{
			// Hasn't voted.
			if (m_iVotes[ENTINDEX(pEntity)] <= 0)
			{
				m_iVotes[ENTINDEX(pEntity)] = 1;

				// Tally
				int votes = 0;
				for (int i = 1; i <= gpGlobals->maxClients; i++)
				{
					CBaseEntity *p = UTIL_PlayerByIndex(i);
					if (p && m_iVotes[p->entindex()] > 0)
						votes++;
				}

				// Confirm
				if (votes >= m_iNeedsVotes)
				{
					m_iNeedsVotes = m_fVoteTime = 0;
					UTIL_ClientPrintAll(HUD_PRINTTALK, "[VOTE] Mutator vote success!\n");
					g_pGameRules->VoteForMutator();
				}
				else
				{
					UTIL_ClientPrintAll(HUD_PRINTTALK,
						UTIL_VarArgs("[VOTE] %s voted (%d / %d)\n", STRING(pEntity->v.netname), votes, m_iNeedsVotes));
				}
			}
		}
	}
}

//// HOST_SAY
// String comes in as
// say blah blah blah
// or as
// blah blah blah
//
void Host_Say( edict_t *pEntity, int teamonly )
{
	CBasePlayer *client;
	int		j;
	char	*p;
	char	text[128];
	char    szTemp[256];
	const char *cpSay = "say";
	const char *cpSayTeam = "say_team";
	const char *pcmd = CMD_ARGV(0);

	// We can get a raw string now, without the "say " prepended
	if ( CMD_ARGC() == 0 )
		return;

	entvars_t *pev = &pEntity->v;
	CBasePlayer* player = GetClassPtr((CBasePlayer *)pev);

	//Not yet.
	if ( player->m_flNextChatTime > gpGlobals->time )
		 return;

	if ( !stricmp( pcmd, cpSay) || !stricmp( pcmd, cpSayTeam ) )
	{
		if ( CMD_ARGC() >= 2 )
		{
			p = (char *)CMD_ARGS();
		}
		else
		{
			// say with a blank message, nothing to do
			return;
		}
	}
	else  // Raw text, need to prepend argv[0]
	{
		if ( CMD_ARGC() >= 2 )
		{
			sprintf( szTemp, "%s %s", ( char * )pcmd, (char *)CMD_ARGS() );
		}
		else
		{
			// Just a one word command, use the first word...sigh
			sprintf( szTemp, "%s", ( char * )pcmd );
		}
		p = szTemp;
	}

// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p)-1] = 0;
	}

// make sure the text has content

	if ( !p || !p[0] || !Q_UnicodeValidate ( p ) )
		return;  // no character found, so say nothing

// turn on color set 2  (color on,  no sound)
	// turn on color set 2  (color on,  no sound)
	if ( player->IsObserver() && ( teamonly ) )
		sprintf( text, "%c(SPEC) %s: ", 2, STRING( pEntity->v.netname ) );
	else if ( teamonly )
		sprintf( text, "%c(TEAM) %s: ", 2, STRING( pEntity->v.netname ) );
	else
		sprintf( text, "%c%s: ", 2, STRING( pEntity->v.netname ) );

	j = sizeof(text) - 2 - strlen(text);  // -2 for /n and null terminator
	if ( (int)strlen(p) > j )
		p[j] = 0;

	strcat( text, p );
	strcat( text, "\n" );


	player->m_flNextChatTime = gpGlobals->time + CHAT_INTERVAL;

	// loop through all players
	// Start with the first player.
	// This may return the world in single player if the client types something between levels or during spawn
	// so check it, or it will infinite loop

	client = NULL;
	while ( ((client = (CBasePlayer*)UTIL_FindEntityByClassname( client, "player" )) != NULL) && (!FNullEnt(client->edict())) ) 
	{
		if ( !client->pev )
			continue;
		
		if ( client->edict() == pEntity )
			continue;

		if ( !(client->IsNetClient()) )	// Not a client ? (should never be true)
			continue;

		// can the receiver hear the sender? or has he muted him?
		if ( g_VoiceGameMgr.PlayerHasBlockedPlayer( client, player ) )
			continue;

		if ( !player->IsObserver() && teamonly && g_pGameRules->PlayerRelationship(client, CBaseEntity::Instance(pEntity)) != GR_TEAMMATE )
			continue;

		// Spectators can only talk to other specs
		if ( player->IsObserver() && teamonly )
			if ( !client->IsObserver() )
				continue;

		if ( client->HasDisconnected )
				continue;

		MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, client->pev );
			WRITE_BYTE( ENTINDEX(pEntity) );
			WRITE_STRING( text );
		MESSAGE_END();

	}

	// print to the sending client
	MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, &pEntity->v );
		WRITE_BYTE( ENTINDEX(pEntity) );
		WRITE_STRING( text );
	MESSAGE_END();

	// echo to server console
	g_engfuncs.pfnServerPrint( text );

	if (UTIL_stristr(text, "jope"))
		UTIL_ClientPrintAll(HUD_PRINTTALK, "ALL HAIL KING JOPE!\n");

	GameplayVote(pEntity, text);
	MutatorVote(pEntity, text);

	char * temp;
	if ( teamonly )
		temp = "say_team";
	else
		temp = "say";
	
	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" %s \"%s\"\n", 
			STRING( pEntity->v.netname ), 
			GETPLAYERUSERID( pEntity ),
			GETPLAYERAUTHID( pEntity ),
			g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pEntity ), "model" ),
			temp,
			p );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" %s \"%s\"\n", 
			STRING( pEntity->v.netname ), 
			GETPLAYERUSERID( pEntity ),
			GETPLAYERAUTHID( pEntity ),
			GETPLAYERUSERID( pEntity ),
			temp,
			p );
	}
}


/*
===========
ClientCommand
called each time a player uses a "cmd" command
============
*/
extern int gmsgVoteFor;
extern int gmsgVoteGameplay;
extern int gmsgVoteMap;
extern int gmsgVoteMutator;

void Vote( CBasePlayer *pPlayer, int vote )
{
	if (pPlayer->m_fVoteCoolDown < gpGlobals->time)
	{
		if (vote > -1)
		{
			MESSAGE_BEGIN(MSG_ALL, gmsgVoteFor);
				WRITE_BYTE(pPlayer->entindex());
				WRITE_SHORT(vote);
			MESSAGE_END();

			g_pGameRules->m_iVoteCount[pPlayer->entindex()-1] = vote;

			MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, pPlayer->edict());
				WRITE_BYTE(CLIENT_SOUND_GREATJOB);
			MESSAGE_END();

			ALERT(at_aiconsole, "id[%d] voted for #%d\n", pPlayer->entindex(), vote);
			ClientPrint(pPlayer->pev, HUD_PRINTTALK, UTIL_VarArgs("[VOTE] You voted for \"%s\". Waiting for others to tally vote.\n", vote == (MUTATOR_VOLATILE + 1) ? "random" : g_szMutators[vote-1]));
		}
		else
		{
			ALERT(at_aiconsole, "id[%d] please provide a vote tally e.g. \"vote 1\"\n");
		}

		pPlayer->m_fVoteCoolDown = gpGlobals->time + 1.0;
	}
	else
	{
		ALERT(at_aiconsole, "id[%d] vote cool down time left: %.2f\n", pPlayer->entindex(), pPlayer->m_fVoteCoolDown - gpGlobals->time);
	}
}

// Use CMD_ARGV,  CMD_ARGV, and CMD_ARGC to get pointers the character string command.
void ClientCommand( edict_t *pEntity )
{
	const char *pcmd = CMD_ARGV(0);
	const char *pstr;

	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	entvars_t *pev = &pEntity->v;

	if ( FStrEq(pcmd, "say" ) )
	{
		Host_Say( pEntity, 0 );
	}
	else if ( FStrEq(pcmd, "say_team" ) )
	{
		Host_Say( pEntity, 1 );
	}
	else if ( FStrEq(pcmd, "fullupdate" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->ForceClientDllUpdate(); 
	}
	else if ( FStrEq(pcmd, "give" ) )
	{
		if (g_psv_cheats->value != 0.0)
		{
			int iszItem = ALLOC_STRING( CMD_ARGV(1) );	// Make a copy of the classname
			GetClassPtr((CBasePlayer *)pev)->GiveNamedItem( STRING(iszItem) );
		}
	}

	else if ( FStrEq(pcmd, "drop" ) )
	{
		// player is dropping an item. 
		GetClassPtr((CBasePlayer *)pev)->DropPlayerItem((char *)CMD_ARGV(1));
	}
	else if ( FStrEq(pcmd, "feign" ) )
	{
		CBasePlayer *player = GetClassPtr((CBasePlayer *)pev);
		entvars_t *p = player->pev;
		if (player->m_fFeignTime < gpGlobals->time)
		{
			if (p->deadflag == DEAD_NO && FBitSet(pev->flags, FL_ONGROUND) && pev->velocity.Length() < 100)
			{
				player->SetAnimation( PLAYER_DIE );
				p->view_ofs[2] = VEC_DUCK_HULL_MIN.z + 2;
				player->EnableControl(FALSE);
				UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
				if (RANDOM_LONG(0,1))
					EMIT_SOUND(ENT(pev), CHAN_VOICE, "common/bodydrop3.wav", 1, ATTN_NORM);
				else
					EMIT_SOUND(ENT(pev), CHAN_VOICE, "common/bodydrop4.wav", 1, ATTN_NORM);
				if (player->m_pActiveItem)
					player->m_pActiveItem->Holster();
				player->pev->viewmodel = 0; 
				player->pev->weaponmodel = 0;
				p->deadflag = DEAD_FAKING;
				player->m_EFlags &= ~(EFLAG_TAUNT | EFLAG_CANCEL);
				player->m_EFlags |= EFLAG_DEADHANDS;
			}
			else if (p->deadflag == DEAD_FAKING)
			{
				player->SetAnimation( PLAYER_IDLE );
				p->view_ofs[2] = VEC_VIEW.z;
				player->EnableControl(TRUE);
				UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
				if (RANDOM_LONG(0,1))
					EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_step1.wav", 1, ATTN_NORM);
				else
					EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_step2.wav", 1, ATTN_NORM);
				p->deadflag = DEAD_NO;
				if (player->m_pActiveItem)
					player->m_pActiveItem->DeployLowKey();
				player->m_EFlags &= ~EFLAG_DEADHANDS;
			}
			
			player->m_fFeignTime = gpGlobals->time + 1.0;
		}
	}
	else if ( FStrEq(pcmd, "taunt" ) )
	{
		CBasePlayer *player = GetClassPtr((CBasePlayer *)pev);
		player->Taunt();
	}
	else if ( FStrEq(pcmd, "fov" ) )
	{
		if (g_psv_cheats->value && CMD_ARGC() > 1)
		{
			GetClassPtr((CBasePlayer *)pev)->m_iFOV = atoi( CMD_ARGV(1) );
		}
		else
		{
			CLIENT_PRINTF( pEntity, print_console, UTIL_VarArgs( "\"fov\" is \"%d\"\n", (int)GetClassPtr((CBasePlayer *)pev)->m_iFOV ) );
		}
	}
#if defined( GRAPPLING_HOOK )
	else if (FStrEq(pcmd, "+hook" ))
	{
		CBasePlayer *plr = GetClassPtr((CBasePlayer *)pev);

		if ( g_pGameRules->AllowGrapplingHook(plr) )
		{
			if (plr->pGrapplingHook == NULL && plr->m_flNextHook < gpGlobals->time) {
				plr->pGrapplingHook = CHook::HookCreate(plr);
				plr->pGrapplingHook->FireHook();
				plr->m_flNextHook = gpGlobals->time + grapplinghookdeploytime.value;
			}
		} else {
			ClientPrint( pev, HUD_PRINTCONSOLE, "Grappling hook is disabled.\n" );
		}
	}
	else if (FStrEq(pcmd, "-hook" ))
	{
		CBasePlayer *plr = GetClassPtr((CBasePlayer *)pev);

		if (plr->pGrapplingHook) {
			plr->pGrapplingHook->KillHook();
			plr->pGrapplingHook = NULL;
		}
	}
#endif
	else if ( FStrEq(pcmd, "drop_rune" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer->m_fHasRune )
		{
			CWorldRunes::DropRune(pPlayer);
			ClientPrint(pPlayer->pev, HUD_PRINTCENTER, "Discarded Rune\n");
		}
	}
	else if ( FStrEq(pcmd, "snowman" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if (g_psv_cheats->value) {
			if ( pPlayer->pev->flags & FL_GODMODE ) {
				pPlayer->pev->flags &= ~FL_GODMODE;
				pPlayer->pev->flags &= ~FL_NOTARGET; // chumtoads and things
				ClientPrint(pPlayer->pev, HUD_PRINTCONSOLE, "God mode OFF\n");
			} else {
				pPlayer->pev->flags |= FL_GODMODE;
				pPlayer->pev->flags |= FL_NOTARGET;
				ClientPrint(pPlayer->pev, HUD_PRINTCONSOLE, "God mode ON\n");
			}
		}
	}
	else if ( FStrEq(pcmd, "use" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->SelectItem((char *)CMD_ARGV(1));
	}
	else if (((pstr = strstr(pcmd, "weapon_")) != NULL)  && (pstr == pcmd))
	{
		GetClassPtr((CBasePlayer *)pev)->SelectItem(pcmd);
	}
	else if (FStrEq(pcmd, "lastinv" ))
	{
		GetClassPtr((CBasePlayer *)pev)->SelectLastItem();
	}
	else if ( FStrEq( pcmd, "spectate" ) )	// clients wants to become a spectator
	{
			// always allow proxies to become a spectator
		if ( (pev->flags & FL_PROXY) || allow_spectators.value  )
		{
			CBasePlayer * pPlayer = GetClassPtr((CBasePlayer *)pev);

			edict_t *pentSpawnSpot = g_pGameRules->GetPlayerSpawnSpot( pPlayer );
			pPlayer->StartObserver(pentSpawnSpot->v.origin, VARS(pentSpawnSpot)->angles);

			// notify other clients of player switching to spectator mode
			UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s switched to spectator mode\n", 
			 	( pev->netname && STRING(pev->netname)[0] != 0 ) ? STRING(pev->netname) : "unconnected" ) );
		}
		else
			ClientPrint( pev, HUD_PRINTCONSOLE, "Spectator mode is disabled.\n" );
			
	}
	else if ( FStrEq(pcmd, "vote" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);
		int vote = -1;
		if (CMD_ARGC() > 1)
			vote = atoi(CMD_ARGV(1));
		Vote(pPlayer, vote);
	}
#ifdef _DEBUG
	else if ( FStrEq( pcmd, "votegameplay") )
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgVoteGameplay);
			WRITE_BYTE(10);
		MESSAGE_END();
	}
	else if ( FStrEq( pcmd, "votemap") )
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgVoteMap);
			WRITE_BYTE(10);
		MESSAGE_END();
	}
	else if ( FStrEq( pcmd, "votemutator") )
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgVoteMutator);
			WRITE_BYTE(10);
		MESSAGE_END();
	}
	else if ( FStrEq( pcmd, "endgame") )
	{
		g_pGameRules->EndMultiplayerGame();
	}
	else if ( FStrEq(pcmd, "barrel") ) 
	{
		UTIL_MakeVectors( pev->v_angle );
		Vector vecSrc = pev->origin + pev->view_ofs + gpGlobals->v_forward * 64 + gpGlobals->v_up * 18;
		CBaseEntity::Create( "monster_barrel", vecSrc, Vector(0, pev->v_angle.y, 0), NULL );
	}
	else if ( FStrEq( pcmd, "sx" ) )
	{
		CBasePlayer * pPlayer = GetClassPtr((CBasePlayer *)pev);
		pPlayer->ExitObserver();
	}
	else if ( FStrEq(pcmd, "mapname" ) )
	{
		//gEngfuncs.pfnGetLevelName()
		extern int gmsgPlayClientSound;
		MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, pEntity );
			WRITE_BYTE(CLIENT_SOUND_TAKENLEAD);
		MESSAGE_END();
		ALERT(at_console, "Map name is %s.bsp\n", STRING(gpGlobals->mapname));
	}
	else if ( FStrEq(pcmd, "eyes" ) )
	{
		static EHANDLE currentView;
		CBaseEntity* pFound = currentView;
		while ((pFound = UTIL_FindEntityInSphere(pFound, pev->origin, 500)) != NULL)
		{
			if (pFound->IsAlive())
			{
				if (strncmp(STRING(pFound->pev->classname), "monster_", 8) == 0)
				{
					ALERT(at_aiconsole, ">>> eyes set to %s\n", STRING(pFound->pev->classname));
					//pFound->pev->view_ofs.z = 64;
					//pev->view_ofs.z = 64;
					//pev->view_ofs = Vector(0,0,64);
					SET_VIEW(ENT(pev), ENT(pFound->pev));

					ALERT(at_aiconsole, "ours->view_ofs.z = %.2f, theirs->view_ofs.z = %.2f\n", pev->view_ofs.z, pFound->pev->view_ofs.z);
					//pev->fixangle = TRUE;
					pev->angles.x = -pev->angles.x;
					break;
				}
			}
		}

		currentView = pFound;
	}
	else if ( FStrEq(pcmd, "xeyes" ) )
	{
		static EHANDLE currentView;
		SET_VIEW(ENT(pev), ENT(pev));
		currentView = NULL;
	}
	else if (FStrEq(pcmd, "fx"))
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		pPlayer->pev->frags += 1;

		if (pPlayer->m_iBurstCount > kRenderFxLightMultiplier ||  pPlayer->m_iBurstCount < kRenderFxNone)
			pPlayer->m_iBurstCount = kRenderFxNone;

		ClientPrint( pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "fx set to: %d\n", pPlayer->m_iBurstCount ));
		
		/*MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_PARTICLEBURST );
			WRITE_COORD(pev->origin.x);
			WRITE_COORD(pev->origin.y);
			WRITE_COORD(pev->origin.z);
			WRITE_SHORT( 50 );
			WRITE_BYTE((unsigned short)pPlayer->m_iBurstCount++);
			WRITE_BYTE( 5 );
		MESSAGE_END();*/

		pPlayer->pev->renderfx = pPlayer->m_iBurstCount++;
	}
	else if (FStrEq(pcmd, "render"))
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);
		if (pPlayer->m_iModeCount > kRenderTransAdd ||  pPlayer->m_iModeCount < kRenderNormal)
			pPlayer->m_iModeCount = kRenderNormal;

		ClientPrint( pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "render set to: %d\n", pPlayer->m_iModeCount ));

		pPlayer->pev->rendermode = pPlayer->m_iModeCount++;
	}
	else if (FStrEq(pcmd, "amt"))
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);
		if (pPlayer->m_iAmt > 255 ||  pPlayer->m_iAmt < 0)
			pPlayer->m_iAmt = 0;

		ClientPrint( pev, HUD_PRINTCONSOLE, UTIL_VarArgs( "amt set to: %d\n", pPlayer->m_iAmt ));

		pPlayer->pev->renderamt = pPlayer->m_iAmt++;
	}
	else if ( FStrEq( pcmd, "fog_off") )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);
		CBaseEntity *pEntity = UTIL_FindEntityByClassname(NULL, "env_fog");
		if (pEntity)
		{
			pEntity->Use( CBaseEntity::Instance(pev), CBaseEntity::Instance(pev), USE_OFF, 0 );
		}
	}
	else if ( FStrEq( pcmd, "fog_on") )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);
		CBaseEntity *pEntity = UTIL_FindEntityByClassname(NULL, "env_fog");
		if (pEntity)
		{
			pEntity->Use( CBaseEntity::Instance(pev), CBaseEntity::Instance(pev), USE_ON, 0 );
		}
	}
	else if ( FStrEq( pcmd, "send") )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);
		extern int gmsgObjective;
		char text[256];
		sprintf(text, "%d, %d, %d", RANDOM_LONG(1,100), RANDOM_LONG(1,100), RANDOM_LONG(1,100));
		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING(text);
			WRITE_STRING("xyz");
			WRITE_BYTE(RANDOM_LONG(1,100));
		MESSAGE_END();
	}
	else if ( FStrEq(pcmd, "prop" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		SET_MODEL(ENT(pev), "models/w_weapons.mdl");
		UTIL_SetSize(pPlayer->pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);

		pPlayer->pev->health = 1;
		pPlayer->pev->armorvalue = 0;
		pPlayer->pev->fuser4 = 1;
		//DROP_TO_FLOOR ( ENT(pPlayer->pev) );
		ALERT(at_aiconsole, "now set to fuser4 > 0\n");
		CLIENT_COMMAND(pPlayer->edict(), "thirdperson\n");
	}
	else if ( FStrEq(pcmd, "exprop" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);
		pPlayer->pev->fuser4 = 0;
		ALERT(at_aiconsole, "now set to fuser4=> 0\n");
		CLIENT_COMMAND(pPlayer->edict(), "firstperson\n");
	}
	else if ( FStrEq(pcmd, "decoy" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);
		UTIL_MakeVectors( pPlayer->pev->v_angle );
		Vector vecSrc = pPlayer->pev->origin + pPlayer->pev->view_ofs + gpGlobals->v_forward * 64 + gpGlobals->v_up * 18;

		if (pPlayer->m_iPropsDeployed < 10)
		{
			CBaseEntity *p = CBaseEntity::Create( "monster_propdecoy", vecSrc, Vector(0, pPlayer->pev->v_angle.y, 0), NULL );
			if (p)
			{
				p->pev->body = pPlayer->pev->fuser4;
			}
		}
		else
			ClientPrint(pPlayer->pev, HUD_PRINTCENTER, "All decoys are deployed.\n");
	}
	else if ( FStrEq( pcmd, "km" )  )
	{
		edict_t *pEdict = FIND_ENTITY_BY_STRING(NULL, "message", "horde");
		while (!FNullEnt(pEdict))
		{
			UTIL_Remove(CBaseEntity::Instance(pEdict));
			pEdict = FIND_ENTITY_BY_STRING(pEdict, "message", "horde");
		}
	}
	else if ( FStrEq( pcmd, "que" )  )
	{
		extern void ClearBodyQue();
		ClearBodyQue();
	}
	else if ( FStrEq( pcmd, "infobuff" )  )
	{
		CBasePlayer * pPlayer = GetClassPtr((CBasePlayer *)pev);
		char *key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict());
		ALERT(at_aiconsole, "key=%s\n", key);
		pPlayer->pev->frags += 1;
	}
	else if ( FStrEq(pcmd, "slou" ) )
	{
		static int whatup = -36;
		while (whatup < 1000000)
		{
		cvar_t *g_fps_max = CVAR_GET_POINTER("fps_max");
		cvar_t *g_sys_timescale = (cvar_t*)(((char*)CVAR_GET_POINTER("fps_max") - whatup));

		bool pointingToGarbage = abs(g_sys_timescale->name - g_fps_max->name) > 1024; // heuristic
		if (!pointingToGarbage) {
			if (memcmp(g_sys_timescale->name, "sys_timescale", 14) == 0) {
				ALERT(at_aiconsole, "yes! %d\n", whatup);
				break;
			}
			else
				ALERT(at_aiconsole, "uh! %s\n", g_sys_timescale->name);
		}
		//else
			//ALERT(at_aiconsole, "no! %d\n", whatup);

		whatup++;
		}
	}
	else if ( FStrEq(pcmd, "slod" ) )
	{
		static int whatup = -36;
		while (whatup > -1000000)
		{
		cvar_t *g_fps_max = CVAR_GET_POINTER("fps_max");
		cvar_t *g_sys_timescale = (cvar_t*)(((char*)CVAR_GET_POINTER("fps_max") - whatup));

		bool pointingToGarbage = abs(g_sys_timescale->name - g_fps_max->name) > 1024; // heuristic
		if (!pointingToGarbage) {
			if (memcmp(g_sys_timescale->name, "sys_timescale", 14) == 0)
			{
				ALERT(at_aiconsole, "yes! %d\n", whatup);
				break;
			}
			else
				ALERT(at_aiconsole, "uh! %s\n", g_sys_timescale->name);
		}
		//else
			//ALERT(at_aiconsole, "no! %d\n", whatup);

		whatup--;
		}
	}
#endif
	else if ( FStrEq( pcmd, "specmode" )  )	// new spectator mode
	{
		CBasePlayer * pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer->IsObserver() )
			pPlayer->Observer_SetMode( atoi( CMD_ARGV(1) ) );
	}
	else if ( FStrEq(pcmd, "closemenus" ) )
	{
		// just ignore it
	}
	else if ( FStrEq( pcmd, "follownext" )  )	// follow next player
	{
		CBasePlayer * pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer->IsObserver() )
			pPlayer->Observer_FindNextPlayer( atoi( CMD_ARGV(1) )?true:false );
	}
	else if ( FStrEq( pcmd, "help" )  )
	{
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "[Client Help Menu]\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"+hook\" - Deploy hook\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"+ironsight\" - Use ironsights\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_achievements [0|1|2|3]\" - displays fast fragging achievements\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_announcehumor [0|1]\" - Play announcement/humor on weapons\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_am [0|1]\" - auto kick or punch an enemy if they are close\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_at [0|1]\" - auto taunt on frag when its safe to do so\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_aws [0|1]\" - auto switches weapon on pickup\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_awt [0|1]\" - auto throws weapon on empty\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_bobtilt [0|1]\" - Old Bob Tilt\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_bulletsmoke [0|1]\" - turn on or off bullet smoke and flare effects\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_crosshairammo [0|1]\" - show ammo status in crosshairs\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_customtempents [0|1]\" - allow or disallow increased temporary entites\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_flashonpickup [0|1]\" - Flash HUD when picking up weapon or item\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_glasshud [0|1]\" - switch elements of the hud bouncing/bobbing on or off\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_glowmodels [0|1]\" - Show glow models is available\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_gunsmoke [0|1]\" - turn on or off gun smoke effects when fired\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_hudbend\" - experimental bending factor of HUD elements\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_hudscale\" - experimental scaling factor of HUD elements\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_icemodels [0-6]\" - changes models with specific ice skins\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_minfo [0|1]\" - display weapon and rune pick up messages\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_objectives [0|1]\" - show objective read out on HUD\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_oldmotd [0|1]\" - Old MOTD (Message of the Day)\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_oldscoreboard [0|1]\" - Old Scoreboard\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_particlesystem [0|1]\" enables or disables special effects like the flamethrower\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_playpoint [0|1]\" - Play ding when inflicting damage, dong for frag\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_music [0|1]\" - Play soundtrack set by map\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "For more, type help_more\n" );
	}
	else if ( FStrEq( pcmd, "help_more" )  )
	{
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "[Client Help More]\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_radar [0|1]\" enables or disables player radar\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_righthand [0|1|-1]\" - Left/Right Handed Models, or left knife\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_shadows [0|1]\" - Show rendered shadows underneath models\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_showposition [0|1]\" - Show leaderboard position\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_showtips [0|1]\" - Show random text tips during play\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_viewroll [0|1]\" - Old View Roll\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_voiceoverpath [string]\" - folder path to custom voiceovers\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_wallclimbindicator [0|1]\" - shows when wallclimb is available\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_weaponfidget [0|1]\" - Weapon Fidget\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_weaponretract [0|1]\" - Weapon Retract\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_weaponsway [0|1]\" - Weapon Sway\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cl_weather [0|1]\" - allow or disallow weather effects on client\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"feign\" - Fake your death\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"drop_rune\" - Drop rune\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"impulse 205\" - Swap between single and dual weapon, if available\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"impulse 206\" - Kick\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"impulse 207\" - Punch\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"impulse 208\" - Slide\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"impulse 209\" - Grenade\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"impulse 210\" - Roll Right\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"impulse 211\" - Roll Left\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"impulse 212\" - Back Flip\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"impulse 213\" - Front Flip\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"impulse 214\" - Hurricane Kick\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"impulse 215\" - Force Grab\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"impulse 216\" - Drop Explosive Weapon\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"keyboard\" - Show default key binds on HUD\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mutator\" - type in the chat to start a mutator vote\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"snowman\" - God mode (when sv_cheats 1)\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"vote\" - type in the chat to start a vote\n" );
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "For more, see readme.txt\n" );
	}
	else if ( FStrEq( pcmd, "help_server" )  )
	{
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "[Server Help Menu]\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"addbot\" - Add a bot\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"botdontshoot [0|1]\" - Enable or disable bots attacking others\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"botpause [0|1]\" - Bots stay in place\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_allowrunes [0|1]\" - Allow powerup runes on server\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_allowvoiceovers [0|1]\" - Allow public voiceovers\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_coldspotspawn\" - name of entity where the cold spot will spawn\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_coldspottime\" - how long the coldspot stays in an area.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_ctcsecondsforpoint\" - amount of second holding chumtoad for a point\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_ctfspawn1\" - name of entity where blue base will spawn\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_ctfspawn2\" - name of entity where red base will spawn\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_ctfdistance\" - units inbetween flags\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_dualsonly [0|1]\" - Replace all weapons with duals only\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_floatingweapons [0|1]\" - Floating world weapons ala Quake\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_gamemode [gamemode]\" -> available gamemodes\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"busters\"\" - one player busts ghosts with the egon\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"chilldemic\"\" - be the survivor from the virus\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"coldskull\"\" - collect all the cold skulls to win\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"ctc\"\" - capture the chumtoad, hold on to it to receive points!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"ctf\"\" - capture the flag\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"ffa\"\" - game mode is deathmatch\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"gungame\"\" - get frags with specific weapons and level up!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"horde\"\" - frag monsters in each wave\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"instagib\"\" - gib with zappers to make tombstones!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"jvs\"\" - game mode is Jesus vs Santa - defeat him!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"prophunt\"\" - hide and don't be found!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"lms\"\" - game mode is battle royale\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"shidden\"\" - invisible dealters and those smelters\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"snowball\"\" - game mode is snowballs and grenades!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"teamplay\"\" - frag on teams\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_ggfrags\" - How many frags needed for the next level\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_ggstartlevel\" - Sets default start level of gun game\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_ggsteallevel\" - Steal levels on fists frag\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_ggsuicide\" - Lose a level if the player suicides\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_grabsky [0|1]\" - Allow player to grapple and portal to the skybox\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_grapplinghook [0|1]\" - Allow grappling hook on server\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_grapplinghookdeploytime 1.0\" - Time (seconds) when next grappling hook can deploy\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_holsterweapons [0|1]\" - Holstering weapons for more realistic gameplay\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_iceblood [0|1]\" - Enable blue blood\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_icesprites [0|1]\" - Switch between select ice or real environment sprites\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_interactiveitems \"grenade;monster_satchel;monster_chumtoad;monster_snark;monster_barrel;gib\"\" - A semicolon separated list of items that are \"interactive\" (kickable, pickupable)\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_meleedrop \"[0|1]\"\" - allow kick or punch attcks to drop weapons out of hands\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_moreblood [0-5]\" - Increase blood up to 0-5 times\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_nukemode \"[0|1|2]\"\" - 2 - nuke kills all, 1 - radius damage, 0 - sharts nothing but bubbles\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_scorelimit\" - Sets the maximum score limit before map change\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_randomgamemodes \"[0|1]\"\" - selects a random gamemode on map change\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_randomweapon [0|1]\" - To spawn with a random weapon\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_roundfraglimit\" - Sets the max frags in a round during an 1 on 1 arena\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_roundlimit\" - Sets the maximum amount of game rounds before a map change\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_roundtimelimit\" - Sets the maximum amount of time a round will run\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_roundwaittime\" - seconds until round begins\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_royaleteam [0|1]\" - set battle royale as two-team based\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_royaldamage [0|1]\" - apply damage to persons outside the safe area\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_snowballfight [0|1]\" - Replace all weapons with deadly snowballs!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_spawnitems\" - Spawn items or not\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_spawnprotectiontime\" - amount of time in seconds a player is protected from damage\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_spawnweaponlist \"weapon_9mmhandgun\"\" A semicolon separated list of the player's spawn weapons\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_spawnweapons\" - Spawn weapons or not\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_startwithall [0|1]\" - Start with all weapons\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_startwithlives\" - Sets the starting lifes during battle royale\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mp_voting [time]\" - enable or disable end of the map voting with timer\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_acrobatics [0|1]\" allow or disallow wall climbing, slides, and flips\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_botsmelee [0|1]\" - enable or disable bot close combat\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_breakabletime\" - amount of seconds before a breakable entity respawns\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_chaosfilter\" - a list of mutators inwhich are ignored during chaos mode\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_disallowlist\" a list of entities that will not spawn\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_infiniteammo [0|1|2]\" - Infinite ammo ala CS 1.6\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_jumpheight 45\" - Adjust the player's jump height\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_mutatoramount [3 (0-7)]\" - how many mutators are rotated during chaos mode\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_mutatorlist\" - semicolon separated list of mutators that are added in sequence\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_mutatortime [30 (10-120)]\" - how long mutator lasts in seconds (approx)\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_addmutator [mutator]\" -> add available mutators\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"chaos\" - randomly selects number of mutators with sv_mutatorcount!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"999\" - start with 999 health and battery.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"astronaut\" - gravity is turned down\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"autoaim\" - all weapons have extreme autoaim.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "For more, type help_server_more\n" );
	}
	else if ( FStrEq( pcmd, "help_server_more" )  )
	{
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "[Server Help More]\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"barrels\" - start with the gravitygun, flaming explosive barrels spawn to throw at others\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"berserker\" - go crazy with chainsaws and fists.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"bigfoot\" - step size is crazy big\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"bighead\" - players heads are very large\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"busters\" - I ain't afraid of ghosts\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"chumxplode\" - a killer chumtoad appears directly after an explosion\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"closeup\" - ironsights are locked in\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"coolflesh\" - gibs stay longer, pick up gibs to eat and gain a healthkit worth of repair\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"cowboy\" - cowboy hats with dual cannons!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"crate\" - boxwars in the 2020's.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"credits\" - show all contributors in an endless loop.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"dealter\" - everyone farts.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"dontshoot\" - firing any weapon will explode the player.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"explosiveai\" - AI blows up when it cannot find its next task.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"fastweapons\" - all weapons are faster.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"firebullets\" - all bullets bring the fire.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"firestarter\" - start with a flamethrower.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"fog\" - heavy fog moves into the gameplay.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"godmode\" - everyone is invincible.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"goldenguns\" - guns provide one shot frags.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"grenades\" - a random chance of a grenade throw on attack.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"halflife\" - model skins turn original.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"ice\" - all the ground is covered in ice.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"infiniteammo\" - all weapons have infinite ammo.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"instagib\" - spawn with a zapgun that dole one hit kills.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"inverse\" - colors are inverted.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"invisible\" - everyone is partially invisible!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"itemsexplode\" - weapons and items react to explosions.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"jack\" - we don't make it 'til you order it.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"jeepathon\" - everyone is a jeep.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"jope\" - it's all a jope!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"lightsout\" - all the lights are turned out, but your flashight has unlimited battery.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"longjump\" - everyone receives a long jump module.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"loopback\" - teleport to the place of your last frag.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"marshmellow\" - come back 1999 to you.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"maxpack\" - drop all weapons and ammo in play (mp only!)\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"megarun\" - run faster than normal.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"minime\" - players are half the size.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"mirror\" - mirror, mirror on the screen.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"napkinstory\" - the legendary story reduced to two words.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"noclip\" - fly through walls.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"notify\" - interrupt players with annoying chat notifications.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"notthebees\" - hornets spawn from a player or monster who was killed.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"oldtime\" - gameplay becomes desaturated.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"paintball\" - weapons and explosions leave paint decals.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"paper\" - models are thin like paper.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"piratehat\" - argh, maty.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"plumber\" - spawn with dual pipe wrenches.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"portal\" - now you're thinking with portals.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"pumpkin\" - on Halloween, he appears.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"pushy\" - all weapon attacks push you back like a gauss attack.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"railguns\" - quake 2 is back.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"randomweapon\" - spawn with a randomly selected weapon.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"ricochet\" - the best mod ever made.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"rocketbees\" - hornets are explosive.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"rocketcrowbar\" - spawn with a rocket crowbar, makes all rockets act drunk.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"rockets\" - a random chance of rockets throw on attack!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sanic\" - evil santa and sanic with a santa hat team up against you.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"santahat\" - everyone wears a santa hat and says hohoho randomly.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sildenafil\" - screen goes blue.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"slowbullets\" - bullets travel slowly to target.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"slowmo\" - everything is slowed down by half! (sp only!)\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"slowweapons\" - all weapons are slower.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"snowballs\" - a random chance of snowballs throw on attack.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"speedup\" - everything is sped up by half! (sp only!)\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"stahp\" - some sounds replaced with stahp!\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"superjump\" - jump three times the height, disables fall damage.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"thirdperson\" - an outside body experience.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"three\" - three random mutators at once.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"topsyturvy\" - everything is turned upside down (sp only!)\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"toilet\" - game is in the toilet, we ain't hurt nobody.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"turrets\" - sentry guns randomly appear, firing bullets and rockets at everyone.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"vested\" - all players get a vest.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"volatile\" - where players blow up when fragged.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_instantmutators [0|1]\" - enable or disable instant mutators.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "\"sv_weather [0|1]\" allow or disallow weather effects on server.\n");
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "For more, see readme.txt\n" );
	}
#ifdef _DEBUG
	else if (FStrEq( pcmd, "rtx" ))
	{
		CBaseEntity *breakable = UTIL_FindEntityByClassname(NULL, "func_breakable");
		while (breakable != NULL)
		{
			((CBreakable *)breakable)->Restart();
			breakable = UTIL_FindEntityByClassname(breakable, "func_breakable");
		}
	}
	else if (FStrEq( pcmd, "edicts" )  )
	{
		ALERT(at_console, "[NUMBER_OF_ENTITIES()=%d , gpGlobals->maxEntities=%d]\n", NUMBER_OF_ENTITIES(), gpGlobals->maxEntities);
		ALERT(at_console, "[gpGlobals->maxClients=%d]\n", gpGlobals->maxClients);
	}
#endif
	else if ( g_pGameRules->ClientCommand( GetClassPtr((CBasePlayer *)pev), pcmd ) )
	{
		// MenuSelect returns true only if the command is properly handled,  so don't print a warning
	}
	else
	{
		// tell the user they entered an unknown command
		char command[128];

		// check the length of the command (prevents crash)
		// max total length is 192 ...and we're adding a string below ("Unknown command: %s\n")
		strncpy( command, pcmd, 127 );
		command[127] = '\0';

		// tell the user they entered an unknown command
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs( "Unknown command: %s\n", command ) );
	}
}


/*
========================
ClientUserInfoChanged

called after the player changes
userinfo - gives dll a chance to modify it before
it gets sent into the rest of the engine.
========================
*/
void ClientUserInfoChanged( edict_t *pEntity, char *infobuffer )
{
	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	CBasePlayer *pl = GetClassPtr((CBasePlayer *)&pEntity->v);

	char* pszAutoWepSwitch = g_engfuncs.pfnInfoKeyValue( infobuffer, "cl_aws");
	if (strlen(pszAutoWepSwitch))
		pl->m_iAutoWepSwitch = atoi(pszAutoWepSwitch);

	char* pszAutoWepThrow = g_engfuncs.pfnInfoKeyValue( infobuffer, "cl_awt");
	if (strlen(pszAutoWepThrow))
		pl->m_iAutoWepThrow = atoi(pszAutoWepThrow);

	char* pszDisplayInfoMessage = g_engfuncs.pfnInfoKeyValue( infobuffer, "cl_minfo");
	if (strlen(pszDisplayInfoMessage))
		pl->m_iDisplayInfoMessage = atoi(pszDisplayInfoMessage);

	char* pszKeyboardAcrobatics = g_engfuncs.pfnInfoKeyValue(infobuffer, "cl_kacro");
	if (strlen(pszKeyboardAcrobatics))
		pl->m_iKeyboardAcrobatics = atoi(pszKeyboardAcrobatics);

	char* pszAutoMelee = g_engfuncs.pfnInfoKeyValue( infobuffer, "cl_am");
	if (strlen(pszAutoMelee))
		pl->m_iAutoMelee = atoi(pszAutoMelee);

	char* pszAutoTaunt = g_engfuncs.pfnInfoKeyValue( infobuffer, "cl_at");
	if (strlen(pszAutoTaunt))
		pl->m_iAutoTaunt = atoi(pszAutoTaunt);

	char* pszPlayMusic = g_engfuncs.pfnInfoKeyValue( infobuffer, "cl_music");
	if (strlen(pszPlayMusic))
	{
		int newvalue = atoi(pszPlayMusic);

		if (pl->m_iPlayMusic != newvalue)
		{
			CBaseEntity *pT = UTIL_FindEntityByClassname( NULL, "trigger_mp3audio");
			if ( pT && pT->edict() )
			{
				pT->Use(pl, pl, USE_ON, 0);
			}
		}

		pl->m_iPlayMusic = atoi(pszPlayMusic);
	}

	// msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
	if ( pEntity->v.netname && STRING(pEntity->v.netname)[0] != 0 && !FStrEq( STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" )) )
	{
		char sName[256];
		char *pName = g_engfuncs.pfnInfoKeyValue( infobuffer, "name" );
		strncpy( sName, pName, sizeof(sName) - 1 );
		sName[ sizeof(sName) - 1 ] = '\0';

		// First parse the name and remove any %'s
		for ( char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++ )
		{
			// Replace it with a space
			if ( *pApersand == '%' )
				*pApersand = ' ';
		}

		// Set the name
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX(pEntity), infobuffer, "name", sName );

		if (gpGlobals->maxClients > 1)
		{
			char text[256];
			sprintf( text, "* %s changed name to %s\n", STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
			MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
				WRITE_BYTE( ENTINDEX(pEntity) );
				WRITE_STRING( text );
			MESSAGE_END();
		}

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed name to \"%s\"\n", 
				STRING( pEntity->v.netname ), 
				GETPLAYERUSERID( pEntity ), 
				GETPLAYERAUTHID( pEntity ),
				g_engfuncs.pfnInfoKeyValue( infobuffer, "model" ), 
				g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" changed name to \"%s\"\n", 
				STRING( pEntity->v.netname ), 
				GETPLAYERUSERID( pEntity ), 
				GETPLAYERAUTHID( pEntity ),
				GETPLAYERUSERID( pEntity ), 
				g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		}
	}

	g_pGameRules->ClientUserInfoChanged( GetClassPtr((CBasePlayer *)&pEntity->v), infobuffer );
}

static int g_serveractive = 0;

void ServerDeactivate( void )
{
	// It's possible that the engine will call this function more times than is necessary
	//  Therefore, only run it one time for each call to ServerActivate 
	if ( g_serveractive != 1 )
	{
		return;
	}

	g_serveractive = 0;

	// Peform any shutdown operations here...
	//

	// Reset fog when reloading
	CEnvFog::SetCurrentEndDist(0, 0);
}

void ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
	int				i;
	CBaseEntity		*pClass;

	// Every call to ServerActivate should be matched by a call to ServerDeactivate
	g_serveractive = 1;

	// Clients have not been initialized yet
	for ( i = 0; i < edictCount; i++ )
	{
		if ( pEdictList[i].free )
			continue;
		
		// Clients aren't necessarily initialized until ClientPutInServer()
		if ( i < clientMax || !pEdictList[i].pvPrivateData )
			continue;

		pClass = CBaseEntity::Instance( &pEdictList[i] );
		// Activate this entity if it's got a class & isn't dormant
		if ( pClass && !(pClass->pev->flags & FL_DORMANT) )
		{
			pClass->Activate();
		}
		else
		{
			ALERT( at_console, "Can't instance %s\n", STRING(pEdictList[i].v.classname) );
		}
	}

	// Link user messages here to make sure first client can get them...
	LinkUserMessages();
}


/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
void PlayerPreThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PreThink( );
}

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void PlayerPostThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PostThink( );
}



void ParmsNewLevel( void )
{
}


void ParmsChangeLevel( void )
{
	// retrieve the pointer to the save data
	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;

	if ( pSaveData )
		pSaveData->connectionCount = BuildChangeList( pSaveData->levelList, MAX_LEVEL_CONNECTIONS );
}


//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
void StartFrame( void )
{
	if ( g_pGameRules )
		g_pGameRules->Think();

	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = teamplay.value;
	g_ulFrameCount++;

	CEnvFog::FogThink();
}


void ClientPrecache( void )
{
	// setup precaches always needed
	PRECACHE_SOUND("sprayer.wav");			// spray paint sound for PreAlpha
	
	// PRECACHE_SOUND("player/pl_jumpland2.wav");		// UNDONE: play 2x step sound
	
	PRECACHE_SOUND("player/pl_fallpain2.wav");		
	PRECACHE_SOUND("player/pl_fallpain3.wav");		
	
	PRECACHE_SOUND("player/pl_step1.wav");		// walk on concrete
	PRECACHE_SOUND("player/pl_step2.wav");
	PRECACHE_SOUND("player/pl_step3.wav");
	PRECACHE_SOUND("player/pl_step4.wav");

	PRECACHE_SOUND("common/npc_step1.wav");		// NPC walk on concrete
	PRECACHE_SOUND("common/npc_step2.wav");
	PRECACHE_SOUND("common/npc_step3.wav");
	PRECACHE_SOUND("common/npc_step4.wav");

	PRECACHE_SOUND("player/pl_metal1.wav");		// walk on metal
	PRECACHE_SOUND("player/pl_metal2.wav");
	PRECACHE_SOUND("player/pl_metal3.wav");
	PRECACHE_SOUND("player/pl_metal4.wav");

	PRECACHE_SOUND("player/pl_dirt1.wav");		// walk on dirt
	PRECACHE_SOUND("player/pl_dirt2.wav");
	PRECACHE_SOUND("player/pl_dirt3.wav");
	PRECACHE_SOUND("player/pl_dirt4.wav");

	PRECACHE_SOUND("player/pl_duct1.wav");		// walk in duct
	PRECACHE_SOUND("player/pl_duct2.wav");
	PRECACHE_SOUND("player/pl_duct3.wav");
	PRECACHE_SOUND("player/pl_duct4.wav");

	PRECACHE_SOUND("player/pl_grate1.wav");		// walk on grate
	PRECACHE_SOUND("player/pl_grate2.wav");
	PRECACHE_SOUND("player/pl_grate3.wav");
	PRECACHE_SOUND("player/pl_grate4.wav");

	PRECACHE_SOUND("player/pl_slosh1.wav");		// walk in shallow water
	PRECACHE_SOUND("player/pl_slosh2.wav");
	PRECACHE_SOUND("player/pl_slosh3.wav");
	PRECACHE_SOUND("player/pl_slosh4.wav");

	PRECACHE_SOUND("player/pl_tile1.wav");		// walk on tile
	PRECACHE_SOUND("player/pl_tile2.wav");
	PRECACHE_SOUND("player/pl_tile3.wav");
	PRECACHE_SOUND("player/pl_tile4.wav");
	PRECACHE_SOUND("player/pl_tile5.wav");

	PRECACHE_SOUND("player/pl_snow1.wav");		// walk on snow
	PRECACHE_SOUND("player/pl_snow2.wav");
	PRECACHE_SOUND("player/pl_snow3.wav");
	PRECACHE_SOUND("player/pl_snow4.wav");

	PRECACHE_SOUND("player/pl_swim1.wav");		// breathe bubbles
	PRECACHE_SOUND("player/pl_swim2.wav");
	PRECACHE_SOUND("player/pl_swim3.wav");
	PRECACHE_SOUND("player/pl_swim4.wav");

	PRECACHE_SOUND("player/pl_ladder1.wav");	// climb ladder rung
	PRECACHE_SOUND("player/pl_ladder2.wav");
	PRECACHE_SOUND("player/pl_ladder3.wav");
	PRECACHE_SOUND("player/pl_ladder4.wav");

	PRECACHE_SOUND("player/pl_wade1.wav");		// wade in water
	PRECACHE_SOUND("player/pl_wade2.wav");
	PRECACHE_SOUND("player/pl_wade3.wav");
	PRECACHE_SOUND("player/pl_wade4.wav");

	PRECACHE_SOUND("debris/wood1.wav");			// hit wood texture
	PRECACHE_SOUND("debris/wood2.wav");
	PRECACHE_SOUND("debris/wood3.wav");

	PRECACHE_SOUND("plats/train_use1.wav");		// use a train

	PRECACHE_SOUND("buttons/spark5.wav");		// hit computer texture
	PRECACHE_SOUND("buttons/spark6.wav");
	PRECACHE_SOUND("debris/glass1.wav");
	PRECACHE_SOUND("debris/glass2.wav");
	PRECACHE_SOUND("debris/glass3.wav");

	PRECACHE_SOUND( SOUND_FLASHLIGHT_ON );
	PRECACHE_SOUND( SOUND_FLASHLIGHT_OFF );

// player gib sounds
	PRECACHE_SOUND("common/bodysplat.wav");		               

// player pain sounds
	PRECACHE_SOUND("player/pl_pain2.wav");
	PRECACHE_SOUND("player/pl_pain4.wav");
	PRECACHE_SOUND("player/pl_pain5.wav");
	PRECACHE_SOUND("player/pl_pain6.wav");
	PRECACHE_SOUND("player/pl_pain7.wav");

	PRECACHE_SOUND("scientist/scream1.wav");
	PRECACHE_SOUND("scientist/sci_pain3.wav"); // stahp!
	PRECACHE_SOUND("napkin_story.wav");

	PRECACHE_MODEL("models/player.mdl");
	PRECACHE_MODEL("models/player/iceman/iceman.mdl");
	PRECACHE_MODEL("models/hats.mdl");
	PRECACHE_MODEL("models/box.mdl");

	// hud sounds

	//PRECACHE_SOUND("wpn_hudoff.wav");
	//PRECACHE_SOUND("wpn_hudon.wav");
	//PRECACHE_SOUND("wpn_moveselect.wav");
	PRECACHE_SOUND("wpn_select.wav");
	PRECACHE_SOUND("point.wav");
	PRECACHE_SOUND("ding.wav");
	PRECACHE_SOUND("common/wpn_denyselect.wav");

	PRECACHE_SOUND("taunt01.wav");
	PRECACHE_SOUND("taunt02.wav");
	PRECACHE_SOUND("taunt03.wav");
	PRECACHE_SOUND("taunt04.wav");
	PRECACHE_SOUND("taunt05.wav");

	// geiger sounds

	PRECACHE_SOUND("player/geiger6.wav");
	PRECACHE_SOUND("player/geiger5.wav");
	PRECACHE_SOUND("player/geiger4.wav");
	PRECACHE_SOUND("player/geiger3.wav");
	PRECACHE_SOUND("player/geiger2.wav");
	PRECACHE_SOUND("player/geiger1.wav");

	PRECACHE_SOUND("taunt_blah.wav");

	PRECACHE_SOUND("heaven.wav");
	PRECACHE_SOUND("odetojoy.wav");

	if (giPrecacheGrunt)
		UTIL_PrecacheOther("monster_human_grunt");

	PRECACHE_SOUND("slide_on_gravel.wav");
}

/*
===============
GetGameDescription

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return "Cold Ice Remastered";
}

/*
================
Sys_Error

Engine is going to shut down, allows setting a breakpoint in game .dll to catch that occasion
================
*/
void Sys_Error( const char *error_string )
{
#ifdef _DEBUG
	ALERT(at_console, "Engine error: %s\n", error_string);
	ALERT(at_console, "Map: %s\n", STRING(gpGlobals->mapname));
	// ALERT(at_console, "Mutators: %s\n", mutators.string);
#endif
	// Default case, do nothing.  MOD AUTHORS:  Add code ( e.g., _asm { int 3 }; here to cause a breakpoint for debugging your game .dlls
}

/*
================
PlayerCustomization

A new player customization has been registered on the server
UNDONE:  This only sets the # of frames of the spray can logo
animation right now.
================
*/
void PlayerCustomization( edict_t *pEntity, customization_t *pCust )
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (!pPlayer)
	{
		ALERT(at_console, "PlayerCustomization:  Couldn't get player!\n");
		return;
	}

	if (!pCust)
	{
		ALERT(at_console, "PlayerCustomization:  NULL customization!\n");
		return;
	}

	switch (pCust->resource.type)
	{
	case t_decal:
		pPlayer->SetCustomDecalFrames(pCust->nUserData2); // Second int is max # of frames.
		break;
	case t_sound:
	case t_skin:
	case t_model:
		// Ignore for now.
		break;
	default:
		ALERT(at_console, "PlayerCustomization:  Unknown customization type!\n");
		break;
	}
}

/*
================
SpectatorConnect

A spectator has joined the game
================
*/
void SpectatorConnect( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorConnect( );
}

/*
================
SpectatorConnect

A spectator has left the game
================
*/
void SpectatorDisconnect( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorDisconnect( );
}

/*
================
SpectatorConnect

A spectator has sent a usercmd
================
*/
void SpectatorThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorThink( );
}

////////////////////////////////////////////////////////
// PAS and PVS routines for client messaging
//

/*
================
SetupVisibility

A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
view entity.  If that's the case, then pViewEntity will be non-NULL and will be used.  Otherwise, the current
entity's origin is used.  Either is offset by the view_ofs to get the eye position.

From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.  At this point, we could
 override the actual PAS or PVS values, or use a different origin.

NOTE:  Do not cache the values of pas and pvs, as they depend on reusable memory in the engine, they are only good for this one frame
================
*/
void SetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char **pvs, unsigned char **pas )
{
	Vector org;
	edict_t *pView = pClient;

	// Find the client's PVS
	if ( pViewEntity )
	{
		pView = pViewEntity;
	}

	if ( pClient->v.flags & FL_PROXY )
	{
		*pvs = NULL;	// the spectator proxy sees
		*pas = NULL;	// and hears everything
		return;
	}

	org = pView->v.origin + pView->v.view_ofs;
	if ( pView->v.flags & FL_DUCKING )
	{
		org = org + ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
	}

	*pvs = ENGINE_SET_PVS ( (float *)&org );
	*pas = ENGINE_SET_PAS ( (float *)&org );
}

#include "entity_state.h"

/*
AddToFullPack

Return 1 if the entity state has been filled in for the ent and the entity will be propagated to the client, 0 otherwise

state is the server maintained copy of the state info that is transmitted to the client
a MOD could alter values copied into state to send the "host" a different look for a particular entity update, etc.
e and ent are the entity that is being added to the update, if 1 is returned
host is the player's edict of the player whom we are sending the update to
player is 1 if the ent/e is a player and 0 otherwise
pSet is either the PAS or PVS that we previous set up.  We can use it to ask the engine to filter the entity against the PAS or PVS.
we could also use the pas/ pvs that we set in SetupVisibility, if we wanted to.  Caching the value is valid in that case, but still only for the current frame
*/
int AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet )
{
	// Entities with an index greater than this will corrupt the client's heap because 
	// the index is sent with only 11 bits of precision (2^11 == 2048).
	// So we don't send them, just like having too many entities would result
	// in the entity not being sent.
	if (e >= MAX_EDICTS)
	{
		return 0;
	}

	int					i;

	// don't send if flagged for NODRAW and it's not the host getting the message
	if ( ( ent->v.effects & EF_NODRAW ) &&
		 ( ent != host ) )
		return 0;

	// Ignore ents without valid / visible models
	if ( !ent->v.modelindex || !STRING( ent->v.model ) )
		return 0;

	// Don't send spectators to other players
	if ( ( ent->v.flags & FL_SPECTATOR ) && ( ent != host ) )
	{
		return 0;
	}

	// Ignore if not the host and not touching a PVS/PAS leaf
	// If pSet is NULL, then the test will always succeed and the entity will be added to the update
	if ( ent != host )
	{
		if ( !ENGINE_CHECK_VISIBILITY( (const struct edict_s *)ent, pSet ) )
		{
			return 0;
		}
	}

	if ( CEnvFog::CheckBBox( host, ent ) )
	{
		return 0;
	}

	// Don't send entity to local client if the client says it's predicting the entity itself.
	if ( ent->v.flags & FL_SKIPLOCALHOST )
	{
		if ( ( hostflags & 1 ) && ( ent->v.owner == host ) )
			return 0;
	}
	
	if ( host->v.groupinfo )
	{
		UTIL_SetGroupTrace( host->v.groupinfo, GROUP_OP_AND );

		// Should always be set, of course
		if ( ent->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND )
			{
				if ( !(ent->v.groupinfo & host->v.groupinfo ) )
					return 0;
			}
			else if ( g_groupop == GROUP_OP_NAND )
			{
				if ( ent->v.groupinfo & host->v.groupinfo )
					return 0;
			}
		}

		UTIL_UnsetGroupTrace();
	}

	memset( state, 0, sizeof( *state ) );

	// Assign index so we can track this entity from frame to frame and
	//  delta from it.
	state->number	  = e;
	state->entityType = ENTITY_NORMAL;
	
	// Flag custom entities.
	if ( ent->v.flags & FL_CUSTOMENTITY )
	{
		state->entityType = ENTITY_BEAM;
	}

	// 
	// Copy state data
	//

	// Round animtime to nearest millisecond
	state->animtime   = (int)(1000.0 * ent->v.animtime ) / 1000.0;

	memcpy( state->origin, ent->v.origin, 3 * sizeof( float ) );
	memcpy( state->angles, ent->v.angles, 3 * sizeof( float ) );
	memcpy( state->mins, ent->v.mins, 3 * sizeof( float ) );
	memcpy( state->maxs, ent->v.maxs, 3 * sizeof( float ) );

	memcpy( state->startpos, ent->v.startpos, 3 * sizeof( float ) );
	memcpy( state->endpos, ent->v.endpos, 3 * sizeof( float ) );

	state->impacttime = ent->v.impacttime;
	state->starttime = ent->v.starttime;

	state->modelindex = ent->v.modelindex;
		
	state->frame      = ent->v.frame;

	state->skin       = ent->v.skin;
	state->effects    = ent->v.effects;

	// This non-player entity is being moved by the game .dll and not the physics simulation system
	//  make sure that we interpolate it's position on the client if it moves
	if ( !player &&
		 ent->v.animtime &&
		 ent->v.velocity[ 0 ] == 0 && 
		 ent->v.velocity[ 1 ] == 0 && 
		 ent->v.velocity[ 2 ] == 0 )
	{
		state->eflags |= EFLAG_SLERP;
	}

	CBaseEntity* entity = reinterpret_cast<CBaseEntity*>(GET_PRIVATE(ent));
	if (entity != NULL)
	{
		if (g_pGameRules->MutatorEnabled(MUTATOR_PAINTBALL))
			entity->m_EFlags |= EFLAG_PAINTBALL;
		else
			entity->m_EFlags &= ~EFLAG_PAINTBALL;

		state->eflags = entity->m_EFlags;
	}

	state->scale	  = ent->v.scale;
	state->solid	  = ent->v.solid;
	state->colormap   = ent->v.colormap;

	state->movetype   = ent->v.movetype;
	state->sequence   = ent->v.sequence;
	state->framerate  = ent->v.framerate;
	state->body       = ent->v.body;

	for (i = 0; i < 4; i++)
	{
		state->controller[i] = ent->v.controller[i];
	}

	for (i = 0; i < 2; i++)
	{
		state->blending[i]   = ent->v.blending[i];
	}

	state->rendermode    = ent->v.rendermode;
	state->renderamt     = ent->v.renderamt; 
	state->renderfx      = ent->v.renderfx;
	state->rendercolor.r = ent->v.rendercolor.x;
	state->rendercolor.g = ent->v.rendercolor.y;
	state->rendercolor.b = ent->v.rendercolor.z;

	state->aiment = 0;
	if ( ent->v.aiment )
	{
		state->aiment = ENTINDEX( ent->v.aiment );
	}

	state->owner = 0;
	if ( ent->v.owner )
	{
		int owner = ENTINDEX( ent->v.owner );
		
		// Only care if owned by a player
		if ( owner >= 1 && owner <= gpGlobals->maxClients )
		{
			state->owner = owner;	
		}
	}

	// HACK:  Somewhat...
	// Class is overridden for non-players to signify a breakable glass object ( sort of a class? )
	//if ( !player )
	//{
		state->playerclass  = ent->v.playerclass;
	//}

	// Special stuff for players only
	if ( player )
	{
		memcpy( state->basevelocity, ent->v.basevelocity, 3 * sizeof( float ) );
		memcpy( state->velocity, ent->v.velocity, 3 * sizeof( float ) );

		state->weaponmodel  = MODEL_INDEX( STRING( ent->v.weaponmodel ) );
		state->gaitsequence = ent->v.gaitsequence;
		state->spectator = ent->v.flags & FL_SPECTATOR;
		state->friction     = ent->v.friction;

		state->gravity      = ent->v.gravity;
		state->team			= ent->v.team;
//		
		state->usehull      = ( ent->v.flags & FL_DUCKING ) ? 1 : 0;
		state->health		= ent->v.health; // radar
	}

	// Entities and players
	state->fuser4 = ent->v.fuser4; // target/special

	return 1;
}

// defaults for clientinfo messages
#define	DEFAULT_VIEWHEIGHT	28

/*
===================
CreateBaseline

Creates baselines used for network encoding, especially for player data since players are not spawned until connect time.
===================
*/
void CreateBaseline( int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs )
{
	baseline->origin		= entity->v.origin;
	baseline->angles		= entity->v.angles;
	baseline->frame			= entity->v.frame;
	baseline->skin			= (short)entity->v.skin;

	// render information
	baseline->rendermode	= (byte)entity->v.rendermode;
	baseline->renderamt		= (byte)entity->v.renderamt;
	baseline->rendercolor.r	= (byte)entity->v.rendercolor.x;
	baseline->rendercolor.g	= (byte)entity->v.rendercolor.y;
	baseline->rendercolor.b	= (byte)entity->v.rendercolor.z;
	baseline->renderfx		= (byte)entity->v.renderfx;

	if ( player )
	{
		baseline->mins			= player_mins;
		baseline->maxs			= player_maxs;

		baseline->colormap		= eindex;
		baseline->modelindex	= playermodelindex;
		baseline->friction		= 1.0;
		baseline->movetype		= MOVETYPE_WALK;

		baseline->scale			= entity->v.scale;
		baseline->solid			= SOLID_SLIDEBOX;
		baseline->framerate		= 1.0;
		baseline->gravity		= 1.0;

	}
	else
	{
		baseline->mins			= entity->v.mins;
		baseline->maxs			= entity->v.maxs;

		baseline->colormap		= 0;
		baseline->modelindex	= entity->v.modelindex;//SV_ModelIndex(pr_strings + entity->v.model);
		baseline->movetype		= entity->v.movetype;

		baseline->scale			= entity->v.scale;
		baseline->solid			= entity->v.solid;
		baseline->framerate		= entity->v.framerate;
		baseline->gravity		= entity->v.gravity;
	}
}

typedef struct
{
	char name[32];
	int	 field;
} entity_field_alias_t;

#define FIELD_ORIGIN0			0
#define FIELD_ORIGIN1			1
#define FIELD_ORIGIN2			2
#define FIELD_ANGLES0			3
#define FIELD_ANGLES1			4
#define FIELD_ANGLES2			5

static entity_field_alias_t entity_field_alias[]=
{
	{ "origin[0]",			0 },
	{ "origin[1]",			0 },
	{ "origin[2]",			0 },
	{ "angles[0]",			0 },
	{ "angles[1]",			0 },
	{ "angles[2]",			0 },
};

void Entity_FieldInit( struct delta_s *pFields )
{
	entity_field_alias[ FIELD_ORIGIN0 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ORIGIN0 ].name );
	entity_field_alias[ FIELD_ORIGIN1 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ORIGIN1 ].name );
	entity_field_alias[ FIELD_ORIGIN2 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ORIGIN2 ].name );
	entity_field_alias[ FIELD_ANGLES0 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ANGLES0 ].name );
	entity_field_alias[ FIELD_ANGLES1 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ANGLES1 ].name );
	entity_field_alias[ FIELD_ANGLES2 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ANGLES2 ].name );
}

/*
==================
Entity_Encode

Callback for sending entity_state_t info over network. 
FIXME:  Move to script
==================
*/
void Entity_Encode( struct delta_s *pFields, const unsigned char *from, const unsigned char *to )
{
	entity_state_t *f, *t;
	int localplayer = 0;
	static int initialized = 0;

	if ( !initialized )
	{
		Entity_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	localplayer =  ( t->number - 1 ) == ENGINE_CURRENT_PLAYER();
	if ( localplayer )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}

	if ( ( t->impacttime != 0 ) && ( t->starttime != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );

		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ANGLES0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ANGLES1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ANGLES2 ].field );
	}

	if ( ( t->movetype == MOVETYPE_FOLLOW ) &&
		 ( t->aiment != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
	else if ( t->aiment != f->aiment )
	{
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
}

static entity_field_alias_t player_field_alias[]=
{
	{ "origin[0]",			0 },
	{ "origin[1]",			0 },
	{ "origin[2]",			0 },
};

void Player_FieldInit( struct delta_s *pFields )
{
	player_field_alias[ FIELD_ORIGIN0 ].field		= DELTA_FINDFIELD( pFields, player_field_alias[ FIELD_ORIGIN0 ].name );
	player_field_alias[ FIELD_ORIGIN1 ].field		= DELTA_FINDFIELD( pFields, player_field_alias[ FIELD_ORIGIN1 ].name );
	player_field_alias[ FIELD_ORIGIN2 ].field		= DELTA_FINDFIELD( pFields, player_field_alias[ FIELD_ORIGIN2 ].name );
}

/*
==================
Player_Encode

Callback for sending entity_state_t for players info over network. 
==================
*/
void Player_Encode( struct delta_s *pFields, const unsigned char *from, const unsigned char *to )
{
	entity_state_t *f, *t;
	int localplayer = 0;
	static int initialized = 0;

	if ( !initialized )
	{
		Player_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	localplayer =  ( t->number - 1 ) == ENGINE_CURRENT_PLAYER();
	if ( localplayer )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}

	if ( ( t->movetype == MOVETYPE_FOLLOW ) &&
		 ( t->aiment != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
	else if ( t->aiment != f->aiment )
	{
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
}

#define CUSTOMFIELD_ORIGIN0			0
#define CUSTOMFIELD_ORIGIN1			1
#define CUSTOMFIELD_ORIGIN2			2
#define CUSTOMFIELD_ANGLES0			3
#define CUSTOMFIELD_ANGLES1			4
#define CUSTOMFIELD_ANGLES2			5
#define CUSTOMFIELD_SKIN			6
#define CUSTOMFIELD_SEQUENCE		7
#define CUSTOMFIELD_ANIMTIME		8

entity_field_alias_t custom_entity_field_alias[]=
{
	{ "origin[0]",			0 },
	{ "origin[1]",			0 },
	{ "origin[2]",			0 },
	{ "angles[0]",			0 },
	{ "angles[1]",			0 },
	{ "angles[2]",			0 },
	{ "skin",				0 },
	{ "sequence",			0 },
	{ "animtime",			0 },
};

void Custom_Entity_FieldInit( struct delta_s *pFields )
{
	custom_entity_field_alias[ CUSTOMFIELD_ORIGIN0 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN0 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ORIGIN1 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN1 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ORIGIN2 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN2 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANGLES0 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES0 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANGLES1 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES1 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANGLES2 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES2 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_SKIN ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_SKIN ].name );
	custom_entity_field_alias[ CUSTOMFIELD_SEQUENCE ].field= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_SEQUENCE ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANIMTIME ].field= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANIMTIME ].name );
}

/*
==================
Custom_Encode

Callback for sending entity_state_t info ( for custom entities ) over network. 
FIXME:  Move to script
==================
*/
void Custom_Encode( struct delta_s *pFields, const unsigned char *from, const unsigned char *to )
{
	entity_state_t *f, *t;
	int beamType;
	static int initialized = 0;

	if ( !initialized )
	{
		Custom_Entity_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	beamType = t->rendermode & 0x0f;
		
	if ( beamType != BEAM_POINTS && beamType != BEAM_ENTPOINT )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN2 ].field );
	}

	if ( beamType != BEAM_POINTS )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES0 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES1 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES2 ].field );
	}

	if ( beamType != BEAM_ENTS && beamType != BEAM_ENTPOINT )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_SKIN ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_SEQUENCE ].field );
	}

	// animtime is compared by rounding first
	// see if we really shouldn't actually send it
	if ( (int)f->animtime == (int)t->animtime )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANIMTIME ].field );
	}
}

/*
=================
RegisterEncoders

Allows game .dll to override network encoding of certain types of entities and tweak values, etc.
=================
*/
void RegisterEncoders( void )
{
	DELTA_ADDENCODER( "Entity_Encode", Entity_Encode );
	DELTA_ADDENCODER( "Custom_Encode", Custom_Encode );
	DELTA_ADDENCODER( "Player_Encode", Player_Encode );
}

int GetWeaponData( struct edict_s *player, struct weapon_data_s *info )
{
#if defined( CLIENT_WEAPONS )
	int i;
	weapon_data_t *item;
	entvars_t *pev = &player->v;
	CBasePlayer *pl = dynamic_cast< CBasePlayer *>( CBasePlayer::Instance( pev ) );
	CBasePlayerWeapon *gun;
	
	ItemInfo II;

	memset( info, 0, MAX_WEAPONS * sizeof( weapon_data_t ) );

	if ( !pl )
		return 1;

	// go through all of the weapons and make a list of the ones to pack
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( pl->m_rgpPlayerItems[ i ] )
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem *pPlayerItem = pl->m_rgpPlayerItems[ i ];

			while ( pPlayerItem )
			{
				gun = dynamic_cast<CBasePlayerWeapon *>( pPlayerItem->GetWeaponPtr() );
				if ( gun && gun->UseDecrement() )
				{
					// Get The ID.
					memset( &II, 0, sizeof( II ) );
					gun->GetItemInfo( &II );

					if ( II.iId >= 0 && II.iId < MAX_WEAPONS )
					{
						item = &info[ II.iId ];
					 	
						item->m_iId						= II.iId;
						item->m_iClip					= gun->m_iClip;

						item->m_flTimeWeaponIdle		= fmax( gun->m_flTimeWeaponIdle, -0.001 );
						item->m_flNextPrimaryAttack		= fmax( gun->m_flNextPrimaryAttack, -0.001 );
						item->m_flNextSecondaryAttack	= fmax( gun->m_flNextSecondaryAttack, -0.001 );
						item->m_fInReload				= gun->m_fInReload;
						item->m_fInSpecialReload		= gun->m_fInSpecialReload;
						item->fuser1					= fmax( gun->pev->fuser1, -0.001 );
						item->fuser2					= gun->m_flStartThrow;
						item->fuser3					= gun->m_flReleaseThrow;
						item->iuser1					= gun->m_chargeReady;
						item->iuser2					= gun->m_fInAttack;
						item->iuser3					= gun->m_fireState;
						
											
//						item->m_flPumpTime				= fmax( gun->m_flPumpTime, -0.001 );
					}
				}
				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}
#else
	memset( info, 0, MAX_WEAPONS * sizeof( weapon_data_t ) );
#endif
	return 1;
}

/*
=================
UpdateClientData

Data sent to current client only
engine sets cd to 0 before calling.
=================
*/
void UpdateClientData ( const edict_t *ent, int sendweapons, struct clientdata_s *cd )
{
	if ( !ent || !ent->pvPrivateData )
		return;

	// No bots
	if (FBitSet(ent->v.flags, FL_FAKECLIENT))
		return;

	entvars_t *		pev	= (entvars_t *)&ent->v;
	CBasePlayer *	pl	= dynamic_cast< CBasePlayer *>(CBasePlayer::Instance( pev ));
	entvars_t *		pevOrg = NULL;

	if (pl)
		cd->iuser4 = pl->m_iWeapons2;

	// if user is spectating different player in First person, override some vars
	if ( pl && pl->pev->iuser1 == OBS_IN_EYE )
	{
		if ( pl->m_hObserverTarget )
		{
			pevOrg = pev;
			pev = pl->m_hObserverTarget->pev;
			pl = dynamic_cast< CBasePlayer *>(CBasePlayer::Instance( pev ) );
		}
	}

	cd->flags			= pev->flags;
	cd->health			= pev->health;

	cd->viewmodel		= MODEL_INDEX( STRING( pev->viewmodel ) );

	cd->waterlevel		= pev->waterlevel;
	cd->watertype		= pev->watertype;
	cd->weapons			= pev->weapons;

	// Vectors
	cd->origin			= pev->origin;
	cd->velocity		= pev->velocity;
	cd->view_ofs		= pev->view_ofs;
	cd->punchangle		= pev->punchangle;

	cd->bInDuck			= pev->bInDuck;
	cd->flTimeStepSound = pev->flTimeStepSound;
	cd->flDuckTime		= pev->flDuckTime;
	cd->flSwimTime		= pev->flSwimTime;
	cd->waterjumptime	= pev->teleport_time;

	strcpy( cd->physinfo, ENGINE_GETPHYSINFO( ent ) );

	cd->maxspeed		= pev->maxspeed;
	cd->fov				= pev->fov;
	cd->weaponanim		= pev->weaponanim;

	cd->pushmsec		= pev->pushmsec;

	//Spectator mode
	if ( pevOrg != NULL )
	{
		// don't use spec vars from chased player
		cd->iuser1			= pevOrg->iuser1;
		cd->iuser2			= pevOrg->iuser2;
	}
	else
	{
		cd->iuser1			= pev->iuser1;
		cd->iuser2			= pev->iuser2;
	}

	

#if defined( CLIENT_WEAPONS )
	if ( sendweapons )
	{
		if ( pl )
		{
			cd->m_flNextAttack	= pl->m_flNextAttack;
			cd->fuser2			= pl->m_flNextAmmoBurn;
			cd->fuser3			= pl->m_flAmmoStartCharge;
			cd->vuser1.x		= pl->ammo_9mm;
			cd->vuser1.y		= pl->ammo_357;
			cd->vuser1.z		= pl->ammo_argrens;
			cd->ammo_nails		= pl->ammo_bolts;
			cd->ammo_shells		= pl->ammo_buckshot;
			cd->ammo_rockets	= pl->ammo_rockets;
			cd->ammo_cells		= pl->ammo_uranium;
			cd->vuser2.x		= pl->ammo_hornets;
			

			if ( pl->m_pActiveItem )
			{
				CBasePlayerWeapon *gun;
				gun = (CBasePlayerWeapon *)pl->m_pActiveItem->GetWeaponPtr();
				if ( gun && gun->UseDecrement() )
				{
					ItemInfo II;
					memset( &II, 0, sizeof( II ) );
					gun->GetItemInfo( &II );

					cd->m_iId = II.iId;

					cd->vuser3.z	= gun->m_iSecondaryAmmoType;
					cd->vuser4.x	= gun->m_iPrimaryAmmoType;
					cd->vuser4.y	= pl->m_rgAmmo[gun->m_iPrimaryAmmoType];
					cd->vuser4.z	= pl->m_rgAmmo[gun->m_iSecondaryAmmoType];
					
					if ( pl->m_pActiveItem->m_iId == WEAPON_RPG )
					{
						cd->vuser2.y = ( ( CRpg * )pl->m_pActiveItem)->m_fSpotActive;
						cd->vuser2.z = ( ( CRpg * )pl->m_pActiveItem)->m_cActiveRockets;
					}
				}
			}
		}
	} 
#endif
}

/*
=================
CmdStart

We're about to run this usercmd for the specified player.  We can set up groupinfo and masking here, etc.
This is the time to examine the usercmd for anything extra.  This call happens even if think does not.
=================
*/
void CmdStart( const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed )
{
	entvars_t *pev = (entvars_t *)&player->v;
	CBasePlayer *pl = dynamic_cast< CBasePlayer *>( CBasePlayer::Instance( pev ) );

	if( !pl )
		return;

	if ( pl->pev->groupinfo != 0 )
	{
		UTIL_SetGroupTrace( pl->pev->groupinfo, GROUP_OP_AND );
	}

	pl->random_seed = random_seed;
}

/*
=================
CmdEnd

Each cmdstart is exactly matched with a cmd end, clean up any group trace flags, etc. here
=================
*/
void CmdEnd ( const edict_t *player )
{
	entvars_t *pev = (entvars_t *)&player->v;
	CBasePlayer *pl = dynamic_cast< CBasePlayer *>( CBasePlayer::Instance( pev ) );

	if( !pl )
		return;
	if ( pl->pev->groupinfo != 0 )
	{
		UTIL_UnsetGroupTrace();
	}
}

/*
================================
ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int	ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size )
{
	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

/*
================================
GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int GetHullBounds( int hullnumber, float *mins, float *maxs )
{
	int iret = 0;

	switch ( hullnumber )
	{
	case 0:				// Normal player
		mins = VEC_HULL_MIN;
		maxs = VEC_HULL_MAX;
		iret = 1;
		break;
	case 1:				// Crouched player
		mins = VEC_DUCK_HULL_MIN;
		maxs = VEC_DUCK_HULL_MAX;
		iret = 1;
		break;
	case 2:				// Point based hull
		mins = Vector( 0, 0, 0 );
		maxs = Vector( 0, 0, 0 );
		iret = 1;
		break;
	}

	return iret;
}

/*
================================
CreateInstancedBaselines

Create pseudo-baselines for items that aren't placed in the map at spawn time, but which are likely
to be created during play ( e.g., grenades, ammo packs, projectiles, corpses, etc. )
================================
*/
void CreateInstancedBaselines ( void )
{
	int iret = 0;
	entity_state_t state;

	memset( &state, 0, sizeof( state ) );

	// Create any additional baselines here for things like grendates, etc.
	// iret = ENGINE_INSTANCE_BASELINE( pc->pev->classname, &state );

	// Destroy objects.
	//UTIL_Remove( pc );
}

/*
================================
InconsistentFile

One of the ENGINE_FORCE_UNMODIFIED files failed the consistency check for the specified player
 Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
================================
*/
int	InconsistentFile( const edict_t *player, const char *filename, char *disconnect_message )
{
	// Server doesn't care?
	if ( CVAR_GET_FLOAT( "mp_consistency" ) != 1 )
		return 0;

	// Default behavior is to kick the player
	sprintf( disconnect_message, "Server is enforcing file consistency for %s\n", filename );

	// Kick now with specified disconnect message.
	return 1;
}

/*
================================
AllowLagCompensation

 The game .dll should return 1 if lag compensation should be allowed ( could also just set
  the sv_unlag cvar.
 Most games right now should return 0, until client-side weapon prediction code is written
  and tested for them ( note you can predict weapons, but not do lag compensation, too, 
  if you want.
================================
*/
int AllowLagCompensation( void )
{
	return 1;
}

/*
================================
ShouldCollide

  Called when the engine believes two entities are about to collide. Return 0 if you
  want the two entities to just pass through each other without colliding or calling the
  touch function.
================================
*/
int ShouldCollide( edict_t *pentTouched, edict_t *pentOther )
{
	CBaseEntity *pTouch = CBaseEntity::Instance( pentTouched );
	CBaseEntity *pOther = CBaseEntity::Instance( pentOther );

	if ( pTouch && pOther )
		return pOther->ShouldCollide( pTouch );

	return 1;
}
