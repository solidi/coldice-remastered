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

#define	CHAINSAW_BODYHIT_VOLUME 128
#define	CHAINSAW_WALLHIT_VOLUME 512

#ifdef CHAINSAW
LINK_ENTITY_TO_CLASS( weapon_chainsaw, CChainsaw );
#endif

enum chainsaw_e {
	CHAINSAW_IDLE = 0,
	CHAINSAW_DRAW,
	CHAINSAW_DRAW_EMPTY,
	CHAINSAW_ATTACK_START,
	CHAINSAW_ATTACK_LOOP,
	CHAINSAW_ATTACK_END,
	CHAINSAW_RELOAD,
	CHAINSAW_SLASH1,
	CHAINSAW_SLASH2,
	CHAINSAW_SLASH3,
	CHAINSAW_SLASH4,
	CHAINSAW_EMPTY,
	CHAINSAW_HOLSTER,
};

void CChainsaw::Spawn( )
{
	Precache( );
	m_iId = WEAPON_CHAINSAW;
	SET_MODEL(ENT(pev), "models/w_chainsaw.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
}

void CChainsaw::Precache( void )
{
	PRECACHE_MODEL("models/v_chainsaw.mdl");
	PRECACHE_MODEL("models/w_chainsaw.mdl");
	PRECACHE_MODEL("models/p_chainsaw.mdl");

	PRECACHE_SOUND("chainsaw_draw.wav");
	PRECACHE_SOUND("chainsaw_slash.wav");
	PRECACHE_SOUND("chainsaw_hit.wav");
	PRECACHE_SOUND("chainsaw_attack1_start.wav");
	PRECACHE_SOUND("chainsaw_attack1_loop.wav");
	PRECACHE_SOUND("chainsaw_attack1_end.wav");

	m_usChainsaw = PRECACHE_EVENT ( 1, "events/chainsaw.sc" );
}

int CChainsaw::AddToPlayer( CBasePlayer *pPlayer )
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

int CChainsaw::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 4;
	p->iId = WEAPON_CHAINSAW;
	p->iWeight = CHAINSAW_WEIGHT;
	p->pszDisplayName = "Koshak's Chainsaw";
	return 1;
}

BOOL CChainsaw::Deploy( )
{
	m_flStartThrow = 0;
	m_flReleaseThrow = -1;

	BOOL success = DefaultDeploy( "models/v_chainsaw.mdl", "models/p_chainsaw.mdl", CHAINSAW_DRAW, "crowbar" );
	
	if ( success )
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "chainsaw_draw.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));

	return success;
}

void CChainsaw::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	if (m_flReleaseThrow > 0) {
		m_pPlayer->pev->weapons &= ~(1<<WEAPON_CHAINSAW);
		SetThink( &CChainsaw::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
	} else {
		SendWeaponAnim( CHAINSAW_HOLSTER );
	}
}

void CChainsaw::PrimaryAttack()
{
	if (!m_flStartThrow && !Swing( 1, TRUE ))
	{
		SetThink( &CChainsaw::SwingAgain );
		pev->nextthink = gpGlobals->time + 0.25;
	}
}

void CChainsaw::SecondaryAttack()
{
	if (!m_flStartThrow) {
		m_flStartThrow = CHAINSAW_ATTACK_START;
		SendWeaponAnim( CHAINSAW_ATTACK_START );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "chainsaw_attack1_start.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		m_flNextSecondaryAttack = m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.25;
	} else if (m_flStartThrow == CHAINSAW_ATTACK_START) {
		SendWeaponAnim( CHAINSAW_ATTACK_LOOP );
		m_flStartThrow = CHAINSAW_ATTACK_END;
		m_flNextSecondaryAttack = m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.25;
	} else if (m_flStartThrow == CHAINSAW_ATTACK_END) {
		Swing( 1, FALSE );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "chainsaw_attack1_loop.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		m_flNextSecondaryAttack = m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.25;
	}
}

void CChainsaw::Smack( )
{
	UTIL_Sparks( m_trHit.vecEndPos );
	DecalGunshot( &m_trHit, BULLET_PLAYER_CHAINSAW );
}

void CChainsaw::SwingAgain( void )
{
	Swing( 0, TRUE );
}

int CChainsaw::Swing( int fFirst, BOOL animation )
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

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usChainsaw, 
	0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0,
	0.0, animation, 0.0 );

	if ( tr.flFraction >= 1.0 )
	{
		if (fFirst)
		{
			// miss
			m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
			
			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		if (animation) {
			switch( ((m_iSwing++) % 3) + 1 )
			{
			case 0:
				SendWeaponAnim( CHAINSAW_SLASH1 ); break;
			case 1:
				SendWeaponAnim( CHAINSAW_SLASH2 ); break;
			case 2:
				SendWeaponAnim( CHAINSAW_SLASH3 ); break;
			case 3:
				SendWeaponAnim( CHAINSAW_SLASH4 ); break;
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
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgChainsaw, gpGlobals->v_forward, &tr, DMG_ALWAYSGIB ); 
		}
		else
		{
			// subsequent swings do half
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgChainsaw / 2, gpGlobals->v_forward, &tr, DMG_ALWAYSGIB ); 
		}	
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "chainsaw_hit.wav", 1, ATTN_NORM);
				m_pPlayer->m_iWeaponVolume = CHAINSAW_BODYHIT_VOLUME;
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
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_CHAINSAW);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "chainsaw_hit.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));

			// delay the decal a bit
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * CHAINSAW_WALLHIT_VOLUME;
#endif
		m_flNextPrimaryAttack = GetNextAttackDelay(0.25);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
		
		SetThink( &CChainsaw::Smack );
		pev->nextthink = UTIL_WeaponTimeBase() + 0.2;
	}

	return fDidHit;
}

void CChainsaw::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if (m_flStartThrow)
	{
		SendWeaponAnim( CHAINSAW_ATTACK_END );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "chainsaw_attack1_end.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		m_flStartThrow = 0;
		m_flNextSecondaryAttack = m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
		return;
	}

	SendWeaponAnim( CHAINSAW_IDLE );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 4.5;
}
