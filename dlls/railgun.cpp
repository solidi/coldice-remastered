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
#include "decals.h"
#include "game.h"

#define RAILGUN_PRIMARY_FIRE_VOLUME	450
#define RAIL_BEAM_SPRITE "sprites/xbeam1.spr"
#define RAIL_NOVA_CHARGE_SOUND "weapons/egon_windup2.wav"
#define RAIL_NOVA_AMMO_COST 40
#define RAIL_NOVA_CHARGE_TIME 3.0f
#define RAIL_NOVA_DAMAGE 220.0f
#define RAIL_NOVA_RADIUS 280.0f

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

#ifndef CLIENT_DLL
static int g_iRailNovaSprite = 0;
static int g_iRailNovaIceSprite = 0;

static Vector RailgunTraceImpactPoint( CBasePlayer *pPlayer, const Vector &vecSrc, const Vector &vecDir, const Vector &effectSrc )
{
	Vector vecDest = vecSrc + effectSrc + vecDir * 4096;
	TraceResult tr;
	UTIL_TraceLine( vecSrc, vecDest, dont_ignore_monsters, ENT(pPlayer->pev), &tr );
	return tr.vecEndPos;
}

static void RailgunSpawnNovaExplosion( CBasePlayer *pPlayer, const Vector &origin )
{
	if ( !pPlayer )
		return;

	EMIT_SOUND_DYN( ENT(pPlayer->pev), CHAN_WEAPON, "nuke_explosion.wav", 0.8f, ATTN_NORM, 0, PITCH_NORM );
	UTIL_ScreenShake( origin, 20.0f, 180.0f, 1.0f, 400.0f );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, origin );
		WRITE_BYTE( TE_BEAMDISK );
		WRITE_COORD( origin.x );
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z + 16 );
		WRITE_COORD( origin.x );
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z + RAIL_NOVA_RADIUS * 2.0f );
		WRITE_SHORT( g_sModelLightning );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 16 );
		WRITE_BYTE( 60 );
		WRITE_BYTE( 64 );
		if ( icesprites.value )
		{
			WRITE_BYTE( 0 );
			WRITE_BYTE( 113 );
			WRITE_BYTE( 230 );
		}
		else
		{
			WRITE_BYTE( 120 );
			WRITE_BYTE( 200 );
			WRITE_BYTE( 255 );
		}
		WRITE_BYTE( 255 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, origin );
		WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( origin.x );
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z + 16 );
		WRITE_COORD( origin.x );
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z + RAIL_NOVA_RADIUS * 2.0f );
		WRITE_SHORT( g_sModelLightning );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 16 );
		WRITE_BYTE( 60 );
		WRITE_BYTE( 64 );
		if ( icesprites.value )
		{
			WRITE_BYTE( 0 );
			WRITE_BYTE( 113 );
			WRITE_BYTE( 230 );
		}
		else
		{
			WRITE_BYTE( 120 );
			WRITE_BYTE( 200 );
			WRITE_BYTE( 255 );
		}
		WRITE_BYTE( 255 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( origin.x );
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z + 128 );
		if ( icesprites.value )
			WRITE_SHORT( g_iRailNovaIceSprite );
		else
			WRITE_SHORT( g_iRailNovaSprite );
		WRITE_BYTE( 32 );
		WRITE_BYTE( 24 );
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	RadiusDamage( origin, pPlayer->pev, pPlayer->pev, RAIL_NOVA_DAMAGE, RAIL_NOVA_RADIUS, CLASS_NONE, DMG_RADIATION | DMG_BLAST | DMG_ALWAYSGIB, TRUE );
}
#endif

//=========================================================
//=========================================================

void CRailgun::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_railgun"); 
	Precache( );
	m_iId = WEAPON_RAILGUN;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_RAILGUN - 1;

	m_iDefaultAmmo = RAILGUN_DEFAULT_GIVE;
	pev->dmg = gSkillData.plrDmgRailgun;

	FallInit();
}

void CRailgun::Precache( void )
{
	PRECACHE_MODEL("models/v_railgun.mdl");

	PRECACHE_SOUND("railgun_fire2.wav");
	PRECACHE_SOUND( RAIL_NOVA_CHARGE_SOUND );
	PRECACHE_SOUND( "nuke_explosion.wav" );
	PRECACHE_SOUND( "weapons/egon_off1.wav" );
	PRECACHE_MODEL( RAIL_BEAM_SPRITE );
	PRECACHE_MODEL( "sprites/blueflare1.spr" );

#ifndef CLIENT_DLL
	g_iRailNovaSprite = PRECACHE_MODEL( "sprites/nuke2.spr" );
	g_iRailNovaIceSprite = PRECACHE_MODEL( "sprites/ice_nuke2.spr" );
#endif

	m_usRailgunFire = PRECACHE_EVENT( 1, "events/railgun.sc" );
}

