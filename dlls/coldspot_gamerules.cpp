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
#include "coldspot_gamerules.h"
#include "game.h"
#include "items.h"
#include "shake.h"
#include "pm_shared.h"

extern int gmsgScoreInfo;
extern int gmsgTeamNames;
extern int gmsgTeamInfo;
extern int gmsgObjective;
extern int gmsgPlayClientSound;
extern int gmsgSpecialEntity;
extern int gmsgBanner;

extern DLL_GLOBAL BOOL g_fGameOver;

#define TEAM_BLUE 0
#define TEAM_RED 1

class CColdSpot : public CBaseEntity
{
public:
	static CColdSpot *CreateColdSpot( Vector vecOrigin, int body );
	void Precache( void );
	void Spawn( void );
	EXPORT void ColdSpotThink( void );
};

CColdSpot *CColdSpot::CreateColdSpot( Vector vecOrigin, int body )
{
	CColdSpot *pSpot = GetClassPtr( (CColdSpot *)NULL );
	UTIL_SetOrigin( pSpot->pev, vecOrigin );
	pSpot->pev->angles = g_vecZero;
	pSpot->Spawn();
	pSpot->pev->body = body;

	// Broadcast special entities to all clients for radar tracking
	int slotIndex = 0;
	MESSAGE_BEGIN(MSG_ALL, gmsgSpecialEntity);
		WRITE_BYTE(slotIndex); // Index 0-7
		WRITE_BYTE(1); // Active
		WRITE_COORD(pSpot->pev->origin.x);
		WRITE_COORD(pSpot->pev->origin.y);
		WRITE_COORD(pSpot->pev->origin.z);
		WRITE_BYTE(RADAR_COLD_SPOT); // Special type
	MESSAGE_END();
	slotIndex++;

	// Clear remaining slots
	for ( ; slotIndex < 8; slotIndex++ )
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgSpecialEntity);
			WRITE_BYTE(slotIndex);
			WRITE_BYTE(0); // Not active
		MESSAGE_END();
	}

	return pSpot;
}

void CColdSpot::Precache( void )
{
	PRECACHE_MODEL("models/coldspot.mdl");
}

void CColdSpot::Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), "models/coldspot.mdl");
	pev->classname = MAKE_STRING("coldspot");
	pev->fuser4 = RADAR_COLD_SPOT;

	pev->angles.x = 0;
	pev->angles.z = 0;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CColdSpot::ColdSpotThink );
	pev->nextthink = gpGlobals->time + 2.0;
}

void CColdSpot::ColdSpotThink( void )
{
	if (g_fGameOver)
		return;

	CBaseEntity *ent = NULL;
	while ( (ent = UTIL_FindEntityInSphere( ent, pev->origin, 256 )) != NULL )
	{
		if ( ent->IsPlayer() && ent->IsAlive() && !ent->pev->iuser1 )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)ent;
			MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
				WRITE_BYTE( ENTINDEX(ent->edict()) );
				WRITE_SHORT( pPlayer->pev->frags );
				WRITE_SHORT( pPlayer->m_iDeaths );
				WRITE_SHORT( ++pPlayer->m_iRoundWins );
				WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
			MESSAGE_END();
			
			UTIL_ScreenFade( ent, Vector(0, 255, 0), 0.25, 2, 32, FFADE_IN);
			((CHalfLifeColdSpot *)g_pGameRules)->UpdateHud();

			MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, pPlayer->edict() );
				WRITE_BYTE(CLIENT_SOUND_LEVEL_UP);
			MESSAGE_END();
			ClientPrint(pPlayer->pev, HUD_PRINTCENTER, UTIL_VarArgs("You Have Scored a Point!\n"));
		}
	}

	pev->nextthink = gpGlobals->time + 2.0;
}

LINK_ENTITY_TO_CLASS( coldspot, CColdSpot );

// --------

CHalfLifeColdSpot::CHalfLifeColdSpot()
{
	UTIL_PrecacheOther("coldspot");
	m_DisableDeathPenalty = FALSE;
	pColdSpot = NULL;
	pLastSpawnPoint = NULL;
	m_fSpawnColdSpot = gpGlobals->time + 2.0;
	m_fColdSpotTime = coldspottime.value;
	m_iBlueScore = m_iRedScore = 0;
}

BOOL CHalfLifeColdSpot::IsSpawnPointValid( CBaseEntity *pSpot )
{
	CBaseEntity *ent = NULL;

	while ( (ent = UTIL_FindEntityInSphere( ent, pSpot->pev->origin, 2048 )) != NULL )
	{
		// Is another spot in area?
		if (FClassnameIs(ent->pev, "coldspot"))
			return FALSE;
	}

	return TRUE;
}

