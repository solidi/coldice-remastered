//
// awards.cpp
//
// Multiplayer awards modeled after GoldenEye 007 (N64) mpCalculateAwards().
//
// Every connected human player gets a "weapon of choice" plus up to two
// awards, recomputed live and pushed to that player only when they change.
// Bots are tracked as hidden rivals: their stats join the ranking pool so a
// human playing against bots is compared to them, but a bot never holds an
// award and is never sent anything.
//
// Translation notes from the N64 logic:
//  - N64 compared four fixed players; here the pool is every eligible player.
//  - N64 broke ties with a coin flip on every call; a live board would flicker,
//    so each slot gets a random tie-breaker and award pick order once per
//    baseline, which keeps the random feel but stays stable between refreshes.
//  - "Other players on screen" is approximated by a view cone + line of sight
//    test sampled on the server.
//  - Speed uses horizontal distance over time alive (not total match time),
//    since players join mid-match.
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "awards.h"

extern int gmsgAwards;
extern DLL_GLOBAL BOOL g_fGameOver;

CMultiplayAwards g_Awards;

static_assert( MAX_WEAPONS <= ( 1 << AWARDS_WEAPON_BITS ), "weapon ids no longer fit the Awards message" );
static_assert( AWARD_COUNT <= ( 1 << AWARDS_AWARD_BITS ), "award ids no longer fit the Awards message" );

#define AWARDS_SAMPLE_INTERVAL		0.25f	// seconds between movement / view / weapon samples
#define AWARDS_REFRESH_INTERVAL		1.0f	// periodic recompute for the continuous metrics
#define AWARDS_REFRESH_DIRTY		0.25f	// recompute delay after a scoreboard-type event
#define AWARDS_MIN_PLAY_TIME		10.0f	// seconds alive before a player is ranked
#define AWARDS_MULTIKILL_WINDOW		2.0f	// N64: 0x78 frames at 60 Hz
#define AWARDS_MAX_SPEED			2000.0f	// units/s; faster sample deltas are teleports
#define AWARDS_UNITS_TO_KM			( 0.0254f / 1000.0f )

// N64 thresholds
#define AWARDS_AMMO_MAX_SHOTS		100
#define AWARDS_ARMOR_FEW			2
#define AWARDS_ARMOR_MANY			6
#define AWARDS_FRANTIC_KMH			10.0f

CMultiplayAwards::CMultiplayAwards()
{
	memset( m_Stats, 0, sizeof( m_Stats ) );
	m_flNextSample = m_flLastSample = 0;
	m_flNextRefresh = m_flLastRefresh = 0;
	m_bDirty = FALSE;
}

BOOL CMultiplayAwards::IsActive( void ) const
{
	return g_pGameRules && g_pGameRules->IsMultiplayer();
}

BOOL CMultiplayAwards::IsInPlay( CBasePlayer *pPlayer ) const
{
	return pPlayer && !pPlayer->HasDisconnected && pPlayer->IsAlive() &&
		!pPlayer->IsObserver() && !pPlayer->IsSpectator();
}

// Monster victims that count as kills / headshots: real creatures only,
// not thrown bioweapons (snarks, chumtoads, hornets) or classless props.
BOOL CMultiplayAwards::IsCountedMonster( CBaseEntity *pEntity ) const
{
	if ( !pEntity || pEntity->IsPlayer() || !pEntity->MyMonsterPointer() )
		return FALSE;

	int iClass = pEntity->Classify();
	return iClass != CLASS_NONE && iClass != CLASS_PLAYER_BIOWEAPON && iClass != CLASS_ALIEN_BIOWEAPON;
}

CMultiplayAwards::stats_t *CMultiplayAwards::PlayerStats( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

	int iIndex = pEntity->entindex();
	if ( iIndex < 1 || iIndex > AWARDS_MAX_PLAYERS || !m_Stats[iIndex].tracked )
		return NULL;

	return &m_Stats[iIndex];
}

