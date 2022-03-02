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
#include "gamerules.h"
#include "decals.h"
#include "plasma.h"
#include "explode.h"

LINK_ENTITY_TO_CLASS( plasma, CPlasma );

CPlasma *CPlasma::CreatePlasmaRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner )
{
	CPlasma *pPlasma = GetClassPtr( (CPlasma *)NULL );
	UTIL_SetOrigin( pPlasma->pev, vecOrigin );
	pPlasma->pev->angles = vecAngles;
	pPlasma->m_iPrimaryMode = TRUE;
	pPlasma->pev->angles = vecAngles;
	pPlasma->pev->owner = pOwner->edict();
	pPlasma->pev->classname = MAKE_STRING("plasma");
	pPlasma->Spawn();
 	pPlasma->pev->angles = vecAngles;
	pPlasma->m_pPlayer = pOwner;

	return pPlasma;
}

//=========================================================
//=========================================================

void CPlasma :: Spawn( void )
{
	Precache( );

	if (m_iPrimaryMode)
		pev->movetype = MOVETYPE_FLY;

	pev->solid = SOLID_BSP;

	SET_MODEL(ENT(pev), "models/plasma.mdl");
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_MakeVectors( pev->angles );
	if( m_bIsAI )
	{
		pev->gravity = 0.5;
		pev->friction = 0.7;
	}
	Glow();

	if (m_iPrimaryMode)
		SetTouch( &CPlasma::RocketTouch );

	SetThink( &CPlasma::FlyThink );
	SetThink( &CPlasma::IgniteThink );
	float flDelay = m_bIsAI ? 4.0 : 2.0;
	pev->dmgtime = gpGlobals->time + flDelay;
	pev->nextthink = gpGlobals->time;

	pev->dmg = gSkillData.plrDmgPlasma;
}

//=========================================================
//=========================================================

void CPlasma :: IgniteThink( void  )
{
	// pev->movetype = MOVETYPE_TOSS;

	pev->movetype = MOVETYPE_FLY;

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );

		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 15 ); // life
		WRITE_BYTE( 15 );  // width
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness

	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	m_flIgniteTime = gpGlobals->time + 0.6;
}

//=========================================================

void CPlasma :: Precache( void )
{
	PRECACHE_MODEL("models/plasma.mdl");
	m_iDrips = PRECACHE_MODEL("sprites/tsplasma.spr");
	m_iGlow = PRECACHE_MODEL("sprites/particles_blue.spr");
	m_iExplode = PRECACHE_MODEL ("sprites/plasmablue5.spr");
	m_iTrail = PRECACHE_MODEL("sprites/plasmatrail.spr");	
	PRECACHE_SOUND ("plasma_hitwall.wav");
}

//=========================================================

void CPlasma :: Glow( void )
{
	m_pSprite = CSprite::SpriteCreate( "sprites/particles_blue.spr", pev->origin, FALSE );
	m_pSprite->SetAttachment( edict(), 0 );
	m_pSprite->pev->scale = 1;
	m_pSprite->pev->frame = 8;
	m_pSprite->pev->rendermode = kRenderTransAdd;
	m_pSprite->pev->renderamt = 255;	
	m_pSprite->SetTransparency( kRenderTransAdd, 0, 113, 230, 100, kRenderFxDistort );
}

//=========================================================

void CPlasma :: FlyThink( void  )
{
	Vector vecEnd = pev->origin.Normalize();

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE_SPRAY );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( pev->origin.x );	// Send to PAS because of the sound
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( vecEnd.x );
		WRITE_COORD( vecEnd.y );
		WRITE_COORD( vecEnd.z );
		WRITE_SHORT( m_iDrips );
		WRITE_BYTE( 2 ); // count
		WRITE_BYTE( 20 ); // speed
		WRITE_BYTE( 80 );
	MESSAGE_END();

	if (pev->waterlevel == 3)
	{
		entvars_t *pevOwner = VARS(pev->owner);
		ClearEffects();
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time;
	}

	if (!m_iPrimaryMode)
	{
		if (pev->dmgtime <= gpGlobals->time)
			Explode();
	}
	pev->nextthink = gpGlobals->time + 0.03;
}

void CPlasma::RocketTouch( CBaseEntity *pOther )
{
	if (pOther->pev->takedamage)
	{
		entvars_t *pevOwner;
		pevOwner = VARS( pev->owner );
		pOther->TakeDamage( pev, pevOwner, pev->dmg / 2, DMG_GENERIC | DMG_PARALYZE | DMG_ENERGYBEAM | DMG_FREEZE );
	}

	Explode();
}

void CPlasma::Explode( void )
{
	if (pev->waterlevel == 3)
	{
		ClearEffects();
		SetThink( &CBaseEntity::SUB_Remove );
		pev->nextthink = gpGlobals->time;
	}

	if ( UTIL_PointContents(pev->origin) == CONTENT_WATER )
	{
		UTIL_Remove( this );
		return;
	}	

	SetTouch( NULL );
	SetThink( NULL );
	EMIT_SOUND(ENT(pev), CHAN_ITEM, "plasma_hitwall.wav", 1, ATTN_NORM);

	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector t = pev->origin - gpGlobals->v_forward * 20;
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( t.x );	// Send to PAS because of the sound
		WRITE_COORD( t.y );
		WRITE_COORD( t.z );
		WRITE_SHORT( m_iExplode );
		WRITE_BYTE( RANDOM_LONG(12, 18) ); // scale * 10
		WRITE_BYTE( 128 ); // framerate
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SMOKE );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( g_sModelIndexSmoke );
		WRITE_BYTE( 10 ); // scale * 10
		WRITE_BYTE( 10  ); // framerate
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE(TE_DLIGHT);
		WRITE_COORD( pev->origin.x );	// X
		WRITE_COORD( pev->origin.y );	// Y
		WRITE_COORD( pev->origin.z );	// Z
		WRITE_BYTE( 15 );		// radius * 0.1
		WRITE_BYTE( 0 );		// r
		WRITE_BYTE( 113 );		// g
		WRITE_BYTE( 230 );		// b
		WRITE_BYTE( 5 );		// time * 10
		WRITE_BYTE( 10 );		// decay * 0.1
	MESSAGE_END( );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_PARTICLEBURST );
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
		WRITE_SHORT( 75 );
		WRITE_BYTE((unsigned short)208);
		WRITE_BYTE( 5 );
	MESSAGE_END();

	entvars_t *pevOwner;
	if ( pev->owner )
		pevOwner = VARS( pev->owner );
	else
		pevOwner = NULL;

	::RadiusDamage ( pev->origin, pev, pevOwner, pev->dmg, 100, CLASS_PLAYER_BIOWEAPON, DMG_GENERIC | DMG_PARALYZE | DMG_ENERGYBEAM | DMG_FREEZE );

	if (m_iPrimaryMode)
	{
		TraceResult tr;
		UTIL_TraceLine( pev->origin, pev->origin + pev->velocity * 10, dont_ignore_monsters, ENT( pev ), &tr );
		UTIL_DecalTrace(&tr, DECAL_SMALLSCORCH3);
	}

	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 0.3;
	UTIL_Remove ( this );
	UTIL_Remove( m_pSprite );
	m_pSprite = NULL;
}

void CPlasma::ClearEffects()
{
	if (m_pSprite)
	{
		UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}
}
