/***
*
*	Copyright (c) 2024, Magic Nipples.
*
*	Use and modification of this code is allowed as long
*	as credit is provided! Enjoy!
*
****/
//=========================================================
// Panthereye - stalking ambush style predator
//
// HORDE MODE BEHAVIOR:
// In multiplayer (horde) mode, panthereyes are significantly more
// aggressive, especially when engaging enemies below them:
// 
// - Radius-based detection: Bypasses line-of-sight for downward targets
// - Ignores facing angle: Can leap at enemies below regardless of direction
// - Forces attack conditions: Overrides base class visibility requirements
// - Extended drop distance: Can drop up to 1024 units to pursue prey
// - Immediate aggression: Becomes "pissed" on first detection
//
// This allows panthereyes on elevated platforms to leap down at
// players below, creating dramatic ambush encounters.
//=========================================================

#include	"extdll.h"
#include	"plane.h"
#include	"util.h"
#include	"cbase.h"
#include	"weapons.h"
#include	"soundent.h"
#include	"monsters.h"
#include	"animation.h"
#include	"game.h"
#include	"decals.h"



//=========================================================
// monster-specific conditions
//=========================================================
#define bits_COND_DIABLO_TOOFAR	( bits_COND_SPECIAL1 )

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		DIABLO_AE_MELEEATTACK	( 1 )
#define		DIABLO_AE_JUMPATTACK	( 2 )

//=========================================================
// Monster-specific Schedule Types
//=========================================================
enum
{
	SCHED_DIABLO_UNCROUCH = LAST_COMMON_SCHEDULE + 1,
	SCHED_DIABLO_CROUCH,
	SCHED_DIABLO_STALK,
	SCHED_DIABLO_CHASE_FAILED,
	SCHED_DIABLO_EMERG_JUMP,
};

//=========================================================
// Monster-Specific tasks
//=========================================================
enum
{
	TASK_DIABLO_RESETATTACK = LAST_COMMON_TASK + 2,
	TASK_DIABLO_GETPATH,
	TASK_DIABLO_CHECKJUMP,
	TASK_DIABLO_CRAWL_PATH
};

//=========================================================
// Let's define the class!
//=========================================================
class CDiablo : public CBaseMonster
{
public:
	void Spawn(void);
	void Precache(void);
	int Classify(void);
	int IRelationship ( CBaseEntity *pTarget );
	void SetYawSpeed(void);

	float GetEnemyHeight(void);

	void HandleAnimEvent(MonsterEvent_t* pEvent);
	int TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	BOOL FInViewCone ( CBaseEntity *pEntity );
	BOOL FInViewCone ( Vector *pOrigin );
	int CheckLocalMove ( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist );
	void Look ( int iDistance );
	void CheckAttacks ( CBaseEntity *pTarget, float flDist );
	BOOL CheckRangeAttack1(float flDot, float flDist);
	BOOL CheckMeleeAttack1(float flDot, float flDist);

	Schedule_t* GetScheduleOfType(int Type);
	Schedule_t* GetSchedule(void);
	void StartTask(Task_t* pTask);
	void RunTask(Task_t* pTask);

	void EXPORT LeapTouch(CBaseEntity* pOther);
	BOOL FallBack(const Vector& vecThreat, const Vector& vecViewOffset);

	void RunAI(void);

	CUSTOM_SCHEDULES;

	void PainSound(void);
	void AlertSound(void);
	void IdleSound(void);
	void AttackSound(void);

	static const char* pIdleSounds[];
	static const char* pAlertSounds[];
	static const char* pPainSounds[];
	static const char* pAttackSounds[];
	static const char* pDeathSounds[];
	static const char* pAttackHitSounds[];

private:
	BOOL	m_fStanding;
	BOOL	m_fPissed;

	// Helper methods for horde mode behavior
	BOOL IsEnemyBelowUs( CBaseEntity *pEntity, float threshold = 32.0f ) const;
	float Calculate2DDistance( const Vector &from, const Vector &to ) const;
	BOOL IsInGodMode( CBaseEntity *pEntity ) const;
	void ForceDownwardAttackConditions( CBaseEntity *pTarget );
};


//=========================================================

Task_t tlDiabloStalk[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_CHASE_ENEMY_FAILED	},
	{ TASK_GET_PATH_TO_ENEMY,	(float)0		},
	{ TASK_DIABLO_CRAWL_PATH,	(float)0		},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0		},
};

Schedule_t slDiabloStalk[] =
{
	{
		tlDiabloStalk,
		ARRAYSIZE(tlDiabloStalk),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_ENEMY_FACING_ME |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_TASK_FAILED |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1,
		0,
		"Stalking"
	},
};

//=========================================================

Task_t tlDiabloLeap[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
};

Schedule_t slDiabloLeap[] =
{
	{
		tlDiabloLeap,
		ARRAYSIZE(tlDiabloLeap),
		bits_COND_ENEMY_DEAD |
		bits_COND_ENEMY_OCCLUDED |
		bits_COND_CAN_MELEE_ATTACK1,
		0,
		"Leap Attack"
	},
};

//=========================================================

Task_t tlDiabloChase[] =
{
	{ TASK_SET_FAIL_SCHEDULE,	(float)SCHED_DIABLO_CHASE_FAILED	},
	{ TASK_GET_PATH_TO_ENEMY,	(float)0		},
	{ TASK_RUN_PATH,			(float)0		},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0		},
};

Schedule_t slDiabloChase[] =
{
	{
		tlDiabloChase,
		ARRAYSIZE(tlDiabloChase),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_LIGHT_DAMAGE |
		bits_COND_HEAVY_DAMAGE |
		bits_COND_TASK_FAILED |
		bits_COND_CAN_RANGE_ATTACK1 |
		bits_COND_CAN_MELEE_ATTACK1,
		0,
		"Chasing"
	},
};

