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
#include "effects.h"
#include "gamerules.h"
#include "game.h"

extern int gmsgStatusIcon;

enum vest_radio_e {
	VEST_RADIO_IDLE1 = 0,
	VEST_RADIO_FIDGET1,
	VEST_RADIO_DRAW_LOWKEY,
	VEST_RADIO_DRAW,
	VEST_RADIO_FIRE,
	VEST_RADIO_HOLSTER
};

static const float VEST_PROX_SCAN_RATE = 0.1f;
static const float VEST_DETONATE_STAGE_TIME = 1.0f;
static const float VEST_FAST_DETONATE_SCALE = 0.5f;

void CVest::SetDangerStatusIcon( BOOL enabled )
{
#ifndef CLIENT_DLL
	if (!m_pPlayer)
		return;

	MESSAGE_BEGIN(MSG_ONE, gmsgStatusIcon, NULL, m_pPlayer->edict());
		WRITE_BYTE(enabled ? 1 : 0);
		WRITE_STRING("cam_danger");
	MESSAGE_END();
#endif
}

void CVest::UpdateProximityIndicator( BOOL enabled )
{
#ifndef CLIENT_DLL
	if (!m_pPlayer)
		return;

	if (enabled)
	{
		if (m_hProximityIndicator)
			return;

		CSprite *pIndicator = CSprite::SpriteCreate( "sprites/glow01.spr", m_pPlayer->pev->origin, FALSE );
		if (pIndicator)
		{
			pIndicator->SetTransparency( kRenderGlow, 255, 32, 32, 220, kRenderFxStrobeFast );
			pIndicator->SetScale( 0.25f );
			pIndicator->SetAttachment( m_pPlayer->edict(), 2 );
			m_hProximityIndicator = pIndicator;
		}
	}
	else if (m_hProximityIndicator)
	{
		CBaseEntity *pIndicator = m_hProximityIndicator;
		if (pIndicator)
		{
			UTIL_Remove( pIndicator );
		}
		m_hProximityIndicator = NULL;
	}
#endif
}

void CVest::SetProximityMode( BOOL enabled )
{
	m_fProximityMode = enabled;
	SetDangerStatusIcon( enabled );
	UpdateProximityIndicator( enabled );
}

BOOL CVest::FindProximityViolator( void )
{
	if (!m_pPlayer)
		return FALSE;

	const float flDamage = pev->dmg > 0 ? pev->dmg : gSkillData.plrDmgVest;
	const float flRadius = flDamage * 2.5f;

	CBaseEntity *pTarget = NULL;
	while ((pTarget = UTIL_FindEntityInSphere( pTarget, m_pPlayer->pev->origin, flRadius )) != NULL)
	{
		if (pTarget == m_pPlayer)
			continue;
		if (pTarget->pev->takedamage == DAMAGE_NO)
			continue;
		if (pTarget->pev->health <= 0)
			continue;
		if (!pTarget->IsPlayer() && !(pTarget->pev->flags & FL_MONSTER))
			continue;

		// Do not detonate on targets that are currently protected from damage.
		if (FBitSet(pTarget->pev->flags, FL_GODMODE))
			continue;

		// Treat invisible targets as protected for proximity purposes.
		if (FBitSet(pTarget->pev->effects, EF_NODRAW) || pTarget->pev->rendermode == kRenderTransAlpha)
			continue;

		if (pTarget->IsPlayer() && g_pGameRules)
		{
			if (!g_pGameRules->FPlayerCanTakeDamage((CBasePlayer *)pTarget, m_pPlayer))
				continue;
		}

		return TRUE;
	}

	return FALSE;
}

void CVest::BeginDetonationSequence( BOOL accelerated )
{
	if (m_fDetonationStarted)
		return;

	if (!m_pPlayer)
		return;

	m_fDetonationStarted = TRUE;
	m_fAcceleratedDetonation = accelerated;

#ifndef CLIENT_DLL
	if (allowvoiceovers.value)
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "vest_attack.wav", 1, ATTN_NORM);
#endif

	SetDangerStatusIcon( TRUE );

	SendWeaponAnim( VEST_RADIO_FIRE );
	const float flScale = m_fAcceleratedDetonation ? VEST_FAST_DETONATE_SCALE : 1.0f;

