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
#include "player.h"
#include "gamerules.h"
#include "game.h"

#define VectorAverage(a, b, o) {((o)[0] = ((a)[0] + (b)[0]) * 0.5, (o)[1] = ((a)[1] + (b)[1]) * 0.5, (o)[2] = ((a)[2] + (b)[2]) * 0.5);}

#ifdef GRAVITYGUN
LINK_ENTITY_TO_CLASS(weapon_gravitygun, CGravityGun);
#endif

enum gravitygun_e {
	GRAVITYGUN_IDLE1 = 0,
	GRAVITYGUN_HOLD_IDLE,
	GRAVITYGUN_PICKUP,
	GRAVITYGUN_FIRE,
	GRAVITYGUN_DRAW_LOWKEY,
	GRAVITYGUN_DRAW,
	GRAVITYGUN_HOLSTER,
};

static const float GRAVITYGUN_REPULSE_RADIUS = 180.0f;
static const float GRAVITYGUN_REPULSE_FORWARD_OFFSET = 48.0f;
static const float GRAVITYGUN_REPULSE_FORCE_MIN = 1150.0f;
static const float GRAVITYGUN_REPULSE_FORCE_MAX = 1900.0f;
static const float GRAVITYGUN_REPULSE_UP_BIAS = 0.35f;
static const float GRAVITYGUN_REPULSE_UPLIFT_MIN = 220.0f;
static const float GRAVITYGUN_REPULSE_UPLIFT_MAX = 420.0f;
static const float GRAVITYGUN_REPULSE_DAMAGE = 8.0f;
static const float GRAVITYGUN_REPULSE_COOLDOWN_SUCCESS = 0.85f;
static const float GRAVITYGUN_REPULSE_COOLDOWN_FAIL = 0.45f;

static BOOL GravityGunCanRepulseTarget(CBasePlayer *pOwner, CBaseEntity *pTarget)
{
	if (!pOwner || !pTarget || !pTarget->pev || pTarget == pOwner)
		return FALSE;

	if (FClassnameIs(pTarget->pev, "worldspawn"))
		return FALSE;

	if (pTarget->IsPlayer() || (pTarget->pev->flags & FL_MONSTER))
		return pTarget->IsAlive();

	if (pTarget->pev->solid == SOLID_NOT || pTarget->pev->movetype == MOVETYPE_FOLLOW)
		return FALSE;

	if (pTarget->pev->movetype == MOVETYPE_NONE ||
		pTarget->pev->movetype == MOVETYPE_PUSH ||
		pTarget->pev->movetype == MOVETYPE_NOCLIP)
	{
		return FALSE;
	}

	if (pTarget->IsBSPModel() && pTarget->pev->movetype != MOVETYPE_PUSHSTEP)
		return FALSE;

	return TRUE;
}

static BOOL GravityGunHasRepulseLOS(CBasePlayer *pOwner, CBaseEntity *pTarget, const Vector &vecStart)
{
	if (!pOwner || !pTarget)
		return FALSE;

	TraceResult tr;
	UTIL_TraceLine(vecStart, pTarget->Center(), dont_ignore_monsters, ENT(pOwner->pev), &tr);

	return (tr.flFraction >= 1.0f || tr.pHit == pTarget->edict());
}

