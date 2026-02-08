
//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Weapon functionality for Discwar
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "effects.h"
#include "disc.h"
 
// Disc trail colors
float g_iaDiscColors[33][3] =
{
	{ 0, 160, 255 },	// Cold Ice
	{ 200, 0, 0 },	// Red
	{ 225, 205, 45 },	// Yellow
	{ 0, 200, 0 },	// Green
	{ 128, 0, 128 },
	{ 0, 128, 128 },
	{ 128, 128, 128 },
	{ 64, 128, 0 },
	{ 128, 64, 0 },
	{ 128, 0, 64 },
	{ 64, 0, 128 },
	{ 0, 64, 128 },
	{ 64, 64, 128 },
	{ 128, 64, 64 },
	{ 64, 128, 64 },
	{ 128, 128, 64 },
	{ 128, 64, 128 },
	{ 64, 128, 128 },
	{ 250, 128, 0 },
	{ 128, 250, 0 },
	{ 128, 0, 250 },
	{ 250, 0, 128 },
	{ 0, 250, 128 },
	{ 250, 250, 128 },
	{ 250, 128, 250 },
	{ 128, 250, 250 },
	{ 250, 128, 64 },
	{ 250, 64, 128 },
	{ 128, 250, 64 },
	{ 64, 128, 250 },
	{ 128, 64, 250 },
	{ 255, 255, 255 },
};

enum disc_e 
{
	DISC_IDLE = 0,
	DISC_FIDGET,
	DISC_PINPULL,
	DISC_THROW1,	// toss
	DISC_THROW2,	// medium
	DISC_THROW3,	// hard
	DISC_HOLSTER,
	DISC_DRAW
};

#if !defined( CLIENT_DLL )
LINK_ENTITY_TO_CLASS( disc, CDisc );

//========================================================================================
// DISC
//========================================================================================
void CDisc::Spawn( void )
{
	Precache( );

	pev->classname = MAKE_STRING("disc");
	pev->movetype = MOVETYPE_BOUNCEMISSILE;
	pev->solid = SOLID_TRIGGER;

	// Setup model
	SET_MODEL(ENT(pev), "models/w_grenade.mdl");
    pev->body = 1;
	UTIL_SetSize(pev, Vector( -4,-4,-4 ), Vector(4, 4, 4));

	UTIL_SetOrigin( pev, pev->origin );
	SetTouch( &CDisc::DiscTouch );
	SetThink( &CDisc::DiscThink );

	m_iBounces = 0;
	m_fDontTouchOwner = gpGlobals->time + 0.2;
	m_fDontTouchEnemies = 0;
	m_bRemoveSelf = false;
	m_bTeleported = false;
	m_pLockTarget = NULL;

	UTIL_MakeVectors( pev->angles );
	pev->velocity = gpGlobals->v_forward * DISC_VELOCITY;
	pev->angles = UTIL_VecToAngles(pev->velocity);

	// Pull our owner out so we will still touch it
	if ( pev->owner )
		m_hOwner = Instance(pev->owner);
	pev->owner = NULL;

	// Trail
	// Clamp team index to prevent array overflow
	int teamIndex = pev->team;
	if (teamIndex < 0 || teamIndex >= 33)
		teamIndex = 0;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model

	if (m_bDecapitate)
		WRITE_BYTE( 5 ); // life
	else
		WRITE_BYTE( 3 ); // life

		WRITE_BYTE( 3 );  // width

		WRITE_BYTE( g_iaDiscColors[teamIndex][0] ); // r, g, b
		WRITE_BYTE( g_iaDiscColors[teamIndex][1] ); // r, g, b
		WRITE_BYTE( g_iaDiscColors[teamIndex][2] ); // r, g, b

		WRITE_BYTE( 250 );	// brightness
	MESSAGE_END();

	// Decapitator's make sound
	if (m_bDecapitate)
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/rocket1.wav", 0.5, 0.5 );

	// Highlighter
	pev->renderfx = kRenderFxGlowShell;
	for (int i = 0; i <= 2;i ++)
	{
		// Clamp team index to prevent array overflow
		int teamIndex = pev->team;
		if (teamIndex < 0 || teamIndex >= 33)
			teamIndex = 0;
		pev->rendercolor[i] = g_iaDiscColors[teamIndex][i];
	}
	pev->renderamt = 10;

	pev->nextthink = gpGlobals->time + 0.1;
}

