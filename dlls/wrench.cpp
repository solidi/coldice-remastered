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

class CFlyingWrench : public CBaseEntity
{
public:

	void Spawn( void );
	void Precache( void );
	void EXPORT BubbleThink( void );
	void EXPORT SpinTouch( CBaseEntity *pOther );
	CBasePlayer *m_pPlayer;

private:

	EHANDLE m_hOwner;		  // Original owner is stored here so we can
									 // allow the wrench to hit the user.
};

LINK_ENTITY_TO_CLASS( flying_wrench, CFlyingWrench );


#define	WRENCH_BODYHIT_VOLUME 128
#define	WRENCH_WALLHIT_VOLUME 512

#ifdef WRENCH
LINK_ENTITY_TO_CLASS( weapon_wrench, CWrench );
#endif

enum wrench_e {
	WRENCH_IDLE = 0,
	WRENCH_DRAW,
	WRENCH_HOLSTER,
	WRENCH_THROW,
	WRENCH_ATTACK1HIT,
	WRENCH_ATTACK1MISS,
	WRENCH_ATTACK2MISS,
	WRENCH_ATTACK2HIT,
	WRENCH_ATTACK3MISS,
	WRENCH_ATTACK3HIT,
	WRENCH_IDLE2,
	WRENCH_IDLE3
};

void CWrench::Spawn( )
{
	Precache( );
	m_iId = WEAPON_WRENCH;
	SET_MODEL(ENT(pev), "models/w_wrench.mdl");
	m_iClip = -1;

	FallInit();// get ready to fall down.
}

void CWrench::Precache( void )
{
	PRECACHE_MODEL("models/v_wrench.mdl");
	PRECACHE_MODEL("models/w_wrench.mdl");
	PRECACHE_MODEL("models/p_wrench.mdl");
	PRECACHE_SOUND("wrench_hit1.wav");
	PRECACHE_SOUND("wrench_hit2.wav");
	PRECACHE_SOUND("wrench_hitbod1.wav");
	PRECACHE_SOUND("wrench_hitbod2.wav");
	PRECACHE_SOUND("wrench_hitbod3.wav");
	PRECACHE_SOUND("wrench_miss1.wav");

	m_usWrench = PRECACHE_EVENT ( 1, "events/wrench.sc" );
}

int CWrench::AddToPlayer( CBasePlayer *pPlayer )
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

int CWrench::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 3;
	p->iId = WEAPON_WRENCH;
	p->iWeight = WRENCH_WEIGHT;
	p->pszDisplayName = "40 Pound Monkey Wrench";
	return 1;
}

BOOL CWrench::Deploy( )
{
	return DefaultDeploy( "models/v_wrench.mdl", "models/p_wrench.mdl", WRENCH_DRAW, "crowbar" );
}

void CWrench::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( WRENCH_HOLSTER );
}

