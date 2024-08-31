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

#define	CROWBAR_BODYHIT_VOLUME 128
#define	CROWBAR_WALLHIT_VOLUME 512

#ifdef ROCKETCROWBAR
LINK_ENTITY_TO_CLASS( weapon_rocketcrowbar, CRocketCrowbar );
#endif

#ifndef CLIENT_DLL

LINK_ENTITY_TO_CLASS( drunk_rocket, CDrunkRocket );

//=========================================================
//=========================================================
CBaseEntity *CDrunkRocket::CreateDrunkRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner )
{
	int speed = pOwner->pev->velocity.Length2D();
	CDrunkRocket *pRocket = GetClassPtr( (CDrunkRocket *)NULL );
	UTIL_SetOrigin( pRocket->pev, vecOrigin );
	pRocket->pev->angles = UTIL_VecToAngles(vecAngles);
	pRocket->pev->velocity = vecAngles * RANDOM_LONG(speed + 200, speed + 250);
	pRocket->Spawn();
	pRocket->SetTouch( &CRpgRocket::RocketTouch );
	pRocket->pev->owner = pOwner->edict();

	return pRocket;
}

//=========================================================
//=========================================================
void CDrunkRocket::Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );

	// Hack
	pev->classname = MAKE_STRING("rocketcrowbar");

	SetThink( &CDrunkRocket::IgniteThink );
	SetTouch( &CDrunkRocket::ExplodeTouch );

	pev->gravity = 0.5;

	pev->nextthink = gpGlobals->time + 0.1;
	pev->dmg = gSkillData.plrDmgRPG;
}

void CDrunkRocket::RocketTouch ( CBaseEntity *pOther )
{
	if (pOther->edict() == pev->owner)
		return;

	STOP_SOUND( edict(), CHAN_VOICE, "rocket1.wav" );
	ExplodeTouch( pOther );
}

void CDrunkRocket::Precache( void )
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND ("rocket1.wav");
}

void CDrunkRocket::IgniteThink( void  )
{
	pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "rocket1.wav", 1, 0.5 );

	// rocket trail
	int rand = RANDOM_LONG(0,2);
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 40 ); // life
		WRITE_BYTE( 5 );  // width
		if (rand == 2) {
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 0 );   // r, g, b
			WRITE_BYTE( 0 );   // r, g, b
		} else if (rand == 1) {
			WRITE_BYTE( 0 );   // r, g, b
			WRITE_BYTE( 255 );   // r, g, b
			WRITE_BYTE( 0 );   // r, g, b
		} else {
			WRITE_BYTE( 0 );   // r, g, b
			WRITE_BYTE( 0 );   // r, g, b
			WRITE_BYTE( 255 );   // r, g, b
		}
		WRITE_BYTE( 255 );	// brightness

	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	m_flIgniteTime = gpGlobals->time;

	// set to follow laser spot
	SetThink( &CDrunkRocket::FollowThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CDrunkRocket::FollowThink( void  )
{
	CBaseEntity *pOther = NULL;
	Vector vecTarget;
	Vector vecDir;
	float flDist, flMax, flDot;
	TraceResult tr;

	UTIL_MakeAimVectors( pev->angles );

	vecTarget = gpGlobals->v_forward;
	flMax = 4096;
	pev->angles = UTIL_VecToAngles( vecTarget );

	// this acceleration and turning math is totally wrong, but it seems to respond well so don't change it.
	float flSpeed = pev->velocity.Length();
	if (gpGlobals->time - m_flIgniteTime < 1.0)
	{
		pev->velocity = pev->velocity * 0.2 + vecTarget * (flSpeed * 0.8 + 400);
		if (pev->waterlevel == 3)
		{
			// go slow underwater
			if (pev->velocity.Length() > 300)
			{
				pev->velocity = pev->velocity.Normalize() * 300;
			}
			UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 4 );
		} 
		else 
		{
			if (pev->velocity.Length() > 200)
			{
				pev->velocity = pev->velocity.Normalize() * 200;
			}
		}
	}
	else
	{

	}

	pev->velocity.x = pev->velocity.x + (RANDOM_FLOAT(-100,100));
	pev->velocity.y = pev->velocity.y + (RANDOM_FLOAT(-100,100));
	pev->velocity.z = pev->velocity.z + (RANDOM_FLOAT(-100,100));

	pev->nextthink = gpGlobals->time + 0.1;
}
#endif

