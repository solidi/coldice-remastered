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

class CFlyingCrowbar : public CBaseEntity
{
public:

	void Spawn( void );
	void Precache( void );
	void EXPORT BubbleThink( void );
	void EXPORT SpinTouch( CBaseEntity *pOther );

	virtual int ObjectCaps( void ) { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_PORTAL; }

private:

   EHANDLE m_hOwner;        // Original owner is stored here so we can
                            // allow the crowbar to hit the user.
};

LINK_ENTITY_TO_CLASS( flying_crowbar, CFlyingCrowbar );


#define	CROWBAR_BODYHIT_VOLUME 128
#define	CROWBAR_WALLHIT_VOLUME 512

#ifdef CROWBAR
LINK_ENTITY_TO_CLASS( weapon_crowbar, CCrowbar );
#endif



enum crowbar_e {
	CROWBAR_IDLE = 0,
	CROWBAR_DRAW_LOWKEY,
	CROWBAR_DRAW,
	CROWBAR_HOLSTER,
	CROWBAR_ATTACK1HIT,
	CROWBAR_ATTACK1MISS,
	CROWBAR_ATTACK2MISS,
	CROWBAR_ATTACK2HIT,
	CROWBAR_ATTACK3MISS,
	CROWBAR_ATTACK3HIT,
	CROWBAR_IDLE2,
	CROWBAR_IDLE3,
	CROWBAR_PULL_BACK,
	CROWBAR_THROW2,
};


void CCrowbar::Spawn( )
{
	Precache( );
	m_iId = WEAPON_CROWBAR;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_CROWBAR - 1;

	m_iClip = -1;

	FallInit();// get ready to fall down.
}


void CCrowbar::Precache( void )
{
	PRECACHE_MODEL("models/v_crowbar.mdl");
	PRECACHE_SOUND("cbar_hit1.wav");
	PRECACHE_SOUND("weapons/cbar_hit2.wav");
	PRECACHE_SOUND("cbar_hitbod1.wav");
	PRECACHE_SOUND("cbar_hitbod2.wav");
	PRECACHE_SOUND("cbar_hitbod3.wav");
	PRECACHE_SOUND("weapons/cbar_miss1.wav");

	m_usCrowbar = PRECACHE_EVENT ( 1, "events/crowbar.sc" );
}


int CCrowbar::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}


int CCrowbar::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 3;
	p->iId = WEAPON_CROWBAR;
	p->iWeight = CROWBAR_WEIGHT;
	p->pszDisplayName = "Standard Crowbar";
	return 1;
}

BOOL CCrowbar::CanDeploy( void )
{
	return m_flReleaseThrow < 1;
}

BOOL CCrowbar::DeployLowKey( )
{
	m_flStartThrow = 0;
	m_flReleaseThrow = -1;
	return DefaultDeploy( "models/v_crowbar.mdl", "models/p_weapons.mdl", CROWBAR_DRAW_LOWKEY, "crowbar" );
}

BOOL CCrowbar::Deploy( )
{
	m_flStartThrow = 0;
	m_flReleaseThrow = -1;
	return DefaultDeploy( "models/v_crowbar.mdl", "models/p_weapons.mdl", CROWBAR_DRAW, "crowbar" );
}

BOOL CCrowbar::CanSlide()
{
	return m_flReleaseThrow < 1;
}

void CCrowbar::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(-1);

	if (m_flReleaseThrow > 0) {
		m_pPlayer->pev->weapons &= ~(1<<WEAPON_CROWBAR);
		SetThink( &CCrowbar::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
	} else {
		SendWeaponAnim( CROWBAR_HOLSTER );
	}
}


