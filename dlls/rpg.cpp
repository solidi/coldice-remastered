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
#if !defined( OEM_BUILD )

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "game.h"


enum rpg_e {
	DUAL_RPG_DRAW_LEFT = 0,
	DUAL_RPG_DRAW_BOTH_LOWKEY,
	DUAL_RPG_DRAW_BOTH,
	DUAL_RPG_IDLE_BOTH,
	DUAL_RPG_FIRE_BOTH,
	DUAL_RPG_HOLSTER_BOTH,
	DUAL_RPG_RELOAD_BOTH,
};

#ifdef RPG
LINK_ENTITY_TO_CLASS( weapon_rpg, CRpg );
#endif

#ifndef CLIENT_DLL

LINK_ENTITY_TO_CLASS( laser_spot, CLaserSpot );

//=========================================================
//=========================================================
CLaserSpot *CLaserSpot::CreateSpot( void )
{
	CLaserSpot *pSpot = GetClassPtr( (CLaserSpot *)NULL );
	pSpot->Spawn();

	pSpot->pev->classname = MAKE_STRING("laser_spot");

	return pSpot;
}

//=========================================================
//=========================================================
void CLaserSpot::Spawn( void )
{
	Precache( );
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;

	pev->rendermode = kRenderGlow;
	pev->renderfx = kRenderFxNoDissipation;
	pev->renderamt = 255;

	SET_MODEL(ENT(pev), "sprites/laserdot.spr");
	UTIL_SetOrigin( pev, pev->origin );
};

//=========================================================
// Suspend- make the laser sight invisible. 
//=========================================================
void CLaserSpot::Suspend( float flSuspendTime )
{
	pev->effects |= EF_NODRAW;
	
	SetThink( &CLaserSpot::Revive );
	pev->nextthink = gpGlobals->time + flSuspendTime;
}

//=========================================================
// Revive - bring a suspended laser sight back.
//=========================================================
void CLaserSpot::Revive( void )
{
	pev->effects &= ~EF_NODRAW;

	SetThink( NULL );
}

void CLaserSpot::Precache( void )
{
	PRECACHE_MODEL("sprites/laserdot.spr");
};

LINK_ENTITY_TO_CLASS( rpg_rocket, CRpgRocket );

//=========================================================
//=========================================================
CBaseEntity *CRpgRocket::CreateRpgRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, float startEngineTime, BOOL redRocket )
{
	if (g_pGameRules->MutatorEnabled(MUTATOR_ROCKETCROWBAR))
	{
		CBaseEntity *pRocket = CDrunkRocket::CreateDrunkRocket( vecOrigin, vecAngles, pOwner );
		if (pRocket)
			pRocket->pev->classname = MAKE_STRING("rpg_rocket");
		return pRocket;
	}

	CRpgRocket *pRocket = GetClassPtr( (CRpgRocket *)NULL );
	UTIL_SetOrigin( pRocket->pev, vecOrigin );
	pRocket->pev->angles = UTIL_VecToAngles(vecAngles);
	pRocket->Spawn(startEngineTime);
	pRocket->SetTouch( &CRpgRocket::RocketTouch );
	//pRocket->m_pLauncher = pLauncher;// remember what RPG fired me. 
	//pRocket->m_pLauncher->m_cActiveRockets++;// register this missile as active for the launcher
	pRocket->pev->owner = pOwner->edict();
	pRocket->m_redRocket = redRocket;

	return pRocket;
}

/**
 * Turrets and creation, without angle adjustment.
 */
void CRpgRocket :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_items.mdl");
	pev->body = 8;
	pev->sequence = 9;
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );

	pev->classname = MAKE_STRING("rpg_rocket");

	SetThink( &CRpgRocket::IgniteThink );
	SetTouch( &CRpgRocket::ExplodeTouch );

	pev->gravity = 0.5;

	pev->nextthink = gpGlobals->time;
	pev->dmg = gSkillData.plrDmgRPG;
}

//=========================================================
//=========================================================
void CRpgRocket :: Spawn( float startEngineTime )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_items.mdl");
	pev->body = 8;
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );

	pev->classname = MAKE_STRING("rpg_rocket");

	SetThink( &CRpgRocket::IgniteThink );
	SetTouch( &CRpgRocket::ExplodeTouch );

	pev->gravity = 0.5;

	pev->nextthink = gpGlobals->time + startEngineTime;
	pev->dmg = gSkillData.plrDmgRPG;
}

