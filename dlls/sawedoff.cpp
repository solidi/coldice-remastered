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

enum sawedoff_e {
	SAWEDOFF_IDLE = 0,
	SAWEDOFF_IDLE2,
	SAWEDOFF_SHOOTLEFT,
	SAWEDOFF_SHOOTRIGHT,
	SAWEDOFF_SHOOTBOTH,
	SAWEDOFF_RELOAD,
	SAWEDOFF_DRAW_LOWKEY,
	SAWEDOFF_DRAW,
	SAWEDOFF_HOLSTER,
};

#ifdef SAWEDOFF
LINK_ENTITY_TO_CLASS( weapon_sawedoff, CSawedOff );
#endif

void CSawedOff::Spawn( )
{
	Precache( );
	m_iId = WEAPON_SAWEDOFF;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_SAWEDOFF - 1;

	m_iDefaultAmmo = SHOTGUN_DEFAULT_GIVE;

	FallInit();// get ready to fall
}

void CSawedOff::Precache( void )
{
	PRECACHE_MODEL("models/v_sawedoff.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_sawedoff.mdl");

	m_iShell = PRECACHE_MODEL ("models/shotgunshell.mdl");// shotgun shell

	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND ("sawedoff.wav");

	PRECACHE_SOUND ("weapons/reload1.wav");	// shotgun reload
	PRECACHE_SOUND ("weapons/reload3.wav");	// shotgun reload
	
	PRECACHE_SOUND ("weapons/357_cock1.wav"); // gun empty sound
	PRECACHE_SOUND ("weapons/scock1.wav");	// cock gun

	m_usSingleFire = PRECACHE_EVENT( 1, "events/sawedoff1.sc" );
	m_usDoubleFire = PRECACHE_EVENT( 1, "events/sawedoff2.sc" );
}

int CSawedOff::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CSawedOff::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = BUCKSHOT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = SAWEDOFF_MAX_CLIP;
	p->iSlot = 1;
	p->iPosition = 5;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SAWEDOFF;
	p->iWeight = SAWEDOFF_WEIGHT;
	p->pszDisplayName = "Sawed-Off Shotgun";

	return 1;
}

BOOL CSawedOff::DeployLowKey( )
{
	return DefaultDeploy( "models/v_sawedoff.mdl", "models/p_sawedoff.mdl", SAWEDOFF_DRAW_LOWKEY, "shotgun" );
}

BOOL CSawedOff::Deploy( )
{
	return DefaultDeploy( "models/v_sawedoff.mdl", "models/p_sawedoff.mdl", SAWEDOFF_DRAW, "shotgun" );
}

void CSawedOff::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(SAWEDOFF_HOLSTER);
}

void CSawedOff::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
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
	Vector vecDir;

#ifdef CLIENT_DLL
	if ( bIsMultiplayer() )
#else
	if ( g_pGameRules->IsMultiplayer() )
#endif
	{
		vecDir = m_pPlayer->FireBulletsPlayer( 8, vecSrc, vecAiming, VECTOR_CONE_DM_SHOTGUN, 2048, BULLET_PLAYER_BUCKSHOT, 1, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	}
	else
	{
		// regular old, untouched spread. 
		vecDir = m_pPlayer->FireBulletsPlayer( 12, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, 2048, BULLET_PLAYER_BUCKSHOT, 1, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	}

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usSingleFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, m_iAltFire++, 0 );
	if (m_iAltFire > 1) m_iAltFire = 0;

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

void CSawedOff::SecondaryAttack( void )
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

	Vector vecDir;
	
#ifdef CLIENT_DLL
	if ( bIsMultiplayer() )
#else
	if ( g_pGameRules->IsMultiplayer() )
#endif
	{
		// tuned for deathmatch
		vecDir = m_pPlayer->FireBulletsPlayer( 16, vecSrc, vecAiming, VECTOR_CONE_DM_DOUBLESHOTGUN, 2048, BULLET_PLAYER_BUCKSHOT, 1, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	}
	else
	{
		// untouched default single player
		vecDir = m_pPlayer->FireBulletsPlayer( 24, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, 2048, BULLET_PLAYER_BUCKSHOT, 1, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	}
		
	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usDoubleFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
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

void CSawedOff::Reload( void )
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == SAWEDOFF_MAX_CLIP)
		return;

	// don't reload until recoil is done
	if (m_flNextPrimaryAttack > UTIL_WeaponTimeBase())
		return;

	DefaultReload( SAWEDOFF_MAX_CLIP, SAWEDOFF_RELOAD, 1.9 );
}

void CSawedOff::WeaponIdle( void )
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
			iAnim = SAWEDOFF_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (60.0/12.0);// * RANDOM_LONG(2, 5);
		}
		else if (flRand <= 0.95)
		{
			iAnim = SAWEDOFF_IDLE2;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (20.0/9.0);
		}
		else
		{
			iAnim = SAWEDOFF_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (20.0/9.0);
		}
		SendWeaponAnim( iAnim );
	}
}

void CSawedOff::ProvideDualItem(CBasePlayer *pPlayer, const char *item) {
	if (pPlayer == NULL || item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	CBasePlayerWeapon::ProvideDualItem(pPlayer, item);

	if (!stricmp(item, "weapon_sawedoff")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_sawedoff")) {
#ifdef _DEBUG
			ALERT(at_aiconsole, "Give weapon_dual_sawedoff!\n");
#endif
			pPlayer->GiveNamedItem("weapon_dual_sawedoff");
			pPlayer->SelectItem("weapon_dual_sawedoff");
		}
	}
#endif
}

void CSawedOff::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_dual_sawedoff");
}
