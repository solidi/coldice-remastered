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
#include "game.h"

LINK_ENTITY_TO_CLASS( plasma, CPlasma );

CPlasma *CPlasma::CreatePlasmaRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner )
{
	CPlasma *pPlasma = GetClassPtr( (CPlasma *)NULL );
	UTIL_SetOrigin( pPlasma->pev, vecOrigin );
	pPlasma->m_iPrimaryMode = TRUE;
	pPlasma->pev->owner = pOwner->edict();
	pPlasma->pev->classname = MAKE_STRING("plasma");
 	pPlasma->pev->angles = vecAngles;
	pPlasma->Spawn();
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
	pev->velocity = pev->angles * RANDOM_LONG(1800, 2200);

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
	pev->movetype = MOVETYPE_FLY;

#ifndef CLIENT_DLL
	// rocket trail
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		if (icesprites.value)
			WRITE_SHORT(m_iIceTrail );	// model
		else
			WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 15 ); // life
		WRITE_BYTE( 15 );  // width
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness

	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)
#endif

	m_flIgniteTime = gpGlobals->time + 0.6;
}

//=========================================================

void CPlasma :: Precache( void )
{
	PRECACHE_MODEL("models/plasma.mdl");
	PRECACHE_MODEL("sprites/ice_particles.spr");
	PRECACHE_MODEL("sprites/particles.spr");
	m_iIceExplode = PRECACHE_MODEL ("sprites/ice_plasma5.spr");
	m_iExplode = PRECACHE_MODEL ("sprites/plasma5.spr");
	m_iIceTrail = PRECACHE_MODEL("sprites/ice_plasmatrail.spr");
	m_iTrail = PRECACHE_MODEL("sprites/plasmatrail.spr");
	PRECACHE_SOUND ("plasma_hitwall.wav");
}

//=========================================================

void CPlasma :: Glow( void )
{
#ifndef CLIENT_DLL
	CSprite *plasmaSprite = NULL;
	if (icesprites.value)
		plasmaSprite = CSprite::SpriteCreate( "sprites/ice_particles.spr", pev->origin, FALSE );
	else
		plasmaSprite = CSprite::SpriteCreate("sprites/particles.spr", pev->origin, FALSE );

	if (plasmaSprite != NULL) {
		plasmaSprite->SetAttachment( edict(), 0 );
		plasmaSprite->pev->scale = 1;
		plasmaSprite->pev->frame = 8;
		plasmaSprite->pev->rendermode = kRenderTransAdd;
		plasmaSprite->pev->renderamt = 255;

		if (icesprites.value)
			plasmaSprite->SetTransparency( kRenderTransAdd, 0, 113, 230, 100, kRenderFxDistort );
		else
			plasmaSprite->SetTransparency( kRenderTransAdd, 0, 200, 0, 100, kRenderFxDistort );

		m_pSprite = plasmaSprite;
	}
#endif
}

//=========================================================

void CPlasma :: FlyThink( void  )
{
	if (pev->waterlevel == 3 || UTIL_PointContents(pev->origin) == CONTENT_WATER)
	{
		ClearEffects();
		UTIL_Remove(this);
		return;
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
		pOther->TakeDamage( pev, VARS(pev->owner), pev->dmg / 2,
			DMG_GENERIC | DMG_PARALYZE | DMG_ENERGYBEAM | DMG_FREEZE );
	}

	Explode();
}

void CPlasma::Explode( void )
{
	pev->model = iStringNull;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	SetTouch( NULL );
	SetThink( NULL );

	if (pev->waterlevel == 3 || UTIL_PointContents(pev->origin) == CONTENT_WATER)
	{
		ClearEffects();
		UTIL_Remove(this);
		return;
	}	

	EMIT_SOUND(ENT(pev), CHAN_ITEM, "plasma_hitwall.wav", 1, ATTN_NORM);

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector t = pev->origin - gpGlobals->v_forward * 20;

#ifndef CLIENT_DLL
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_SPRITE );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( t.x );	// Send to PAS because of the sound
		WRITE_COORD( t.y );
		WRITE_COORD( t.z );
		if (icesprites.value)
			WRITE_SHORT( m_iIceExplode );
		else
			WRITE_SHORT( m_iExplode );
		WRITE_BYTE( RANDOM_LONG(12, 18) ); // scale * 10
		WRITE_BYTE( 128 ); // framerate
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE(TE_DLIGHT);
		WRITE_COORD( pev->origin.x );	// X
		WRITE_COORD( pev->origin.y );	// Y
		WRITE_COORD( pev->origin.z );	// Z
		WRITE_BYTE( 15 );		// radius * 0.1
		if (icesprites.value) {
			WRITE_BYTE( 0 );		// r
			WRITE_BYTE( 113 );		// g
			WRITE_BYTE( 230 );		// b
		} else {
			WRITE_BYTE( 0 );		// r
			WRITE_BYTE( 200 );		// g
			WRITE_BYTE( 0 );		// b
		}
		WRITE_BYTE( 5 );		// time * 10
		WRITE_BYTE( 10 );		// decay * 0.1
	MESSAGE_END( );
#endif

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
#ifndef CLIENT_DLL
		int decal = DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2);
		if (g_pGameRules->MutatorEnabled(MUTATOR_PAINTBALL)) {
			decal = DECAL_PAINTL1 + RANDOM_LONG(0, 7);
		}
		UTIL_DecalTrace(&tr, decal);
#endif
	}

	pev->velocity = g_vecZero;
	ClearEffects();
	UTIL_Remove(this);
}

void CPlasma::ClearEffects()
{
#ifndef CLIENT_DLL
	if (m_pSprite)
	{
		UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}
#endif
}