//=========================================================
//=========================================================
void CRpgRocket :: RocketTouch ( CBaseEntity *pOther )
{
	/*
	if ( m_pLauncher )
	{
		// my launcher is still around, tell it I'm dead.
		m_pLauncher->m_cActiveRockets--;
	}
	*/

	STOP_SOUND( edict(), CHAN_VOICE, "rocket1.wav" );
	ExplodeTouch( pOther );
}

//=========================================================
//=========================================================
void CRpgRocket :: Precache( void )
{
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_SOUND ("rocket1.wav");
}


void CRpgRocket :: IgniteThink( void  )
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
		WRITE_BYTE( 5 );  // width
		if (icesprites.value) {
			if (m_redRocket) {
				WRITE_BYTE( 224 );   // r, g, b
				WRITE_BYTE( 0 );   // r, g, b
				WRITE_BYTE( 0 );   // r, g, b
			} else {
				WRITE_BYTE( 0 );   // r, g, b
				WRITE_BYTE( 160 );   // r, g, b
				WRITE_BYTE( 255 );   // r, g, b
			}
		} else {
			WRITE_BYTE( 224 );   // r, g, b
			WRITE_BYTE( 224 );   // r, g, b
			WRITE_BYTE( 255 );   // r, g, b
		}
		WRITE_BYTE( 255 );	// brightness

	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

	m_flIgniteTime = gpGlobals->time;

	// set to follow laser spot
	SetThink( &CRpgRocket::FollowThink );
	pev->nextthink = gpGlobals->time + 0.1;
}


void CRpgRocket :: FollowThink( void  )
{
	CBaseEntity *pOther = NULL;
	Vector vecTarget;
	Vector vecDir;
	float flDist, flMax, flDot;
	TraceResult tr;

	UTIL_MakeAimVectors( pev->angles );

	vecTarget = gpGlobals->v_forward;
	flMax = 4096;
	
	// Examine all entities within a reasonable radius
	while ((pOther = UTIL_FindEntityByClassname( pOther, "laser_spot" )) != NULL)
	{
		UTIL_TraceLine ( pev->origin, pOther->pev->origin, dont_ignore_monsters, ENT(pev), &tr );
		// ALERT( at_console, "%f\n", tr.flFraction );
		if (tr.flFraction >= 0.90)
		{
			vecDir = pOther->pev->origin - pev->origin;
			flDist = vecDir.Length( );
			vecDir = vecDir.Normalize( );
			flDot = DotProduct( gpGlobals->v_forward, vecDir );
			if ((flDot > 0) && (flDist * (1 - flDot) < flMax))
			{
				flMax = flDist * (1 - flDot);
				vecTarget = vecDir;
			}
		}
	}

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
			float speed = 2000;

			if (pev->velocity.Length() > speed)
			{
				pev->velocity = pev->velocity.Normalize() * speed;
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



void CRpg::Reload( void )
{
	int iResult = 0;

	if ( m_pPlayer->ammo_rockets <= 0 )
		return;

	if (m_flNextReload > gpGlobals->time)
		return;

	if (m_iClip == RPG_MAX_CLIP)
	{
		m_fSpotActive = ! m_fSpotActive;

#ifndef CLIENT_DLL
		if (!m_fSpotActive && m_pSpot)
		{
			m_pSpot->Killed( NULL, GIB_NORMAL );
			m_pSpot = NULL;
		}
#endif

		m_flNextReload = gpGlobals->time + 0.25;
	}

	// because the RPG waits to autoreload when no missiles are active while  the LTD is on, the
	// weapons code is constantly calling into this function, but is often denied because 
	// a) missiles are in flight, but the LTD is on
	// or
	// b) player is totally out of ammo and has nothing to switch to, and should be allowed to
	//    shine the designator around
	//
	// Set the next attack time into the future so that WeaponIdle will get called more often
	// than reload, allowing the RPG LTD to be updated
	
	m_flNextPrimaryAttack = GetNextAttackDelay(0.5);

	if ( m_cActiveRockets && m_fSpotActive )
	{
		// no reloading when there are active missiles tracking the designator.
		// ward off future autoreload attempts by setting next attack time into the future for a bit. 
		return;
	}

#ifndef CLIENT_DLL
	if ( m_pSpot && m_fSpotActive )
	{
		m_pSpot->Suspend( 2.1 );
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.1;
	}
#endif

	iResult = DefaultReload( RPG_MAX_CLIP, DUAL_RPG_RELOAD_BOTH, 2 );
	
	if ( iResult )
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	
}

void CRpg::Spawn( )
{
	Precache( );
	m_iId = WEAPON_RPG;

	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_RPG - 1;

#ifdef CLIENT_DLL
	if ( bIsMultiplayer() )
#else
	if ( g_pGameRules->IsMultiplayer() )
#endif
	{
		// more default ammo in multiplay. 
		m_iDefaultAmmo = RPG_DEFAULT_GIVE * 2;
	}
	else
	{
		m_iDefaultAmmo = RPG_DEFAULT_GIVE;
	}
	pev->dmg = gSkillData.plrDmgRPG;

	FallInit();// get ready to fall down.
}


void CRpg::Precache( void )
{
	PRECACHE_MODEL("models/v_dual_rpg.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");

	UTIL_PrecacheOther( "laser_spot" );
	UTIL_PrecacheOther( "rpg_rocket" );

	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("weapons/glauncher.wav"); // alternative fire sound

	m_usRpg = PRECACHE_EVENT ( 1, "events/rpg.sc" );
	m_usRpgExtreme = PRECACHE_EVENT ( 1, "events/rpg_extreme.sc" );
}


int CRpg::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "rockets";
	p->iMaxAmmo1 = ROCKET_MAX_CARRY;
	p->pszAmmo2 = "rockets";
	p->iMaxAmmo2 = ROCKET_MAX_CARRY;
	p->iMaxClip = RPG_MAX_CLIP;
	p->iSlot = 3;
	p->iPosition = 0;
	p->iId = m_iId = WEAPON_RPG;
	p->iFlags = 0;
	p->iWeight = RPG_WEIGHT;
	p->pszDisplayName = "50-Pound Automatic LAW Rocket Launcher";

	return 1;
}

int CRpg::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

BOOL CRpg::DeployLowKey( )
{
	m_fSpotActive = 1;
	return DefaultDeploy( "models/v_dual_rpg.mdl", "models/p_weapons.mdl", DUAL_RPG_DRAW_BOTH_LOWKEY, "rpg" );
}

BOOL CRpg::Deploy( )
{
	m_fSpotActive = 1;

	if ( m_iClip == 0 )
	{
		return DefaultDeploy( "models/v_dual_rpg.mdl", "models/p_weapons.mdl", DUAL_RPG_DRAW_BOTH, "rpg" );
	}

	return DefaultDeploy( "models/v_dual_rpg.mdl", "models/p_weapons.mdl", DUAL_RPG_DRAW_BOTH, "rpg" );
}


BOOL CRpg::CanHolster( void )
{
	if ( m_fSpotActive && m_cActiveRockets )
	{
		// can't put away while guiding a missile.
		return FALSE;
	}

	return TRUE;
}

void CRpg::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(DUAL_RPG_HOLSTER_BOTH);

	m_fSpotActive = 0;

#ifndef CLIENT_DLL
	if (m_pSpot)
	{
		m_pSpot->Killed( NULL, GIB_NEVER );
		m_pSpot = NULL;
	}
#endif

}



