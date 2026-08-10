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

enum glauncher_e
{
	GLAUNCHER_IDLE1 = 0,
	GLAUNCHER_IDLE2,
	GLAUNCHER_DRAW_LOWKEY,
	GLAUNCHER_DRAW,
	GLAUNCHER_HOLSTER,
	GLAUNCHER_RELOAD,
	GLAUNCHER_SHOOT,
	GLAUNCHER_SHOOT2,
};

enum glauncher_primary_mode_e
{
	GLAUNCHER_PRIMARY_CONTACT = 0,
	GLAUNCHER_PRIMARY_BOUNCE,
	GLAUNCHER_PRIMARY_CLUSTER,
	GLAUNCHER_PRIMARY_FREEZE,
	GLAUNCHER_PRIMARY_STICKY_PROX,
	GLAUNCHER_PRIMARY_STICKY_DRUG,
	GLAUNCHER_PRIMARY_MODE_COUNT,
};

static const float GLAUNCHER_BOUNCE_FUSE_TIME = 3.0f;
static const float GLAUNCHER_CLUSTER_FUSE_TIME = 6.0f;
static const float GLAUNCHER_FREEZE_FUSE_TIME = 3.0f;

static const char *GLauncherPrimaryModeName( int iMode )
{
	switch (iMode)
	{
	case GLAUNCHER_PRIMARY_BOUNCE:
		return "Bounce";
	case GLAUNCHER_PRIMARY_CLUSTER:
		return "Cluster";
	case GLAUNCHER_PRIMARY_FREEZE:
		return "Freeze";
	case GLAUNCHER_PRIMARY_STICKY_PROX:
		return "Sticky Proximity";
	case GLAUNCHER_PRIMARY_STICKY_DRUG:
		return "Sticky Drug";
	case GLAUNCHER_PRIMARY_CONTACT:
	default:
		return "Contact";
	}
}

#ifdef GLAUNCHER
LINK_ENTITY_TO_CLASS( weapon_glauncher, CGrenadeLauncher );
#endif

//=========================================================
//=========================================================

void CGrenadeLauncher::Spawn( )
{
	Precache();

	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_GLAUNCHER - 1;

	m_iId = WEAPON_GLAUNCHER;
	m_iDefaultAmmo = GLAUNCHER_DEFAULT_GIVE;
	m_iPrimaryMode = GLAUNCHER_PRIMARY_CONTACT;
	pev->dmg = gSkillData.plrDmgM203Grenade;

	FallInit();// get ready to fall down.
}

void CGrenadeLauncher::Precache( void )
{
	PRECACHE_MODEL("models/v_glauncher.mdl");

	PRECACHE_SOUND( "weapons/glauncher.wav" );
	PRECACHE_SOUND( "weapons/glauncher2.wav" );
	PRECACHE_SOUND( "glauncher_reload.wav" );

	PRECACHE_SOUND("glauncher_bad.wav");

	PRECACHE_SOUND("weapons/357_cock1.wav");

	PRECACHE_SOUND( "m16_glauncher.wav" );
	PRECACHE_SOUND( "m16_glauncher2.wav" );

	// Precache flying_snowball and snowbomb for GAME_SNOWBALL mode
	UTIL_PrecacheOther( "flying_snowball" );
	UTIL_PrecacheOther( "snowbomb" );
	UTIL_PrecacheOther( "freezegrenade" );
	UTIL_PrecacheOther( "monster_satchel" );
	UTIL_PrecacheOther( "monster_proxmine" );

	m_usGrenadeLauncher = PRECACHE_EVENT( 1, "events/glauncher.sc" );
}

int CGrenadeLauncher::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "ARgrenades";
	p->iMaxAmmo1 = M203_GRENADE_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = GLAUNCHER_MAX_CLIP;
	p->iSlot = 3;
	p->iPosition = 5;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLAUNCHER;
	p->iWeight = GLAUNCHER_WEIGHT;
	p->pszDisplayName = "120-Pound Grenade Launcher";

	return 1;
}

int CGrenadeLauncher::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		m_iPrimaryMode = GLAUNCHER_PRIMARY_CONTACT;
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

BOOL CGrenadeLauncher::DeployLowKey( )
{
	BOOL bResult = DefaultDeploy( "models/v_glauncher.mdl", "models/p_weapons.mdl", GLAUNCHER_DRAW_LOWKEY, "mp5" );

	if (bResult)
		PrintPrimaryMode();

	return bResult;
}

BOOL CGrenadeLauncher::Deploy( )
{
	BOOL bResult = DefaultDeploy( "models/v_glauncher.mdl", "models/p_weapons.mdl", GLAUNCHER_DRAW, "mp5" );

	if (bResult)
		PrintPrimaryMode();

	return bResult;
}

void CGrenadeLauncher::Holster( int skiplocal )
{
	CBasePlayerWeapon::DefaultHolster(GLAUNCHER_HOLSTER);
}

void CGrenadeLauncher::CyclePrimaryMode( void )
{
	m_iPrimaryMode++;
	if (m_iPrimaryMode >= GLAUNCHER_PRIMARY_MODE_COUNT)
		m_iPrimaryMode = GLAUNCHER_PRIMARY_CONTACT;

	PrintPrimaryMode();
}

void CGrenadeLauncher::PrintPrimaryMode( void )
{
#ifndef CLIENT_DLL
	if (!m_pPlayer)
		return;

	ClientPrint( m_pPlayer->pev, HUD_PRINTCENTER, UTIL_VarArgs("Launcher Mode: %s\n", GLauncherPrimaryModeName( m_iPrimaryMode )) );
#endif
}

