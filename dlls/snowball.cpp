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

class CFlyingSnowball : public CBaseEntity
{
public:

	void Spawn( void );
	void Precache( void );
	void EXPORT BubbleThink( void );
	void EXPORT SpinTouch( CBaseEntity *pOther );

	static CFlyingSnowball *Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, CBasePlayer *m_pPlayer );

private:

	EHANDLE m_hOwner;		 // Original owner is stored here so we can
							// allow the wrench to hit the user.
};

LINK_ENTITY_TO_CLASS( flying_snowball, CFlyingSnowball );

enum snowball_e {
	SNOWBALL_IDLE = 0,
	SNOWBALL_FIDGET,
	SNOWBALL_PINPULL,
	SNOWBALL_THROW1,	// toss
	SNOWBALL_THROW2,	// medium
	SNOWBALL_THROW3,	// hard
	SNOWBALL_HOLSTER,
	SNOWBALL_DRAW,
	SNOWBALL_DEPLOY,	// with select sound, bro.
};

#ifdef SNOWBALL
LINK_ENTITY_TO_CLASS( weapon_snowball, CSnowball );
#endif

void CSnowball::Spawn( )
{
	Precache( );
	m_iId = WEAPON_SNOWBALL;
	SET_MODEL(ENT(pev), "models/w_snowball.mdl");

#ifndef CLIENT_DLL
	pev->dmg = gSkillData.plrDmgSnowball;
#endif

	m_iDefaultAmmo = SNOWBALL_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}

void CSnowball::Precache( void )
{
	PRECACHE_MODEL("models/w_snowball.mdl");
	PRECACHE_MODEL("models/v_snowball.mdl");
	PRECACHE_MODEL("models/p_snowball.mdl");

	UTIL_PrecacheOther( "flying_snowball" );
}

int CSnowball::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Hand Grenade";
	p->iMaxAmmo1 = SNOWBALL_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_SNOWBALL;
	p->iWeight = SNOWBALL_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	p->pszDisplayName = "Snowball";

	return 1;
}

BOOL CSnowball::Deploy( )
{
	m_flReleaseThrow = -1;
	return DefaultDeploy( "models/v_snowball.mdl", "models/p_snowball.mdl", SNOWBALL_DEPLOY, "crowbar" );
}

BOOL CSnowball::CanHolster( void )
{
	// can only holster hand grenades when not primed!
	return ( m_flStartThrow == 0 );
}

void CSnowball::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
	{
		SendWeaponAnim( SNOWBALL_HOLSTER );
	}
	else
	{
		// no more grenades!
		m_pPlayer->pev->weapons &= ~(1<<WEAPON_SNOWBALL);
		SetThink( &CSnowball::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
	}

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

void CSnowball::PrimaryAttack()
{
	if ( !m_flStartThrow && m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] > 0 )
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;
		m_fireState = 1;

		SendWeaponAnim( SNOWBALL_PINPULL );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
	}
}

void CSnowball::Throw() {
	// Don't throw underwater, and only throw if we were able to detatch
	// from player.
	if ( (m_pPlayer->pev->waterlevel != 3) )
	{
		SendWeaponAnim( SNOWBALL_THROW1 );

		// Important! Capture globals before it is stomped on.
		Vector anglesAim = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;
		UTIL_MakeVectors( anglesAim );

		// Get the origin, direction, and fix the angle of the throw.
		Vector vecSrc = m_pPlayer->GetGunPosition( )
					+ gpGlobals->v_right * 8
					+ gpGlobals->v_forward * 16;

		Vector vecDir = gpGlobals->v_forward;
		Vector vecAng = UTIL_VecToAngles (vecDir);
		vecAng.z = vecDir.z - 90;

		// Create a flying snowball.
		CFlyingSnowball *pSnowball = (CFlyingSnowball *)Create( "flying_snowball",
					vecSrc, Vector(0,0,0), m_pPlayer->edict() );

		// Give the wrench its velocity, angle, and spin.
		// Lower the gravity a bit, so it flys.
		pSnowball->pev->velocity = vecDir * RANDOM_LONG(500,1000) + m_pPlayer->pev->velocity;
		pSnowball->pev->angles = vecAng;
		pSnowball->pev->avelocity.x = -1000;
		pSnowball->pev->gravity = .25;

		// Do player weapon anim and sound effect.
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON,
			"snowball_throw.wav", 1, ATTN_NORM, 0,
			94 + RANDOM_LONG(0,0xF));
		
		m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ]--;
	}
}

void CSnowball::SecondaryAttack()
{
	if (m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] > 0 ) {
		SendWeaponAnim( SNOWBALL_PINPULL );
		SetThink( &CSnowball::Throw );

		m_flNextPrimaryAttack = GetNextAttackDelay(0.75);
		// Just for kicks, set this.
		// But we destroy this weapon anyway so... thppt.
		m_flNextSecondaryAttack = GetNextAttackDelay(0.75);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
		pev->nextthink = gpGlobals->time + 0.5;
	} else {
		RetireWeapon();
	}
}