void CRpg::PrimaryAttack()
{
	if ( m_iClip )
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

#ifndef CLIENT_DLL
		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
		Vector vecSrc = m_pPlayer->GetGunPosition( ) + (vecAiming * 32) + gpGlobals->v_right * 8 + gpGlobals->v_up * -8;

		CBaseEntity *pRocket = CRpgRocket::CreateRpgRocket( vecSrc, vecAiming, m_pPlayer, 0.0, FALSE );
		if (pRocket)
			pRocket->pev->velocity = pRocket->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );
#endif

		// firing RPG no longer turns on the designator. ALT fire is a toggle switch for the LTD.
		// Ken signed up for this as a global change (sjb)

		int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

		PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usRpg );

		m_iClip--; 
				
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.75);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5;
	}
	else
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
	}
	UpdateSpot( );
}


void CRpg::SecondaryAttack()
{
	if ( m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] > 0 )
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

#ifndef CLIENT_DLL
		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
		Vector vecSrc1 = m_pPlayer->GetGunPosition( ) + vecAiming * 32 + gpGlobals->v_right * 16 + gpGlobals->v_up * -8;
		Vector vecSrc2 = m_pPlayer->GetGunPosition( ) + vecAiming * 32 + gpGlobals->v_right * -16 + gpGlobals->v_up * -8;

		CBaseEntity *pRocket1 = CRpgRocket::CreateRpgRocket( vecSrc1, vecAiming, m_pPlayer, 0.0, FALSE );
		CBaseEntity *pRocket2 = CRpgRocket::CreateRpgRocket( vecSrc2, vecAiming, m_pPlayer, 0.0, FALSE );

		Vector vecSrc3 = m_pPlayer->GetGunPosition( ) + vecAiming * 16 + gpGlobals->v_right * 24 + gpGlobals->v_up * -8;
		Vector vecSrc4 = m_pPlayer->GetGunPosition( ) + vecAiming * 16 + gpGlobals->v_right + gpGlobals->v_up * -8;
		Vector vecSrc5 = m_pPlayer->GetGunPosition( ) + vecAiming * 16 + gpGlobals->v_right * -24 + gpGlobals->v_up * -8;

		CBaseEntity *pRocket3 = CRpgRocket::CreateRpgRocket( vecSrc3, vecAiming, m_pPlayer, 0.0, TRUE );
		CBaseEntity *pRocket4 = CRpgRocket::CreateRpgRocket( vecSrc4, vecAiming, m_pPlayer, 0.0, TRUE );
		CBaseEntity *pRocket5 = CRpgRocket::CreateRpgRocket( vecSrc5, vecAiming, m_pPlayer, 0.0, TRUE );

		if (pRocket1)
			pRocket1->pev->velocity = pRocket1->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );
		if (pRocket2)
			pRocket2->pev->velocity = pRocket2->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );
		if (pRocket3)
			pRocket3->pev->velocity = pRocket3->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );
		if (pRocket4)
			pRocket4->pev->velocity = pRocket4->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );
		if (pRocket5)
			pRocket5->pev->velocity = pRocket5->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );
#endif

		int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

		PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usRpgExtreme );

		m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType]--;

		m_flNextSecondaryAttack = GetNextAttackDelay(3.5);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.5;
	}
	else
	{
		// PlayEmptySound( ); // Stop rapid clicking.
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
	}
	UpdateSpot( );
}

int CRpg::SecondaryAmmoIndex( void )
{
	return m_iSecondaryAmmoType;
}

void CRpg::WeaponIdle( void )
{
	UpdateSpot( );

	ResetEmptySound( );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
		if (flRand <= 0.75 || m_fSpotActive)
		{
			if ( m_iClip == 0 )
				iAnim = DUAL_RPG_IDLE_BOTH;
			else
				iAnim = DUAL_RPG_IDLE_BOTH;

			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 90.0 / 15.0;
		}
		else
		{
			if ( m_iClip == 0 )
				iAnim = DUAL_RPG_IDLE_BOTH;
			else
				iAnim = DUAL_RPG_IDLE_BOTH;

			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 6.1;
		}

		SendWeaponAnim( iAnim );
	}
	else
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1;
	}
}



void CRpg::UpdateSpot( void )
{

#ifndef CLIENT_DLL
	if (m_fSpotActive)
	{
		if (!m_pSpot)
		{
			m_pSpot = CLaserSpot::CreateSpot();
		}

		Vector vecSrc = m_pPlayer->GetGunPosition( );
		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

		TraceResult tr;
		UTIL_TraceLine ( vecSrc, vecSrc + vecAiming * 8192, dont_ignore_monsters, ENT(m_pPlayer->pev), &tr );
		
		UTIL_SetOrigin( m_pSpot->pev, tr.vecEndPos );
	}
#endif

}

void CRpg::ProvideDualItem(CBasePlayer *pPlayer, const char *item) {
	if (pPlayer == NULL || item == NULL) {
		return;
	}

#ifndef CLIENT_DLL
	CBasePlayerWeapon::ProvideDualItem(pPlayer, item);

	if (!stricmp(item, "weapon_rpg")) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_rpg")) {
			pPlayer->GiveNamedItem("weapon_dual_rpg");
			pPlayer->SelectItem("weapon_dual_rpg");
		}
	}
#endif
}

void CRpg::SwapDualWeapon( void ) {
	m_pPlayer->SelectItem("weapon_dual_rpg");
}

class CRpgAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_ammo.mdl");
		pev->body = AMMO_RPG;
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_ammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int iGive;

#ifdef CLIENT_DLL
	if ( bIsMultiplayer() )
#else
	if ( g_pGameRules->IsMultiplayer() )
#endif
		{
			// hand out more ammo per rocket in multiplayer.
			iGive = AMMO_RPGCLIP_GIVE * 2;
		}
		else
		{
			iGive = AMMO_RPGCLIP_GIVE;
		}

		if (pOther->GiveAmmo( iGive, "rockets", ROCKET_MAX_CARRY ) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_rpgclip, CRpgAmmo );

#endif