void CDisc::Precache( void )
{
	PRECACHE_MODEL("models/w_grenade.mdl");
	PRECACHE_SOUND("weapons/cbar_hitbod1.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod2.wav");
	PRECACHE_SOUND("weapons/cbar_hitbod3.wav");
	PRECACHE_SOUND("items/gunpickup2.wav");
	PRECACHE_SOUND("weapons/electro5.wav");
	PRECACHE_SOUND("weapons/xbow_hit1.wav");
	PRECACHE_SOUND("weapons/xbow_hit2.wav");
	PRECACHE_SOUND("weapons/rocket1.wav");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	m_iSpriteTexture = PRECACHE_MODEL( "sprites/lgtning.spr" );
}

// Give the disc back to it's owner
void CDisc::ReturnToThrower( void )
{
	if (m_bDecapitate)
	{
		STOP_SOUND( edict(), CHAN_VOICE, "weapons/rocket1.wav" );
		if ( !m_bRemoveSelf && m_hOwner )
			((CBasePlayer*)(CBaseEntity*)m_hOwner)->GiveAmmo( MAX_DISCS, "disc", MAX_DISCS );
	}
	else
	{
		if ( !m_bRemoveSelf && m_hOwner )
			((CBasePlayer*)(CBaseEntity*)m_hOwner)->GiveAmmo( 1, "disc", MAX_DISCS );
	}

	UTIL_Remove( this );
	if (m_hOwner)
		((CBasePlayer*)(CBaseEntity*)m_hOwner)->m_iFlyingDiscs -= 1;

}

