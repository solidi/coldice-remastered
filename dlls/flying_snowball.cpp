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
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "game.h"
#include "gamerules.h"

LINK_ENTITY_TO_CLASS( flying_snowball, CFlyingSnowball );

namespace
{
	const float SNOWBALL_TRACK_MAX_DIST = 2400.0f;
	const float SNOWBALL_TRACK_MIN_DOT = 0.50f;
	const float SNOWBALL_TRACK_AIM_DOT = 0.80f;
	const float SNOWBALL_TRACK_NUDGE = 0.08f;
	const float SNOWBALL_TRACK_THINK_INTERVAL = 0.02f;

	inline BOOL SnowballHasWorldLineOfSight( const Vector &vecStart, const Vector &vecEnd, edict_t *pentIgnore )
	{
		TraceResult tr;
		UTIL_TraceLine( vecStart, vecEnd, ignore_monsters, pentIgnore, &tr );
		return ( tr.flFraction >= 1.0f );
	}
}

CFlyingSnowball * CFlyingSnowball::Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, CBasePlayer *m_pPlayer )
{
	CFlyingSnowball *pSnowball = GetClassPtr( (CFlyingSnowball *)NULL );
	pSnowball->pev->owner = ENT(pevOwner);
	pSnowball->pev->angles = vecVelocity;
	pSnowball->Spawn();
	UTIL_SetOrigin( pSnowball->pev, vecStart );
	pSnowball->pev->velocity = vecVelocity;
	pSnowball->AcquireTrackTarget();
	pSnowball->ApplyTrackNudge();

	// Maximum lifetime so snowballs do not accumulate and exhaust the entity pool.
	pSnowball->pev->dmgtime = gpGlobals->time + 8.0;

	// Tumble through the air
	//pSnowball->pev->avelocity.x = -1000;
	pSnowball->pev->gravity = 0.1;
	pSnowball->pev->friction = 0;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(pSnowball->entindex());	// entity
		WRITE_SHORT(g_sModelIndexSmoke2);	// model
		WRITE_BYTE( 2 ); // life
		WRITE_BYTE( 2 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 100 );	// brightness
	MESSAGE_END();

	SET_MODEL(ENT(pSnowball->pev), "models/w_weapons.mdl");
	pSnowball->pev->body = WEAPON_SNOWBALL - 1;

	if (m_pPlayer)
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "snowball_throw.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF));

	return pSnowball;
}

void CFlyingSnowball::Spawn( )
{
	Precache( );

	// We want it to be affected by gravity, and hit objects
	// within the game.
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->classname = MAKE_STRING("flying_snowball");

	// Use the world wrench model.
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_SNOWBALL - 1;

	// Set the origin and size for the HL engine collision
	// tables.
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));

	pev->angles.x -= 30;
	pev->angles.x = -(pev->angles.x + 30);

	// Store the owner for later use. We want the owner to be able
	// to hit themselves with the snowball. The pev->owner gets cleared
	// later to avoid hitting the player as they throw the snowball.
	if ( pev->owner )
		m_hOwner = Instance( pev->owner );
	m_hTrackTarget = NULL;
	m_fTrackTargetResolved = FALSE;
	m_iTrackTurnSign = 0;

	// Set the think funtion.
	SetThink( &CFlyingSnowball::BubbleThink );
	pev->nextthink = gpGlobals->time + SNOWBALL_TRACK_THINK_INTERVAL;

	// Set the touch function.
	SetTouch( &CFlyingSnowball::SpinTouch );
}

void CFlyingSnowball::Precache( )
{
	PRECACHE_MODEL ("models/w_weapons.mdl");

	PRECACHE_SOUND ("snowball_miss.wav");
	PRECACHE_SOUND ("snowball_hitbod.wav");
	PRECACHE_SOUND ("snowball_throw.wav");
}

