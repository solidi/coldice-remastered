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

enum dual_glock_e {
	DUAL_GLOCK_IDLE,
	DUAL_GLOCK_FIRE_LEFT,
	DUAL_GLOCK_FIRE_RIGHT,
	DUAL_GLOCK_FIRE_LAST_LEFT,
	DUAL_GLOCK_FIRE_LAST_RIGHT,
	DUAL_GLOCK_RELOAD,
	DUAL_GLOCK_DEPLOY_LOWKEY,
	DUAL_GLOCK_DEPLOY,
	DUAL_GLOCK_HOLSTER,
	DUAL_GLOCK_FIRE_BOTH,
};

/*
enum glock_e {
	GLOCK_AIM = 0,
	GLOCK_IDLE1,
	GLOCK_IDLE2,
	GLOCK_IDLE3,
	GLOCK_SHOOT,
	GLOCK_SHOOT_EMPTY,
	GLOCK_RELOAD,
	GLOCK_RELOAD_NOT_EMPTY,
	GLOCK_DRAW_LOWKEY,
	GLOCK_DRAW,
	GLOCK_HOLSTER,
	GLOCK_ADD_SILENCER
};
*/

#ifdef SILENCER
LINK_ENTITY_TO_CLASS( weapon_glock, CGlock );
LINK_ENTITY_TO_CLASS( weapon_9mmhandgun, CGlock );
#endif

void CGlock::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_9mmhandgun"); // hack to allow for old names
	Precache( );
	m_iId = WEAPON_GLOCK;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_GLOCK - 1;

	m_iDefaultAmmo = GLOCK_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}

void CGlock::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_handgun.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_weapons.mdl");

	m_iShell = PRECACHE_MODEL ("models/w_shell.mdl");// brass shell

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("items/9mmclip2.wav");

	PRECACHE_SOUND ("handgun.wav");
	PRECACHE_SOUND ("handgun_silenced.wav");

	m_usFireGlock1 = PRECACHE_EVENT( 1, "events/glock1.sc" );
}

int CGlock::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CGlock::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = GLOCK_MAX_CLIP;
	p->iSlot = 1;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLOCK;
	p->iWeight = GLOCK_WEIGHT;
	p->pszDisplayName = "Silenced Handgun";

	return 1;
}

BOOL CGlock::DeployLowKey( )
{
	return DefaultDeploy( "models/v_dual_handgun.mdl", "models/p_weapons.mdl", DUAL_GLOCK_DEPLOY_LOWKEY, "onehanded", 0, m_chargeReady );
}

BOOL CGlock::Deploy( )
{
	m_chargeReady = 0; // body
	return DefaultDeploy( "models/v_dual_handgun.mdl", "models/p_weapons.mdl", DUAL_GLOCK_DEPLOY, "onehanded", 0, m_chargeReady );
}

void CGlock::Holster( int skiplocal )
{
	pev->nextthink = -1;
	CBasePlayerWeapon::DefaultHolster(DUAL_GLOCK_HOLSTER, m_chargeReady);
}

void CGlock::SecondaryAttack( void )
{
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(2.65);
	SetThink( &CGlock::AddSilencer );
	pev->nextthink = gpGlobals->time + 2.5f;

	SendWeaponAnim( DUAL_GLOCK_HOLSTER, 0, m_chargeReady );

	if (m_chargeReady) {
		m_chargeReady = 0;
	} else {
		m_chargeReady = 9;
	}
}

void CGlock::AddSilencer( void )
{
	SendWeaponAnim( DUAL_GLOCK_DEPLOY, 0, m_chargeReady );
}

void CGlock::PrimaryAttack( void )
{
	//ALERT(at_console, "m_chargeReady=%d\n", m_chargeReady);
	GlockFire( 0.03, 0.3, m_chargeReady );
	//m_chargeReady++;
	//if (m_chargeReady > 100)
	//	m_chargeReady = 0;
}

void CGlock::GlockFire( float flSpread , float flCycleTime, int silencer )
{
	if (m_iClip <= 0)
	{
		if (m_fFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = GetNextAttackDelay(0.2);
		}

		return;
	}

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	int flags;

#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// silenced
	if (m_chargeReady)
	{
		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	}
	else
	{
		// non-silenced
		m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	}

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector spread = Vector( flSpread, flSpread, flSpread );
	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		spread = VECTOR_CONE_1DEGREES;

	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, spread, 8192, BULLET_PLAYER_9MM, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFireGlock1, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, ( m_iClip == 0 ) ? 1 : 0, silencer );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCycleTime);

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}


void CGlock::Reload( void )
{
	if ( m_pPlayer->ammo_9mm <= 0 )
		 return;

	int iResult;

	if (m_iClip == 0)
		iResult = DefaultReload( GLOCK_MAX_CLIP, DUAL_GLOCK_RELOAD, 1.5, m_chargeReady );
	else
		iResult = DefaultReload( GLOCK_MAX_CLIP, DUAL_GLOCK_RELOAD, 1.5, m_chargeReady );

	if (iResult)
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	}
}



void CGlock::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	// only idle if the slid isn't back
	if (m_iClip != 0)
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.0, 1.0 );

		if (flRand <= 0.3 + 0 * 0.75)
		{
			iAnim = DUAL_GLOCK_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 49.0 / 16;
		}
		else if (flRand <= 0.6 + 0 * 0.875)
		{
			iAnim = DUAL_GLOCK_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 60.0 / 16.0;
		}
		else
		{
			iAnim = DUAL_GLOCK_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 40.0 / 16.0;
		}
		SendWeaponAnim( iAnim, 1, m_chargeReady );
	}
}

void CGlock::ProvideDualItem(CBasePlayer *pPlayer, const char *item) {
	if (pPlayer == NULL || item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	CBasePlayerWeapon::ProvideDualItem(pPlayer, item);

	if (!stricmp(item, "weapon_9mmhandgun")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_glock")) {
			pPlayer->GiveNamedItem("weapon_dual_glock");
			pPlayer->SelectItem("weapon_dual_glock");
		}
	}
#endif
}

void CGlock::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_dual_glock");
}







class CGlockAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_ammo.mdl");
		pev->body = AMMO_9MMCLIP;
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_ammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_GLOCKCLIP_GIVE, "9mm", _9MM_MAX_CARRY ) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_glockclip, CGlockAmmo );
LINK_ENTITY_TO_CLASS( ammo_9mmclip, CGlockAmmo );















