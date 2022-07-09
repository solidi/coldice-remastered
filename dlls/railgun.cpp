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
#include "shake.h"
#include "gamerules.h"
#include "effects.h"
#include "customentity.h"

#define RAILGUN_PRIMARY_FIRE_VOLUME	450
#define RAIL_BEAM_SPRITE "sprites/xbeam1.spr"

enum railgun_e {
	RAILGUN_IDLE = 0,
	RAILGUN_IDLE2,
	RAILGUN_FIDGET,
	RAILGUN_SPINUP,
	RAILGUN_SPIN,
	RAILGUN_FIRE,
	RAILGUN_FIRE2,
	RAILGUN_HOLSTER,
	RAILGUN_DRAW_LOWKEY,
	RAILGUN_DRAW
};

#ifdef RAILGUN
LINK_ENTITY_TO_CLASS( weapon_railgun, CRailgun );
#endif

//=========================================================
//=========================================================

void CRailgun::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_railgun"); 
	Precache( );
	m_iId = WEAPON_RAILGUN;
	SET_MODEL(ENT(pev), "models/w_railgun.mdl");

	m_iDefaultAmmo = RAILGUN_DEFAULT_GIVE;

	FallInit();
}

void CRailgun::Precache( void )
{
	PRECACHE_MODEL("models/w_railgun.mdl");
	PRECACHE_MODEL("models/v_railgun.mdl");
	PRECACHE_MODEL("models/p_railgun.mdl");

	PRECACHE_SOUND("railgun_fire.wav");
	
	PRECACHE_MODEL( RAIL_BEAM_SPRITE );	
	m_iGlow = PRECACHE_MODEL ( "sprites/blueflare1.spr" );
	m_iBalls = PRECACHE_MODEL( "sprites/blueflare1.spr" );
	m_iBeam = PRECACHE_MODEL ( "sprites/smoke.spr" );
}

int CRailgun::AddToPlayer( CBasePlayer *pPlayer )
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

int CRailgun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 4;
	p->iId = m_iId = WEAPON_RAILGUN;
	p->iFlags = 0;
	p->iWeight = RAILGUN_WEIGHT;
	p->pszDisplayName = "Quake II Railgun";

	return 1;
}

BOOL CRailgun::DeployLowKey( )
{
	return DefaultDeploy( "models/v_railgun.mdl", "models/p_railgun.mdl", RAILGUN_DRAW_LOWKEY, "gauss" );
}

BOOL CRailgun::Deploy( )
{
	return DefaultDeploy( "models/v_railgun.mdl", "models/p_railgun.mdl", RAILGUN_DRAW, "gauss" );
}

void CRailgun::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( RAILGUN_HOLSTER );
}

void CRailgun::PrimaryAttack()
{
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 2)
	{
		PlayEmptySound( );
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->time + 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 2;

	StartFire();

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_flNextPrimaryAttack = gpGlobals->time + 1.0; 
}

void CRailgun::CreateTrail(Vector a, Vector b)
{
	CBeam *m_pBeam = CBeam::BeamCreate( RAIL_BEAM_SPRITE, 40 );
	m_pBeam->PointsInit(a,b);
	m_pBeam->SetFlags( BEAM_FSINE );
	m_pBeam->SetColor( 0, 113, 230 );
	m_pBeam->LiveForTime( 0.75 );
	m_pBeam->SetScrollRate ( 10 );
	m_pBeam->SetNoise( 20 );
	m_pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;

	CBeam *m_inBeam = CBeam::BeamCreate( RAIL_BEAM_SPRITE, 20 );
	m_inBeam->PointsInit(a,b);
	m_inBeam->SetFlags( BEAM_FSINE );
	m_inBeam->SetColor( 200, 200, 200 );
	m_inBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;
	m_inBeam->LiveForTime( 0.75 );
	m_inBeam->SetScrollRate ( 10 );
	m_inBeam->SetNoise( 10 );

	CBeam *m_lBeam = CBeam::BeamCreate( RAIL_BEAM_SPRITE, 50 );
	m_lBeam->PointsInit(a,b);
	m_lBeam->SetFlags( BEAM_FSINE );
	m_lBeam->SetColor( 200, 200, 200 );
	m_lBeam->SetScrollRate ( 10 );
	m_lBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;
	m_lBeam->LiveForTime( 0.75 );
}

