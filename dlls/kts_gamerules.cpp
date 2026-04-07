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

//=========================================================
// kts_gamerules.cpp — Kick the Snowball gamemode
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "kts_gamerules.h"
#include "game.h"
#include "items.h"
#include "pm_shared.h"

extern int gmsgObjective;
extern int gmsgScoreInfo;
extern int gmsgTeamNames;
extern int gmsgTeamInfo;
extern int gmsgPlayClientSound;
extern int gmsgCtfInfo;
extern int gmsgBanner;
extern int gmsgSpecialEntity;
extern int gmsgStatusIcon;

#define TEAM_BLUE 0
#define TEAM_RED  1

// Speed thresholds for ball-to-player damage
#define KTS_DAMAGE_MIN_SPEED  150.0f
#define KTS_DAMAGE_SCALE      (200.0f / 150.0f)  // (speed-150)*(200/150)

// Force magnitudes applied to ball
#define KTS_FORCE_TOUCH       600.0f   // player walks into ball / punch
#define KTS_FORCE_SLIDE       900.0f   // player is sliding when they touch ball
#define KTS_FORCE_DAMAGE_MULT 3.0f     // damage * 3 = force for explosions/melee (~300 rocket)
#define KTS_FORCE_BULLET_MULT 20.0f    // per-bullet force (~160 per 9mm bullet)

// Ball gets reset after this many seconds motionless
#define KTS_STUCK_TIMEOUT     3.0f

// Ball respawn delay after a goal
#define KTS_BALL_RESPAWN_DELAY 6.0f

// Minimum z before the ball is considered out of bounds
#define KTS_FALLING_Z_LIMIT   -4096.0f

//=========================================================
// CKtsSnowball — physics snowball entity
//=========================================================

class CKtsSnowball : public CBaseEntity
{
public:
	static CKtsSnowball *CreateBall( Vector vecOrigin );
	void Precache( void );
	void Spawn( void );
	void EXPORT BallThink( void );
	void EXPORT BallTouch( CBaseEntity *pOther );
	virtual int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void ResetToMidpoint( void );

	EHANDLE m_hLastToucher;
	float   m_fStuckTime;
	float   m_fBounceTime;
	Vector  m_vLastContactVelocity;
};

void CKtsSnowball::Precache( void )
{
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_SOUND("ball_bounce.wav");
}

CKtsSnowball *CKtsSnowball::CreateBall( Vector vecOrigin )
{
	CKtsSnowball *pBall = GetClassPtr( (CKtsSnowball *)NULL );
	UTIL_SetOrigin( pBall->pev, vecOrigin );
	pBall->pev->angles = g_vecZero;
	pBall->Spawn();
	return pBall;
}

