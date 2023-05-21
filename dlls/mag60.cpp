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
#include "game.h"

enum mag60_e {
	MAG60_AIM = 0,
	MAG60_IDLE1,
	MAG60_IDLE2,
	MAG60_IDLE3,
	MAG60_SHOOT,
	MAG60_SHOOT_SIDEWAYS,
	MAG60_SHOOT_EMPTY,
	MAG60_RELOAD,
	MAG60_RELOAD_SIDEWAYS,
	MAG60_RELOAD_NOT_EMPTY,
	MAG60_DRAW_LOWKEY,
	MAG60_DRAW,
	MAG60_HOLSTER,
	MAG60_BUTTON,
	MAG60_ROTATE_DOWN,
	MAG60_ROTATE_UP
};

#ifdef MAG60
LINK_ENTITY_TO_CLASS( weapon_mag60, CMag60 );
#endif

void CMag60::Spawn( )
{
	Precache( );
	m_iId = WEAPON_MAG60;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_MAG60 - 1;

	m_iDefaultAmmo = MAG60_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}

int CMag60::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

const char *CMag60::pRotateUpBladeSounds[] = 
{
	"mag60_blade_arrangement.wav",
	"mag60_blade_nature.wav",
	"mag60_blade_capable.wav",
	"mag60_blade_uphill.wav",
	"mag60_blade_topping.wav",
	"mag60_blade_pass.wav",
};

const char *CMag60::pRotateDownBladeSounds[] = 
{
	"mag60_blade_music.wav",
	"mag60_blade_suckaheads.wav",
	"mag60_blade_frost.wav",
	"mag60_blade_usethem.wav",
	"mag60_blade_trigger.wav",
	"mag60_blade_dead.wav",
};

void CMag60::Precache( void )
{
	PRECACHE_MODEL("models/v_mag60.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_mag60.mdl");

	m_iShell = PRECACHE_MODEL ("models/w_shell.mdl");// brass shell

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("items/9mmclip2.wav");

	PRECACHE_SOUND ("mag60_fire.wav");

	PRECACHE_SOUND_ARRAY(pRotateDownBladeSounds);
	PRECACHE_SOUND_ARRAY(pRotateUpBladeSounds);

	m_useFireMag60 = PRECACHE_EVENT( 1, "events/mag60.sc" );
}

int CMag60::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = MAG60_MAX_CLIP;
	p->iSlot = 1;
	p->iPosition = 3;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_MAG60;
	p->iWeight = MAG60_WEIGHT;
	p->pszDisplayName = "Blade's Mag 60 Automatic Handgun";

	return 1;
}

BOOL CMag60::DeployLowKey( )
{
	return DefaultDeploy("models/v_mag60.mdl", "models/p_mag60.mdl", MAG60_DRAW_LOWKEY, "onehanded", 0);
}

BOOL CMag60::Deploy( )
{
	BOOL result = DefaultDeploy("models/v_mag60.mdl", "models/p_mag60.mdl", MAG60_DRAW, "onehanded", 0);

#ifndef CLIENT_DLL
	if (result && allowvoiceovers.value) {
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, RANDOM_SOUND_ARRAY(pRotateUpBladeSounds), 1.0, ATTN_NORM );
	}
#endif

	return result;
}

void CMag60::Holster( int skiplocal )
{
	m_iRotated = 0;
	pev->nextthink = -1;
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( MAG60_HOLSTER );
}

void CMag60::SecondaryAttack( void )
{
	// don't select underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.5);

	if (m_iRotated) {
		m_iRotated = 0;
		SendWeaponAnim( MAG60_ROTATE_UP );
#ifndef CLIENT_DLL
		if (allowvoiceovers.value)
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, RANDOM_SOUND_ARRAY(pRotateUpBladeSounds), 1.0, ATTN_NORM );
#endif
	} else {
		m_iRotated = 1;
		SendWeaponAnim( MAG60_ROTATE_DOWN );
#ifndef CLIENT_DLL
		if (allowvoiceovers.value)
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, RANDOM_SOUND_ARRAY(pRotateDownBladeSounds), 1.0, ATTN_NORM );
#endif
	}
}

void CMag60::PrimaryAttack( void )
{
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	Fire( m_iRotated ? 0.03 : 0.01, m_iRotated ? 0.125 : 0.20, TRUE, m_iRotated );
}

void CMag60::Fire( float flSpread , float flCycleTime, BOOL fUseAutoAim, int rotated )
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

	if (m_iRotated == 1)
	{
		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	}
	else
	{
		m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	}

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
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
	vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, Vector( flSpread, flSpread, flSpread ), 8192, BULLET_PLAYER_9MM, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_useFireMag60, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, ( m_iClip == 0 ) ? 1 : 0, rotated );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCycleTime);

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CMag60::Reload( void )
{
	if ( m_pPlayer->ammo_9mm <= 0 )
		 return;

	int iResult;

	if (m_iRotated > 0)
		iResult = DefaultReload( MAG60_MAX_CLIP, MAG60_RELOAD_SIDEWAYS, 1.4 );
	else
		iResult = DefaultReload( MAG60_MAX_CLIP, MAG60_RELOAD, 1.4 );

	if (iResult)
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	}
}

void CMag60::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	// No idle animations when rotated (which would be a lot of work for rotated state)
	if (m_iRotated > 0)
		return;

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
			iAnim = MAG60_IDLE3;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 49.0 / 16;
		}
		else if (flRand <= 0.6 + 0 * 0.875)
		{
			iAnim = MAG60_IDLE1;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 60.0 / 16.0;
		}
		else
		{
			iAnim = MAG60_IDLE2;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 40.0 / 16.0;
		}
		SendWeaponAnim( iAnim, 1 );
	}
}

void CMag60::ProvideDualItem(CBasePlayer *pPlayer, const char *item) {
	if (pPlayer == NULL || item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	CBasePlayerWeapon::ProvideDualItem(pPlayer, item);

	if (!stricmp(item, "weapon_mag60")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_mag60")) {
#ifdef _DEBUG
			ALERT(at_aiconsole, "Give weapon_dual_mag60!\n");
#endif
			pPlayer->GiveNamedItem("weapon_dual_mag60");
			pPlayer->SelectItem("weapon_dual_mag60");
		}
	}
#endif
}

void CMag60::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_dual_mag60");
}
