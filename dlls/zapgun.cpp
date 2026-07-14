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

static const float ZAPGUN_PRIMARY_CYCLE = 0.30f;
static const float ZAPGUN_BURST_CYCLE = 0.62f;
static const float ZAPGUN_LASER_CYCLE = 0.55f;
static const float ZAPGUN_BURST_SHOT_GAP = 0.20f;
static const float ZAPGUN_UNDERWATER_LOCKOUT = 0.15f;
static const int ZAPGUN_BURST_SHOT_COUNT = 3;

enum zapgun_fire_event_e
{
	ZAPGUN_EVENT_PRIMARY = 0,
	ZAPGUN_EVENT_BURST
};

enum zapgun_e {
	ZAPGUN_AIM = 0,
	ZAPGUN_IDLE1,
	ZAPGUN_IDLE2,
	ZAPGUN_IDLE3,
	ZAPGUN_SHOOT,
	ZAPGUN_SHOOT_EMPTY,
	ZAPGUN_RELOAD,
	ZAPGUN_RELOAD_NOT_EMPTY,
	ZAPGUN_DRAW_LOWKEY,
	ZAPGUN_DRAW,
	ZAPGUN_HOLSTER,
	ZAPGUN_ADD_SILENCER
};

#ifdef ZAPGUN
LINK_ENTITY_TO_CLASS( weapon_zapgun, CZapgun );
#endif

void CZapgun::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_zapgun"); // hack to allow for old names
	Precache( );
	m_iId = WEAPON_ZAPGUN;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_ZAPGUN - 1;

	pev->dmg = 9999;
	m_iBurstShotsRemaining = 0;

	FallInit();// get ready to fall down.
}


void CZapgun::Precache( void )
{
	PRECACHE_MODEL("models/v_zapgun.mdl");
	PRECACHE_MODEL("sprites/xbeam1.spr");
	PRECACHE_MODEL("sprites/blueflare1.spr");

	PRECACHE_SOUND ("zapgun.wav");
	PRECACHE_SOUND("railgun_fire2.wav");

	m_usFireZapgun = PRECACHE_EVENT( 1, "events/zapgun.sc" );
	m_usFireZapgunLaser = PRECACHE_EVENT( 1, "events/zapgun_laser.sc" );
}

int CZapgun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CZapgun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 1;
	p->iPosition = 7;
	p->iFlags = ITEM_FLAG_NOAUTORELOAD;
	p->iId = m_iId = WEAPON_ZAPGUN;
	p->iWeight = ZAPGUN_WEIGHT;
	p->pszDisplayName = "Zapgun";

	return 1;
}

BOOL CZapgun::DeployLowKey( )
{
	return DefaultDeploy( "models/v_zapgun.mdl", "models/p_weapons.mdl", ZAPGUN_DRAW_LOWKEY, "onehanded", 0 );
}

BOOL CZapgun::Deploy( )
{
	return DefaultDeploy( "models/v_zapgun.mdl", "models/p_weapons.mdl", ZAPGUN_DRAW, "onehanded", 0 );
}

void CZapgun::Holster( int skiplocal )
{
	m_iBurstShotsRemaining = 0;
	pev->nextthink = -1;
	pev->body = 0;
	CBasePlayerWeapon::DefaultHolster(ZAPGUN_HOLSTER);
}

void CZapgun::PrimaryAttack( void )
{
	if ( BlockFireUnderwater( ZAPGUN_UNDERWATER_LOCKOUT ) )
		return;

	m_iBurstShotsRemaining = 0;
	pev->nextthink = -1;
	ZapFire( 0.03f, ZAPGUN_PRIMARY_CYCLE );
}

void CZapgun::SecondaryAttack( void )
{
	if ( BlockFireUnderwater( ZAPGUN_UNDERWATER_LOCKOUT ) )
		return;

	m_iBurstShotsRemaining = ZAPGUN_BURST_SHOT_COUNT - 1;
	FireZapProjectile( 0.04f, ZAPGUN_EVENT_BURST );

#ifdef CLIENT_DLL
	// Follow-up burst shots are server-think driven, so mirror delayed local playback
	// events to keep owner-side animation/audio in sync for each pew in the burst.
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireZapgun,
		ZAPGUN_BURST_SHOT_GAP, (float *)&g_vecZero, (float *)&g_vecZero,
		0, 0, 0, 0, ZAPGUN_EVENT_BURST, 0 );
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFireZapgun,
		ZAPGUN_BURST_SHOT_GAP * 2.0f, (float *)&g_vecZero, (float *)&g_vecZero,
		0, 0, 0, 0, ZAPGUN_EVENT_BURST, 0 );
