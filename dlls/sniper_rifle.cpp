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
#include "gamerules.h"

enum rifle_e
{
	RIFLE_DRAW = 0,
	RIFLE_IDLE,
	RIFLE_SHOOT,
	RIFLE_SHOOT_EMPTY,
	RIFLE_RELOAD,
	RIFLE_HOLSTER,
	RIFLE_ZOOM_IN,
	RIFLE_ZOOM_OUT
};

#ifdef SNIPER_RIFLE
LINK_ENTITY_TO_CLASS( weapon_sniperrifle, CSniperRifle );
#endif

void CSniperRifle::Spawn( )
{
	Precache( );
	SET_MODEL(ENT(pev), "models/w_sniperrifle.mdl");
	m_iId = WEAPON_SNIPER_RIFLE;

	m_iDefaultAmmo = MP5_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}

void CSniperRifle::Precache( void )
{
	PRECACHE_MODEL("models/v_sniperrifle.mdl");
	PRECACHE_MODEL("models/w_sniperrifle.mdl");
	PRECACHE_MODEL("models/p_sniperrifle.mdl");

	m_iShell = PRECACHE_MODEL("models/w_762shell.mdl");// brass shellTE_MODEL

	PRECACHE_SOUND ("rifle1.wav");

	m_usSniperRifle = PRECACHE_EVENT( 1, "events/sniper_rifle.sc" );
}

int CSniperRifle::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "357";
	p->iMaxAmmo1 = _357_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = PYTHON_MAX_CLIP;
	p->iFlags = 0;
	p->iSlot = 2;
	p->iPosition = 4;
	p->iId = m_iId = WEAPON_SNIPER_RIFLE;
	p->iWeight = PYTHON_WEIGHT;
	p->pszDisplayName = "7.65mm Sniper Rifle";

	return 1;
}

int CSniperRifle::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

BOOL CSniperRifle::Deploy( )
{
	return DefaultDeploy( "models/v_sniperrifle.mdl", "models/p_sniperrifle.mdl", RIFLE_DRAW, "rpg" );
}

void CSniperRifle::Holster( int skiplocal )
{
	if ( m_pPlayer->pev->fov != 0 )
	{
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;
	}

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	SendWeaponAnim( RIFLE_HOLSTER );
}

void CSniperRifle::PrimaryAttack()
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
		PlayEmptySound();
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir;

	vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, 8192, BULLET_PLAYER_RIFLE, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

  int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usSniperRifle, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = GetNextAttackDelay(1.25);

	if ( m_flNextPrimaryAttack < UTIL_WeaponTimeBase() )
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CSniperRifle::SecondaryAttack( void )
{
#ifdef CLIENT_DLL
	if ( !bIsMultiplayer() )
#else
	if ( !g_pGameRules->IsMultiplayer() )
#endif
	{
		return;
	}

	if ( m_pPlayer->pev->fov != 0 )
	{
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;  // 0 means reset to default fov
		SendWeaponAnim( RIFLE_ZOOM_OUT );
	}
	else if ( m_pPlayer->pev->fov != 40 )
	{
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 40;
		SendWeaponAnim( RIFLE_ZOOM_IN );
	}

	m_flNextSecondaryAttack = 0.5;
}

void CSniperRifle::Reload( void )
{
	if ( m_pPlayer->ammo_9mm <= 0 )
		return;

	DefaultReload( MP5_MAX_CLIP, RIFLE_RELOAD, 1.5 );
}

void CSniperRifle::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:	
		iAnim = RIFLE_IDLE;
		break;
	
	default:
	case 1:
		iAnim = RIFLE_IDLE;
		break;
	}

	SendWeaponAnim( iAnim );

	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 ); // how long till we do this again.
}
