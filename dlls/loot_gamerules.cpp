
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

const char *CHalfLifeLoot::s_TeamNames[4] = { "iceman", "santa", "gina", "frost" };

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
	void EXPORT LostThink( void );  // watches for crates that fell out of bounds

	int  ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	BOOL   m_bHasLoot;
	int    m_iGibModel;
	EHANDLE m_pLastAttacker;    // player who dealt the killing blow
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
	pev->framerate    = 1.0f;
	pev->fuser4      = RADAR_BASE_BLUE;
	m_bHasLoot       = FALSE;
	m_pLastAttacker  = (CBaseEntity *)NULL;

	UTIL_SetSize( pev, Vector(-32, -32, 0), Vector(32, 32, 64) );
	UTIL_SetOrigin( pev, pev->origin );

	// Check 2.5 s after spawn that the crate hasn't fallen out of the map
	SetThink( &CLootCrate::LostThink );
	pev->nextthink = gpGlobals->time + 2.5f;
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
		if ( pevAttacker && pevAttacker->flags & FL_CLIENT )
			m_pLastAttacker = CBaseEntity::Instance( ENT(pevAttacker) );
		Break();
		return 0;
	}
	return 1;
}

void CLootCrate::Break( void )
{
	pev->takedamage = DAMAGE_NO;
	pev->solid      = SOLID_NOT;
	SetThink( NULL );  // cancel LostThink

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

	if ( g_pGameRules && g_pGameRules->IsLoot() )
	{
		CHalfLifeLoot *pRules = (CHalfLifeLoot *)g_pGameRules;
		if ( m_bHasLoot )
			pRules->SpawnLootAtPosition( pev->origin );
		pRules->OnCrateBroken();
	}

	if ( !m_bHasLoot && (CBaseEntity *)m_pLastAttacker != NULL )
	{
		// Tell the breaker they found an empty crate
		MESSAGE_BEGIN( MSG_ONE, gmsgPlayClientSound, NULL, ((CBaseEntity *)m_pLastAttacker)->edict() );
			WRITE_BYTE( CLIENT_SOUND_NOPE );
		MESSAGE_END();
	}

	UTIL_Remove( this );
}

