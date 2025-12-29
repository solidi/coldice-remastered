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

enum fingergun_e {
	FINGERGUN_IDLE = 0,
	FINGERGUN_IDLE_BOTH,
	FINGERGUN_SHOOT,
	FINGERGUN_SHOOT_BOTH,
	FINGERGUN_DRAW_LOWKEY,
	FINGERGUN_DRAW,
	FINGERGUN_DRAW_BOTH_LOWKEY,
	FINGERGUN_DRAW_BOTH,
	FINGERGUN_HOLSTER,
	FINGERGUN_HOLSTER_BOTH
};

#ifdef FINGERGUN
LINK_ENTITY_TO_CLASS( weapon_fingergun, CFingerGun );
#endif

int CFingerGun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iFlags = 0;
	p->iSlot = 0;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_FINGERGUN;
	p->iWeight = FINGERGUN_WEIGHT;
	p->pszDisplayName = "Fingergun";

	return 1;
}

int CFingerGun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

void CFingerGun::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_fingergun"); // hack to allow for old names
	Precache( );
	m_iId = WEAPON_FINGERGUN;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_CROWBAR - 1;

	FallInit();// get ready to fall down.
}


void CFingerGun::Precache( void )
{
	PRECACHE_MODEL("models/v_fingergun.mdl");

	PRECACHE_SOUND ("weapons/357_reload1.wav");
	PRECACHE_SOUND ("weapons/357_cock1.wav");
	PRECACHE_SOUND ("revolver_fire.wav");
	PRECACHE_SOUND ("weapons/357_shot1.wav");
	PRECACHE_SOUND ("weapons/357_shot2.wav");

	m_usFireFingergun = PRECACHE_EVENT( 1, "events/fingergun.sc" );
}

BOOL CFingerGun::DeployLowKey( )
{
	return DefaultDeploy( "models/v_fingergun.mdl", iStringNull, FINGERGUN_DRAW_BOTH_LOWKEY, "python" );
}

BOOL CFingerGun::Deploy( )
{
	return DefaultDeploy( "models/v_fingergun.mdl", iStringNull, FINGERGUN_DRAW_BOTH, "python" );
}

void CFingerGun::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(FINGERGUN_HOLSTER_BOTH);
}

void CFingerGun::SecondaryAttack( void )
{

}

void CFingerGun::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector spread = VECTOR_CONE_3DEGREES;
	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		spread = VECTOR_CONE_1DEGREES;

	int dmg = 0;
#ifndef CLIENT_DLL
	if (g_pGameRules->IsGunGame())
		dmg = gSkillData.plrDmg9MM;
#endif

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	TraceResult tr;
	UTIL_TraceLine(vecSrc, vecSrc + vecAiming * 8192, dont_ignore_monsters, m_pPlayer->edict(), &tr);

#ifndef CLIENT_DLL
	if ( tr.pHit && tr.pHit->v.takedamage )
	{
		ClearMultiDamage( );
		CBaseEntity *ent = CBaseEntity::Instance(tr.pHit);
		if (ent)
		{
			ent->TraceAttack(m_pPlayer->pev, dmg, vecAiming, &tr, DMG_NEVERGIB | DMG_CONFUSE ); 
			ApplyMultiDamage( pev, m_pPlayer->pev );
		}
	}
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFireFingergun, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecAiming.x, vecAiming.y, 0, 0, 0, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = 0.3;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CFingerGun::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
	if (flRand <= 0.5)
	{
		iAnim = FINGERGUN_IDLE_BOTH;
		m_flTimeWeaponIdle = (70.0/30.0);
	}
	else if (flRand <= 0.7)
	{
		iAnim = FINGERGUN_IDLE_BOTH;
		m_flTimeWeaponIdle = (60.0/30.0);
	}
	else if (flRand <= 0.9)
	{
		iAnim = FINGERGUN_IDLE_BOTH;
		m_flTimeWeaponIdle = (88.0/30.0);
	}
	else
	{
		iAnim = FINGERGUN_IDLE_BOTH;
		m_flTimeWeaponIdle = (170.0/30.0);
	}

	SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0 );
}