int CRailgun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
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
	p->iFlags = ITEM_FLAG_SINGLE_HAND;
	p->iWeight = RAILGUN_WEIGHT;
	p->pszDisplayName = "Quake II Railgun";

	return 1;
}

BOOL CRailgun::DeployLowKey( )
{
	m_fInAttack = 0;
	return DefaultDeploy( "models/v_railgun.mdl", "models/p_weapons.mdl", RAILGUN_DRAW_LOWKEY, "gauss" );
}

BOOL CRailgun::Deploy( )
{
	m_fInAttack = 0;
	return DefaultDeploy( "models/v_railgun.mdl", "models/p_weapons.mdl", RAILGUN_DRAW, "gauss" );
}

void CRailgun::Holster( int skiplocal )
{
	CancelChargedShot( FALSE );
	CBasePlayerWeapon::DefaultHolster(RAILGUN_HOLSTER);
}

void CRailgun::PrimaryAttack()
{
	if ( m_fInAttack == 1 )
	{
		if ( m_pPlayer->pev->button & IN_RELOAD )
			Reload();
		else
			CancelChargedShot();
		return;
	}

	if (m_pPlayer->pev->waterlevel == 3 || m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 2)
	{
		PlayEmptySound( );
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 2;

	StartFire();

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(1.0); 
}

void CRailgun::SecondaryAttack()
{
	if ( m_fInAttack == 1 )
	{
		if ( m_pPlayer->pev->button & IN_RELOAD )
			Reload();
		else
			CancelChargedShot();
		return;
	}

	if (m_pPlayer->pev->waterlevel == 3 || m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 2)
	{
		PlayEmptySound( );
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 2;

	StartFire();

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
}

void CRailgun::Reload( void )
{
	if ( m_fInAttack != 1 && ( m_flNextPrimaryAttack > UTIL_WeaponTimeBase() || m_flNextSecondaryAttack > UTIL_WeaponTimeBase() ) )
		return;

	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_fInAttack == 1 )
			CancelChargedShot();
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		m_pPlayer->m_flNextAttack = m_flNextPrimaryAttack;
		return;
	}

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < RAIL_NOVA_AMMO_COST )
	{
		if ( m_fInAttack == 1 )
			CancelChargedShot( FALSE );
		PlayEmptySound();
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.25f;
		m_pPlayer->m_flNextAttack = m_flNextPrimaryAttack;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.3f;
		return;
	}

	if ( m_fInAttack != 1 )
	{
		m_fInAttack = 1;
		m_pPlayer->m_flStartCharge = gpGlobals->time;
		m_flChargeSoundTime = gpGlobals->time;
		m_flChargeAnimTime = gpGlobals->time;
		SendWeaponAnim( RAILGUN_SPINUP, 0 );
		m_pPlayer->m_iWeaponVolume = RAILGUN_PRIMARY_FIRE_VOLUME;
		EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_STATIC, RAIL_NOVA_CHARGE_SOUND, 0.95f, ATTN_NORM, 0, 90 );
	}

	float flChargeFrac = ( gpGlobals->time - m_pPlayer->m_flStartCharge ) / RAIL_NOVA_CHARGE_TIME;
	if ( flChargeFrac < 0.0f )
		flChargeFrac = 0.0f;
	else if ( flChargeFrac > 1.0f )
		flChargeFrac = 1.0f;

	if ( gpGlobals->time >= m_flChargeSoundTime )
	{
		int iPitch = 90 + (int)( flChargeFrac * 95.0f );
		if ( iPitch > 185 )
			iPitch = 185;
		EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_STATIC, RAIL_NOVA_CHARGE_SOUND, 0.95f, ATTN_NORM, SND_CHANGE_PITCH, iPitch );
		m_flChargeSoundTime = gpGlobals->time + 0.08f;
	}

	if ( gpGlobals->time >= m_flChargeAnimTime )
	{
		SendWeaponAnim( RAILGUN_SPIN, 0 );
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		m_pPlayer->pev->punchangle = m_pPlayer->pev->punchangle + Vector( RANDOM_FLOAT( -0.3f, 0.3f ), RANDOM_FLOAT( -0.6f, 0.6f ), 0 );
		m_flChargeAnimTime = gpGlobals->time + 0.12f;
	}

	if ( flChargeFrac >= 1.0f )
	{
		FireChargedShot();
		return;
	}

	m_flNextSecondaryAttack = m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.05f;
	m_pPlayer->m_flNextAttack = m_flNextPrimaryAttack;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1f;
}

