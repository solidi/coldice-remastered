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
#include "ctf_gamerules.h"
#include "game.h"
#include "items.h"

extern int gmsgObjective;
extern int gmsgScoreInfo;
extern int gmsgItemPickup;
extern int gmsgTeamNames;
extern int gmsgTeamInfo;
extern int gmsgPlayClientSound;
extern int gmsgCtfInfo;
extern int gmsgBanner;
extern int gmsgSpecialEntity;

#define TEAM_BLUE 0
#define TEAM_RED 1

enum CTF_FLAG
{
	ON_GROUND = 0,
	NOT_CARRIED,
	CARRIED,
	WAVE_IDLE,
	FLAG_POSITIONED
};

class CFlagCharm : public CBaseEntity
{
public:
	static CFlagCharm *CreateFlag( Vector vecOrigin, int body );
	void Precache( void );
	void Spawn( void );
	void EXPORT FlagTouch( CBaseEntity *pOther );
	void EXPORT SolidThink( void );
	void EXPORT ReturnThink( void );

private:
	float m_fReturnTime = 0;
};

void CFlagCharm::Precache( void )
{
	PRECACHE_MODEL("models/flag.mdl");
	PRECACHE_SOUND("rune_pickup.wav");
}

CFlagCharm *CFlagCharm::CreateFlag( Vector vecOrigin, int body )
{
	CFlagCharm *pFlag = GetClassPtr( (CFlagCharm *)NULL );
	UTIL_SetOrigin( pFlag->pev, vecOrigin );
	pFlag->pev->angles = g_vecZero;
	pFlag->Spawn();
	pFlag->pev->body = body;
	if (body == TEAM_RED)
	{
		pFlag->pev->rendercolor = Vector(255, 0, 0);
		pFlag->pev->fuser4 = RADAR_FLAG_RED;
	}
	return pFlag;
}

void CFlagCharm::Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), "models/flag.mdl");

	pev->classname = MAKE_STRING("flag");
	pev->renderfx = kRenderFxGlowShell;
	pev->renderamt = 5;
	pev->rendercolor = Vector(0, 0, 255);
	pev->fuser4 = RADAR_FLAG_BLUE;

	pev->angles = g_vecZero;
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_NOT;
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CFlagCharm::FlagTouch );
	SetThink( &CFlagCharm::SolidThink );
	pev->nextthink = gpGlobals->time + 0.25;

	pev->sequence = FLAG_POSITIONED;
	pev->animtime = gpGlobals->time + RANDOM_FLOAT(0.0, 0.75);
	pev->framerate = 1.0;
}

void CFlagCharm::SolidThink( void )
{
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));

	SetThink( &CFlagCharm::ReturnThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CFlagCharm::ReturnThink( void )
{
	if (!((CHalfLifeCaptureTheFlag *)g_pGameRules)->pRedBase || 
		!((CHalfLifeCaptureTheFlag *)g_pGameRules)->pBlueBase)
		return;

	Vector vRedBase = ((CHalfLifeCaptureTheFlag *)g_pGameRules)->pRedBase->pev->origin;
	Vector vBlueBase = ((CHalfLifeCaptureTheFlag *)g_pGameRules)->pBlueBase->pev->origin;
	Vector returnOrigin = (pev->fuser4 == RADAR_FLAG_RED) ? vRedBase : vBlueBase;

	if (!m_fReturnTime && !pev->aiment && pev->origin != returnOrigin)
		m_fReturnTime = gpGlobals->time + 30.0;

	if (m_fReturnTime && !pev->aiment && m_fReturnTime < gpGlobals->time)
	{
		EHANDLE pMyBase = ((CHalfLifeCaptureTheFlag *)g_pGameRules)->pBlueBase;
		if (pev->fuser4 == RADAR_FLAG_RED)
			pMyBase = ((CHalfLifeCaptureTheFlag *)g_pGameRules)->pRedBase;
		pMyBase->pev->iuser4 = TRUE;

		if (pev->fuser4 == RADAR_FLAG_RED)
			((CHalfLifeCaptureTheFlag *)g_pGameRules)->UpdateHud(-1, 0);
		else
			((CHalfLifeCaptureTheFlag *)g_pGameRules)->UpdateHud(0, -1);

		MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
			WRITE_BYTE(CLIENT_SOUND_EBELL);
		MESSAGE_END();
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "%s flag returned to base!", (pev->fuser4 == RADAR_FLAG_RED) ? "Red" : "Blue" );

		UTIL_SetOrigin(pev, returnOrigin);
		pev->angles = g_vecZero;
	}

	// Always reset timer if in base
	if (pev->origin == returnOrigin)
		m_fReturnTime = 0;

	pev->nextthink = gpGlobals->time + 1.0;
}

