/***
*
*	Copyright (c) 2024, Magic Nipples.
*
*	Use and modification of this code is allowed as long
*	as credit is provided! Enjoy!
*
****/
//=========================================================
// Panthereye - stalking ambush style preditor
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
	void SetYawSpeed(void);

	float GetEnemyHeight(void);

	void HandleAnimEvent(MonsterEvent_t* pEvent);
	int TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
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

	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 64));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	pev->health = gSkillData.pantherHealth;
	pev->view_ofs = Vector(0, 0, 24);	// position of the eyes relative to monster's origin.

	m_flFieldOfView = 0.5;	// indicates the width of this monster's forward view cone ( as a dotproduct result )
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

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CDiablo::SetYawSpeed(void)
{
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
	switch (pEvent->event)
	{
	case DIABLO_AE_MELEEATTACK:
	{
		AttackSound();

		CBaseEntity* pHurt = CheckTraceHullAttack(75, gSkillData.zombieDmgBothSlash, DMG_SLASH);
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
// Needs to be provoked to start running
//=========================================================
int CDiablo::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if(!m_fPissed)
		m_fPissed = TRUE;

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL CDiablo::CheckRangeAttack1(float flDot, float flDist)
{
	if (m_flNextAttack < gpGlobals->time)
	{
		if (FBitSet(pev->flags, FL_ONGROUND) && flDist <= 256 && flDist > 75 && flDot >= 0.65)
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

		pOther->TakeDamage(pev, pev, 25, DMG_SLASH);

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
			ALERT(at_console, "%f %f\n", EnemyHeight, tr.flFraction);
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
	CBaseMonster::RunAI();
}