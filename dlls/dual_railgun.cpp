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
#include "soundent.h"
#include "shake.h"
#include "gamerules.h"
#include "decals.h"
#include "game.h"

#define RAILGUN_PRIMARY_FIRE_VOLUME	450
#define RAIL_BEAM_SPRITE "sprites/xbeam1.spr"

enum dual_railgun_e {
	DUAL_RAILGUN_IDLE = 0,
	DUAL_RAILGUN_FIRE_BOTH,
	DUAL_RAILGUN_FIRE_RIGHT,
	DUAL_RAILGUN_FIRE_LEFT,
	DUAL_RAILGUN_HOLSTER,
	DUAL_RAILGUN_DRAW_LOWKEY,
	DUAL_RAILGUN_DRAW
};

enum railgun_side_e {
	RAILGUN_SIDE_RIGHT = 0,
	RAILGUN_SIDE_LEFT,
};

#ifdef DUALRAILGUN
LINK_ENTITY_TO_CLASS( weapon_dual_railgun, CDualRailgun );
#endif

//=========================================================
//=========================================================

void CDualRailgun::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_dual_railgun"); 
	Precache( );
	m_iId = WEAPON_DUAL_RAILGUN;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_DUAL_RAILGUN - 1;

	m_iDefaultAmmo = RAILGUN_DEFAULT_GIVE * 2;
	pev->dmg = gSkillData.plrDmgRailgun;

	FallInit();
}

void CDualRailgun::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_railgun.mdl");

	PRECACHE_SOUND("railgun_fire2.wav");
	
	PRECACHE_MODEL( RAIL_BEAM_SPRITE );
	PRECACHE_MODEL( "sprites/blueflare1.spr" );
	m_usRailgunFire = PRECACHE_EVENT( 1, "events/dual_railgun.sc" );
}

int CDualRailgun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CDualRailgun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 6;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_DUAL_RAILGUN;
	p->iFlags = 0;
	p->iWeight = RAILGUN_WEIGHT * 2;
	p->pszDisplayName = "Quake II Railguns";

	return 1;
}

BOOL CDualRailgun::DeployLowKey( )
{
	return DefaultDeploy( "models/v_dual_railgun.mdl", "models/p_weapons.mdl", DUAL_RAILGUN_DRAW_LOWKEY, "dual_shotgun" );
}

BOOL CDualRailgun::Deploy( )
{
	return DefaultDeploy( "models/v_dual_railgun.mdl", "models/p_weapons.mdl", DUAL_RAILGUN_DRAW, "dual_shotgun" );
}

void CDualRailgun::Holster( int skiplocal )
{
	CBasePlayerWeapon::DefaultHolster(DUAL_RAILGUN_HOLSTER);
}

void CDualRailgun::PrimaryAttack()
{
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 2)
	{
		PlayEmptySound( );
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 2;

	int right = 16;
	int side = RAILGUN_SIDE_RIGHT;
	if (!m_iAltFire) {
		SendWeaponAnim( DUAL_RAILGUN_FIRE_RIGHT );
	} else {
		SendWeaponAnim( DUAL_RAILGUN_FIRE_LEFT );
		right = -12;
		side = RAILGUN_SIDE_LEFT;
	}
	m_iAltFire++;
	if (m_iAltFire > 1) m_iAltFire = 0;

	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	StartFire(vecAiming, vecSrc, (gpGlobals->v_up * -12 + gpGlobals->v_right * right + vecAiming * 32), side);
	m_pPlayer->pev->punchangle.x = RANDOM_LONG(-5, -8);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.5); 
}

void CDualRailgun::SecondaryAttack()
{
	if (m_pPlayer->pev->waterlevel == 3 || m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 2)
	{
		PlayEmptySound( );
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 4;

	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	SendWeaponAnim( DUAL_RAILGUN_FIRE_BOTH );
	StartFire(vecAiming, vecSrc, (gpGlobals->v_up * -12 + gpGlobals->v_right * 16 + vecAiming * 32), RAILGUN_SIDE_RIGHT);
#ifdef CLIENT_DLL
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usRailgunFire, 0.2,
		(float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles,
		gSkillData.plrDmgRailgun, 0.0, RAILGUN_SIDE_LEFT, 0, 0, 0 );
#endif
#ifndef CLIENT_DLL
	SetThink( &CDualRailgun::FireThink );
	pev->nextthink = gpGlobals->time + (0.2 * g_pGameRules->WeaponMultipler());
#endif

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.75); 
}

