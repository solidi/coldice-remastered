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
#include "game.h"

enum snowball_e {
	SNOWBALL_IDLE = 0,
	SNOWBALL_FIDGET,
	SNOWBALL_PINPULL,
	SNOWBALL_THROW1,	// toss
	SNOWBALL_THROW2,	// medium
	SNOWBALL_THROW3,	// hard
	SNOWBALL_HOLSTER,
	SNOWBALL_DRAW_LOWKEY,
	SNOWBALL_DRAW,	// with select sound, bro.
};

#ifdef SNOWBALL
LINK_ENTITY_TO_CLASS( weapon_snowball, CSnowball );
#endif

void CSnowball::Spawn( )
{
	Precache( );
	m_iId = WEAPON_SNOWBALL;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_SNOWBALL - 1;

#ifndef CLIENT_DLL
	pev->dmg = gSkillData.plrDmgSnowball;
#endif

	m_iDefaultAmmo = SNOWBALL_DEFAULT_GIVE;
	pev->dmg = gSkillData.plrDmgSnowball;

	FallInit();// get ready to fall down.
}

void CSnowball::Precache( void )
{
	PRECACHE_MODEL("models/v_snowball.mdl");

	UTIL_PrecacheOther( "flying_snowball" );
}

int CSnowball::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Snowball";
	p->iMaxAmmo1 = SNOWBALL_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_SNOWBALL;
	p->iWeight = SNOWBALL_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE | ITEM_FLAG_SINGLE_HAND;
	p->pszDisplayName = "Snowball";

	return 1;
}

int CSnowball::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

BOOL CSnowball::DeployLowKey( )
{
	m_flReleaseThrow = -1;
	return DefaultDeploy( "models/v_snowball.mdl", "models/p_weapons.mdl", SNOWBALL_DRAW_LOWKEY, "crowbar" );
}

BOOL CSnowball::Deploy( )
{
	m_flReleaseThrow = -1;
	return DefaultDeploy( "models/v_snowball.mdl", "models/p_weapons.mdl", SNOWBALL_DRAW, "crowbar" );
}

BOOL CSnowball::CanHolster( void )
{
	// can only holster hand grenades when not primed!
	return ( m_flStartThrow == 0 );
}

void CSnowball::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(-1);

	if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
	{
		SendWeaponAnim( SNOWBALL_HOLSTER );
	}
	else
	{
		m_pPlayer->pev->weapons &= ~(1<<WEAPON_SNOWBALL);
		SetThink( &CSnowball::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
	}

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

void CSnowball::PrimaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] > 0 ) {
		// Reset primary attack if player tapped primary, then secondary
		m_flReleaseThrow = 0;
		m_flStartThrow = 0;

		SendWeaponAnim( SNOWBALL_PINPULL );
		SetThink( &CSnowball::Throw );

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.75);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75;
		pev->nextthink = gpGlobals->time + 0.5;
	} else {
		RetireWeapon();
	}
}

void CSnowball::Throw()
{
	// Don't throw underwater, and only throw if we were able to detatch
	// from player.
	if ( (m_pPlayer->pev->waterlevel != 3) )
	{
		SendWeaponAnim( SNOWBALL_THROW1 );

		// Important! Capture globals before it is stomped on.
		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

		// Get the origin, direction, and fix the angle of the throw.
		Vector vecSrc = m_pPlayer->GetGunPosition( ) + vecAiming + gpGlobals->v_up * 4;

		Vector vecDir = vecAiming;
		Vector vecAng = UTIL_VecToAngles(vecDir);
		vecAng.z = vecDir.z - 90;

		// Create a flying snowball.
		CFlyingSnowball *pSnowball = (CFlyingSnowball *)Create( "flying_snowball",
					vecSrc, vecAiming, m_pPlayer->edict() );

		// Give the wrench its velocity, angle, and spin.
		if (pSnowball)
		{
			pSnowball->pev->velocity = vecAiming * 1200;
			pSnowball->pev->angles = vecAng;
			pSnowball->pev->gravity = 0.1;
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_BEAMFOLLOW );
				WRITE_SHORT(pSnowball->entindex());	// entity
				WRITE_SHORT(PRECACHE_MODEL("sprites/smoke.spr"));	// model
				WRITE_BYTE( 1 ); // life
				WRITE_BYTE( 2 );  // width
				WRITE_BYTE( 224 );   // r, g, b
				WRITE_BYTE( 224 );   // r, g, b
				WRITE_BYTE( 255 );   // r, g, b
				WRITE_BYTE( 50 );	// brightness
			MESSAGE_END();
		}

		// Do player weapon anim and sound effect.
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		m_pPlayer->pev->punchangle = Vector(-2, -2, -4);

		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON,
			"snowball_throw.wav", 1, ATTN_NORM, 0,
			94 + RANDOM_LONG(0,0xF));
		
		m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ]--;
	}
}

void CSnowball::SecondaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if ( !m_flStartThrow && m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] > 0 )
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;

		SendWeaponAnim( SNOWBALL_PINPULL );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
	}
}

void CSnowball::WeaponIdle( void )
{
	if ( m_flReleaseThrow == 0 && m_flStartThrow )
		 m_flReleaseThrow = gpGlobals->time;

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flStartThrow )
	{
		Vector angThrow = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
		
		// Calculate hold time and scale velocity accordingly
		float flHoldTime = gpGlobals->time - m_flStartThrow;
		float flVel = 500 + (flHoldTime * 750); // Start at 500, scale up to 2000 over 2 seconds
		
		// Clamp velocity between min and max
		if (flVel < 500)
			flVel = 500;
		if (flVel > 2000)
			flVel = 2000;

		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + angThrow + gpGlobals->v_up * 4;
		Vector vecThrow = angThrow * flVel + m_pPlayer->pev->velocity;

#ifndef CLIENT_DLL
		CFlyingSnowball::Shoot( m_pPlayer->pev, vecSrc, vecThrow, m_pPlayer );
#endif

		if ( flVel < 500 )
		{
			SendWeaponAnim( SNOWBALL_THROW1 );
		}
		else if ( flVel < 1000 )
		{
			SendWeaponAnim( SNOWBALL_THROW2 );
		}
		else
		{
			SendWeaponAnim( SNOWBALL_THROW3 );
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		m_pPlayer->pev->punchangle = Vector(-2, -2, -4);

		m_flReleaseThrow = 0;
		m_flStartThrow = 0;
		m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		m_flNextSecondaryAttack = GetNextAttackDelay(0.5);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;

		m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ]--;

		if ( !m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
		{
			// just threw last grenade
			// set attack times in the future, and weapon idle in the future so we can see the whole throw
			// animation, weapon idle will automatically retire the weapon for us.
			m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.5);// ensure that the animation can finish playing
		}
		return;
	}
	else if ( m_flReleaseThrow > 0 )
	{
		// we've finished the throw, restart.
		m_flStartThrow = 0;

		if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
		{
			SendWeaponAnim( SNOWBALL_DRAW_LOWKEY );
		}
		else
		{
			RetireWeapon();
			return;
		}

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		m_flReleaseThrow = -1;
		return;
	}

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
		if (flRand <= 0.75)
		{
			iAnim = SNOWBALL_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );// how long till we do this again.
		}
		else 
		{
			iAnim = SNOWBALL_FIDGET;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 75.0 / 30.0;
		}

		SendWeaponAnim( iAnim );
	}
}
