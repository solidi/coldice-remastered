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
#include "game.h"

LINK_ENTITY_TO_CLASS( snowbomb, CSnowbomb );

CSnowbomb *CSnowbomb::CreateSnowbomb( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner )
{
	CSnowbomb *pBomb = GetClassPtr( (CSnowbomb *)NULL );

	UTIL_SetOrigin( pBomb->pev, vecOrigin );
	pBomb->pev->angles = vecAngles;
	pBomb->Spawn();
	pBomb->SetTouch( &CSnowbomb::BombTouch );
	pBomb->m_hOwner = pOwner;
	if (pOwner)
		pBomb->pev->owner = pOwner->edict();

	return pBomb;
}

void CSnowbomb::Spawn( )
{
	Precache( );
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;
	pev->scale = 3.0;

	// Use the snowball model (same as flying_snowball)
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_SNOWBALL - 1;
	
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );
	
	// Add some spin to make it look like it's tumbling
	pev->avelocity.x = RANDOM_FLOAT ( -100, -500 );

	pev->classname = MAKE_STRING("snowbomb");

	// Set to explode after a short time
	SetThink( &CSnowbomb::BlowUp );
	pev->nextthink = gpGlobals->time + 1.5;
	
	// If velocity/angles are set, use them for initial trajectory
	if (pev->velocity.Length() < 1)
		pev->velocity = pev->angles * RANDOM_LONG(400, 600);

	// Lower gravity than normal - snowballs are light
	pev->gravity = 0.3;

	pev->dmg = gSkillData.plrDmgSnowball * 2; // Slightly more damage than regular snowball

	// Create a white smoke trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(PRECACHE_MODEL("sprites/smoke.spr"));	// model
		WRITE_BYTE( 15 ); // life
		WRITE_BYTE( 4 );  // width (larger than flying_snowball)
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 150 );	// brightness
	MESSAGE_END();
}

void CSnowbomb::BombTouch( CBaseEntity *pOther )
{
	// Bounce off surfaces, lose some velocity
	pev->velocity = pev->velocity * 0.6;
	
	// Play a bounce sound
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "snowball_miss.wav", 0.8, ATTN_NORM, 0, 95 + RANDOM_LONG(0,10));

	if ( pOther->IsAlive() && (pOther->pev->flags & (FL_CLIENT|FL_MONSTER)) )
	{
		SetThink( &CSnowbomb::BlowUp );
		pev->nextthink = gpGlobals->time;
	}
}

BOOL CSnowbomb::ShouldCollide( CBaseEntity *pOther )
{
	if (pev->modelindex == pOther->pev->modelindex)
		return FALSE;

	return TRUE;
}

void CSnowbomb::BlowUp( void )
{
	// Get the owner for the spawned snowballs
	entvars_t *pevOwner = NULL;
	if (m_hOwner)
		pevOwner = m_hOwner->pev;
	else if (pev->owner)
		pevOwner = VARS(pev->owner);

	// Create large white smoke explosion effect
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_SPRITE );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_SHORT( g_sModelIndexSnowballHit );
		WRITE_BYTE( 100 ); // scale * 10 (larger than regular snowball)
		WRITE_BYTE( 128 ); // framerate
	MESSAGE_END();

	// Spawn 6 flying snowballs in different directions
	/*for (int i = 0; i < 6; i++)
	{
		Vector vecVelocity = Vector(
			RANDOM_LONG(-100, 100), 
			RANDOM_LONG(-100, 100), 
			RANDOM_LONG(-50, 150)  // Bias upward slightly
		) * RANDOM_LONG(5, 12);
		
		// Create the flying snowball
		CFlyingSnowball *pSnowball = CFlyingSnowball::Shoot(
			pevOwner ? pevOwner : pev, 
			pev->origin, 
			vecVelocity, 
			NULL  // No sound on these spawned snowballs
		);
	}*/

	// RadiusDamage in a small area
	if (pevOwner)
		::RadiusDamage(pev->origin, pev, pevOwner, pev->dmg * 0.5, 160, CLASS_NONE, DMG_BLAST | DMG_NEVERGIB | DMG_FREEZE);
	else
		::RadiusDamage(pev->origin, pev, pev, pev->dmg * 0.5, 160, CLASS_NONE, DMG_BLAST | DMG_NEVERGIB | DMG_FREEZE);

	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "snowball_hitbod.wav", 1.0, ATTN_NORM, 0, 95 + RANDOM_LONG(0,10));

	// Remove the snowbomb
	pev->effects |= EF_NODRAW;
	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CSnowbomb::Precache( void )
{
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("sprites/smoke.spr");
	
	PRECACHE_SOUND("snowball_miss.wav");
	PRECACHE_SOUND("snowball_hitbod.wav");
	PRECACHE_SOUND("snowball_throw.wav");
}