void CRailgun::StartFire( void )
{
	float flDamage;
	
	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( ); 
	
	flDamage = gSkillData.plrDmgRailgun;
	m_pPlayer->pev->punchangle.x = -20;
	
	float flZVel = m_pPlayer->pev->velocity.z;

	SendWeaponAnim( RAILGUN_FIRE2 );

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "railgun_fire.wav", 0.5 + flDamage * (1.0 / 400.0), ATTN_NORM, 0, 85 + RANDOM_LONG(0,0x1f)); 

	Fire( vecSrc, vecAiming, flDamage );
}

void CRailgun::Fire( Vector vecOrigSrc, Vector vecDir, float flDamage )
{
	m_pPlayer->m_iWeaponVolume = RAILGUN_PRIMARY_FIRE_VOLUME;

	Vector vecSrc = vecOrigSrc;
	Vector vecDest = vecSrc + vecDir * 4096;
	edict_t		*pentIgnore;
	TraceResult tr, beam_tr;

	pentIgnore = ENT( m_pPlayer->pev );

	UTIL_TraceLine(vecSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr);
	if (tr.flFraction > 0.02) // no trail when too close to an entity
		CreateTrail(vecSrc + gpGlobals->v_up * -12 + gpGlobals->v_right * 3 + gpGlobals->v_forward * 32, tr.vecEndPos);

	if (tr.fAllSolid)
		return;

	CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

	if (pEntity == NULL)
		return;

	if (pEntity->pev->takedamage)
	{
		ClearMultiDamage();
		pEntity->TraceAttack( m_pPlayer->pev, flDamage, vecDir, &tr, DMG_ENERGYBEAM | DMG_ALWAYSGIB );
		ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);
	}
	else
	{
		m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * 200;

		// Make some balls and a decal
		DecalGunshot( &tr, BULLET_MONSTER_12MM );
		
		MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
			WRITE_BYTE( TE_GLOWSPRITE );
			WRITE_COORD( tr.vecEndPos.x);	// pos
			WRITE_COORD( tr.vecEndPos.y);
			WRITE_COORD( tr.vecEndPos.z);
			WRITE_SHORT( m_iGlow );		    // model
			WRITE_BYTE( 20 );				// life * 10
			WRITE_BYTE( 3 );				// size * 10
			WRITE_BYTE( 200 );			    // brightness
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, tr.vecEndPos );
			WRITE_BYTE( TE_SPRITETRAIL );
			WRITE_COORD( tr.vecEndPos.x );
			WRITE_COORD( tr.vecEndPos.y );
			WRITE_COORD( tr.vecEndPos.z );
			WRITE_COORD( tr.vecEndPos.x + tr.vecPlaneNormal.x );
			WRITE_COORD( tr.vecEndPos.y + tr.vecPlaneNormal.y );
			WRITE_COORD( tr.vecEndPos.z + tr.vecPlaneNormal.z );
			WRITE_SHORT( m_iBalls );		    // model
			WRITE_BYTE( 4 );				    // count
			WRITE_BYTE( 6 );				    // life * 10
			WRITE_BYTE( RANDOM_LONG( 1, 2 ) );	// size * 10
			WRITE_BYTE( 10 );				    // amplitude * 0.1
			WRITE_BYTE( 20 );				    // speed * 100
		MESSAGE_END();	
	}
}

void CRailgun::WeaponIdle( void )
{
	ResetEmptySound( );

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	int iAnim;
	float flRand = RANDOM_FLOAT(0, 1);
	if (flRand <= 0.5)
	{
		iAnim = RAILGUN_IDLE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	}
	else if (flRand <= 0.75)
	{
		iAnim = RAILGUN_IDLE2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	}
	else
	{
		iAnim = RAILGUN_FIDGET;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3;
	}

	SendWeaponAnim( iAnim );
}