void CSnowball::WeaponIdle( void )
{
	if ( m_flReleaseThrow == 0 && m_flStartThrow )
		 m_flReleaseThrow = gpGlobals->time;

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flStartThrow )
	{
		Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

		if ( angThrow.x < 0 )
			angThrow.x = -10 + angThrow.x * ( ( 90 - 10 ) / 90.0 );
		else
			angThrow.x = -10 + angThrow.x * ( ( 90 + 10 ) / 90.0 );

		float flVel = ( 90 - angThrow.x ) * 4;
		if ( flVel > 500 )
			flVel = 500;

		UTIL_MakeVectors( angThrow );

		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;

		Vector vecThrow = gpGlobals->v_forward * flVel + m_pPlayer->pev->velocity;

		// alway explode 3 seconds after the pin was pulled
		float time = m_flStartThrow - gpGlobals->time + 3.0;
		if (time < 0)
			time = 0;

#ifndef CLIENT_DLL
		CFlyingSnowball::Shoot( m_pPlayer->pev, vecSrc, vecThrow, m_pPlayer );
#endif

		if ( flVel < 500 )
		{
			SendWeaponAnim( SNOWBALL_THROW1 );
		}
		else if ( flVel < 1000 )
		{
			SendWeaponAnim( SNOWBALL_THROW2 );
		}
		else
		{
			SendWeaponAnim( SNOWBALL_THROW3 );
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		m_flReleaseThrow = 0;
		m_flStartThrow = 0;
		m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		m_flNextSecondaryAttack = GetNextAttackDelay(0.5);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;

		m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ]--;

		if ( !m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
		{
			// just threw last grenade
			// set attack times in the future, and weapon idle in the future so we can see the whole throw
			// animation, weapon idle will automatically retire the weapon for us.
			m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.5);// ensure that the animation can finish playing
		}
		return;
	}
	else if ( m_flReleaseThrow > 0 )
	{
		// we've finished the throw, restart.
		m_flStartThrow = 0;

		if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
		{
			SendWeaponAnim( SNOWBALL_DRAW );
		}
		else
		{
			RetireWeapon();
			return;
		}

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		m_flReleaseThrow = -1;
		return;
	}

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
		if (flRand <= 0.75)
		{
			iAnim = SNOWBALL_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );// how long till we do this again.
		}
		else 
		{
			iAnim = SNOWBALL_FIDGET;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 75.0 / 30.0;
		}

		SendWeaponAnim( iAnim );
	}
}

// =========================================

// =========================================

CFlyingSnowball * CFlyingSnowball::Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, CBasePlayer *m_pPlayer )
{
	CFlyingSnowball *pSnowball = GetClassPtr( (CFlyingSnowball *)NULL );
	pSnowball->pev->owner = ENT(pevOwner);
	pSnowball->Spawn();
	UTIL_SetOrigin( pSnowball->pev, vecStart );
	pSnowball->pev->velocity = vecVelocity;
	pSnowball->pev->angles = UTIL_VecToAngles(pSnowball->pev->velocity);

	// Tumble through the air
	pSnowball->pev->avelocity.x = -400;

	pSnowball->pev->gravity = 0.5;
	pSnowball->pev->friction = 0.8;

	SET_MODEL(ENT(pSnowball->pev), "models/w_snowball.mdl");
	EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "snowball_throw.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF));

	return pSnowball;
}

void CFlyingSnowball::Spawn( )
{
	Precache( );

	// The flying wrench is MOVETYPE_TOSS, and SOLID_BBOX.
	// We want it to be affected by gravity, and hit objects
	// within the game.
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->classname = MAKE_STRING("flying_snowball");

	pev->gravity = .25;

	// Use the world wrench model.
	SET_MODEL(ENT(pev), "models/w_snowball.mdl");

	// Set the origin and size for the HL engine collision
	// tables.
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));

	pev->angles.x -= 30;
	UTIL_MakeVectors( pev->angles );
	pev->angles.x = -(pev->angles.x + 30);

	pev->velocity = gpGlobals->v_forward * 250;
	pev->gravity = 0.5;

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
	PRECACHE_MODEL ("models/w_snowball.mdl");

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
		if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
		{
			Vector forward = gpGlobals->v_forward;
			Vector pullOut = pev->origin - forward * 20;
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

	DecalGunshot( &tr, BULLET_PLAYER_SNOWBALL );

	// Remove this snowball from the world.
	SetThink ( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + .1;
}

void CFlyingSnowball::BubbleThink( void )
{
	// We have no owner. We do this .25 seconds AFTER the snowball
	// is thrown so that we don't hit the owner immediately when throwing
	// it. If is comes back later, we want to be able to hit the owner.
	// pev->owner = NULL;

	// Only think every .25 seconds.
	pev->nextthink = gpGlobals->time + 0.25;

	// Make a whooshy sound.
	//EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "wrench_miss1.wav",
	//					1, ATTN_NORM, 0, 120);

	// If the snowball enters water, make some bubbles.
	if (pev->waterlevel)
		UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 1 );
}