//=========================================================

Task_t tlDiabloChaseFailed[] =
{
	{ TASK_SET_FAIL_SCHEDULE,			(float)SCHED_DIABLO_EMERG_JUMP	},
	{ TASK_STOP_MOVING,					(float)0		},
	{ TASK_FACE_ENEMY,					(float)0		},
	{ TASK_DIABLO_GETPATH,				(float)0		},
	{ TASK_RUN_PATH,					(float)0		},
	{ TASK_WAIT_FOR_MOVEMENT,			(float)0		},
	{ TASK_STOP_MOVING,					(float)0		},
	{ TASK_FACE_ENEMY,					(float)0		},
	{ TASK_DIABLO_RESETATTACK,			(float)0		},
};

Schedule_t slDiabloChaseFailed[] =
{
	{
		tlDiabloChaseFailed,
		ARRAYSIZE(tlDiabloChaseFailed),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_TASK_FAILED |
		bits_COND_CAN_MELEE_ATTACK1,
		0,
		"Get Down"
	},
};

//=========================================================

Task_t tlDiabloEmergencyJump[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_DIABLO_CHECKJUMP,	(float)0		},
	{ TASK_FACE_ENEMY,			(float)0		},
	{ TASK_RANGE_ATTACK1_NOTURN,		(float)0		},
};

Schedule_t slDiabloEmergencyJump[] =
{
	{
		tlDiabloEmergencyJump,
		ARRAYSIZE(tlDiabloEmergencyJump),
		bits_COND_NEW_ENEMY |
		bits_COND_ENEMY_DEAD |
		bits_COND_CAN_MELEE_ATTACK1,
		0,
		"Get Down"
	},
};

//=========================================================

DEFINE_CUSTOM_SCHEDULES(CDiablo)
{
	slDiabloStalk,
	slDiabloLeap,
	slDiabloChase,
	slDiabloEmergencyJump,
};
IMPLEMENT_CUSTOM_SCHEDULES(CDiablo, CBaseMonster);


LINK_ENTITY_TO_CLASS(monster_diablo, CDiablo);
LINK_ENTITY_TO_CLASS(monster_panther, CDiablo); //alpha maps


const char* CDiablo::pIdleSounds[] =
{
	"panthereye/pa_idle1.wav",
	"panthereye/pa_idle2.wav",
	"panthereye/pa_idle3.wav",
	"panthereye/pa_idle4.wav",
};
const char* CDiablo::pAlertSounds[] =
{
	"panthereye/pa_alert1.wav",
	"panthereye/pa_alert2.wav",
};
const char* CDiablo::pPainSounds[] =
{
	"panthereye/pa_pain1.wav",
	"panthereye/pa_pain2.wav",
};
const char* CDiablo::pAttackSounds[] =
{
	"panthereye/pa_pain1.wav",
	"panthereye/pa_pain2.wav",
	"panthereye/pa_alert2.wav",
};

/*const char* CDiablo::pDeathSounds[] =
{
	"panthereye/pa_alert1.wav",
	"panthereye/pa_pain2.wav",
};*/

const char* CDiablo::pAttackHitSounds[] =
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};


//=========================================================
// Spawn
//=========================================================
void CDiablo::Spawn()
{
	Precache();

	pev->classname = MAKE_STRING("monster_panther");

	SET_MODEL(ENT(pev), "models/panthereye.mdl");

	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->health = gSkillData.pantherHealth;
	pev->view_ofs = Vector(0, 0, 24);	// position of the eyes relative to monster's origin.

	// For horde
	if (g_pGameRules->IsMultiplayer())
	{
		pev->framerate = 2.0;
		m_flFieldOfView = -0.707; // 270 degrees;
	}
	else
	{
		m_flFieldOfView = 0.5;	// indicates the width of this monster's forward view cone ( as a dotproduct result )
	}

	m_bloodColor = BLOOD_COLOR_YELLOW;
	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = bits_CAP_RANGE_ATTACK1 | bits_CAP_MELEE_ATTACK1;

	m_fStanding = FALSE;
	m_fPissed = FALSE;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CDiablo::Precache()
{
	PRECACHE_SOUND_ARRAY(pIdleSounds);
	PRECACHE_SOUND_ARRAY(pAlertSounds);
	PRECACHE_SOUND_ARRAY(pPainSounds);
	PRECACHE_SOUND_ARRAY(pAttackSounds);
	//PRECACHE_SOUND_ARRAY(pDeathSounds);
	PRECACHE_SOUND_ARRAY(pAttackHitSounds);

	PRECACHE_MODEL("models/panthereye.mdl");
	PRECACHE_MODEL("models/spit.mdl");
}

void CDiablo::PainSound(void)
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pPainSounds[RANDOM_LONG(0, ARRAYSIZE(pPainSounds) - 1)], 1.0, ATTN_NORM, 0, 100);
}

void CDiablo::AlertSound(void)
{
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pAlertSounds[RANDOM_LONG(0, ARRAYSIZE(pAlertSounds) - 1)], 1.0, ATTN_NORM, 0, 100);
}

void CDiablo::IdleSound(void)
{
	// Play a random idle sound
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pIdleSounds[RANDOM_LONG(0, ARRAYSIZE(pIdleSounds) - 1)], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));
}