static BOOL GravityGunRepulseTarget(CBasePlayer *pOwner, CBaseEntity *pTarget, const Vector &vecBurstOrigin)
{
	if (!GravityGunCanRepulseTarget(pOwner, pTarget) || !GravityGunHasRepulseLOS(pOwner, pTarget, vecBurstOrigin))
		return FALSE;

	Vector vecToTarget = pTarget->Center() - vecBurstOrigin;
	float flDistance = vecToTarget.Length();

	if (flDistance <= 1.0f)
		vecToTarget = gpGlobals->v_forward;
	else
		vecToTarget = vecToTarget / flDistance;

	Vector vecLaunchDir = vecToTarget + gpGlobals->v_forward * 0.55f + Vector(0, 0, GRAVITYGUN_REPULSE_UP_BIAS);
	if (vecLaunchDir.Length() <= 0.01f)
		vecLaunchDir = gpGlobals->v_forward;
	else
		vecLaunchDir = vecLaunchDir.Normalize();

	float flScale = 1.0f - (flDistance / GRAVITYGUN_REPULSE_RADIUS);
	if (flScale < 0.0f)
		flScale = 0.0f;
	if (flScale > 1.0f)
		flScale = 1.0f;

	const float flForce = GRAVITYGUN_REPULSE_FORCE_MIN + (GRAVITYGUN_REPULSE_FORCE_MAX - GRAVITYGUN_REPULSE_FORCE_MIN) * flScale;
	const float flUplift = GRAVITYGUN_REPULSE_UPLIFT_MIN + (GRAVITYGUN_REPULSE_UPLIFT_MAX - GRAVITYGUN_REPULSE_UPLIFT_MIN) * flScale;

	pTarget->pev->flags &= ~FL_ONGROUND;
	pTarget->pev->velocity = vecLaunchDir * flForce + pOwner->pev->velocity * 0.45f;
	pTarget->pev->velocity.z += flUplift;

	if (pTarget->pev->takedamage != DAMAGE_NO)
	{
		pTarget->TakeDamage(pOwner->pev, pOwner->pev, GRAVITYGUN_REPULSE_DAMAGE, DMG_SONIC | DMG_NEVERGIB);
	}

	return TRUE;
}

static void GravityGunRepulseBurstFX(CBasePlayer *pOwner, int iBeamSprite, int iFlashSprite, const Vector &vecOrigin)
{
	if (!pOwner || (iBeamSprite <= 0 && iFlashSprite <= 0))
		return;

	if (iBeamSprite > 0)
	{
		MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_BEAMDISK);
			WRITE_COORD(vecOrigin.x);
			WRITE_COORD(vecOrigin.y);
			WRITE_COORD(vecOrigin.z);
			WRITE_COORD(vecOrigin.x);
			WRITE_COORD(vecOrigin.y);
			WRITE_COORD(vecOrigin.z + GRAVITYGUN_REPULSE_RADIUS);
			WRITE_SHORT(iBeamSprite);
			WRITE_BYTE(0);
			WRITE_BYTE(0);
			WRITE_BYTE(10);
			WRITE_BYTE(36);
			WRITE_BYTE(0);
			if ( icesprites.value )
			{
				WRITE_BYTE( 0 );
				WRITE_BYTE( 113 );
				WRITE_BYTE( 230 );
			}
			else
			{
				WRITE_BYTE(255);
				WRITE_BYTE(180);
				WRITE_BYTE(48);
			}
			WRITE_BYTE(255);
			WRITE_BYTE(0);
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_BEAMCYLINDER);
			WRITE_COORD(vecOrigin.x);
			WRITE_COORD(vecOrigin.y);
			WRITE_COORD(vecOrigin.z + 8);
			WRITE_COORD(vecOrigin.x);
			WRITE_COORD(vecOrigin.y);
			WRITE_COORD(vecOrigin.z + GRAVITYGUN_REPULSE_RADIUS);
			WRITE_SHORT(iBeamSprite);
			WRITE_BYTE(0);
			WRITE_BYTE(0);
			WRITE_BYTE(10);
			WRITE_BYTE(28);
			WRITE_BYTE(0);
			if ( icesprites.value )
			{
				WRITE_BYTE( 0 );
				WRITE_BYTE( 113 );
				WRITE_BYTE( 230 );
			}
			else
			{
				WRITE_BYTE(255);
				WRITE_BYTE(210);
				WRITE_BYTE(72);
			}
			WRITE_BYTE(240);
			WRITE_BYTE(0);
		MESSAGE_END();
	}

	if (iFlashSprite > 0)
	{
		MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_SPRITE);
			WRITE_COORD(vecOrigin.x);
			WRITE_COORD(vecOrigin.y);
			WRITE_COORD(vecOrigin.z + 12);
			WRITE_SHORT(iFlashSprite);
			WRITE_BYTE(24);
			WRITE_BYTE(180);
		MESSAGE_END();
	}

	MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE(TE_DLIGHT);
		WRITE_COORD(vecOrigin.x);
		WRITE_COORD(vecOrigin.y);
		WRITE_COORD(vecOrigin.z + 8);
		WRITE_BYTE(45);
		if ( icesprites.value )
		{
			WRITE_BYTE( 0 );
			WRITE_BYTE( 113 );
			WRITE_BYTE( 230 );
		}
		else
		{
			WRITE_BYTE( 255 );
			WRITE_BYTE( 180 );
			WRITE_BYTE( 48 );
		}
		WRITE_BYTE(6);
		WRITE_BYTE(30);
	MESSAGE_END();
}