void CCrowbar::PrimaryAttack()
{
	if (!m_flStartThrow && !Swing( 1 ))
	{
		SetThink( &CCrowbar::SwingAgain );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}


void CCrowbar::SecondaryAttack()
{
#ifdef CLIENT_DLL
	if (IsGunGame())
#else
	if (g_pGameRules->IsGunGame())
#endif
	{
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
		return PrimaryAttack();
	}

	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if ( !m_flStartThrow )
	{
		SendWeaponAnim( CROWBAR_PULL_BACK );
		m_pPlayer->pev->punchangle = Vector(-2, -2, 0);
		m_flStartThrow = 1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
	}

	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
}

void CCrowbar::Throw()
{
	// Don't throw underwater, and only throw if we were able to detatch
	// from player.
	if ( (m_pPlayer->pev->waterlevel != 3) )
	{
		// Important! Capture globals before it is stomped on.
		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

		// Get the origin, direction, and fix the angle of the throw.
		Vector vecSrc = m_pPlayer->GetGunPosition( )
					+ gpGlobals->v_right * 8
					+ vecAiming * 16;

		Vector vecDir = vecAiming;
		Vector vecAng = UTIL_VecToAngles(vecDir);
		vecAng.z = vecDir.z - 90;

		// Create a flying crowbar.
		CFlyingCrowbar *pCrowbar = (CFlyingCrowbar *)Create( "flying_crowbar",
					vecSrc, vecAiming, m_pPlayer->edict() );

		// Give the crowbar its velocity, angle, and spin.
		// Lower the gravity a bit, so it flys.
		if (pCrowbar)
		{
			pCrowbar->pev->velocity = vecDir * 1000 + m_pPlayer->pev->velocity;
			pCrowbar->pev->angles = vecAng;
			pCrowbar->pev->avelocity.x = -1000;
			pCrowbar->pev->gravity = .25;
		}

		// Do player weapon anim and sound effect.
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		m_pPlayer->pev->punchangle = Vector(-4, -4, -2);

		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON,
			"weapons/cbar_miss1.wav", 1, ATTN_NORM, 0,
			94 + RANDOM_LONG(0,0xF));
	}
}


void CCrowbar::Smack( )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}


void CCrowbar::SwingAgain( void )
{
	Swing( 0 );
}


int CCrowbar::Swing( int fFirst )
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
				UTIL_FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}
#endif

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usCrowbar, 
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
			SendWeaponAnim( CROWBAR_ATTACK1HIT ); break;
		case 1:
			SendWeaponAnim( CROWBAR_ATTACK2HIT ); break;
		case 2:
			SendWeaponAnim( CROWBAR_ATTACK3HIT ); break;
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
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgCrowbar, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		}
		else
		{
			// subsequent swings do half
			pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgCrowbar / 2, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		}	
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				pEntity->pev->velocity = (pEntity->pev->velocity + (gpGlobals->v_forward * RANDOM_LONG(100,200)));

				// play thwack or smack sound
				switch( RANDOM_LONG(0,2) )
				{
				case 0:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_BODY, "cbar_hitbod1.wav", 1, ATTN_NORM); break;
				case 1:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_BODY, "cbar_hitbod2.wav", 1, ATTN_NORM); break;
				case 2:
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_BODY, "cbar_hitbod3.wav", 1, ATTN_NORM); break;
				}
				m_pPlayer->m_iWeaponVolume = CROWBAR_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
				{
					  m_flNextPrimaryAttack = GetNextAttackDelay(0.25);
					  return TRUE;
				} else
					  flVol = 0.1;

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_CROWBAR);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play crowbar strike
			switch( RANDOM_LONG(0,1) )
			{
			case 0:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_BODY, "cbar_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
				break;
			case 1:
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_BODY, "weapons/cbar_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
				break;
			}

			// delay the decal a bit
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * CROWBAR_WALLHIT_VOLUME;
#endif
		m_flNextPrimaryAttack = GetNextAttackDelay(0.25);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		
		SetThink( &CCrowbar::Smack );
		pev->nextthink = UTIL_WeaponTimeBase() + 0.2;

		
	}
	return fDidHit;
}

void CCrowbar::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;
	
	if ( m_flStartThrow == 1 )
	{
		SendWeaponAnim( CROWBAR_THROW2 );
#ifndef CLIENT_DLL
		Throw();
#endif
		m_flStartThrow = 2;
		m_flReleaseThrow = 1;
		m_flTimeWeaponIdle = GetNextAttackDelay(0.75);// ensure that the animation can finish playing
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.75);
		return;
	}
	else if ( m_flReleaseThrow > 0 )
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
#ifndef CLIENT_DLL
		if (infiniteammo.value) {
			m_flStartThrow = 0;
			m_flReleaseThrow = -1;
			SendWeaponAnim( CROWBAR_DRAW );
		} else
			RetireWeapon();
#endif
		return;
	}

	int iAnim;
	float flAnimTime = 5.34; // Only CROWBAR_IDLE has a different time
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		iAnim = CROWBAR_IDLE;
		flAnimTime = 2.7;
		break;
	case 1:
		iAnim = CROWBAR_IDLE2;
		break;
	case 2:
		iAnim = CROWBAR_IDLE3;
		break;
	}
	SendWeaponAnim( iAnim, 1 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flAnimTime;
}

