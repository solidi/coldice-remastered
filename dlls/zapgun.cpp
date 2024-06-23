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

	//m_iDefaultAmmo = ZAPGUN_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void CZapgun::Precache( void )
{
	PRECACHE_MODEL("models/v_zapgun.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_weapons.mdl");

	PRECACHE_SOUND ("zapgun.wav");

	m_usFireZapgun = PRECACHE_EVENT( 1, "events/zapgun.sc" );
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
	p->iFlags = 0;
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
	pev->nextthink = -1;
	pev->body = 0;
	CBasePlayerWeapon::DefaultHolster(ZAPGUN_HOLSTER);
}

void CZapgun::PrimaryAttack( void )
{
	ZapFire( 0.03, 0.3 );
}

void CZapgun::ZapFire( float flSpread , float flCycleTime )
{
	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	int flags;

#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// non-silenced
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector spread = Vector( flSpread, flSpread, flSpread );
	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		spread = VECTOR_CONE_1DEGREES;

	//Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, spread, 8192, BULLET_PLAYER_9MM, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFireZapgun, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0, 0, 0, 0 );

#ifndef CLIENT_DLL
	CTracer::CreateTracer( vecSrc, vecAiming, m_pPlayer, RANDOM_LONG(1200, 1500), 900, pev->classname );
#endif

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCycleTime);

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
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