static CBaseEntity *FindPlayerDribbledKtsBall(CBasePlayer *pPlayer)
{
	#ifdef CLIENT_DLL
	return NULL;
	#else
	if (!pPlayer || !g_pGameRules || !g_pGameRules->IsKickTheSnowball())
		return NULL;

	CBaseEntity *pBall = NULL;
	while ((pBall = UTIL_FindEntityByClassname(pBall, "kts_snowball")) != NULL)
	{
		if (pBall->pev && pBall->pev->euser1 == pPlayer->edict())
			return pBall;
	}

	return NULL;
	#endif
}

void CGravityGun::Spawn()
{
	m_iRepulseBurstSprite = 0;
	m_iRepulseFlashSprite = 0;
	m_flNextRepulseTime = 0;
	m_flNextIdleTime = 0;
	m_bResetIdle = false;
	m_bFoundPotentialTarget = false;
	m_pCurrentEntity = NULL;

	Precache();

	pev->classname = MAKE_STRING("weapon_gravitygun");

	m_iId = WEAPON_GRAVITYGUN;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_GRAVITYGUN - 1;
	m_iClip = -1;

	FallInit(); // get ready to fall down.
}

void CGravityGun::Precache()
{
	PRECACHE_MODEL("models/v_gravitygun.mdl");
	m_iRepulseBurstSprite = PRECACHE_MODEL("sprites/lgtning.spr");
	m_iRepulseFlashSprite = PRECACHE_MODEL("sprites/glowbig.spr");
	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("common/wpn_denyselect.wav");

	m_usGravGun = PRECACHE_EVENT(1, "events/gravitygun.sc");
}

int CGravityGun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CGravityGun::GetItemInfo(ItemInfo* p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 8;
	p->iId = WEAPON_GRAVITYGUN;
	p->iWeight = GRAVITYGUN_WEIGHT;
	p->pszDisplayName = "Gravity Gun";
	return true;
}

BOOL CGravityGun::DeployLowKey()
{
	return DefaultDeploy( "models/v_gravitygun.mdl", "models/p_weapons.mdl", GRAVITYGUN_DRAW_LOWKEY, "gauss" );
}

BOOL CGravityGun::Deploy()
{
	return DefaultDeploy("models/v_gravitygun.mdl", "models/p_weapons.mdl", GRAVITYGUN_DRAW, "gauss");
}

void CGravityGun::Holster(int skiplocal)
{
	STOP_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "ambience/pulsemachine.wav");
	CBasePlayerWeapon::DefaultHolster(GRAVITYGUN_HOLSTER);
}