void CDiablo::AttackSound(void)
{
	// Play a random attack sound
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pAttackSounds[RANDOM_LONG(0, ARRAYSIZE(pAttackSounds) - 1)], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CDiablo::Classify(void)
{
	return	CLASS_ALIEN_PREDATOR;
}

int CDiablo::IRelationship ( CBaseEntity *pTarget )
{
	if ( g_pGameRules->IsMultiplayer() && FClassnameIs( pTarget->pev, "monster_panther" ) )
	{
		return R_AL;
	}

	return CBaseMonster::IRelationship( pTarget );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CDiablo::SetYawSpeed(void)
{
	// For horde
	if (g_pGameRules->IsMultiplayer())
		pev->framerate = 2.0;

	int ys = 90;
	switch (m_Activity)
	{
	case ACT_IDLE:
		ys = 60;
		break;
	case ACT_WALK:
		ys = 60;
		break;
	case ACT_RUN:
		ys = 90;
		break;
	default:
		ys = 90;
		break;
	}
	pev->yaw_speed = ys;
}

float CDiablo::GetEnemyHeight(void)
{
	float m_hEnemyHeight;

	if (m_hEnemy != NULL)
	{
		float EnemyHeight = m_hEnemy->pev->origin.z;

		if (m_hEnemy->IsPlayer())
		{
			if (FBitSet(m_hEnemy->pev->flags, FL_DUCKING))
				m_hEnemyHeight = m_hEnemy->pev->origin.z - 18;
			else
				m_hEnemyHeight = m_hEnemy->pev->origin.z - 36;
		}

		//ALERT(at_console, "%f\n", (m_hEnemyHeight - pev->origin.z));
		return (m_hEnemyHeight - pev->origin.z);
	}
	return 0;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CDiablo::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	// For horde
	if (g_pGameRules->IsMultiplayer())
		pev->framerate = 2.0;

	switch (pEvent->event)
	{
	case DIABLO_AE_MELEEATTACK:
	{
		AttackSound();

		CBaseEntity* pHurt = CheckTraceHullAttack(128, gSkillData.zombieDmgBothSlash, DMG_SLASH);
		if (pHurt)
		{
			if (pHurt->pev->flags & (FL_MONSTER | FL_CLIENT))
			{
				UTIL_MakeVectors(pev->angles);
				pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * -125;

				Vector bloodvec = pev->origin;
				bloodvec.z += 5;

				UTIL_BloodStream(bloodvec, UTIL_RandomBloodVector(), pHurt->BloodColor(), RANDOM_LONG(100, 175));
				//UTIL_Blood(pev->origin, UTIL_RandomBloodVector(), pHurt->BloodColor(), RANDOM_LONG(25, 35)); //turn on when real beta.

				EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pAttackHitSounds[RANDOM_LONG(0, ARRAYSIZE(pAttackHitSounds) - 1)], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5, 5));
			}
		}
	}
	break;

	case DIABLO_AE_JUMPATTACK:
	{
		AttackSound();

		ClearBits(pev->flags, FL_ONGROUND);

		UTIL_SetOrigin(pev, pev->origin + Vector(0, 0, 1));// take him off ground so engine doesn't instantly reset onground 
		UTIL_MakeVectors(pev->angles);

		Vector vecJumpDir;
		float EnemyHeight = GetEnemyHeight();

		if ((m_hEnemy != NULL) && (fabs(EnemyHeight) > 1))
		{
			//ALERT(at_console, "enemy.z: %f diablo.z: %f\n", EnemyHeight, pev->origin.z);
			float gravity = g_psv_gravity->value;
			if (gravity <= 1)
				gravity = 1;

			// How fast does the panthereye need to travel to reach that height given gravity?
			float height = (m_hEnemy->pev->origin.z + m_hEnemy->pev->view_ofs.z - pev->origin.z);
			if (height < 16)
				height = 16;
			float speed = sqrt(2 * gravity * height);
			float time = speed / gravity;

			// Scale the sideways velocity to get there at the right time
			vecJumpDir = (m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs - pev->origin);
			vecJumpDir = vecJumpDir * (1.0 / time);

			// Speed to offset gravity at the desired height
			vecJumpDir.z = speed * 0.9;

			// Don't jump too far/fast
			float distance = vecJumpDir.Length();

			if (distance > 650)
			{
				vecJumpDir = vecJumpDir * (650.0 / distance);
			}
		}
		else
		{
			vecJumpDir = Vector(gpGlobals->v_forward.x, gpGlobals->v_forward.y, gpGlobals->v_up.z) * 450;
			vecJumpDir.z *= 0.6;
		}

		pev->velocity = vecJumpDir;
		m_flNextAttack = gpGlobals->time + RANDOM_LONG(2, 6);
	}
	break;

	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}

//=========================================================
// Helper: Check if entity is below us by threshold units
//=========================================================
BOOL CDiablo::IsEnemyBelowUs( CBaseEntity *pEntity, float threshold ) const
{
	if ( !pEntity )
		return FALSE;
	
	float heightDiff = pev->origin.z - pEntity->pev->origin.z;
	return ( heightDiff > threshold );
}

//=========================================================
// Helper: Calculate 2D horizontal distance (ignoring Z)
//=========================================================
float CDiablo::Calculate2DDistance( const Vector &from, const Vector &to ) const
{
	Vector vecDist2D = to - from;
	vecDist2D.z = 0;
	return vecDist2D.Length();
}

//=========================================================
// Helper: Check if entity has godmode enabled
//=========================================================
BOOL CDiablo::IsInGodMode( CBaseEntity *pEntity ) const
{
	if ( !pEntity )
		return FALSE;
	
	return FBitSet( pEntity->pev->flags, FL_GODMODE );
}

//=========================================================
// Helper: Force attack conditions for enemies below us
// Used in multiple places to reduce duplication
//=========================================================
void CDiablo::ForceDownwardAttackConditions( CBaseEntity *pTarget )
{
	if ( !pTarget || !g_pGameRules->IsMultiplayer() )
		return;
	
	// Force visibility conditions so base class AI doesn't clear them
	SetConditions( bits_COND_SEE_ENEMY | bits_COND_SEE_CLIENT );
	
	// Clear occlusion to prevent leap attack schedule interruption
	ClearConditions( bits_COND_ENEMY_OCCLUDED );
	
	// Update last known position for pathfinding
	m_vecEnemyLKP = pTarget->pev->origin;
	
	// Check if we can force a leap attack
	if ( FBitSet(pev->flags, FL_ONGROUND) && m_flNextAttack < gpGlobals->time )
	{
		float flDist2D = Calculate2DDistance( pev->origin, pTarget->pev->origin );
		
		// Force leap if within horizontal range (32-512 units)
		if ( flDist2D <= 512 && flDist2D > 32 )
		{
			SetConditions( bits_COND_CAN_RANGE_ATTACK1 );
		}
		else if ( flDist2D < 75 )
		{
			SetConditions( bits_COND_CAN_MELEE_ATTACK1 );
		}
	}
}

//=========================================================
// Needs to be provoked to start running
//=========================================================
int CDiablo::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (!m_fPissed)
		m_fPissed = TRUE;

	// Remember attacker - helps with detecting enemies below/above
	if ( pevAttacker )
	{
		CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );
		
		// Don't target godmode players or dead entities
		if ( pAttacker && pAttacker->IsAlive() && !IsInGodMode( pAttacker ) )
		{
			// Switch to the attacker as our primary target
			m_hEnemy = pAttacker;
			PushEnemy( m_hEnemy, pev->origin );
			m_vecEnemyLKP = pAttacker->pev->origin;
			
			// In horde mode, force immediate aggressive engagement
			if ( g_pGameRules->IsMultiplayer() )
			{
				SetConditions( bits_COND_NEW_ENEMY | bits_COND_SEE_ENEMY | bits_COND_SEE_CLIENT );
				m_IdealMonsterState = MONSTERSTATE_COMBAT;
				
				// Check if we can leap immediately when damaged
				if ( FBitSet(pev->flags, FL_ONGROUND) && m_flNextAttack < gpGlobals->time )
				{
					float flDist = (pAttacker->pev->origin - pev->origin).Length();
					float flDist2D = Calculate2DDistance( pev->origin, pAttacker->pev->origin );
					
					BOOL bShouldLeap = FALSE;
					
					// Enemy below us: aggressive leap (up to 1024 horizontal units)
					if ( IsEnemyBelowUs( pAttacker ) && flDist2D <= 1024 && flDist2D > 32 )
					{
						bShouldLeap = TRUE;
					}
					// Same height: moderate leap (up to 384 units)
					else if ( fabs(pev->origin.z - pAttacker->pev->origin.z) <= 64 && flDist <= 384 && flDist > 64 )
					{
						bShouldLeap = TRUE;
					}
					
					if ( bShouldLeap )
					{
						SetConditions( bits_COND_CAN_RANGE_ATTACK1 );
						ClearConditions( bits_COND_CAN_MELEE_ATTACK1 );
					}
				}
			}
			else
			{
				// Single player: force combat but less aggressive
				SetConditions( bits_COND_NEW_ENEMY );
				m_IdealMonsterState = MONSTERSTATE_COMBAT;
			}
		}
	}

	// For horde: reduce damage taken (but not as much as before for better balance)
	if (g_pGameRules->IsMultiplayer())
	{
		flDamage *= 0.5;
	}

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

