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
#include "game.h"

enum dual_smg_e
{	
	DUAL_SMG_IDLE = 0,
	DUAL_SMG_FIRE_BOTH1,
	DUAL_SMG_RELOAD,
	DUAL_SMG_DEPLOY_LOWKEY,
	DUAL_SMG_DEPLOY,
	DUAL_SMG_HOLSTER,
};

#ifdef DUALSMG
LINK_ENTITY_TO_CLASS( weapon_dual_smg, CDualSMG );
#endif

void CDualSMG::Spawn( )
{
	Precache( );
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_DUAL_SMG;
	m_iId = WEAPON_DUAL_SMG;

	m_iDefaultAmmo = SMG_DEFAULT_GIVE * 2;

	FallInit();// get ready to fall down.
}

const char *CDualSMG::pHansSounds[] = 
{
	"smg_selected.wav",
	"smg_gruber_nolossoflife.wav",
	"smg_gruber_nicesuit.wav",
	"smg_gruber_doitmyself.wav",
	"smg_gruber_shes.wav",
	"smg_gruber_nolossoflife.wav",
	"smg_gruber_shootglass.wav",
	"smg_gruber_timemagazine.wav",
	"smg_gruber_hohoho.wav",
	"smg_gruber_troublesome.wav",
};

void CDualSMG::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_smg.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_dual_smg.mdl");

	m_iShell = PRECACHE_MODEL ("models/w_shell.mdl");// brass shellTE_MODEL

	PRECACHE_MODEL("models/w_9mmARclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("items/clipinsert1.wav");
	PRECACHE_SOUND("items/cliprelease1.wav");

	PRECACHE_SOUND ("smg_fire.wav");

	PRECACHE_SOUND ("weapons/357_cock1.wav");

	PRECACHE_SOUND_ARRAY(pHansSounds);

	m_usSmg = PRECACHE_EVENT( 1, "events/dual_smg.sc" );
}

int CDualSMG::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = SMG_MAX_CLIP * 2;
	p->iSlot = 5;
	p->iPosition = 3;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_DUAL_SMG;
	p->iWeight = SMG_WEIGHT * 2;
	p->pszDisplayName = "Hans Gruber's Submachine guns";

	return 1;
}

int CDualSMG::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

BOOL CDualSMG::DeployLowKey( )
{
	m_sFireCount = 0;
	return DefaultDeploy( "models/v_dual_smg.mdl", "models/p_dual_smg.mdl", DUAL_SMG_DEPLOY_LOWKEY, "akimbo" );
}

BOOL CDualSMG::Deploy( )
{
	m_sFireCount = 0;

	BOOL result = DefaultDeploy( "models/v_dual_smg.mdl", "models/p_dual_smg.mdl", DUAL_SMG_DEPLOY, "akimbo" );

#ifndef CLIENT_DLL
	if (result && allowvoiceovers.value) {
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, RANDOM_SOUND_ARRAY(pHansSounds), 1.0, ATTN_NORM );
	}
#endif

	return result;
}

void CDualSMG::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.25;
	SendWeaponAnim( DUAL_SMG_HOLSTER );
}

void CDualSMG::PrimaryAttack()
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
		PlayEmptySound();
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
	Vector vecDir;

#ifdef CLIENT_DLL
	if ( !bIsMultiplayer() )
#else
	if ( !g_pGameRules->IsMultiplayer() )
#endif
	{
		// optimized multiplayer. Widened to make it easier to hit a moving player
		vecDir = m_pPlayer->FireBulletsPlayer( 2, vecSrc, vecAiming, VECTOR_CONE_6DEGREES, 8192, BULLET_PLAYER_MP5, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	}
	else
	{
		// single player spread
		vecDir = m_pPlayer->FireBulletsPlayer( 2, vecSrc, vecAiming, VECTOR_CONE_3DEGREES, 8192, BULLET_PLAYER_MP5, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	}

  int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usSmg, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = GetNextAttackDelay(0.1);

	if ( m_flNextPrimaryAttack < UTIL_WeaponTimeBase() )
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.1;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CDualSMG::Reload( void )
{
	if ( m_pPlayer->ammo_9mm <= 0 )
		return;

	DefaultReload( SMG_MAX_CLIP * 2, DUAL_SMG_RELOAD, 2.8 );
}

void CDualSMG::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	SendWeaponAnim( DUAL_SMG_IDLE );

	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 ); // how long till we do this again.
}

void CDualSMG::ProvideSingleItem(CBasePlayer *pPlayer, const char *item) {
	if (item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	if (!stricmp(item, "weapon_dual_smg")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_smg")) {
			ALERT(at_aiconsole, "Give weapon_smg!\n");
			pPlayer->GiveNamedItem("weapon_smg");
			pPlayer->SelectItem("weapon_dual_smg");
		}
	}
#endif
}

void CDualSMG::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_smg");
}