void CKtsSnowball::Spawn( void )
{
	Precache();
	pev->scale = 4.0;
	SET_MODEL( ENT(pev), "models/w_weapons.mdl" );
	pev->body = WEAPON_SNOWBALL - 1;
	pev->classname = MAKE_STRING("kts_snowball");

	pev->movetype   = MOVETYPE_BOUNCE;
	pev->solid      = SOLID_BBOX;
	pev->takedamage = DAMAGE_YES;
	pev->health     = 9999;

	pev->friction   = 0.1f;	// inverse elasticity — 0.1 = 90% energy retained per bounce
	pev->fuser4     = RADAR_SNOWBALL;

	UTIL_SetSize( pev, Vector(-12, -12, -12), Vector(12, 12, 12) );
	UTIL_SetOrigin( pev, pev->origin );

	m_hLastToucher         = NULL;
	m_fStuckTime           = -1.0f;
	m_fBounceTime          = -1.0f;
	m_vLastContactVelocity = g_vecZero;
	pev->iuser1    = 0;
	pev->iuser2    = 0;
	pev->owner     = NULL;

	SetTouch( &CKtsSnowball::BallTouch );
	SetThink( &CKtsSnowball::BallThink );
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CKtsSnowball::BallThink( void )
{
	// Bail out if we've already been scored/removed
	if (g_pGameRules)
	{
		CHalfLifeKickTheSnowball *pKts = (CHalfLifeKickTheSnowball *)g_pGameRules;
		if ((CBaseEntity *)pKts->pBall != this)
			return;
	}

	// Sync last-toucher when the gravity gun picked up or launched the ball.
	// iuser2 is set to the operator's entity index by gravitygun.cpp.
	if (pev->iuser2 != 0)
	{
		edict_t *pGrabber = INDEXENT(pev->iuser2);
		if (!FNullEnt(pGrabber))
			m_hLastToucher = CBaseEntity::Instance(pGrabber);
		pev->iuser2 = 0;
	}

	// -------------------------------------------------------
	// Forcegrab attract / repel logic
	// pev->owner holds the grabbing player's edict while active.
	// pev->iuser1 == 0 → attract toward owner
	// pev->iuser1 == 1 → repel signal (set by player tap)
	// -------------------------------------------------------
	if (!FNullEnt(pev->owner))
	{
		CBaseEntity *pOwner = CBaseEntity::Instance(pev->owner);

		// Release if owner gone or dead
		if (!pOwner || !pOwner->IsAlive())
		{
			pev->owner  = NULL;
			pev->iuser1 = 0;
		}
		else if (pev->iuser1 == 1)
		{
			MAKE_VECTORS(pOwner->pev->v_angle);
			Vector slamDir = gpGlobals->v_forward;

			pev->velocity  = slamDir * 1500.0f;
			m_hLastToucher = pOwner;
			m_fStuckTime   = -1.0f;
			pev->owner     = NULL;
			pev->iuser1    = 0;
			pev->nextthink = gpGlobals->time + 0.1f;
			return;
		}
		else
		{
			// Attract: pull toward owner at 350 u/s
			Vector dir = (pOwner->pev->origin - pev->origin);
			float dist = dir.Length();

			// Auto-release: ball reached the player — slam it away in their
			// view direction so they can't hold it indefinitely.
			if (dist < 60.0f)
			{
				CBaseEntity *p = Instance( pev->owner );
				CBasePlayer *pPlayer = NULL;
				if (p && p->IsPlayer())
				{
					pPlayer = (CBasePlayer *)p;
					pPlayer->EndForceGrab();
				}

				MAKE_VECTORS(pOwner->pev->v_angle);
				pev->velocity  = gpGlobals->v_forward * 1500.0f;
				m_hLastToucher = pOwner;
				m_fStuckTime   = -1.0f;
				pev->owner     = NULL;
				pev->iuser1    = 0;
				pev->nextthink = gpGlobals->time + 0.1f;
				return;
			}

			if (dist > 0.0f) dir = dir * (1.0f / dist);

			pev->velocity  = dir * 350.0f;
			m_fStuckTime   = -1.0f;  // not considered stuck while grabbed
			pev->nextthink = gpGlobals->time + 0.1f;
			return;  // skip normal stuck / fall-out logic
		}
	}

	// Fallen out of bounds check
	if (pev->origin.z < KTS_FALLING_Z_LIMIT)
	{
		ResetToMidpoint();
		return;
	}

	// Stuck (motionless) check
	if (pev->velocity.Length() < 20.0f)
	{
		if (m_fStuckTime < 0.0f)
			m_fStuckTime = gpGlobals->time;
		else if (gpGlobals->time - m_fStuckTime > KTS_STUCK_TIMEOUT)
		{
			ResetToMidpoint();
			return;
		}
	}
	else
	{
		m_fStuckTime = -1.0f;
	}

	pev->nextthink = gpGlobals->time + 0.1f;
}

void CKtsSnowball::BallTouch( CBaseEntity *pOther )
{
	if (!pOther)
		return;

	// Surface bounce sound — non-player contact (wall, floor, brush).
	// Gate on velocity *change* (ΔV) rather than raw speed so that:
	//   • real impacts (velocity direction reverses) → large ΔV → plays
	//   • rolling at constant speed / gravity micro-bounces → tiny ΔV → silent
	if (!pOther->IsPlayer())
	{
		Vector curVel    = pev->velocity;
		float  deltaSpeed = (curVel - m_vLastContactVelocity).Length();
		m_vLastContactVelocity = curVel;

		if (deltaSpeed > 120.0f && gpGlobals->time > m_fBounceTime)
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_AUTO, "ball_bounce.wav",
				min(0.9f, 1),
				ATTN_NORM, 0, PITCH_NORM);
			m_fBounceTime = gpGlobals->time + 0.2f;
		}
		return;
	}

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	if (pPlayer->pev->deadflag != DEAD_NO)
		return;

	// If this player is the current grab owner, ignore the touch
	// (ball is floating toward them — don't apply kick force)
	if (!FNullEnt(pev->owner) && pev->owner == pPlayer->edict())
		return;

	// Another player touched while ball is being grabbed: release the grab
	if (!FNullEnt(pev->owner))
	{
		pev->owner  = NULL;
		pev->iuser1 = 0;
	}

	// Direction from player to ball (horizontal base)
	Vector dir = (pev->origin - pPlayer->pev->origin);
	dir.z = 0.0f;

	// Blend in the player's horizontal movement direction so that running
	// along a wall redirects the ball along it rather than back into it.
	// A stationary player still gets a clean away-from-body kick.
	Vector playerVel = pPlayer->pev->velocity;
	playerVel.z = 0.0f;
	float playerSpeed = playerVel.Length();
	if (playerSpeed > 50.0f)
	{
		Vector moveDir = playerVel * (1.0f / playerSpeed);
		dir = dir + moveDir * 0.75f;
	}

	dir.z = 0.1f; // slight upward component for natural feel
	float len = dir.Length();
	if (len > 0)
		dir = dir * (1.0f / len);
	else
	{
		// Fallback: use view direction
		MAKE_VECTORS(pPlayer->pev->v_angle);
		dir = gpGlobals->v_forward;
	}

	// Damage to player if ball is already fast on impact
	float preSpeed = pev->velocity.Length();
	if (preSpeed > KTS_DAMAGE_MIN_SPEED)
	{
		float dmg = (preSpeed - KTS_DAMAGE_MIN_SPEED) * KTS_DAMAGE_SCALE;
		// pPlayer->TakeDamage( pev, pev, dmg, DMG_CLUB );
	}

	// Kick force: slide gives more power
	float kickForce = pPlayer->m_fSelacoSliding ? KTS_FORCE_SLIDE : KTS_FORCE_TOUCH;
	pev->velocity = dir * kickForce;

	// Update last-toucher
	m_hLastToucher = pPlayer;
	m_fStuckTime   = -1.0f;
}