//=========================================================
// Look - Enhanced for horde mode to detect players below
// using radius-based detection that bypasses visibility checks
//=========================================================
void CDiablo::Look ( int iDistance )
{
	// In horde mode, use aggressive radius-based detection for players below
	if ( g_pGameRules->IsMultiplayer() )
	{
		// Check for players below using simple radius check (bypasses visibility)
		CBaseEntity *pList[100];
		Vector delta = Vector( iDistance, iDistance, iDistance );
		
		int count = UTIL_EntitiesInBox( pList, 100, pev->origin - delta, pev->origin + delta, FL_CLIENT );
		BOOL bFoundEnemyBelow = FALSE;
		
		for ( int i = 0; i < count; i++ )
		{
			CBaseEntity *pPlayer = pList[i];
			
			// Skip invalid targets: self, dead, neutral/friendly, godmode
			if ( pPlayer && pPlayer != this && pPlayer->IsAlive() && 
				 IRelationship( pPlayer ) > R_NO && 
				 !IsInGodMode( pPlayer ) )
			{
				// Calculate distance and check if below us
				Vector vecDist = pPlayer->pev->origin - pev->origin;
				float flDist = vecDist.Length();
				
				// Radius check for targets below (no visibility or angle checks)
				// This bypasses platform edge occlusion issues
				if ( IsEnemyBelowUs( pPlayer ) && flDist <= iDistance )
				{
					// Only switch if we have no enemy or this one is closer
					if ( m_hEnemy == NULL || flDist < (m_hEnemy->pev->origin - pev->origin).Length() )
					{
						m_hEnemy = pPlayer;
						PushEnemy( m_hEnemy, pev->origin );
						SetConditions( bits_COND_SEE_ENEMY | bits_COND_NEW_ENEMY );
						m_IdealMonsterState = MONSTERSTATE_COMBAT;
						m_fPissed = TRUE; // Immediately aggressive in horde mode
						bFoundEnemyBelow = TRUE;
					}
				}
			}
		}
		
		// Always call base class Look to detect horizontal/above targets
		// This ensures we don't miss enemies at same level
		CBaseMonster::Look( iDistance );
	}
	else
	{
		// Single player: use normal behavior
		CBaseMonster::Look( iDistance );
	}
}