int CMultiplayAwards::GetPackedAwards( int iPlayerIndex ) const
{
	if ( iPlayerIndex < 1 || iPlayerIndex > AWARDS_MAX_PLAYERS )
		return 0;
	return m_Stats[iPlayerIndex].packed;
}

//=========================================================
// Lifecycle
//=========================================================

void CMultiplayAwards::ResetSlot( int iIndex, BOOL bTracked, BOOL bBot )
{
	if ( iIndex < 1 || iIndex > AWARDS_MAX_PLAYERS )
		return;

	stats_t &s = m_Stats[iIndex];
	memset( &s, 0, sizeof( s ) );

	s.tracked = bTracked;
	s.bot = bBot;
	s.shortestLife = -1;
	s.lastSentPacked = -1;
	for ( int i = 0; i < 4; i++ )
		s.lastKillTimes[i] = -1;

	if ( !bTracked )
		return;

	s.tieBreak = RANDOM_LONG( 0, 0x7FFF );

	for ( int i = 0; i < AWARD_COUNT - 1; i++ )
		s.order[i] = i + 1;
	for ( int i = AWARD_COUNT - 2; i > 0; i-- )
	{
		int j = RANDOM_LONG( 0, i );
		int t = s.order[i];
		s.order[i] = s.order[j];
		s.order[j] = t;
	}
}

void CMultiplayAwards::Reset( void )
{
	for ( int i = 1; i <= AWARDS_MAX_PLAYERS; i++ )
		ResetSlot( i, FALSE );

	m_flNextSample = m_flLastSample = 0;
	m_flNextRefresh = m_flLastRefresh = 0;
	m_bDirty = FALSE;
}

void CMultiplayAwards::ClientPutInServer( edict_t *pEntity )
{
	if ( !pEntity )
		return;

	// Bots are tracked as rivals; HLTV proxies are never tracked.
	ResetSlot( ENTINDEX( pEntity ), !FBitSet( pEntity->v.flags, FL_PROXY ),
		FBitSet( pEntity->v.flags, FL_FAKECLIENT ) ? TRUE : FALSE );
	m_bDirty = TRUE;
}

void CMultiplayAwards::ClientDisconnected( edict_t *pEntity )
{
	if ( !pEntity )
		return;

	ResetSlot( ENTINDEX( pEntity ), FALSE );
	m_bDirty = TRUE;
}

void CMultiplayAwards::ClientHUDInitialized( CBasePlayer *pPlayer )
{
	stats_t *s = PlayerStats( pPlayer );
	if ( !s || s->bot )
		return;

	// The client clears its awards on InitHUD; resend whatever we have.
	s->lastSentPacked = 0;
	m_bDirty = TRUE;
}

void CMultiplayAwards::Think( void )
{
	if ( !IsActive() )
		return;

	float flNow = gpGlobals->time;

	if ( !g_fGameOver && flNow >= m_flNextSample )
	{
		float flInterval = m_flLastSample > 0 ? flNow - m_flLastSample : 0;
		flInterval = fmax( 0.0f, fmin( flInterval, 1.0f ) );

		Sample( flInterval );

		m_flLastSample = flNow;
		m_flNextSample = flNow + AWARDS_SAMPLE_INTERVAL;
	}

	if ( flNow >= m_flNextRefresh || ( m_bDirty && flNow >= m_flLastRefresh + AWARDS_REFRESH_DIRTY ) )
		Refresh();
}

//=========================================================
// Gameplay events
//=========================================================

void CMultiplayAwards::ShotFired( CBasePlayer *pShooter )
{
	if ( !IsActive() || g_fGameOver )
		return;

	stats_t *s = PlayerStats( pShooter );
	if ( !s || s->lastShotTime == gpGlobals->time )
		return;

	s->lastShotTime = gpGlobals->time;
	s->shots++;
}

void CMultiplayAwards::Headshot( CBaseEntity *pVictim, entvars_t *pevAttacker )
{
	if ( !IsActive() || g_fGameOver || !pVictim || !pevAttacker )
		return;

	if ( !pVictim->IsPlayer() && !IsCountedMonster( pVictim ) )
		return;

	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );
	if ( pAttacker == pVictim )
		return;

	// Pellets of one shot arrive in the same frame; count the shot once.
	stats_t *s = PlayerStats( pAttacker );
	if ( !s || s->lastHeadshotTime == gpGlobals->time )
		return;

	s->lastHeadshotTime = gpGlobals->time;
	s->headshots++;
	m_bDirty = TRUE;
}

