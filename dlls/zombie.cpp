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
// Zombie
//=========================================================

// UNDONE: Don't flinch every time you get hit

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"game.h"


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	ZOMBIE_AE_ATTACK_RIGHT		0x01
#define	ZOMBIE_AE_ATTACK_LEFT		0x02
#define	ZOMBIE_AE_ATTACK_BOTH		0x03

#define ZOMBIE_FLINCH_DELAY			2		// at most one flinch every n secs

//=========================================================
// Custom horde schedule / task IDs
//=========================================================
enum
{
	SCHED_ZOMBIE_DIRECT_APPROACH = LAST_COMMON_SCHEDULE + 1,
};

enum
{
	TASK_ZOMBIE_DIRECT_APPROACH = LAST_COMMON_TASK + 1,
};

class CZombie : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int IgnoreConditions ( void );

	float m_flNextFlinch;

	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );

	static const char *pAttackSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckMeleeAttack1 ( float flDot, float flDist );
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType( int Type );
	void StartTask( Task_t *pTask );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	CUSTOM_SCHEDULES;
};

LINK_ENTITY_TO_CLASS( monster_zombie, CZombie );

//=========================================================
// Direct-approach schedule: bypasses nav graph so the zombie
// walks off crates/ledges using MOVETYPE_STEP physics.
//=========================================================
Task_t tlZombieDirectApproach[] =
{
	{ TASK_STOP_MOVING,				(float)0 },
	{ TASK_ZOMBIE_DIRECT_APPROACH,	(float)0 },
	{ TASK_RUN_PATH,				(float)0 },
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0 },
};

Schedule_t slZombieDirectApproach[] =
{
	{
		tlZombieDirectApproach,
		ARRAYSIZE( tlZombieDirectApproach ),
		bits_COND_NEW_ENEMY			|
		bits_COND_ENEMY_DEAD		|
		bits_COND_CAN_MELEE_ATTACK1	|
		bits_COND_TASK_FAILED,
		0,
		"ZombieDirectApproach"
	},
};

DEFINE_CUSTOM_SCHEDULES( CZombie )
{
	slZombieDirectApproach,
};
IMPLEMENT_CUSTOM_SCHEDULES( CZombie, CBaseMonster );

const char *CZombie::pAttackHitSounds[] = 
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CZombie::pAttackMissSounds[] = 
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

const char *CZombie::pAttackSounds[] = 
{
	"zombie/zo_attack1.wav",
	"zombie/zo_attack2.wav",
};

const char *CZombie::pIdleSounds[] = 
{
	"zombie/zo_idle1.wav",
	"zombie/zo_idle2.wav",
	"zombie/zo_idle3.wav",
	"zombie/zo_idle4.wav",
};

const char *CZombie::pAlertSounds[] = 
{
	"zombie/zo_alert10.wav",
	"zombie/zo_alert20.wav",
	"zombie/zo_alert30.wav",
};