enum rocket_crowbar_e {
	ROCKET_CROWBAR_IDLE = 0,
	ROCKET_CROWBAR_DRAW_LOWKEY,
	ROCKET_CROWBAR_DRAW,
	ROCKET_CROWBAR_HOLSTER,
	ROCKET_CROWBAR_ATTACK1HIT,
	ROCKET_CROWBAR_ATTACK1MISS,
	ROCKET_CROWBAR_ATTACK2MISS,
	ROCKET_CROWBAR_ATTACK2HIT,
	ROCKET_CROWBAR_ATTACK3MISS,
	ROCKET_CROWBAR_ATTACK3HIT,
	ROCKET_CROWBAR_IDLE2,
	ROCKET_CROWBAR_IDLE3,
};

void CRocketCrowbar::Spawn( )
{
	Precache( );
	m_iId = WEAPON_ROCKETCROWBAR;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_ROCKETCROWBAR - 1;
	m_iClip = -1;

	UTIL_PrecacheOther( "drunk_rocket" );

	FallInit();// get ready to fall down.
}

void CRocketCrowbar::Precache( void )
{
	PRECACHE_MODEL("models/v_rocketcrowbar.mdl");
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_weapons.mdl");
	PRECACHE_SOUND("cbar_hit1.wav");
	PRECACHE_SOUND("weapons/cbar_hit2.wav");
	PRECACHE_SOUND("cbar_hitbod1.wav");
	PRECACHE_SOUND("cbar_hitbod2.wav");
	PRECACHE_SOUND("cbar_hitbod3.wav");
	PRECACHE_SOUND("weapons/cbar_miss1.wav");

	m_usRocketCrowbar = PRECACHE_EVENT ( 1, "events/rocketcrowbar.sc" );
}

int CRocketCrowbar::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CRocketCrowbar::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 7;
	p->iId = WEAPON_ROCKETCROWBAR;
	p->iWeight = ROCKETCROWBAR_WEIGHT;
	p->pszDisplayName = "Rocket Crowbar";
	return 1;
}

BOOL CRocketCrowbar::DeployLowKey( )
{
	return DefaultDeploy( "models/v_rocketcrowbar.mdl", "models/p_weapons.mdl", ROCKET_CROWBAR_DRAW_LOWKEY, "crowbar", UseDecrement(), 1 );
}

BOOL CRocketCrowbar::Deploy( )
{
	return DefaultDeploy( "models/v_rocketcrowbar.mdl", "models/p_weapons.mdl", ROCKET_CROWBAR_DRAW, "crowbar", UseDecrement(), 1 );
}

void CRocketCrowbar::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(ROCKET_CROWBAR_HOLSTER);
}

void CRocketCrowbar::PrimaryAttack()
{
	if (!Swing( 1 ))
	{
		SetThink( &CRocketCrowbar::SwingAgain );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CRocketCrowbar::SecondaryAttack()
{
	if (!Swing( 1 ))
	{
#ifndef CLIENT_DLL
		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
		Vector vecSrc = m_pPlayer->GetGunPosition( ) + (vecAiming * 48) + gpGlobals->v_right * 8;
		CDrunkRocket::CreateDrunkRocket( vecSrc, vecAiming, m_pPlayer );
#endif

		SetThink( &CRocketCrowbar::SwingAgain );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}

void CRocketCrowbar::Smack( )
{
	DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}

void CRocketCrowbar::SwingAgain( void )
{
	Swing( 0 );
}

int CRocketCrowbar::Swing( int fFirst )
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

	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usRocketCrowbar, 
	0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0, 0, 0,
	0.0, 0, 0.0 );


	if ( tr.flFraction >= 1.0 )
	{
		if (fFirst)
		{
			// miss
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.5);
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
			SendWeaponAnim( ROCKET_CROWBAR_ATTACK1HIT, UseDecrement(), 1 ); break;
		case 1:
			SendWeaponAnim( ROCKET_CROWBAR_ATTACK2HIT, UseDecrement(), 1); break;
		case 2:
			SendWeaponAnim( ROCKET_CROWBAR_ATTACK3HIT, UseDecrement(), 1 ); break;
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
					  m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.25);
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
		m_flNextPrimaryAttack = m_flNextSecondaryAttack =GetNextAttackDelay(0.25);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		
		SetThink( &CRocketCrowbar::Smack );
		pev->nextthink = UTIL_WeaponTimeBase() + 0.2;
	}
	return fDidHit;
}

void CRocketCrowbar::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	float flAnimTime = 5.34; // Only CROWBAR_IDLE has a different time
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		iAnim = ROCKET_CROWBAR_IDLE;
		flAnimTime = 2.7;
		break;
	case 1:
		iAnim = ROCKET_CROWBAR_IDLE2;
		break;
	case 2:
		iAnim = ROCKET_CROWBAR_IDLE3;
		break;
	}
	SendWeaponAnim( iAnim, UseDecrement(), 1 );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flAnimTime;
}