int CKtsSnowball::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if (!pevInflictor)
		return 0;

	// Bullets need a much higher per-hit multiplier since their damage is low (7-8 per shot).
	// Explosions and melee keep the base multiplier.
	float mult      = (bitsDamageType & DMG_BULLET) ? KTS_FORCE_BULLET_MULT : KTS_FORCE_DAMAGE_MULT;
	float magnitude = flDamage * mult;

	Vector pushDir = (pev->origin - pevInflictor->origin);
	float len = pushDir.Length();
	if (len > 0)
		pushDir = pushDir * (1.0f / len);
	else
	{
		MAKE_VECTORS( pevInflictor->angles );
		pushDir = gpGlobals->v_forward;
	}

	pev->velocity = pushDir * magnitude;
	m_fStuckTime  = -1.0f;
	// Release any active forcegrab — weapon/explosion impact cancels it
	pev->owner  = NULL;
	pev->iuser1 = 0;

	// Update last-toucher if attacker is a player
	if (pevAttacker)
	{
		CBaseEntity *pAttacker = CBaseEntity::Instance(pevAttacker);
		if (pAttacker && pAttacker->IsPlayer())
			m_hLastToucher = pAttacker;
	}

	return 1;
}

// -------------------------------------------------------
// FindSafeBallSpawn — finds the entity closest to `center`
// among info_player_deathmatch, weapon_*, and ammo_* and
// returns its origin + a standing height offset.
// Note: UTIL_FindEntityByClassname only does exact matching,
// so weapon_* and ammo_* require a full edict-list scan.
// -------------------------------------------------------
static Vector FindSafeBallSpawn( const Vector &center,
	const Vector &redGoalOrigin, const Vector &blueGoalOrigin )
{
	CBaseEntity *pBestSpot = NULL;
	float        bestDist  = 9999999.0f;

	// info_player_deathmatch — exact classname, use the fast lookup
	CBaseEntity *pSpot = NULL;
	while ((pSpot = UTIL_FindEntityByClassname(pSpot, "info_player_deathmatch")) != NULL)
	{
		float dist = (pSpot->pev->origin - center).Length();
		if (dist < bestDist)
		{
			bestDist  = dist;
			pBestSpot = pSpot;
		}
	}

	// weapon_* and ammo_* — prefix match requires iterating all edicts
	for (int i = 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t *pEdict = INDEXENT(i);
		if (!pEdict || pEdict->free) continue;
		const char *cn = STRING(pEdict->v.classname);
		if (!cn || !*cn) continue;
		if (strncmp(cn, "weapon_", 7) != 0 && strncmp(cn, "ammo_", 5) != 0) continue;
		CBaseEntity *pEnt = CBaseEntity::Instance(pEdict);
		if (!pEnt) continue;
		float dist = (pEnt->pev->origin - center).Length();
		if (dist < bestDist)
		{
			bestDist  = dist;
			pBestSpot = pEnt;
		}
	}

	if (pBestSpot)
		return pBestSpot->pev->origin + Vector(0, 0, 36);

	// No entities found at all — return center as absolute last resort
	return center + Vector(0, 0, 36);
}

void CKtsSnowball::ResetToMidpoint( void )
{
	m_hLastToucher         = NULL;
	m_fStuckTime           = -1.0f;
	m_vLastContactVelocity = g_vecZero;
	pev->iuser1    = 0;
	pev->iuser2    = 0;
	pev->owner     = NULL;

	if (!g_pGameRules)
		return;

	CHalfLifeKickTheSnowball *pKts = (CHalfLifeKickTheSnowball *)g_pGameRules;
	if (!pKts->pRedGoal || !pKts->pBlueGoal)
		return;

	Vector midpoint = (pKts->pRedGoal->pev->origin + pKts->pBlueGoal->pev->origin) * 0.5f;
	Vector spawnPos = FindSafeBallSpawn(midpoint,
		pKts->pRedGoal->pev->origin, pKts->pBlueGoal->pev->origin);

	float distToSpawn = (pev->origin - spawnPos).Length();

	pev->velocity   = g_vecZero;
	pev->avelocity  = g_vecZero;
	UTIL_SetOrigin(pev, spawnPos);

	pev->nextthink = gpGlobals->time + 0.1f;

	// Only announce if the ball actually had to travel — suppress for a ball
	// that is already idling at the midpoint (e.g. just spawned, no one nearby).
	if (distToSpawn > 128.0f)
	{
		UTIL_ClientPrintAll(HUD_PRINTTALK, "Ball reset to mid field!\n");

		MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
			WRITE_BYTE(CLIENT_SOUND_AIRHORN);
		MESSAGE_END();
	}
}

LINK_ENTITY_TO_CLASS( kts_snowball, CKtsSnowball );

//=========================================================
// CKtsGoal — trigger zone that the ball scores in
//=========================================================

class CKtsGoal : public CBaseEntity
{
public:
	static CKtsGoal *CreateGoal( Vector vecOrigin, int team );
	void Precache( void );
	void Spawn( void );
	void EXPORT GoalTouch( CBaseEntity *pOther );
};

CKtsGoal *CKtsGoal::CreateGoal( Vector vecOrigin, int team )
{
	CKtsGoal *pGoal = GetClassPtr( (CKtsGoal *)NULL );
	UTIL_SetOrigin( pGoal->pev, vecOrigin );
	pGoal->pev->angles = g_vecZero;
	pGoal->Spawn();

	pGoal->pev->body   = team;
	pGoal->pev->fuser4 = (team == TEAM_RED) ? RADAR_BASE_RED : RADAR_BASE_BLUE;

	// Broadcast goal location to radar
	MESSAGE_BEGIN(MSG_ALL, gmsgSpecialEntity);
		WRITE_BYTE(team);
		WRITE_BYTE(1);
		WRITE_COORD(pGoal->pev->origin.x);
		WRITE_COORD(pGoal->pev->origin.y);
		WRITE_COORD(pGoal->pev->origin.z);
		WRITE_BYTE(pGoal->pev->fuser4);
	MESSAGE_END();

	return pGoal;
}

void CKtsGoal::Precache( void )
{
	PRECACHE_MODEL("models/teleporter_orange_rings.mdl");
}