const char *CZombie::pPainSounds[] = 
{
	"zombie/zo_pain1.wav",
	"zombie/zo_pain2.wav",
};

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CZombie :: Classify ( void )
{
	return	CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CZombie :: SetYawSpeed ( void )
{
	int ys;

	ys = 120;

#if 0
	switch ( m_Activity )
	{
	}
#endif

	pev->yaw_speed = ys;
}

int CZombie :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// Take 30% damage from bullets
	if ( bitsDamageType == DMG_BULLET )
	{
		Vector vecDir = pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();
		float flForce = DamageForce( flDamage );
		pev->velocity = pev->velocity + vecDir * flForce;
		flDamage *= 0.3;
	}

	// HACK HACK -- until we fix this.
	if ( IsAlive() )
		PainSound();

	// For horde
	if (g_pGameRules->IsHorde())
	{
		flDamage *= 0.25;

		// Force the attacker as our enemy immediately, even if they are out of sight
		// (e.g. player hitting the zombie from below a ledge). This ensures the zombie
		// turns and walks toward the attacker to engage.
		if ( IsAlive() && pevAttacker && (pevAttacker->flags & (FL_MONSTER | FL_CLIENT)) )
		{
			CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );
			if ( pAttacker && pAttacker != this && m_hEnemy != pAttacker )
			{
				if ( m_hEnemy != NULL )
					PushEnemy( m_hEnemy, m_vecEnemyLKP );
				m_hEnemy = pAttacker;
				m_vecEnemyLKP = pevAttacker->origin;
				SetConditions( bits_COND_NEW_ENEMY );
				m_IdealMonsterState = MONSTERSTATE_COMBAT;
			}
		}
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CZombie :: PainSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	if (RANDOM_LONG(0,5) < 2)
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pPainSounds[ RANDOM_LONG(0,ARRAYSIZE(pPainSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CZombie :: AlertSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAlertSounds[ RANDOM_LONG(0,ARRAYSIZE(pAlertSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CZombie :: IdleSound( void )
{
	int pitch = 100 + RANDOM_LONG(-5,5);

	// Play a random idle sound
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pIdleSounds[ RANDOM_LONG(0,ARRAYSIZE(pIdleSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CZombie :: AttackSound( void )
{
	int pitch = 100 + RANDOM_LONG(-5,5);

	// Play a random attack sound
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAttackSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CZombie :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case ZOMBIE_AE_ATTACK_RIGHT:
		{
			// do stuff for this event.
	//		ALERT( at_console, "Slash right!\n" );
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, gSkillData.zombieDmgOneSlash, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = -18;
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * 100;
				}
				// Play a random attack hit sound
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
			}
			else // Play a random attack miss sound
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case ZOMBIE_AE_ATTACK_LEFT:
		{
			// do stuff for this event.
	//		ALERT( at_console, "Slash left!\n" );
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, gSkillData.zombieDmgOneSlash, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = 18;
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * 100;
				}
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
			}
			else
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case ZOMBIE_AE_ATTACK_BOTH:
		{
			// do stuff for this event.
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, gSkillData.zombieDmgBothSlash, DMG_SLASH );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * -100;
				}
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
			}
			else
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CZombie :: Spawn()
{
	Precache( );

	pev->classname = MAKE_STRING("monster_zombie");

	SET_MODEL(ENT(pev), "models/zombie.mdl");
	UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	pev->health			= gSkillData.zombieHealth;
	pev->view_ofs		= VEC_VIEW;// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CZombie :: Precache()
{
	int i;

	PRECACHE_MODEL("models/zombie.mdl");

	for ( i = 0; i < ARRAYSIZE( pAttackHitSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackHitSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackMissSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackMissSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pIdleSounds ); i++ )
		PRECACHE_SOUND((char *)pIdleSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAlertSounds ); i++ )
		PRECACHE_SOUND((char *)pAlertSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pPainSounds ); i++ )
		PRECACHE_SOUND((char *)pPainSounds[i]);
}	

//=========================================================
// AI Schedules Specific to this monster
//=========================================================



//=========================================================
// CheckMeleeAttack1 - overridden for zombie so that in horde
// mode it attacks more aggressively: relaxed dot threshold and
// no requirement for the enemy to be on the ground.
//=========================================================
BOOL CZombie :: CheckMeleeAttack1 ( float flDot, float flDist )
{
	if ( flDist <= 64 )
	{
		if ( g_pGameRules->IsHorde() )
		{
			// In horde: attack at steep angles (ledge above/below) and don't
			// require the enemy to be standing on the ground.
			if ( flDot >= 0.5 && m_hEnemy != NULL )
				return TRUE;
		}
		else
		{
			if ( flDot >= 0.7 && m_hEnemy != NULL && FBitSet( m_hEnemy->pev->flags, FL_ONGROUND ) )
				return TRUE;
		}
	}
	return FALSE;
}

//=========================================================
// GetScheduleOfType - in horde, retry the chase instead of
// taking cover when pathfinding to enemy fails.
//=========================================================
Schedule_t *CZombie :: GetScheduleOfType( int Type )
{
	if ( g_pGameRules->IsHorde() && Type == SCHED_CHASE_ENEMY_FAILED )
	{
		// Only redirect when on the ground. If already airborne from a previous
		// launch, let the existing velocity carry the zombie rather than
		// re-applying another push (which causes floating).
		if ( m_hEnemy != NULL && FBitSet( pev->flags, FL_ONGROUND ) )
			return &slZombieDirectApproach[0];

		// No enemy left - retry the standard chase
		return GetScheduleOfType( SCHED_CHASE_ENEMY );
	}

	return CBaseMonster::GetScheduleOfType( Type );
}

//=========================================================
// StartTask - handle the zombie's custom direct-approach task.
//=========================================================
void CZombie :: StartTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_ZOMBIE_DIRECT_APPROACH:
		{
			if ( m_hEnemy == NULL )
			{
				TaskFail();
				return;
			}

			// Try standard routing first.
			if ( BuildRoute( m_hEnemy->pev->origin, bits_MF_TO_ENEMY, m_hEnemy ) ||
				 BuildNearestRoute( m_hEnemy->pev->origin, m_hEnemy->pev->view_ofs, 0,
									(m_hEnemy->pev->origin - pev->origin).Length() ) )
			{
				TaskComplete();
				return;
			}

			// Already airborne from a previous launch — don't push again.
			// Let gravity and the existing velocity carry the zombie down.
			if ( !FBitSet( pev->flags, FL_ONGROUND ) )
			{
				RouteNew();
				m_movementGoal = MOVEGOAL_NONE;
				TaskComplete();
				return;
			}

			// All nav routing failed (zombie on isolated geometry like a crate with
			// no nav nodes). Launch the zombie physically toward the enemy, using
			// the same gravity-based ballistic technique as the panthereye leap.
			Vector vecToEnemy = m_hEnemy->pev->origin - pev->origin;
			float flHeight    = vecToEnemy.z;

			float gravity = g_psv_gravity->value;
			if ( gravity <= 1 ) gravity = 1;

			Vector vecLaunch;
			if ( flHeight < -16 )
			{
				// Enemy is below us — step off the ledge with horizontal momentum.
				// Zero out Z so gravity pulls the zombie down naturally.
				Vector vec2D = vecToEnemy;
				vec2D.z = 0;
				float len = vec2D.Length();
				if ( len > 0 ) vec2D = vec2D * (1.0f / len);
				vecLaunch   = vec2D * 200.0f;
				vecLaunch.z = 0;
			}
			else
			{
				// Enemy at same level or above — arc up to reach them (same formula
				// as panthereye's DIABLO_AE_JUMPATTACK).
				if ( flHeight < 16 ) flHeight = 16;
				float speed = sqrt( 2.0f * gravity * flHeight );
				float time  = speed / gravity;
				vecLaunch   = vecToEnemy * ( 1.0f / time );
				vecLaunch.z = speed * 0.9f;
				float dist  = vecLaunch.Length();
				if ( dist > 650 ) vecLaunch = vecLaunch * ( 650.0f / dist );
			}

			// Lift off and apply velocity — same technique as panthereye's jump.
			// Clearing FL_ONGROUND lets the engine treat the zombie as airborne
			// so the velocity is actually applied (MOVETYPE_STEP ignores velocity
			// on grounded entities).
			ClearBits( pev->flags, FL_ONGROUND );
			UTIL_SetOrigin( pev, pev->origin + Vector( 0, 0, 1 ) );
			pev->velocity = vecLaunch;

			// Clear the route so TASK_WAIT_FOR_MOVEMENT finishes immediately.
			RouteNew();
			m_movementGoal = MOVEGOAL_NONE;
			TaskComplete();
		}
		break;

	default:
		CBaseMonster::StartTask( pTask );
		break;
	}
}

//=========================================================
// GetSchedule - horde override for aggressive combat behavior.
//=========================================================
Schedule_t *CZombie :: GetSchedule( void )
{
	if ( m_MonsterState == MONSTERSTATE_COMBAT && g_pGameRules->IsHorde() )
	{
		if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			return CBaseMonster::GetSchedule();

		// In horde: melee if possible, otherwise chase off the ledge.
		if ( HasConditions( bits_COND_CAN_MELEE_ATTACK1 ) )
			return GetScheduleOfType( SCHED_MELEE_ATTACK1 );

		return GetScheduleOfType( SCHED_CHASE_ENEMY );
	}

	return CBaseMonster::GetSchedule();
}

int CZombie::IgnoreConditions ( void )
{
	int iIgnore = CBaseMonster::IgnoreConditions();

	if ((m_Activity == ACT_MELEE_ATTACK1) || (m_Activity == ACT_MELEE_ATTACK1))
	{
#if 0
		if (pev->health < 20)
			iIgnore |= (bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE);
		else
#endif			
		if (m_flNextFlinch >= gpGlobals->time)
			iIgnore |= (bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE);
	}

	if ((m_Activity == ACT_SMALL_FLINCH) || (m_Activity == ACT_BIG_FLINCH))
	{
		if (m_flNextFlinch < gpGlobals->time)
			m_flNextFlinch = gpGlobals->time + ZOMBIE_FLINCH_DELAY;
	}

	return iIgnore;
	
}