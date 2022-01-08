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

#define	FISTS_BODYHIT_VOLUME 128
#define	FISTS_WALLHIT_VOLUME 512
#define SHORYUKEN 3

#ifdef FISTS
LINK_ENTITY_TO_CLASS( weapon_fists, CFists );
#endif

enum fists_e {
	FISTS_IDLE = 0,
	FISTS_DRAW,
	FISTS_HOLSTER,
	FISTS_ATTACK1HIT,
	FISTS_ATTACK1MISS,
	FISTS_ATTACK2MISS,
	FISTS_ATTACK2HIT,
	FISTS_ATTACK3MISS,
	FISTS_ATTACK3HIT,
	FISTS_IDLE2,
	FISTS_IDLE3
};

void CFists::Spawn( )
{
	Precache( );
	m_iId = WEAPON_FISTS;
	SET_MODEL(ENT(pev), "models/w_crowbar.mdl");

	m_iClip = -1;
	FallInit();// get ready to fall down.
}

void CFists::Precache( void )
{
	PRECACHE_MODEL("models/v_fists.mdl");
	PRECACHE_SOUND("fists_hit.wav");
	PRECACHE_SOUND("fists_hitbod.wav");
	PRECACHE_SOUND("fists_miss.wav");
	PRECACHE_SOUND("fists_shoryuken.wav");

	m_usFists = PRECACHE_EVENT ( 1, "events/fists.sc" );
}

int CFists::AddToPlayer( CBasePlayer *pPlayer )
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

int CFists::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 0;
	p->iId = WEAPON_FISTS;
	p->iWeight = FISTS_WEIGHT;
	p->pszDisplayName = "Manos de Piedras";
	return 1;
}

BOOL CFists::Deploy( )
{
	return DefaultDeploy( "models/v_fists.mdl", iStringNull, FISTS_DRAW, "crowbar" );
}

void CFists::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( FISTS_HOLSTER );
}

void CFists::PrimaryAttack()
{
	if (! Swing( 1 ))
	{
		SetThink( &CFists::SwingAgain );
		pev->nextthink = gpGlobals->time + 0.25;
	}
}

void CFists::SecondaryAttack()
{
	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity + (gpGlobals->v_up * 300);
	Swing( SHORYUKEN );
	m_flNextSecondaryAttack = GetNextAttackDelay(1.0);
}

void CFists::Smack( )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_FIST );
}

void CFists::SwingAgain( void )
{
	Swing( 0 );
}

int CFists::Swing( int fFirst )
{
	int fDidHit = FALSE;

	TraceResult tr;

	UTIL_MakeVectors (m_pPlayer->pev->v_angle);
	Vector vecSrc	= m_pPlayer->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				UTIL_FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}
#endif

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usFists, 
	0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0,
	0.0, fFirst == SHORYUKEN, 0.0 );

	if ( tr.flFraction >= 1.0 )
	{
		if (fFirst)
		{
			// miss
			m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
			
			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		if (fFirst == SHORYUKEN) {
			SendWeaponAnim( FISTS_ATTACK3HIT );
		} else {
			switch( ((m_iSwing++) % 2) )
			{
			case 0:
				SendWeaponAnim( FISTS_ATTACK1HIT ); break;
			case 1:
				SendWeaponAnim( FISTS_ATTACK2HIT ); break;
			}
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		
#ifndef CLIENT_DLL

		// hit
		fDidHit = TRUE;
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		ClearMultiDamage( );

		if ( (m_flNextPrimaryAttack + 1 < UTIL_WeaponTimeBase() ) || g_pGameRules->IsMultiplayer() )
		{
			// first swing does full damage
			pEntity->TraceAttack(m_pPlayer->pev, fFirst == SHORYUKEN ? gSkillData.plrDmgShoryuken : gSkillData.plrDmgFists, gpGlobals->v_forward, &tr, DMG_CLUB );
		}
		else
		{
			// subsequent swings do half
			pEntity->TraceAttack(m_pPlayer->pev, fFirst == SHORYUKEN ? gSkillData.plrDmgShoryuken : gSkillData.plrDmgFists / 2, gpGlobals->v_forward, &tr, DMG_CLUB );
		}	
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// play thwack or smack sound
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "fists_hitbod.wav", 1, ATTN_NORM);
				m_pPlayer->m_iWeaponVolume = FISTS_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
					  return TRUE;
				else
					  flVol = 0.1;

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_CROWBAR);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play fist strike
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "fists_hit.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));

			// delay the decal a bit
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * FISTS_WALLHIT_VOLUME;
#endif
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.5);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		
		SetThink( &CFists::Smack );
		pev->nextthink = gpGlobals->time + 0.2;
	}
	return fDidHit;
}

void CFists::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	float flAnimTime = 5.34; // Only FISTS_IDLE has a different time
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		iAnim = FISTS_IDLE;
		flAnimTime = 2.7;
		break;
	case 1:
		iAnim = FISTS_IDLE2;
		break;
	case 2:
		iAnim = FISTS_IDLE3;
		break;
	}
	SendWeaponAnim( iAnim, 1 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flAnimTime;
}
