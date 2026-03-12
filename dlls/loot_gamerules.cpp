
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
#include "loot_gamerules.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"
#include "shake.h"

extern int gmsgGameMode;
extern int gmsgScoreInfo;
extern int gmsgPlayClientSound;
extern int gmsgObjective;
extern int gmsgTeamNames;
extern int gmsgTeamInfo;
extern int gmsgStatusIcon;
extern int gmsgBanner;
extern int gmsgDEraser;
extern int gmsgShowTimer;
extern int gmsgSpecialEntity;

extern DLL_GLOBAL BOOL g_fGameOver;

const char *CHalfLifeLoot::s_TeamNames[4] = { "iceman", "santa", "holo", "commando" };

// ============================================================
// CLootCrate
//   Breakable wooden crate (w_crate.mdl) that may hold
//   the loot.  Health 25 - a few hits with a crowbar or
//   gunfire will smash it open.
// ============================================================
class CLootCrate : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	int  TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker,
	                 float flDamage, int bitsDamageType );
	void Break( void );

	int  ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	BOOL m_bHasLoot;
	int  m_iGibModel;
};

LINK_ENTITY_TO_CLASS( loot_crate, CLootCrate );

void CLootCrate::Precache( void )
{
	PRECACHE_MODEL( "models/w_crate.mdl" );
	m_iGibModel = PRECACHE_MODEL( "models/woodgibs.mdl" );
	PRECACHE_SOUND( "debris/wood1.wav" );
	PRECACHE_SOUND( "debris/wood2.wav" );
	PRECACHE_SOUND( "debris/wood3.wav" );
}

void CLootCrate::Spawn( void )
{
	Precache();
	SET_MODEL( ENT(pev), "models/w_crate.mdl" );

	pev->skin      = 0;//RANDOM_LONG( 3, 7 );
	pev->solid     = SOLID_BBOX;
	pev->movetype  = MOVETYPE_TOSS;
	pev->takedamage = DAMAGE_YES;
	pev->health    = 25;
	pev->sequence  = 0;
	pev->animtime  = gpGlobals->time;
	pev->framerate = 1.0f;
	m_bHasLoot     = FALSE;

	UTIL_SetSize( pev, Vector(-16, -16, 0), Vector(16, 16, 48) );
	UTIL_SetOrigin( pev, pev->origin );
}

int CLootCrate::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker,
                             float flDamage, int bitsDamageType )
{
	if ( !pev->takedamage )
		return 0;

	pev->health -= flDamage;

	static const char *woodSounds[] = {
		"debris/wood1.wav", "debris/wood2.wav", "debris/wood3.wav"
	};
	EMIT_SOUND_DYN( ENT(pev), CHAN_BODY,
	                woodSounds[RANDOM_LONG(0, 2)], 0.5f, ATTN_NORM, 0, 100 );

	if ( pev->health <= 0 )
	{
		Break();
		return 0;
	}
	return 1;
}

void CLootCrate::Break( void )
{
	pev->takedamage = DAMAGE_NO;
	pev->solid      = SOLID_NOT;

	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "debris/wood1.wav", 0.8f, ATTN_NORM, 0, 95 );

	// TE_BREAKMODEL: scatter wood gibs
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE ( TE_BREAKMODEL );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( 32 );           // size x
		WRITE_COORD( 32 );           // size y
		WRITE_COORD( 32 );           // size z
		WRITE_COORD( 0  );           // velocity x
		WRITE_COORD( 0  );           // velocity y
		WRITE_COORD( 0  );           // velocity z
		WRITE_BYTE ( 10 );           // random velocity range
		WRITE_SHORT( m_iGibModel );
		WRITE_BYTE ( 5  );           // count
		WRITE_BYTE ( 25 );           // life (0.1s units -> 2.5s)
		WRITE_BYTE ( 0  );           // flags
	MESSAGE_END();

	if ( m_bHasLoot && g_pGameRules && g_pGameRules->IsLoot() )
		((CHalfLifeLoot *)g_pGameRules)->SpawnLootAtPosition( pev->origin );

	UTIL_Remove( this );
}

// ============================================================
// CLootEntity
//   The w_isotopebox that spawns from the marked crate.
//   Touchable by players; hidden (EF_NODRAW) while held.
// ============================================================
class CLootEntity : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT LootTouch( CBaseEntity *pOther );
	void EXPORT LootThink( void );
	void Drop( Vector origin );

	int  ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

LINK_ENTITY_TO_CLASS( loot_entity, CLootEntity );

void CLootEntity::Precache( void )
{
	PRECACHE_MODEL( "models/w_isotopebox.mdl" );
}

void CLootEntity::Spawn( void )
{
	Precache();
	SET_MODEL( ENT(pev), "models/w_isotopebox.mdl" );

	pev->solid     = SOLID_TRIGGER;
	pev->movetype  = MOVETYPE_TOSS;
	pev->effects   = 0;
	pev->takedamage = DAMAGE_NO;

	UTIL_SetSize( pev, Vector(-16, -16, -16), Vector(16, 16, 16) );
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CLootEntity::LootTouch );
	SetThink( &CLootEntity::LootThink );
	pev->nextthink = gpGlobals->time + 0.5f;
}

void CLootEntity::LootTouch( CBaseEntity *pOther )
{
	if ( !pOther || !pOther->IsPlayer() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	if ( !pPlayer->IsAlive() )
		return;
	if ( FBitSet( pPlayer->pev->flags, FL_FAKECLIENT ) )
		return;
	if ( pev->effects & EF_NODRAW )   // already held - touch is disabled
		return;

	if ( !g_pGameRules || !g_pGameRules->IsLoot() )
		return;

	CHalfLifeLoot *pRules = (CHalfLifeLoot *)g_pGameRules;
	if ( !pRules->IsGameInProgress() || !pRules->IsLootFree() )
		return;

	pRules->CaptureCharm( pPlayer );
}

void CLootEntity::LootThink( void )
{
	// When free in the world, settle on floor and watch bounds
	if ( !(pev->effects & EF_NODRAW) )
	{
		// Transition to static once it lands
		if ( pev->movetype == MOVETYPE_TOSS && FBitSet( pev->flags, FL_ONGROUND ) )
			pev->movetype = MOVETYPE_NONE;

		// Bounds / stuck check
		/*TraceResult tr;
		UTIL_TraceHull( pev->origin, pev->origin,
		                dont_ignore_monsters, human_hull, ENT(pev), &tr );
		if ( tr.fStartSolid )
		{
			// Teleport to nearest spawn point
			CBaseEntity *pSpot = UTIL_FindEntityByClassname( NULL, "info_player_deathmatch" );
			if ( pSpot )
			{
				UTIL_SetOrigin( pev, pSpot->pev->origin + Vector(0, 0, 36) );
				pev->velocity  = g_vecZero;
				pev->movetype  = MOVETYPE_NONE;
			}
		}*/
	}
	pev->nextthink = gpGlobals->time + 0.5f;
}

void CLootEntity::Drop( Vector origin )
{
	pev->effects  &= ~EF_NODRAW;
	pev->solid     = SOLID_TRIGGER;
	pev->movetype  = MOVETYPE_TOSS;
	pev->aiment   = NULL;
	pev->owner    = NULL;

	// Bounds check before placing
	/*TraceResult tr;
	UTIL_TraceHull( origin, origin, dont_ignore_monsters, human_hull, ENT(pev), &tr );
	if ( tr.fStartSolid )
	{
		CBaseEntity *pSpot = UTIL_FindEntityByClassname( NULL, "info_player_deathmatch" );
		if ( pSpot )
			origin = pSpot->pev->origin + Vector(0, 0, 36);
	}*/

	UTIL_SetOrigin( pev, origin );
	pev->velocity.x = RANDOM_FLOAT( -100, 100 );
	pev->velocity.y = RANDOM_FLOAT( -100, 100 );
	pev->velocity.z = RANDOM_FLOAT(   50, 150 );

	SetTouch( &CLootEntity::LootTouch );
	SetThink( &CLootEntity::LootThink );
	pev->nextthink = gpGlobals->time + 0.5f;
}

// ============================================================
// CLootGoal
//   Static scoring zone.  When the loot-holder touches it,
//   OnGoalReached is called on the gamerules.
// ============================================================
class CLootGoal : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT GoalTouch( CBaseEntity *pOther );

	int  ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

LINK_ENTITY_TO_CLASS( loot_goal, CLootGoal );

void CLootGoal::Precache( void )
{
	PRECACHE_MODEL( "models/in_teleport.mdl" );
}

void CLootGoal::Spawn( void )
{
	Precache();
	SET_MODEL( ENT(pev), "models/in_teleport.mdl" );

	pev->solid     = SOLID_TRIGGER;
	pev->movetype  = MOVETYPE_NONE;
	pev->takedamage = DAMAGE_NO;
	pev->effects   = 0;
	pev->framerate = 1;

	UTIL_SetSize( pev, Vector(-48, -48, -32), Vector(48, 48, 48) );
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CLootGoal::GoalTouch );
}