void CLootCrate::LostThink( void )
{
	// If still in the air, keep watching for a bit longer
	if ( pev->movetype == MOVETYPE_TOSS && !FBitSet(pev->flags, FL_ONGROUND) )
	{
		pev->nextthink = gpGlobals->time + 1.0f;
		return;
	}

	// Compare against world lower bound
	float worldFloor = VARS(ENT(0))->absmin.z;
	if ( worldFloor > -1024 ) worldFloor = -1024;  // sanity: never trust a tiny map extent

	if ( pev->origin.z < worldFloor + 128 )
	{
		// Crate has fallen out of the level
		ALERT( at_console, "[Loot] Crate fell out of level at (%.0f, %.0f, %.0f)\n",
		       pev->origin.x, pev->origin.y, pev->origin.z );

		if ( g_pGameRules && g_pGameRules->IsLoot() )
			((CHalfLifeLoot *)g_pGameRules)->OnCrateLost( this );
		else
			UTIL_Remove( this );
	}
	// else: crate landed safely, no further checking needed
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
	pev->fuser4     = RADAR_LOOT;

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
	pev->sequence = 0;

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
	pev->fuser4 = RADAR_BASE_RED;

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
	m_flCrateSpawnTime     = 0;
	m_flCelebrationEndTime = 0;
	m_iPendingWinnerTeam   = -1;
	m_flRoundStartTime     = 0;
	m_bLootExposed         = FALSE;
	m_iTotalCrates         = 0;
	m_iCratesLeft          = 0;

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
// ExposeLoot
//   Triggered when 5 minutes pass with the loot still hidden
//   in a crate, or when 60 seconds remain on the round timer.
//   Shatters all remaining crates and drops the loot at a
//   random info_player_deathmatch spawn point.
// ============================================================
void CHalfLifeLoot::ExposeLoot( void )
{
	m_bLootExposed = TRUE;

	// Pick a random info_player_deathmatch as the loot landing spot
	Vector exposedOrigin = g_vecZero;
	{
		int skip = RANDOM_LONG(0, 7);
		CBaseEntity *pSpot = NULL;
		for ( int i = 0; i <= skip; i++ )
		{
			CBaseEntity *pNext = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
			if ( !pNext ) break;
			pSpot = pNext;
		}
		if ( pSpot )
			exposedOrigin = pSpot->pev->origin;
	}

	// Shatter every remaining crate; suppress their individual loot-spawn callbacks
	{
		CBaseEntity *pEnt = NULL;
		while ( (pEnt = UTIL_FindEntityByClassname(pEnt, "loot_crate")) != NULL )
		{
			CLootCrate *pCrate = (CLootCrate *)pEnt;
			pCrate->m_bHasLoot = FALSE;  // prevent Break() from calling SpawnLootAtPosition
			pCrate->Break();
		}
	}

	// Place / relocate the loot entity
	CBaseEntity *pLoot = (CBaseEntity *)m_hLootEntity;
	if ( pLoot && (CBaseEntity *)m_hLootHolder == NULL )
	{
		// Already in the world but unclaimed — teleport to chosen spawn
		((CLootEntity *)pLoot)->Drop( exposedOrigin + Vector(0, 0, 36) );
	}
	else if ( !pLoot )
	{
		// Still inside a crate — spawn it at the chosen spot now
		SpawnLootAtPosition( exposedOrigin );
		pLoot = (CBaseEntity *)m_hLootEntity;
	}

	// Center-screen flash
	UTIL_ClientPrintAll( HUD_PRINTCENTER, "Loot Exposed!\nFind it!\n" );

	// Objective panel for every real player
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !plr || !plr->IsPlayer() || FBitSet(plr->pev->flags, FL_FAKECLIENT) ) continue;
		MESSAGE_BEGIN( MSG_ONE, gmsgObjective, NULL, plr->edict() );
			WRITE_STRING( "Loot Exposed!" );
			WRITE_STRING( "Crates shattered - grab it!" );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	m_flLastObjUpdate = gpGlobals->time + 4.0f;

	ALERT( at_console, "[Loot] Loot exposed at (%.0f, %.0f, %.0f)\n",
	       exposedOrigin.x, exposedOrigin.y, exposedOrigin.z );
}

// ============================================================
// OnCrateLost  (called by CLootCrate::LostThink when a crate
//              escapes the map without being broken)
// ============================================================
void CHalfLifeLoot::OnCrateLost( CLootCrate *pCrate )
{
	BOOL hadLoot = pCrate->m_bHasLoot;
	pCrate->m_bHasLoot = FALSE;  // prevent any further callbacks
	UTIL_Remove( pCrate );

	if ( m_iCratesLeft  > 0 ) m_iCratesLeft--;
	if ( m_iTotalCrates > 0 ) m_iTotalCrates--;

	ALERT( at_console, "[Loot] Crate lost (hadLoot=%d). Crates remaining: %d/%d\n",
	       hadLoot, m_iCratesLeft, m_iTotalCrates );

	if ( hadLoot && !m_bLootExposed )
	{
		// The loot crate escaped — immediately expose loot at a random spawn
		ExposeLoot();
	}
	else if ( !m_bLootExposed )
	{
		// Non-loot crate — just refresh the crate count on the objective panel
		m_flLastObjUpdate = 0;
		SendObjectiveUpdate();
	}
}