edict_t *CHalfLifeColdSpot::EntSelectSpawnPoint( const char *szSpawnPoint )
{
	CBaseEntity *pSpot;

	// choose a point
	pSpot = pLastSpawnPoint;
	// Randomize the start spot
	for ( int i = RANDOM_LONG(1,5); i > 0; i-- )
		pSpot = UTIL_FindEntityByClassname( pSpot, szSpawnPoint );
	if ( FNullEnt( pSpot ) )  // skip over the null point
		pSpot = UTIL_FindEntityByClassname( pSpot, szSpawnPoint );

	CBaseEntity *pFirstSpot = pSpot;

	do
	{
		if ( pSpot )
		{
			// check if pSpot is valid
			if ( IsSpawnPointValid( pSpot ) )
			{
				// if so, go to pSpot
				goto ReturnSpot;
			}
		}
		// increment pSpot
		pSpot = UTIL_FindEntityByClassname( pSpot, szSpawnPoint );
	} while ( pSpot != pFirstSpot ); // loop if we're not back to the start

	// we haven't found a place to spawn yet
	if ( !FNullEnt( pSpot ) )
	{
		goto ReturnSpot;
	}

ReturnSpot:
	if ( FNullEnt( pSpot ) )
	{
		ALERT(at_error, "no cold spot on level");
		return INDEXENT(0);
	}

	pLastSpawnPoint = pSpot;
	return pSpot->edict();
}

void CHalfLifeColdSpot::AutoJoin( CBasePlayer *pPlayer, int team )
{
	int blueteam = 0, redteam = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
		{
			if (plr->pev->fuser4 == TEAM_BLUE)
				blueteam++;
			else
				redteam++;
		}
	}

	if (team > TEAM_RED)
		pPlayer->pev->fuser4 = redteam >= blueteam ? TEAM_BLUE : TEAM_RED;
	else
		pPlayer->pev->fuser4 = team;

	if (pPlayer->pev->fuser4 == TEAM_BLUE)
	{
		strncpy( pPlayer->m_szTeamName, "blue", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()),
			g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "iceman");
	}
	else
	{
		strncpy( pPlayer->m_szTeamName, "red", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()),
			g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "santa");
	}

	char text[64];
	sprintf( text, "[ColdSpot] You're on team \'%s\'\n", pPlayer->m_szTeamName );
	UTIL_SayText( text, pPlayer );

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

void CHalfLifeColdSpot::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
		WRITE_BYTE( 2 );
		WRITE_STRING( "blue" );
		WRITE_STRING( "red" );
	MESSAGE_END();

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Hold the cold spot");
			WRITE_STRING(UTIL_VarArgs("You're on %s team", (pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
			WRITE_BYTE(0);
		MESSAGE_END();
	}
}

void CHalfLifeColdSpot::Think( void )
{
	CHalfLifeMultiplay::Think();

	if ( g_fGameOver )
		return;

	if (coldspottime.value != m_fColdSpotTime)
		m_fColdSpotTime = m_fSpawnColdSpot = coldspottime.value;

	if (m_fSpawnColdSpot && m_fSpawnColdSpot < gpGlobals->time)
	{
		edict_t *pentSpawnSpot = EntSelectSpawnPoint(coldspotspawn.string);
		if (!pColdSpot)
			pColdSpot = CColdSpot::CreateColdSpot(pentSpawnSpot->v.origin, 0);
		else
		{
			UTIL_SetOrigin(pColdSpot->pev, pentSpawnSpot->v.origin);
			MESSAGE_BEGIN(MSG_ALL, gmsgSpecialEntity);
				WRITE_BYTE(0); // Index 0-7
				WRITE_BYTE(1); // Active
				WRITE_COORD(pColdSpot->pev->origin.x);
				WRITE_COORD(pColdSpot->pev->origin.y);
				WRITE_COORD(pColdSpot->pev->origin.z);
				WRITE_BYTE(RADAR_COLD_SPOT); // Special type
			MESSAGE_END();
		}

		MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
			WRITE_BYTE(CLIENT_SOUND_EBELL);
		MESSAGE_END();
		UTIL_ClientPrintAll(HUD_PRINTTALK, "[ColdSpot] The cold spot has moved!\n");

		if (!m_fColdSpotTime)
			m_fSpawnColdSpot = 0;
		else
			m_fSpawnColdSpot = gpGlobals->time + m_fColdSpotTime;
	}
}

void CHalfLifeColdSpot::PlayerThink( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerThink(pPlayer);

	if (pPlayer->m_iShowGameModeMessage > -1 &&
		pPlayer->m_iShowGameModeMessage < gpGlobals->time &&
		!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgBanner, NULL, pPlayer->edict());
			if (pPlayer->pev->fuser4 == TEAM_RED)
				WRITE_STRING("You're on Team Red!");
			else
				WRITE_STRING("You're on Team Blue!");
			WRITE_STRING("Find the cold spot and hold it to score points!");
			WRITE_BYTE(80);
		MESSAGE_END();
		pPlayer->m_iShowGameModeMessage = -1;
	}
}

int CHalfLifeColdSpot::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		if (!strcmp(pTeamName, "red"))
			return TEAM_RED;
		else
			return TEAM_BLUE;
	}
	
	return -1;	// No match
}

int CHalfLifeColdSpot::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if ( !pPlayer || (pTarget && !pTarget->IsPlayer()) )
		return GR_NOTTEAMMATE;

	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && 
			!stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