void CLootGoal::GoalTouch( CBaseEntity *pOther )
{
	if ( !pOther || !pOther->IsPlayer() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	if ( !pPlayer->IsAlive() || !pPlayer->m_bHoldingLoot )
		return;

	if ( !g_pGameRules || !g_pGameRules->IsLoot() )
		return;

	((CHalfLifeLoot *)g_pGameRules)->OnGoalReached( pPlayer );
}

// ============================================================
// CHalfLifeLoot
// ============================================================

CHalfLifeLoot::CHalfLifeLoot()
{
	PauseMutators();

	m_hLootEntity  = (CBaseEntity *)NULL;
	m_hGoalEntity  = (CBaseEntity *)NULL;
	m_hLootHolder  = (CBaseEntity *)NULL;

	m_flLootPickupTime = 0;
	m_fSendBannerTime  = 0;
	m_flLastObjUpdate  = 0;
	m_iUsedSpotCount   = 0;
	m_fSpawnGroupSolidRestoreTime = 0;
	m_flCelebrationEndTime = 0;
	m_iPendingWinnerTeam   = -1;

	memset( m_flTeamHoldTime,    0, sizeof(m_flTeamHoldTime) );
	memset( m_iTeamPlayers,      0, sizeof(m_iTeamPlayers) );
	memset( m_iTeamPlayerCount,  0, sizeof(m_iTeamPlayerCount) );
	memset( m_vecTeamSpawnOrigin,0, sizeof(m_vecTeamSpawnOrigin) );
	memset( m_vecUsedSpots,      0, sizeof(m_vecUsedSpots) );

	UTIL_PrecacheOther("loot_entity");
	UTIL_PrecacheOther("loot_crate");
	UTIL_PrecacheOther("loot_goal");
}

// ============================================================
// FindSafeFloorPosition
//   Random XY within world bounds, trace down to floor,
//   hull-check for clearance, enforce min distance from avoid.
// ============================================================
BOOL CHalfLifeLoot::FindSafeFloorPosition( Vector &out, float minDistFromAvoid,
                                            Vector avoid, int maxAttempts )
{
	edict_t *worldEdict = ENT(0);
	Vector worldMin = VARS(worldEdict)->absmin;
	Vector worldMax = VARS(worldEdict)->absmax;

	// Sanity-check world bounds
	if ( (worldMax - worldMin).Length() < 256 )
	{
		worldMin = Vector(-4096, -4096, -4096);
		worldMax = Vector( 4096,  4096,  4096);
	}

	for ( int attempt = 0; attempt < maxAttempts; attempt++ )
	{
		float x = worldMin.x + RANDOM_FLOAT(0, 1) * (worldMax.x - worldMin.x);
		float y = worldMin.y + RANDOM_FLOAT(0, 1) * (worldMax.y - worldMin.y);

		// Trace downward from near ceiling
		Vector start( x, y, worldMax.z - 32 );
		Vector end  ( x, y, worldMin.z + 32 );

		TraceResult tr;
		UTIL_TraceLine( start, end, ignore_monsters, ENT(0), &tr );

		if ( tr.flFraction >= 1.0f ) continue;  // no floor
		if ( tr.fAllSolid )          continue;

		Vector testPos = tr.vecEndPos + Vector(0, 0, 48);

		// Check minimum separation
		if ( minDistFromAvoid > 0 && (testPos - avoid).Length() < minDistFromAvoid )
			continue;

		// Check that a player hull fits here
		TraceResult hullTr;
		UTIL_TraceHull( testPos, testPos, dont_ignore_monsters, human_hull, ENT(0), &hullTr );
		if ( hullTr.fStartSolid ) continue;

		out = testPos;
		return TRUE;
	}
	return FALSE;
}

// ============================================================
// SpawnLootAtPosition  (callback from CLootCrate::Break)
// ============================================================
void CHalfLifeLoot::SpawnLootAtPosition( Vector origin )
{
	Vector spawnPos = origin + Vector(0, 0, 36);

	CBaseEntity *pLoot = CBaseEntity::Create( "loot_entity", spawnPos, g_vecZero, NULL );
	if ( pLoot )
	{
		// Pop upward out of the crate debris
		pLoot->pev->velocity.x = RANDOM_FLOAT(-80, 80);
		pLoot->pev->velocity.y = RANDOM_FLOAT(-80, 80);
		pLoot->pev->velocity.z = RANDOM_FLOAT(100, 200);
		m_hLootEntity = pLoot;
	}
}

// ============================================================
// SpawnLootEntities
//   3 × playercount crates, one marked with loot.
//   Goal placed > 512 units from loot crate.
// ============================================================
void CHalfLifeLoot::SpawnLootEntities( void )
{
	int count = m_iPlayersInGame * 3;
	if ( count < 3  ) count = 3;
	if ( count > 32 ) count = 32;

	int lootCrateIndex = RANDOM_LONG(0, count - 1);
	Vector lootCrateOrigin = g_vecZero;

	ALERT(at_console, "[Loot] Spawning %d crates (loot at index %d)\n", count, lootCrateIndex);

	// Fallback weapon-entity classnames tried when floor trace fails
	static const char *wFallback[] = {
		"weapon_shotgun", "weapon_mp5", "weapon_crowbar",
		"ammo_buckshot",  "weapon_357", "weapon_9mmclip"
	};
	const int wFallbackCount = 6;

	for ( int i = 0; i < count; i++ )
	{
		Vector spawnPos;
		BOOL found = FindSafeFloorPosition( spawnPos, 0, g_vecZero, 30 );

		if ( !found )
		{
			// Try to piggy-back on an existing weapon entity
			for ( int f = 0; f < wFallbackCount && !found; f++ )
			{
				CBaseEntity *pW = UTIL_FindEntityByClassname( NULL, wFallback[f] );
				if ( pW ) { spawnPos = pW->pev->origin + Vector(0,0,32); found = TRUE; }
			}
			if ( !found ) continue;
		}

		CBaseEntity *pEnt = CBaseEntity::Create( "loot_crate", spawnPos, g_vecZero, NULL );
		if ( pEnt )
		{
			CLootCrate *pCrate = (CLootCrate *)pEnt;
			if ( i == lootCrateIndex )
			{
				pCrate->m_bHasLoot  = TRUE;
				lootCrateOrigin     = spawnPos;
				ALERT(at_console, "[Loot] Loot crate at (%.0f, %.0f, %.0f)\n",
				      spawnPos.x, spawnPos.y, spawnPos.z);
			}
		}
	}

	// Spawn goal; try to keep it > 512 units from the loot crate
	Vector goalPos;
	BOOL foundGoal = FALSE;

	for ( int attempt = 0; attempt < 3 && !foundGoal; attempt++ )
	{
		float minDist = (attempt == 0) ? 512.0f : (attempt == 1 ? 256.0f : 0.0f);
		foundGoal = FindSafeFloorPosition( goalPos, minDist, lootCrateOrigin, 40 );
	}

	if ( !foundGoal )
	{
		// Fallback: a random info_player_deathmatch
		CBaseEntity *pSpot = NULL;
		for ( int i = RANDOM_LONG(1, 8); i > 0; i-- )
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
		if ( pSpot )
			goalPos = pSpot->pev->origin;
		else
			goalPos = Vector(0, 0, 0);
	}

	CBaseEntity *pGoal = CBaseEntity::Create( "loot_goal", goalPos, g_vecZero, NULL );
	if ( pGoal )
	{
		m_hGoalEntity = pGoal;
		SendGoalSpecialEntity();
		ALERT(at_console, "[Loot] Goal at (%.0f, %.0f, %.0f)\n", goalPos.x, goalPos.y, goalPos.z);
	}

	UTIL_ClientPrintAll( HUD_PRINTTALK, "[Loot] Crates have been dropped! One contains the loot!\n" );
}

// ============================================================
// SendGoalSpecialEntity  (slot 0 = goal, red dot on radar)
// ============================================================
void CHalfLifeLoot::SendGoalSpecialEntity( void )
{
	CBaseEntity *pGoal = (CBaseEntity *)m_hGoalEntity;
	if ( !pGoal ) return;

	MESSAGE_BEGIN( MSG_ALL, gmsgSpecialEntity );
		WRITE_BYTE ( 0 );   // slot 0
		WRITE_BYTE ( 1 );   // active
		WRITE_COORD( pGoal->pev->origin.x );
		WRITE_COORD( pGoal->pev->origin.y );
		WRITE_COORD( pGoal->pev->origin.z );
		WRITE_BYTE ( RADAR_BASE_RED );
	MESSAGE_END();

	// Clear remaining slots
	for ( int s = 1; s < 8; s++ )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgSpecialEntity );
			WRITE_BYTE( s );
			WRITE_BYTE( 0 );  // inactive
		MESSAGE_END();
	}
}