//=========================================================
// CheckAttacks - Override to force attack conditions for enemies below
// in horde mode, bypassing base class visibility requirements
//=========================================================
void CDiablo::CheckAttacks ( CBaseEntity *pTarget, float flDist )
{
	// In horde mode with targets below, ensure base class doesn't clear our conditions
	if ( g_pGameRules->IsMultiplayer() && m_fPissed && pTarget && IsEnemyBelowUs( pTarget ) )
	{
		// Pre-force conditions before base class check
		ForceDownwardAttackConditions( pTarget );
	}
	
	// Call base class to do the actual attack condition checking
	CBaseMonster::CheckAttacks( pTarget, flDist );
	
	// Re-force conditions after base class (in case it cleared them)
	if ( g_pGameRules->IsMultiplayer() && m_fPissed && pTarget && IsEnemyBelowUs( pTarget ) )
	{
		ForceDownwardAttackConditions( pTarget );
	}
}

//=========================================================
// CheckRangeAttack1 - Leap attack validation
//=========================================================
BOOL CDiablo::CheckRangeAttack1(float flDot, float flDist)
{
	if (m_flNextAttack < gpGlobals->time && FBitSet(pev->flags, FL_ONGROUND))
	{
		// In horde mode with targets below, use special rules
		if ( g_pGameRules->IsMultiplayer() && m_hEnemy != NULL && IsEnemyBelowUs( m_hEnemy ) )
		{
			// For downward targets: ignore facing angle (flDot), only check distance
			// Distance restrictions still apply: 32-512 units horizontally
			float flDist2D = Calculate2DDistance( pev->origin, m_hEnemy->pev->origin );
			
			if ( flDist2D <= 512 && flDist2D > 32 )
				return TRUE;
		}
		
		// Normal leap attack conditions: must be facing target (flDot >= 0.65)
		if (flDist <= 256 && flDist > 75 && flDot >= 0.65)
			return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckMeleeAttack1
//=========================================================
BOOL CDiablo::CheckMeleeAttack1(float flDot, float flDist)
{
	if (flDot > 0.5 && flDist < 75)
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckLocalMove - allows panthereye to be aggressive
// about dropping off ledges to pursue prey
//=========================================================
int CDiablo::CheckLocalMove ( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist )
{
	// Call base class to do the actual movement checking
	int iReturn = CBaseMonster::CheckLocalMove( vecStart, vecEnd, pTarget, pflDist );
	
	// In horde mode, be even more aggressive about elevation changes
	if ( g_pGameRules->IsMultiplayer() )
	{
		// Override the height restriction - panthereye can drop up to 1024 units in horde mode
		if ( iReturn == LOCALMOVE_INVALID_DONT_TRIANGULATE )
		{
			// Check if this was blocked due to height difference
			if ( !(pev->flags & (FL_FLY|FL_SWIM) ) && (!pTarget || (pTarget->pev->flags & FL_ONGROUND)) )
			{
				float heightDiff = fabs(vecEnd.z - vecStart.z);
				
				// If moving downward and height difference is less than 1024 units, allow the move
				// Panthereye is extremely brave in horde mode
				if ( heightDiff <= 1024 && vecEnd.z < vecStart.z )
				{
					iReturn = LOCALMOVE_VALID;
				}
			}
		}
	}
	else
	{
		// Single player: allow 256 unit drops
		if ( iReturn == LOCALMOVE_INVALID_DONT_TRIANGULATE )
		{
			if ( !(pev->flags & (FL_FLY|FL_SWIM) ) && (!pTarget || (pTarget->pev->flags & FL_ONGROUND)) )
			{
				float heightDiff = fabs(vecEnd.z - vecStart.z);
				
				if ( heightDiff <= 256 && vecEnd.z < vecStart.z )
				{
					iReturn = LOCALMOVE_VALID;
				}
			}
		}
	}
	
	return iReturn;
}

//=========================================================
// FInViewCone - 3D vision cone that can see above/below
// Overrides base class 2D-only implementation
//=========================================================
BOOL CDiablo::FInViewCone ( CBaseEntity *pEntity )
{
	Vector vecLOS;
	float flDot;

	UTIL_MakeVectors ( pev->angles );
	
	// Calculate 3D direction to entity
	vecLOS = ( pEntity->pev->origin - pev->origin ).Normalize();
	
	// Check vertical distance for special handling
	float heightDiff = pEntity->pev->origin.z - pev->origin.z;
	
	// For horde mode, use wider field of view
	if (g_pGameRules->IsMultiplayer())
	{
		// If target is significantly below (more than 32 units), be very lenient
		if ( heightDiff < -32 )
		{
			// Can see almost anything below us in horde mode
			// Only check 2D horizontal direction with very wide FOV
			Vector vec2DForward = gpGlobals->v_forward;
			vec2DForward.z = 0;
			float len2DForward = vec2DForward.Length();
			
			// Safety check: if forward vector has no horizontal component, allow sight
			if ( len2DForward < 0.01f )
				return TRUE;
			
			vec2DForward = vec2DForward.Normalize();
			
			Vector vec2DLOS = vecLOS;
			vec2DLOS.z = 0;
			float len2DLOS = vec2DLOS.Length();
			
			if ( len2DLOS < 0.01f )
				return TRUE;
			
			vec2DLOS = vec2DLOS.Normalize();
			
			float flDot2D = DotProduct( vec2DLOS, vec2DForward );
			
			// Almost 360 degree horizontal arc for targets below (very aggressive)
			if ( flDot2D > -0.95 )
				return TRUE;
		}
		
		// Use 3D dot product for normal detection
		flDot = DotProduct ( vecLOS , gpGlobals->v_forward );
		
		// 270 degree horizontal + vertical awareness
		if ( flDot > -0.707 )
			return TRUE;
	}
	else
	{
		// Single player: also check for targets below but less aggressive
		if ( heightDiff < -32 )
		{
			Vector vec2DForward = gpGlobals->v_forward;
			vec2DForward.z = 0;
			float len2DForward = vec2DForward.Length();
			
			if ( len2DForward < 0.01f )
				return TRUE;
			
			vec2DForward = vec2DForward.Normalize();
			
			Vector vec2DLOS = vecLOS;
			vec2DLOS.z = 0;
			float len2DLOS = vec2DLOS.Length();
			
			if ( len2DLOS < 0.01f )
				return TRUE;
			
			vec2DLOS = vec2DLOS.Normalize();
			
			float flDot2D = DotProduct( vec2DLOS, vec2DForward );
			
			// 270 degree arc for targets below in single player
			if ( flDot2D > -0.5 )
				return TRUE;
		}
		
		// Use 3D dot product
		flDot = DotProduct ( vecLOS , gpGlobals->v_forward );
		
		// Normal 90 degree cone with vertical awareness
		if ( flDot > 0.5 )
			return TRUE;
	}
	
	return FALSE;
}

//=========================================================
// FInViewCone - 3D vision cone for vector positions
//=========================================================
BOOL CDiablo::FInViewCone ( Vector *pOrigin )
{
	Vector vecLOS;
	float flDot;

	UTIL_MakeVectors ( pev->angles );
	
	// Calculate 3D direction to position
	vecLOS = ( *pOrigin - pev->origin ).Normalize();
	
	// Check vertical distance for special handling
	float heightDiff = pOrigin->z - pev->origin.z;
	
	// For horde mode, use wider field of view
	if (g_pGameRules->IsMultiplayer())
	{
		// If target is significantly below, be very lenient
		if ( heightDiff < -32 )
		{
			Vector vec2DForward = gpGlobals->v_forward;
			vec2DForward.z = 0;
			float len2DForward = vec2DForward.Length();
			
			if ( len2DForward < 0.01f )
				return TRUE;
			
			vec2DForward = vec2DForward.Normalize();
			
			Vector vec2DLOS = vecLOS;
			vec2DLOS.z = 0;
			float len2DLOS = vec2DLOS.Length();
			
			if ( len2DLOS < 0.01f )
				return TRUE;
			
			vec2DLOS = vec2DLOS.Normalize();
			
			float flDot2D = DotProduct( vec2DLOS, vec2DForward );
			
			if ( flDot2D > -0.95 )
				return TRUE;
		}
		
		// Use 3D dot product
		flDot = DotProduct ( vecLOS , gpGlobals->v_forward );
		
		if ( flDot > -0.707 )
			return TRUE;
	}
	else
	{
		// Single player: check for targets below
		if ( heightDiff < -32 )
		{
			Vector vec2DForward = gpGlobals->v_forward;
			vec2DForward.z = 0;
			float len2DForward = vec2DForward.Length();
			
			if ( len2DForward < 0.01f )
				return TRUE;
			
			vec2DForward = vec2DForward.Normalize();
			
			Vector vec2DLOS = vecLOS;
			vec2DLOS.z = 0;
			float len2DLOS = vec2DLOS.Length();
			
			if ( len2DLOS < 0.01f )
				return TRUE;
			
			vec2DLOS = vec2DLOS.Normalize();
			
			float flDot2D = DotProduct( vec2DLOS, vec2DForward );
			
			if ( flDot2D > -0.5 )
				return TRUE;
		}
		
		// Use 3D dot product
		flDot = DotProduct ( vecLOS , gpGlobals->v_forward );
		
		if ( flDot > 0.5 )
			return TRUE;
	}
	
	return FALSE;
}

//=========================================================
// Any custom shedules?
//=========================================================
Schedule_t* CDiablo::GetScheduleOfType(int Type)
{
	switch (Type)
	{
	case SCHED_DIABLO_STALK:
		return 	slDiabloStalk;
		break;

	case SCHED_RANGE_ATTACK1:
		return 	slDiabloLeap;
		break;

	case SCHED_CHASE_ENEMY:
		return 	slDiabloChase;
		break;

	case SCHED_DIABLO_CHASE_FAILED:
		return 	slDiabloChaseFailed;
		break;

	case SCHED_DIABLO_EMERG_JUMP:
		return 	slDiabloEmergencyJump;
		break;
	}
	return CBaseMonster::GetScheduleOfType(Type);

}

//=========================================================
// Load up the schedules so ai isn't dumb
//=========================================================
Schedule_t* CDiablo::GetSchedule(void)
{
	// Call another switch class, to check the monster's attitude
	switch (m_MonsterState)
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_ALERT:
	{
		m_fPissed = FALSE;
		//ALERT(at_console, "MONSTERSTATE_IDLE/ALERT\n");
	}
	break;

	case MONSTERSTATE_COMBAT:
	{
		if ((!HasConditions(bits_COND_NEW_ENEMY)) && (HasConditions(bits_COND_ENEMY_DEAD)))
			return CBaseMonster::GetSchedule();

		// In horde mode, always be aggressive immediately
		if (g_pGameRules->IsMultiplayer())
		{
			if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
				return GetScheduleOfType(SCHED_MELEE_ATTACK1);

			if (HasConditions(bits_COND_CAN_RANGE_ATTACK1))
				return GetScheduleOfType(SCHED_RANGE_ATTACK1);

			return GetScheduleOfType(SCHED_CHASE_ENEMY);
		}

		// Single player stealth behavior
		if ((!HasConditions(bits_COND_ENEMY_FACING_ME)) && (!m_fPissed))
		{
			if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
				return GetScheduleOfType(SCHED_MELEE_ATTACK1);

			if (HasConditions(bits_COND_CAN_RANGE_ATTACK1))
				return GetScheduleOfType(SCHED_RANGE_ATTACK1);

			return GetScheduleOfType(SCHED_DIABLO_STALK);
		}
		else if (!m_fPissed)
		{
			if (HasConditions(bits_COND_NEW_ENEMY))
				AlertSound();
		}

		/*if(!m_fPissed)
		{
			if (HasConditions(bits_COND_NEW_ENEMY))
				AlertSound();

			if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
				return GetScheduleOfType(SCHED_MELEE_ATTACK1);

			if (HasConditions(bits_COND_CAN_RANGE_ATTACK1))
				return GetScheduleOfType(SCHED_RANGE_ATTACK1);

			return GetScheduleOfType(SCHED_DIABLO_STALK);
		}*/
		else
		{
			if (HasConditions(bits_COND_CAN_MELEE_ATTACK1))
				return GetScheduleOfType(SCHED_MELEE_ATTACK1);

			if (HasConditions(bits_COND_CAN_RANGE_ATTACK1))
				return GetScheduleOfType(SCHED_RANGE_ATTACK1);

			return GetScheduleOfType(SCHED_CHASE_ENEMY);
		}
	}
	break;

	}
	//if all else fails, the base probably knows what to do
	return CBaseMonster::GetSchedule();
}

//=========================================================
// StartTask
//=========================================================
void CDiablo::StartTask(Task_t* pTask)
{
	TraceResult tr;

	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch (pTask->iTask)
	{
	case TASK_FACE_ENEMY:
	{
		// In horde mode, if enemy is below us, skip facing requirement
		// The leap attack will handle trajectory automatically
		if ( g_pGameRules->IsMultiplayer() && m_hEnemy != NULL )
		{
			float heightDiff = pev->origin.z - m_hEnemy->pev->origin.z;
			if ( heightDiff > 32 )
			{
				// Enemy is below - don't need to face them precisely
				// Just complete the task immediately
				TaskComplete();
				break;
			}
		}
		// Normal facing for same-level or above targets
		CBaseMonster::StartTask(pTask);
		break;
	}
	case TASK_DIABLO_RESETATTACK:
	{
		m_flNextAttack = -1;
		TaskComplete();
		break;
	}
	case TASK_DIABLO_GETPATH:
	{
		if (m_hEnemy == NULL)
		{
			TaskFail();
			return;
		}
		if (FallBack(m_hEnemy->pev->origin, m_hEnemy->pev->view_ofs))
		{
			m_flMoveWaitFinished = gpGlobals->time + pTask->flData;
			TaskComplete();
		}
		/*else if (FindCover(pev->origin, pev->view_ofs, 0, CoverRadius()))
		{
			// then try for plain ole cover
			m_flMoveWaitFinished = gpGlobals->time + pTask->flData;
			TaskComplete();
		}*/
		else if (BuildNearestRoute(m_hEnemy->pev->origin, m_hEnemy->pev->view_ofs, 0, (m_hEnemy->pev->origin - pev->origin).Length()))
		{
			TaskComplete();
		}
		else if (BuildRoute(m_hEnemy->pev->origin, bits_MF_TO_ENEMY, m_hEnemy))
		{
			TaskComplete();
		}
		else
		{
			//ALERT(at_console, "GetPathToEnemy failed!!\n");
			TaskFail();
		}
		break;
	}
	case TASK_RANGE_ATTACK1:
	case TASK_RANGE_ATTACK2:
	{
		//EMIT_SOUND_DYN(edict(), CHAN_WEAPON, pAttackSounds[0], GetSoundVolue(), ATTN_IDLE, 0, GetVoicePitch());
		m_IdealActivity = ACT_RANGE_ATTACK1;
		SetTouch(&CDiablo::LeapTouch);
		break;
	}
	case TASK_RANGE_ATTACK1_NOTURN:
	case TASK_RANGE_ATTACK2_NOTURN:
	{
		//EMIT_SOUND_DYN(edict(), CHAN_WEAPON, pAttackSounds[0], GetSoundVolue(), ATTN_IDLE, 0, GetVoicePitch());
		m_IdealActivity = ACT_RANGE_ATTACK1;
		SetTouch(&CDiablo::LeapTouch);
		break;
	}
	case TASK_DIABLO_CRAWL_PATH:
	{
		m_movementActivity = ACT_CROUCH;
		TaskComplete();
		break;
	}
	case TASK_DIABLO_CHECKJUMP:
	{
		if (m_hEnemy == NULL)
		{
			TaskFail();
			return;
		}

		UTIL_TraceLine(m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs, pev->origin + pev->view_ofs, ignore_monsters, ignore_glass, ENT(pev), &tr);

		if (tr.flFraction == 1.0)
			TaskComplete();
		else
			TaskFail();

		break;
	}
	default:
	{
		CBaseMonster::StartTask(pTask);
	}
	}
}

//=========================================================
// RunTask 
//=========================================================
void CDiablo::RunTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_RANGE_ATTACK1:
	case TASK_RANGE_ATTACK2:
	{
		MakeIdealYaw(m_vecEnemyLKP);
		ChangeYaw(pev->yaw_speed);

		if (m_fSequenceFinished)
		{
			TaskComplete();
			SetTouch(NULL);
			m_IdealActivity = ACT_IDLE;
		}
		break;
	}
	case TASK_RANGE_ATTACK1_NOTURN:
	case TASK_RANGE_ATTACK2_NOTURN:
	{
		if (m_fSequenceFinished)
		{
			TaskComplete();
			SetTouch(NULL);
			m_IdealActivity = ACT_IDLE;
		}
		break;
	}
	default:
	{
		CBaseMonster::RunTask(pTask);
	}
	}
}