void CDualRailgun::FireThink( void )
{
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	StartFire(vecAiming, vecSrc, (gpGlobals->v_up * -12 + gpGlobals->v_right * -12 + vecAiming * 32), RAILGUN_SIDE_LEFT);
	m_pPlayer->pev->punchangle.x = RANDOM_LONG(-8, -12);
}
void CDualRailgun::StartFire( Vector vecAiming, Vector vecSrc, Vector effectSrc, int iRailSide )
{	
	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	Fire( vecSrc, vecAiming, effectSrc, gSkillData.plrDmgRailgun, iRailSide );
}

void CDualRailgun::Fire( Vector vecSrc, Vector vecDir, Vector effectSrc, float flDamage, int iRailSide )
{
	m_pPlayer->m_iWeaponVolume = RAILGUN_PRIMARY_FIRE_VOLUME;

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usRailgunFire, 0.0,
		(float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles,
		flDamage, 0.0, iRailSide, 0, 0, 0 );

	Vector vecDest = vecSrc + effectSrc + vecDir * 4096;
	edict_t *pentIgnore = ENT(m_pPlayer->pev);
	TraceResult tr;

#ifndef CLIENT_DLL
	int nMaxPunchThroughs = 3;
	BOOL firstBeam = FALSE;
	while (nMaxPunchThroughs > 0)
	{
		// Reflection
		if (firstBeam)
		{
			float n, flMaxFrac = 1.0;
			n = -DotProduct( tr.vecPlaneNormal, vecDir);
			if (n < 0.5) // 60 degrees
			{
				Vector r;
				VectorMA( vecDir, 2.0 * n, tr.vecPlaneNormal, r );
				flMaxFrac = flMaxFrac - tr.flFraction;
				VectorCopy( r, vecDir );
				VectorMA( tr.vecEndPos, 8.0, vecDir, vecSrc );
				VectorMA( vecSrc, 8192.0, vecDir, vecDest );
			}
		}
		firstBeam = TRUE;

		UTIL_TraceLine(vecSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr);

		effectSrc = Vector(0,0,0);
		//if (tr.fAllSolid)
		//	break;

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		if (pEntity == NULL)
			return;

		if (pEntity->pev->takedamage)
		{
			ClearMultiDamage();
			pEntity->TraceAttack( m_pPlayer->pev, flDamage, vecDir, &tr, DMG_ENERGYBEAM | DMG_ALWAYSGIB );
			ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);
		}
		else
		{
			// Keep server decals authoritative while visual beam/trail is client event-driven.
			int decal = DECAL_GUNSHOT1 + RANDOM_LONG(0,4);
			if (g_pGameRules->MutatorEnabled(MUTATOR_PAINTBALL)) {
				decal = DECAL_PAINT1 + RANDOM_LONG(0, 7);
			}
			UTIL_DecalTrace(&tr, decal);

			nMaxPunchThroughs--; // only when through solids.
		}

		// Increase next position
		vecSrc = tr.vecEndPos + vecDir;
		pentIgnore = ENT(pEntity->pev);
	}

	if (!FBitSet(m_pPlayer->pev->flags, FL_ONGROUND))
		m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * 500;
#endif
}

void CDualRailgun::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	ResetEmptySound( );

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	SendWeaponAnim( DUAL_RAILGUN_IDLE );
}

void CDualRailgun::ProvideSingleItem(CBasePlayer *pPlayer, const char *item) {
	if (item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	if (!stricmp(item, "weapon_dual_railgun")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_railgun")) {
			pPlayer->GiveNamedItem("weapon_railgun");
			pPlayer->SelectItem("weapon_dual_railgun");
		}
	}
#endif
}

void CDualRailgun::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_railgun");
}