const char *CHalfLifeColdSpot::GetTeamID( CBaseEntity *pEntity )
{
	if ( pEntity == NULL || pEntity->pev == NULL )
		return "";

	// return their team name
	return pEntity->TeamID();
}

void CHalfLifeColdSpot::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn( pPlayer );

	CHalfLifeMultiplay::SavePlayerModel(pPlayer);

	// New player
	if (pPlayer->pev->iuser3 > 0)
	{
		pPlayer->pev->iuser3 = OBS_UNDECIDED_BOTH;
		return;
	}
	else if (pPlayer->pev->iuser3 == 0) // Spectator now joining
	{
		AutoJoin(pPlayer, pPlayer->m_iObserverWeapon);
		pPlayer->pev->iuser3 = -1;
		pPlayer->m_iObserverWeapon = 0; // Used as the menu option
		pPlayer->m_iShowGameModeMessage = gpGlobals->time + 0.5;
	}
}

void CHalfLifeColdSpot::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	// prevent skin/color/model changes
	char text[1024];
	char *mdls = g_engfuncs.pfnInfoKeyValue( infobuffer, "model" );
	int clientIndex = pPlayer->entindex();

	if (pPlayer->IsSpectator())
		return;

	// prevent skin/color/model changes
	if ( !stricmp( "red", pPlayer->m_szTeamName ) && !stricmp( "santa", mdls ) )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "[ColdSpot] You're on team '%s' To change, type 'model iceman'\n", pPlayer->m_szTeamName );
		return;
	}
	if ( !stricmp( "blue", pPlayer->m_szTeamName ) && !stricmp( "iceman", mdls ) )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "[ColdSpot] You're on team '%s' To change, type 'model santa'\n", pPlayer->m_szTeamName );
		return;
	}

	if ( stricmp( mdls, "iceman" ) && stricmp( mdls, "santa" ) )
	{
		g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", (char *)(pPlayer->pev->fuser4 == TEAM_RED ? "santa" : "iceman") );
		sprintf( text, "[ColdSpot] Can't change team to \'%s\'\n", mdls );
		UTIL_SayText( text, pPlayer );
		sprintf( text, "[ColdSpot] Server limits teams to \'%s\'\n", "iceman (blue), santa (red)" );
		UTIL_SayText( text, pPlayer );
		return;
	}

	//m_DisableDeathMessages = TRUE;
	m_DisableDeathPenalty = TRUE;

	entvars_t *pevWorld = VARS( INDEXENT(0) );
	pPlayer->TakeDamage( pevWorld, pevWorld, 900, DMG_ALWAYSGIB );

	//m_DisableDeathMessages = FALSE;
	m_DisableDeathPenalty = FALSE;

	int id = TEAM_BLUE;
	if ( !stricmp( mdls, "santa" ) )
		id = TEAM_RED;
	pPlayer->pev->fuser4 = id;

	if (pPlayer->pev->fuser4 == TEAM_RED)
	{
		strncpy( pPlayer->m_szTeamName, "red", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "santa" );
	}
	else
	{
		strncpy( pPlayer->m_szTeamName, "blue", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "iceman" );
	}

	// notify everyone of the team change
	sprintf( text, "[ColdSpot] %s has changed to team \'%s\'\n", STRING(pPlayer->pev->netname), pPlayer->m_szTeamName );
	UTIL_SayTextAll( text, pPlayer );

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

	MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
		WRITE_STRING("Hold the cold spot");
		WRITE_STRING(UTIL_VarArgs("You're on %s team", (pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
		WRITE_BYTE(0);
	MESSAGE_END();
}

void CHalfLifeColdSpot::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	if ( m_DisableDeathPenalty )
		return;

	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );
}

BOOL CHalfLifeColdSpot::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
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

BOOL CHalfLifeColdSpot::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if ( pAttacker && PlayerRelationship( pPlayer, pAttacker ) == GR_TEAMMATE )
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

void CHalfLifeColdSpot::UpdateHud( void )
{
	int bluescore = 0;
	int redscore = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
		{
			if (plr->pev->fuser4 == TEAM_RED)
				redscore += plr->m_iRoundWins;
			else
				bluescore += plr->m_iRoundWins;
		}
	}

	m_iBlueScore = bluescore;
	m_iRedScore = redscore;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
		{
			if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
			{
				MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
					WRITE_STRING("Hold the cold spot");
					if (plr->pev->fuser4 == TEAM_RED)
					{
						WRITE_STRING(UTIL_VarArgs("Red Team: %d", m_iRedScore));
						WRITE_BYTE(0);
						WRITE_STRING(UTIL_VarArgs("Blue Team: %d", m_iBlueScore));
					}
					else
					{
						WRITE_STRING(UTIL_VarArgs("Blue Team: %d", m_iBlueScore));
						WRITE_BYTE(0);
						WRITE_STRING(UTIL_VarArgs("Red Team: %d", m_iRedScore));
					}
				MESSAGE_END();
			}
		}
	}

	// End session if hit round limit
	if ( redscore >= scorelimit.value || bluescore >= scorelimit.value )
	{
		GoToIntermission();
	}
}