void CRailgun::CancelChargedShot( BOOL bPlayStopSound )
{
	if ( m_fInAttack != 1 )
		return;

	m_fInAttack = 0;
	STOP_SOUND( ENT(m_pPlayer->pev), CHAN_STATIC, RAIL_NOVA_CHARGE_SOUND );
	if ( bPlayStopSound )
		EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/egon_off1.wav", 0.8f, ATTN_NORM, 0, 110 );
	SendWeaponAnim( RAILGUN_IDLE2, 0 );
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.25f;
	m_pPlayer->m_flNextAttack = m_flNextPrimaryAttack;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.4f;
}

void CRailgun::FireChargedShot( void )
{
	if ( m_fInAttack != 1 )
		return;

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < RAIL_NOVA_AMMO_COST )
	{
		CancelChargedShot( FALSE );
		PlayEmptySound();
		return;
	}

	m_fInAttack = 0;
	STOP_SOUND( ENT(m_pPlayer->pev), CHAN_STATIC, RAIL_NOVA_CHARGE_SOUND );
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= RAIL_NOVA_AMMO_COST;

	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	Vector effectSrc = gpGlobals->v_up * -12 + gpGlobals->v_right * 12 + vecAiming * 32;

#ifndef CLIENT_DLL
	Vector vecImpact = RailgunTraceImpactPoint( m_pPlayer, vecSrc, vecAiming, effectSrc );
#endif

	StartFire();

#ifndef CLIENT_DLL
	RailgunSpawnNovaExplosion( m_pPlayer, vecImpact );
#endif

	m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(2.0f);
	m_pPlayer->m_flNextAttack = m_flNextPrimaryAttack;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0f;
}

void CRailgun::StartFire( void )
{
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	SendWeaponAnim( RAILGUN_FIRE2, 0 );

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Fire( vecSrc, vecAiming, (gpGlobals->v_up * -12 + gpGlobals->v_right * 12 + vecAiming * 32), gSkillData.plrDmgRailgun );

	m_pPlayer->pev->punchangle.x = RANDOM_LONG(-5, -8);
}

void CRailgun::Fire( Vector vecSrc, Vector vecDir, Vector effectSrc, float flDamage )
{
	m_pPlayer->m_iWeaponVolume = RAILGUN_PRIMARY_FIRE_VOLUME;

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usRailgunFire, 0.0,
		(float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles,
		flDamage, 0.0, 0, 0, 0, 0 );

	Vector vecDest = vecSrc + effectSrc + vecDir * 4096;
	edict_t *pentIgnore = ENT(m_pPlayer->pev);
	TraceResult tr;

#ifndef CLIENT_DLL
	int nMaxPunchThroughs = 3;
	BOOL firstBeam = FALSE;
	while (nMaxPunchThroughs > 0)
	{
		// Reflection
		if (firstBeam)
		{
			float n, flMaxFrac = 1.0;
			n = -DotProduct( tr.vecPlaneNormal, vecDir);
			if (n < 0.5) // 60 degrees
			{
				Vector r;
				VectorMA( vecDir, 2.0 * n, tr.vecPlaneNormal, r );
				flMaxFrac = flMaxFrac - tr.flFraction;
				VectorCopy( r, vecDir );
				VectorMA( tr.vecEndPos, 8.0, vecDir, vecSrc );
				VectorMA( vecSrc, 8192.0, vecDir, vecDest );
			}
		}
		firstBeam = TRUE;

		UTIL_TraceLine(vecSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr);

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
			// Keep server decals authoritative while visual beam/trail is client event-driven.
			int decal = DECAL_GUNSHOT1 + RANDOM_LONG(0,4);
			if (g_pGameRules->MutatorEnabled(MUTATOR_PAINTBALL)) {
				decal = DECAL_PAINT1 + RANDOM_LONG(0, 7);
			}
			UTIL_DecalTrace(&tr, decal);

			nMaxPunchThroughs--; // only when through solids.
		}

		// Increase next position
		vecSrc = tr.vecEndPos + vecDir;
		pentIgnore = ENT(pEntity->pev);
	}

	if (!FBitSet(m_pPlayer->pev->flags, FL_ONGROUND))
		m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * 500;
#endif
}

void CRailgun::WeaponIdle( void )
{
	if ( m_fInAttack == 1 )
	{
		CancelChargedShot();
		return;
	}

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

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

void CRailgun::ProvideDualItem(CBasePlayer *pPlayer, const char *item) {
	if (pPlayer == NULL || item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	CBasePlayerWeapon::ProvideDualItem(pPlayer, item);

	if (!stricmp(item, "weapon_railgun")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_railgun")) {
			pPlayer->GiveNamedItem("weapon_dual_railgun");
			pPlayer->SelectItem("weapon_dual_railgun");
		}
	}
#endif
}

void CRailgun::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_dual_railgun");
}
