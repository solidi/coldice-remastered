/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology"). Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*	Use, distribution, and modification of this source code and/or resulting
*	object code is restricted to non-commercial enhancements to products from
*	Valve LLC. All other use, distribution, or modification is prohibited
*	without written permission from Valve LLC.
*
****/

/*

		Entry for Half-Life Mod - FlatLine Arena
		Copyright (c) 2005-2019, Napoleon. All rights reserved.
		
===== flamethrower.cpp ========================================================

		Contains the code for weapon flamethrower
		Based on code by Ghoul BB and Valve (gargantua monster)
		Modified Valve code. Additional code by Napoleon. 

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"
#include "flame.h"
#include "flameball.h"
#include "game.h"

enum flamethrower_e
{
	FLAMETHROWER_IDLE1,
	FLAMETHROWER_FIDGET,
	FLAMETHROWER_ON,
	FLAMETHROWER_CYCLE,
	FLAMETHROWER_OFF,
	FLAMETHROWER_FIRE1,
	FLAMETHROWER_FIRE2,
	FLAMETHROWER_FIRE3,
	FLAMETHROWER_FIRE4,
	FLAMETHROWER_DEPLOY_LOWKEY,
	FLAMETHROWER_DEPLOY,
	FLAMETHROWER_HOLSTER,
};

#ifdef FLAMETHROWER
LINK_ENTITY_TO_CLASS( weapon_flamethrower, CFlameThrower );
#endif

void CFlameThrower::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_flamethrower");
	Precache( );
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_FLAMETHROWER - 1;
	m_iId = WEAPON_FLAMETHROWER;

	m_iDefaultAmmo = FLAMETHROWER_DEFAULT_GIVE;
	pev->dmg = gSkillData.plrDmgFlameThrower;

	FallInit();
}

void CFlameThrower::Precache( void )
{
	PRECACHE_MODEL("models/v_flamethrower.mdl");

	PRECACHE_MODEL("sprites/flamesteam.spr");
	PRECACHE_MODEL("sprites/null.spr");

	PRECACHE_SOUND("flame_hitwall.wav");
	PRECACHE_SOUND("flamethrower.wav");
	PRECACHE_SOUND("flamethrowerend.wav");

	m_usFlameStream = PRECACHE_EVENT ( 1, "events/flamestream.sc" );
	m_usFlameThrower = PRECACHE_EVENT ( 1, "events/flamethrower.sc" );
	m_usFlameThrowerEnd = PRECACHE_EVENT ( 1, "events/flamethrowerend.sc" );

	UTIL_PrecacheOther("flamestream");
	UTIL_PrecacheOther("flameball");
}

int CFlameThrower::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = FUEL_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 7;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_FLAMETHROWER;
	p->iWeight = FLAMETHROWER_WEIGHT;
	p->pszDisplayName = "Napoleon's Flamethrower";

	return 1;
}

int CFlameThrower::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

BOOL CFlameThrower::Deploy( )
{
	m_fireState = 0;
	m_pPlayer->pev->playerclass = 0;
	return DefaultDeploy( "models/v_flamethrower.mdl", "models/p_weapons.mdl", FLAMETHROWER_DEPLOY, "egon" );
}

BOOL CFlameThrower::DeployLowKey( )
{
	m_fireState = 0;
	m_pPlayer->pev->playerclass = 0;
	return DefaultDeploy( "models/v_flamethrower.mdl", "models/p_weapons.mdl", FLAMETHROWER_DEPLOY_LOWKEY, "egon" );
}

void CFlameThrower::Holster( int skiplocal )
{
	m_pPlayer->pev->playerclass = 0;
	CBasePlayerWeapon::DefaultHolster(FLAMETHROWER_HOLSTER);
}

void CFlameThrower::Fire( void )
{
	if ( gpGlobals->time >= m_flAmmoUseTime )
	{
		Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
		UTIL_MakeVectors( anglesAim );
		Vector start = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_right * 1.2 + gpGlobals->v_forward * 16 + gpGlobals->v_up * -4;

#ifndef CLIENT_DLL
		CFlame *Flame = CFlame::CreateFlameStream( m_pPlayer->pev, start, gpGlobals->v_forward * 1000, gSkillData.plrDmgFlameThrower / 10 );

		if (DangerSoundTime < gpGlobals->time)
		{
			CSoundEnt::InsertSound(bits_SOUND_DANGER, m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 250, 200, 0.5);
			CSoundEnt::InsertSound(bits_SOUND_DANGER, m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 100, 200, 0.5);
			DangerSoundTime = gpGlobals->time + 0.5;
		}

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_DLIGHT );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_BYTE( 20 );
			if (icesprites.value)
			{
				WRITE_BYTE( 0 );
				WRITE_BYTE( 113 );
				WRITE_BYTE( 230 );
			}
			else
			{
				WRITE_BYTE( 128 );
				WRITE_BYTE( 128 );
				WRITE_BYTE( 0 );
			}
			WRITE_BYTE( 5 );
			WRITE_BYTE( 10 );
		MESSAGE_END();
#endif
		if (m_fireMode == 0)
		{
			m_fireMode = 1;
			UseAmmo(1);
		}
		else
			m_fireMode = 0;

		m_flAmmoUseTime = gpGlobals->time + 0.05;
	}
}

void CFlameThrower::EndAttack( void )
{
	bool bMakeNoise = false;
		
	if ( m_fireState != 0 )
		 bMakeNoise = true;

	PLAYBACK_EVENT_FULL( FEV_GLOBAL | FEV_RELIABLE, m_pPlayer->edict(), m_usFlameThrowerEnd, 0,
		(float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles, 0.0, 0.0, bMakeNoise, 0, 0, 0 );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 4.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;

	m_fireState = 0;
	m_pPlayer->pev->playerclass = 0;
}

void CFlameThrower::UseAmmo( int count )
{
	m_pPlayer->m_fWeapon = FALSE;
	if (!m_pPlayer->pev->playerclass)
		m_pPlayer->pev->playerclass = 1;

	if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] >= count )
		 m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] -= count;
	else
		 m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] = 0;
}

void CFlameThrower::PrimaryAttack( void )
{
	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_fireState != 0 )
		{
			EndAttack();
		}
		else
		{
			PlayEmptySound();
		}
		return;
	}

	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecSrc = m_pPlayer->GetGunPosition();

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	switch( m_fireState )
	{
		case 0:
		{
			if (m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0)
			{
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
				PlayEmptySound( );
				return;
			}

			m_flAmmoUseTime = gpGlobals->time;
			
			PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFlameStream, 0.0,
				(float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0,  1, 1, 0 );
			m_pPlayer->m_iWeaponVolume = 450;
			sctime = gpGlobals->time + 0.1;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2;
			m_fireState = 1;
			m_pPlayer->pev->playerclass = 1;
		}
		break;

		case 1:
		{
			Fire();
			m_pPlayer->m_iWeaponVolume = 450;
		
			if (sctime <= gpGlobals->time)
			{
				PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFlameStream, 0,
					(float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 1, 1, 0, 0 );
				sctime = gpGlobals->time + 6.51;
			}

			if (m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0)
			{
				EndAttack();
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.0;
				m_pPlayer->pev->playerclass = 0;
			}
		}
		break;
	}
}

void CFlameThrower::SecondaryAttack( void )
{
	m_pPlayer->pev->playerclass = 0;

	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	if (m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usFlameThrower );

	m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ]--;

	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

#ifndef CLIENT_DLL
	CFlameBall::ShootFlameBall( m_pPlayer->pev, 
		m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + vecAiming * 32 + gpGlobals->v_right * 1.2 + gpGlobals->v_up * -4,
		vecAiming * 900 );
#endif

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5;

	m_pPlayer->pev->punchangle.x -= 1;
}

void CFlameThrower::WeaponIdle( void )
{
	m_pPlayer->pev->playerclass = 0;

	ResetEmptySound();

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if ( m_fireState != 0 )
		 EndAttack();

	int iAnim;
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		 iAnim = FLAMETHROWER_IDLE1;
		 break;

	default:
		 iAnim = FLAMETHROWER_FIDGET;
		 break;
	}

	SendWeaponAnim( iAnim );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT ( 10, 15 );
}

void CFlameThrower::ProvideDualItem(CBasePlayer *pPlayer, const char *item) {
	if (pPlayer == NULL || item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	CBasePlayerWeapon::ProvideDualItem(pPlayer, item);

	if (!stricmp(item, "weapon_flamethrower")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_flamethrower")) {
			pPlayer->GiveNamedItem("weapon_dual_flamethrower");
			pPlayer->SelectItem("weapon_dual_flamethrower");
		}
	}
#endif
}

void CFlameThrower::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_dual_flamethrower");
}