void CMultiplayAwards::PlayerDamaged( CBasePlayer *pVictim, entvars_t *pevInflictor, entvars_t *pevAttacker )
{
	if ( !IsActive() || g_fGameOver || !pVictim || !pevAttacker )
		return;

	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );
	if ( !pAttacker || !pAttacker->IsPlayer() || pAttacker == pVictim )
		return;

	stats_t *s = PlayerStats( pAttacker );
	if ( !s )
		return;

	s->hits++;

	// N64: the hit came from the victim's back half (damage direction within
	// 90 degrees of the victim's facing). Use the inflictor for projectiles.
	Vector vecSource = pevInflictor ? pevInflictor->origin : pAttacker->pev->origin;
	Vector vecToSource = vecSource - pVictim->pev->origin;
	vecToSource.z = 0;

	if ( vecToSource.Length() < 1.0f )
		return;

	Vector vecForward;
	UTIL_MakeVectorsPrivate( Vector( 0, pVictim->pev->v_angle.y, 0 ), vecForward, NULL, NULL );

	if ( DotProduct( vecToSource.Normalize(), vecForward ) < 0 )
		s->backHits++;
}

void CMultiplayAwards::PlayerKilled( CBasePlayer *pVictim, entvars_t *pevAttacker )
{
	if ( !IsActive() || g_fGameOver || !pVictim )
		return;

	CBaseEntity *pAttacker = pevAttacker ? CBaseEntity::Instance( pevAttacker ) : NULL;

	stats_t *s = PlayerStats( pVictim );
	if ( s )
	{
		s->deaths++;

		// Self kills and the world (falls, triggers, crushers) are suicides.
		if ( !pAttacker || pAttacker == pVictim || ( !pAttacker->IsPlayer() && !pAttacker->MyMonsterPointer() ) )
			s->suicides++;

		if ( s->alive )
			EndLife( *s, TRUE );
	}

	if ( pAttacker && pAttacker != pVictim )
		CreditKill( pAttacker );

	m_bDirty = TRUE;
}

void CMultiplayAwards::MonsterKilled( CBaseMonster *pVictim, entvars_t *pevAttacker )
{
	if ( !IsActive() || g_fGameOver || !pevAttacker || !IsCountedMonster( pVictim ) )
		return;

	CreditKill( CBaseEntity::Instance( pevAttacker ) );
	m_bDirty = TRUE;
}

void CMultiplayAwards::ArmorPickedUp( CBasePlayer *pPlayer )
{
	if ( !IsActive() || g_fGameOver )
		return;

	stats_t *s = PlayerStats( pPlayer );
	if ( !s )
		return;

	s->armorPickups++;
	m_bDirty = TRUE;
}

void CMultiplayAwards::CreditKill( CBaseEntity *pAttacker )
{
	stats_t *s = PlayerStats( pAttacker );
	if ( !s )
		return;

	s->kills++;

	for ( int i = 3; i > 0; i-- )
		s->lastKillTimes[i] = s->lastKillTimes[i - 1];
	s->lastKillTimes[0] = gpGlobals->time;

	int iRecent = 1;
	while ( iRecent < 4 && s->lastKillTimes[iRecent] >= 0 &&
		( s->lastKillTimes[0] - s->lastKillTimes[iRecent] ) < AWARDS_MULTIKILL_WINDOW )
		iRecent++;

	if ( iRecent > s->bestMultiKill )
		s->bestMultiKill = iRecent;
}

void CMultiplayAwards::EndLife( stats_t &s, BOOL bDied )
{
	float flLife = gpGlobals->time - s.lifeStart;

	if ( flLife > s.longestLife )
		s.longestLife = flLife;

	if ( bDied && ( s.shortestLife < 0 || flLife < s.shortestLife ) )
		s.shortestLife = flLife;

	s.alive = FALSE;
}