// =========================================

// =========================================

void CFlyingCrowbar::Spawn( )
{
	Precache( );

	// The flying crowbar is MOVETYPE_TOSS, and SOLID_BBOX.
	// We want it to be affected by gravity, and hit objects
	// within the game.
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
	pev->classname = MAKE_STRING("flying_crowbar");

	// Use the world crowbar model.
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_CROWBAR - 1;

	// Set the origin and size for the HL engine collision
	// tables.
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize(pev, Vector(-4, -4, -4), Vector(4, 4, 4));

	// Store the owner for later use. We want the owner to be able
	// to hit themselves with the crowbar. The pev->owner gets cleared
	// later to avoid hitting the player as they throw the crowbar.
	if ( pev->owner )
		m_hOwner = Instance( pev->owner );

	// Set the think funtion.
	SetThink( &CFlyingCrowbar::BubbleThink );
	pev->nextthink = gpGlobals->time + 0.25;

	// Set the touch function.
	SetTouch( &CFlyingCrowbar::SpinTouch );
}

void CFlyingCrowbar::Precache( )
{
   PRECACHE_MODEL ("models/w_weapons.mdl");
   PRECACHE_SOUND ("cbar_hitbod1.wav");
   PRECACHE_SOUND ("cbar_hit1.wav");
   PRECACHE_SOUND ("weapons/cbar_miss1.wav");
}

void CFlyingCrowbar::SpinTouch( CBaseEntity *pOther )
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
		pOther->TraceAttack(pev, gSkillData.plrDmgFlyingCrowbar, pev->velocity.Normalize(), &tr,
							DMG_NEVERGIB );
		if (m_hOwner != NULL)
			ApplyMultiDamage( pev, m_hOwner->pev );
		else
			ApplyMultiDamage( pev, pev );
	}

	// If we hit a player, make a nice squishy thunk sound. Otherwise
	// make a clang noise and throw a bunch of sparks.
	if (pOther->IsPlayer()) {
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "cbar_hitbod1.wav",
						1.0, ATTN_NORM, 0, 100);
	}
	else
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "cbar_hit1.wav",
						1.0, ATTN_NORM, 0, 100);
		if (UTIL_PointContents(pev->origin) != CONTENTS_WATER)
		{
			UTIL_Sparks( pev->origin );
			UTIL_Sparks( pev->origin );
			UTIL_Sparks( pev->origin );
		}
	}

	// Don't draw the flying crowbar anymore.
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;

	#ifndef CLIENT_DLL
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pev->owner);
	if (pPlayer && g_pGameRules->DeadPlayerWeapons(pPlayer) != GR_PLR_DROP_GUN_NO)
	{
		CBasePlayerWeapon *pItem = (CBasePlayerWeapon *)Create( "weapon_crowbar",
								pev->origin , pev->angles, edict() );

		if (pItem != NULL)
		{
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

				// Pack the crowbar in the weapon box
				pWeaponBox->PackWeapon( pItem );

				// Get the unit vector in the direction of motion.
				Vector vecDir = pev->velocity.Normalize( );

				// Trace a line along the velocity vector to get the normal at impact.
				TraceResult tr;
				UTIL_TraceLine(pev->origin, pev->origin + vecDir * 100,
							dont_ignore_monsters, ENT(pev), &tr);

				DecalGunshot( &tr, BULLET_PLAYER_CROWBAR );

				// Throw the weapon box along the normal so it looks kinda
				// like a ricochet. This would be better if I actually
				// calcualted the reflection angle, but I'm lazy. :)
				pWeaponBox->pev->velocity = tr.vecPlaneNormal * 300;
			}
		}
	}
	#endif

	// Remove this flying_crowbar from the world.
	SetThink ( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CFlyingCrowbar::BubbleThink( void )
{
	// We have no owner. We do this .25 seconds AFTER the crowbar
	// is thrown so that we don't hit the owner immediately when throwing
	// it. If is comes back later, we want to be able to hit the owner.
	// pev->owner = NULL;

	// Only think every .25 seconds.
	pev->nextthink = gpGlobals->time + 0.25;

	// Make a whooshy sound.
	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "weapons/cbar_miss1.wav",
					1, ATTN_NORM, 0, 120);

	// If the crowbar enters water, make some bubbles.
	if (pev->waterlevel)
		UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 1 );
}
