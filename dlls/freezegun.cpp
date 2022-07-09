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
#include "plasma.h"
#include "game.h"

enum freezegun_e {
	FREEZEGUN_IDLE,
	FREEZEGUN_RELOAD,
	FREEZEGUN_DRAW_LOWKEY,
	FREEZEGUN_DRAW,
	FREEZEGUN_HOLSTER,
	FREEZEGUN_SHOOT1,
	FREEZEGUN_SHOOT2,
	FREEZEGUN_SHOOT3,
};

#ifdef FREEZEGUN
LINK_ENTITY_TO_CLASS(weapon_freezegun, CFreezeGun);
#endif

void CFreezeGun::Spawn()
{
	Precache();
	m_iId = WEAPON_FREEZEGUN;
	SET_MODEL(ENT(pev), "models/w_freezegun.mdl");

	m_iDefaultAmmo = FREEZEGUN_DEFAULT_GIVE;

	FallInit();// get ready to fall
}

void CFreezeGun::Precache(void)
{
	PRECACHE_MODEL("models/v_freezegun.mdl");
	PRECACHE_MODEL("models/w_freezegun.mdl");
	PRECACHE_MODEL("models/p_freezegun.mdl");

	PRECACHE_SOUND("freezegun_fire.wav");
	PRECACHE_SOUND("freezegun_spin.wav");

	PRECACHE_SOUND("items/9mmclip1.wav");

	m_iPlasmaSprite = PRECACHE_MODEL("sprites/tsplasma.spr");
	m_iMuzzlePlasma = PRECACHE_MODEL("sprites/muzzleflashplasma.spr");
	m_iIceMuzzlePlasma = PRECACHE_MODEL("sprites/ice_muzzleflashplasma.spr");
	UTIL_PrecacheOther("plasma");

	m_usPlasmaFire = PRECACHE_EVENT(1, "events/freezegun.sc");
}

int CFreezeGun::AddToPlayer(CBasePlayer *pPlayer)
{
	if (CBasePlayerWeapon::AddToPlayer(pPlayer))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
			WRITE_BYTE(m_iId);
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

int CFreezeGun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = FREEZEGUN_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 7;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_FREEZEGUN;
	p->iWeight = FREEZEGUN_WEIGHT;
	p->pszDisplayName = "Xero n2o Freeze Gun";

	return 1;
}

BOOL CFreezeGun::DeployLowKey()
{
	return DefaultDeploy( "models/v_freezegun.mdl", "models/p_freezegun.mdl", FREEZEGUN_DRAW_LOWKEY, "egon", 0.6 );
}

BOOL CFreezeGun::Deploy()
{
	return DefaultDeploy( "models/v_freezegun.mdl", "models/p_freezegun.mdl", FREEZEGUN_DRAW, "egon", 0.6 );
}

void CFreezeGun::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	m_fInAttack = 0;
	SendWeaponAnim( FREEZEGUN_HOLSTER );
}

void CFreezeGun::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = GetNextAttackDelay(0.12);
		return;
	}

	if (m_iClip <= 0)
	{
		Reload();
		if (m_iClip == 0)
			PlayEmptySound();
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

	// m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;
	// EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "freezegun_fire.wav", 1.0, ATTN_NORM );

	m_pPlayer->SetAnimation(PLAYER_ATTACK1);
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -8;

#ifndef CLIENT_DLL	
	CPlasma *pPlasma = CPlasma::CreatePlasmaRocket( vecSrc, m_pPlayer->pev->v_angle, m_pPlayer );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	pPlasma->pev->velocity = pPlasma->pev->velocity + gpGlobals->v_forward * 2000;
	
	Vector vecSrcMuzzle = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 50 + gpGlobals->v_right * 8 + gpGlobals->v_up * -8;

	TraceResult tr;
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecSrcMuzzle );
		WRITE_BYTE( TE_SPRITE );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( vecSrcMuzzle.x );	// Send to PAS because of the sound
		WRITE_COORD( vecSrcMuzzle.y );
		WRITE_COORD( vecSrcMuzzle.z );
		if (icesprites.value)
			WRITE_SHORT( m_iIceMuzzlePlasma );
		else
			WRITE_SHORT( m_iMuzzlePlasma );
		WRITE_BYTE( 3 ); // scale * 10
		WRITE_BYTE( 128 ); // framerate
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrcMuzzle  );
		WRITE_BYTE(TE_DLIGHT);
		WRITE_COORD( vecSrcMuzzle.x );	// X
		WRITE_COORD( vecSrcMuzzle.y );	// Y
		WRITE_COORD( vecSrcMuzzle.z );	// Z
		WRITE_BYTE( 15 );		// radius * 0.1
		if (icesprites.value) {
			WRITE_BYTE( 0 );		// r
			WRITE_BYTE( 113 );		// g
			WRITE_BYTE( 230 );		// b
		} else {
			WRITE_BYTE( 0 );		// r
			WRITE_BYTE( 200 );		// g
			WRITE_BYTE( 0 );		// b
		}
		WRITE_BYTE( 5 );		// time * 10
		WRITE_BYTE( 10 );		// decay * 0.1
	MESSAGE_END( );
#endif

	// SendWeaponAnim(FREEZEGUN_SHOOT1);

	PLAYBACK_EVENT_FULL(
		flags,
		m_pPlayer->edict(), 
		m_usPlasmaFire, 
		0.0, 
		(float *)&g_vecZero, 
		(float *)&g_vecZero, 
		vecSrc.x,
		vecSrc.y,
		*(int*)&vecSrc.z,
		m_iPlasmaSprite, 
		0,
		TRUE);

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = GetNextAttackDelay(0.25);
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.25;
}

void CFreezeGun::WeaponIdle( void )
{
	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	if (m_fInAttack == 1)
	{
		m_fInAttack = 0;
		return;
	}

	SendWeaponAnim( FREEZEGUN_IDLE );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CFreezeGun::Reload( void )
{
	if ( m_pPlayer->ammo_uranium <= 0 )
		return;

	DefaultReload( FREEZEGUN_MAX_CLIP, FREEZEGUN_RELOAD, 3.5 );
}
