/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// rat - environmental monster
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"soundent.h"
#include	"weapons.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================

class CRat : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void EXPORT MonsterThink( void );
	void Move( float flInterval );
	void PickNewDest( void );
	void EXPORT Touch( CBaseEntity *pOther );
	void Killed( entvars_t *pevAttacker, int iGib );
	
	float m_flLastLightLevel;
	CBaseEntity *m_pGoalEnt;
	float m_flNextJumpTime;
	float m_flNextBiteTime;
	float m_flNextExplodeCheck;
	void Explode( void );
};
LINK_ENTITY_TO_CLASS( monster_rat, CRat );

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CRat :: Classify ( void )
{
	return	CLASS_INSECT;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CRat :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
	default:
		ys = 45;
		break;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// Spawn
//=========================================================
void CRat :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/bigrat.mdl");
	UTIL_SetSize( pev, Vector( -4, -4, 0 ), Vector( 4, 4, 4 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->health			= 8;
	pev->view_ofs		= Vector ( 0, 0, 6 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	MonsterInit();
	SetActivity( ACT_IDLE );
	
	pev->takedamage		= DAMAGE_YES;
	m_flLastLightLevel	= -1;
	m_pGoalEnt			= NULL;
	m_flNextJumpTime	= 0;
	m_flNextBiteTime	= 0;
	m_flNextExplodeCheck= 0;

	if (g_pGameRules->IsMultiplayer())
	{
		SetThink( &CRat::MonsterThink );
		pev->nextthink = gpGlobals->time + 0.1f;
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CRat :: Precache()
{
	PRECACHE_MODEL("models/bigrat.mdl");
	PRECACHE_SOUND("bullchicken/bc_bite2.wav");
}

//=========================================================
// Touch
//=========================================================
void CRat :: Touch( CBaseEntity *pOther )
{
	// If touched by player, bite them
	if ( pOther->IsPlayer() && pOther->IsAlive() )
	{
		// Only deal damage if cooldown has expired
		if ( gpGlobals->time >= m_flNextBiteTime )
		{
			// Apply bite damage
			pOther->TakeDamage( pev, pev, RANDOM_FLOAT(3, 7), DMG_SLASH );
			
			// Play bite sound
			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "bullchicken/bc_bite2.wav", 1, ATTN_NORM, 0, 100 );
			
			// Set next bite time (0.5 second cooldown)
			m_flNextBiteTime = gpGlobals->time + 0.5f;
		}
		return;
	}
	
	// If stepped on by player, die
	if ( pOther->IsPlayer() && pOther->pev->velocity.z < -50 )
	{
		TakeDamage( pOther->pev, pOther->pev, pev->health, DMG_CRUSH );
	}
}

//=========================================================
// Killed
//=========================================================
void CRat :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->solid = SOLID_NOT;
	
	CBaseEntity *pOwner = CBaseEntity::Instance(pev->owner);
	if ( pOwner )
	{
		pOwner->DeathNotice( pev );
	}
	UTIL_Remove( this );
}

//=========================================================
// MonsterThink - main AI loop
//=========================================================
void CRat :: MonsterThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1f;
	
	float flInterval = StudioFrameAdvance();
	
	// Find nearest player
	CBaseEntity *pPlayer = NULL;
	float flMinDist = 8192;
	CBaseEntity *pNearest = NULL;
	
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->IsAlive() )
		{
			float flDist = (pPlayer->pev->origin - pev->origin).Length();
			if ( flDist < flMinDist )
			{
				flMinDist = flDist;
				pNearest = pPlayer;
			}
		}
	}
	
	// Chase the nearest player
	if ( pNearest && flMinDist < 512 )
	{
		m_pGoalEnt = pNearest;
		
		// Random chance to explode when very close to player
		if ( flMinDist < 80 && gpGlobals->time >= m_flNextExplodeCheck )
		{
			m_flNextExplodeCheck = gpGlobals->time + 0.5f;
			// 10% chance to explode
			if ( RANDOM_LONG(0, 9) == 0 )
			{
				Explode();
				return;
			}
		}
		
		// Jump attack if close enough and jump is ready
		if ( flMinDist < 96 && flMinDist > 32 && gpGlobals->time >= m_flNextJumpTime && (pev->flags & FL_ONGROUND) )
		{
			// Face the player
			Vector vecDir = (m_pGoalEnt->pev->origin - pev->origin).Normalize();
			pev->angles.y = UTIL_VecToYaw( vecDir );
			
			// Jump at player
			UTIL_MakeVectors( pev->angles );
			pev->velocity = gpGlobals->v_forward * 200;
			pev->velocity.z = 180;
			
			// Set next jump time
			m_flNextJumpTime = gpGlobals->time + RANDOM_FLOAT(1.5f, 3.0f);
			
			// Set to attack activity
			SetActivity( ACT_RANGE_ATTACK1 );
		}
		else if ( pev->flags & FL_ONGROUND )
		{
			// Only set running velocity when on ground
			SetActivity( ACT_RUN );
			
			// Set ideal yaw to face player
			Vector vecDir = (m_pGoalEnt->pev->origin - pev->origin).Normalize();
			pev->ideal_yaw = UTIL_VecToYaw( vecDir );
			ChangeYaw( pev->yaw_speed );
			
			// Move forward
			UTIL_MakeVectors( pev->angles );
			pev->velocity = gpGlobals->v_forward * 120;
		}
	}
	else if ( RANDOM_LONG(0, 99) == 1 )
	{
		// Random movement when no player nearby
		PickNewDest();
		SetActivity( ACT_WALK );
	}
	else if ( pev->velocity.Length() < 1 )
	{
		SetActivity( ACT_IDLE );
		pev->velocity = Vector(0, 0, 0);
	}
}

//=========================================================
// Move - move rat in current direction
//=========================================================
void CRat :: Move( float flInterval )
{
	// Movement is handled by velocity in MonsterThink
	// This function is kept for compatibility but does nothing
}

//=========================================================
// PickNewDest - pick a random destination
//=========================================================
void CRat :: PickNewDest( void )
{
	Vector vecNewDir;
	Vector vecDest;
	
	vecNewDir.x = RANDOM_FLOAT( -1, 1 );
	vecNewDir.y = RANDOM_FLOAT( -1, 1 );
	vecNewDir.z = 0;
	
	vecDest = pev->origin + vecNewDir * RANDOM_FLOAT( 128, 256 );
	
	// Set ideal yaw
	pev->ideal_yaw = UTIL_VecToYaw( vecNewDir );
	ChangeYaw( pev->yaw_speed );
	
	// Set velocity
	UTIL_MakeVectors( pev->angles );
	pev->velocity = gpGlobals->v_forward * 80;
	
	m_flLastLightLevel = GETENTITYILLUM( ENT( pev ) );
}

//=========================================================
// Explode - rat explodes dealing damage to nearby entities
//=========================================================
void CRat :: Explode( void )
{
	// Create explosion effect
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( g_sModelIndexFireball );
		WRITE_BYTE( 10 ); // scale
		WRITE_BYTE( 15 ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();
	
	// Play explosion sound
	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/explode3.wav", 1, ATTN_NORM );
	
	// Deal radius damage
	::RadiusDamage( pev->origin, pev, pev, 40, 100, CLASS_NONE, DMG_BLAST );
	
	// Remove the rat
	SetThink( &CRat::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
