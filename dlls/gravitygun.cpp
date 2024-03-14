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

void CGravityGun::Spawn()
{
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
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_weapons.mdl");

	PRECACHE_SOUND("weapons/rocketfire1.wav");

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
		m_pCurrentEntity->pev->iuser3 = 0;
		m_pCurrentEntity = NULL;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
	}
	else
	{
		CBaseEntity* pEntity = GetEntity(324, true);
		#ifndef CLIENT_DLL
		TraceResult tr = UTIL_GetGlobalTrace();
		if (pEntity)
		{
			idx = ENTINDEX(pEntity->edict());
			isBspModel = pEntity->IsBSPModel();

			ClearMultiDamage();
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgGravityGun, gpGlobals->v_forward, &tr, DMG_ENERGYBEAM);
			ApplyMultiDamage(pev, m_pPlayer->pev);
			pEntity->pev->velocity = gpGlobals->v_forward * 256;

			m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
		}

		if (pEntity && strstr(STRING(pEntity->pev->classname), "worldspawn"))
		{
			UTIL_MakeVectors( m_pPlayer->pev->v_angle );
			Vector vecSrc = pev->origin + pev->view_ofs + gpGlobals->v_forward * 64 + gpGlobals->v_up * 18;
			m_pCurrentEntity = CBaseEntity::Create( "monster_barrel", vecSrc, Vector(0, pev->v_angle.y, 0), m_pPlayer->edict());
			m_pCurrentEntity->pev->iuser3 = 1;
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/rocketfire1.wav", 1.0, ATTN_NORM);
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
		m_pCurrentEntity->pev->velocity = m_pPlayer->pev->velocity;
		m_pCurrentEntity->pev->iuser3 = 0;
		m_pCurrentEntity = NULL;
	}
	else
	{
		m_pCurrentEntity = GetEntity(2048);
		if (m_pCurrentEntity)
		{
			m_pCurrentEntity->pev->origin[2] += 0.2f;
			m_pCurrentEntity->pev->iuser3 = 1;
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

void CGravityGun::ItemPostFrame()
{
	if (m_pCurrentEntity)
	{
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
		pPotentialTarget = GetEntity(2048);
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