void CKtsGoal::Spawn( void )
{
	Precache();
	pev->classname = MAKE_STRING("kts_goal");
	pev->movetype  = MOVETYPE_NONE;
	pev->solid     = SOLID_TRIGGER;
	SET_MODEL( ENT(pev), "models/teleporter_orange_rings.mdl" );
	pev->animtime = gpGlobals->time;
	pev->framerate = 1.0f;
	pev->rendermode = kRenderTransColor;
	pev->renderamt = 128;

	// 64w x 16d x 72h — orientation-agnostic trigger volume
	UTIL_SetSize( pev, Vector(-32, -8, -72), Vector(32, 8, 72) );
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CKtsGoal::GoalTouch );
}

void CKtsGoal::GoalTouch( CBaseEntity *pOther )
{
	if (!pOther || !FClassnameIs(pOther->pev, "kts_snowball"))
		return;

	if (!g_pGameRules)
		return;

	CHalfLifeKickTheSnowball *pKts = (CHalfLifeKickTheSnowball *)g_pGameRules;

	// Ignore if this isn't the active ball (already scored, or FL_KILLME re-touch)
	if ((CBaseEntity *)pKts->pBall != pOther)
		return;

	// The ball enters this goal → the OPPOSING team scores
	int scoringTeam = (pev->body == TEAM_RED) ? TEAM_BLUE : TEAM_RED;

	pKts->OnGoalScored(scoringTeam, pOther);
}

LINK_ENTITY_TO_CLASS( kts_goal, CKtsGoal );

//=========================================================
// CHalfLifeKickTheSnowball — gamerules class
//=========================================================

CHalfLifeKickTheSnowball::CHalfLifeKickTheSnowball()
{
	m_DisableDeathPenalty = FALSE;
	pRedGoal = pBlueGoal = pBall = NULL;
	m_fSpawnBlueGoal   = gpGlobals->time + 2.0f;
	m_fSpawnRedGoal    = gpGlobals->time + 2.0f;
	m_fBallRespawnTime = -1.0f;
	m_iBlueScore = m_iRedScore = m_iBlueMode = m_iRedMode = 0;
	UTIL_PrecacheOther("kts_snowball");
	UTIL_PrecacheOther("kts_goal");
}

BOOL CHalfLifeKickTheSnowball::IsSpawnPointValid( CBaseEntity *pSpot )
{
	CBaseEntity *ent = NULL;

	if (FBitSet(pSpot->pev->spawnflags, SF_GIVEITEM))
		return FALSE;

	while ((ent = UTIL_FindEntityInSphere(ent, pSpot->pev->origin, 1024.0f)) != NULL)
	{
		if (FClassnameIs(ent->pev, "kts_goal"))
			return FALSE;
	}

	return TRUE;
}

edict_t *CHalfLifeKickTheSnowball::EntSelectSpawnPoint( const char *szSpawnPoint )
{
	CBaseEntity *pSpot = NULL;

	if (g_pGameRules->IsDeathmatch())
	{
		for (int i = RANDOM_LONG(1, 5); i > 0; i--)
			pSpot = UTIL_FindEntityByClassname(pSpot, szSpawnPoint);
		if (FNullEnt(pSpot))
			pSpot = UTIL_FindEntityByClassname(pSpot, szSpawnPoint);

		CBaseEntity *pFirstSpot = pSpot;
		do
		{
			if (pSpot && IsSpawnPointValid(pSpot))
				goto ReturnSpot;
			pSpot = UTIL_FindEntityByClassname(pSpot, szSpawnPoint);
		}
		while (pSpot != pFirstSpot);

		pSpot = NULL;
	}

ReturnSpot:
	if (FNullEnt(pSpot))
	{
		ALERT(at_error, "could not find %s for kts goal spot\n", szSpawnPoint);
		return NULL;
	}

	return pSpot->edict();
}