// ============================================================
// StartRound
// ============================================================
void CHalfLifeLoot::StartRound( int clients )
{
	SetRoundLimits();
	RestoreMutators();
	RemoveAndFillItems();

	extern void ClearBodyQue();
	ClearBodyQue();

	MESSAGE_BEGIN( MSG_ALL, gmsgDEraser ); MESSAGE_END();

	// Reset team state
	memset( m_iTeamPlayers,       0, sizeof(m_iTeamPlayers) );
	memset( m_iTeamPlayerCount,   0, sizeof(m_iTeamPlayerCount) );
	memset( m_flTeamHoldTime,     0, sizeof(m_flTeamHoldTime) );
	memset( m_vecTeamSpawnOrigin, 0, sizeof(m_vecTeamSpawnOrigin) );
	m_iUsedSpotCount   = 0;
	m_flLootPickupTime = 0;
	m_hLootHolder      = (CBaseEntity *)NULL;
	m_hLootEntity      = (CBaseEntity *)NULL;
	m_hGoalEntity      = (CBaseEntity *)NULL;
	m_flCelebrationEndTime = 0;
	m_iPendingWinnerTeam   = -1;
	memset( m_vecUsedSpots, 0, sizeof(m_vecUsedSpots) );

	// Fisher-Yates shuffle of available player slots
	for ( int i = clients - 1; i > 0; i-- )
	{
		int j        = RANDOM_LONG(0, i);
		int tmp      = m_iPlayersInArena[i];
		m_iPlayersInArena[i] = m_iPlayersInArena[j];
		m_iPlayersInArena[j] = tmp;
	}

	// Team assignment:
	//   ≤ 8 players: form pairs — consecutive shuffled players share a team
	//                (4 players → 2 teams of 2; 6 → 3 teams of 2; 8 → 4 teams of 2;
	//                 odd counts get one solo team e.g. 5 → 2+2+1)
	//   > 8 players: 4 teams, round-robin balanced
	for ( int i = 0; i < clients; i++ )
	{
		int t = (clients <= 8) ? (i / 2) : (i % 4);
		if ( t > 3 ) t = 3;   // safety clamp to 4 teams max
		if ( m_iTeamPlayerCount[t] < 32 )
		{
			m_iTeamPlayers[t][m_iTeamPlayerCount[t]] = m_iPlayersInArena[i];
			m_iTeamPlayerCount[t]++;
		}
	}

	// Pick a unique info_player_deathmatch per team
	CBaseEntity *pUsedSpots[4] = { NULL, NULL, NULL, NULL };
	for ( int t = 0; t < 4; t++ )
	{
		if ( m_iTeamPlayerCount[t] == 0 ) continue;

		CBaseEntity *pSpot = NULL;
		while ( (pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch")) != NULL )
		{
			BOOL used = FALSE;
			for ( int k = 0; k < t; k++ )
				if ( pUsedSpots[k] == pSpot ) { used = TRUE; break; }

			if ( !used ) { pUsedSpots[t] = pSpot; break; }
		}

		if ( !pUsedSpots[t] )
		{
			// Fallback: first available spot
			pUsedSpots[t] = UTIL_FindEntityByClassname( NULL, "info_player_deathmatch" );
		}

		m_vecTeamSpawnOrigin[t] = pUsedSpots[t] ? pUsedSpots[t]->pev->origin : g_vecZero;

		// Assign team name and loot-team index to each player on this team
		for ( int j = 0; j < m_iTeamPlayerCount[t]; j++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( m_iTeamPlayers[t][j] );
			if ( !plr ) continue;

			plr->m_iLootTeam = t;
			strncpy( plr->m_szTeamName, s_TeamNames[t], TEAM_NAME_LENGTH );
		}
	}

	// Set SOLID_NOT before spawning to suppress telefrag during team grouping.
	// TODO: FALLBACK -- if this causes a crash, remove the lines below and accept
	// a brief overlap; restore the pev->solid = SOLID_SLIDEBOX line in Think as well.
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
			plr->pev->solid = SOLID_NOT;
	}
	m_fSpawnGroupSolidRestoreTime = gpGlobals->time + 3.0f;

	// Set g_GameInProgress TRUE before InsertClientsIntoArena so that the
	// PlayerSpawn path (ExitObserver -> Spawn -> PlayerSpawn) does not hit
	// the "!g_GameInProgress && IsRoundBased()" early-return guard, which
	// would skip giving the player fists and other spawn setup.
	g_GameInProgress = TRUE;

	InsertClientsIntoArena( 0 );

	SpawnLootEntities();

	// Broadcast team names (4 slots)
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamNames );
		WRITE_BYTE( 4 );
		WRITE_STRING( "iceman" );
		WRITE_STRING( "santa" );
		WRITE_STRING( "holo" );
		WRITE_STRING( "commando" );
	MESSAGE_END();

	// Update all player HUDs
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !plr || !plr->IsPlayer() ) continue;

		MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
			WRITE_BYTE( ENTINDEX(plr->edict()) );
			WRITE_STRING( plr->m_szTeamName );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE ( ENTINDEX(plr->edict()) );
			WRITE_SHORT( plr->pev->frags );
			WRITE_SHORT( plr->m_iDeaths );
			WRITE_SHORT( plr->m_iRoundWins );
			WRITE_SHORT( GetTeamIndex(plr->m_szTeamName) + 1 );
		MESSAGE_END();
	}

	g_GameInProgress      = TRUE;  // already set before InsertClientsIntoArena; kept for clarity
	m_iCountDown          = 5;
	m_fWaitForPlayersTime = -1;
	m_fSendBannerTime     = gpGlobals->time + 1.0f;

	UTIL_ClientPrintAll( HUD_PRINTTALK,
	    UTIL_VarArgs("[Loot] %d players have entered! Find and break crates!\n", clients) );
}

// ============================================================
// CleanupRoundEntities
// ============================================================
void CHalfLifeLoot::CleanupRoundEntities( void )
{
	edict_t *pEdict;

	pEdict = FIND_ENTITY_BY_CLASSNAME( NULL, "loot_crate" );
	while ( !FNullEnt(pEdict) )
	{
		UTIL_Remove( CBaseEntity::Instance(pEdict) );
		pEdict = FIND_ENTITY_BY_CLASSNAME( pEdict, "loot_crate" );
	}

	pEdict = FIND_ENTITY_BY_CLASSNAME( NULL, "loot_entity" );
	while ( !FNullEnt(pEdict) )
	{
		UTIL_Remove( CBaseEntity::Instance(pEdict) );
		pEdict = FIND_ENTITY_BY_CLASSNAME( pEdict, "loot_entity" );
	}

	pEdict = FIND_ENTITY_BY_CLASSNAME( NULL, "loot_goal" );
	while ( !FNullEnt(pEdict) )
	{
		UTIL_Remove( CBaseEntity::Instance(pEdict) );
		pEdict = FIND_ENTITY_BY_CLASSNAME( pEdict, "loot_goal" );
	}
}