void CWrench::FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
{
	int			i, j, k;
	float		distance;
	float		*minmaxs[2] = {mins, maxs};
	TraceResult tmpTrace;
	Vector		vecHullEnd = tr.vecEndPos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if ( tmpTrace.flFraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
				if ( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}

void CWrench::PrimaryAttack()
{
	if (! Swing( 1 ))
	{
		SetThink( &CWrench::SwingAgain );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CWrench::SecondaryAttack()
{
	SendWeaponAnim( WRENCH_THROW );
	SetThink( &CWrench::Throw );

	m_flNextPrimaryAttack = GetNextAttackDelay(0.75);
	// Just for kicks, set this.
	// But we destroy this weapon anyway so... thppt.
	m_flNextSecondaryAttack = gpGlobals->time + 0.75;
	pev->nextthink = gpGlobals->time + 0.15;
}

void CWrench::Throw() {
	// Don't throw underwater, and only throw if we were able to detatch
	// from player.
	if ( (m_pPlayer->pev->waterlevel != 3) &&
			(m_pPlayer->RemovePlayerItem( this )) )
	{
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

		// Create a flying wrench.
		CFlyingWrench *pFCBar = (CFlyingWrench *)Create( "flying_wrench",
					vecSrc, Vector(0,0,0), m_pPlayer->edict() );

		// Give the wrench its velocity, angle, and spin.
		// Lower the gravity a bit, so it flys.
		pFCBar->pev->velocity = vecDir * 1000 + m_pPlayer->pev->velocity;
		pFCBar->pev->angles = vecAng;
		pFCBar->pev->avelocity.x = -1000;
		pFCBar->pev->gravity = .25;
		pFCBar->m_pPlayer = m_pPlayer;

		// Do player weapon anim and sound effect.
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON,
			"wrench_miss1.wav", 1, ATTN_NORM, 0,
			94 + RANDOM_LONG(0,0xF));

		// take item off hud
		m_pPlayer->pev->weapons &= ~(1<<this->m_iId);

		// Destroy this weapon
		DestroyItem();

		// RetireWeapon();
	}
}

void CWrench::Smack( )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_WRENCH );
}

void CWrench::SwingAgain( void )
{
	Swing( 0 );
}

int CWrench::Swing( int fFirst )
{
	int fDidHit = FALSE;

	TraceResult tr;

	UTIL_MakeVectors (m_pPlayer->pev->v_angle);
	Vector vecSrc	= m_pPlayer->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}
#endif

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usWrench, 
	0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0,
	0.0, 0, 0.0 );


	if ( tr.flFraction >= 1.0 )
	{
		if (fFirst)
		{
			// miss
			m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
			
			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		switch( ((m_iSwing++) % 2) + 1 )
		{
		case 0:
			SendWeaponAnim( WRENCH_ATTACK1HIT ); break;
		case 1:
			SendWeaponAnim( WRENCH_ATTACK2HIT ); break;
		case 2:
			SendWeaponAnim( WRENCH_ATTACK3HIT ); break;
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		
#ifndef CLIENT_DLL

		// hit
		fDidHit = TRUE;
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		ClearMultiDamage( );

		if ( (m_flNextPrimaryAttack + 1 < UTIL_WeaponTimeBase() ) || g_pGameRules->IsMultiplayer() )
		{
			// first swing does full damage
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgWrench, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		}
		else
		{
			// subsequent swings do half
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgWrench / 2, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		}	
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// play thwack or smack sound
				switch( RANDOM_LONG(0,2) )
				{
				case 0:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "wrench_hitbod1.wav", 1, ATTN_NORM); break;
				case 1:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "wrench_hitbod2.wav", 1, ATTN_NORM); break;
				case 2:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, "wrench_hitbod3.wav", 1, ATTN_NORM); break;
				}
				m_pPlayer->m_iWeaponVolume = WRENCH_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
					  return TRUE;
				else
					  flVol = 0.1;

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_WRENCH);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play wrench strike
			switch( RANDOM_LONG(0,1) )
			{
			case 0:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "wrench_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "wrench_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
				break;
			}

			// delay the decal a bit
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * WRENCH_WALLHIT_VOLUME;
#endif
		m_flNextPrimaryAttack = GetNextAttackDelay(0.25);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		
		SetThink( &CWrench::Smack );
		pev->nextthink = UTIL_WeaponTimeBase() + 0.2;

		
	}
	return fDidHit;
}

void CWrench::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	float flAnimTime = 5.34; // Only WRENCH_IDLE has a different time
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		iAnim = WRENCH_IDLE;
		flAnimTime = 2.7;
		break;
	case 1:
		iAnim = WRENCH_IDLE2;
		break;
	case 2:
		iAnim = WRENCH_IDLE3;
		break;
	}
	SendWeaponAnim( iAnim, 1 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flAnimTime;
}

// =========================================

// =========================================

void CFlyingWrench::Spawn( )
{
	Precache( );

	// The flying wrench is MOVETYPE_TOSS, and SOLID_BBOX.
	// We want it to be affected by gravity, and hit objects
	// within the game.
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;

	// Use the world wrench model.
	SET_MODEL(ENT(pev), "models/w_wrench.mdl");

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
	PRECACHE_MODEL ("models/w_wrench.mdl");
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

	// Spawn a wrench weapon
	CBasePlayerWeapon *pItem = (CBasePlayerWeapon *)Create( "weapon_wrench",
										 pev->origin , pev->angles, edict() );

	// Spawn a weapon box
	CWeaponBox *pWeaponBox = (CWeaponBox *)CBaseEntity::Create(
						"weaponbox", pev->origin, pev->angles, edict() );

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

	// Remove this flying_wrench from the world.
	SetThink ( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + .1;
}

void CFlyingWrench::BubbleThink( void )
{
	// We have no owner. We do this .25 seconds AFTER the wrench
	// is thrown so that we don't hit the owner immediately when throwing
	// it. If is comes back later, we want to be able to hit the owner.
	pev->owner = NULL;

	// Only think every .25 seconds.
	pev->nextthink = gpGlobals->time + 0.25;

	// Make a whooshy sound.
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "wrench_miss1.wav",
						1, ATTN_NORM, 0, 120);

	// If the wrench enters water, make some bubbles.
	if (pev->waterlevel)
		UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 1 );
}
