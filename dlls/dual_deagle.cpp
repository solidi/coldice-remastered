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

enum dual_deagle_e {
	DUAL_DEAGLE_IDLE,
	DUAL_DEAGLE_FIRE_LEFT,
	DUAL_DEAGLE_FIRE_RIGHT,
	DUAL_DEAGLE_FIRE_LAST_LEFT,
	DUAL_DEAGLE_FIRE_LAST_RIGHT,
	DUAL_DEAGLE_RELOAD,
	DUAL_DEAGLE_DEPLOY_LOWKEY,
	DUAL_DEAGLE_DEPLOY,
	DUAL_DEAGLE_HOLSTER,
	DUAL_DEAGLE_FIRE_BOTH,
};

#ifdef DUALDEAGLE
LINK_ENTITY_TO_CLASS( weapon_dual_deagle, CDualDeagle );
#endif

int CDualDeagle::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "357";
	p->iMaxAmmo1 = _357_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = DEAGLE_MAX_CLIP * 2;
	p->iFlags = 0;
	p->iSlot = 5;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_DUAL_DEAGLE;
	p->iWeight = DEAGLE_WEIGHT * 2;
	p->pszDisplayName = "Dual Desert Eagles";

	return 1;
}

int CDualDeagle::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

void CDualDeagle::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_dual_deagle"); // hack to allow for old names
	Precache( );
	m_iId = WEAPON_DUAL_DEAGLE;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_DUAL_DEAGLE - 1;

	m_iDefaultAmmo = DEAGLE_DEFAULT_GIVE * 2;
	pev->dmg = gSkillData.plrDmg357;

	FallInit();// get ready to fall down.
}

void CDualDeagle::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_deagle.mdl");

	PRECACHE_SOUND("weapons/357_cock1.wav");

	PRECACHE_SOUND ("deagle_fire.wav");

	m_usFireDeagle = PRECACHE_EVENT( 1, "events/dual_deagle.sc" );
	m_usFireDeagleBoth = PRECACHE_EVENT( 1, "events/dual_deagle_both.sc" );
}

BOOL CDualDeagle::DeployLowKey( )
{
	return DefaultDeploy( "models/v_dual_deagle.mdl", "models/p_weapons.mdl", DUAL_DEAGLE_DEPLOY_LOWKEY, "akimbo", UseDecrement(), pev->body );
}

BOOL CDualDeagle::Deploy( )
{
	return DefaultDeploy( "models/v_dual_deagle.mdl", "models/p_weapons.mdl", DUAL_DEAGLE_DEPLOY, "akimbo", UseDecrement(), pev->body );
}

void CDualDeagle::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(DUAL_DEAGLE_HOLSTER);
}

void CDualDeagle::PrimaryAttack()
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
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, 8192, BULLET_PLAYER_357, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFireDeagle, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, m_iAltFire++, m_iClip < 2 );
	if (m_iAltFire > 1) m_iAltFire = 0;

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = 0.3;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CDualDeagle::SecondaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	if (m_iClip <= 1)
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

	m_iClip -= 2;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 2, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, 8192, BULLET_PLAYER_357, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

    int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFireDeagleBoth, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, m_iClip, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = 0.3;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CDualDeagle::Reload( void )
{
	if ( m_pPlayer->ammo_357 <= 0 )
		return;

	DefaultReload( DEAGLE_MAX_CLIP * 2, DUAL_DEAGLE_RELOAD, 2.5, 0 );
}

void CDualDeagle::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
	if (flRand <= 0.5)
	{
		iAnim = DUAL_DEAGLE_IDLE;
		m_flTimeWeaponIdle = (70.0/30.0);
	}
	else if (flRand <= 0.7)
	{
		iAnim = DUAL_DEAGLE_IDLE;
		m_flTimeWeaponIdle = (60.0/30.0);
	}
	else if (flRand <= 0.9)
	{
		iAnim = DUAL_DEAGLE_IDLE;
		m_flTimeWeaponIdle = (88.0/30.0);
	}
	else
	{
		iAnim = DUAL_DEAGLE_IDLE;
		m_flTimeWeaponIdle = (170.0/30.0);
	}
	
	SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0, 0 );
}

void CDualDeagle::ProvideSingleItem(CBasePlayer *pPlayer, const char *item) {
	if (item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	if (!stricmp(item, "weapon_dual_deagle")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_deagle")) {
			pPlayer->GiveNamedItem("weapon_deagle");
			pPlayer->SelectItem("weapon_dual_deagle");
		}
	}
#endif
}

void CDualDeagle::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_deagle");
}
