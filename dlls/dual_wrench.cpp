/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*	Use, distribution, and modification of this source code and/or resulting
*	object code is restricted to non-commercial enhancements to products from
*	Valve LLC.  All other use, distribution, or modification is prohibited
*	without written permission from Valve LLC.
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
#include "game.h"

#define	WRENCH_BODYHIT_VOLUME 128
#define	WRENCH_WALLHIT_VOLUME 512

#ifdef DUALWRENCH
LINK_ENTITY_TO_CLASS( weapon_dual_wrench, CDualWrench );
#endif

enum dual_wrench_e {
	DUAL_WRENCH_IDLE= 0,
	DUAL_WRENCH_ATTACK1HIT,
	DUAL_WRENCH_ATTACK1MISS,
	DUAL_WRENCH_ATTACK2HIT,
	DUAL_WRENCH_ATTACK2MISS,
	DUAL_WRENCH_ATTACK3HIT,
	DUAL_WRENCH_ATTACK3MISS,
	DUAL_WRENCH_PULL_BACK,
	DUAL_WRENCH_THROW,
	DUAL_WRENCH_DRAW_LOWKEY,
	DUAL_WRENCH_DRAW,
	DUAL_WRENCH_HOLSTER,
};

void CDualWrench::Spawn( )
{
	Precache( );
	m_iId = WEAPON_DUAL_WRENCH;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_DUAL_WRENCH - 1;
	m_iClip = -1;

	FallInit();// get ready to fall down.
}

void CDualWrench::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_wrench.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_dual_wrench.mdl");

	PRECACHE_SOUND("wrench_hit1.wav");
	PRECACHE_SOUND("wrench_hit2.wav");
	PRECACHE_SOUND("wrench_hitbod1.wav");
	PRECACHE_SOUND("wrench_hitbod2.wav");
	PRECACHE_SOUND("wrench_hitbod3.wav");
	PRECACHE_SOUND("wrench_miss1.wav");

	m_usWrench = PRECACHE_EVENT ( 1, "events/dual_wrench.sc" );
}

int CDualWrench::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CDualWrench::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 5;
	p->iPosition = 0;
	p->iId = WEAPON_DUAL_WRENCH;
	p->iWeight = WRENCH_WEIGHT * 2;
	p->pszDisplayName = "40 Pound Monkey Wrenches";
	return 1;
}

BOOL CDualWrench::DeployLowKey( )
{
	m_flStartThrow = 0;
	m_flReleaseThrow = -1;
	return DefaultDeploy( "models/v_dual_wrench.mdl", "models/p_dual_wrench.mdl", DUAL_WRENCH_DRAW_LOWKEY, "dual_club" );
}

BOOL CDualWrench::Deploy( )
{
	m_flStartThrow = 0;
	m_flReleaseThrow = -1;
	return DefaultDeploy( "models/v_dual_wrench.mdl", "models/p_dual_wrench.mdl", DUAL_WRENCH_DRAW, "dual_club" );
}

BOOL CDualWrench::CanSlide()
{
	return m_flReleaseThrow < 1;
}

void CDualWrench::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	if (m_flReleaseThrow > 0) {
		m_pPlayer->m_iWeapons2 &= ~(1<<(WEAPON_DUAL_WRENCH - 32));
		SetThink( &CDualWrench::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
	} else {
		SendWeaponAnim( DUAL_WRENCH_HOLSTER );
	}
}

void CDualWrench::PrimaryAttack()
{
	if (!m_flStartThrow && !Swing( 1 ))
	{
		SetThink( &CDualWrench::SwingAgain );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CDualWrench::SecondaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if ( !m_flStartThrow ) {
		SendWeaponAnim( DUAL_WRENCH_PULL_BACK );
		m_pPlayer->pev->punchangle = Vector(-2, -2, 0);
		m_flStartThrow = 1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
	}

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
}

void CDualWrench::Throw() {
	// Don't throw underwater, and only throw if we were able to detatch
	// from player.
	if ( (m_pPlayer->pev->waterlevel != 3) )
	{
		// Important! Capture globals before it is stomped on.
		Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
		UTIL_MakeVectors( anglesAim );

		// Get the origin, direction, and fix the angle of the throw.
		Vector vecSrc = m_pPlayer->GetGunPosition( )
					+ gpGlobals->v_right * 12
					+ gpGlobals->v_up * 8
					+ gpGlobals->v_forward * 16;

		Vector vecDir = gpGlobals->v_forward;
		Vector vecAng = UTIL_VecToAngles (vecDir);
		vecAng.z = vecDir.z - 90;

		// Create a flying wrenches
		CFlyingWrench *pWrench = (CFlyingWrench *)Create( "flying_wrench",
					vecSrc, Vector(0,0,0), m_pPlayer->edict() );

		Vector vecSrc2 = m_pPlayer->GetGunPosition( )
					+ gpGlobals->v_right * -12
					+ gpGlobals->v_up * 8
					+ gpGlobals->v_forward * 16;

		CFlyingWrench *pWrench2 = (CFlyingWrench *)Create( "flying_wrench",
					vecSrc2, Vector(0,0,0), m_pPlayer->edict() );

		// Give the wrench its velocity, angle, and spin.
		// Lower the gravity a bit, so it flys.
		if (pWrench)
		{
			pWrench->pev->velocity = vecDir * RANDOM_LONG(900,1000) + m_pPlayer->pev->velocity;
			pWrench->pev->angles = vecAng;
			pWrench->pev->avelocity.x = RANDOM_LONG(-1000,-700);
			pWrench->pev->gravity = RANDOM_FLOAT(0.25,0.35);
		}

		if (pWrench2)
		{
			pWrench2->pev->velocity = vecDir * RANDOM_LONG(800,1000) + m_pPlayer->pev->velocity;
			pWrench2->pev->angles = vecAng;
			pWrench2->pev->avelocity.x = RANDOM_LONG(-1000,-700);
			pWrench2->pev->gravity = RANDOM_FLOAT(0.25,0.35);
		}

		// Do player weapon anim and sound effect.
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		m_pPlayer->pev->punchangle = Vector(-4, -4, -6);

		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON,
			"wrench_miss1.wav", 1, ATTN_NORM, 0,
			94 + RANDOM_LONG(0,0xF));
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM,
			"wrench_miss1.wav", 1, ATTN_NORM, 0,
			94 + RANDOM_LONG(0,0xF));
	}
}