// ============================================================
// OnCrateBroken  (called by CLootCrate::Break for every crate)
// ============================================================
void CHalfLifeLoot::OnCrateBroken( void )
{
	if ( m_iCratesLeft > 0 ) m_iCratesLeft--;

	// ExposeLoot handles its own objective messaging; skip during mass-shatter
	if ( m_bLootExposed ) return;

	// Immediately push updated crate count to all real players
	m_flLastObjUpdate = 0;
	SendObjectiveUpdate();
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

	// Mark loot as exposed so OnCrateBroken suppresses objective-update spam
	m_bLootExposed = TRUE;

	// Shatter all remaining crates — loot has been found
	CBaseEntity *pEnt = NULL;
	while ( (pEnt = UTIL_FindEntityByClassname( pEnt, "loot_crate" )) != NULL )
	{
		CLootCrate *pCrate = (CLootCrate *)pEnt;
		// Skip the originating crate — it is already mid-Break() and will be removed
		if ( pCrate->pev->solid == SOLID_NOT ) continue;
		pCrate->m_bHasLoot = FALSE;  // prevent recursion
		pCrate->Break();
	}

	MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
		WRITE_BYTE( CLIENT_SOUND_SIREN );
	MESSAGE_END();
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

	// Fallback weapon-entity classnames tried when floor trace fails.
	// These are shuffled per-crate so no single entity becomes the magnet.
	static const char *wFallback[] = {
		"weapon_shotgun", "weapon_mp5", "weapon_crowbar",
		"ammo_buckshot",  "weapon_357", "weapon_9mmclip"
	};
	const int wFallbackCount = 6;

	// Track placed positions to enforce minimum crate separation
	const float MIN_CRATE_SEP    = 96.0f;
	// Minimum clear radius around a candidate before we consider spawning there.
	// A crate is ~32 units wide and a player hull is 32 units — 80 gives a safe buffer.
	const float CRATE_PLAYER_RADIUS = 80.0f;
	Vector crateSpots[32];
	int    crateSpotCount = 0;

	// Returns TRUE if any living player is within CRATE_PLAYER_RADIUS of pos.
	auto PlayerNearby = [&]( const Vector &pos ) -> BOOL
	{
		for ( int n = 1; n <= gpGlobals->maxClients; n++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(n);
			if ( !plr || !plr->IsAlive() ) continue;
			if ( (plr->pev->origin - pos).Length() < CRATE_PLAYER_RADIUS )
				return TRUE;
		}
		return FALSE;
	};

	for ( int i = 0; i < count; i++ )
	{
		Vector spawnPos;
		BOOL found = FALSE;

		// Up to 4 passes of random floor positions; re-roll if too close to an existing crate.
		// Strictness relaxes across passes so we always place something.
		for ( int pass = 0; pass < 4 && !found; pass++ )
		{
			int attempts = ( pass < 2 ) ? 30 : 15;
			Vector candidate;
			if ( !FindSafeFloorPosition( candidate, 0, g_vecZero, attempts ) )
				break;

			BOOL tooClose = FALSE;
			for ( int k = 0; k < crateSpotCount && !tooClose; k++ )
				if ( (candidate - crateSpots[k]).Length() < MIN_CRATE_SEP )
					tooClose = TRUE;

			if ( !tooClose && !PlayerNearby(candidate) ) { spawnPos = candidate; found = TRUE; }
		}

		if ( !found )
		{
			// Fallback: piggy-back on a random weapon entity.
			// Shuffle a local index array so we don't always start from weapon_shotgun.
			int order[6] = { 0, 1, 2, 3, 4, 5 };
			for ( int s = wFallbackCount - 1; s > 0; s-- )
			{
				int j    = RANDOM_LONG( 0, s );
				int tmp  = order[s]; order[s] = order[j]; order[j] = tmp;
			}

			for ( int f = 0; f < wFallbackCount && !found; f++ )
			{
				// Iterate all entities of this class and pick a random one
				CBaseEntity *pW    = NULL;
				CBaseEntity *candidates[16];
				int          nCand = 0;
				while ( nCand < 16 &&
				        (pW = UTIL_FindEntityByClassname( pW, wFallback[order[f]] )) != NULL )
					candidates[nCand++] = pW;

				if ( nCand == 0 ) continue;

				// Try each candidate (shuffled order); skip if a crate is already close
				int pick = RANDOM_LONG( 0, nCand - 1 );
				for ( int c = 0; c < nCand && !found; c++ )
				{
					Vector candidate = candidates[(pick + c) % nCand]->pev->origin + Vector(0, 0, 32);

					BOOL tooClose = FALSE;
					for ( int k = 0; k < crateSpotCount && !tooClose; k++ )
						if ( (candidate - crateSpots[k]).Length() < MIN_CRATE_SEP )
							tooClose = TRUE;

					if ( !tooClose && !PlayerNearby(candidate) ) { spawnPos = candidate; found = TRUE; }
				}
			}

			if ( !found ) continue;
		}

		CBaseEntity *pEnt = CBaseEntity::Create( "loot_crate", spawnPos, g_vecZero, NULL );
		if ( pEnt )
		{
			if ( crateSpotCount < 32 )
				crateSpots[crateSpotCount++] = spawnPos;

			m_iTotalCrates++;
			m_iCratesLeft++;
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
	m_bLootExposed         = FALSE;
	m_flRoundStartTime     = 0;
	m_iTotalCrates         = 0;
	m_iCratesLeft          = 0;
	memset( m_vecUsedSpots, 0, sizeof(m_vecUsedSpots) );

	// Fisher-Yates shuffle of available player slots
	for ( int i = clients - 1; i > 0; i-- )
	{
		int j        = RANDOM_LONG(0, i);
		int tmp      = m_iPlayersInArena[i];
		m_iPlayersInArena[i] = m_iPlayersInArena[j];
		m_iPlayersInArena[j] = tmp;
	}

	// Shuffle the team name order so the same player doesn't always land on "iceman"
	int teamNameOrder[4] = { 0, 1, 2, 3 };
	for ( int i = 3; i > 0; i-- )
	{
		int j              = RANDOM_LONG(0, i);
		int tmp            = teamNameOrder[i];
		teamNameOrder[i]   = teamNameOrder[j];
		teamNameOrder[j]   = tmp;
	}

	// Team assignment:
	//   ≤ 8 players: form pairs — consecutive shuffled players share a team
	//                (4 players → 2 teams of 2; 6 → 3 teams of 2; 8 → 4 teams of 2;
	//                 odd counts get one solo team e.g. 5 → 2+2+1)
	//   > 8 players: 4 teams, round-robin balanced
	// teamNameOrder remaps slot → name-index so the same player doesn't always land
	// on "iceman" just because they happen to be assigned slot 0.
	for ( int i = 0; i < clients; i++ )
	{
		int t      = (clients <= 8) ? (i / 2) : (i % 4);
		if ( t > 3 ) t = 3;                    // safety clamp to 4 teams max
		int name_t = teamNameOrder[t];          // use name-index for all arrays
		if ( m_iTeamPlayerCount[name_t] < 32 )
		{
			m_iTeamPlayers[name_t][m_iTeamPlayerCount[name_t]] = m_iPlayersInArena[i];
			m_iTeamPlayerCount[name_t]++;
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

		// +36 Z lifts the player's hull centre above the floor (same offset used
		// everywhere else in this file when placing entities at spawn points).
		m_vecTeamSpawnOrigin[t] = pUsedSpots[t] ? pUsedSpots[t]->pev->origin + Vector(0, 0, 36) : g_vecZero;

		// Assign team name and loot-team index to each player on this team.
		// t is already in name-index space (m_iTeamPlayers[t] was stored that way above).
		for ( int j = 0; j < m_iTeamPlayerCount[t]; j++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( m_iTeamPlayers[t][j] );
			if ( !plr ) continue;

			plr->m_iLootTeam = t;
			plr->pev->fuser4 = t;
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

	// Delay crate spawning by 3 seconds so players have fully settled at
	// their spawn points before crates are placed (avoids spawning on top of them).
	m_flCrateSpawnTime = gpGlobals->time + 3.0f;

	// Broadcast team names (4 slots)
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamNames );
		WRITE_BYTE( 4 );
		WRITE_STRING( "iceman" );
		WRITE_STRING( "santa" );
		WRITE_STRING( "gina" );
		WRITE_STRING( "frost" );
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
	m_flRoundStartTime    = gpGlobals->time;
	m_bLootExposed        = FALSE;

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
		pLootEnt->pev->sequence = 1;
	}

	m_hLootHolder      = pPlayer;
	m_flLootPickupTime = gpGlobals->time;
	pPlayer->m_bHoldingLoot = TRUE;

	// Green glow on holder
	pPlayer->pev->renderfx    = kRenderFxGlowShell;
	pPlayer->pev->renderamt   = 10;
	pPlayer->pev->rendercolor = Vector(255, 117, 24);
	pPlayer->pev->fuser4      = RADAR_LOOT;

	// Attach a green beam trail to the holder
	int iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE ( TE_BEAMFOLLOW );
		WRITE_SHORT( pPlayer->entindex() );
		WRITE_SHORT( iTrail );
		WRITE_BYTE ( 50 );   // life
		WRITE_BYTE ( 3  );   // width
		WRITE_BYTE ( 255 );   // r
		WRITE_BYTE ( 117 );  // g
		WRITE_BYTE ( 24 );   // b
		WRITE_BYTE ( 30 );  // brightness
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

	m_flLastObjUpdate = gpGlobals->time + 4.0f;

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
	pPlayer->pev->renderfx    = kRenderFxNone;
	pPlayer->pev->renderamt   = 0;
	pPlayer->pev->rendercolor = Vector(0, 0, 0);

	// Kill the green beam trail on the scorer
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE ( TE_KILLBEAM );
		WRITE_SHORT( pPlayer->entindex() );
	MESSAGE_END();

	// Remove the loot entity from the world
	CBaseEntity *pLoot = (CBaseEntity *)m_hLootEntity;
	if ( pLoot )
	{
		UTIL_Remove( pLoot );
		m_hLootEntity = (CBaseEntity *)NULL;
	}

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
				detail = "You have the loot!";
			else
				detail = UTIL_VarArgs("Cover %s!", STRING(pHolder->pev->netname));
			title = "Get to the goal!";
		}
		else
		{
			// Use a local buffer for title so the second UTIL_VarArgs call
			// (for detail) does not overwrite the shared static VarArgs buffer
			// that title already points into.
			static char titleBuf[64];
			snprintf( titleBuf, sizeof(titleBuf), "Frag %s!", STRING(pHolder->pev->netname) );
			title  = titleBuf;
			detail = UTIL_VarArgs("%s has the loot!", STRING(pHolder->pev->netname));
		}

		MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict() );
			WRITE_STRING( title );
			WRITE_STRING( detail );
			if ( m_iTotalCrates > 0 && !m_bLootExposed && !(CBaseEntity *)m_hLootHolder )
			{
				WRITE_BYTE( ((m_iTotalCrates - m_iCratesLeft) / (float)m_iTotalCrates) * 100 );
				WRITE_STRING( UTIL_VarArgs("%d of %d crates left", m_iCratesLeft, m_iTotalCrates) );
			}
			else
			{
				WRITE_BYTE( 0 );
				WRITE_STRING( "" );
			}
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

	// Fire deferred crate spawn
	if ( m_flCrateSpawnTime > 0 && gpGlobals->time >= m_flCrateSpawnTime )
	{
		m_flCrateSpawnTime = 0;
		SpawnLootEntities();
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

		// Expose loot after 5 minutes with no one finding it, or with 60s left on the clock
		if ( !m_bLootExposed && (CBaseEntity *)m_hLootHolder == NULL )
		{
			BOOL timeout5min = ( (CBaseEntity *)m_hLootEntity == NULL &&
			                     m_flRoundStartTime > 0 &&
			                     gpGlobals->time - m_flRoundStartTime >= 300.0f );
			BOOL last60sec   = ( m_flRoundTimeLimit > 0 &&
			                     (m_flRoundTimeLimit - gpGlobals->time) <= 60.0f );
			if ( timeout5min || last60sec )
				ExposeLoot();
		}

		SendObjectiveUpdate();

		// Determine how many teams still have living players
		int teamsAlive  = 0;
		int lastTeamIdx = -1;
		for ( int t = 0; t < 4; t++ )
		{
			if ( teamAlive[t] > 0 ) { teamsAlive++; lastTeamIdx = t; }
		}

		if ( ( totalAlive == 0 || teamsAlive <= 1 ) && m_flCelebrationEndTime == 0 )
		{
			// One team survived — celebrate before tearing down the round
			if ( lastTeamIdx >= 0 )
			{
				m_flLastObjUpdate = gpGlobals->time + 5.0f;

				// Surviving team members celebrate
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
					if ( plr && plr->IsPlayer() && plr->IsInArena &&
					     plr->IsAlive() && plr->m_iLootTeam == lastTeamIdx )
						plr->Celebrate();
				}

				// Objective panel for every real player
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
					if ( !plr || !plr->IsPlayer() || FBitSet(plr->pev->flags, FL_FAKECLIENT) ) continue;
					MESSAGE_BEGIN( MSG_ONE, gmsgObjective, NULL, plr->edict() );
						WRITE_STRING( "Round Over" );
						WRITE_STRING( UTIL_VarArgs("Team %s won the round!", s_TeamNames[lastTeamIdx]) );
						WRITE_BYTE( 0 );
					MESSAGE_END();
				}

				UTIL_ClientPrintAll( HUD_PRINTCENTER,
				    UTIL_VarArgs("Team %s wins!\n", s_TeamNames[lastTeamIdx]) );
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE( CLIENT_SOUND_OUTSTANDING );
				MESSAGE_END();
			}

			m_iPendingWinnerTeam   = lastTeamIdx;
			m_flCelebrationEndTime = gpGlobals->time + 4.0f;
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
			if ( g_GameInProgress && m_iTotalCrates > 0 && !m_bLootExposed && !(CBaseEntity *)m_hLootHolder )
				WRITE_STRING( UTIL_VarArgs("%d of %d crates left", m_iCratesLeft, m_iTotalCrates) );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict() );
			WRITE_BYTE( 4 );
			WRITE_STRING( "iceman" );
			WRITE_STRING( "santa" );
			WRITE_STRING( "gina" );
			WRITE_STRING( "frost" );
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
// PlaceAtTeamSpawn
// Finds a clear world position for pPlayer around their team's
// assigned spawn origin and calls UTIL_SetOrigin to place them.
//
// Three-pass search:
//   1. 16 offsets (8 compass + diagonals, at two heights) around the base.
//   2. Full scan of every info_player_deathmatch in the map.
//   3. Upward nudge (1 unit/step, up to 72 units) to escape floor-clipping.
// Followed by a push-away loop that resolves any remaining player overlap.
// ============================================================
void CHalfLifeLoot::PlaceAtTeamSpawn( CBasePlayer *pPlayer, int teamIdx )
{
	// Determine this player's slot index within the team (used to select the
	// initial offset so teammates don't all start on exactly the same spot).
	int posInTeam = 0;
	for ( int j = 0; j < m_iTeamPlayerCount[teamIdx]; j++ )
	{
		if ( m_iTeamPlayers[teamIdx][j] == pPlayer->entindex() )
		{
			posInTeam = j;
			break;
		}
	}

	// A standing player's hull is 32 units wide; 48 gives a comfortable margin.
	static const float PLAYER_AVOID_RADIUS = 48.0f;

	// 8-compass + diagonal offsets at floor level, then repeated ~18 units
	// higher, giving 16 candidates before falling back to wider searches.
	static const Vector s_SpawnOffsets[] = {
		Vector(  0,   0,  0),
		Vector( 48,   0,  0),
		Vector(-48,   0,  0),
		Vector(  0,  48,  0),
		Vector(  0, -48,  0),
		Vector( 34,  34,  0),
		Vector(-34,  34,  0),
		Vector(-34, -34,  0),
		Vector( 34, -34,  0),
		Vector(  0,   0, 18),
		Vector( 48,   0, 18),
		Vector(-48,   0, 18),
		Vector(  0,  48, 18),
		Vector(  0, -48, 18),
		Vector( 34,  34, 18),
		Vector(-34, -34, 18),
	};
	static const int s_NumSpawnOffsets = ARRAYSIZE(s_SpawnOffsets);

	// Returns TRUE when pos is clear of world geometry AND no other living
	// player is within PLAYER_AVOID_RADIUS.  Traces with ignore_monsters so
	// only BSP faces are tested; players are checked explicitly.
	auto IsClear = [&]( const Vector &pos ) -> BOOL
	{
		TraceResult tr;
		UTIL_TraceHull( pos, pos, ignore_monsters, human_hull,
		                ENT(pPlayer->pev), &tr );
		if ( tr.fStartSolid ) return FALSE;

		for ( int n = 1; n <= gpGlobals->maxClients; n++ )
		{
			CBasePlayer *other = (CBasePlayer *)UTIL_PlayerByIndex(n);
			if ( !other || other == pPlayer || !other->IsAlive() ) continue;
			if ( (other->pev->origin - pos).Length() < PLAYER_AVOID_RADIUS )
				return FALSE;
		}
		return TRUE;
	};

	Vector baseOrigin = m_vecTeamSpawnOrigin[teamIdx];
	Vector spawnPos   = baseOrigin + s_SpawnOffsets[posInTeam % s_NumSpawnOffsets];
	BOOL   placed     = FALSE;

	// Pass 1: offsets around the team's assigned spawn point.
	for ( int o = 0; o < s_NumSpawnOffsets && !placed; o++ )
	{
		Vector tryPos = baseOrigin + s_SpawnOffsets[(posInTeam + o) % s_NumSpawnOffsets];
		if ( IsClear(tryPos) ) { spawnPos = tryPos; placed = TRUE; }
	}

	// Pass 2: every info_player_deathmatch in the map.
	if ( !placed )
	{
		CBaseEntity *pSpot = NULL;
		while ( !placed && (pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" )) != NULL )
		{
			Vector candidate = pSpot->pev->origin + Vector(0, 0, 36);
			if ( IsClear(candidate) ) { spawnPos = candidate; placed = TRUE; }
		}
	}

	// Pass 3: nudge upward to escape floor-clipping.
	if ( !placed )
	{
		for ( int step = 1; step <= 72 && !placed; step++ )
		{
			Vector nudge = spawnPos;
			nudge.z     += step;
			if ( IsClear(nudge) ) { spawnPos = nudge; placed = TRUE; }
		}
	}

	UTIL_SetOrigin( pPlayer->pev, spawnPos );

	// Post-placement de-overlap: push this player directly away from the
	// nearest overlapper, up to 8 times, stopping if a wall blocks the push.
	for ( int attempt = 0; attempt < 8; attempt++ )
	{
		CBasePlayer *nearest  = NULL;
		float        nearDist = PLAYER_AVOID_RADIUS;

		for ( int n = 1; n <= gpGlobals->maxClients; n++ )
		{
			CBasePlayer *other = (CBasePlayer *)UTIL_PlayerByIndex(n);
			if ( !other || other == pPlayer || !other->IsAlive() ) continue;
			float d = (other->pev->origin - spawnPos).Length();
			if ( d < nearDist ) { nearDist = d; nearest = other; }
		}

		if ( !nearest ) break;

		Vector away = spawnPos - nearest->pev->origin;
		if ( away.Length() < 1.0f ) away = Vector(1, 0, 0);  // degenerate: same origin
		away = away.Normalize() * PLAYER_AVOID_RADIUS;

		Vector candidate = spawnPos + away;
		TraceResult tr;
		UTIL_TraceHull( candidate, candidate, ignore_monsters, human_hull,
		                ENT(pPlayer->pev), &tr );
		if ( !tr.fStartSolid )
			{ spawnPos = candidate; UTIL_SetOrigin( pPlayer->pev, spawnPos ); }
		else
			break;
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

		PlaceAtTeamSpawn( pPlayer, teamIdx );

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

//=========================================================
//=========================================================
BOOL CHalfLifeLoot::CanHaveNamedItem( CBasePlayer *pPlayer, const char *pszItemName )
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

	int maxWeapons = hasLootAdvantage ? 3 : 1;
	if ( count >= maxWeapons )
		return FALSE;  // At limit; +use swap in player.cpp handles the swap

	return CHalfLifeMultiplay::CanHaveNamedItem( pPlayer, pszItemName );
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