void CDisc::DiscTouch ( CBaseEntity *pOther )
{
	if ( pOther->IsAlive() )
	{
		if ( m_hOwner && ((CBaseEntity*)m_hOwner) == pOther )
		{
			if (m_fDontTouchOwner < gpGlobals->time)
			{
				// Play catch sound
				EMIT_SOUND_DYN( pOther->edict(), CHAN_WEAPON, "items/gunpickup2.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 

				ReturnToThrower();
			}

			return;
		}
		else if ( m_fDontTouchEnemies < gpGlobals->time)
		{
			if (pOther->IsPlayer())
			{
				if ( pev->team != pOther->pev->team )
				{
					// Decap or push
					if (m_bDecapitate)
					{
						// Decapitate!
						if ( m_bTeleported )
							((CBasePlayer*)pOther)->m_flLastDiscHitTeleport = gpGlobals->time;

						m_fDontTouchEnemies = gpGlobals->time + 0.5;
					}
					else 
					{
						// Play thwack sound
						switch( RANDOM_LONG(0,2) )
						{
						case 0:
							EMIT_SOUND_DYN( pOther->edict(), CHAN_ITEM, "weapons/cbar_hitbod1.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
							break;
						case 1:
							EMIT_SOUND_DYN( pOther->edict(), CHAN_ITEM, "weapons/cbar_hitbod2.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
							break;
						case 2:
							EMIT_SOUND_DYN( pOther->edict(), CHAN_ITEM, "weapons/cbar_hitbod3.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
							break;
						}

						// Push the player
						Vector vecDir = pev->velocity.Normalize();
						pOther->pev->flags &= ~FL_ONGROUND;
						((CBasePlayer*)pOther)->m_vecHitVelocity = vecDir * DISC_PUSH_MULTIPLIER;

						((CBasePlayer*)pOther)->m_hLastPlayerToHitMe = m_hOwner;
						((CBasePlayer*)pOther)->m_flLastDiscHit = gpGlobals->time;
						((CBasePlayer*)pOther)->m_iLastDiscBounces = m_iBounces;
						if ( m_bTeleported )
							((CBasePlayer*)pOther)->m_flLastDiscHitTeleport = gpGlobals->time;

						// Only do damage if owner still exists
						if (m_hOwner)
						{
							UTIL_MakeVectors(pev->angles);
							TraceResult tr;
							Vector vecEnd = pev->origin + gpGlobals->v_forward * 32;
							UTIL_TraceLine(pev->origin, vecEnd, dont_ignore_monsters, ENT(m_hOwner->pev), &tr);

							if (tr.iHitgroup == HITGROUP_HEAD)
							{
								extern entvars_t *g_pevLastInflictor;
								g_pevLastInflictor = pev;
								((CBasePlayer*)pOther)->pev->health = 0; // without this, player can walk as a ghost.
								((CBasePlayer*)pOther)->Killed(m_hOwner->pev, GIB_NEVER);
							}
							else
							{
								ClearMultiDamage();
								pOther->TraceAttack(m_hOwner->pev, gSkillData.plrDmgCrowbar, gpGlobals->v_forward, &tr, DMG_SLASH);
								ApplyMultiDamage(pev, m_hOwner->pev);
							}
						}

						m_fDontTouchEnemies = gpGlobals->time + 2.0;
					}
				}
			}
			else
			{
				// Only do damage if owner still exists
				if (m_hOwner)
				{
					UTIL_MakeVectors(pev->angles);
					TraceResult tr;
					Vector vecEnd = pev->origin + gpGlobals->v_forward * 32;
					UTIL_TraceLine(pev->origin, vecEnd, dont_ignore_monsters, ENT(m_hOwner->pev), &tr);
					
					ClearMultiDamage();
					pOther->TraceAttack(m_hOwner->pev, gSkillData.plrDmgCrowbar, gpGlobals->v_forward, &tr, DMG_SLASH);
					ApplyMultiDamage(pev, m_hOwner->pev);
				}

				m_fDontTouchEnemies = gpGlobals->time + 2.0;	
			}
		}
	}
	// Hit a disc?
	else if ( pOther->pev->iuser4 ) 
	{
		// Enemy Discs destroy each other
		if ( pOther->pev->iuser4 != pev->iuser4 )
		{
			// Play a warp sound and sprite
			CSprite *pSprite = CSprite::SpriteCreate( /*"sprites/discreturn.spr"*/ "sprites/lgtning.spr", pev->origin, TRUE );
			pSprite->AnimateAndDie( 60 );
			pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
			pSprite->SetScale( 1 );
			EMIT_SOUND_DYN( edict(), CHAN_ITEM, "dischit.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));

			// Return both discs to their owners
			((CDisc*)pOther)->ReturnToThrower();
			ReturnToThrower();
		}
		else
		{
			// Friendly discs just pass through each other
		}
	}
	else
	{
		m_iBounces++;

		switch ( RANDOM_LONG( 0, 1 ) )
		{
		case 0:	EMIT_SOUND_DYN( edict(), CHAN_ITEM, "weapons/xbow_hit1.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));  break;
		case 1:	EMIT_SOUND_DYN( edict(), CHAN_ITEM, "weapons/xbow_hit2.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));  break;
		}

		UTIL_Sparks( pev->origin );
		pev->angles = UTIL_VecToAngles(pev->velocity);
	}
}

void CDisc::DiscThink()
{
	// Make Freeze discs home towards any player ahead of them

	// Track the player if we've bounced 3 or more times ( Fast discs remove immediately )
	if ( m_iBounces >= 3 /*|| (m_iPowerupFlags & POW_FAST && m_iBounces >= 1)*/ )
	{
		// Remove myself if my owner's died
		if (m_bRemoveSelf)
		{
			STOP_SOUND( edict(), CHAN_VOICE, "weapons/rocket1.wav" );
			UTIL_Remove( this );
			if (m_hOwner)
				((CBasePlayer*)(CBaseEntity*)m_hOwner)->m_iFlyingDiscs -= 1;
			return;
		}

		// 7 Bounces, just remove myself
		if ( m_iBounces > 7 )
		{
			ReturnToThrower();
			return;
		}

		// Start heading for the player
		if ( m_hOwner )
		{
			Vector vecDir = ( m_hOwner->pev->origin - pev->origin );
			vecDir = vecDir.Normalize();
			pev->velocity = vecDir * DISC_VELOCITY;
			pev->nextthink = gpGlobals->time + 0.1;
		}
		else
		{
			UTIL_Remove( this );
			if (m_hOwner)
				((CBasePlayer*)(CBaseEntity*)m_hOwner)->m_iFlyingDiscs -= 1;
		}
	}

	// Sanity check
	if ( pev->velocity == g_vecZero )
		ReturnToThrower();

	pev->nextthink = gpGlobals->time + 0.1;
}

CDisc *CDisc::CreateDisc( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, bool bDecapitator, int iPowerupFlags )
{
	CDisc *pDisc = GetClassPtr( (CDisc *)NULL );

	UTIL_SetOrigin( pDisc->pev, vecOrigin );
	pDisc->m_iPowerupFlags = iPowerupFlags;
	pDisc->m_bDecapitate = bDecapitator;

	pDisc->pev->angles = vecAngles;
	pDisc->pev->owner = pOwner->edict();
	if (pOwner->IsPlayer())
		pDisc->pev->team = g_pGameRules->GetTeamIndex(((CBasePlayer *)pOwner)->m_szTeamName);
	else
		pDisc->pev->team = 0;
	pDisc->pev->iuser4 = pOwner->pev->iuser4;

	// Set the Group Info
	pDisc->pev->groupinfo = pOwner->pev->groupinfo;

	pDisc->Spawn();

	return pDisc;
}
#endif // !CLIENT_DLL
