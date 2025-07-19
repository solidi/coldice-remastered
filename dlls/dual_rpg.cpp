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

enum dual_rpg_e {
	DUAL_RPG_DRAW_LEFT = 0,
	DUAL_RPG_DRAW_BOTH_LOWKEY,
	DUAL_RPG_DRAW_BOTH,
	DUAL_RPG_IDLE_BOTH,
	DUAL_RPG_FIRE_BOTH,
	DUAL_RPG_HOLSTER_BOTH,
	DUAL_RPG_RELOAD_BOTH,
};

#ifdef DUALRPG
LINK_ENTITY_TO_CLASS( weapon_dual_rpg, CDualRpg );
#endif

void CDualRpg::Reload( void )
{
	int iResult = 0;

	if ( m_pPlayer->ammo_rockets <= 0 )
		return;

	if (m_flNextReload > gpGlobals->time)
		return;

	if (m_iClip == RPG_MAX_CLIP * 2)
	{
		m_fSpotActive = ! m_fSpotActive;

#ifndef CLIENT_DLL
		if (!m_fSpotActive && m_pSpot)
		{
			m_pSpot->Killed( NULL, GIB_NORMAL );
			m_pSpot = NULL;
		}
#endif

		m_flNextReload = gpGlobals->time + 0.25;
	}

	// because the RPG waits to autoreload when no missiles are active while  the LTD is on, the
	// weapons code is constantly calling into this function, but is often denied because 
	// a) missiles are in flight, but the LTD is on
	// or
	// b) player is totally out of ammo and has nothing to switch to, and should be allowed to
	//    shine the designator around
	//
	// Set the next attack time into the future so that WeaponIdle will get called more often
	// than reload, allowing the RPG LTD to be updated
	
	m_flNextPrimaryAttack = GetNextAttackDelay(0.5);

	if ( m_cActiveRockets && m_fSpotActive )
	{
		// no reloading when there are active missiles tracking the designator.
		// ward off future autoreload attempts by setting next attack time into the future for a bit. 
		return;
	}

#ifndef CLIENT_DLL
	if ( m_pSpot && m_fSpotActive )
	{
		m_pSpot->Suspend( 2.1 );
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.1;
	}
#endif

	iResult = DefaultReload( RPG_MAX_CLIP * 2, DUAL_RPG_RELOAD_BOTH, 2, 1 );
	
	if ( iResult )
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	
}

void CDualRpg::Spawn( )
{
	Precache( );
	m_iId = WEAPON_DUAL_RPG;

	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_DUAL_RPG - 1;

#ifdef CLIENT_DLL
	if ( bIsMultiplayer() )
#else
	if ( g_pGameRules->IsMultiplayer() )
#endif
	{
		// more default ammo in multiplay. 
		m_iDefaultAmmo = RPG_DEFAULT_GIVE * 2;
	}
	else
	{
		m_iDefaultAmmo = RPG_DEFAULT_GIVE;
	}
	pev->dmg = gSkillData.plrDmgRPG;

	FallInit();// get ready to fall down.
}

void CDualRpg::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_rpg.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");

	UTIL_PrecacheOther( "laser_spot" );
	UTIL_PrecacheOther( "rpg_rocket" );

	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("weapons/glauncher.wav"); // alternative fire sound

	m_usRpg = PRECACHE_EVENT ( 1, "events/dual_rpg_both.sc" );
	m_usRpgExtreme = PRECACHE_EVENT ( 1, "events/rpg_extreme.sc" );
}

int CDualRpg::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "rockets";
	p->iMaxAmmo1 = ROCKET_MAX_CARRY;
	p->pszAmmo2 = "rockets";
	p->iMaxAmmo2 = ROCKET_MAX_CARRY;
	p->iMaxClip = RPG_MAX_CLIP * 2;
	p->iSlot = 6;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_DUAL_RPG;
	p->iFlags = 0;
	p->iWeight = RPG_WEIGHT * 2;
	p->pszDisplayName = "Dual 50-Pound Automatic LAW Rocket Launchers";

	return 1;
}

int CDualRpg::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

BOOL CDualRpg::DeployLowKey( )
{
	m_fSpotActive = 1;
	return DefaultDeploy( "models/v_dual_rpg.mdl", "models/p_weapons.mdl", DUAL_RPG_DRAW_BOTH_LOWKEY, "dual_rpg", 0, 1 );
}

BOOL CDualRpg::Deploy( )
{
	m_fSpotActive = 1;
	return DefaultDeploy( "models/v_dual_rpg.mdl", "models/p_weapons.mdl", DUAL_RPG_DRAW_BOTH, "dual_rpg", 0, 1 );
}

BOOL CDualRpg::CanHolster( void )
{
	if ( m_fSpotActive && m_cActiveRockets )
	{
		// can't put away while guiding a missile.
		return FALSE;
	}

	return TRUE;
}

void CDualRpg::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(DUAL_RPG_HOLSTER_BOTH, 1);

	m_fSpotActive = 0;

#ifndef CLIENT_DLL
	if (m_pSpot)
	{
		m_pSpot->Killed( NULL, GIB_NEVER );
		m_pSpot = NULL;
	}
#endif

}

void CDualRpg::PrimaryAttack()
{
	if ( m_iClip )
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

#ifndef CLIENT_DLL
		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
		Vector vecSrc = m_pPlayer->GetGunPosition( ) + (vecAiming * 16) + gpGlobals->v_right * 12 + gpGlobals->v_up * -8;

		CBaseEntity *pRocket = CRpgRocket::CreateRpgRocket( vecSrc, vecAiming, m_pPlayer, 0.0, FALSE );
		if (pRocket)
			pRocket->pev->velocity = pRocket->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );

		SetThink( &CDualRpg::FireSecondRocket );
		pev->nextthink = gpGlobals->time + (0.15 * g_pGameRules->WeaponMultipler());
