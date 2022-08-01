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

enum chaingun_e
{
    CHAINGUN_IDLE = 0,
	CHAINGUN_IDLE1,
	CHAINGUN_SPINUP,
	CHAINGUN_SPINDOWN,
	CHAINGUN_FIRE,
	CHAINGUN_DRAW_LOWKEY,
	CHAINGUN_DRAW,
	CHAINGUN_HOLSTER,
	CHAINGUN_RELOAD,
};

#ifdef CHAINGUN
LINK_ENTITY_TO_CLASS( weapon_chaingun, CChaingun );
#endif

void CChaingun::Spawn( )
{	 
	pev->classname = MAKE_STRING("weapon_chaingun");
	Precache();
	SET_MODEL(ENT(pev), "models/w_chaingun.mdl");
	m_iId = WEAPON_CHAINGUN;

	m_iDefaultAmmo = CHAINGUN_DEFAULT_GIVE;

	FallInit();
}

void CChaingun::Precache( void )
{
	PRECACHE_MODEL("models/v_chaingun.mdl");
	PRECACHE_MODEL("models/w_chaingun.mdl");
	PRECACHE_MODEL("models/p_chaingun.mdl");

	PRECACHE_MODEL ("models/w_shell.mdl");

	PRECACHE_SOUND("chaingun_fire.wav");
	PRECACHE_SOUND("chaingun_spinup.wav");
	PRECACHE_SOUND("chaingun_spindown.wav");
	PRECACHE_SOUND("chaingun_reload.wav");
	PRECACHE_SOUND("weapons/357_cock1.wav");

	m_useFireChaingun = PRECACHE_EVENT( 1, "events/chaingun.sc" );
}

int CChaingun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = CHAINGUN_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 6;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_CHAINGUN;
	p->iWeight = CHAINGUN_WEIGHT;
	p->pszDisplayName = "25-Inch Chaingun";

	return 1;
}

int CChaingun::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CChaingun::DeployLowKey( )
{
	return DefaultDeploy( "models/v_chaingun.mdl", "models/p_chaingun.mdl", CHAINGUN_DRAW_LOWKEY, "mp5" );
}

BOOL CChaingun::Deploy( )
{
	return DefaultDeploy( "models/v_chaingun.mdl", "models/p_chaingun.mdl", CHAINGUN_DRAW, "mp5" );
}

void CChaingun::Holster( int skiplocal )
{
	pev->nextthink = -1;
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( CHAINGUN_HOLSTER );
}

void CChaingun::PrimaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_iClip <= 0)
	{
		SendWeaponAnim( CHAINGUN_SPINDOWN );
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

	if ( m_iWeaponMode == CHAINGUN_IDLE && m_iClip > 0 )
	{
		SendWeaponAnim( CHAINGUN_SPINUP );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "chaingun_spinup.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));	
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.75;
		m_iWeaponMode = CHAINGUN_FIRE;
		return;
	}

	SlowDownPlayer();

	if (m_fFireMagnitude == 0) 
		m_fFireMagnitude = 4;

	if (m_fFireMagnitude < 8) 
		m_fFireMagnitude++;

	if (m_iWeaponMode == CHAINGUN_FIRE && m_iClip > 0) {
		Fire (VECTOR_CONE_3DEGREES.x, 1/(float) m_fFireMagnitude, TRUE);
	}
}

void CChaingun::Fire( float flSpread, float flCycleTime, BOOL fUseAutoAim )
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
	Vector vecAiming;
	
	if ( fUseAutoAim )
	{
		vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	}
	else
	{
		vecAiming = gpGlobals->v_forward;
	}

	Vector vecDir;
	vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( flSpread, flSpread, flSpread ), 8192, BULLET_PLAYER_357, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_useFireChaingun, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, ( m_iClip == 0 ) ? 1 : 0, m_pPlayer->pev->weaponanim != CHAINGUN_FIRE );

	if (m_pPlayer->pev->weaponanim != CHAINGUN_FIRE) {
		m_pPlayer->pev->weaponanim = CHAINGUN_FIRE;
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCycleTime);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CChaingun::SlowDownPlayer()
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

void CChaingun::Reload( void )
{
	if ( m_iClip < CHAINGUN_MAX_CLIP )
	{
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "chaingun_reload.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));	
		DefaultReload( CHAINGUN_MAX_CLIP, CHAINGUN_RELOAD, 2.5 );
	}
}

void CChaingun::WeaponIdle( void )
{
	if (m_iWeaponMode == CHAINGUN_FIRE)
	{
		SendWeaponAnim( CHAINGUN_SPINDOWN );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "chaingun_spindown.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		m_iWeaponMode = CHAINGUN_IDLE;
		return;
	}
	
	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;
	
	ResetEmptySound( );
	
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	int iAnim;
	
	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:	
		iAnim = CHAINGUN_IDLE;
		break;
	
	default:
	case 1:
		iAnim = CHAINGUN_IDLE1;
		break;
	}

	SendWeaponAnim( iAnim );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT ( 10, 15 );
}
