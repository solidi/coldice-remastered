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

enum dual_usas_e
{	
	DUAL_USAS_IDLE = 0,
	DUAL_USAS_FIRE_BOTH1,
	DUAL_USAS_RELOAD,
	DUAL_USAS_DEPLOY_LOWKEY,
	DUAL_USAS_DEPLOY,
	DUAL_USAS_HOLSTER,
	DUAL_USAS_FIRE_LEFT,
	DUAL_USAS_FIRE_RIGHT
};

#ifdef DUALUSAS
LINK_ENTITY_TO_CLASS( weapon_dual_usas, CDualUsas );
#endif

void CDualUsas::Spawn( )
{
	Precache( );
	m_iId = WEAPON_DUAL_USAS;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_DUAL_USAS - 1;

	m_iDefaultAmmo = USAS_DEFAULT_GIVE * 2;

	FallInit();// get ready to fall
}

void CDualUsas::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_usas.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND ("usas_fire.wav");

	PRECACHE_SOUND ("weapons/reload1.wav");	// shotgun reload
	PRECACHE_SOUND ("weapons/reload3.wav");	// shotgun reload

	PRECACHE_SOUND ("weapons/357_cock1.wav"); // gun empty sound
	PRECACHE_SOUND ("weapons/scock1.wav");	// cock gun

	m_usSingleFire = PRECACHE_EVENT( 1, "events/dual_usas.sc" );
	m_usDualFire = PRECACHE_EVENT( 1, "events/dual_usas_both.sc" );
}

int CDualUsas::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CDualUsas::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = BUCKSHOT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = USAS_MAX_CLIP * 2;
	p->iSlot = 5;
	p->iPosition = 5;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_DUAL_USAS;
	p->iWeight = USAS_WEIGHT * 2;
	p->pszDisplayName = "Dual USAS-12 Auto Shotguns";

	return 1;
}

BOOL CDualUsas::DeployLowKey( )
{
	return DefaultDeploy( "models/v_dual_usas.mdl", "models/p_weapons.mdl", DUAL_USAS_DEPLOY_LOWKEY, "dual_shotgun" );
}

BOOL CDualUsas::Deploy( )
{
	return DefaultDeploy( "models/v_dual_usas.mdl", "models/p_weapons.mdl", DUAL_USAS_DEPLOY, "dual_shotgun" );
}

void CDualUsas::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(DUAL_USAS_HOLSTER);
}

void CDualUsas::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_iClip <= 0)
	{
		Reload( );
		if (m_iClip == 0)
			PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;

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
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 4, vecSrc, vecAiming, VECTOR_CONE_DM_SHOTGUN, 2048, BULLET_PLAYER_BUCKSHOT, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usSingleFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, m_iAltFire++, 0 );
	if (m_iAltFire > 1) m_iAltFire = 0;

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = GetNextAttackDelay(0.25);
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
	if (m_iClip != 0)
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
	else
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75;
	m_fInSpecialReload = 0;
}

void CDualUsas::SecondaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_iClip <= 1)
	{
		Reload( );
		if (m_iClip == 0)
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
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 8, vecSrc, vecAiming, VECTOR_CONE_DM_SHOTGUN, 2048, BULLET_PLAYER_BUCKSHOT, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usDualFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = GetNextAttackDelay(0.25);
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
	if (m_iClip != 0)
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
	else
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75;
	m_fInSpecialReload = 0;
}

void CDualUsas::Reload( void )
{
	if ( m_pPlayer->ammo_buckshot <= 0 )
		return;

	DefaultReload( USAS_MAX_CLIP * 2, DUAL_USAS_RELOAD, 2.8 );
}

void CDualUsas::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	int iAnim;
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		iAnim = DUAL_USAS_IDLE;
		break;

	default:
	case 1:
		iAnim = DUAL_USAS_IDLE;
		break;
	}

	SendWeaponAnim( iAnim );

	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 ); // how long till we do this again.
}

void CDualUsas::ProvideSingleItem(CBasePlayer *pPlayer, const char *item) {
	if (item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	if (!stricmp(item, "weapon_dual_usas")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_usas")) {
			pPlayer->GiveNamedItem("weapon_usas");
			pPlayer->SelectItem("weapon_dual_usas");
		}
	}
#endif
}

void CDualUsas::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_usas");
}
