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

enum usas_e {
	USAS_AIM = 0,
	USAS_LONGIDLE,
	USAS_IDLE1,
	USAS_LAUNCH,
	USAS_RELOAD,
	USAS_DRAW_LOWKEY,
	USAS_DEPLOY,
	USAS_FIRE1,
	USAS_FIRE2,
	USAS_FIRE3,
	USAS_HOLSTER,
};

#ifdef USAS
LINK_ENTITY_TO_CLASS( weapon_usas, CUsas );
#endif

void CUsas::Spawn( )
{
	Precache( );
	m_iId = WEAPON_USAS;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_USAS - 1;

	m_iDefaultAmmo = USAS_DEFAULT_GIVE;

	FallInit();// get ready to fall
}

void CUsas::Precache( void )
{
	PRECACHE_MODEL("models/v_usas.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_weapons.mdl");

	m_iShell = PRECACHE_MODEL ("models/w_shotgunshell.mdl");// shotgun shell

	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND ("usas_fire.wav");

	PRECACHE_SOUND ("weapons/reload1.wav");	// shotgun reload
	PRECACHE_SOUND ("weapons/reload3.wav");	// shotgun reload

	PRECACHE_SOUND ("weapons/357_cock1.wav"); // gun empty sound
	PRECACHE_SOUND ("weapons/scock1.wav");	// cock gun

	m_usSingleFire = PRECACHE_EVENT( 1, "events/usas.sc" );
}

int CUsas::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CUsas::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = BUCKSHOT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = USAS_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 3;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_USAS;
	p->iWeight = USAS_WEIGHT;
	p->pszDisplayName = "USAS-12 Auto Shotgun";

	return 1;
}

BOOL CUsas::DeployLowKey( )
{
	return DefaultDeploy( "models/v_usas.mdl", "models/p_weapons.mdl", USAS_DRAW_LOWKEY, "mp5" );
}

BOOL CUsas::Deploy( )
{
	return DefaultDeploy( "models/v_usas.mdl", "models/p_weapons.mdl", USAS_DEPLOY, "mp5" );
}

void CUsas::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(USAS_HOLSTER);
}

void CUsas::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_iClip <= 0)
	{
		Reload( );
		if (m_iClip == 0)
			PlayEmptySound( );
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

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	Vector spread = VECTOR_CONE_DM_SHOTGUN;
	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		spread = VECTOR_CONE_5DEGREES;

	Vector vecDir = m_pPlayer->FireBulletsPlayer( 4, vecSrc, vecAiming, spread, 2048, BULLET_PLAYER_BUCKSHOT, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usSingleFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = GetNextAttackDelay(0.25);
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
	if (m_iClip != 0)
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
	else
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75;
	m_fInSpecialReload = 0;
}

void CUsas::Reload( void )
{
	if ( m_pPlayer->ammo_buckshot <= 0 )
		return;

	DefaultReload( USAS_MAX_CLIP, USAS_RELOAD, 1.5 );
}

void CUsas::WeaponIdle( void )
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
		iAnim = USAS_LONGIDLE;
		break;

	default:
	case 1:
		iAnim = USAS_IDLE1;
		break;
	}

	SendWeaponAnim( iAnim );

	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 ); // how long till we do this again.
}

void CUsas::ProvideDualItem(CBasePlayer *pPlayer, const char *item) {
	if (pPlayer == NULL || item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	CBasePlayerWeapon::ProvideDualItem(pPlayer, item);

	if (!stricmp(item, "weapon_usas")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_usas")) {
			pPlayer->GiveNamedItem("weapon_dual_usas");
			pPlayer->SelectItem("weapon_dual_usas");
		}
	}
#endif
}

void CUsas::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_dual_usas");
}