void CGravityGun::PrimaryAttack()
{
	int idx = 0;
	bool isBspModel = false;

	if (m_pCurrentEntity)
	{
		Vector forward = m_pPlayer->GetAutoaimVector(0.0f);

		idx = ENTINDEX(m_pCurrentEntity->edict());
		if (m_pCurrentEntity->IsBSPModel())
			isBspModel = true;

		m_pCurrentEntity->pev->velocity = m_pPlayer->pev->velocity + forward * 1024;
		if (FClassnameIs(m_pCurrentEntity->pev, "kts_snowball"))
				m_pCurrentEntity->pev->iuser2 = ENTINDEX(m_pPlayer->edict());
		m_pCurrentEntity->pev->iuser3 = 0;
		m_pCurrentEntity = NULL;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
	}
	else
	{
		CBaseEntity* pEntity = FindPlayerDribbledKtsBall(m_pPlayer);
		if (pEntity)
		{
			// KTS: if the player is already dribbling, a tap should always pick
			// the ball up into gravity-gun hold, even when it is not in the trace.
			g_pGameRules->DropCharm(m_pPlayer, pEntity->pev->origin);
			m_pCurrentEntity = pEntity;
			m_pCurrentEntity->pev->iuser3 = 1;
			m_pCurrentEntity->pev->iuser2 = ENTINDEX(m_pPlayer->edict());
			idx = ENTINDEX(pEntity->edict());
			isBspModel = pEntity->IsBSPModel();
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
		}

		if (!m_pCurrentEntity)
			pEntity = GetEntity(256, true);
		#ifndef CLIENT_DLL
		TraceResult tr = UTIL_GetGlobalTrace();
		if (!m_pCurrentEntity && pEntity)
		{
			idx = ENTINDEX(pEntity->edict());
			isBspModel = pEntity->IsBSPModel();

			ClearMultiDamage();
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgGravityGun, gpGlobals->v_forward, &tr, DMG_ENERGYBEAM);
			ApplyMultiDamage(pev, m_pPlayer->pev);
			pEntity->pev->velocity = gpGlobals->v_forward * 256;

			m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
		}

		if (!m_pCurrentEntity && !g_pGameRules->IsKickTheSnowball())
		{
			if (pEntity && strstr(STRING(pEntity->pev->classname), "worldspawn"))
			{
				UTIL_MakeVectors( m_pPlayer->pev->v_angle );
				Vector vecSrc = pev->origin + pev->view_ofs + gpGlobals->v_forward * 64 + gpGlobals->v_up * 18;
				m_pCurrentEntity = CBaseEntity::Create( "monster_barrel", vecSrc, Vector(0, pev->v_angle.y, 0), m_pPlayer->edict());
				m_pCurrentEntity->pev->iuser3 = 1;
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/rocketfire1.wav", 1.0, ATTN_NORM);
			}
		}
		#endif
	}

	PLAYBACK_EVENT_FULL(FEV_SERVER, m_pPlayer->edict(), m_usGravGun,
		0.0, Vector(0,0,0), Vector(0,0,0), 0.0f, 0.0f, idx,
		0, isBspModel ? 1 : 0, 0);

	STOP_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "ambience/pulsemachine.wav");
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.4f);
	m_flNextIdleTime = gpGlobals->time + 2.0f;
}

void CGravityGun::SecondaryAttack()
{
	if (m_pCurrentEntity)
	{
		STOP_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "ambience/pulsemachine.wav");
		if (g_pGameRules && g_pGameRules->IsKickTheSnowball() &&
			FClassnameIs(m_pCurrentEntity->pev, "kts_snowball"))
		{
			// KTS: secondary while holding the ball returns it to foot dribble.
			m_pCurrentEntity->pev->iuser3 = 0;
			m_pCurrentEntity->pev->velocity = g_vecZero;
			m_pCurrentEntity = NULL;
			g_pGameRules->CaptureCharm(m_pPlayer);
		}
		else
		{
			m_pCurrentEntity->pev->velocity = m_pPlayer->pev->velocity;
			m_pCurrentEntity->pev->iuser3 = 0;
			m_pCurrentEntity = NULL;
		}
	}
	else
	{
		CBaseEntity *pEntity = FindPlayerDribbledKtsBall(m_pPlayer);
		if (pEntity)
		{
			g_pGameRules->DropCharm(m_pPlayer, pEntity->pev->origin);
			m_pCurrentEntity = pEntity;
		}
		else
		{
			m_pCurrentEntity = GetEntity(256);
		}

		if (m_pCurrentEntity)
		{
			m_pCurrentEntity->pev->origin[2] += 0.2f;
			m_pCurrentEntity->pev->iuser3 = 1;
			if (FClassnameIs(m_pCurrentEntity->pev, "kts_snowball"))
				m_pCurrentEntity->pev->iuser2 = ENTINDEX(m_pPlayer->edict());
			SendWeaponAnim(GRAVITYGUN_HOLD_IDLE);
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "ambience/pulsemachine.wav", 1.0, ATTN_NORM, 0, PITCH_HIGH);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.53f;
		}
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.4f);

	m_flNextIdleTime = gpGlobals->time + 2.0f;

	if (!m_pCurrentEntity)
		SendWeaponAnim(GRAVITYGUN_FIRE);
}

