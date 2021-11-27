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
#include "shake.h"

enum nuke_e {
	NUKE_IDLE = 0,
	NUKE_FIDGET,
	NUKE_RELOAD,		// to reload
	NUKE_FIRE2,		// to empty
	NUKE_FIRE3,		// to empty
	NUKE_HOLSTER1,	// loaded
	NUKE_DRAW1,		// loaded
	NUKE_HOLSTER2,	// unloaded
	NUKE_DRAW_UL,	// unloaded
	NUKE_IDLE_UL,	// unloaded idle
	NUKE_FIDGET_UL,	// unloaded fidget
};

#ifdef NUKE
LINK_ENTITY_TO_CLASS( weapon_nuke, CNuke );
#endif

#ifndef CLIENT_DLL

LINK_ENTITY_TO_CLASS( nuke_rocket, CNukeRocket );

CNukeRocket *CNukeRocket::CreateNukeRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, CNuke *pLauncher, float startEngineTime )
{
	CNukeRocket *pRocket = GetClassPtr( (CNukeRocket *)NULL );

	UTIL_SetOrigin( pRocket->pev, vecOrigin );
	pRocket->pev->angles = vecAngles;
	pRocket->Spawn(startEngineTime);
	pRocket->SetTouch( &CNukeRocket::RocketTouch );
	pRocket->m_pLauncher = pLauncher;// remember what Nuke fired me. 
	pRocket->pev->owner = pOwner->edict();

	return pRocket;
}

void CNukeRocket :: Spawn( float startEngineTime )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );

	pev->classname = MAKE_STRING("nuke_rocket");

	SetThink( &CNukeRocket::IgniteThink );
	SetTouch( &CNukeRocket::ExplodeTouch );

	pev->angles.x -= 30;
	UTIL_MakeVectors( pev->angles );
	pev->angles.x = -(pev->angles.x + 30);

	pev->velocity = gpGlobals->v_forward * 250;
	pev->gravity = 0.5;

	pev->nextthink = gpGlobals->time + startEngineTime;
	pev->dmg = gSkillData.plrDmgNuke;
}

void CNukeRocket :: RocketTouch ( CBaseEntity *pOther )
{
	STOP_SOUND( edict(), CHAN_VOICE, "rocket1.wav" );
	//ExplodeTouch( pOther );

	UTIL_ScreenShake( pev->origin, 48.0, 300.0, 6.0, 0.0 );
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "nuke_explosion.wav", 1, 0.5 );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE(TE_BEAMDISK);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z + 32);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z + 32 + pev->dmg * 2 / .2); // reach damage radius over .3 seconds
		WRITE_SHORT(PRECACHE_MODEL( "sprites/lgtning.spr" ));
		WRITE_BYTE( 0 ); // startframe
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 1000 ); // life
		WRITE_BYTE( 200 );  // width
		WRITE_BYTE( 100 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 160 );   // r, g, b
		WRITE_BYTE( 100 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
		WRITE_BYTE( 0 );		// speed
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE(TE_BEAMCYLINDER);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z + 32);
		WRITE_COORD(pev->origin.x);
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z + 32 + pev->dmg * 2 / .2); // reach damage radius over .3 seconds
		WRITE_SHORT(PRECACHE_MODEL( "sprites/lgtning.spr" ));
		WRITE_BYTE( 0 ); // startframe
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 1000 ); // life
		WRITE_BYTE( 200 );  // width
		WRITE_BYTE( 100 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 160 );   // r, g, b
		WRITE_BYTE( 100 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
		WRITE_BYTE( 0 );		// speed
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( pev->origin.x );	// Send to PAS because of the sound
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 128 );
		WRITE_SHORT( m_iExp );
		WRITE_BYTE( (pev->dmg - 50) * .40  ); // scale * 10
		WRITE_BYTE( 20 ); // framerate
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		// FIXME:  Probably don't need to cast this just to read m_iDeaths
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			plr->TakeDamage(pev, VARS(pev->owner), 200, DMG_RADIATION);
			UTIL_ScreenFade(plr, Vector(255, 255, 255), 2, 6, 200, FFADE_IN);
		}
	}

	UTIL_Remove( this );
}

void CNukeRocket :: Precache( void )
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	m_iExp = PRECACHE_MODEL("sprites/nuke2.spr");
	PRECACHE_SOUND ("rocket1.wav");
	PRECACHE_SOUND ("nuke_explosion.wav");
}

