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
#include "decals.h"
#include "game.h"

#define RAILGUN_PRIMARY_FIRE_VOLUME	450
#define RAIL_BEAM_SPRITE "sprites/xbeam1.spr"

extern DLL_GLOBAL const char *g_MutatorPaintball;

enum dual_railgun_e {
	DUAL_RAILGUN_IDLE = 0,
	DUAL_RAILGUN_FIRE_BOTH,
	DUAL_RAILGUN_FIRE_RIGHT,
	DUAL_RAILGUN_FIRE_LEFT,
	DUAL_RAILGUN_HOLSTER,
	DUAL_RAILGUN_DRAW_LOWKEY,
	DUAL_RAILGUN_DRAW
};

#ifdef DUALRAILGUN
LINK_ENTITY_TO_CLASS( weapon_dual_railgun, CDualRailgun );
#endif

//=========================================================
//=========================================================

void CDualRailgun::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_dual_railgun"); 
	Precache( );
	m_iId = WEAPON_DUAL_RAILGUN;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_DUAL_RAILGUN - 1;

	m_iDefaultAmmo = RAILGUN_DEFAULT_GIVE * 2;

	FallInit();
}

void CDualRailgun::Precache( void )
{
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/v_dual_railgun.mdl");
	PRECACHE_MODEL("models/p_dual_railgun.mdl");

	PRECACHE_SOUND("railgun_fire.wav");
	
	PRECACHE_MODEL( RAIL_BEAM_SPRITE );	
	m_iGlow = PRECACHE_MODEL ( "sprites/blueflare1.spr" );
	m_iBalls = PRECACHE_MODEL( "sprites/blueflare1.spr" );
	m_iBeam = PRECACHE_MODEL ( "sprites/smoke.spr" );
}

int CDualRailgun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CDualRailgun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 5;
	p->iPosition = 5;
	p->iId = m_iId = WEAPON_DUAL_RAILGUN;
	p->iFlags = 0;
	p->iWeight = RAILGUN_WEIGHT * 2;
	p->pszDisplayName = "Quake II Railguns";

	return 1;
}

BOOL CDualRailgun::DeployLowKey( )
{
	return DefaultDeploy( "models/v_dual_railgun.mdl", "models/p_dual_railgun.mdl", DUAL_RAILGUN_DRAW_LOWKEY, "dual_shotgun" );
}

BOOL CDualRailgun::Deploy( )
{
	return DefaultDeploy( "models/v_dual_railgun.mdl", "models/p_dual_railgun.mdl", DUAL_RAILGUN_DRAW, "dual_shotgun" );
}

void CDualRailgun::Holster( int skiplocal )
{
	CBasePlayerWeapon::DefaultHolster(DUAL_RAILGUN_HOLSTER);
}

void CDualRailgun::PrimaryAttack()
{
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 2)
	{
		PlayEmptySound( );
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 2;

	int right = 16;
	if (!m_iAltFire) {
		SendWeaponAnim( DUAL_RAILGUN_FIRE_RIGHT );
	} else {
		SendWeaponAnim( DUAL_RAILGUN_FIRE_LEFT );
		right = -12;
	}
	m_iAltFire++;
	if (m_iAltFire > 1) m_iAltFire = 0;

	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecSrc = m_pPlayer->GetGunPosition();
#ifndef CLIENT_DLL
	StartFire(vecAiming, vecSrc, (gpGlobals->v_up * -12 + gpGlobals->v_right * right + vecAiming * 32));
#endif
	m_pPlayer->pev->punchangle.x = RANDOM_LONG(-5, -8);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.5); 
}

void CDualRailgun::SecondaryAttack()
{
	if (m_pPlayer->pev->waterlevel == 3 || m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 2)
	{
		PlayEmptySound( );
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 4;

	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	SendWeaponAnim( DUAL_RAILGUN_FIRE_BOTH );
#ifndef CLIENT_DLL
	StartFire(vecAiming, vecSrc, (gpGlobals->v_up * -12 + gpGlobals->v_right * 16 + vecAiming * 32));
	SetThink( &CDualRailgun::FireThink );
	pev->nextthink = gpGlobals->time + (0.2 * g_pGameRules->WeaponMultipler());
#endif

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.75); 
}

