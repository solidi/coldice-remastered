/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*	Use, distribution, and modification of this source code and/or resulting
*	object code is restricted to non-commercial enhancements to products from
*	Valve LLC.  All other use, distribution, or modification is prohibited
*	without written permission from Valve LLC.
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

LINK_ENTITY_TO_CLASS( flying_wrench, CFlyingWrench );

void CFlyingWrench::Spawn( )
{
	Precache( );

	// The flying wrench is MOVETYPE_TOSS, and SOLID_BBOX.
	// We want it to be affected by gravity, and hit objects
	// within the game.
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->classname = MAKE_STRING("flying_wrench");

	// Use the world wrench model.
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_WRENCH - 1;

	// Set the origin and size for the HL engine collision
	// tables.
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));

	// Store the owner for later use. We want the owner to be able
	// to hit themselves with the wrench. The pev->owner gets cleared
	// later to avoid hitting the player as they throw the wrench.
	if ( pev->owner )
		m_hOwner = Instance( pev->owner );

	// Set the think funtion.
	SetThink( &CFlyingWrench::BubbleThink );
	pev->nextthink = gpGlobals->time + 0.25;

	// Set the touch function.
	SetTouch( &CFlyingWrench::SpinTouch );
}

void CFlyingWrench::Precache( )
{
	PRECACHE_MODEL ("models/w_weapons.mdl");
	PRECACHE_SOUND ("wrench_hitbod1.wav");
	PRECACHE_SOUND ("wrench_hit1.wav");
	PRECACHE_SOUND ("wrench_miss1.wav");
}

void CFlyingWrench::SpinTouch( CBaseEntity *pOther )
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
		pOther->TraceAttack(pev, gSkillData.plrDmgFlyingWrench, pev->velocity.Normalize(), &tr,
								  DMG_NEVERGIB );
		if (m_hOwner != NULL)
			ApplyMultiDamage( pev, m_hOwner->pev );
		else
			ApplyMultiDamage( pev, pev );
	}

	// If we hit a player, make a nice squishy thunk sound. Otherwise
	// make a clang noise and throw a bunch of sparks.
	if (pOther->IsPlayer()) {
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "wrench_hitbod1.wav",
							1.0, ATTN_NORM, 0, 100);
	}
	else
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "wrench_hit1.wav",
							1.0, ATTN_NORM, 0, 100);
		if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
		{
			UTIL_Sparks( pev->origin );
			UTIL_Sparks( pev->origin );
			UTIL_Sparks( pev->origin );
		}
	}

	// Don't draw the flying wrench anymore.
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;

#ifndef CLIENT_DLL
	if (g_pGameRules->DeadPlayerWeapons(NULL) != GR_PLR_DROP_GUN_NO)
	{
		CBasePlayerWeapon *pItem = (CBasePlayerWeapon *)Create( "weapon_wrench",
											pev->origin , pev->angles, edict() );

		// Spawn a weapon box
		CWeaponBox *pWeaponBox = (CWeaponBox *)CBaseEntity::Create(
							"weaponbox", pev->origin, pev->angles, edict() );

		if (pWeaponBox != NULL)
		{
			// don't let weapon box tilt.
			pWeaponBox->pev->angles.x = 0;
			pWeaponBox->pev->angles.z = 0;

			// remove the weapon box after 2 mins.
			pWeaponBox->pev->nextthink = gpGlobals->time + 120;
			pWeaponBox->SetThink( &CWeaponBox::Kill );

			// Pack the wrench in the weapon box
			pWeaponBox->PackWeapon( pItem );

			// Get the unit vector in the direction of motion.
			Vector vecDir = pev->velocity.Normalize( );

			// Trace a line along the velocity vector to get the normal at impact.
			TraceResult tr;
			UTIL_TraceLine(pev->origin, pev->origin + vecDir * 100,
								dont_ignore_monsters, ENT(pev), &tr);

			DecalGunshot( &tr, BULLET_PLAYER_WRENCH );

			// Throw the weapon box along the normal so it looks kinda
			// like a ricochet. This would be better if I actually
			// calcualted the reflection angle, but I'm lazy. :)
			pWeaponBox->pev->velocity = tr.vecPlaneNormal * 300;
		}
	}
#endif

	// Remove this flying_wrench from the world.
	SetThink ( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + .1;
}

void CFlyingWrench::BubbleThink( void )
{
	// We have no owner. We do this .25 seconds AFTER the wrench
	// is thrown so that we don't hit the owner immediately when throwing
	// it. If is comes back later, we want to be able to hit the owner.
	// pev->owner = NULL;

	// Only think every .25 seconds.
	pev->nextthink = gpGlobals->time + 0.25;

	// Make a whooshy sound.
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "wrench_miss1.wav",
						1, ATTN_NORM, 0, 120);

	// If the wrench enters water, make some bubbles.
	if (pev->waterlevel)
		UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 1 );
}
