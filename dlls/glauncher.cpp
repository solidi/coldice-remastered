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

enum glauncher_e
{
	GLAUNCHER_IDLE1 = 0,
	GLAUNCHER_IDLE2,
	GLAUNCHER_DRAW_LOWKEY,
	GLAUNCHER_DRAW,
	GLAUNCHER_HOLSTER,
	GLAUNCHER_RELOAD,
	GLAUNCHER_SHOOT,
};

#ifdef GLAUNCHER
LINK_ENTITY_TO_CLASS( weapon_glauncher, CGrenadeLauncher );
#endif

//=========================================================
//=========================================================

void CGrenadeLauncher::Spawn( )
{
	Precache();

	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_GLAUNCHER;

	m_iId = WEAPON_GLAUNCHER;
	m_iDefaultAmmo = GLAUNCHER_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}

void CGrenadeLauncher::Precache( void )
{
	PRECACHE_MODEL("models/v_glauncher.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_glauncher.mdl");

	PRECACHE_MODEL("models/w_contact_grenade.mdl");

	PRECACHE_SOUND( "weapons/glauncher.wav" );
	PRECACHE_SOUND( "weapons/glauncher2.wav" );
	PRECACHE_SOUND( "glauncher_reload.wav" );

	PRECACHE_SOUND("glauncher_bad.wav");

	PRECACHE_SOUND("weapons/357_cock1.wav");

	m_usGrenadeLauncher = PRECACHE_EVENT( 1, "events/glauncher.sc" );
}

int CGrenadeLauncher::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "ARgrenades";
	p->iMaxAmmo1 = M203_GRENADE_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = GLAUNCHER_MAX_CLIP;
	p->iSlot = 3;
	p->iPosition = 5;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLAUNCHER;
	p->iWeight = GLAUNCHER_WEIGHT;
	p->pszDisplayName = "120-Pound Grenade Launcher";

	return 1;
}

int CGrenadeLauncher::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

BOOL CGrenadeLauncher::DeployLowKey( )
{
	return DefaultDeploy( "models/v_glauncher.mdl", "models/p_glauncher.mdl", GLAUNCHER_DRAW_LOWKEY, "mp5" );
}

BOOL CGrenadeLauncher::Deploy( )
{
	return DefaultDeploy( "models/v_glauncher.mdl", "models/p_glauncher.mdl", GLAUNCHER_DRAW, "mp5" );
}

void CGrenadeLauncher::Holster( int skiplocal )
{
	CBasePlayerWeapon::DefaultHolster(GLAUNCHER_HOLSTER);
}

void CGrenadeLauncher::PrimaryAttack()
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
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_DANGER;
	m_pPlayer->m_flStopExtraSoundTime = UTIL_WeaponTimeBase() + 0.2;

	m_iClip--;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	// we don't add in player velocity anymore.
	CGrenade::ShootContact( m_pPlayer->pev, 
							m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + vecAiming * 16, 
							vecAiming * 800 );

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usGrenadeLauncher );
	
	m_flNextPrimaryAttack = GetNextAttackDelay(1.3);
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.3;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5;// idle pretty soon after shooting.

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
}

void CGrenadeLauncher::SecondaryAttack( void )
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
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_DANGER;
	m_pPlayer->m_flStopExtraSoundTime = UTIL_WeaponTimeBase() + 0.2;

	m_iClip--;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if (RANDOM_LONG(0,2) == 0) {
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_VOICE, "glauncher_bad.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));	
	}

	CGrenade::ShootTimedCluster(m_pPlayer->pev, 
							m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + vecAiming * 16, 
							vecAiming * 800, 6 );

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usGrenadeLauncher );
	
	m_flNextSecondaryAttack = GetNextAttackDelay(1.3);
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.3;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5;// idle pretty soon after shooting.

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
}

void CGrenadeLauncher::Reload( void )
{
	BOOL result = DefaultReload( GLAUNCHER_MAX_CLIP, GLAUNCHER_RELOAD, 1.5 );

	if (result) {
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "glauncher_reload.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
	}
}

void CGrenadeLauncher::WeaponIdle( void )
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
		iAnim = GLAUNCHER_IDLE1;
		break;
	
	default:
	case 1:
		iAnim = GLAUNCHER_IDLE2;
		break;
	}

	SendWeaponAnim( iAnim );

	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}