//=========================================================
// Sampling
//=========================================================

int CMultiplayAwards::CountOthersInView( CBasePlayer *pPlayer )
{
	Vector vecForward;
	UTIL_MakeVectorsPrivate( pPlayer->pev->v_angle, vecForward, NULL, NULL );

	float flFov = pPlayer->pev->fov > 0 ? pPlayer->pev->fov : 90.0f;
	// Real pi on purpose: util.h M_PI follows the pi-changing mutator.
	float flMinDot = cos( flFov * 0.5f * ( 3.14159265f / 180.0f ) );
	Vector vecEye = pPlayer->EyePosition();

	int iCount = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pOther = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !pOther || pOther == pPlayer || !IsInPlay( pOther ) || FBitSet( pOther->pev->effects, EF_NODRAW ) )
			continue;

		Vector vecTarget = pOther->EyePosition();
		Vector vecDir = vecTarget - vecEye;
		float flDist = vecDir.Length();
		if ( flDist < 1.0f )
			continue;

		if ( DotProduct( vecDir / flDist, vecForward ) < flMinDot )
			continue;

		TraceResult tr;
		UTIL_TraceLine( vecEye, vecTarget, ignore_monsters, ignore_glass, pPlayer->edict(), &tr );
		if ( tr.flFraction >= 1.0f )
			iCount++;
	}

	return iCount;
}

void CMultiplayAwards::Sample( float flInterval )
{
	float flNow = gpGlobals->time;

	for ( int i = 1; i <= gpGlobals->maxClients && i <= AWARDS_MAX_PLAYERS; i++ )
	{
		stats_t &s = m_Stats[i];
		if ( !s.tracked )
			continue;

		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !pPlayer || pPlayer->HasDisconnected )
			continue;

		BOOL bInPlay = IsInPlay( pPlayer );
		if ( bInPlay && !s.alive )
		{
			s.alive = TRUE;
			s.lifeStart = flNow;
			s.hasLastPos = FALSE;
		}
		else if ( !bInPlay && s.alive )
		{
			// Left play without dying (spectate, round reset): not a death.
			EndLife( s, FALSE );
		}

		if ( !s.alive )
			continue;

		s.aliveTime += flInterval;

		if ( flNow - s.lifeStart > s.longestLife )
			s.longestLife = flNow - s.lifeStart;

		if ( pPlayer->m_pActiveItem )
		{
			int iId = pPlayer->m_pActiveItem->m_iId;
			if ( iId > WEAPON_NONE && iId < MAX_WEAPONS )
				s.weaponTime[iId] += flInterval;
		}

		Vector vecDelta = pPlayer->pev->origin - s.lastPos;
		vecDelta.z = 0;
		float flStep = vecDelta.Length();
		if ( s.hasLastPos && flStep <= AWARDS_MAX_SPEED * flInterval )
			s.distance += flStep;
		s.lastPos = pPlayer->pev->origin;
		s.hasLastPos = TRUE;

		s.timeOthersInView += flInterval * CountOthersInView( pPlayer );
	}
}

//=========================================================
// Award computation
//=========================================================

// Index of the pool member with the highest (or lowest) value, 0 if none.
int CMultiplayAwards::FindBest( const int *pPool, int iCount, const float *pValues, BOOL bHighest, const BOOL *pAllowed ) const
{
	int iBest = 0;

	for ( int k = 0; k < iCount; k++ )
	{
		int i = pPool[k];
		if ( pAllowed && !pAllowed[i] )
			continue;

		if ( !iBest )
		{
			iBest = i;
			continue;
		}

		float a = pValues[i];
		float b = pValues[iBest];
		if ( ( bHighest && a > b ) || ( !bHighest && a < b ) ||
			( a == b && m_Stats[i].tieBreak > m_Stats[iBest].tieBreak ) )
			iBest = i;
	}

	return iBest;
}