void CFlagCharm::FlagTouch( CBaseEntity *pOther )
{
	if (!((CHalfLifeCaptureTheFlag *)g_pGameRules)->pRedBase || 
		!((CHalfLifeCaptureTheFlag *)g_pGameRules)->pBlueBase)
		return;

	// if it's not a player, ignore
	if ( !pOther->IsPlayer() )
		return;

	// If someone already has it, cannot take it directly.
	if ( pev->aiment && pOther->edict() != pev->aiment )
		return;

	if ( pOther->pev->deadflag != DEAD_NO )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	if (pPlayer->m_fFlagTime < gpGlobals->time)
	{
		if (!pPlayer->pFlag)
		{
			EHANDLE pMyBase = ((CHalfLifeCaptureTheFlag *)g_pGameRules)->pBlueBase;
			if (pev->fuser4 == RADAR_FLAG_RED)
				pMyBase = ((CHalfLifeCaptureTheFlag *)g_pGameRules)->pRedBase;

			// Flag pick up at base
			if ((pPlayer->pev->fuser4 + RADAR_FLAG_BLUE) != pev->fuser4)
			{
				pMyBase->pev->iuser4 = FALSE;

				pev->aiment = pOther->edict();
				pPlayer->m_fFlagTime = gpGlobals->time + 1.0;
				pPlayer->pFlag = this;
				pev->movetype = MOVETYPE_FOLLOW;
				pev->sequence = CARRIED;

				if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
				{
					MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer->edict());
						WRITE_STRING("You have the flag!");
						WRITE_STRING(UTIL_VarArgs("Get it to %s base", (pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
						WRITE_BYTE(0);
					MESSAGE_END();

					ClientPrint(pPlayer->pev, HUD_PRINTCENTER, UTIL_VarArgs("You have the %s flag!", (pPlayer->pev->fuser4 == TEAM_RED) ? "blue" : "red"));

					if (pPlayer->pev->fuser4 == TEAM_RED)
						((CHalfLifeCaptureTheFlag *)g_pGameRules)->UpdateHud(1, -1, pPlayer);
					else
						((CHalfLifeCaptureTheFlag *)g_pGameRules)->UpdateHud(-1, 1, pPlayer);

					for (int i = 1; i <= gpGlobals->maxClients; i++)
					{
						CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
						if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
						{
							if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
							{
								if (!plr->pFlag)
								{
									MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
										WRITE_STRING(UTIL_VarArgs("%s has the flag!", STRING(pPlayer->pev->netname)));
										WRITE_STRING(UTIL_VarArgs("You're on %s team", (pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
										WRITE_BYTE(0);
									MESSAGE_END();
								}
							}
						}
					}
				}

				MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
					WRITE_BYTE(CLIENT_SOUND_CTF_TAKEN);
				MESSAGE_END();
			}
			// Returning
			else if ((pPlayer->pev->fuser4 + RADAR_FLAG_BLUE) == pev->fuser4)
			{
				// Cannot touch same flag in its base
				Vector vRedBase = ((CHalfLifeCaptureTheFlag *)g_pGameRules)->pRedBase->pev->origin;
				Vector vBlueBase = ((CHalfLifeCaptureTheFlag *)g_pGameRules)->pBlueBase->pev->origin;
				Vector og = (pev->fuser4 == RADAR_FLAG_RED) ? vRedBase : vBlueBase;
				if (pev->origin != og)
				{
					pMyBase->pev->iuser4 = TRUE;

					pPlayer->m_fFlagTime = gpGlobals->time + 1.0;
					pev->movetype = MOVETYPE_TOSS;
					pev->aiment = 0;
					pev->sequence = FLAG_POSITIONED;
					pev->angles = g_vecZero;
					UTIL_SetOrigin(pev, og);

					if (pev->fuser4 == RADAR_FLAG_RED)
						((CHalfLifeCaptureTheFlag *)g_pGameRules)->UpdateHud(-1, 0);
					else
						((CHalfLifeCaptureTheFlag *)g_pGameRules)->UpdateHud(0, -1);

					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_NOPE);
					MESSAGE_END();
					UTIL_ClientPrintAll( HUD_PRINTCENTER, "%s flag returned by %s!", (pev->fuser4 == RADAR_FLAG_RED) ? "Red" : "Blue", STRING(pPlayer->pev->netname) );
				}
			}
		}
		else
		{
			// Touch my flag while holding the enemies one
			if ((pPlayer->pev->fuser4 + RADAR_FLAG_BLUE) == pev->fuser4)
			{
				ClientPrint(pPlayer->pev, HUD_PRINTCENTER, "Cannot return while holding a flag!");
				pPlayer->m_fFlagTime = gpGlobals->time + 1.0;
			}
		}
	}
}

LINK_ENTITY_TO_CLASS( flag, CFlagCharm );

// -------

class CFlagBase : public CBaseEntity
{
public:
	static CFlagBase *CreateFlagBase( Vector vecOrigin, int body );
	void Precache( void );
	void Spawn( void );
	void EXPORT CTFTouch( CBaseEntity *pOther );
	void SetFlag( BOOL flag );

private:
	float m_fNextTouch;
};

CFlagBase *CFlagBase::CreateFlagBase( Vector vecOrigin, int body )
{
	CFlagBase *pBase = GetClassPtr( (CFlagBase *)NULL );
	UTIL_SetOrigin( pBase->pev, vecOrigin );
	pBase->pev->angles = g_vecZero;
	pBase->Spawn();
	pBase->pev->body = body;
	if (body == TEAM_RED)
		pBase->pev->fuser4 = RADAR_BASE_RED;
	pBase->pev->iuser4 = TRUE; // mounted flag?
	pBase->m_fNextTouch = gpGlobals->time;

	// Broadcast special entities to all clients for radar tracking
	MESSAGE_BEGIN(MSG_ALL, gmsgSpecialEntity);
		WRITE_BYTE(body); // Index 0-7
		WRITE_BYTE(1); // Active
		WRITE_COORD(pBase->pev->origin.x);
		WRITE_COORD(pBase->pev->origin.y);
		WRITE_COORD(pBase->pev->origin.z);
		WRITE_BYTE(pBase->pev->fuser4); // Special type
	MESSAGE_END();

	return pBase;
}

void CFlagBase::Precache( void )
{
	PRECACHE_MODEL("models/flagbase.mdl");
	PRECACHE_SOUND("rune_pickup.wav");
}

void CFlagBase::Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), "models/flagbase.mdl");
	pev->classname = MAKE_STRING("base");
	pev->fuser4 = RADAR_BASE_BLUE;

	pev->angles.x = 0;
	pev->angles.z = 0;
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 96));

	SetTouch( &CFlagBase::CTFTouch );
}