// ============================================================
// CaptureCharm  (CTC-style — single authority for loot pickup)
// ============================================================
void CHalfLifeLoot::CaptureCharm( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsAlive() ) return;
	if ( pPlayer->m_iLootTeam < 0 || pPlayer->m_iLootTeam > 3 ) return;

	// Hide the loot entity so it can't be double-picked
	CLootEntity *pLootEnt = (CLootEntity *)((CBaseEntity *)m_hLootEntity);
	if ( pLootEnt )
	{
		//pLootEnt->pev->effects |= EF_NODRAW;
		//pLootEnt->pev->solid    = SOLID_NOT;
		pLootEnt->SetTouch( NULL );
		pLootEnt->pev->movetype = MOVETYPE_FOLLOW;
		pLootEnt->pev->aiment = pPlayer->edict();
	}

	m_hLootHolder      = pPlayer;
	m_flLootPickupTime = gpGlobals->time;
	pPlayer->m_bHoldingLoot = TRUE;

	// Green glow on holder
	pPlayer->pev->renderfx    = kRenderFxGlowShell;
	pPlayer->pev->renderamt   = 10;
	pPlayer->pev->rendercolor = Vector(0, 200, 0);
	pPlayer->pev->fuser4      = RADAR_LOOT;

	// Attach a green beam trail to the holder
	int iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE ( TE_BEAMFOLLOW );
		WRITE_SHORT( pPlayer->entindex() );
		WRITE_SHORT( iTrail );
		WRITE_BYTE ( 50 );   // life
		WRITE_BYTE ( 3  );   // width
		WRITE_BYTE ( 0  );   // r
		WRITE_BYTE ( 200 );  // g
		WRITE_BYTE ( 0  );   // b
		WRITE_BYTE ( 200 );  // brightness
	MESSAGE_END();

	// Status icon on holder and all alive teammates
	int teamIdx = pPlayer->m_iLootTeam;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !plr || !plr->IsPlayer() || plr->HasDisconnected )
			continue;
		if ( FBitSet(plr->pev->flags, FL_FAKECLIENT) )
			continue;

		if ( plr == pPlayer || (plr->IsAlive() && plr->m_iLootTeam == teamIdx) )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, plr->edict() );
				WRITE_BYTE( 1 );          // enable
				WRITE_STRING( "loot" );
				WRITE_BYTE( 0   );
				WRITE_BYTE( 180 );
				WRITE_BYTE( 0   );
			MESSAGE_END();
		}
	}

	// Give 3 random weapons to holder and all alive teammates
	GiveTeamWeapons( teamIdx );

	// Scoreboard refresh for holder
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_STRING( pPlayer->m_szTeamName );
	MESSAGE_END();
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE ( ENTINDEX(pPlayer->edict()) );
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iRoundWins );
		WRITE_SHORT( GetTeamIndex(pPlayer->m_szTeamName) + 1 );
	MESSAGE_END();

	UTIL_ClientPrintAll( HUD_PRINTTALK,
	    UTIL_VarArgs("[Loot] %s has the loot!\n", STRING(pPlayer->pev->netname)) );

	MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
		WRITE_BYTE( CLIENT_SOUND_BULLSEYE );
	MESSAGE_END();

	// Force an objective update immediately
	m_flLastObjUpdate = 0;
}

// ============================================================
// DropCharm  (CTC-style — single authority for loot release)
// ============================================================
CBaseEntity *CHalfLifeLoot::DropCharm( CBasePlayer *pPlayer, Vector origin )
{
	if ( !pPlayer ) return NULL;

	// Flush hold-time into team totals
	if ( m_flLootPickupTime > 0 && pPlayer->m_iLootTeam >= 0 && pPlayer->m_iLootTeam < 4 )
	{
		m_flTeamHoldTime[pPlayer->m_iLootTeam] += gpGlobals->time - m_flLootPickupTime;
		m_flLootPickupTime = 0;
	}

	// Clear holder visual effects
	pPlayer->pev->rendermode = kRenderNormal;
	pPlayer->pev->renderfx   = kRenderFxNone;
	pPlayer->pev->renderamt  = 0;
	pPlayer->pev->fuser4     = 0;
	pPlayer->m_bHoldingLoot  = FALSE;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE ( TE_KILLBEAM );
		WRITE_SHORT( pPlayer->entindex() );
	MESSAGE_END();

	// Disable status icon on holder and all teammates
	int teamIdx = pPlayer->m_iLootTeam;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !plr || !plr->IsPlayer() || plr->HasDisconnected )
			continue;
		if ( FBitSet(plr->pev->flags, FL_FAKECLIENT) )
			continue;

		if ( plr == pPlayer || plr->m_iLootTeam == teamIdx )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, plr->edict() );
				WRITE_BYTE( 0 );          // disable
				WRITE_STRING( "loot" );
			MESSAGE_END();
		}
	}

	// Alive teammates lose the loot advantage -> reduce to 1 weapon
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() && plr != pPlayer &&
		     plr->IsAlive() && plr->m_iLootTeam == teamIdx )
		{
			ReduceToOneWeapon( plr );
		}
	}

	m_hLootHolder = (CBaseEntity *)NULL;

	// Re-release the loot entity into the world
	CLootEntity *pLootEnt = (CLootEntity *)((CBaseEntity *)m_hLootEntity);
	if ( pLootEnt )
	{
		pLootEnt->Drop( origin );
	}
	else
	{
		// Fallback: create a fresh entity (handles edge case where entity was removed)
		SpawnLootAtPosition( origin );
	}

	UTIL_ClientPrintAll( HUD_PRINTTALK,
	    UTIL_VarArgs("[Loot] %s dropped the loot!\n", STRING(pPlayer->pev->netname)) );

	MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
		WRITE_BYTE( CLIENT_SOUND_MANIAC );
	MESSAGE_END();

	// Force objective update
	m_flLastObjUpdate = 0;

	return (CBaseEntity *)m_hLootEntity;
}

// ============================================================
// OnGoalReached
// ============================================================
void CHalfLifeLoot::OnGoalReached( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsAlive() )     return;
	if ( !pPlayer->m_bHoldingLoot )             return;
	if ( !g_GameInProgress )                    return;

	int teamIdx = pPlayer->m_iLootTeam;

	// Flush any remaining hold time
	if ( m_flLootPickupTime > 0 && teamIdx >= 0 && teamIdx < 4 )
		m_flTeamHoldTime[teamIdx] += gpGlobals->time - m_flLootPickupTime;
	m_flLootPickupTime = 0;

	// Award 2 points to the scorer
	pPlayer->m_iRoundWins += 2;
	pPlayer->Celebrate();
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE ( ENTINDEX(pPlayer->edict()) );
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iRoundWins );
		WRITE_SHORT( GetTeamIndex(pPlayer->m_szTeamName) + 1 );
	MESSAGE_END();

	// Award 1 point to each alive teammate
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() && plr != pPlayer &&
		     plr->IsAlive() && plr->m_iLootTeam == teamIdx )
		{
			plr->m_iRoundWins++;
			plr->Celebrate();
			MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
				WRITE_BYTE ( ENTINDEX(plr->edict()) );
				WRITE_SHORT( plr->pev->frags );
				WRITE_SHORT( plr->m_iDeaths );
				WRITE_SHORT( plr->m_iRoundWins );
				WRITE_SHORT( GetTeamIndex(plr->m_szTeamName) + 1 );
			MESSAGE_END();
		}
	}

	m_hLootHolder = (CBaseEntity *)NULL;
	pPlayer->m_bHoldingLoot = FALSE;
	pPlayer->pev->fuser4 = 0;

	UTIL_ClientPrintAll( HUD_PRINTCENTER,
	    UTIL_VarArgs("%s scored for team %s!\n",
	    STRING(pPlayer->pev->netname), s_TeamNames[teamIdx]) );
	UTIL_ClientPrintAll( HUD_PRINTTALK,
	    UTIL_VarArgs("[Loot] %s delivered the loot! Team %s wins!\n",
	    STRING(pPlayer->pev->netname), s_TeamNames[teamIdx]) );

	MESSAGE_BEGIN( MSG_BROADCAST, gmsgObjective );
		WRITE_STRING( "Loot Scored!" );
		WRITE_STRING( UTIL_VarArgs("Team %s wins!", s_TeamNames[teamIdx]) );
		WRITE_BYTE( 0 );
		WRITE_STRING( UTIL_VarArgs("%s delivered the loot!", STRING(pPlayer->pev->netname)) );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
		WRITE_BYTE( CLIENT_SOUND_OUTSTANDING );
	MESSAGE_END();

	// Defer EndRound by 4 seconds so players can celebrate before being reset.
	m_iPendingWinnerTeam  = teamIdx;
	m_flCelebrationEndTime = gpGlobals->time + 4.0f;
}

