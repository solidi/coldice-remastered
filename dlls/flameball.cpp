/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology"). Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*	Use, distribution, and modification of this source code and/or resulting
*	object code is restricted to non-commercial enhancements to products from
*	Valve LLC. All other use, distribution, or modification is prohibited
*	without written permission from Valve LLC.
*
****/

/*

	Entry for Half-Life Mod - FlatLine Arena
	Copyright (c) 2005-2019, Napoleon. All rights reserved.
			
===== flameball.cpp ========================================================

	Contains the code for flame effect, used by weapon flamethrower 
	Based on Valve flame code and gargantua monster 
	Modified Valve code. Additional code by Napoleon. 

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "soundent.h"
#include "decals.h"
#include "flameball.h"
#include "game.h"

LINK_ENTITY_TO_CLASS( flameball, CFlameBall );

void CFlameBall::Spawn( void )
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->classname = MAKE_STRING( "flameball" );

	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "sprites/null.spr");
	if ( RANDOM_FLOAT( 0 , 1 ) < 0.5 )
	{
		pev->scale = 1;
	}
	else
	{
		pev->scale = 2;
	}

	UTIL_SetSize(pev, Vector ( 0, 0, 0 ), Vector ( 0, 0, 0 ) );

	pev->dmg = gSkillData.plrDmgFlameThrower;
	m_fRegisteredSound = FALSE;
}

extern int gmsgParticle;

CFlameBall *CFlameBall::ShootFlameBall( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity )
{
	CFlameBall *pFlame = GetClassPtr( (CFlameBall *)NULL );
	pFlame->Spawn();
	// Flames
	pFlame->pev->playerclass = 1;
	pFlame->pev->colormap = pFlame->entindex();

	pFlame->pev->gravity = 1.0;
	UTIL_SetOrigin( pFlame->pev, vecStart );
	pFlame->pev->velocity = vecVelocity;
	pFlame->pev->velocity.x *= RANDOM_FLOAT(0.8, 1);
	pFlame->pev->velocity.y *= RANDOM_FLOAT(0.8, 1);
	pFlame->pev->velocity.z *= RANDOM_FLOAT(0.8, 1);
	pFlame->pev->angles = UTIL_VecToAngles (pFlame->pev->velocity);
	pFlame->pev->owner = ENT(pevOwner);

	pFlame->SetThink( &CFlameBall::DangerSoundThink );
	pFlame->pev->nextthink = gpGlobals->time;

	pFlame->SetTouch( &CFlameBall::ExplodeTouch );

	pFlame->pev->dmg = gSkillData.plrDmgFlameThrower;

#ifndef CLIENT_DLL
	MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
		WRITE_SHORT( pFlame->entindex() );
		WRITE_STRING( "FlameSteam1.aur" );
	MESSAGE_END();
#endif

	return pFlame;
}

void CFlameBall::Explode( Vector vecSrc, Vector vecAim )
{
	TraceResult tr;
	UTIL_TraceLine ( pev->origin, pev->origin + Vector ( 0, 0, -32 ), ignore_monsters, ENT(pev), & tr);

	Explode( &tr, DMG_BURN );
}

void CFlameBall::Explode( TraceResult *pTrace, int bitsDamageType )
{
	float flRndSound;

	pev->model = iStringNull;
	pev->solid = SOLID_NOT;

	pev->takedamage = DAMAGE_NO;

	// Pull out of the wall a bit
	if ( pTrace->flFraction != 1.0 )
	{
		UTIL_SetOrigin(pev, pTrace->vecEndPos + (pTrace->vecPlaneNormal * 60));
	}

	int iContents = UTIL_PointContents ( pev->origin );

#ifndef CLIENT_DLL
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z - 32 );
		if (icesprites.value)
			WRITE_SHORT( g_sModelIndexIceFireball );
		else
			WRITE_SHORT( g_sModelIndexFlame );
		WRITE_BYTE( 10 );
		WRITE_BYTE( 15 );
		WRITE_BYTE( TE_EXPLFLAG_NOSOUND );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_DLIGHT );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_BYTE( 25 );
		if (icesprites.value) {
			WRITE_BYTE( 0 );
			WRITE_BYTE( 113 );
			WRITE_BYTE( 230 );
		} else {
			WRITE_BYTE( 220 );
			WRITE_BYTE( 150 );
			WRITE_BYTE( 0 );
		}
		WRITE_BYTE( 10 );
		WRITE_BYTE( 10 );
	MESSAGE_END();
#endif

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, BIG_EXPLOSION_VOLUME, 3.0 );
	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	pev->owner = NULL;

	::RadiusDamage( pev->origin, pev, pevOwner, pev->dmg, gSkillData.plrDmgFlameThrower * 5, CLASS_NONE, DMG_BURN );

	if ( RANDOM_FLOAT( 0 , 1 ) < 0.5 )
	{
		UTIL_DecalTrace( pTrace, DECAL_SMALLSCORCH1 );
	}
	else
	{
		UTIL_DecalTrace( pTrace, DECAL_SMALLSCORCH3 );
	}

	// Flames ---
	CBaseEntity *pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, 64)) != NULL)
	{
		if (pEntity->pev->takedamage)
		{
			pEntity->m_fBurnTime += RANDOM_LONG(1,3);
			pEntity->m_hFlameOwner = Instance(pevOwner);
		}
	}
	// ---- 

	flRndSound = RANDOM_FLOAT( 0 , 1 );

	EMIT_SOUND(ENT(pev), CHAN_VOICE, "flame_hitwall.wav", 0.55, ATTN_NORM);

	// Flames
	pev->playerclass = 0;

	pev->effects |= EF_NODRAW;
	pev->velocity = g_vecZero;
	SetThink( &CFlameBall::Smoke );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CFlameBall::Smoke( void )
{
#ifndef CLIENT_DLL
	if (UTIL_PointContents ( pev->origin ) == CONTENTS_WATER)
	{
		UTIL_Bubbles( pev->origin - Vector( 64, 64, 64 ), pev->origin + Vector( 64, 64, 64 ), 100 );
	}
	else
	{
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( 10 );
			WRITE_BYTE( 6 );
		MESSAGE_END();
	}
	UTIL_Remove( this );
#endif
}

void CFlameBall::Killed( entvars_t *pevAttacker, int iGib )
{
	Detonate( );
}

void CFlameBall::DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( &CFlameBall::Detonate );
	pev->nextthink = gpGlobals->time;
}

void CFlameBall::PreDetonate( void )
{
	CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin, 400, 0.3 );
	SetThink( &CFlameBall::Detonate );
	pev->nextthink = gpGlobals->time + 1;
}

void CFlameBall::Detonate( void )
{
	TraceResult tr;
	Vector vecSpot;

	vecSpot = pev->origin + Vector ( 0 , 0 , 8 );
	UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -40 ), ignore_monsters, ENT(pev), & tr);

	Explode( &tr, DMG_BURN );
}

void CFlameBall::ExplodeTouch( CBaseEntity *pOther )
{
	TraceResult tr;
	Vector vecSpot;

	pev->enemy = pOther->edict();

	vecSpot = pev->origin - pev->velocity.Normalize() * 32;
	UTIL_TraceLine( vecSpot, vecSpot + pev->velocity.Normalize() * 64, ignore_monsters, ENT(pev), &tr );

	Explode( &tr, DMG_BURN );
}

void CFlameBall::DangerSoundThink( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin + pev->velocity * 0.5, pev->velocity.Length( ), 0.2 );
	pev->nextthink = gpGlobals->time + 0.2;

	if (pev->waterlevel != 0)
	{
		pev->velocity = pev->velocity * 0.5;
	}
}

void CFlameBall::BounceTouch( CBaseEntity *pOther )
{
	if ( pOther->edict() == pev->owner )
		return;

	if (m_flNextAttack < gpGlobals->time && pev->velocity.Length() > 100)
	{
		entvars_t *pevOwner = VARS( pev->owner );
		if (pevOwner)
		{
			TraceResult tr = UTIL_GetGlobalTrace( );
			ClearMultiDamage( );
			pOther->TraceAttack(pevOwner, 1, gpGlobals->v_forward, &tr, DMG_CLUB );
			ApplyMultiDamage( pev, pevOwner);
		}
		m_flNextAttack = gpGlobals->time + 1.0;
	}

	Vector vecTestVelocity;
	vecTestVelocity = pev->velocity;
	vecTestVelocity.z *= 0.45;

	if ( !m_fRegisteredSound && vecTestVelocity.Length() <= 60 )
	{
		CSoundEnt::InsertSound ( bits_SOUND_DANGER, pev->origin, pev->dmg / 0.4, 0.3 );
		m_fRegisteredSound = TRUE;
	}

	if (pev->flags & FL_ONGROUND)
	{
		pev->velocity = pev->velocity * 0.8;
		pev->sequence = RANDOM_LONG( 1, 1 );
	}
	else
	{
		BounceSound();
	}
	pev->framerate = pev->velocity.Length() / 200.0;
	if (pev->framerate > 1.0)
		pev->framerate = 1;
	else if (pev->framerate < 0.5)
		pev->framerate = 0;
}

void CFlameBall::SlideTouch( CBaseEntity *pOther )
{
	if ( pOther->edict() == pev->owner )
		return;

	if (pev->flags & FL_ONGROUND)
	{
		pev->velocity = pev->velocity * 0.95;

		if (pev->velocity.x != 0 || pev->velocity.y != 0)
		{
		}
	}
	else
	{
		BounceSound();
	}
}

void CFlameBall::BounceSound( void )
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "flame_hitwall.wav", 0.25, ATTN_NORM);
}