#ifndef CLIENT_DLL
	SetThink( &CVest::BlowThink );
	pev->nextthink = gpGlobals->time + (VEST_DETONATE_STAGE_TIME * flScale * g_pGameRules->WeaponMultipler());
#endif

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + (2.0f * flScale);
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase();
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (2.0f * flScale);
}

void CVest::ProximityThink( void )
{
	if (!m_pPlayer || !m_pPlayer->IsAlive())
	{
		SetProximityMode( FALSE );
		pev->nextthink = -1;
		return;
	}

	if (!m_fProximityMode || m_fDetonationStarted)
	{
		pev->nextthink = -1;
		return;
	}

	if (FindProximityViolator())
	{
		BeginDetonationSequence( TRUE );
		return;
	}

	pev->nextthink = gpGlobals->time + VEST_PROX_SCAN_RATE;
}

int CVest::AddToPlayer( CBasePlayer *pPlayer )
{
	m_fProximityMode = FALSE;
	m_fAcceleratedDetonation = FALSE;
	m_fDetonationStarted = FALSE;
	m_hProximityIndicator = NULL;

	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

void CVest::Spawn( )
{
	Precache( );
	m_iId = WEAPON_VEST;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_VEST - 1;

	m_iDefaultAmmo = SATCHEL_DEFAULT_GIVE;
	pev->dmg = gSkillData.plrDmgVest;
	m_fProximityMode = FALSE;
	m_fAcceleratedDetonation = FALSE;
	m_fDetonationStarted = FALSE;
	m_hProximityIndicator = NULL;
	m_iLightning = 0;

	FallInit();
}

void CVest::Precache( void )
{
	PRECACHE_MODEL("models/v_vest_radio.mdl");

	PRECACHE_SOUND("vest_attack.wav");
	PRECACHE_SOUND("vest_alive.wav");
	PRECACHE_SOUND("vest_equip.wav");
	PRECACHE_SOUND("buttons/blip1.wav");
	PRECACHE_MODEL("sprites/glow01.spr");
}

int CVest::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Dynamite";
	p->iMaxAmmo1 = 1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 6;
	p->iFlags = ITEM_FLAG_SINGLE_HAND;
	p->iId = m_iId = WEAPON_VEST;
	p->iWeight = VEST_WEIGHT;
	p->pszDisplayName = "Leeroy Jenkins Dynamite Vest";

	return 1;
}

BOOL CVest::IsUseable( void )
{
	if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] > 0 ) 
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CVest::CanDeploy( void )
{
	if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] > 0 ) 
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CVest::DeployLowKey( )
{
	m_fDetonationStarted = FALSE;
	m_fAcceleratedDetonation = FALSE;
	SetProximityMode( FALSE );

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.25;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	return DefaultDeploy( "models/v_vest_radio.mdl", "models/p_weapons.mdl", VEST_RADIO_DRAW_LOWKEY, "hive" );
}

BOOL CVest::Deploy( )
{
	m_fDetonationStarted = FALSE;
	m_fAcceleratedDetonation = FALSE;
	SetProximityMode( FALSE );

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "vest_equip.wav", 1, ATTN_NORM);
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.25;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );

	return DefaultDeploy( "models/v_vest_radio.mdl", "models/p_weapons.mdl", VEST_RADIO_DRAW, "hive" );
}

void CVest::Holster( int skiplocal )
{
	SetProximityMode( FALSE );
	m_fDetonationStarted = FALSE;
	m_fAcceleratedDetonation = FALSE;

#ifndef CLIENT_DLL
	if (allowvoiceovers.value)
		STOP_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "vest_attack.wav");
	pev->nextthink = -1;
#endif
	CBasePlayerWeapon::DefaultHolster(VEST_RADIO_HOLSTER);
}