void CHalfLifeKickTheSnowball::AutoJoin( CBasePlayer *pPlayer, int team )
{
	int blueteam = 0, redteam = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
		if (plr && plr != pPlayer && plr->IsPlayer() && !plr->HasDisconnected)
		{
			if (plr->pev->fuser4 == TEAM_BLUE)
				blueteam++;
			else
				redteam++;
		}
	}

	if (team > TEAM_RED)
		pPlayer->pev->fuser4 = (redteam >= blueteam) ? TEAM_BLUE : TEAM_RED;
	else
		pPlayer->pev->fuser4 = team;

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

	char text[256];
	sprintf(text, "[KtS] You're on team '%s'\n", pPlayer->m_szTeamName);
	UTIL_SayText(text, pPlayer);

	MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo);
		WRITE_BYTE(ENTINDEX(pPlayer->edict()));
		WRITE_STRING(pPlayer->m_szTeamName);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
		WRITE_BYTE(ENTINDEX(pPlayer->edict()));
		WRITE_SHORT(pPlayer->pev->frags);
		WRITE_SHORT(pPlayer->m_iDeaths);
		WRITE_SHORT(pPlayer->m_iRoundWins);
		WRITE_SHORT(GetTeamIndex(pPlayer->m_szTeamName) + 1);
	MESSAGE_END();

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING(UTIL_VarArgs("Kick the snowball into the %s goal!",
				(pPlayer->pev->fuser4 == TEAM_RED) ? "blue" : "red"));
			WRITE_STRING(UTIL_VarArgs("You're on %s team",
				(pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
			WRITE_BYTE(0);
			WRITE_STRING("");
		MESSAGE_END();
	}
}

void CHalfLifeKickTheSnowball::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD(pPlayer);

	MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
		WRITE_BYTE(2);
		WRITE_STRING("blue");
		WRITE_STRING("red");
	MESSAGE_END();

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgCtfInfo, NULL, pPlayer->edict());
			WRITE_BYTE(m_iBlueScore);
			WRITE_BYTE(m_iRedScore);
			WRITE_BYTE(m_iBlueMode);
			WRITE_BYTE(m_iRedMode);
		MESSAGE_END();
	}

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity *plr = UTIL_PlayerByIndex(i);
		if (plr && !FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgTeamInfo, NULL, pPlayer->edict());
				WRITE_BYTE(plr->entindex());
				WRITE_STRING(plr->TeamID());
			MESSAGE_END();
		}
	}

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Kick the Snowball");
			WRITE_STRING("Choose a team");
			WRITE_BYTE(0);
			WRITE_STRING("");
		MESSAGE_END();
	}

	// Send goal positions for radar if already spawned
	if (pRedGoal && pBlueGoal)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgSpecialEntity, NULL, pPlayer->edict());
			WRITE_BYTE(TEAM_RED);
			WRITE_BYTE(1);
			WRITE_COORD(pRedGoal->pev->origin.x);
			WRITE_COORD(pRedGoal->pev->origin.y);
			WRITE_COORD(pRedGoal->pev->origin.z);
			WRITE_BYTE(pRedGoal->pev->fuser4);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgSpecialEntity, NULL, pPlayer->edict());
			WRITE_BYTE(TEAM_BLUE);
			WRITE_BYTE(1);
			WRITE_COORD(pBlueGoal->pev->origin.x);
			WRITE_COORD(pBlueGoal->pev->origin.y);
			WRITE_COORD(pBlueGoal->pev->origin.z);
			WRITE_BYTE(pBlueGoal->pev->fuser4);
		MESSAGE_END();
	}
}

void CHalfLifeKickTheSnowball::Think( void )
{
	CHalfLifeMultiplay::Think();

	// Spawn blue goal
	if (m_fSpawnBlueGoal != -1.0f && m_fSpawnBlueGoal < gpGlobals->time)
	{
		edict_t *pentSpot = EntSelectSpawnPoint(ktsspawn1.string);
		if (pentSpot)
		{
			ALERT(at_console, "[KtS] Blue goal set!\n");
			pBlueGoal = CKtsGoal::CreateGoal(pentSpot->v.origin, TEAM_BLUE);
			m_fSpawnBlueGoal = -1.0f;
			pentSpot->v.solid   = SOLID_NOT;
			pentSpot->v.effects |= EF_NODRAW;
		}
		else
			m_fSpawnBlueGoal = gpGlobals->time + 2.0f;
	}

	// Spawn red goal
	if (m_fSpawnRedGoal != -1.0f && m_fSpawnRedGoal < gpGlobals->time)
	{
		edict_t *pentSpot2 = EntSelectSpawnPoint(ktsspawn2.string);
		if (pentSpot2)
		{
			ALERT(at_console, "[KtS] Red goal set!\n");
			pRedGoal = CKtsGoal::CreateGoal(pentSpot2->v.origin, TEAM_RED);
			m_fSpawnRedGoal = -1.0f;
			pentSpot2->v.solid   = SOLID_NOT;
			pentSpot2->v.effects |= EF_NODRAW;
		}
		else
			m_fSpawnRedGoal = gpGlobals->time + 2.0f;
	}

	// Spawn ball at midpoint after goals are ready and respawn timer has elapsed
	if (m_fBallRespawnTime != -1.0f && m_fBallRespawnTime < gpGlobals->time)
	{
		m_fBallRespawnTime = -1.0f;  // Always clear so it doesn't keep firing
		if (pRedGoal && pBlueGoal && !pBall)
			SpawnBallAtMidpoint();
	}
	else if (m_fBallRespawnTime == -1.0f && !pBall &&
		m_fSpawnBlueGoal == -1.0f && m_fSpawnRedGoal == -1.0f &&
		pRedGoal && pBlueGoal)
	{
		// Initial ball spawn — both goals must be ready, no pending respawn
		SpawnBallAtMidpoint();
	}
}

void CHalfLifeKickTheSnowball::SpawnBallAtMidpoint( void )
{
	if (!pRedGoal || !pBlueGoal)
		return;

	MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
		WRITE_BYTE(CLIENT_SOUND_SOCCER);
	MESSAGE_END();

	Vector midpoint = (pRedGoal->pev->origin + pBlueGoal->pev->origin) * 0.5f;
	Vector spawnPos = FindSafeBallSpawn(midpoint,
		pRedGoal->pev->origin, pBlueGoal->pev->origin);

	CKtsSnowball *pNewBall = CKtsSnowball::CreateBall(spawnPos);
	pBall = pNewBall;

	// Replace the lingering "Ball incoming!" objective now that the ball is live
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
		if (plr && plr->IsPlayer() && !plr->HasDisconnected && !FBitSet(plr->pev->flags, FL_FAKECLIENT))
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, plr->edict());
				WRITE_STRING("Ball in play!");
				if (plr->IsSpectator())
					WRITE_STRING("Watch the action!");
				else
					WRITE_STRING(UTIL_VarArgs("Kick the snowball into the %s goal!",
						(plr->pev->fuser4 == TEAM_RED) ? "blue" : "red"));
				WRITE_BYTE(0);
				WRITE_STRING("");
			MESSAGE_END();
		}
	}
}