void CFlyingSnowball::SpinTouch( CBaseEntity *pOther )
{
	// Guard against multiple touch calls queued in the same engine frame.
	// Set SOLID_NOT immediately — before any processing — so that any further
	// touch events already queued by the engine (e.g. hitting two bots in the
	// same physics step) hit this guard and return without double-processing.
	// On Linux the GoldSrc engine may dispatch queued touches without
	// re-checking the touch function pointer, making this the only reliable
	// re-entrancy barrier.
	if (pev->solid == SOLID_NOT)
		return;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;

	SetTouch( NULL );

	// Cache whether we hit a player BEFORE calling ApplyMultiDamage.
	// The kill chain triggered by ApplyMultiDamage (PlayerKilled -> FireTargets
	// -> entity activation) may alter entity state in ways that make a virtual
	// call on pOther unsafe after the damage is applied, particularly on Linux
	// where the heap can reuse a freed edict slot within the same frame.
	const BOOL bHitPlayer = pOther->IsPlayer();

	// We touched something in the game. Look to see if the object
	// is allowed to take damage.
	if (pOther->pev->takedamage)
	{
		// Get the traceline info to the target.
		TraceResult tr = UTIL_GetGlobalTrace( );

		// Apply damage to the target. If we have an owner stored, use that one,
		// otherwise count it as self-inflicted.
		ClearMultiDamage( );
		pOther->TraceAttack(pev, gSkillData.plrDmgSnowball, pev->velocity.Normalize(), &tr,
								  DMG_NEVERGIB );
		if (m_hOwner != NULL)
			ApplyMultiDamage( pev, m_hOwner->pev );
		else
			ApplyMultiDamage( pev, pev );
	}

	// If we hit a player, make a nice squishy thunk sound. Otherwise
	// make a clang noise and throw a bunch of sparks.
	if (bHitPlayer) {
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "snowball_hitbod.wav",
							1.0, ATTN_NORM, 0, 100);
	}
	else
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "snowball_miss.wav",
							1.0, ATTN_NORM, 0, 100);
	}

	// Get the unit vector in the direction of motion.
	Vector vecDir = pev->velocity.Normalize( );

	// Trace a line along the velocity vector to get the normal at impact.
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + vecDir * 100,
						dont_ignore_monsters, ENT(pev), &tr);

	if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
	{
		Vector pullOut = tr.vecEndPos + (tr.vecPlaneNormal * 10);
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_SPRITE );
			WRITE_COORD( pullOut.x );
			WRITE_COORD( pullOut.y );
			WRITE_COORD( pullOut.z );
			WRITE_SHORT( g_sModelIndexSnowballHit );
			WRITE_BYTE( 15 ); // scale * 10
			WRITE_BYTE( 128 ); // framerate
		MESSAGE_END();
	}

	DecalGunshot( &tr, BULLET_PLAYER_SNOWBALL );

	if (g_pGameRules->MutatorEnabled(MUTATOR_CHUMXPLODE))
	{
		edict_t *owner = NULL;
		if (m_hOwner)
			owner = m_hOwner->edict();
		CBaseEntity *pChumtoad = CBaseEntity::Create("monster_chumtoad", pev->origin, pev->angles, owner);
		if (pChumtoad)
		{
			pChumtoad->pev->velocity.x = RANDOM_FLOAT( -400, 400 );
			pChumtoad->pev->velocity.y = RANDOM_FLOAT( -400, 400 );
			pChumtoad->pev->velocity.z = RANDOM_FLOAT( 0, 400 );
		}
	}

	// Remove this snowball from the world.
	SetThink ( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + .1;
}

void CFlyingSnowball::AcquireTrackTarget( void )
{
	m_fTrackTargetResolved = TRUE;
	m_hTrackTarget = NULL;
	m_iTrackTurnSign = 0;

	Vector vecVelocity2D( pev->velocity.x, pev->velocity.y, 0 );
	float flSpeed2D = vecVelocity2D.Length2D();
	if ( flSpeed2D < 1.0f )
		return;

	Vector vecForward2D = vecVelocity2D / flSpeed2D;
	Vector vecAimForward2D = vecForward2D;
	if ( m_hOwner && m_hOwner->IsPlayer() )
	{
		UTIL_MakeAimVectors( m_hOwner->pev->v_angle );
		Vector vecOwnerAim2D( gpGlobals->v_forward.x, gpGlobals->v_forward.y, 0 );
		float flOwnerAimLen = vecOwnerAim2D.Length2D();
		if ( flOwnerAimLen > 0.0001f )
			vecAimForward2D = vecOwnerAim2D / flOwnerAimLen;
	}

	float flBestForwardDist = SNOWBALL_TRACK_MAX_DIST;
	CBaseEntity *pBestTarget = NULL;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsPlayer() || !pPlayer->IsAlive() )
			continue;

		if ( m_hOwner && pPlayer->edict() == m_hOwner->edict() )
			continue;

		Vector vecTargetPos = pPlayer->pev->origin + pPlayer->pev->view_ofs;
		Vector vecToTarget = vecTargetPos - pev->origin;
		float flForwardDist = DotProduct( vecToTarget, vecAimForward2D );
		if ( flForwardDist <= 0.0f || flForwardDist >= flBestForwardDist )
			continue;

		float flToTarget = vecToTarget.Length();
		if ( flToTarget < 1.0f )
			continue;

		float flDot = flForwardDist / flToTarget;
		if ( flDot < SNOWBALL_TRACK_MIN_DOT )
			continue;

		float flAimDot = DotProduct( vecToTarget / flToTarget, vecAimForward2D );
		if ( flAimDot < SNOWBALL_TRACK_AIM_DOT )
			continue;

		if ( !SnowballHasWorldLineOfSight( pev->origin, vecTargetPos, ENT(pev) ) )
			continue;

		flBestForwardDist = flForwardDist;
		pBestTarget = pPlayer;
	}

	if ( pBestTarget )
		m_hTrackTarget = pBestTarget;
}

