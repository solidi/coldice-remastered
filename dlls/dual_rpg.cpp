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

enum rpg_e {
	DRAW_LEFT = 0,
	DRAW_BOTH,
	IDLE_BOTH,
	FIRE_BOTH,
	HOLSTER_BOTH,
	RELOAD_BOTH,
};

#ifdef DUALRPG
LINK_ENTITY_TO_CLASS( weapon_dual_rpg, CDualRpg );
#endif

void CDualRpg::Reload( void )
{
	int iResult = 0;

	if ( m_pPlayer->ammo_rockets <= 0 )
		return;

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

	iResult = DefaultReload( RPG_MAX_CLIP * 2, RELOAD_BOTH, 2 );
	
	if ( iResult )
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	
}

void CDualRpg::Spawn( )
{
	Precache( );
	m_iId = WEAPON_DUAL_RPG;

	SET_MODEL(ENT(pev), "models/w_dual_rpg.mdl");
	m_fSpotActive = 1;

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

	FallInit();// get ready to fall down.
}

void CDualRpg::Precache( void )
{
	PRECACHE_MODEL("models/w_dual_rpg.mdl");
	PRECACHE_MODEL("models/v_dual_rpg.mdl");
	PRECACHE_MODEL("models/p_dual_rpg.mdl");

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
	p->iSlot = 5;
	p->iPosition = 4;
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
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

BOOL CDualRpg::Deploy( )
{
	if ( m_iClip == 0 )
	{
		return DefaultDeploy( "model/v_dual_rpg.mdl", "models/p_dual_rpg.mdl", DRAW_BOTH, "dual_rpg" );
	}

	return DefaultDeploy( "models/v_dual_rpg.mdl", "models/p_dual_rpg.mdl", DRAW_BOTH, "dual_rpg" );
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
	m_fInReload = FALSE;// cancel any reload in progress.

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	
	SendWeaponAnim( HOLSTER_BOTH );

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

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 12 + gpGlobals->v_up * -18;
		
		CRpgRocket *pRocket = CRpgRocket::CreateRpgRocket( vecSrc, m_pPlayer->pev->v_angle, m_pPlayer, 0.0, FALSE );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );// RpgRocket::Create stomps on globals, so remake.
		pRocket->pev->velocity = pRocket->pev->velocity + gpGlobals->v_forward * DotProduct( m_pPlayer->pev->velocity, gpGlobals->v_forward );

		SetThink( &CDualRpg::FireSecondRocket );
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.15, 0.35);
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
	}
	UpdateSpot( );
}

void CDualRpg::FireSecondRocket() {
#ifndef CLIENT_DLL
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * -12 + gpGlobals->v_up * -18;
		
	CRpgRocket *pRocket = CRpgRocket::CreateRpgRocket( vecSrc, m_pPlayer->pev->v_angle, m_pPlayer, 0.0, FALSE );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );// RpgRocket::Create stomps on globals, so remake.
	pRocket->pev->velocity = pRocket->pev->velocity + gpGlobals->v_forward * DotProduct( m_pPlayer->pev->velocity, gpGlobals->v_forward );
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

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc1 = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 32 + gpGlobals->v_right * 16 + gpGlobals->v_up * -18;
		Vector vecSrc2 = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 32 + gpGlobals->v_right * -16 + gpGlobals->v_up * -18;

		CRpgRocket *pRocket1 = CRpgRocket::CreateRpgRocket( vecSrc1, m_pPlayer->pev->v_angle, m_pPlayer, 0.0, FALSE );
		CRpgRocket *pRocket2 = CRpgRocket::CreateRpgRocket( vecSrc2, m_pPlayer->pev->v_angle, m_pPlayer, 0.0, FALSE );

		Vector vecSrc3 = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 24 + gpGlobals->v_up * -8;
		Vector vecSrc4 = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right + gpGlobals->v_up * -8;
		Vector vecSrc5 = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * -24 + gpGlobals->v_up * -8;

		CRpgRocket *pRocket3 = CRpgRocket::CreateRpgRocket( vecSrc3, m_pPlayer->pev->v_angle, m_pPlayer, 0.5, TRUE );
		CRpgRocket *pRocket4 = CRpgRocket::CreateRpgRocket( vecSrc4, m_pPlayer->pev->v_angle, m_pPlayer, 0.5, TRUE );
		CRpgRocket *pRocket5 = CRpgRocket::CreateRpgRocket( vecSrc5, m_pPlayer->pev->v_angle, m_pPlayer, 0.5, TRUE );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );// RpgRocket::Create stomps on globals, so remake.

		pRocket1->pev->velocity = pRocket1->pev->velocity + gpGlobals->v_forward * DotProduct( m_pPlayer->pev->velocity, gpGlobals->v_forward );
		pRocket2->pev->velocity = pRocket2->pev->velocity + gpGlobals->v_forward * DotProduct( m_pPlayer->pev->velocity, gpGlobals->v_forward );
		pRocket3->pev->velocity = pRocket3->pev->velocity + gpGlobals->v_forward * DotProduct( m_pPlayer->pev->velocity, gpGlobals->v_forward );
		pRocket4->pev->velocity = pRocket4->pev->velocity + gpGlobals->v_forward * DotProduct( m_pPlayer->pev->velocity, gpGlobals->v_forward );
		pRocket5->pev->velocity = pRocket5->pev->velocity + gpGlobals->v_forward * DotProduct( m_pPlayer->pev->velocity, gpGlobals->v_forward );
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
				iAnim = IDLE_BOTH;
			else
				iAnim = IDLE_BOTH;

			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 90.0 / 15.0;
		}
		else
		{
			if ( m_iClip == 0 )
				iAnim = IDLE_BOTH;
			else
				iAnim = IDLE_BOTH;

			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 6.1;
		}

		SendWeaponAnim( iAnim );
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

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc = m_pPlayer->GetGunPosition( );;
		Vector vecAiming = gpGlobals->v_forward;

		TraceResult tr;
		UTIL_TraceLine ( vecSrc, vecSrc + vecAiming * 8192, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr );
		
		UTIL_SetOrigin( m_pSpot->pev, tr.vecEndPos );
	}
#endif

}

void CDualRpg::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_rpg");
}