void CGravityGun::Reload()
{
	if (!m_pPlayer)
		return;

	if (gpGlobals->time < m_flNextRepulseTime)
		return;

	const float flWeaponMultiplier = (g_pGameRules ? g_pGameRules->WeaponMultipler() : 1.0f);

#ifdef CLIENT_DLL
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + GRAVITYGUN_REPULSE_COOLDOWN_FAIL * flWeaponMultiplier;
	return;
#else
	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	const Vector vecBurstOrigin = m_pPlayer->pev->origin + gpGlobals->v_forward * 36.0f + gpGlobals->v_up * 28.0f;

	int iTargetsPushed = 0;
	CBaseEntity *pHeldEntity = m_pCurrentEntity;

	if (pHeldEntity)
	{
		STOP_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "ambience/pulsemachine.wav");
		pHeldEntity->pev->iuser3 = 0;

		if (FClassnameIs(pHeldEntity->pev, "kts_snowball"))
			pHeldEntity->pev->iuser2 = ENTINDEX(m_pPlayer->edict());

		if (GravityGunRepulseTarget(m_pPlayer, pHeldEntity, vecBurstOrigin))
			iTargetsPushed++;

		m_pCurrentEntity = NULL;
	}

	CBaseEntity *pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecBurstOrigin, GRAVITYGUN_REPULSE_RADIUS)) != NULL)
	{
		if (pEntity == pHeldEntity)
			continue;

		if (GravityGunRepulseTarget(m_pPlayer, pEntity, vecBurstOrigin))
			iTargetsPushed++;
	}

	const bool bSuccess = (iTargetsPushed > 0);
	const float flCooldown = (bSuccess ? GRAVITYGUN_REPULSE_COOLDOWN_SUCCESS : GRAVITYGUN_REPULSE_COOLDOWN_FAIL) * flWeaponMultiplier;

	if (bSuccess)
	{
		SendWeaponAnim(GRAVITYGUN_FIRE);
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/rocketfire1.wav", 1.0f, ATTN_NORM);
		GravityGunRepulseBurstFX(m_pPlayer, m_iRepulseBurstSprite, m_iRepulseFlashSprite, vecBurstOrigin);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.7f;
	}
	else
	{
		SendWeaponAnim(GRAVITYGUN_PICKUP);
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "common/wpn_denyselect.wav", 0.8f, ATTN_NORM);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.45f;
	}

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + flCooldown;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCooldown);
	m_flNextRepulseTime = gpGlobals->time + flCooldown;
	m_flNextIdleTime = gpGlobals->time + 0.6f;
#endif
}