void CHalfLifeKickTheSnowball::OnGoalScored( int scoringTeam, CBaseEntity *pScoredBall )
{
	if (!pScoredBall)
		return;

	MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
		WRITE_BYTE(CLIENT_SOUND_GOL);
	MESSAGE_END();

	CKtsSnowball *pBallPtr = (CKtsSnowball *)pScoredBall;

	// Identify scorer (last toucher on the scoring team) or own-goal maker
	CBasePlayer *pScorer  = NULL;
	CBasePlayer *pOwnGoal = NULL;

	if (pBallPtr->m_hLastToucher)
	{
		CBasePlayer *pLast = (CBasePlayer *)(CBaseEntity *)pBallPtr->m_hLastToucher;
		if (pLast && pLast->IsPlayer() && !pLast->HasDisconnected)
		{
			if (GetTeamIndex(pLast->m_szTeamName) == scoringTeam)
				pScorer = pLast;
			else
				pOwnGoal = pLast;
		}
	}

	// Remove the ball now (touches can still fire briefly — guard with pBall == NULL)
	pBall = NULL;
	UTIL_Remove(pScoredBall);

	// Own-goal penalty: deduct a frag from the own-goal maker
	if (pOwnGoal)
	{
		pOwnGoal->pev->frags -= 1;
		MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
			WRITE_BYTE(ENTINDEX(pOwnGoal->edict()));
			WRITE_SHORT(pOwnGoal->pev->frags);
			WRITE_SHORT(pOwnGoal->m_iDeaths);
			WRITE_SHORT(pOwnGoal->m_iRoundWins);
			WRITE_SHORT(GetTeamIndex(pOwnGoal->m_szTeamName) + 1);
		MESSAGE_END();
	}

	// Award frags and round wins to scoring team; bonus frag to scorer
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
		if (plr && plr->IsPlayer() && !plr->HasDisconnected)
		{
			if (GetTeamIndex(plr->m_szTeamName) == scoringTeam)
			{
				plr->pev->frags++;
				plr->m_iRoundWins++;
				if (plr == pScorer)
					plr->pev->frags++; // bonus for the player who kicked it in

				MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
					WRITE_BYTE(ENTINDEX(plr->edict()));
					WRITE_SHORT(plr->pev->frags);
					WRITE_SHORT(plr->m_iDeaths);
					WRITE_SHORT(plr->m_iRoundWins);
					WRITE_SHORT(GetTeamIndex(plr->m_szTeamName) + 1);
				MESSAGE_END();
			}
		}
	}

	// Celebrate for scorer
	if (pScorer)
		pScorer->Celebrate();

	// Banner announcement
	const char *teamName = (scoringTeam == TEAM_BLUE) ? "Blue" : "Red";
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
		if (plr && plr->IsPlayer() && !plr->HasDisconnected && !FBitSet(plr->pev->flags, FL_FAKECLIENT))
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgBanner, NULL, plr->edict());
				WRITE_STRING(UTIL_VarArgs("GOAL! %s Team Scores!", teamName));
				WRITE_STRING(pScorer ? UTIL_VarArgs("%s kicked it in!", STRING(pScorer->pev->netname)) : "Goal!");
				WRITE_BYTE(80);
			MESSAGE_END();
		}
	}

	// HUD score update (also checks scorelimit → GoToIntermission)
	if (scoringTeam == TEAM_BLUE)
		UpdateHud(-1, 0);
	else
		UpdateHud(0, -1);

	// Schedule ball respawn — 3-second delay
	m_fBallRespawnTime = gpGlobals->time + KTS_BALL_RESPAWN_DELAY;

	// Broadcast "Ball incoming!" objective notice
	MESSAGE_BEGIN(MSG_ALL, gmsgObjective);
		WRITE_STRING("Ball incoming!");
		WRITE_STRING("Respawning in 3 seconds...");
		WRITE_BYTE(0);
		WRITE_STRING("");
	MESSAGE_END();
}

void CHalfLifeKickTheSnowball::PlayerThink( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerThink(pPlayer);

	if (!pPlayer->HasDisconnected)
	{
		if (pPlayer->m_iShowGameModeMessage > -1 &&
			pPlayer->m_iShowGameModeMessage < gpGlobals->time &&
			!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgBanner, NULL, pPlayer->edict());
				WRITE_STRING(UTIL_VarArgs("You Are On Team %s",
					pPlayer->pev->fuser4 == TEAM_RED ? "Red" : "Blue"));
				WRITE_STRING(UTIL_VarArgs("Kick the snowball into the %s goal!",
					pPlayer->pev->fuser4 == TEAM_RED ? "Blue" : "Red"));
				WRITE_BYTE(80);
			MESSAGE_END();
			pPlayer->m_iShowGameModeMessage = -1;
		}
	}
}

void CHalfLifeKickTheSnowball::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);
	CHalfLifeMultiplay::SavePlayerModel(pPlayer);

	// New player: hold at team-select observer screen
	if (pPlayer->pev->iuser3 > 0)
	{
		pPlayer->pev->iuser3 = OBS_UNDECIDED_BOTH;
		return;
	}
	else if (pPlayer->pev->iuser3 == 0) // Spectator now joining
	{
		AutoJoin(pPlayer, pPlayer->m_iObserverWeapon);
		pPlayer->pev->iuser3 = -1;
		pPlayer->m_iObserverWeapon = 0;
		pPlayer->m_iShowGameModeMessage = gpGlobals->time + 0.5f;
	}

	pPlayer->GiveNamedItem("weapon_gravitygun");
}

extern DLL_GLOBAL BOOL g_fGameOver;

