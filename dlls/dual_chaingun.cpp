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

enum dual_chaingun_e
{
	DUAL_CHAINGUN_IDLE = 0,
	DUAL_CHAINGUN_IDLE1,
	DUAL_CHAINGUN_SPINUP,
	DUAL_CHAINGUN_SPINDOWN,
	DUAL_CHAINGUN_FIRE,
	DUAL_CHAINGUN_DRAW_LOWKEY,
	DUAL_CHAINGUN_DRAW,
	DUAL_CHAINGUN_HOLSTER,
	DUAL_CHAINGUN_RELOAD,
};

#ifdef DUALCHAINGUN
LINK_ENTITY_TO_CLASS( weapon_dual_chaingun, CDualChaingun );
#endif

void CDualChaingun::Spawn( )
{	 
	pev->classname = MAKE_STRING("weapon_dual_chaingun");
	Precache();
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_DUAL_CHAINGUN - 1;
	m_iId = WEAPON_DUAL_CHAINGUN;

	m_iDefaultAmmo = CHAINGUN_DEFAULT_GIVE * 2;

	FallInit();
}

void CDualChaingun::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_chaingun.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_weapons.mdl");

	PRECACHE_MODEL ("models/w_shell.mdl");

	PRECACHE_SOUND("chaingun_fire.wav");
	PRECACHE_SOUND("chaingun_spinup.wav");
	PRECACHE_SOUND("chaingun_spindown.wav");
	PRECACHE_SOUND("chaingun_reload.wav");
	PRECACHE_SOUND("weapons/357_cock1.wav");

	m_useFireChaingun = PRECACHE_EVENT( 1, "events/dual_chaingun.sc" );
}

int CDualChaingun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = CHAINGUN_MAX_CLIP * 2;
	p->iSlot = 6;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_DUAL_CHAINGUN;
	p->iWeight = CHAINGUN_WEIGHT * 2;
	p->pszDisplayName = "Dual 25-Inch Chainguns";

	return 1;
}

int CDualChaingun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

BOOL CDualChaingun::DeployLowKey( )
{
	return DefaultDeploy( "models/v_dual_chaingun.mdl", "models/p_weapons.mdl", DUAL_CHAINGUN_DRAW_LOWKEY, "dual_egon" );
}

BOOL CDualChaingun::Deploy( )
{
	return DefaultDeploy( "models/v_dual_chaingun.mdl", "models/p_weapons.mdl", DUAL_CHAINGUN_DRAW, "dual_egon" );
}

void CDualChaingun::Holster( int skiplocal )
{
	pev->nextthink = -1;
	CBasePlayerWeapon::DefaultHolster(DUAL_CHAINGUN_HOLSTER);
}

void CDualChaingun::PrimaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_iClip <= 0)
	{
		SendWeaponAnim( DUAL_CHAINGUN_SPINDOWN );
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;
		return;
	}

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0 && m_iClip == 0) 
	{
		PlayEmptySound();
		RetireWeapon();
		return;
	}

	if ( m_iWeaponMode == DUAL_CHAINGUN_IDLE && m_iClip > 0 )
	{
		SendWeaponAnim( DUAL_CHAINGUN_SPINUP );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "chaingun_spinup.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.75;
		m_iWeaponMode = DUAL_CHAINGUN_FIRE;
		return;
	}

	SlowDownPlayer();

	if (m_fFireMagnitude == 0) 
		m_fFireMagnitude = 4;

	if (m_fFireMagnitude < 8) 
		m_fFireMagnitude++;

	if (m_iWeaponMode == DUAL_CHAINGUN_FIRE && m_iClip > 0) {
		Fire(VECTOR_CONE_3DEGREES.x, 1/(float) m_fFireMagnitude);
	}
}

void CDualChaingun::SecondaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_iClip <= 0)
	{
		SendWeaponAnim( DUAL_CHAINGUN_SPINDOWN );
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.3;
		return;
	}

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0 && m_iClip == 0) 
	{
		PlayEmptySound();
		RetireWeapon();
		return;
	}

	if ( m_iWeaponMode == DUAL_CHAINGUN_IDLE && m_iClip > 0 )
	{
		SendWeaponAnim( DUAL_CHAINGUN_SPINUP );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "chaingun_spinup.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));	
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.75;
		m_iWeaponMode = DUAL_CHAINGUN_FIRE;
		return;
	}

	SlowDownPlayer();

	if (m_fFireMagnitude == 0) 
		m_fFireMagnitude = 4;

	if (m_fFireMagnitude < 8) 
		m_fFireMagnitude++;
}

void CDualChaingun::Fire( float flSpread, float flCycleTime )
{
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

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector spread = Vector( flSpread, flSpread, flSpread );
	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		spread = VECTOR_CONE_1DEGREES;

	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, spread, 8192, BULLET_PLAYER_357, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_useFireChaingun, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, ( m_iClip == 0 ) ? 1 : 0, m_pPlayer->pev->weaponanim != DUAL_CHAINGUN_FIRE );

	if (m_pPlayer->pev->weaponanim != DUAL_CHAINGUN_FIRE) {
		m_pPlayer->pev->weaponanim = DUAL_CHAINGUN_FIRE;
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCycleTime);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CDualChaingun::SlowDownPlayer()
{
	if ( m_iClip != 0 )
	{
		Vector xy = Vector(m_pPlayer->pev->velocity.x, m_pPlayer->pev->velocity.y, 0).Normalize();
	
		// Slow down player while rapid firing
		if (m_pPlayer->pev->velocity.Length2D() > 50) {
			m_pPlayer->pev->velocity = Vector(xy.x * 50, xy.y * 50, m_pPlayer->pev->velocity.z);
		}
	}
}

void CDualChaingun::Reload( void )
{
	if ( m_iClip < CHAINGUN_MAX_CLIP )
	{
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "chaingun_reload.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));	
		DefaultReload( CHAINGUN_MAX_CLIP, DUAL_CHAINGUN_RELOAD, 2.5 );
	}
}

void CDualChaingun::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if (m_iWeaponMode == DUAL_CHAINGUN_FIRE)
	{
		SendWeaponAnim( DUAL_CHAINGUN_SPINDOWN );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "chaingun_spindown.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		m_iWeaponMode = DUAL_CHAINGUN_IDLE;
		return;
	}
	
	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;
	
	ResetEmptySound( );

	int iAnim;
	
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:	
		iAnim = DUAL_CHAINGUN_IDLE;
		break;
	
	default:
	case 1:
		iAnim = DUAL_CHAINGUN_IDLE1;
		break;
	}

	SendWeaponAnim( iAnim );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT ( 10, 15 );
}

void CDualChaingun::ProvideSingleItem(CBasePlayer *pPlayer, const char *item) {
	if (item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	if (!stricmp(item, "weapon_dual_chaingun")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_chaingun")) {
			pPlayer->GiveNamedItem("weapon_chaingun");
			pPlayer->SelectItem("weapon_dual_chaingun");
		}
	}
#endif
}

void CDualChaingun::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_chaingun");
}