void CDualRailgun::FireThink( void )
{
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	StartFire(vecAiming, vecSrc, (gpGlobals->v_up * -12 + gpGlobals->v_right * -12 + vecAiming * 32));
	m_pPlayer->pev->punchangle.x = RANDOM_LONG(-8, -12);
}

void CDualRailgun::CreateTrail(Vector a, Vector b)
{
	CBeam *m_pBeam = CBeam::BeamCreate( RAIL_BEAM_SPRITE, 40 );
	m_pBeam->PointsInit(b,a);
	m_pBeam->SetFlags( BEAM_FSINE );
	m_pBeam->SetColor( 0, 113, 230 );
	m_pBeam->LiveForTime( 0.25 );
	m_pBeam->SetScrollRate ( 12 );
	m_pBeam->SetNoise( 20 );
	m_pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;

	CBeam *m_inBeam = CBeam::BeamCreate( RAIL_BEAM_SPRITE, 20 );
	m_inBeam->PointsInit(b,a);
	m_inBeam->SetFlags( BEAM_FSINE );
	m_inBeam->SetColor( 200, 200, 200 );
	m_inBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;
	m_inBeam->LiveForTime( 0.25 );
	m_inBeam->SetScrollRate ( 12 );
	m_inBeam->SetNoise( 10 );

	CBeam *m_lBeam = CBeam::BeamCreate( RAIL_BEAM_SPRITE, 50 );
	m_lBeam->PointsInit(b,a);
	m_lBeam->SetFlags( BEAM_FSINE );
	m_lBeam->SetColor( 200, 200, 200 );
	m_lBeam->SetScrollRate ( 12 );
	m_lBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;
	m_lBeam->LiveForTime( 0.25 );
}

void CDualRailgun::StartFire( Vector vecAiming, Vector vecSrc, Vector effectSrc)
{	
	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "railgun_fire.wav", 0.9, ATTN_NORM);

	Fire( vecSrc, vecAiming, effectSrc, gSkillData.plrDmgRailgun );
}

void CDualRailgun::Fire( Vector vecSrc, Vector vecDir, Vector effectSrc, float flDamage )
{
	m_pPlayer->m_iWeaponVolume = RAILGUN_PRIMARY_FIRE_VOLUME;

	Vector vecDest = vecSrc + effectSrc + vecDir * 4096;
	edict_t *pentIgnore = ENT(m_pPlayer->pev);
	TraceResult tr;

#ifndef CLIENT_DLL
	int nMaxPunchThroughs = RANDOM_LONG(2,4);
	while (nMaxPunchThroughs > 0)
	{
		UTIL_TraceLine(vecSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr);
		if (tr.flFraction > 0.02) // no trail when too close to an entity
			CreateTrail(vecSrc + effectSrc, tr.vecEndPos);

		effectSrc = Vector(0,0,0);
		//if (tr.fAllSolid)
		//	break;

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
			// Make some balls and a decal
#ifndef CLIENT_DLL
			int decal = DECAL_GUNSHOT1 + RANDOM_LONG(0,4);
			if (strstr(mutators.string, g_MutatorPaintball) ||
				atoi(mutators.string) == MUTATOR_PAINTBALL) {
				decal = DECAL_PAINT1 + RANDOM_LONG(0, 7);
			}
			UTIL_DecalTrace(&tr, decal);
#endif

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

			nMaxPunchThroughs--; // only when through solids.
		}

		// Increase next position
		vecSrc = tr.vecEndPos + vecDir;
		pentIgnore = ENT(pEntity->pev);
	}

	m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * 200;
#endif
}

void CDualRailgun::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	ResetEmptySound( );

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	SendWeaponAnim( DUAL_RAILGUN_IDLE );
}

void CDualRailgun::ProvideSingleItem(CBasePlayer *pPlayer, const char *item) {
	if (item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	if (!stricmp(item, "weapon_dual_railgun")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_railgun")) {
			ALERT(at_aiconsole, "Give weapon_railgun!\n");
			pPlayer->GiveNamedItem("weapon_railgun");
			pPlayer->SelectItem("weapon_dual_railgun");
		}
	}
#endif
}

void CDualRailgun::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_railgun");
}