void CFlagBase::CTFTouch( CBaseEntity *pOther )
{
	if (m_fNextTouch > gpGlobals->time)
		return;

	if ( !pOther->IsPlayer() )
		return;

	if ( pOther->pev->deadflag != DEAD_NO )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	if (pPlayer && pPlayer->IsPlayer())
	{
		CBaseEntity *pFlag = pPlayer->pFlag;
		if (pFlag && pPlayer->m_fFlagTime < gpGlobals->time)
		{
			// Capture
			Vector vRedBase = ((CHalfLifeCaptureTheFlag *)g_pGameRules)->pRedBase->pev->origin;
			Vector vBlueBase = ((CHalfLifeCaptureTheFlag *)g_pGameRules)->pBlueBase->pev->origin;
			EHANDLE pOtherBase = ((CHalfLifeCaptureTheFlag *)g_pGameRules)->pRedBase;
			if (pev->fuser4 == RADAR_BASE_RED)
				pOtherBase = ((CHalfLifeCaptureTheFlag *)g_pGameRules)->pBlueBase;

			// search for enemy flag, otherwise no score.
			if (pFlag->pev->fuser4 != (pev->fuser4 - 2))
			{
				// My flag is at my base, so I score
				if (pev->iuser4 == TRUE)
				{
					// Return flag state
					int flagid = pFlag->pev->fuser4;
					pOtherBase->pev->iuser4 = TRUE;

					m_fNextTouch = gpGlobals->time + 1.0;
					pPlayer->m_fFlagTime = gpGlobals->time + 1.0;

					pFlag->pev->movetype = MOVETYPE_TOSS;
					pFlag->pev->aiment = 0;
					pFlag->pev->sequence = FLAG_POSITIONED;
					pFlag->pev->angles = g_vecZero;

					UTIL_SetOrigin(pFlag->pev, (pFlag->pev->fuser4 == RADAR_FLAG_RED) ? vRedBase : vBlueBase);
					pPlayer->pFlag = NULL;

					for (int i = 1; i <= gpGlobals->maxClients; i++)
					{
						CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
						if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
						{
							if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING(UTIL_VarArgs("Capture the %s flag", (plr->pev->fuser4 == TEAM_RED) ? "blue" : "red"));
									WRITE_STRING(UTIL_VarArgs("You're on %s team", (pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
									WRITE_BYTE(0);
								MESSAGE_END();
							}
						}
					}

					ClientPrint(pPlayer->pev, HUD_PRINTCENTER, "You've scored!");
					UTIL_ClientPrintAll(HUD_PRINTCENTER, "%s scored for the %s team!\n", STRING(pPlayer->pev->netname), pPlayer->m_szTeamName);

					MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
						WRITE_BYTE(CLIENT_SOUND_CTF_CAPTURE);
					MESSAGE_END();

					pPlayer->Celebrate();

					MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
						WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
						WRITE_SHORT( pPlayer->pev->frags );
						WRITE_SHORT( pPlayer->m_iDeaths );
						WRITE_SHORT( ++pPlayer->m_iRoundWins );
						WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
					MESSAGE_END();

					if (flagid == RADAR_FLAG_RED)
						((CHalfLifeCaptureTheFlag *)g_pGameRules)->UpdateHud(-1, 0);
					else
						((CHalfLifeCaptureTheFlag *)g_pGameRules)->UpdateHud(0, -1);
				}
				else
				{
					ClientPrint(pPlayer->pev, HUD_PRINTCENTER, "Cannot score while flag is missing!");
					pPlayer->m_fFlagTime = gpGlobals->time + 1.0;
				}
			}
		}
	}
}

LINK_ENTITY_TO_CLASS( base, CFlagBase );

// --------

CHalfLifeCaptureTheFlag::CHalfLifeCaptureTheFlag()
{
	m_DisableDeathPenalty = FALSE;
	pRedBase = pBlueBase = NULL;
	m_fSpawnBlueHardware = gpGlobals->time + 2.0;
	m_fSpawnRedHardware = gpGlobals->time + 2.0;
	m_iBlueScore = m_iRedScore = m_iBlueMode = m_iRedMode = 0;
	UTIL_PrecacheOther("flag");
	UTIL_PrecacheOther("base");
}

BOOL CHalfLifeCaptureTheFlag::IsSpawnPointValid( CBaseEntity *pSpot )
{
	CBaseEntity *ent = NULL;

	ALERT(at_console, "checking spawn point %s\n", STRING(pSpot->pev->classname));

	// Dont capture entity that's given (to players).
	if (FBitSet(pSpot->pev->spawnflags, SF_GIVEITEM))
		return FALSE;

	while ( (ent = UTIL_FindEntityInSphere( ent, pSpot->pev->origin, ctfdistance.value )) != NULL )
	{
		// Is another base in area
		if (FClassnameIs(ent->pev, "base"))
		{
			ALERT(at_error, "%s: too close to base\n", STRING(pSpot->pev->classname));
			return FALSE;
		}
	}

	return TRUE;
}

edict_t *CHalfLifeCaptureTheFlag::EntSelectSpawnPoint( const char *szSpawnPoint )
{
	CBaseEntity *pSpot = NULL;

	// choose a point
	if ( g_pGameRules->IsDeathmatch() )
	{
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

		pSpot = NULL;
	}

ReturnSpot:
	if ( FNullEnt( pSpot ) )
	{
		ALERT(at_error, "could not find %s for ctf spot\n", szSpawnPoint);
		return NULL;
	}

	return pSpot->edict();
}

void CHalfLifeCaptureTheFlag::InitHUD( CBasePlayer *pPlayer )
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

	pPlayer->pev->fuser4 = redteam >= blueteam ? TEAM_BLUE : TEAM_RED;

	if (pPlayer->pev->fuser4 == TEAM_BLUE)
	{
		strncpy(pPlayer->m_szTeamName, "blue", TEAM_NAME_LENGTH);
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()),
			g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "iceman");
	}
	else
	{
		strncpy(pPlayer->m_szTeamName, "red", TEAM_NAME_LENGTH);
		g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()),
			g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "santa");
	}

	CHalfLifeMultiplay::InitHUD( pPlayer );

	MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
		WRITE_BYTE( 2 );
		WRITE_STRING( "blue" );
		WRITE_STRING( "red" );
	MESSAGE_END();

	char text[256];
	sprintf( text, "[CtF]: You're on team \'%s\'\n", pPlayer->m_szTeamName );
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

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING(UTIL_VarArgs("Capture the %s flag", (pPlayer->pev->fuser4 == TEAM_RED) ? "blue" : "red"));
			WRITE_STRING(UTIL_VarArgs("You're on %s team", (pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
			WRITE_BYTE(0);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgCtfInfo, NULL, pPlayer->edict());
			WRITE_BYTE(m_iBlueScore);
			WRITE_BYTE(m_iRedScore);
			WRITE_BYTE(m_iBlueMode);
			WRITE_BYTE(m_iRedMode);
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

void CHalfLifeCaptureTheFlag::Think( void )
{
	CHalfLifeMultiplay::Think();

	if (m_fSpawnBlueHardware != -1 && m_fSpawnBlueHardware < gpGlobals->time)
	{
		edict_t *pentSpawnSpot = EntSelectSpawnPoint(ctfspawn1.string);
		if (pentSpawnSpot)
		{
			ALERT(at_console, "[CtF] Blue base set!\n");
			CFlagCharm::CreateFlag(pentSpawnSpot->v.origin, TEAM_BLUE);
			pBlueBase = CFlagBase::CreateFlagBase(pentSpawnSpot->v.origin, TEAM_BLUE);
			m_fSpawnBlueHardware = -1;
			pentSpawnSpot->v.solid = SOLID_NOT;
			pentSpawnSpot->v.effects |= EF_NODRAW;
		}
		else
			m_fSpawnBlueHardware = gpGlobals->time + 2.0;
	}

	if (m_fSpawnRedHardware != -1 && m_fSpawnRedHardware < gpGlobals->time)
	{
		edict_t *pentSpawnSpot2 = EntSelectSpawnPoint(ctfspawn2.string);
		if (pentSpawnSpot2)
		{
			ALERT(at_console, "[CtF] Red base set!\n");
			CFlagCharm::CreateFlag(pentSpawnSpot2->v.origin, TEAM_RED);
			pRedBase = CFlagBase::CreateFlagBase(pentSpawnSpot2->v.origin, TEAM_RED);
			m_fSpawnRedHardware = -1;
			pentSpawnSpot2->v.solid = SOLID_NOT;
			pentSpawnSpot2->v.effects |= EF_NODRAW;
		}
		else
			m_fSpawnRedHardware = gpGlobals->time + 2.0;
	}

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
		{
			if (plr->m_iShowGameModeMessage > -1 && plr->m_iShowGameModeMessage < gpGlobals->time && !FBitSet(plr->pev->flags, FL_FAKECLIENT))
			{
				MESSAGE_BEGIN(MSG_ONE, gmsgBanner, NULL, plr->edict());
					WRITE_STRING(UTIL_VarArgs("You Are On Team %s", plr->pev->fuser4 == TEAM_RED ? "Red" : "Blue"));
					WRITE_STRING(UTIL_VarArgs("Capture the %s flag and run it back to your base!", plr->pev->fuser4 == TEAM_RED ? "Blue" : "Red"));
					WRITE_BYTE(80);
				MESSAGE_END();
				plr->m_iShowGameModeMessage = -1;
			}
		}
	}
}

int CHalfLifeCaptureTheFlag::GetTeamIndex( const char *pTeamName )
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

int CHalfLifeCaptureTheFlag::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
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

const char *CHalfLifeCaptureTheFlag::GetTeamID( CBaseEntity *pEntity )
{
	if ( pEntity == NULL || pEntity->pev == NULL )
		return "";

	// return their team name
	return pEntity->TeamID();
}

void CHalfLifeCaptureTheFlag::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn( pPlayer );

	CHalfLifeMultiplay::SavePlayerModel(pPlayer);

	if (pPlayer->m_iShowGameModeMessage > -1)
		pPlayer->m_iShowGameModeMessage = gpGlobals->time + 0.5;
}

