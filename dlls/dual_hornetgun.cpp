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
#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "hornet.h"
#include "gamerules.h"


enum dual_hgun_e {
	DUAL_HGUN_IDLE1 = 0,
	DUAL_HGUN_FIDGETSWAY,
	DUAL_HGUN_FIDGETSHAKE,
	DUAL_HGUN_DOWN,
	DUAL_HGUN_DRAW_LOWKEY,
	DUAL_HGUN_UP,
	DUAL_HGUN_SHOOT
};

enum firemode_e
{
	FIREMODE_TRACK = 0,
	FIREMODE_FAST
};

#ifdef DUALHIVEHAND
LINK_ENTITY_TO_CLASS( weapon_dual_hornetgun, CDualHgun );
#endif

BOOL CDualHgun::IsUseable( void )
{
	return TRUE;
}

void CDualHgun::Spawn( )
{
	Precache( );
	m_iId = WEAPON_DUAL_HORNETGUN;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_DUAL_HORNETGUN - 1;

	m_iDefaultAmmo = HIVEHAND_DEFAULT_GIVE * 2;
	pev->dmg = gSkillData.plrDmgHornet;
	m_iFirePhase = 0;
	m_iChargeSoundOn = 0;
	m_flChargeNextEmpty = 0.0f;

	FallInit();// get ready to fall down.
}


void CDualHgun::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_hgun.mdl");

	m_usHornetFire = PRECACHE_EVENT ( 1, "events/dual_hornetgun.sc" );

	PRECACHE_SOUND("items/medcharge4.wav");
	PRECACHE_SOUND("items/medshotno1.wav");

	UTIL_PrecacheOther("hornet");
}

int CDualHgun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{

#ifndef CLIENT_DLL
		if ( g_pGameRules->IsMultiplayer() )
		{
			// in multiplayer, all hivehands come full. 
			pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] = HORNET_MAX_CARRY;
		}
#endif

		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CDualHgun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Hornets";
	p->iMaxAmmo1 = HORNET_MAX_CARRY * 2;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 6;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_DUAL_HORNETGUN;
	p->iFlags = ITEM_FLAG_NOAUTOSWITCHEMPTY | ITEM_FLAG_NOAUTORELOAD;
	p->iWeight = HORNETGUN_WEIGHT;
	p->pszDisplayName = "Dual Hivehand";

	return 1;
}

BOOL CDualHgun::DeployLowKey( )
{
	return DefaultDeploy( "models/v_dual_hgun.mdl", "models/p_weapons.mdl", DUAL_HGUN_DRAW_LOWKEY, "dual_egon", 0, 1 );
}

BOOL CDualHgun::Deploy( )
{
	return DefaultDeploy( "models/v_dual_hgun.mdl", "models/p_weapons.mdl", DUAL_HGUN_UP, "dual_egon", 0, 1 );
}

void CDualHgun::Holster( int skiplocal /* = 0 */ )
{
	if ( m_iChargeSoundOn )
	{
		STOP_SOUND( ENT(m_pPlayer->pev), CHAN_STATIC, "items/medcharge4.wav" );
		m_iChargeSoundOn = 0;
	}

	CBasePlayerWeapon::DefaultHolster(DUAL_HGUN_DOWN, 1);

	//!!!HACKHACK - can't select hornetgun if it's empty! no way to get ammo for it, either.
	if ( !m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] )
	{
		m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] = 1;
	}
}


void CDualHgun::PrimaryAttack()
{
	// Holding IN_RELOAD locks out firing so the hivehand isn't cycling shots
	// while the fast-charge loop is running.
	if ( m_pPlayer->pev->button & IN_RELOAD )
	{
		Reload();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1;
		return;
	}

	Reload( );

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		// Longer cooldown while dry so we don't rapidly re-enter this branch
		// every 0.15s; the recharge will backfill soon enough.
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
		return;
	}

#ifndef CLIENT_DLL
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	// Only inherit a small fraction of the player's velocity so hard strafes
	// nudge the trajectory instead of throwing hornets 40 degrees off-axis.
	const float flInheritVel = 0.15f;

	CBaseEntity *pHornet = CBaseEntity::Create( "hornet", m_pPlayer->GetGunPosition( ) + vecAiming * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12, UTIL_VecToAngles(vecAiming), m_pPlayer->edict() );
	if (pHornet != NULL)
		pHornet->pev->velocity = m_pPlayer->pev->velocity * flInheritVel + vecAiming * 300;

	pHornet = CBaseEntity::Create( "hornet", m_pPlayer->GetGunPosition( ) + vecAiming * 16 + gpGlobals->v_right * - 8 + gpGlobals->v_up * -12, UTIL_VecToAngles(vecAiming), m_pPlayer->edict() );
	if (pHornet != NULL)
		pHornet->pev->velocity = m_pPlayer->pev->velocity * flInheritVel + vecAiming * 300;

	m_flRechargeTime = gpGlobals->time + (0.25 * g_pGameRules->WeaponMultipler());
#endif
	
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usHornetFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, FIREMODE_TRACK, 0, 0, 0 );

	

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
	{
		// Just fired the last hornet: enforce a real cooldown so we don't
		// immediately fall back into the empty-branch tap-fire loop.
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.0;
	}
	else
	{
		m_flNextPrimaryAttack = GetNextAttackDelay(0.25);

		if (m_flNextPrimaryAttack < UTIL_WeaponTimeBase() )
		{
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.25;
		}
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}



void CDualHgun::SecondaryAttack( void )
{
	if ( m_pPlayer->pev->button & IN_RELOAD )
	{
		Reload();
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.1;
		return;
	}

	Reload();

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
		return;
	}

	//Wouldn't be a bad idea to completely predict these, since they fly so fast...
