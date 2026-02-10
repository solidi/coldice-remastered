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
#include "gamerules.h"

LINK_ENTITY_TO_CLASS( flying_snowball, CFlyingSnowball );

CFlyingSnowball * CFlyingSnowball::Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, CBasePlayer *m_pPlayer )
{
	CFlyingSnowball *pSnowball = GetClassPtr( (CFlyingSnowball *)NULL );
	pSnowball->pev->owner = ENT(pevOwner);
	pSnowball->pev->angles = vecVelocity;
	pSnowball->Spawn();
	UTIL_SetOrigin( pSnowball->pev, vecStart );
	pSnowball->pev->velocity = vecVelocity;

	// Tumble through the air
	//pSnowball->pev->avelocity.x = -1000;
	pSnowball->pev->gravity = 0.1;
	pSnowball->pev->friction = 0;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(pSnowball->entindex());	// entity
		WRITE_SHORT(PRECACHE_MODEL("sprites/smoke.spr"));	// model
		WRITE_BYTE( 2 ); // life
		WRITE_BYTE( 2 );  // width
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 224 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 100 );	// brightness
	MESSAGE_END();

	SET_MODEL(ENT(pSnowball->pev), "models/w_weapons.mdl");
	pSnowball->pev->body = WEAPON_SNOWBALL - 1;

	if (m_pPlayer)
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "snowball_throw.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF));

	return pSnowball;
}

void CFlyingSnowball::Spawn( )
{
	Precache( );

	// We want it to be affected by gravity, and hit objects
	// within the game.
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->classname = MAKE_STRING("flying_snowball");

	// Use the world wrench model.
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_SNOWBALL - 1;

	// Set the origin and size for the HL engine collision
	// tables.
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));

	pev->angles.x -= 30;
	pev->angles.x = -(pev->angles.x + 30);

	// Store the owner for later use. We want the owner to be able
	// to hit themselves with the snowball. The pev->owner gets cleared
	// later to avoid hitting the player as they throw the snowball.
	if ( pev->owner )
		m_hOwner = Instance( pev->owner );

	// Set the think funtion.
	SetThink( &CFlyingSnowball::BubbleThink );
	pev->nextthink = gpGlobals->time + 0.25;

	// Set the touch function.
	SetTouch( &CFlyingSnowball::SpinTouch );
}

void CFlyingSnowball::Precache( )
{
	PRECACHE_MODEL ("models/w_weapons.mdl");

	PRECACHE_SOUND ("snowball_miss.wav");
	PRECACHE_SOUND ("snowball_hitbod.wav");
	PRECACHE_SOUND ("snowball_throw.wav");
}

void CFlyingSnowball::SpinTouch( CBaseEntity *pOther )
{
	// We touched something in the game. Look to see if the object
	// is allowed to take damage.
	if (pOther->pev->takedamage)
	{
		// Get the traceline info to the target.
		TraceResult tr = UTIL_GetGlobalTrace( );

		// Apply damage to the target. If we have an owner stored, use that one,
		// otherwise count it as self-inflicted.
		ClearMultiDamage( );
		pOther->TraceAttack(pev, gSkillData.plrDmgSnowball, pev->velocity.Normalize(), &tr,
								  DMG_NEVERGIB );
		if (m_hOwner != NULL)
			ApplyMultiDamage( pev, m_hOwner->pev );
		else
			ApplyMultiDamage( pev, pev );
	}

	// If we hit a player, make a nice squishy thunk sound. Otherwise
	// make a clang noise and throw a bunch of sparks.
	if (pOther->IsPlayer()) {
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "snowball_hitbod.wav",
							1.0, ATTN_NORM, 0, 100);
	}
	else
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "snowball_miss.wav",
							1.0, ATTN_NORM, 0, 100);
	}

	// Don't draw the flying snowball anymore.
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;

	// Get the unit vector in the direction of motion.
	Vector vecDir = pev->velocity.Normalize( );

	// Trace a line along the velocity vector to get the normal at impact.
	TraceResult tr;
	UTIL_TraceLine(pev->origin, pev->origin + vecDir * 100,
						dont_ignore_monsters, ENT(pev), &tr);

	if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
	{
		Vector pullOut = tr.vecEndPos + (tr.vecPlaneNormal * 10);
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_SPRITE );
			WRITE_COORD( pullOut.x );
			WRITE_COORD( pullOut.y );
			WRITE_COORD( pullOut.z );
			WRITE_SHORT( g_sModelIndexSnowballHit );
			WRITE_BYTE( 15 ); // scale * 10
			WRITE_BYTE( 128 ); // framerate
		MESSAGE_END();
	}

	DecalGunshot( &tr, BULLET_PLAYER_SNOWBALL );

	if (g_pGameRules->MutatorEnabled(MUTATOR_CHUMXPLODE))
	{
		edict_t *owner = NULL;
		if (m_hOwner)
			owner = m_hOwner->edict();
		CBaseEntity *pChumtoad = CBaseEntity::Create("monster_chumtoad", pev->origin, pev->angles, owner);
		if (pChumtoad)
		{
			pChumtoad->pev->velocity.x = RANDOM_FLOAT( -400, 400 );
			pChumtoad->pev->velocity.y = RANDOM_FLOAT( -400, 400 );
			pChumtoad->pev->velocity.z = RANDOM_FLOAT( 0, 400 );
		}
	}

	// Remove this snowball from the world.
	SetThink ( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + .1;
}

void CFlyingSnowball::BubbleThink( void )
{
	// Only think every .25 seconds.
	pev->nextthink = gpGlobals->time + 0.25;

	// If the snowball enters water, make some bubbles.
	if (pev->waterlevel)
		UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 1 );
}