int CMultiplayAwards::WeaponOfChoice( const stats_t &s ) const
{
	int iBest = WEAPON_NONE;
	float flMost = 0;

	for ( int i = WEAPON_NONE + 1; i < MAX_WEAPONS; i++ )
	{
		if ( s.weaponTime[i] > flMost )
		{
			flMost = s.weaponTime[i];
			iBest = i;
		}
	}

	return iBest;
}

void CMultiplayAwards::Refresh( void )
{
	float flNow = gpGlobals->time;
	m_bDirty = FALSE;
	m_flLastRefresh = flNow;
	m_flNextRefresh = flNow + AWARDS_REFRESH_INTERVAL;

	int iMaxClients = gpGlobals->maxClients < AWARDS_MAX_PLAYERS ? gpGlobals->maxClients : AWARDS_MAX_PLAYERS;

	// Ranking pool: connected players (humans and rival bots) that have actually
	// played for a while.
	int pool[AWARDS_MAX_PLAYERS];
	int iPool = 0;
	BOOL bAnyHuman = FALSE;
	for ( int i = 1; i <= iMaxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !m_Stats[i].tracked || !pPlayer || pPlayer->HasDisconnected )
			continue;

		if ( !m_Stats[i].bot )
			bAnyHuman = TRUE;

		if ( m_Stats[i].aliveTime >= AWARDS_MIN_PLAY_TIME )
			pool[iPool++] = i;
	}

	// Awards are only ever shown to humans.
	if ( !bAnyHuman )
		return;

	// "Most / least" awards need someone to compare against (the N64 always had
	// at least two players). An award won by a bot is simply not shown to anyone.
	int iRankPool = ( iPool >= 2 ) ? iPool : 0;

	int earned[AWARDS_MAX_PLAYERS + 1];
	float value[AWARDS_MAX_PLAYERS + 1];
	BOOL allowed[AWARDS_MAX_PLAYERS + 1];
	memset( earned, 0, sizeof( earned ) );