//=========================================================
// LeapTouch - this is the headcrab's touch function when it
// is in the air
//=========================================================
void CDiablo::LeapTouch(CBaseEntity* pOther)
{
	if ((!pOther->pev->takedamage) && (!pOther->IsPlayer()))
		return;

	if (pOther->Classify() == Classify())
		return;

	// Don't hit if back on ground
	if (!FBitSet(pev->flags, FL_ONGROUND))
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, pAttackHitSounds[RANDOM_LONG(0, ARRAYSIZE(pAttackHitSounds) - 1)], 1.0, ATTN_NORM, 0, 100);

		Vector bloodvec = pev->origin;
		bloodvec.z += 5;

		UTIL_BloodStream(bloodvec, UTIL_RandomBloodVector(), pOther->BloodColor(), RANDOM_LONG(100, 175));
		//UTIL_Blood(pev->origin, UTIL_RandomBloodVector(), pOther->BloodColor(), RANDOM_LONG(25, 35)); //turn on when real beta.

		pOther->TakeDamage(pev, pev, gSkillData.pantherSlash, DMG_SLASH);

		ClearBits(pOther->pev->flags, FL_ONGROUND);

		UTIL_SetOrigin(pOther->pev, pOther->pev->origin + Vector(0, 0, 1));// take him off ground so engine doesn't instantly reset onground 
		UTIL_MakeVectors(pev->angles);

		pOther->pev->velocity = pOther->pev->velocity + gpGlobals->v_forward * 360;
		pOther->pev->velocity = pOther->pev->velocity + gpGlobals->v_up * 128;
	}
	SetTouch(NULL);
}