#endif

		// firing RPG no longer turns on the designator. ALT fire is a toggle switch for the LTD.
		// Ken signed up for this as a global change (sjb)

		int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

		PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usRpg );

		m_iClip--; 
				
		m_flNextPrimaryAttack = GetNextAttackDelay(0.75);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5;
	}
	else
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
	}
	UpdateSpot( );
}

void CDualRpg::FireSecondRocket()
{
#ifndef CLIENT_DLL
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/glauncher.wav", 0.9, ATTN_NORM);

	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecSrc = m_pPlayer->GetGunPosition( ) + (vecAiming * 16) + gpGlobals->v_right * -12 + gpGlobals->v_up * -8;

	CBaseEntity *pRocket = CRpgRocket::CreateRpgRocket( vecSrc, vecAiming, m_pPlayer, 0.0, FALSE );
	if (pRocket) 
		pRocket->pev->velocity = pRocket->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );
#endif
}

void CDualRpg::SecondaryAttack()
{
	if ( m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] > 0 )
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

#ifndef CLIENT_DLL
		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
		Vector vecSrc1 = m_pPlayer->GetGunPosition( ) + vecAiming * 32 + gpGlobals->v_right * 16 + gpGlobals->v_up * -8;
		Vector vecSrc2 = m_pPlayer->GetGunPosition( ) + vecAiming * 32 + gpGlobals->v_right * -16 + gpGlobals->v_up * -8;

		CBaseEntity *pRocket1 = CRpgRocket::CreateRpgRocket( vecSrc1, vecAiming, m_pPlayer, 0.0, FALSE );
		CBaseEntity *pRocket2 = CRpgRocket::CreateRpgRocket( vecSrc2, vecAiming, m_pPlayer, 0.0, FALSE );

		Vector vecSrc3 = m_pPlayer->GetGunPosition( ) + vecAiming * 16 + gpGlobals->v_right * 24 + gpGlobals->v_up * -8;
		Vector vecSrc4 = m_pPlayer->GetGunPosition( ) + vecAiming * 16 + gpGlobals->v_right + gpGlobals->v_up * -8;
		Vector vecSrc5 = m_pPlayer->GetGunPosition( ) + vecAiming * 16 + gpGlobals->v_right * -24 + gpGlobals->v_up * -8;

		CBaseEntity *pRocket3 = CRpgRocket::CreateRpgRocket( vecSrc3, vecAiming, m_pPlayer, 0.0, TRUE );
		CBaseEntity *pRocket4 = CRpgRocket::CreateRpgRocket( vecSrc4, vecAiming, m_pPlayer, 0.0, TRUE );
		CBaseEntity *pRocket5 = CRpgRocket::CreateRpgRocket( vecSrc5, vecAiming, m_pPlayer, 0.0, TRUE );

		if (pRocket1)
			pRocket1->pev->velocity = pRocket1->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );
		if (pRocket2)
			pRocket2->pev->velocity = pRocket2->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );
		if (pRocket3)
			pRocket3->pev->velocity = pRocket3->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );
		if (pRocket4)
			pRocket4->pev->velocity = pRocket4->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );
		if (pRocket5)
			pRocket5->pev->velocity = pRocket5->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );
#endif

		int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

		PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usRpg );

		m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType]--;

		m_flNextSecondaryAttack = GetNextAttackDelay(3.5);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.5;
	}
	else
	{
		PlayEmptySound( );
	}
	UpdateSpot( );
}

int CDualRpg::SecondaryAmmoIndex( void )
{
	return m_iSecondaryAmmoType;
}

void CDualRpg::WeaponIdle( void )
{
	UpdateSpot( );

	ResetEmptySound( );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
		if (flRand <= 0.75 || m_fSpotActive)
		{
			if ( m_iClip == 0 )
				iAnim = DUAL_RPG_IDLE_BOTH;
			else
				iAnim = DUAL_RPG_IDLE_BOTH;

			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 90.0 / 15.0;
		}
		else
		{
			if ( m_iClip == 0 )
				iAnim = DUAL_RPG_IDLE_BOTH;
			else
				iAnim = DUAL_RPG_IDLE_BOTH;

			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 6.1;
		}

		SendWeaponAnim( iAnim, 1, 1 );
	}
	else
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1;
	}
}

void CDualRpg::UpdateSpot( void )
{

#ifndef CLIENT_DLL
	if (m_fSpotActive)
	{
		if (!m_pSpot)
		{
			m_pSpot = CLaserSpot::CreateSpot();
		}

		Vector vecSrc = m_pPlayer->GetGunPosition( );
		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

		TraceResult tr;
		UTIL_TraceLine ( vecSrc, vecSrc + vecAiming * 8192, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr );
		
		UTIL_SetOrigin( m_pSpot->pev, tr.vecEndPos );
	}
#endif

}

void CDualRpg::ProvideSingleItem(CBasePlayer *pPlayer, const char *item) {
	if (item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	if (!stricmp(item, "weapon_dual_rpg")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_rpg")) {
			pPlayer->GiveNamedItem("weapon_rpg");
			pPlayer->SelectItem("weapon_dual_rpg");
		}
	}
#endif
}

void CDualRpg::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_rpg");
}