#define GIVE( slot, award )	( earned[slot] |= ( 1 << (award) ) )
#define FILL( expr )		for ( int k = 0; k < iPool; k++ ) { const stats_t &s = m_Stats[pool[k]]; value[pool[k]] = (float)( expr ); }
#define ALLOW( expr )		for ( int k = 0; k < iPool; k++ ) { const stats_t &s = m_Stats[pool[k]]; allowed[pool[k]] = ( expr ) ? TRUE : FALSE; }

	int w;

	FILL( s.suicides );
	if ( ( w = FindBest( pool, iRankPool, value, TRUE, NULL ) ) && value[w] > 0 )
		GIVE( w, AWARD_LEMMING );

	FILL( s.shots );
	if ( ( w = FindBest( pool, iRankPool, value, FALSE, NULL ) ) && value[w] < AWARDS_AMMO_MAX_SHOTS )
		GIVE( w, AWARD_WHERES_THE_AMMO );

	FILL( s.armorPickups );
	if ( ( w = FindBest( pool, iRankPool, value, FALSE, NULL ) ) && value[w] <= AWARDS_ARMOR_FEW )
		GIVE( w, AWARD_WHERES_THE_ARMOR );
	if ( ( w = FindBest( pool, iRankPool, value, TRUE, NULL ) ) && value[w] > AWARDS_ARMOR_MANY )
		GIVE( w, AWARD_AC10 );

	FILL( s.headshots );
	if ( ( w = FindBest( pool, iRankPool, value, TRUE, NULL ) ) && value[w] > 0 )
		GIVE( w, AWARD_MARKSMANSHIP );

	FILL( s.kills * 100.0f / ( s.shots + 1.0f ) );
	if ( ( w = FindBest( pool, iRankPool, value, TRUE, NULL ) ) && value[w] > 0 )
		GIVE( w, AWARD_MOST_PROFESSIONAL );

	// Mostly Harmless needs a death to be meaningful and never goes to the
	// Most Deadly player.
	FILL( s.kills * 100.0f / ( s.deaths + 1.0f ) );
	int iDeadly = FindBest( pool, iRankPool, value, TRUE, NULL );
	if ( iDeadly && value[iDeadly] > 0 )
		GIVE( iDeadly, AWARD_MOST_DEADLY );
	else
		iDeadly = 0;
	ALLOW( s.deaths > 0 );
	if ( ( w = FindBest( pool, iRankPool, value, FALSE, allowed ) ) && w != iDeadly )
		GIVE( w, AWARD_MOSTLY_HARMLESS );

	FILL( s.timeOthersInView );
	if ( ( w = FindBest( pool, iRankPool, value, FALSE, NULL ) ) )
		GIVE( w, AWARD_MOST_COWARDLY );

	FILL( s.aliveTime > 0 ? ( s.distance * AWARDS_UNITS_TO_KM ) / ( s.aliveTime / 3600.0f ) : 0 );
	if ( ( w = FindBest( pool, iRankPool, value, TRUE, NULL ) ) && value[w] > AWARDS_FRANTIC_KMH )
		GIVE( w, AWARD_MOST_FRANTIC );

	// Honor is ranked among players that have landed hits on someone.
	FILL( s.backHits );
	ALLOW( s.hits > 0 );
	int iHonorable = FindBest( pool, iRankPool, value, FALSE, allowed );
	if ( iHonorable )
		GIVE( iHonorable, AWARD_MOST_HONORABLE );
	if ( ( w = FindBest( pool, iRankPool, value, TRUE, NULL ) ) && value[w] > 0 && w != iHonorable )
		GIVE( w, AWARD_MOST_DISHONORABLE );

	FILL( s.longestLife );
	int iLongest = FindBest( pool, iRankPool, value, TRUE, NULL );
	if ( iLongest && value[iLongest] > 0 )
		GIVE( iLongest, AWARD_LONGEST_INNINGS );
	else
		iLongest = 0;
	FILL( s.shortestLife );
	ALLOW( s.shortestLife > 0 );
	if ( ( w = FindBest( pool, iRankPool, value, FALSE, allowed ) ) && w != iLongest )
		GIVE( w, AWARD_SHORTEST_INNINGS );

	for ( int k = 0; k < iPool; k++ )
	{
		int i = pool[k];
		if ( m_Stats[i].bestMultiKill >= 4 )
			GIVE( i, AWARD_QUAD_KILL );
		else if ( m_Stats[i].bestMultiKill == 3 )
			GIVE( i, AWARD_TRIPLE_KILL );
		else if ( m_Stats[i].bestMultiKill == 2 )
			GIVE( i, AWARD_DOUBLE_KILL );
	}

#undef GIVE
#undef FILL
#undef ALLOW

	// Pick two per player (Quad Kill always shown first, as on the N64),
	// pack them with the weapon of choice and push any change.
	for ( int i = 1; i <= iMaxClients; i++ )
	{
		stats_t &s = m_Stats[i];
		if ( !s.tracked || s.bot )
			continue;

		int iMask = earned[i];
		int iFirst = AWARD_NONE;
		int iSecond = AWARD_NONE;

		if ( iMask & ( 1 << AWARD_QUAD_KILL ) )
		{
			iFirst = AWARD_QUAD_KILL;
			iMask &= ~( 1 << AWARD_QUAD_KILL );
		}

		for ( int k = 0; k < AWARD_COUNT - 1 && !iSecond; k++ )
		{
			int iAward = s.order[k];
			if ( !( iMask & ( 1 << iAward ) ) )
				continue;

			if ( !iFirst )
				iFirst = iAward;
			else
				iSecond = iAward;
		}

		s.packed = AWARDS_PACK( WeaponOfChoice( s ), iFirst, iSecond );

		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );
		if ( !pPlayer || pPlayer->HasDisconnected || !pPlayer->m_fGameHUDInitialized ||
			FBitSet( pPlayer->pev->flags, FL_FAKECLIENT ) || s.packed == s.lastSentPacked )
			continue;

		MESSAGE_BEGIN( MSG_ONE, gmsgAwards, NULL, pPlayer->edict() );
			WRITE_SHORT( s.packed );
		MESSAGE_END();

		s.lastSentPacked = s.packed;
	}
}