// ============================================================
// EndRound
//   winningTeam >= 0: goal was already scored by OnGoalReached
//   winningTeam  = -1: tiebreaker (timer expired / all dead)
// ============================================================
void CHalfLifeLoot::EndRound( int winningTeam )
{
	if ( g_fGameOver ) return;

	g_GameInProgress  = FALSE;
	m_flRoundTimeLimit = 0;
	PauseMutators();

	MESSAGE_BEGIN( MSG_ALL, gmsgShowTimer );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	// --- Tiebreaker path ---
	if ( winningTeam == -1 )
	{
		// Flush any hold-time still accumulating
		CBaseEntity *pHolderEnt = (CBaseEntity *)m_hLootHolder;
		if ( pHolderEnt )
		{
			CBasePlayer *pHolder = (CBasePlayer *)pHolderEnt;
			if ( m_flLootPickupTime > 0 && pHolder->m_iLootTeam >= 0 )
			{
				m_flTeamHoldTime[pHolder->m_iLootTeam] += gpGlobals->time - m_flLootPickupTime;
				m_flLootPickupTime = 0;
			}
		}

		// Find best team
		int   bestTeam = -1;
		float bestTime = 0;
		BOOL  tied     = FALSE;

		for ( int t = 0; t < 4; t++ )
		{
			if ( m_iTeamPlayerCount[t] == 0 ) continue;
			if ( m_flTeamHoldTime[t] > bestTime )
			{
				bestTime = m_flTeamHoldTime[t];
				bestTeam = t;
				tied     = FALSE;
			}
			else if ( m_flTeamHoldTime[t] > 0 && m_flTeamHoldTime[t] == bestTime )
			{
				tied = TRUE;
			}
		}

		if ( bestTeam >= 0 && !tied )
		{
			winningTeam = bestTeam;

			// Award 1 survival point to alive members of winning team
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
				if ( plr && plr->IsPlayer() && plr->IsInArena &&
				     plr->IsAlive() && plr->m_iLootTeam == winningTeam )
				{
					plr->m_iRoundWins++;
					plr->Celebrate();
					MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
						WRITE_BYTE ( ENTINDEX(plr->edict()) );
						WRITE_SHORT( plr->pev->frags );
						WRITE_SHORT( plr->m_iDeaths );
						WRITE_SHORT( plr->m_iRoundWins );
						WRITE_SHORT( GetTeamIndex(plr->m_szTeamName) + 1 );
					MESSAGE_END();
				}
			}

			UTIL_ClientPrintAll( HUD_PRINTCENTER,
			    UTIL_VarArgs("Team %s wins!\n", s_TeamNames[winningTeam]) );
			UTIL_ClientPrintAll( HUD_PRINTTALK,
			    UTIL_VarArgs("[Loot] Team %s wins by hold time (%.0fs)!\n",
			    s_TeamNames[winningTeam], bestTime) );
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgObjective );
				WRITE_STRING( "Loot Complete!" );
				WRITE_STRING( UTIL_VarArgs("Team %s wins!", s_TeamNames[winningTeam]) );
				WRITE_BYTE( 0 );
				WRITE_STRING( UTIL_VarArgs("Hold time: %.0fs", bestTime) );
			MESSAGE_END();
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE( CLIENT_SOUND_OUTSTANDING );
			MESSAGE_END();
		}
		else
		{
			// No one held the loot, or exact tie
			UTIL_ClientPrintAll( HUD_PRINTCENTER, "No winner this round!\n" );
			UTIL_ClientPrintAll( HUD_PRINTTALK, "[Loot] Round ends with no winner!\n" );
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgObjective );
				WRITE_STRING( "Loot Complete!" );
				WRITE_STRING( "No winner this round" );
				WRITE_BYTE( 0 );
				WRITE_STRING( "" );
			MESSAGE_END();
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE( CLIENT_SOUND_HULIMATING_DEAFEAT );
			MESSAGE_END();
		}
	}

	// --- Common cleanup ---

	// Deactivate all SpecEnt slots
	for ( int s = 0; s < 8; s++ )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgSpecialEntity );
			WRITE_BYTE( s );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	// Remove status icons and visual effects from every player
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !plr || !plr->IsPlayer() ) continue;

		if ( !FBitSet(plr->pev->flags, FL_FAKECLIENT) )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, plr->edict() );
				WRITE_BYTE( 0 );
				WRITE_STRING( "loot" );
			MESSAGE_END();
		}

		plr->m_bHoldingLoot = FALSE;
		plr->pev->fuser4    = 0;
		plr->pev->renderfx  = kRenderFxNone;
		plr->pev->rendermode = kRenderNormal;
		plr->pev->renderamt  = 0;
	}

	// Kill all beam trails (belt and suspenders)
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() )
		{
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE ( TE_KILLBEAM );
				WRITE_SHORT( plr->entindex() );
			MESSAGE_END();
		}
	}

	CleanupRoundEntities();

	m_hLootEntity  = (CBaseEntity *)NULL;
	m_hGoalEntity  = (CBaseEntity *)NULL;
	m_hLootHolder  = (CBaseEntity *)NULL;
	m_flLootPickupTime = 0;

	memset( m_flTeamHoldTime,    0, sizeof(m_flTeamHoldTime) );
	memset( m_iTeamPlayers,      0, sizeof(m_iTeamPlayers) );
	memset( m_iTeamPlayerCount,  0, sizeof(m_iTeamPlayerCount) );
	m_iUsedSpotCount = 0;

	// Reset per-player loot state
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() )
			plr->m_iLootTeam = -1;
	}

	// Update every player's objective panel with the round result
	{
		const char *pWinner = (winningTeam >= 0) ? s_TeamNames[winningTeam] : NULL;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( !plr || !plr->IsPlayer() || FBitSet(plr->pev->flags, FL_FAKECLIENT) ) continue;
			MESSAGE_BEGIN( MSG_ONE, gmsgObjective, NULL, plr->edict() );
				WRITE_STRING( "Round Over" );
				WRITE_STRING( pWinner ? UTIL_VarArgs("Team %s wins", pWinner) : "No winner" );
				WRITE_BYTE( 0 );
			MESSAGE_END();
		}
	}

	SuckAllToSpectator();

	m_iSuccessfulRounds++;
	flUpdateTime      = gpGlobals->time + 3.0f;
	m_fWaitForPlayersTime = -1;
}

// ============================================================
// Helper: weapon management
// ============================================================
int CHalfLifeLoot::CountNonFistWeapons( CBasePlayer *pPlayer )
{
	int count = 0;
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		CBasePlayerItem *pItem = pPlayer->m_rgpPlayerItems[i];
		while ( pItem )
		{
			if ( strcmp(STRING(pItem->pev->classname), "weapon_fists") != 0 )
				count++;
			pItem = pItem->m_pNext;
		}
	}
	return count;
}

void CHalfLifeLoot::GiveTeamWeapons( int teamIndex )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( plr && plr->IsPlayer() && plr->IsAlive() && plr->m_iLootTeam == teamIndex )
		{
			plr->GiveRandomWeapon("weapon_nuke");
			plr->GiveRandomWeapon("weapon_nuke");
			plr->GiveRandomWeapon("weapon_nuke");
		}
	}
}

