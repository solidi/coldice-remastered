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
#include "weapons.h"
#include "monsters.h"
#include "player.h"
#include "gamerules.h"

enum dual_glock_e {
	DUAL_GLOCK_IDLE,
	DUAL_GLOCK_FIRE_LEFT,
	DUAL_GLOCK_FIRE_RIGHT,
	DUAL_GLOCK_FIRE_LAST_LEFT,
	DUAL_GLOCK_FIRE_LAST_RIGHT,
	DUAL_GLOCK_RELOAD,
	DUAL_GLOCK_DEPLOY_LOWKEY,
	DUAL_GLOCK_DEPLOY,
	DUAL_GLOCK_HOLSTER,
	DUAL_GLOCK_HOLSTER_BOND,
	DUAL_GLOCK_FIRE_BOTH,
};

#ifdef DUALGLOCK
LINK_ENTITY_TO_CLASS( weapon_dual_glock, CDualGlock );
#endif

int CDualGlock::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = GLOCK_MAX_CLIP * 2;
	p->iFlags = 0;
	p->iSlot = 5;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_DUAL_GLOCK;
	p->iWeight = GLOCK_WEIGHT * 2;
	p->pszDisplayName = "Dual Glock";

	return 1;
}

int CDualGlock::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

void CDualGlock::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_dual_glock"); // hack to allow for old names
	Precache( );
	m_iId = WEAPON_DUAL_GLOCK;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_DUAL_GLOCK - 1;

	m_iDefaultAmmo = GLOCK_DEFAULT_GIVE * 2;

	FallInit();// get ready to fall down.
}

void CDualGlock::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_handgun.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_weapons.mdl");

	PRECACHE_SOUND("weapons/357_cock1.wav");

	PRECACHE_SOUND ("deagle_fire.wav");

	m_usFireGlock = PRECACHE_EVENT( 1, "events/dual_glock.sc" );
}

BOOL CDualGlock::DeployLowKey( )
{
	return DefaultDeploy( "models/v_dual_handgun.mdl", "models/p_weapons.mdl", DUAL_GLOCK_DEPLOY_LOWKEY, "akimbo", UseDecrement(), m_chargeReady );
}

BOOL CDualGlock::Deploy( )
{
	m_chargeReady = 8;
	return DefaultDeploy( "models/v_dual_handgun.mdl", "models/p_weapons.mdl", DUAL_GLOCK_DEPLOY, "akimbo", UseDecrement(), m_chargeReady );
}

void CDualGlock::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(DUAL_GLOCK_HOLSTER, m_chargeReady);
}

void CDualGlock::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		if (!m_fFireOnEmpty)
			Reload( );
		else
		{
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM);
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, 8192, BULLET_PLAYER_9MM, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFireGlock, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, m_iAltFire++, m_chargeReady );
	if (m_iAltFire > 1) m_iAltFire = 0;

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = 0.3;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CDualGlock::SecondaryAttack()
{
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(2.65);
	SetThink( &CGlock::AddSilencer );
	pev->nextthink = gpGlobals->time + 2.5f;

	SendWeaponAnim( DUAL_GLOCK_HOLSTER, 0, m_chargeReady );

	if (m_chargeReady > 8) {
		m_chargeReady = 8;
	} else {
		m_chargeReady = 98;
	}
}

void CDualGlock::AddSilencers( void )
{
	SendWeaponAnim( DUAL_GLOCK_DEPLOY, 0, m_chargeReady );
}

void CDualGlock::Reload( void )
{
	if ( m_pPlayer->ammo_9mm <= 0 )
		return;

	DefaultReload( GLOCK_MAX_CLIP * 2, DUAL_GLOCK_RELOAD, 2.5, m_chargeReady );
}

void CDualGlock::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
	if (flRand <= 0.5)
	{
		iAnim = DUAL_GLOCK_IDLE;
		m_flTimeWeaponIdle = (70.0/30.0);
	}
	else if (flRand <= 0.7)
	{
		iAnim = DUAL_GLOCK_IDLE;
		m_flTimeWeaponIdle = (60.0/30.0);
	}
	else if (flRand <= 0.9)
	{
		iAnim = DUAL_GLOCK_IDLE;
		m_flTimeWeaponIdle = (88.0/30.0);
	}
	else
	{
		iAnim = DUAL_GLOCK_IDLE;
		m_flTimeWeaponIdle = (170.0/30.0);
	}
	
	SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0, m_chargeReady );
}

void CDualGlock::ProvideSingleItem(CBasePlayer *pPlayer, const char *item) {
	if (item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	if (!stricmp(item, "weapon_dual_glock")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_9mmhandgun")) {
			pPlayer->GiveNamedItem("weapon_9mmhandgun");
			pPlayer->SelectItem("weapon_dual_glock");
		}
	}
#endif
}

void CDualGlock::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_9mmhandgun");
}