void CFlyingSnowball::ApplyTrackNudge( void )
{
	if ( !m_hTrackTarget )
		return;

	CBaseEntity *pTarget = m_hTrackTarget;
	if ( !pTarget || !pTarget->IsAlive() )
		return;

	Vector vecVelocity2D( pev->velocity.x, pev->velocity.y, 0 );
	float flSpeed2D = vecVelocity2D.Length2D();
	if ( flSpeed2D < 1.0f )
		return;

	Vector vecTargetPos = pTarget->pev->origin + pTarget->pev->view_ofs;
	if ( !SnowballHasWorldLineOfSight( pev->origin, vecTargetPos, ENT(pev) ) )
	{
		// Stop assisting once geometry blocks the path and continue as a normal toss.
		m_hTrackTarget = NULL;
		return;
	}

	Vector vecToTarget2D = vecTargetPos - pev->origin;
	vecToTarget2D.z = 0;
	float flToTarget2D = vecToTarget2D.Length2D();
	if ( flToTarget2D < 1.0f )
		return;

	Vector vecCurrentDir2D = vecVelocity2D / flSpeed2D;
	Vector vecTargetDir2D = vecToTarget2D / flToTarget2D;
	float flDot = DotProduct( vecCurrentDir2D, vecTargetDir2D );
	if ( flDot < 0.2f )
		return;

	float flCrossZ = (vecCurrentDir2D.x * vecTargetDir2D.y) - (vecCurrentDir2D.y * vecTargetDir2D.x);
	if ( flCrossZ > -0.0001f && flCrossZ < 0.0001f )
		return;

	if ( m_iTrackTurnSign == 0 )
		m_iTrackTurnSign = (flCrossZ > 0.0f) ? 1 : -1;
	else if ( (flCrossZ > 0.0f && m_iTrackTurnSign < 0) || (flCrossZ < 0.0f && m_iTrackTurnSign > 0) )
		return;

	float flNudge = SNOWBALL_TRACK_NUDGE;
	float flAbsCrossZ = (flCrossZ >= 0.0f) ? flCrossZ : -flCrossZ;
	if ( flAbsCrossZ < flNudge )
		flNudge = flAbsCrossZ;

	Vector vecNewDir2D = (vecCurrentDir2D * (1.0f - flNudge) + vecTargetDir2D * flNudge).Normalize();

	pev->velocity.x = vecNewDir2D.x * flSpeed2D;
	pev->velocity.y = vecNewDir2D.y * flSpeed2D;
}

void CFlyingSnowball::BubbleThink( void )
{
	// Expire after maximum lifetime to prevent entity exhaustion.
	if (pev->dmgtime && gpGlobals->time > pev->dmgtime)
	{
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time + 0.1;
		return;
	}

	if ( !m_fTrackTargetResolved )
		AcquireTrackTarget();

	ApplyTrackNudge();

	// Think more frequently for smoother subtle steering.
	pev->nextthink = gpGlobals->time + SNOWBALL_TRACK_THINK_INTERVAL;

	// If the snowball enters water, make some bubbles.
	if (pev->waterlevel)
		UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 1 );
}
