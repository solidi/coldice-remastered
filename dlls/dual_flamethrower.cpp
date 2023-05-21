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

enum dual_flamethrower_e
{
	DUAL_FLAMETHROWER_IDLE1,
	DUAL_FLAMETHROWER_FIDGET,
	DUAL_FLAMETHROWER_ON,
	DUAL_FLAMETHROWER_CYCLE,
	DUAL_FLAMETHROWER_OFF,
	DUAL_FLAMETHROWER_FIRE1,
	DUAL_FLAMETHROWER_FIRE2,
	DUAL_FLAMETHROWER_FIRE3,
	DUAL_FLAMETHROWER_FIRE4,
	DUAL_FLAMETHROWER_DEPLOY_LOWKEY,
	DUAL_FLAMETHROWER_DEPLOY,
	DUAL_FLAMETHROWER_HOLSTER,
};

#ifdef DUALFLAMETHROWER
LINK_ENTITY_TO_CLASS( weapon_dual_flamethrower, CDualFlameThrower );
#endif

void CDualFlameThrower::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_dual_flamethrower");
	Precache( );
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_DUAL_FLAMETHROWER - 1;
	m_iId = WEAPON_DUAL_FLAMETHROWER;

	m_iDefaultAmmo = FLAMETHROWER_DEFAULT_GIVE * 2;

	FallInit();
}

void CDualFlameThrower::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_flamethrower.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_dual_flamethrower.mdl");

	PRECACHE_MODEL("sprites/flamesteam.spr");
	PRECACHE_MODEL("sprites/null.spr");

	// Particles
	PRECACHE_MODEL ("sprites/flameline.spr");
	PRECACHE_MODEL ("sprites/blacksmoke.spr");
	PRECACHE_MODEL ("sprites/black_smoke1.spr");
    PRECACHE_MODEL ("sprites/flamepart1.spr");
	PRECACHE_MODEL ("sprites/flamepart3.spr");
	PRECACHE_MODEL ("sprites/flamepart9.spr");
	PRECACHE_MODEL ("sprites/flamepart11.spr");
	PRECACHE_MODEL ("sprites/flamepart12.spr");
	PRECACHE_MODEL ("sprites/flamepart13.spr");

	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("flamethrower_reload.wav");
	PRECACHE_SOUND("flame_hitwall.wav");
	PRECACHE_SOUND("items/clipinsert1.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");
	PRECACHE_SOUND("flamethrower.wav");
	PRECACHE_SOUND("flamethrowerend.wav");

	PRECACHE_SOUND("weapons/357_cock1.wav");

	m_usFlameStream = PRECACHE_EVENT ( 1, "events/dual_flamestream.sc" );
	m_usFlameThrower = PRECACHE_EVENT ( 1, "events/dual_flamethrower.sc" );
	m_usFlameThrowerEnd = PRECACHE_EVENT ( 1, "events/dual_flamethrowerend.sc" );

	UTIL_PrecacheOther("flamestream");
	UTIL_PrecacheOther("flameball");
}

int CDualFlameThrower::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = FUEL_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = FLAMETHROWER_MAX_CLIP;
	p->iSlot = 5;
	p->iPosition = 7;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_DUAL_FLAMETHROWER;
	p->iWeight = FLAMETHROWER_WEIGHT * 2;
	p->pszDisplayName = "Viserion's Flamethrowers";

	return 1;
}

int CDualFlameThrower::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

BOOL CDualFlameThrower::Deploy( )
{
	m_fireState = 0;
	m_pPlayer->pev->playerclass = 0;
	return DefaultDeploy( "models/v_dual_flamethrower.mdl", "models/p_dual_flamethrower.mdl", DUAL_FLAMETHROWER_DEPLOY, "dual_egon" );
}

BOOL CDualFlameThrower::DeployLowKey( )
{
	m_fireState = 0;
	m_pPlayer->pev->playerclass = 0;
	return DefaultDeploy( "models/v_dual_flamethrower.mdl", "models/p_dual_flamethrower.mdl", DUAL_FLAMETHROWER_DEPLOY_LOWKEY, "dual_egon" );
}