#ifndef CLIENT_DLL
	CBaseEntity *pHornet;
	Vector vecSrc;

	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	vecSrc = m_pPlayer->GetGunPosition( ) + vecAiming * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12;

	m_iFirePhase++;
	switch ( m_iFirePhase )
	{
	case 1:
		vecSrc = vecSrc + gpGlobals->v_up * 8;
		break;
	case 2:
		vecSrc = vecSrc + gpGlobals->v_up * 8;
		vecSrc = vecSrc + gpGlobals->v_right * 8;
		break;
	case 3:
		vecSrc = vecSrc + gpGlobals->v_right * 8;
		break;
	case 4:
		vecSrc = vecSrc + gpGlobals->v_up * -8;
		vecSrc = vecSrc + gpGlobals->v_right * 8;
		break;
	case 5:
		vecSrc = vecSrc + gpGlobals->v_up * -8;
		break;
	case 6:
		vecSrc = vecSrc + gpGlobals->v_up * -8;
		vecSrc = vecSrc + gpGlobals->v_right * -16;
		break;
	case 7:
		vecSrc = vecSrc + gpGlobals->v_right * -16;
		break;
	case 8:
		vecSrc = vecSrc + gpGlobals->v_up * 8;
		vecSrc = vecSrc + gpGlobals->v_right * -16;
		m_iFirePhase = 0;
		break;
	}

	pHornet = CBaseEntity::Create( "hornet", vecSrc, vecAiming, m_pPlayer->edict() );
	if (pHornet != NULL)
	{
		pHornet->pev->velocity = m_pPlayer->pev->velocity + vecAiming * 1200;
		pHornet->pev->angles = UTIL_VecToAngles( pHornet->pev->velocity );
		pHornet->SetThink( &CHornet::StartDart );
	}

	m_flRechargeTime = gpGlobals->time + (0.25 * g_pGameRules->WeaponMultipler());
#endif

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usHornetFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, FIREMODE_FAST, 0, 0, 0 );


	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

		// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
	{
		// Fired the last hornet on secondary — hold off before empty taps.
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.0;
	}
	else
	{
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.1;
	}
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}


void CDualHgun::Reload( void )
{
	// Same dual-mode recharge behavior as CHgun: passive callers trickle at the
	// normal rate; holding IN_RELOAD doubles the rate and plays the func_healthcharger
	// loop for audible feedback. See hornetgun.cpp for the reference implementation.
	const int iCap = HORNET_MAX_CARRY * 2;
	BOOL bReloadHeld = ( m_pPlayer->pev->button & IN_RELOAD ) != 0;
	BOOL bFull = ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= iCap );

	if ( bReloadHeld )
	{
		if ( bFull )
		{
			if ( m_iChargeSoundOn )
			{
				STOP_SOUND( ENT(m_pPlayer->pev), CHAN_STATIC, "items/medcharge4.wav" );
				m_iChargeSoundOn = 0;
			}
			if ( m_flChargeNextEmpty <= gpGlobals->time )
			{
				EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_ITEM, "items/medshotno1.wav", 1.0, ATTN_NORM );
				m_flChargeNextEmpty = gpGlobals->time + 0.62f;
			}
			return;
		}

		if ( !m_iChargeSoundOn )
		{
			EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_STATIC, "items/medcharge4.wav", 1.0, ATTN_NORM );
			m_iChargeSoundOn = 1;
		}

		while (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < iCap && m_flRechargeTime < gpGlobals->time)
		{
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]++;
#ifndef CLIENT_DLL
			// 2x rate: half of the passive 0.25s interval.
			m_flRechargeTime += 0.125 * g_pGameRules->WeaponMultipler();
#endif
		}

		if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= iCap )
		{
			STOP_SOUND( ENT(m_pPlayer->pev), CHAN_STATIC, "items/medcharge4.wav" );
			m_iChargeSoundOn = 0;
			EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_ITEM, "items/medshotno1.wav", 1.0, ATTN_NORM );
			m_flChargeNextEmpty = gpGlobals->time + 0.62f;
		}
		return;
	}

	if ( m_iChargeSoundOn )
	{
		STOP_SOUND( ENT(m_pPlayer->pev), CHAN_STATIC, "items/medcharge4.wav" );
		m_iChargeSoundOn = 0;
	}

	if ( bFull )
		return;

	while (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < iCap && m_flRechargeTime < gpGlobals->time)
	{
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]++;
#ifndef CLIENT_DLL
		m_flRechargeTime += 0.25 * g_pGameRules->WeaponMultipler();
#endif
	}
}


void CDualHgun::WeaponIdle( void )
{
	Reload( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
	if (flRand <= 0.75)
	{
		iAnim = DUAL_HGUN_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 30.0 / 16 * (2);
	}
	else if (flRand <= 0.875)
	{
		iAnim = DUAL_HGUN_FIDGETSWAY;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 40.0 / 16.0;
	}
	else
	{
		iAnim = DUAL_HGUN_FIDGETSHAKE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 35.0 / 16.0;
	}
	SendWeaponAnim( iAnim, 1, 1 );
}

void CDualHgun::ProvideSingleItem(CBasePlayer *pPlayer, const char *item) {
	if (item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	if (!stricmp(item, "weapon_dual_hornetgun")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_hornetgun")) {
			pPlayer->GiveNamedItem("weapon_hornetgun");
			pPlayer->SelectItem("weapon_dual_hornetgun");
		}
	}
#endif
}

void CDualHgun::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_hornetgun");
}


#endif