void CVest::PrimaryAttack()
{
	BeginDetonationSequence( FALSE );
}

void CVest::Reload()
{
	if (m_fDetonationStarted)
		return;

	if (m_fProximityMode)
		return;

	SendWeaponAnim( VEST_RADIO_FIRE );
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "buttons/blip1.wav", 1, ATTN_NORM);
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	SetProximityMode( TRUE );
	SetThink( &CVest::ProximityThink );
	pev->nextthink = gpGlobals->time + VEST_PROX_SCAN_RATE;

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.35f;
	m_flNextPrimaryAttack = m_pPlayer->m_flNextAttack;
	m_flNextSecondaryAttack = m_pPlayer->m_flNextAttack;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
}

void CVest::BlowThink() {
	SendWeaponAnim( VEST_RADIO_FIRE );
	SetThink( &CVest::GoneThink );

	for ( int i = 0; i < RANDOM_LONG(1,3); i++ ) {
		Create( "spark_shower", pev->origin, pev->angles, NULL );
	}

	m_pPlayer->SetAnimation( PLAYER_JUMP );
#ifndef CLIENT_DLL
	const float flScale = m_fAcceleratedDetonation ? VEST_FAST_DETONATE_SCALE : 1.0f;
	pev->nextthink = gpGlobals->time + (VEST_DETONATE_STAGE_TIME * flScale * g_pGameRules->WeaponMultipler());
#endif
}

void CVest::GoneThink() {
	if (!m_pPlayer)
		return;

	SetProximityMode( FALSE );
	m_fDetonationStarted = FALSE;
	m_fAcceleratedDetonation = FALSE;

	CGrenade::Vest( m_pPlayer->pev, pev->origin, gSkillData.plrDmgVest );

	if (!FBitSet(m_pPlayer->pev->flags, FL_GODMODE))
	{
		if (m_pPlayer && m_pPlayer->IsAlive())
		{
			ClearMultiDamage();
			m_pPlayer->pev->health = 0; // without this, player can walk as a ghost.
			m_pPlayer->Killed(m_pPlayer->pev, pev, GIB_ALWAYS);
		}
	}

#ifndef CLIENT_DLL
	if (allowvoiceovers.value)
		STOP_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "vest_attack.wav");

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, m_pPlayer->pev->origin );
		WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( m_pPlayer->pev->origin.x);
		WRITE_COORD( m_pPlayer->pev->origin.y);
		WRITE_COORD( m_pPlayer->pev->origin.z);
		WRITE_COORD( m_pPlayer->pev->origin.x);
		WRITE_COORD( m_pPlayer->pev->origin.y);
		WRITE_COORD( m_pPlayer->pev->origin.z + 600 );
		WRITE_SHORT( g_sModelLightning );
		WRITE_BYTE( 0 ); // startframe
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 4 ); // life
		WRITE_BYTE( 32 );  // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 192 );   // r, g, b
		WRITE_BYTE( 128 ); // brightness
		WRITE_BYTE( 0 );		// speed
	MESSAGE_END();
#endif
}

void CVest::SecondaryAttack()
{
	SetProximityMode( FALSE );
	m_fDetonationStarted = FALSE;
	m_fAcceleratedDetonation = FALSE;

	pev->nextthink = -1;
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "vest_alive.wav", 1, ATTN_NORM);
	SendWeaponAnim( VEST_RADIO_HOLSTER );

	SetThink( &CVest::RetireThink );
	pev->nextthink = gpGlobals->time + 1.0;

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 2.0;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
}

void CVest::RetireThink( )
{
	SetProximityMode( FALSE );
	m_fDetonationStarted = FALSE;
	m_fAcceleratedDetonation = FALSE;
	RetireWeapon();
}

void CVest::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	SendWeaponAnim( VEST_RADIO_FIDGET1 );
	strcpy( m_pPlayer->m_szAnimExtention, "hive" );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );// how long till we do this again.
}


LINK_ENTITY_TO_CLASS( weapon_vest, CVest );