#endif

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( ZAPGUN_BURST_CYCLE );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + ZAPGUN_BURST_CYCLE;

	if ( m_iBurstShotsRemaining > 0 )
	{
		SetThink( &CZapgun::BurstThink );
		pev->nextthink = gpGlobals->time + ZAPGUN_BURST_SHOT_GAP;
	}
}

void CZapgun::Reload( void )
{
	if ( !m_pPlayer )
		return;

	// Only fire the laser on an explicit reload press edge.
	if ( !(m_pPlayer->pev->button & IN_RELOAD) || !(m_pPlayer->m_afButtonPressed & IN_RELOAD) )
		return;

	if ( BlockFireUnderwater( ZAPGUN_UNDERWATER_LOCKOUT ) )
		return;

	m_iBurstShotsRemaining = 0;
	pev->nextthink = -1;
	FireZapLaser();
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( ZAPGUN_LASER_CYCLE );
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + ZAPGUN_LASER_CYCLE;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + ZAPGUN_LASER_CYCLE;
}

void CZapgun::ZapFire( float flSpread , float flCycleTime )
{
	FireZapProjectile( flSpread, ZAPGUN_EVENT_PRIMARY );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCycleTime);
}

void CZapgun::FireZapProjectile( float flSpread, int iShotMode )
{
	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	int flags;

#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	// player "shoot" animation
	SendWeaponAnim( ZAPGUN_SHOOT, UseDecrement() ? 1 : 0 );
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// non-silenced
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecDir = vecAiming;

	Vector spread = Vector( flSpread, flSpread, flSpread );
	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		spread = VECTOR_CONE_1DEGREES;
if ( spread.x > 0.0f )
{
	vecDir = vecDir + gpGlobals->v_right * RANDOM_FLOAT( -spread.x, spread.x ) + gpGlobals->v_up * RANDOM_FLOAT( -spread.y, spread.y );
	vecDir = vecDir.Normalize();
}

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFireZapgun, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, iShotMode, 0 );

#ifndef CLIENT_DLL
	CTracer::CreateTracer( vecSrc, vecDir, m_pPlayer, RANDOM_LONG(1200, 1500), 900, pev->classname );
#endif

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CZapgun::FireZapLaser( void )
{
	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = gpGlobals->v_forward;

#ifndef CLIENT_DLL
	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * 8192.0f, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );
	if ( tr.pHit && tr.pHit->v.takedamage )
	{
		ClearMultiDamage();
		CBaseEntity *ent = CBaseEntity::Instance( tr.pHit );
		if ( ent )
		{
			ent->TraceAttack( m_pPlayer->pev, 1, vecAiming, &tr, DMG_NEVERGIB | DMG_CONFUSE | DMG_ENERGYBEAM );
			ApplyMultiDamage( pev, m_pPlayer->pev );
		}
	}
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFireZapgunLaser, 0.0, (float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles, 0, 0, 0, 0, 0, 0 );
}

BOOL CZapgun::BlockFireUnderwater( float flLockout )
{
	if ( !m_pPlayer || m_pPlayer->pev->waterlevel != 3 )
		return FALSE;

	PlayEmptySound();
	m_iBurstShotsRemaining = 0;
	pev->nextthink = -1;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( flLockout );
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + flLockout;
	return TRUE;
}

void CZapgun::BurstThink( void )
{
	if ( !m_pPlayer || m_iBurstShotsRemaining <= 0 || m_pPlayer->m_pActiveItem != this )
	{
		m_iBurstShotsRemaining = 0;
		pev->nextthink = -1;
		return;
	}

	if ( BlockFireUnderwater( ZAPGUN_UNDERWATER_LOCKOUT ) )
		return;

	FireZapProjectile( 0.04f, ZAPGUN_EVENT_BURST );
	m_iBurstShotsRemaining--;

	if ( m_iBurstShotsRemaining > 0 )
	{
		SetThink( &CZapgun::BurstThink );
		pev->nextthink = gpGlobals->time + ZAPGUN_BURST_SHOT_GAP;
	}
	else
	{
		pev->nextthink = -1;
	}
}

void CZapgun::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.0, 1.0 );

	if (flRand <= 0.3 + 0 * 0.75)
	{
		iAnim = ZAPGUN_IDLE3;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 49.0 / 16;
	}
	else if (flRand <= 0.6 + 0 * 0.875)
	{
		iAnim = ZAPGUN_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 60.0 / 16.0;
	}
	else
	{
		iAnim = ZAPGUN_IDLE2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 40.0 / 16.0;
	}
	SendWeaponAnim( iAnim, 1 );
}
