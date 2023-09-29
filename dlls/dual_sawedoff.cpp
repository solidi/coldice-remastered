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
#include "gamerules.h"

// special deathmatch shotgun spreads
#define VECTOR_CONE_DM_SHOTGUN	Vector( 0.08716, 0.04362, 0.00  )// 10 degrees by 5 degrees
#define VECTOR_CONE_DM_DOUBLESHOTGUN Vector( 0.17365, 0.04362, 0.00 ) // 20 degrees by 5 degrees

enum dual_sawedoff_e {
	DUAL_SAWEDOFF_IDLE = 0,
	DUAL_SAWEDOFF_SHOOTBOTH,
	DUAL_SAWEDOFF_SHOOT_RIGHT,
	DUAL_SAWEDOFF_SHOOT_LEFT,
	DUAL_SAWEDOFF_DRAW_LOWKEY,
	DUAL_SAWEDOFF_DRAW,
	DUAL_SAWEDOFF_RELOAD,
	DUAL_SAWEDOFF_HOLSTER,
};

#ifdef DUALSAWEDOFF
LINK_ENTITY_TO_CLASS( weapon_dual_sawedoff, CDualSawedOff );
#endif

void CDualSawedOff::Spawn( )
{
	Precache( );
	m_iId = WEAPON_DUAL_SAWEDOFF;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_DUAL_SAWEDOFF - 1;

	m_iDefaultAmmo = SHOTGUN_DEFAULT_GIVE;

	FallInit();// get ready to fall
}

void CDualSawedOff::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_sawedoff.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_dual_sawedoff.mdl");

	m_iShell = PRECACHE_MODEL ("models/shotgunshell.mdl");// shotgun shell

	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND ("sawedoff.wav");

	PRECACHE_SOUND ("weapons/reload1.wav");	// shotgun reload
	PRECACHE_SOUND ("weapons/reload3.wav");	// shotgun reload
	
	PRECACHE_SOUND ("weapons/357_cock1.wav"); // gun empty sound
	PRECACHE_SOUND ("weapons/scock1.wav");	// cock gun

	m_usSingleFire = PRECACHE_EVENT( 1, "events/dualsawedoff1.sc" );
	m_usDoubleFire = PRECACHE_EVENT( 1, "events/dualsawedoff2.sc" );
}

int CDualSawedOff::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CDualSawedOff::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = BUCKSHOT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = SAWEDOFF_MAX_CLIP * 2;
	p->iSlot = 1;
	p->iPosition = 6;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_DUAL_SAWEDOFF;
	p->iWeight = SAWEDOFF_WEIGHT * 2;
	p->pszDisplayName = "Dual Sawed-Off Shotguns";

	return 1;
}

BOOL CDualSawedOff::DeployLowKey( )
{
	m_iAltFire = SAWEDOFF_MAX_CLIP * 2;
	return DefaultDeploy( "models/v_dual_sawedoff.mdl", "models/p_dual_sawedoff.mdl", DUAL_SAWEDOFF_DRAW_LOWKEY, "dual_shotgun" );
}

BOOL CDualSawedOff::Deploy( )
{
	m_iAltFire = SAWEDOFF_MAX_CLIP * 2;
	return DefaultDeploy( "models/v_dual_sawedoff.mdl", "models/p_dual_sawedoff.mdl", DUAL_SAWEDOFF_DRAW, "dual_shotgun" );
}

void CDualSawedOff::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(DUAL_SAWEDOFF_HOLSTER);
}

void CDualSawedOff::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_iClip <= 1)
	{
		Reload( );
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip -= 2;

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 16, vecSrc, vecAiming, VECTOR_CONE_DM_SHOTGUN, 2048, BULLET_PLAYER_BUCKSHOT, 1, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	m_iAltFire -= SAWEDOFF_MAX_CLIP;
	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usSingleFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, m_iAltFire, 0 );
	if (m_iAltFire <= 0) m_iAltFire = SAWEDOFF_MAX_CLIP * 2;

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
	if (m_iClip != 0)
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
	else
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75;
}

void CDualSawedOff::SecondaryAttack( void )
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_iClip <= 3)
	{
		Reload( );
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip -= 4;

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 32, vecSrc, vecAiming, VECTOR_CONE_DM_DOUBLESHOTGUN, 2048, BULLET_PLAYER_BUCKSHOT, 1, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usDoubleFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * 200;

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = GetNextAttackDelay(0.75);
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.75;
	if (m_iClip != 0)
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 6.0;
	else
		m_flTimeWeaponIdle = 1.5;
}

void CDualSawedOff::Reload( void )
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == (SAWEDOFF_MAX_CLIP * 2))
		return;

	// don't reload until recoil is done
	if (m_flNextPrimaryAttack > UTIL_WeaponTimeBase())
		return;

	DefaultReload( SAWEDOFF_MAX_CLIP * 2, DUAL_SAWEDOFF_RELOAD, 1.5 );
}

void CDualSawedOff::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	if (m_flTimeWeaponIdle <  UTIL_WeaponTimeBase() )
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
		if (flRand <= 0.8)
		{
			iAnim = DUAL_SAWEDOFF_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (60.0/12.0);// * RANDOM_LONG(2, 5);
		}
		else if (flRand <= 0.95)
		{
			iAnim = DUAL_SAWEDOFF_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (20.0/9.0);
		}
		else
		{
			iAnim = DUAL_SAWEDOFF_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (20.0/9.0);
		}
		SendWeaponAnim( iAnim );
	}
}

void CDualSawedOff::ProvideSingleItem(CBasePlayer *pPlayer, const char *item) {
	if (item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	if (!stricmp(item, "weapon_dual_sawedoff")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_sawedoff")) {
			ALERT(at_aiconsole, "Give weapon_sawedoff!\n");
			pPlayer->GiveNamedItem("weapon_sawedoff");
			pPlayer->SelectItem("weapon_dual_sawedoff");
		}
	}
#endif
}

void CDualSawedOff::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_dual_sawedoff");
}