void CNukeRocket :: IgniteThink( void  )
{
	// pev->movetype = MOVETYPE_TOSS;

	pev->movetype = MOVETYPE_FLY;
	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "rocket1.wav", 1, 0.5 );

	// rocket trail
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 40 ); // life
		WRITE_BYTE( 2 );  // width
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	m_flIgniteTime = gpGlobals->time;

	// set to follow laser spot
	SetThink( &CNukeRocket::FollowThink );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CNukeRocket :: FollowThink( void  )
{
	CBaseEntity *pOther = NULL;
	Vector vecTarget;
	Vector vecDir;
	float flMax;
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
			if (pev->velocity.Length() > 2000)
			{
				pev->velocity = pev->velocity.Normalize() * 2000;
			}
		}
	}
	else
	{
		if (pev->effects & EF_LIGHT)
		{
			pev->effects = 0;
			STOP_SOUND( ENT(pev), CHAN_VOICE, "rocket1.wav" );
		}
		pev->velocity = pev->velocity * 0.2 + vecTarget * flSpeed * 0.798;
		if (pev->waterlevel == 0 && pev->velocity.Length() < 1500)
		{
			Detonate( );
		}
	}
	// ALERT( at_console, "%.0f\n", flSpeed );

	pev->nextthink = gpGlobals->time + 0.1;
}

#endif

void CNuke::Reload( void )
{
	int iResult = 0;

	if ( m_iClip == 1 )
	{
		// don't bother with any of this if don't need to reload.
		return;
	}

	if ( m_pPlayer->ammo_rockets <= 0 )
		return;

	// because the Nuke waits to autoreload when no missiles are active while  the LTD is on, the
	// weapons code is constantly calling into this function, but is often denied because 
	// a) missiles are in flight, but the LTD is on
	// or
	// b) player is totally out of ammo and has nothing to switch to, and should be allowed to
	//    shine the designator around
	//
	// Set the next attack time into the future so that WeaponIdle will get called more often
	// than reload, allowing the Nuke LTD to be updated
	
	m_flNextPrimaryAttack = GetNextAttackDelay(0.5);

	//if ( m_iClip == 0 )
	//	iResult = DefaultReload( NUKE_MAX_CLIP, NUKE_RELOAD, 2 );
	
	if ( iResult )
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	
}

void CNuke::Spawn( )
{
	Precache( );
	m_iId = WEAPON_NUKE;

	SET_MODEL(ENT(pev), "models/w_nuke.mdl");

#ifdef CLIENT_DLL
	if ( bIsMultiplayer() )
#else
	if ( g_pGameRules->IsMultiplayer() )
#endif
	{
		// more default ammo in multiplay. 
		m_iDefaultAmmo = NUKE_DEFAULT_GIVE * 2;
	}
	else
	{
		m_iDefaultAmmo = NUKE_DEFAULT_GIVE;
	}

	FallInit();// get ready to fall down.
}

void CNuke::Precache( void )
{
	PRECACHE_MODEL("models/w_nuke.mdl");
	PRECACHE_MODEL("models/v_nuke.mdl");
	PRECACHE_MODEL("models/p_nuke.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");

	UTIL_PrecacheOther( "nuke_rocket" );

	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("weapons/glauncher.wav"); // alternative fire sound

	m_usNuke = PRECACHE_EVENT ( 1, "events/nuke1.sc" );
}

int CNuke::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "nuke";
	p->iMaxAmmo1 = NUKE_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = 1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 6;
	p->iId = m_iId = WEAPON_NUKE;
	p->iFlags = 0;
	p->iWeight = NUKE_WEIGHT;
	p->pszDisplayName = "Tactical Nuke Launcher";

	return 1;
}

int CNuke::AddToPlayer( CBasePlayer *pPlayer )
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

BOOL CNuke::Deploy( )
{
	if ( m_iClip == 0 )
	{
		return DefaultDeploy( "models/v_nuke.mdl", "models/p_nuke.mdl", NUKE_DRAW_UL, "rpg" );
	}

	return DefaultDeploy( "models/v_nuke.mdl", "models/p_nuke.mdl", NUKE_DRAW1, "rpg" );
}

void CNuke::Holster( int skiplocal /* = 0 */ )
{
	m_fInReload = FALSE;// cancel any reload in progress.

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	
	SendWeaponAnim( NUKE_HOLSTER1 );
}

void CNuke::PrimaryAttack()
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

#ifndef CLIENT_DLL
		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -18;
		
		CNukeRocket *pRocket = CNukeRocket::CreateNukeRocket( vecSrc, m_pPlayer->pev->v_angle, m_pPlayer, this, 0.0 );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );// NukeRocket::Create stomps on globals, so remake.
		pRocket->pev->velocity = pRocket->pev->velocity + gpGlobals->v_forward * DotProduct( m_pPlayer->pev->velocity, gpGlobals->v_forward );
#endif

		int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

		PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usNuke );

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		m_flNextPrimaryAttack = GetNextAttackDelay(0.75);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5;
	}
	else
	{
		PlayEmptySound( );
	}
}

void CNuke::WeaponIdle( void )
{
	ResetEmptySound( );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
		if (flRand <= 0.75)
		{
			if ( m_iClip == 0 )
				iAnim = NUKE_IDLE_UL;
			else
				iAnim = NUKE_IDLE;

			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 90.0 / 15.0;
		}
		else
		{
			if ( m_iClip == 0 )
				iAnim = NUKE_FIDGET_UL;
			else
				iAnim = NUKE_FIDGET;

			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 6.1;
		}

		SendWeaponAnim( iAnim );
	}
	else
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1;
	}
}