void CDualFlameThrower::Holster( int skiplocal )
{
	m_pPlayer->pev->playerclass = 0;
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( DUAL_FLAMETHROWER_HOLSTER );
}

void CDualFlameThrower::Fire( void )
{
	if ( gpGlobals->time >= m_flAmmoUseTime )
	{
		Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
		UTIL_MakeVectors( anglesAim );
		Vector start = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_right * 1.2 + gpGlobals->v_forward * 16 + gpGlobals->v_up * -4;

#ifndef CLIENT_DLL
		CFlame *Flame = CFlame::CreateFlameStream( m_pPlayer->pev, start, gpGlobals->v_forward * 1000, gSkillData.plrDmgFlameThrower / 5 );

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

void CDualFlameThrower::EndAttack( void )
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

void CDualFlameThrower::UseAmmo( int count )
{
	m_pPlayer->m_fWeapon = FALSE;
	if (!m_pPlayer->pev->playerclass)
		m_pPlayer->pev->playerclass = 1;

	if ( m_iClip >= count )
		 m_iClip -= count;
	else
		 m_iClip = 0;
}

void CDualFlameThrower::PrimaryAttack( void )
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

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming = gpGlobals->v_forward;
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
			if (m_iClip <= 0)
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

			if (m_iClip <= 0)
			{
				EndAttack();
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.0;
				m_pPlayer->pev->playerclass = 0;
			}
		}
		break;
	}
}

void CDualFlameThrower::SecondaryAttack( void )
{
	m_pPlayer->pev->playerclass = 0;

	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	if (m_iClip <= 0)
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

	m_iClip--;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

#ifndef CLIENT_DLL
	CFlameBall::ShootFlameBall( m_pPlayer->pev, 
		m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 32 + gpGlobals->v_right * 16 + gpGlobals->v_up * -4,
		gpGlobals->v_forward * 900 );

	m_fSecondaryFireTime = gpGlobals->time + 0.15;
#endif

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5;

	m_pPlayer->pev->punchangle.x -= 1;
}

void CDualFlameThrower::Reload( void )
{
	int iResult = 0;
	m_pPlayer->pev->playerclass = 0;

	if ( m_iClip == FLAMETHROWER_MAX_CLIP )
	{
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
		return;
	}

	if (m_iClip == 0)
	{
		iResult = DefaultReload( FLAMETHROWER_MAX_CLIP, DUAL_FLAMETHROWER_OFF, 4.3 );
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "flamethrower_reload.wav", 1, ATTN_NORM);
	}

	if (iResult)
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT ( 10, 15 );
	}
}

void CDualFlameThrower::WeaponIdle( void )
{
	ResetEmptySound();

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

#ifndef CLIENT_DLL
	if (m_fSecondaryFireTime && m_fSecondaryFireTime <= gpGlobals->time)
	{
		UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
		CFlameBall::ShootFlameBall( m_pPlayer->pev, 
			m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 32 + gpGlobals->v_right * -16 + gpGlobals->v_up * -4,
			gpGlobals->v_forward * 900 );
		m_fSecondaryFireTime = 0;
	}
#endif

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	m_pPlayer->pev->playerclass = 0;

	if ( m_fireState != 0 )
		 EndAttack();

	int iAnim;
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		 iAnim = DUAL_FLAMETHROWER_IDLE1;
		 break;

	default:
		 iAnim = DUAL_FLAMETHROWER_FIDGET;
		 break;
	}

	SendWeaponAnim( iAnim );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT ( 10, 15 );
}

void CDualFlameThrower::ProvideSingleItem(CBasePlayer *pPlayer, const char *item) {
	if (item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	if (!stricmp(item, "weapon_dual_flamethrower")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_flamethrower")) {
			ALERT(at_aiconsole, "Give weapon_flamethrower!\n");
			pPlayer->GiveNamedItem("weapon_flamethrower");
			pPlayer->SelectItem("weapon_dual_flamethrower");
		}
	}
#endif
}

void CDualFlameThrower::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_flamethrower");
}