void CDualWrench::Smack( )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_WRENCH );
}

void CDualWrench::SwingAgain( void )
{
	Swing( 0 );
}

int CDualWrench::Swing( int fFirst )
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

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usWrench, 
	0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0,
	0.0, 0, 0.0 );


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
		switch( ((m_iSwing++) % 2) + 1 )
		{
		case 0:
			SendWeaponAnim( DUAL_WRENCH_ATTACK1HIT );// break;
		case 1:
			SendWeaponAnim( DUAL_WRENCH_ATTACK2HIT ); break;
		case 2:
			SendWeaponAnim( DUAL_WRENCH_ATTACK3HIT ); break;
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		
#ifndef CLIENT_DLL

		// hit
		fDidHit = TRUE;
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		ClearMultiDamage( );

		float flDamage = 0;
		if (pEntity->pev->deadflag != DEAD_FAKING && FBitSet(pEntity->pev->flags, FL_FROZEN)) {
			pEntity->pev->renderamt = 100;
			flDamage = 200;
			::IceExplode(pEntity, DMG_FREEZE);
		}

		if ( (m_flNextPrimaryAttack + 1 < UTIL_WeaponTimeBase() ) || g_pGameRules->IsMultiplayer() )
		{
			// first swing does full damage
			pEntity->TraceAttack(m_pPlayer->pev, (gSkillData.plrDmgWrench * 2) + flDamage, gpGlobals->v_forward, &tr, DMG_CLUB );
		}
		else
		{
			// subsequent swings do half
			pEntity->TraceAttack(m_pPlayer->pev, ((gSkillData.plrDmgWrench * 2) / 2) + flDamage, gpGlobals->v_forward, &tr, DMG_CLUB );
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
				switch( RANDOM_LONG(0,2) )
				{
				case 0:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "wrench_hitbod1.wav", 1, ATTN_NORM); break;
				case 1:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "wrench_hitbod2.wav", 1, ATTN_NORM); break;
				case 2:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "wrench_hitbod3.wav", 1, ATTN_NORM); break;
				}
				m_pPlayer->m_iWeaponVolume = WRENCH_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
				{
					  GetNextAttackDelay(0.25);
					  return TRUE;
				} else
					  flVol = 0.1;

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_WRENCH);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play wrench strike
			switch( RANDOM_LONG(0,1) )
			{
			case 0:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "wrench_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "wrench_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
				break;
			}

			// delay the decal a bit
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * WRENCH_WALLHIT_VOLUME;
#endif
		m_flNextPrimaryAttack = GetNextAttackDelay(0.25);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		
		SetThink( &CDualWrench::Smack );
		pev->nextthink = UTIL_WeaponTimeBase() + 0.2;

		
	}
	return fDidHit;
}

void CDualWrench::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flStartThrow == 1 )
	{
		SendWeaponAnim( DUAL_WRENCH_THROW );
#ifndef CLIENT_DLL
		Throw();
#endif
		m_flStartThrow = 2;
		m_flReleaseThrow = 1;
		m_flTimeWeaponIdle = GetNextAttackDelay(0.75);// ensure that the animation can finish playing
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.75);
		return;
	}
	else if ( m_flReleaseThrow > 0 )
	{
#ifndef CLIENT_DLL
		if (infiniteammo.value) {
			m_flStartThrow = 0;
			m_flReleaseThrow = -1;
			SendWeaponAnim( DUAL_WRENCH_DRAW );
		} else
			RetireWeapon();
#endif
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		return;
	}

	int iAnim;
	float flAnimTime = 5.34; // Only WRENCH_IDLE has a different time
	//switch ( RANDOM_LONG( 0, 2 ) )
	//{
	//case 0:
		iAnim = DUAL_WRENCH_IDLE;
		flAnimTime = 2.7;
		//break;
	//case 1:
		//iAnim = WRENCH_IDLE2;
		//break;
	//case 2:
		//iAnim = WRENCH_IDLE3;
		//break;
	//}
	SendWeaponAnim( iAnim, 1 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flAnimTime;
}

void CDualWrench::ProvideSingleItem(CBasePlayer *pPlayer, const char *item) {
	if (item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	if (!stricmp(item, "weapon_dual_wrench")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_wrench")) {
			ALERT(at_aiconsole, "Give weapon_wrench!\n");
			pPlayer->GiveNamedItem("weapon_wrench");
			pPlayer->SelectItem("weapon_dual_wrench");
		}
	}
#endif
}

void CDualWrench::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_wrench");
}