void CHalfLifeKickTheSnowball::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	char text[1024];
	char *mdls = g_engfuncs.pfnInfoKeyValue(infobuffer, "model");
	int clientIndex = pPlayer->entindex();

	if (!pPlayer->m_szTeamName || !strlen(pPlayer->m_szTeamName))
		return;

	if (g_fGameOver)
		return;

	if (pPlayer->IsSpectator())
		return;

	if (!stricmp("red", pPlayer->m_szTeamName) && !stricmp("santa", mdls))
	{
		ClientPrint(pPlayer->pev, HUD_PRINTCONSOLE,
			"[KtS] You're on team '%s'. To change, type 'model iceman'\n", pPlayer->m_szTeamName);
		CLIENT_COMMAND(pPlayer->edict(), "model santa\n");
		return;
	}
	if (!stricmp("blue", pPlayer->m_szTeamName) && !stricmp("iceman", mdls))
	{
		ClientPrint(pPlayer->pev, HUD_PRINTCONSOLE,
			"[KtS] You're on team '%s'. To change, type 'model santa'\n", pPlayer->m_szTeamName);
		return;
	}

	if (stricmp(mdls, "iceman") && stricmp(mdls, "santa"))
	{
		g_engfuncs.pfnSetClientKeyValue(clientIndex,
			g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model",
			(char *)(pPlayer->pev->fuser4 == TEAM_RED ? "santa" : "iceman"));
		sprintf(text, "[KtS] Can't change team to '%s'\n", mdls);
		UTIL_SayText(text, pPlayer);
		sprintf(text, "[KtS] Server limits teams to 'iceman (blue), santa (red)'\n");
		UTIL_SayText(text, pPlayer);
		return;
	}

	m_DisableDeathPenalty = TRUE;

	ClearMultiDamage();
	pPlayer->pev->health = 0;
	pPlayer->Killed(pPlayer->pev, VARS(INDEXENT(0)), GIB_ALWAYS);

	m_DisableDeathPenalty = FALSE;

	int id = TEAM_BLUE;
	if (!stricmp(mdls, "santa"))
		id = TEAM_RED;
	pPlayer->pev->fuser4 = id;

	if (pPlayer->pev->fuser4 == TEAM_RED)
	{
		strncpy(pPlayer->m_szTeamName, "red", TEAM_NAME_LENGTH);
		g_engfuncs.pfnSetClientKeyValue(clientIndex,
			g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "santa");
	}
	else
	{
		strncpy(pPlayer->m_szTeamName, "blue", TEAM_NAME_LENGTH);
		g_engfuncs.pfnSetClientKeyValue(clientIndex,
			g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()), "model", "iceman");
	}

	sprintf(text, "[KtS] %s has changed to team '%s'\n",
		STRING(pPlayer->pev->netname), pPlayer->m_szTeamName);
	UTIL_SayTextAll(text, pPlayer);

	MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo);
		WRITE_BYTE(ENTINDEX(pPlayer->edict()));
		WRITE_STRING(pPlayer->m_szTeamName);
	MESSAGE_END();

	MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
		WRITE_BYTE(ENTINDEX(pPlayer->edict()));
		WRITE_SHORT(pPlayer->pev->frags);
		WRITE_SHORT(pPlayer->m_iDeaths);
		WRITE_SHORT(pPlayer->m_iRoundWins);
		WRITE_SHORT(GetTeamIndex(pPlayer->m_szTeamName) + 1);
	MESSAGE_END();

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING(UTIL_VarArgs("Kick the snowball into the %s goal!",
				(pPlayer->pev->fuser4 == TEAM_RED) ? "blue" : "red"));
			WRITE_STRING(UTIL_VarArgs("You're on %s team",
				(pPlayer->pev->fuser4 == TEAM_RED) ? "red" : "blue"));
			WRITE_BYTE(0);
			WRITE_STRING("");
		MESSAGE_END();
	}
}

void CHalfLifeKickTheSnowball::ClientDisconnected( edict_t *pClient )
{
	CHalfLifeMultiplay::ClientDisconnected(pClient);

	if (pClient)
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance(pClient);
		if (pPlayer && pBall)
		{
			// Clear last-touch reference if the disconnecting player was the last toucher
			CBaseEntity *pBallEnt = (CBaseEntity *)pBall;
			if (pBallEnt)
			{
				CKtsSnowball *pActualBall = (CKtsSnowball *)pBallEnt;
				CBaseEntity *pToucher = (CBaseEntity *)pActualBall->m_hLastToucher;
				if (pToucher && pToucher == pPlayer)
					pActualBall->m_hLastToucher = NULL;
			}
		}
	}
}

void CHalfLifeKickTheSnowball::UpdateHud( int bluemode, int redmode, CBasePlayer *pPlayer )
{
	int bluescore = 0;
	int redscore  = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
		if (plr && plr->IsPlayer() && !plr->HasDisconnected)
		{
			if (plr->pev->fuser4 == TEAM_RED)
				redscore += plr->m_iRoundWins;
			else
				bluescore += plr->m_iRoundWins;
		}
	}

	if (bluemode != -1) m_iBlueMode = bluemode;
	if (redmode  != -1) m_iRedMode  = redmode;

	m_iBlueScore = bluescore;
	m_iRedScore  = redscore;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
		if (plr && plr->IsPlayer() && !plr->HasDisconnected)
		{
			if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
			{
				MESSAGE_BEGIN(MSG_ONE, gmsgCtfInfo, NULL, plr->edict());
					WRITE_BYTE(m_iBlueScore);
					WRITE_BYTE(m_iRedScore);
					plr == pPlayer && plr->pev->fuser4 == TEAM_RED
						? WRITE_BYTE(3) : WRITE_BYTE(bluemode != -1 ? bluemode : m_iBlueMode);
					plr == pPlayer && plr->pev->fuser4 == TEAM_BLUE
						? WRITE_BYTE(3) : WRITE_BYTE(redmode != -1 ? redmode : m_iRedMode);
				MESSAGE_END();
			}
		}
	}

	// End session if score limit reached
	if (redscore >= scorelimit.value || bluescore >= scorelimit.value)
		GoToIntermission();
}