void CGravityGun::ItemPostFrame()
{
	if (m_pCurrentEntity)
	{
		// Drop the held entity if it has drifted too far from the player
		// (e.g. snagged on geometry while the player kept moving).
		float holdDist = (m_pCurrentEntity->pev->origin - m_pPlayer->pev->origin).Length();
		if (holdDist > 128.0f)
		{
			STOP_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "ambience/pulsemachine.wav");
			m_pCurrentEntity->pev->iuser3 = 0;
			m_pCurrentEntity->pev->velocity = g_vecZero;
			m_pCurrentEntity = NULL;
			SendWeaponAnim(GRAVITYGUN_FIRE);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
			CBasePlayerWeapon::ItemPostFrame();
			return;
		}

		m_pPlayer->m_flNextAutoMelee = gpGlobals->time + 0.5f; // always advance melee if I have an item
		m_pPlayer->GetAutoaimVector(0.0f);

		if (m_pCurrentEntity->IsBSPModel())
		{
			Vector absorigin;
			VectorAverage(m_pCurrentEntity->pev->absmax, m_pCurrentEntity->pev->absmin, absorigin);

			m_pCurrentEntity->pev->velocity = ((m_pPlayer->pev->origin - absorigin) + gpGlobals->v_forward * 86) * 35;
		}
		else
		{
			if (strcmp("player", STRING(m_pCurrentEntity->pev->classname)) == 0)
				m_pCurrentEntity = NULL;
			else if (!strncmp("weapon_", STRING(m_pCurrentEntity->pev->classname), 7) || !strncmp("item_", STRING(m_pCurrentEntity->pev->classname), 5))
				m_pCurrentEntity->pev->velocity = ((m_pPlayer->pev->origin - m_pCurrentEntity->pev->origin) + gpGlobals->v_forward * 86 + Vector(0, 0, 24)) * 35;
			else
				m_pCurrentEntity->pev->velocity = ((m_pPlayer->pev->origin - m_pCurrentEntity->pev->origin) + gpGlobals->v_forward * 86) * 35;
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}

CBaseEntity* CGravityGun::GetEntity(float fldist, bool m_bTakeDamage)
{
	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector forward = m_pPlayer->GetAutoaimVector(0.0f);
	Vector vecSrc = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 32;
	Vector vecEnd = vecSrc + forward * fldist;
	CBaseEntity* pEntity = NULL;

	UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, NULL, &tr);

	if (!tr.pHit)
		pEntity = UTIL_FindEntityInSphere(pEntity, tr.vecEndPos, 2.0f);
	else
		pEntity = CBaseEntity::Instance(tr.pHit);

	if (pEntity && pEntity->IsPlayer())
		return NULL;

	if (m_bTakeDamage)
	{
		if (!pEntity)
			return NULL;

		if ((pEntity->IsBSPModel() && (pEntity->pev->movetype == MOVETYPE_PUSHSTEP || pEntity->pev->takedamage == DAMAGE_YES)))
		{
			return pEntity;
		}
	}
	else
	{
		if (!pEntity || (pEntity->IsBSPModel() && pEntity->pev->movetype != MOVETYPE_PUSHSTEP))
			pEntity = UTIL_FindEntityInSphere(pEntity, tr.vecEndPos, 2.0f);

		if (!pEntity || (pEntity->IsBSPModel() && pEntity->pev->movetype != MOVETYPE_PUSHSTEP))
			return NULL;
	}
	if (pEntity == m_pPlayer)
		return NULL;

	return pEntity;
}

void CGravityGun::WeaponIdle()
{	
	CBaseEntity* pPotentialTarget = NULL;

	if (m_flNextIdleTime > gpGlobals->time)
		return;

	if (!m_pCurrentEntity)
	{
		pPotentialTarget = GetEntity(256);
		if (m_bFoundPotentialTarget && !pPotentialTarget)
		{
			m_bFoundPotentialTarget = false;
			m_bResetIdle = true;
		}
		else if (pPotentialTarget && !m_bFoundPotentialTarget)
		{
			m_bResetIdle = true;
		}	
	}

	if (m_bResetIdle)
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
		m_bResetIdle = false;
	}

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if (m_pCurrentEntity)
	{
		SendWeaponAnim(GRAVITYGUN_HOLD_IDLE);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.53;
	}
	else
	{
		if (pPotentialTarget)
		{
			m_pPlayer->m_flNextAutoMelee = gpGlobals->time + 1.5f; // always advance melee if I have an item
			SendWeaponAnim(GRAVITYGUN_PICKUP);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0f;
			m_bFoundPotentialTarget = true;
		}
		else
		{
			int iAnim = GRAVITYGUN_IDLE1;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
			SendWeaponAnim(iAnim);
		}
	}
}