#define COVER_DELTA		84// distance between checks

BOOL CDiablo::FallBack(const Vector& vecThreat, const Vector& vecViewOffset)
{
	TraceResult	tr;
	Vector	vecStepBack, vecBackTest;

	UTIL_MakeVectors(pev->angles);
	vecStepBack = (gpGlobals->v_forward * -COVER_DELTA);
	vecStepBack.z = 0;

	vecBackTest = pev->origin;
	vecBackTest = vecBackTest + vecStepBack;

	float EnemyHeight = GetEnemyHeight();

	//enemy is below me, make sure its worth jumping now...
	if (EnemyHeight <= -1)
	{
		UTIL_TraceLine(vecThreat + vecViewOffset, pev->origin + pev->view_ofs, ignore_monsters, ignore_glass, ENT(pev), &tr);
	
		if (tr.flFraction != 1.0)
		{
			//ALERT(at_console, "%f %f\n", EnemyHeight, tr.flFraction);
			return FALSE;
		}
	}

	//first check if the panther can see the player clearly
	UTIL_TraceLine(vecThreat, pev->origin, ignore_monsters, ignore_glass, ENT(pev), &tr);

	//can't see the player - good time to back up and gett a better look
	if (tr.flFraction != 1.0)
	{
		//ALERT(at_console, "TEST BACK\n");
		UTIL_TraceLine(vecThreat + vecViewOffset, vecBackTest + pev->view_ofs, ignore_monsters, ignore_glass, ENT(pev), &tr);

		/*MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
		WRITE_BYTE(TE_LINE);

		WRITE_COORD(pev->origin[0]);
		WRITE_COORD(pev->origin[1]);
		WRITE_COORD(pev->origin[2] + 12);

		WRITE_COORD(tr.vecEndPos[0]);
		WRITE_COORD(tr.vecEndPos[1]);
		WRITE_COORD(tr.vecEndPos[2] + 12);

		WRITE_SHORT(5);
		WRITE_BYTE(255);
		WRITE_BYTE(2);
		WRITE_BYTE(2);
		MESSAGE_END();*/

		if (tr.flFraction == 1.0)
		{

			if (CheckLocalMove(pev->origin, vecBackTest, NULL, NULL) == LOCALMOVE_VALID)
			{
				if (MoveToLocation(ACT_RUN, 0, vecBackTest))
				{
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

void CDiablo::RunAI(void)
{
	// Check if current enemy is invalid (godmode or dead)
	if ( m_hEnemy != NULL )
	{
		if ( IsInGodMode( m_hEnemy ) || !m_hEnemy->IsAlive() )
		{
			// Enemy is invalid, clear them
			m_hEnemy = NULL;
			ClearConditions( bits_COND_SEE_ENEMY | bits_COND_SEE_CLIENT );
			SetConditions( bits_COND_ENEMY_DEAD );
		}
	}
	
	// In horde mode, if we have a valid pissed enemy, be aggressive
	if ( g_pGameRules->IsMultiplayer() && m_fPissed && m_hEnemy != NULL )
	{
		BOOL bEnemyBelow = IsEnemyBelowUs( m_hEnemy );
		
		// For enemies below, trust last known position without visibility check
		// For same-level enemies, require visibility
		if ( bEnemyBelow || FVisible( m_hEnemy ) )
		{
			m_vecEnemyLKP = m_hEnemy->pev->origin;
			
			// Force conditions for enemies below to prevent base class from clearing
			if ( bEnemyBelow )
			{
				ForceDownwardAttackConditions( m_hEnemy );
			}
			
			// Ensure we stay in combat state
			if ( m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_ALERT )
			{
				m_IdealMonsterState = MONSTERSTATE_COMBAT;
			}
		}
	}
	
	CBaseMonster::RunAI();
	
	// AFTER base class runs, re-force conditions for enemies below
	// This ensures base class CheckAttacks doesn't clear our forced conditions
	if ( g_pGameRules->IsMultiplayer() && m_fPissed && m_hEnemy != NULL && IsEnemyBelowUs( m_hEnemy ) )
	{
		ForceDownwardAttackConditions( m_hEnemy );
	}
}