BOOL CGrenadeLauncher::FireSelectedPrimaryMode( const Vector &vecSrc, const Vector &vecAiming )
{
	switch (m_iPrimaryMode)
	{
	case GLAUNCHER_PRIMARY_BOUNCE:
		CGrenade::ShootTimed( m_pPlayer->pev, vecSrc, vecAiming * 800, GLAUNCHER_BOUNCE_FUSE_TIME );
		break;

	case GLAUNCHER_PRIMARY_CLUSTER:
		CGrenade::ShootTimedCluster( m_pPlayer->pev, vecSrc, vecAiming * 800, GLAUNCHER_CLUSTER_FUSE_TIME );
		break;

	case GLAUNCHER_PRIMARY_FREEZE:
	#ifndef CLIENT_DLL
		CFreezeGrenade::ShootTimed( m_pPlayer->pev, vecSrc, vecAiming * 800, GLAUNCHER_FREEZE_FUSE_TIME );
	#else
		// Keep client prediction cadence without depending on server-only freeze entity code.
		CGrenade::ShootTimed( m_pPlayer->pev, vecSrc, vecAiming * 800, GLAUNCHER_FREEZE_FUSE_TIME );
	#endif
		break;

	case GLAUNCHER_PRIMARY_STICKY_PROX:
	case GLAUNCHER_PRIMARY_STICKY_DRUG:
		{
		#ifndef CLIENT_DLL
			CBaseEntity *pPackage = Create( "monster_satchel", vecSrc, vecAiming, m_pPlayer->edict() );
			if (!pPackage)
				return FALSE;

			if (m_iPrimaryMode == GLAUNCHER_PRIMARY_STICKY_DRUG)
				pPackage->pev->spawnflags |= SF_SATCHEL_DRUG_PACKAGE;
			else
				pPackage->pev->spawnflags |= SF_SATCHEL_PROX_PACKAGE;

			pPackage->pev->velocity = vecAiming * 800 + m_pPlayer->pev->velocity;
			pPackage->pev->avelocity.y = RANDOM_LONG(180, 420);
		#endif
		}
		break;

	case GLAUNCHER_PRIMARY_CONTACT:
	default:
		CGrenade::ShootContact( m_pPlayer->pev, vecSrc, vecAiming * 800 );
		break;
	}

	return TRUE;
}

void CGrenadeLauncher::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_DANGER;
	m_pPlayer->m_flStopExtraSoundTime = UTIL_WeaponTimeBase() + 0.2;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + vecAiming * 16;

	int snowballfight = 0;
#ifndef CLIENT_DLL
	snowballfight = g_pGameRules->IsSnowballFight() ||
					g_pGameRules->MutatorEnabled(MUTATOR_SNOWBALL);
#endif

	// GAME_SNOWBALL mode: fire snowballs at rapid pace
	if (snowballfight)
	{
#ifndef CLIENT_DLL
		// Fire a flying snowball projectile
		CFlyingSnowball::Shoot( m_pPlayer->pev, vecSrc, vecAiming * 1500, m_pPlayer );
#endif
		// Rapid fire rate for snowball mode
		m_flNextPrimaryAttack = GetNextAttackDelay(0.3);
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.3;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
	}
	else
	{
		m_iClip--;

		if (!FireSelectedPrimaryMode( vecSrc, vecAiming ))
		{
			m_iClip++;
			PlayEmptySound( );
			m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
			m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.15;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
			return;
		}
		
		m_flNextPrimaryAttack = GetNextAttackDelay(1.3);
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.3;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
	}

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_RELIABLE;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usGrenadeLauncher,  0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0, 0, snowballfight, 0 );

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
}

void CGrenadeLauncher::SecondaryAttack( void )
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_DANGER;
	m_pPlayer->m_flStopExtraSoundTime = UTIL_WeaponTimeBase() + 0.2;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + vecAiming * 16;

	int snowballfight = 0;
#ifndef CLIENT_DLL
	snowballfight = g_pGameRules->IsSnowballFight() ||
					g_pGameRules->MutatorEnabled(MUTATOR_SNOWBALL);
#endif

	// GAME_SNOWBALL mode: fire a bouncing snowbomb that explodes into 6 snowballs
	if (snowballfight)
	{
#ifndef CLIENT_DLL
		// Create and launch a snowbomb
		CSnowbomb *pBomb = CSnowbomb::CreateSnowbomb( vecSrc, vecAiming, m_pPlayer );
		if (pBomb)
		{
			// Slower velocity than regular snowballs (1000 vs 1500)
			pBomb->pev->velocity = vecAiming * 1000;
		}
#endif
	}
	else
	{
		m_iClip--;

		// Normal cluster grenade mode
		if (RANDOM_LONG(0,2) == 0) {
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_VOICE, "glauncher_bad.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));	
		}

		CGrenade::ShootTimedCluster(m_pPlayer->pev, vecSrc, vecAiming * 800, 6 );
	}

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usGrenadeLauncher );
	
	m_flNextSecondaryAttack = GetNextAttackDelay(1.3);
	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.3;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5;// idle pretty soon after shooting.

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
}

void CGrenadeLauncher::Reload( void )
{
	if (m_iClip >= GLAUNCHER_MAX_CLIP)
	{
		if ( m_pPlayer->m_afButtonPressed & IN_RELOAD )
		{
			CyclePrimaryMode();
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/357_cock1.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 100 + RANDOM_LONG(-2,2));
		}
		return;
	}

	BOOL result = DefaultReload( GLAUNCHER_MAX_CLIP, GLAUNCHER_RELOAD, 1.5 );

	if (result) {
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "glauncher_reload.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
	}
}

void CGrenadeLauncher::WeaponIdle( void )
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
		iAnim = GLAUNCHER_IDLE1;
		break;
	
	default:
	case 1:
		iAnim = GLAUNCHER_IDLE2;
		break;
	}

	SendWeaponAnim( iAnim );

	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}