extern DLL_GLOBAL BOOL g_fGameOver;

void CHalfLifeCaptureTheFlag::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	// prevent skin/color/model changes
	char text[1024];
	char *mdls = g_engfuncs.pfnInfoKeyValue( infobuffer, "model" );
	int clientIndex = pPlayer->entindex();

	// Spectator
	if ( !pPlayer->m_szTeamName || !strlen(pPlayer->m_szTeamName) )
		return;

	// Ignore ctf on model changing back.
	if ( g_fGameOver )
		return;

	// prevent skin/color/model changes
	if ( !stricmp( "red", pPlayer->m_szTeamName ) && !stricmp( "santa", mdls ) )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "[CtF]: You're on team '%s' To change, type 'model iceman'\n", pPlayer->m_szTeamName );
		CLIENT_COMMAND(pPlayer->edict(), "model santa\n");
		return;
	}
	if ( !stricmp( "blue", pPlayer->m_szTeamName ) && !stricmp( "iceman", mdls ) )
	{
		ClientPrint( pPlayer->pev, HUD_PRINTCONSOLE, "[CtF]: You're on team '%s' To change, type 'model santa'\n", pPlayer->m_szTeamName );
		return;
	}

	if ( stricmp( mdls, "iceman" ) && stricmp( mdls, "santa" ) )
	{
		g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", (char *)(pPlayer->pev->fuser4 == TEAM_RED ? "santa" : "iceman") );
		sprintf( text, "* Can't change team to \'%s\'\n", mdls );
		UTIL_SayText( text, pPlayer );
		sprintf( text, "* Server limits teams to \'%s\'\n", "iceman (blue), santa (red)" );
		UTIL_SayText( text, pPlayer );
		return;
	}

	//m_DisableDeathMessages = TRUE;
	m_DisableDeathPenalty = TRUE;

	ClearMultiDamage();
	pPlayer->pev->health = 0; // without this, player can walk as a ghost.
	pPlayer->Killed(pPlayer->pev, VARS(INDEXENT(0)), GIB_ALWAYS);

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
		//g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "team", pPlayer->m_szTeamName );
	}
	else
	{
		strncpy( pPlayer->m_szTeamName, "blue", TEAM_NAME_LENGTH );
		g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "iceman" );
		//g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "team", pPlayer->m_szTeamName );
	}

	// notify everyone of the team change
	sprintf( text, "[CtF]: %s has changed to team \'%s\'\n", STRING(pPlayer->pev->netname), pPlayer->m_szTeamName );
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

	MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer->edict());
		WRITE_STRING(UTIL_VarArgs("Capture the %s flag", (pPlayer->pev->fuser4 == TEAM_RED) ? "blue" : "red"));
		WRITE_STRING(UTIL_VarArgs("You're on %s team", (pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
		WRITE_BYTE(0);
	MESSAGE_END();
}

CBaseEntity *CHalfLifeCaptureTheFlag::DropCharm( CBasePlayer *pPlayer, Vector origin )
{
	CBaseEntity *pFlag = pPlayer->pFlag;

	if (pFlag)
	{
		if (pFlag->pev->fuser4 == RADAR_FLAG_RED)
			((CHalfLifeCaptureTheFlag *)g_pGameRules)->UpdateHud(-1, 2);
		else
			((CHalfLifeCaptureTheFlag *)g_pGameRules)->UpdateHud(2, -1);

		//pFlag->pev->solid = SOLID_TRIGGER;
		pFlag->pev->movetype = MOVETYPE_TOSS;
		pFlag->pev->aiment = 0;
		pFlag->pev->sequence = ON_GROUND;

		origin.z -= 32;
		UTIL_SetOrigin(pFlag->pev, origin);

		pPlayer->m_fFlagTime = gpGlobals->time + 0.25;
		pPlayer->pFlag = NULL;

		CBasePlayer *pPlayerWithFlag = NULL;
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( plr && plr->IsPlayer() && !plr->HasDisconnected && plr->pFlag )
			{
				pPlayerWithFlag = plr;
				break;
			}
		}

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( plr && plr->IsPlayer() && !plr->HasDisconnected && plr != pPlayerWithFlag )
			{
				if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
				{
					if (pPlayerWithFlag)
					{
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
							WRITE_STRING(UTIL_VarArgs("%s has the flag!", STRING(pPlayerWithFlag->pev->netname)));
							WRITE_STRING(UTIL_VarArgs("You're on %s team", (pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
							WRITE_BYTE(0);
						MESSAGE_END();
					}
					else
					{
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
							WRITE_STRING(UTIL_VarArgs("Capture the %s flag", (plr->pev->fuser4 == TEAM_RED) ? "blue" : "red"));
							WRITE_STRING(UTIL_VarArgs("You're on %s team", (pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
							WRITE_BYTE(0);
						MESSAGE_END();
					}
				}
			}
		}
	}

	return pFlag;
}

void CHalfLifeCaptureTheFlag::ClientDisconnected( edict_t *pClient )
{
	CHalfLifeMultiplay::ClientDisconnected( pClient );

	if (pClient)
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance(pClient);

		if (pPlayer)
			if (pPlayer->pFlag)
				DropCharm(pPlayer, pPlayer->pev->origin);
	}
}

void CHalfLifeCaptureTheFlag::UpdateHud(int bluemode, int redmode, CBasePlayer *pPlayer)
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

	if (bluemode != -1)
		m_iBlueMode = bluemode;
	if (redmode != -1)
		m_iRedMode = redmode;

	m_iBlueScore = bluescore;
	m_iRedScore = redscore;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
		{
			if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
			{
				MESSAGE_BEGIN(MSG_ONE, gmsgCtfInfo, NULL, plr->edict());
					WRITE_BYTE(m_iBlueScore);
					WRITE_BYTE(m_iRedScore);
					plr == pPlayer && plr->pev->fuser4 == TEAM_RED ? WRITE_BYTE(3) : WRITE_BYTE(bluemode);
					plr == pPlayer && plr->pev->fuser4 == TEAM_BLUE ? WRITE_BYTE(3) : WRITE_BYTE(redmode);
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

void CHalfLifeCaptureTheFlag::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	if ( m_DisableDeathPenalty )
		return;

	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );
}

BOOL CHalfLifeCaptureTheFlag::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
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

BOOL CHalfLifeCaptureTheFlag::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
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

BOOL CHalfLifeCaptureTheFlag::IsTeamplay( void )
{
	return TRUE;
}