void CHalfLifeLoot::StripToFists( CBasePlayer *pPlayer )
{
	pPlayer->RemoveAllItems( FALSE );
	pPlayer->GiveNamedItem("weapon_fists");
}

void CHalfLifeLoot::ReduceToOneWeapon( CBasePlayer *pPlayer )
{
	// Save active weapon classname (skip fists)
	char activeClass[64] = "";
	if ( pPlayer->m_pActiveItem )
	{
		const char *cls = STRING(pPlayer->m_pActiveItem->pev->classname);
		if ( strcmp(cls, "weapon_fists") != 0 )
			strncpy( activeClass, cls, sizeof(activeClass) - 1 );
	}

	// RemoveAllItems(FALSE) calls Drop() on each weapon which schedules SUB_Remove --
	// no world weaponbox is created.
	pPlayer->RemoveAllItems( FALSE );
	pPlayer->GiveNamedItem("weapon_fists");
	if ( strlen(activeClass) )
		pPlayer->GiveNamedItem( activeClass );
}

// ============================================================
// SendObjectiveUpdate  (rate-limited per-client HUD messages)
// ============================================================
void CHalfLifeLoot::SendObjectiveUpdate( void )
{
	if ( m_flLastObjUpdate > gpGlobals->time - 2.0f )
		return;
	m_flLastObjUpdate = gpGlobals->time;

	CBaseEntity *pHolderEnt = (CBaseEntity *)m_hLootHolder;
	CBasePlayer *pHolder    = pHolderEnt ? (CBasePlayer *)pHolderEnt : NULL;

	CBaseEntity *pLootEnt = (CBaseEntity *)m_hLootEntity;
	BOOL lootFree = pLootEnt && !(pLootEnt->pev->effects & EF_NODRAW);

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !plr || !plr->IsPlayer() || plr->HasDisconnected )
			continue;
		if ( FBitSet(plr->pev->flags, FL_FAKECLIENT) )
			continue;

		const char *title  = "Loot";
		const char *detail = "";

		if ( !pHolder )
		{
			if ( lootFree )
			{
				title  = "Loot is free!";
				detail = "Grab the loot!";
			}
			else
			{
				title  = "Find the loot";
				detail = "Break crates to find loot!";
			}
		}
		else if ( plr->m_iLootTeam == pHolder->m_iLootTeam )
		{
			if ( plr == pHolder )
				detail = "Get to the goal!";
			else
				detail = UTIL_VarArgs("Cover %s!", STRING(pHolder->pev->netname));
			title = "Get to the goal!";
		}
		else
		{
			title  = UTIL_VarArgs("Frag %s!", STRING(pHolder->pev->netname));
			detail = UTIL_VarArgs("%s has the loot!", STRING(pHolder->pev->netname));
		}

		MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict() );
			WRITE_STRING( title );
			WRITE_STRING( detail );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
}

// ============================================================
// Think
// ============================================================
void CHalfLifeLoot::Think( void )
{
	CHalfLifeMultiplay::Think();

	if ( flUpdateTime > gpGlobals->time ) return;

	CheckRounds();

	// No further work during intermission
	if ( m_flIntermissionEndTime )
	{
		CleanupRoundEntities();
		return;
	}

	// Fire deferred EndRound after the goal-scored celebration window
	if ( m_flCelebrationEndTime > 0 && m_flCelebrationEndTime <= gpGlobals->time )
	{
		m_flCelebrationEndTime = 0;
		EndRound( m_iPendingWinnerTeam );
		m_iPendingWinnerTeam = -1;
		return;
	}

	if ( m_flRoundTimeLimit )
	{
		if ( HasGameTimerExpired() ) return;
	}

	// Restore SOLID_SLIDEBOX after spawn grouping window
	if ( m_fSpawnGroupSolidRestoreTime > 0 &&
	     m_fSpawnGroupSolidRestoreTime < gpGlobals->time )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( plr && plr->IsPlayer() && plr->IsInArena &&
			     plr->IsAlive() && plr->pev->solid == SOLID_NOT )
			{
				plr->pev->solid = SOLID_SLIDEBOX;
			}
		}
		m_fSpawnGroupSolidRestoreTime = 0;
	}

	// Round-start banner (sent 1 second after spawn)
	if ( m_fSendBannerTime > 0 && m_fSendBannerTime < gpGlobals->time )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( plr && plr->IsPlayer() && !plr->HasDisconnected &&
			     !FBitSet(plr->pev->flags, FL_FAKECLIENT) )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgBanner, NULL, plr->edict() );
					WRITE_STRING( "Loot" );
					WRITE_STRING( "Partner with that guy next to you, and score the loot!" );
					WRITE_BYTE( 80 );
				MESSAGE_END();
			}
		}
		m_fSendBannerTime = 0;
	}

	if ( g_GameInProgress )
	{
		// Count alive players and handle spectator transitions
		int teamAlive[4]  = { 0, 0, 0, 0 };
		int totalAlive    = 0;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( !plr || !plr->IsPlayer() || plr->HasDisconnected ) continue;

			if ( plr->m_flForceToObserverTime &&
			     plr->m_flForceToObserverTime < gpGlobals->time )
			{
				SuckToSpectator( plr );
				plr->m_flForceToObserverTime = 0;
			}

			if ( plr->IsInArena && plr->IsAlive() )
			{
				totalAlive++;
				if ( plr->m_iLootTeam >= 0 && plr->m_iLootTeam < 4 )
					teamAlive[plr->m_iLootTeam]++;
			}
			else if ( !plr->IsSpectator() )
			{
				plr->m_flForceToObserverTime = gpGlobals->time;
			}
		}

		SendObjectiveUpdate();

		// Determine how many teams still have living players
		int teamsAlive  = 0;
		int lastTeamIdx = -1;
		for ( int t = 0; t < 4; t++ )
		{
			if ( teamAlive[t] > 0 ) { teamsAlive++; lastTeamIdx = t; }
		}

		if ( totalAlive == 0 || teamsAlive <= 1 )
		{
			// If exactly one team remains, they win by survival
			EndRound( lastTeamIdx );
			return;
		}

		flUpdateTime = gpGlobals->time + 1.5f;
		return;
	}

	// ---- Pre-game phase ----
	int clients = CheckClients();
	const int MIN_PLAYERS = 4;

	if ( m_fWaitForPlayersTime == -1 )
	{
		m_fWaitForPlayersTime = gpGlobals->time + roundwaittime.value;
		RemoveAndFillItems();
		extern void ClearBodyQue();
		ClearBodyQue();
		MESSAGE_BEGIN( MSG_ALL, gmsgDEraser ); MESSAGE_END();
	}

	if ( clients >= MIN_PLAYERS )
	{
		if ( m_fWaitForPlayersTime > gpGlobals->time )
		{
			SuckAllToSpectator();
			flUpdateTime = gpGlobals->time + 1.0f;
			UTIL_ClientPrintAll( HUD_PRINTCENTER,
			    UTIL_VarArgs("Battle will begin in %.0f\n",
			    (m_fWaitForPlayersTime + 5) - gpGlobals->time) );
			return;
		}

		if ( m_iCountDown > 0 )
		{
			// Reset countdown if numbers drop below the minimum mid-count
			if ( clients < MIN_PLAYERS )
			{
				m_iCountDown        = 5;
				m_fWaitForPlayersTime = gpGlobals->time + roundwaittime.value;
				UTIL_ClientPrintAll( HUD_PRINTTALK, "[Loot] Need at least 4 players to start.\n" );
				flUpdateTime = gpGlobals->time + 1.0f;
				return;
			}

			if      ( m_iCountDown == 2 ) { MESSAGE_BEGIN(MSG_BROADCAST,gmsgPlayClientSound); WRITE_BYTE(CLIENT_SOUND_PREPAREFORBATTLE); MESSAGE_END(); }
			else if ( m_iCountDown == 3 ) { MESSAGE_BEGIN(MSG_BROADCAST,gmsgPlayClientSound); WRITE_BYTE(CLIENT_SOUND_THREE);             MESSAGE_END(); }
			else if ( m_iCountDown == 4 ) { MESSAGE_BEGIN(MSG_BROADCAST,gmsgPlayClientSound); WRITE_BYTE(CLIENT_SOUND_FOUR);              MESSAGE_END(); }
			else if ( m_iCountDown == 5 ) { MESSAGE_BEGIN(MSG_BROADCAST,gmsgPlayClientSound); WRITE_BYTE(CLIENT_SOUND_FIVE);              MESSAGE_END(); }

			SuckAllToSpectator();
			UTIL_ClientPrintAll( HUD_PRINTCENTER,
			    UTIL_VarArgs("Prepare for Loot\n\n%d...\n", m_iCountDown) );
			m_iCountDown--;
			m_iFirstBloodDecided = FALSE;
			flUpdateTime = gpGlobals->time + 1.0f;
			return;
		}

		StartRound( clients );
	}
	else
	{
		m_iCountDown = 5;
		SuckAllToSpectator();
		m_flRoundTimeLimit = 0;
		MESSAGE_BEGIN( MSG_BROADCAST, gmsgObjective );
			WRITE_STRING( "Loot" );
			WRITE_STRING( "Waiting for players (4 minimum)" );
			WRITE_BYTE( 0 );
			if ( roundlimit.value > 0 )
				WRITE_STRING( UTIL_VarArgs("%d Rounds", (int)roundlimit.value) );
		MESSAGE_END();
		m_fWaitForPlayersTime = gpGlobals->time + roundwaittime.value;
	}

	flUpdateTime = gpGlobals->time + 1.0f;
}

