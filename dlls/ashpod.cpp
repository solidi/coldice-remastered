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

enum ashpod_e {
	PORTALGUN_IDLE = 0,
	PORTALGUN_SHOOT1,
	PORTALGUN_SHOOT2,
	PORTALGUN_SHOOT3,
	PORTALGUN_SHOTLAST,
	PORTALGUN_RELOAD,
	PORTALGUN_HOLSTER,
	PORTALGUN_DRAW_LOWKEY,
	PORTALGUN_DRAW,
	PORTALGUN_SHOOT1_STILL,
	PORTALGUN_SHOOT2_STILL,
};

#ifdef ASHPOD
LINK_ENTITY_TO_CLASS( weapon_ashpod, CAshpod );
LINK_ENTITY_TO_CLASS( weapon_portalgun, CAshpod );
#endif

void CAshpod::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_ashpod");
	Precache( );
	m_iId = WEAPON_ASHPOD;
	SET_MODEL(ENT(pev), "models/w_portalgun.mdl");
	//SetBodygroup(0, WEAPON_ASHPOD - 1);
	//SetBodygroup(1, WEAPON_ASHPOD - 1);

	FallInit();// get ready to fall down.
}

void CAshpod::Precache( void )
{
	PRECACHE_MODEL("models/v_portalgun.mdl");
	PRECACHE_MODEL("models/w_portalgun.mdl");
	PRECACHE_MODEL("models/p_portalgun.mdl");

	m_iShell = PRECACHE_MODEL ("models/w_shell.mdl");// brass shell

	PRECACHE_SOUND("portalgun_shoot_blue.wav");
	PRECACHE_SOUND("portalgun_shoot_red.wav");

	PRECACHE_SOUND ("handgun.wav");
	PRECACHE_SOUND ("handgun_silenced.wav");

	m_iIceTrail = PRECACHE_MODEL("sprites/ice_plasmatrail.spr");
	m_iTrail = PRECACHE_MODEL("sprites/plasmatrail.spr");

	UTIL_PrecacheOther("ent_portal");
}

int CAshpod::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CAshpod::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 8;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_ASHPOD;
	p->iWeight = ASHPOD_WEIGHT;
	p->pszDisplayName = " Aperture Science Handheld Portal Device";

	return 1;
}

BOOL CAshpod::DeployLowKey( )
{
	return DefaultDeploy( "models/v_portalgun.mdl", "models/p_portalgun.mdl", PORTALGUN_DRAW_LOWKEY, "gauss", 0 );
}

BOOL CAshpod::Deploy( )
{
	return DefaultDeploy( "models/v_portalgun.mdl", "models/p_portalgun.mdl", PORTALGUN_DRAW, "gauss", 0 );
}

void CAshpod::Holster( int skiplocal )
{
	pev->nextthink = -1;
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( PORTALGUN_HOLSTER );
}

void CAshpod::PrimaryAttack()
{
	PortalFire(0);
}

void CAshpod::SecondaryAttack()
{
	PortalFire(1);
}

void CAshpod::PortalFire( int state )
{
	SendWeaponAnim(1);

	TraceResult tr;
	UTIL_MakeVectors(m_pPlayer->pev->v_angle);
	Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs;
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 8192;

	UTIL_TraceLine(vecSrc, vecEnd, ignore_monsters, ENT(m_pPlayer->pev), &tr);
	if (tr.flFraction < 1.0f)
	{
		Vector angle;
		UTIL_VectorAngles(tr.vecPlaneNormal, angle);

		auto pPortal = CBaseEntity::Create("ent_portal", tr.vecEndPos - gpGlobals->v_forward * 3, angle, m_pPlayer->edict());
		if (pPortal)
		{
#ifndef CLIENT_DLL
			if (state)
				EMIT_SOUND(ENT(pev), CHAN_WEAPON, "portalgun_shoot_red.wav", 1, ATTN_NORM);
			else
				EMIT_SOUND(ENT(pev), CHAN_WEAPON, "portalgun_shoot_blue.wav", 1, ATTN_NORM);

			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_BEAMPOINTS );
				WRITE_COORD( vecSrc.x );
				WRITE_COORD( vecSrc.y );
				WRITE_COORD( vecSrc.z );
				WRITE_COORD( pPortal->pev->origin.x );
				WRITE_COORD( pPortal->pev->origin.y );
				WRITE_COORD( pPortal->pev->origin.z );
				if (icesprites.value)
					WRITE_SHORT(m_iIceTrail);	// model
				else
					WRITE_SHORT(m_iTrail);	// model
				WRITE_BYTE( 1 );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 2 );
				WRITE_BYTE( 10 );
				WRITE_BYTE( 0 );
				if (exit) {
					WRITE_BYTE( 0 );   // r, g, b
					WRITE_BYTE( 113 );   // r, g, b
					WRITE_BYTE( 230 );   // r, g, b
				} else {
					WRITE_BYTE( 230 );   // r, g, b
					WRITE_BYTE( 0 );   // r, g, b
					WRITE_BYTE( 110 );   // r, g, b
				}
				WRITE_BYTE( 185 ); // Brightness
				WRITE_BYTE( 10 );
			MESSAGE_END( );
#endif

			pPortal->pev->skin = state;

#ifndef CLIENT_DLL
			if (m_pPlayer->m_pPortal[state])
				UTIL_Remove(m_pPlayer->m_pPortal[state]);
			m_pPlayer->m_pPortal[state] = pPortal;
#endif
		}
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.2);
}

void CAshpod::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	// only idle if the slid isn't back
	if (m_iClip != 0)
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.0, 1.0 );

		if (flRand <= 0.3 + 0 * 0.75)
		{
			iAnim = PORTALGUN_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 49.0 / 16;
		}
		else if (flRand <= 0.6 + 0 * 0.875)
		{
			iAnim = PORTALGUN_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 60.0 / 16.0;
		}
		else
		{
			iAnim = PORTALGUN_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 40.0 / 16.0;
		}
		SendWeaponAnim( iAnim, 1 );
	}
}