void CHalfLifeKickTheSnowball::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	if (m_DisableDeathPenalty)
		return;

	// If the killer is the snowball, resolve kill credit from last-toucher
	if (pInflictor && FClassnameIs(pInflictor, "kts_snowball"))
	{
		CKtsSnowball *pActualBall = NULL;
		CBaseEntity *pBallEnt = (CBaseEntity *)pBall;
		if (pBallEnt && FClassnameIs(pBallEnt->pev, "kts_snowball"))
			pActualBall = (CKtsSnowball *)pBallEnt;

		CBasePlayer *pLastToucher = NULL;
		if (pActualBall && pActualBall->m_hLastToucher)
			pLastToucher = (CBasePlayer *)(CBaseEntity *)pActualBall->m_hLastToucher;

		if (pLastToucher && pLastToucher->IsPlayer() && !pLastToucher->HasDisconnected)
		{
			// Determine if it's a kill or self-kill
			if (pLastToucher != pVictim &&
				GetTeamIndex(pLastToucher->m_szTeamName) != GetTeamIndex(pVictim->m_szTeamName))
			{
				// Enemy last-toucher gets kill credit
				CHalfLifeMultiplay::PlayerKilled(pVictim, pLastToucher->pev, pInflictor);
			}
			else
			{
				// Same team or self → treat as self-kill for victim
				CHalfLifeMultiplay::PlayerKilled(pVictim, pVictim->pev, pInflictor);
			}
		}
		else
		{
			// No valid last-toucher; treat as environmental death
			CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);
		}
		return;
	}

	CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);
}

int CHalfLifeKickTheSnowball::GetTeamIndex( const char *pTeamName )
{
	if (pTeamName && *pTeamName != 0)
	{
		if (!strcmp(pTeamName, "red"))
			return TEAM_RED;
		else
			return TEAM_BLUE;
	}
	return -1;
}

int CHalfLifeKickTheSnowball::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if (!pPlayer || (pTarget && !pTarget->IsPlayer()))
		return GR_NOTTEAMMATE;

	if ((*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') &&
		!stricmp(GetTeamID(pPlayer), GetTeamID(pTarget)))
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

const char *CHalfLifeKickTheSnowball::GetTeamID( CBaseEntity *pEntity )
{
	if (pEntity == NULL || pEntity->pev == NULL)
		return "";
	return pEntity->TeamID();
}

BOOL CHalfLifeKickTheSnowball::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
{
	CBaseEntity *pTgt = CBaseEntity::Instance(target);
	if (pTgt && pTgt->IsPlayer())
	{
		if (PlayerRelationship(pPlayer, pTgt) == GR_TEAMMATE)
			return FALSE;
	}
	return CHalfLifeMultiplay::ShouldAutoAim(pPlayer, target);
}

BOOL CHalfLifeKickTheSnowball::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if (pAttacker && PlayerRelationship(pPlayer, pAttacker) == GR_TEAMMATE)
	{
		if ((friendlyfire.value == 0) && (pAttacker != pPlayer))
			return FALSE;
	}
	return CHalfLifeMultiplay::FPlayerCanTakeDamage(pPlayer, pAttacker);
}

BOOL CHalfLifeKickTheSnowball::IsTeamplay( void )
{
	return TRUE;
}

BOOL CHalfLifeKickTheSnowball::MutatorAllowed( const char *mutator )
{
	if (strstr(mutator, g_szMutators[MUTATOR_THIRDPERSON - 1]) || atoi(mutator) == MUTATOR_THIRDPERSON)
		return FALSE;

	return CHalfLifeMultiplay::MutatorAllowed(mutator);
}

BOOL CHalfLifeKickTheSnowball::CanHaveNamedItem( CBasePlayer *pPlayer, const char *pszItemName )
{
	if (strcmp(pszItemName, "weapon_fists") != 0 &&
		strcmp(pszItemName, "weapon_gravitygun") != 0) {
		return FALSE;
	}

	return TRUE;
}

int CHalfLifeKickTheSnowball::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_GUN_NO;
}

int CHalfLifeKickTheSnowball::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_AMMO_NO;
}

BOOL CHalfLifeKickTheSnowball::IsAllowedToDropWeapon( CBasePlayer *pPlayer )
{
	return FALSE;
}

BOOL CHalfLifeKickTheSnowball::AllowRuneSpawn( const char *szRune )
{
	if (!strcmp("rune_ammo", szRune))
		return FALSE;
	return TRUE;
}

const char *itemList[] =
{
	"item_healthkit",
	"item_battery",
	"item_longjump",
};

BOOL CHalfLifeKickTheSnowball::IsAllowedToSpawn( CBaseEntity *pEntity )
{
	if (!FBitSet(pEntity->pev->spawnflags, SF_GIVEITEM) &&
		(strncmp(STRING(pEntity->pev->classname), "weapon_", 7) == 0 ||
		strncmp(STRING(pEntity->pev->classname), "ammo_", 5) == 0))
	{
		CBaseEntity::Create((char *)itemList[RANDOM_LONG(0, ARRAYSIZE(itemList)-1)], pEntity->pev->origin, pEntity->pev->angles, pEntity->pev->owner);
		return FALSE;
	}

	return TRUE;
}