// ============================================================
// HasGameTimerExpired  (called when the round timer runs out)
// ============================================================
BOOL CHalfLifeLoot::HasGameTimerExpired( void )
{
	if ( CHalfLifeMultiplay::HasGameTimerExpired() )
	{
		// Flush remaining hold time for whoever is currently holding
		CBaseEntity *pHolderEnt = (CBaseEntity *)m_hLootHolder;
		if ( pHolderEnt )
		{
			CBasePlayer *pHolder = (CBasePlayer *)pHolderEnt;
			if ( m_flLootPickupTime > 0 && pHolder->m_iLootTeam >= 0 )
			{
				m_flTeamHoldTime[pHolder->m_iLootTeam] += gpGlobals->time - m_flLootPickupTime;
				m_flLootPickupTime = 0;
			}
		}

		EndRound( -1 ); // Tiebreaker
		return TRUE;
	}
	return FALSE;
}

// ============================================================
// InitHUD
// ============================================================
void CHalfLifeLoot::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if ( !FBitSet(pPlayer->pev->flags, FL_FAKECLIENT) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgObjective, NULL, pPlayer->edict() );
			WRITE_STRING( "Loot" );
			if ( roundlimit.value > 0 )
				WRITE_STRING( UTIL_VarArgs("Round %d of %d",
				              m_iSuccessfulRounds + 1, (int)roundlimit.value) );
			else
				WRITE_STRING( "" );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict() );
			WRITE_BYTE( 4 );
			WRITE_STRING( "iceman" );
			WRITE_STRING( "santa" );
			WRITE_STRING( "holo" );
			WRITE_STRING( "commando" );
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

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE ( ENTINDEX(pPlayer->edict()) );
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iRoundWins );
		WRITE_SHORT( GetTeamIndex(pPlayer->m_szTeamName) + 1 );
	MESSAGE_END();

	// Re-send the goal indicator if a round is in progress
	if ( g_GameInProgress )
	{
		CBaseEntity *pGoal = (CBaseEntity *)m_hGoalEntity;
		if ( pGoal && !FBitSet(pPlayer->pev->flags, FL_FAKECLIENT) )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgSpecialEntity, NULL, pPlayer->edict() );
				WRITE_BYTE ( 0 );
				WRITE_BYTE ( 1 );
				WRITE_COORD( pGoal->pev->origin.x );
				WRITE_COORD( pGoal->pev->origin.y );
				WRITE_COORD( pGoal->pev->origin.z );
				WRITE_BYTE ( RADAR_BASE_RED );
			MESSAGE_END();
		}
	}
}

// ============================================================
// PlayerSpawn
// ============================================================
void CHalfLifeLoot::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn( pPlayer );
	CHalfLifeMultiplay::SavePlayerModel( pPlayer );

	// Latecomers and pre-round spectators wait
	if ( (g_GameInProgress && !pPlayer->IsInArena) ||
	     (!g_GameInProgress && IsRoundBased()) )
	{
		return;
	}

	// Give only fists at spawn
	pPlayer->RemoveAllItems( FALSE );
	pPlayer->GiveNamedItem("weapon_fists");

	// Position team members together around their chosen spawn point
	if ( pPlayer->m_iLootTeam >= 0 && pPlayer->m_iLootTeam < 4 )
	{
		int teamIdx = pPlayer->m_iLootTeam;

		// Determine this player's index within the team (0 = first member)
		int posInTeam = 0;
		for ( int j = 0; j < m_iTeamPlayerCount[teamIdx]; j++ )
		{
			if ( m_iTeamPlayers[teamIdx][j] == pPlayer->entindex() )
			{
				posInTeam = j;
				break;
			}
		}

		static const Vector offsets[4] = {
			Vector(  0,   0, 0),
			Vector( 48,   0, 0),
			Vector(-48,   0, 0),
			Vector(  0,  48, 0),
		};

		Vector baseOrigin = m_vecTeamSpawnOrigin[teamIdx];
		Vector spawnPos   = baseOrigin + offsets[posInTeam % 4];

		// Try to find a non-stuck offset; rotate through if blocked
		BOOL placed = FALSE;
		for ( int o = 0; o < 4 && !placed; o++ )
		{
			Vector tryPos = baseOrigin + offsets[(posInTeam + o) % 4];
			TraceResult tr;
			UTIL_TraceHull( tryPos, tryPos, dont_ignore_monsters, human_hull,
			                ENT(pPlayer->pev), &tr );
			if ( !tr.fStartSolid ) { spawnPos = tryPos; placed = TRUE; }
		}

		UTIL_SetOrigin( pPlayer->pev, spawnPos );

		// Enforce the team model so everyone on the same team looks the part
		char *key = g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() );
		g_engfuncs.pfnSetClientKeyValue(
		    ENTINDEX(pPlayer->edict()), key, "model", (char *)s_TeamNames[teamIdx] );
	}

	// Broadcast team assignment
	if ( pPlayer->m_iLootTeam >= 0 )
		strncpy( pPlayer->m_szTeamName, s_TeamNames[pPlayer->m_iLootTeam], TEAM_NAME_LENGTH );

	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_STRING( pPlayer->m_szTeamName );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE ( ENTINDEX(pPlayer->edict()) );
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iRoundWins );
		WRITE_SHORT( GetTeamIndex(pPlayer->m_szTeamName) + 1 );
	MESSAGE_END();

	// Schedule the Loot game-mode banner (0.5 s after spawn)
	pPlayer->m_iShowGameModeMessage = (int)(gpGlobals->time + 0.5f);
}

// ============================================================
// PlayerThink
// ============================================================
void CHalfLifeLoot::PlayerThink( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerThink( pPlayer );

	if ( m_flIntermissionEndTime ) return;

	if ( !g_fGameOver && scorelimit.value > 0 )
	{
		if ( pPlayer->m_iRoundWins >= (int)scorelimit.value )
		{
			GoToIntermission();
			return;
		}
	}

	// Game-mode welcome banner for new/rejoining players
	if ( pPlayer->m_iShowGameModeMessage > -1 &&
	     pPlayer->m_iShowGameModeMessage < (int)gpGlobals->time &&
	     !FBitSet(pPlayer->pev->flags, FL_FAKECLIENT) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgBanner, NULL, pPlayer->edict() );
			WRITE_STRING( "Loot" );
			WRITE_STRING( "Partner with that guy next to you, and score the loot!" );
			WRITE_BYTE( 80 );
		MESSAGE_END();
		pPlayer->m_iShowGameModeMessage = -1;
	}
}

// ============================================================
// PlayerKilled
// ============================================================
void CHalfLifeLoot::PlayerKilled( CBasePlayer *pVictim,
                                   entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );

	if ( pVictim->m_bHoldingLoot )
	{
		UTIL_MakeVectors( pVictim->pev->v_angle );
		DropCharm( pVictim, pVictim->pev->origin + gpGlobals->v_forward * 32 );
	}

	if ( !pVictim->m_flForceToObserverTime )
		pVictim->m_flForceToObserverTime = gpGlobals->time + 3.0f;
}

// ============================================================
// ClientDisconnected
// ============================================================
void CHalfLifeLoot::ClientDisconnected( edict_t *pClient )
{
	CHalfLifeMultiplay::ClientDisconnected( pClient );

	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );
		if ( pPlayer && pPlayer->m_bHoldingLoot )
			DropCharm( pPlayer, pPlayer->pev->origin );
	}
}

// ============================================================
// ClientUserInfoChanged  (enforce team model during active round)
// ============================================================
void CHalfLifeLoot::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	if ( !g_GameInProgress ) return;
	if ( pPlayer->m_iLootTeam < 0 || pPlayer->m_iLootTeam > 3 ) return;

	const char *teamModel = s_TeamNames[pPlayer->m_iLootTeam];
	const char *reqModel  = g_engfuncs.pfnInfoKeyValue( infobuffer, "model" );

	if ( reqModel && stricmp(reqModel, teamModel) != 0 )
	{
		g_engfuncs.pfnSetClientKeyValue(
		    pPlayer->entindex(), infobuffer, "model", (char *)teamModel );
	}
}

// ============================================================
// FPlayerCanRespawn  (no respawning in Loot rounds)
// ============================================================
BOOL CHalfLifeLoot::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	if ( !pPlayer->IsAlive() && !pPlayer->m_flForceToObserverTime )
		pPlayer->m_flForceToObserverTime = gpGlobals->time + 3.0f;
	return FALSE;
}

// ============================================================
// FPlayerCanTakeDamage  (standard FF rules; trigger_hurt always fires)
// ============================================================
BOOL CHalfLifeLoot::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	// trigger_hurt always applies
	if ( pAttacker && strcmp(STRING(pAttacker->pev->classname), "trigger_hurt") == 0 )
		return TRUE;

	// Friendly fire between teammates
	if ( pAttacker && pAttacker->IsPlayer() )
	{
		CBasePlayer *pAtk = (CBasePlayer *)pAttacker;
		if ( pAtk != pPlayer &&
		     pAtk->m_iLootTeam >= 0 &&
		     pAtk->m_iLootTeam == pPlayer->m_iLootTeam )
		{
			if ( friendlyfire.value == 0 )
				return FALSE;
		}
	}

	return CHalfLifeMultiplay::FPlayerCanTakeDamage( pPlayer, pAttacker );
}

// ============================================================
// CanHavePlayerItem  (enforce 1-weapon / 3-weapon limit)
// ============================================================
BOOL CHalfLifeLoot::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	// Fists are always allowed
	if ( !strcmp(STRING(pItem->pev->classname), "weapon_fists") )
		return CHalfLifeMultiplay::CanHavePlayerItem( pPlayer, pItem );

	if ( g_GameInProgress )
	{
		// Determine whether this player benefits from loot-advantage (3-weapon rule)
		BOOL hasLootAdvantage = pPlayer->m_bHoldingLoot;
		if ( !hasLootAdvantage && pPlayer->m_iLootTeam >= 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
				if ( plr && plr->IsPlayer() && plr != pPlayer &&
				     plr->IsAlive() && plr->m_iLootTeam == pPlayer->m_iLootTeam &&
				     plr->m_bHoldingLoot )
				{
					hasLootAdvantage = TRUE;
					break;
				}
			}
		}

		int maxWeapons = hasLootAdvantage ? 3 : 1;
		if ( CountNonFistWeapons(pPlayer) >= maxWeapons )
			return FALSE;  // At limit; +use swap in player.cpp handles the swap
	}

	return CHalfLifeMultiplay::CanHavePlayerItem( pPlayer, pItem );
}

// ============================================================
// DeadPlayerWeapons  (no world drop – removing weapons silently)
// ============================================================
int CHalfLifeLoot::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_GUN_NO;
}

// ============================================================
// WeaponShouldRespawn
// ============================================================
int CHalfLifeLoot::WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	return CHalfLifeMultiplay::WeaponShouldRespawn( pWeapon );
}

// ============================================================
// CanRandomizeWeapon
// ============================================================
BOOL CHalfLifeLoot::CanRandomizeWeapon( const char *name )
{
	if ( !strcmp(name, "weapon_nuke") )
		return FALSE;
	return TRUE;
}

// ============================================================
// IsAllowedToSpawn  (block rune_ammo to avoid weapon flooding)
// ============================================================
BOOL CHalfLifeLoot::IsAllowedToSpawn( CBaseEntity *pEntity )
{
	if ( !strcmp(STRING(pEntity->pev->classname), "rune_ammo") )
		return FALSE;
	return CHalfLifeMultiplay::IsAllowedToSpawn( pEntity );
}

// ============================================================
// IsAllowedToDropWeapon
// ============================================================
BOOL CHalfLifeLoot::IsAllowedToDropWeapon( CBasePlayer *pPlayer )
{
	return g_GameInProgress ? TRUE : FALSE;
}

// ============================================================
// MutatorAllowed
// ============================================================
BOOL CHalfLifeLoot::MutatorAllowed( const char *mutator )
{
	// Block mutators that undermine weapon-count restrictions or break the mode
	if ( strstr(mutator, g_szMutators[MUTATOR_999         - 1]) || atoi(mutator) == MUTATOR_999         ) return FALSE;
	if ( strstr(mutator, g_szMutators[MUTATOR_CHUMXPLODE  - 1]) || atoi(mutator) == MUTATOR_CHUMXPLODE  ) return FALSE;
	if ( strstr(mutator, g_szMutators[MUTATOR_NOCLIP      - 1]) || atoi(mutator) == MUTATOR_NOCLIP      ) return FALSE;
	if ( strstr(mutator, g_szMutators[MUTATOR_RANDOMWEAPON- 1]) || atoi(mutator) == MUTATOR_RANDOMWEAPON) return FALSE;
	if ( strstr(mutator, g_szMutators[MUTATOR_DONTSHOOT   - 1]) || atoi(mutator) == MUTATOR_DONTSHOOT   ) return FALSE;
	if ( strstr(mutator, g_szMutators[MUTATOR_MAXPACK     - 1]) || atoi(mutator) == MUTATOR_MAXPACK     ) return FALSE;

	return CHalfLifeMultiplay::MutatorAllowed( mutator );
}

// ============================================================
// AllowRuneSpawn
// ============================================================
BOOL CHalfLifeLoot::AllowRuneSpawn( const char *szRune )
{
	if ( !strcmp("rune_frag",  szRune) ) return FALSE;
	if ( !strcmp("rune_ammo",  szRune) ) return FALSE;
	return CHalfLifeMultiplay::AllowRuneSpawn( szRune );
}

// ============================================================
// IPointsForKill  (frags are secondary – standard kill credit)
// ============================================================
int CHalfLifeLoot::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return CHalfLifeMultiplay::IPointsForKill( pAttacker, pKilled );
}

// ============================================================
// Team / relationship helpers
// ============================================================
int CHalfLifeLoot::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName )
	{
		for ( int t = 0; t < 4; t++ )
			if ( !strcmp(pTeamName, s_TeamNames[t]) ) return t;
	}
	return -1;
}

const char *CHalfLifeLoot::GetTeamID( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->pev ) return "";
	if ( !pEntity->IsPlayer() )      return "";
	return pEntity->TeamID();
}

int CHalfLifeLoot::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() )
		return GR_NOTTEAMMATE;

	CBasePlayer *pl = (CBasePlayer *)pPlayer;
	CBasePlayer *pt = (CBasePlayer *)pTarget;

	if ( pl->m_iLootTeam >= 0 && pl->m_iLootTeam == pt->m_iLootTeam )
		return GR_TEAMMATE;

	return GR_NOTTEAMMATE;
}
