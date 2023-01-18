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

enum cannon_e {
	CANNON_IDLE = 0,
	CANNON_IDLE2,
	CANNON_FIDGET,
	CANNON_SPINUP,
	CANNON_SPIN,
	CANNON_FIRE_BOMB,
	CANNON_FIRE_FLAK,
	CANNON_HOLSTER1,
	CANNON_DRAW_LOWKEY,
	CANNON_DRAW1
};

#ifdef CANNON
LINK_ENTITY_TO_CLASS( weapon_cannon, CCannon );
#endif

#ifndef CLIENT_DLL

CFlakBomb *CFlakBomb::CreateFlakBomb( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner )
{
	CFlakBomb *pBomb = GetClassPtr( (CFlakBomb *)NULL );

	UTIL_SetOrigin( pBomb->pev, vecOrigin );
	pBomb->pev->angles = vecAngles;
	pBomb->Spawn();
	pBomb->SetTouch( &CFlakBomb::BombTouch );
	pBomb->owner = pOwner;
	pBomb->pev->owner = pOwner->edict();

	return pBomb;
}

void CFlakBomb :: Spawn( )
{
	Precache( );
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/rpgrocket.mdl");
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );
	pev->avelocity.x = RANDOM_FLOAT ( -100, -500 );

	pev->classname = MAKE_STRING("flak_bomb");

	//SetTouch( &CFlakBomb::ExplodeTouch );
	SetThink( &CFlakBomb::BlowUp );
	pev->nextthink = gpGlobals->time + 1.0;
	
	pev->angles.x -= 8;
	UTIL_MakeVectors( pev->angles );
	//pev->angles.x = -(pev->angles.x + 30);

	pev->velocity = gpGlobals->v_forward * RANDOM_LONG(700, 900);
	pev->gravity = 0.5;

	pev->dmg = gSkillData.plrDmgFlakBomb;

	pev->effects |= EF_LIGHT;

	// make rocket sound
	EMIT_SOUND( ENT(pev), CHAN_VOICE, "rocket1.wav", 1, 0.5 );

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(m_iTrail );	// model
		WRITE_BYTE( 40 ); // life
		WRITE_BYTE( 5 );  // width
		WRITE_BYTE( 200 );   // r, g, b
		WRITE_BYTE( 160 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );	// brightness
	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)
}

void CFlakBomb :: BombTouch ( CBaseEntity *pOther )
{
	pev->velocity = pev->velocity * 0.8;
	STOP_SOUND( edict(), CHAN_VOICE, "rocket1.wav" );
}

void CFlakBomb :: BlowUp() {
	for (int i = 0; i < 6; i++) {
		CFlak *flak = CFlak::CreateFlak( pev->origin, pev->angles, owner );
		flak->pev->velocity = Vector(RANDOM_LONG(-100, 100), RANDOM_LONG(-100, 100), RANDOM_LONG(-100, 100)) * RANDOM_LONG(5, 10);
	}
	pev->nextthink = -1;
	Detonate();
}

void CFlakBomb :: Precache( void )
{
	PRECACHE_MODEL("models/rpgrocket.mdl");
	m_iTrail = PRECACHE_MODEL("sprites/smoke.spr");
	PRECACHE_MODEL("sprites/xspark4.spr");
	PRECACHE_MODEL("sprites/ice_xspark4.spr");
	PRECACHE_SOUND ("rocket1.wav");
}

LINK_ENTITY_TO_CLASS( flak_bomb, CFlakBomb );

//=========================================================
//=========================================================

CFlak *CFlak::CreateFlak( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner )
{
	CFlak *pFlak = GetClassPtr( (CFlak *)NULL );

	UTIL_SetOrigin( pFlak->pev, vecOrigin );
	pFlak->pev->angles = vecAngles;
	pFlak->Spawn();
	pFlak->SetTouch( &CFlak::FlakTouch );
	if (pOwner != NULL)
	pFlak->pev->owner = pOwner->edict();

	return pFlak;
}

void CFlak :: Spawn( )
{
	Precache( );

	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/cindergibs.mdl");
	pev->body = RANDOM_FLOAT ( 0, 3 );
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );
	pev->avelocity.x = RANDOM_FLOAT ( -100, -500 );

	pev->classname = MAKE_STRING("flak");

	SetThink( &CBaseEntity::SUB_FadeOut );
	pev->nextthink = gpGlobals->time + 5.0;

	UTIL_MakeVectors( pev->angles );

	pev->velocity = gpGlobals->v_forward * RANDOM_LONG(1200, 1800);
	pev->gravity = 0.5;
	pev->dmg = gSkillData.plrDmgFlak;

	CSprite *glowing;
	if (icesprites.value)
		glowing = CSprite::SpriteCreate( "sprites/ice_xspark4.spr", pev->origin, TRUE );
	else
		glowing = CSprite::SpriteCreate( "sprites/xspark4.spr", pev->origin, TRUE );
	if (glowing != NULL)
	{
		glowing->SetTransparency( kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation );
		glowing->SetAttachment( edict(), 0 );
		glowing->SetScale( 0.5 );
		glowing->SetThink( &CBaseEntity::SUB_FadeOutFast );
		glowing->pev->nextthink = gpGlobals->time + 0.1;
	}

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT(entindex());	// entity
		WRITE_SHORT(PRECACHE_MODEL("sprites/smoke.spr"));	// model
		WRITE_BYTE( 7 ); // life
		WRITE_BYTE( 2 );  // width
		WRITE_BYTE( 200 );   // r, g, b
		WRITE_BYTE( 160 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 100 );	// brightness
	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)
}

void CFlak :: FlakTouch ( CBaseEntity *pOther )
{
	TraceResult tr = UTIL_GetGlobalTrace( );

	// it's not another flak
	if (tr.pHit->v.modelindex == pev->modelindex)
	{
		return;
	}

	pev->movetype = MOVETYPE_BOUNCE;
	pev->velocity = pev->velocity * 0.7;

	if (m_flNextAttack < gpGlobals->time ) {
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "debris/concrete2.wav", 1, 0.8 );

		if ( pOther->pev->takedamage )
		{
			entvars_t *pevOwner = VARS( pev->owner );
			if (pevOwner)
			{
				ClearMultiDamage();
				pOther->TraceAttack(pevOwner, pev->dmg, gpGlobals->v_forward, &tr, DMG_NEVERGIB );
				ApplyMultiDamage( pev, pevOwner );
				UTIL_Remove(this);
			}
		}

		m_flNextAttack = gpGlobals->time + 0.25;
	}
}

void CFlak :: Precache( void )
{
	PRECACHE_MODEL("models/cindergibs.mdl");
	PRECACHE_MODEL("sprites/xspark4.spr");
	PRECACHE_SOUND("debris/concrete2.wav");
}

LINK_ENTITY_TO_CLASS( flak, CFlak );

#endif

//=========================================================
//=========================================================

void CCannon::Precache( void )
{
	PRECACHE_MODEL("models/w_cannon.mdl");
	PRECACHE_MODEL("models/v_cannon.mdl");
	PRECACHE_MODEL("models/p_cannon.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");

	UTIL_PrecacheOther( "flak" );
	UTIL_PrecacheOther( "flak_bomb" );

	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("weapons/glauncher.wav"); // alternative fire sound
	PRECACHE_SOUND("cannon_fire.wav");

	m_usCannon = PRECACHE_EVENT ( 1, "events/cannon.sc" );
	m_usCannonFlak = PRECACHE_EVENT ( 1, "events/cannon_flak.sc" );
}

void CCannon::Spawn( )
{
	Precache( );
	m_iId = WEAPON_CANNON;

	SET_MODEL(ENT(pev), "models/w_cannon.mdl");

#ifdef CLIENT_DLL
	if ( bIsMultiplayer() )
#else
	if ( g_pGameRules->IsMultiplayer() )
#endif
	{
		// more default ammo in multiplay. 
		m_iDefaultAmmo = CANNON_DEFAULT_GIVE * 2;
	}
	else
	{
		m_iDefaultAmmo = CANNON_DEFAULT_GIVE;
	}

	FallInit();// get ready to fall down.
}

int CCannon::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "rockets";
	p->iMaxAmmo1 = ROCKET_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = CANNON_MAX_CLIP;
	p->iSlot = 3;
	p->iPosition = 6;
	p->iId = m_iId = WEAPON_CANNON;
	p->iFlags = 0;
	p->iWeight = CANNON_WEIGHT;
	p->pszDisplayName = "30-Pound Automatic Assault Cannon";

	return 1;
}

int CCannon::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

BOOL CCannon::DeployLowKey( )
{
	return DefaultDeploy( "models/v_cannon.mdl", "models/p_cannon.mdl", CANNON_DRAW_LOWKEY, "rpg" );
}

BOOL CCannon::Deploy( )
{
	return DefaultDeploy( "models/v_cannon.mdl", "models/p_cannon.mdl", CANNON_DRAW1, "rpg" );
}

BOOL CCannon::CanHolster( void )
{
	return TRUE;
}

void CCannon::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	SendWeaponAnim( CANNON_HOLSTER1 );
}

void CCannon::SecondaryAttack()
{
	if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

#ifndef CLIENT_DLL
		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 4 + gpGlobals->v_right * 8 + gpGlobals->v_up * -4;
		
		CFlakBomb::CreateFlakBomb( vecSrc, m_pPlayer->pev->v_angle, m_pPlayer );
#endif

		int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

		PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usCannon );

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.75);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5;
	}
	else
	{
		PlayEmptySound( );
	}
}

void CCannon::PrimaryAttack()
{
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

#ifndef CLIENT_DLL
		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc1 = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 4 + gpGlobals->v_right * RANDOM_LONG(-18, 18) + gpGlobals->v_up * RANDOM_LONG(-18, 18);
		CFlak::CreateFlak( vecSrc1, m_pPlayer->pev->v_angle, m_pPlayer );

		Vector vecSrc2 = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 4 + gpGlobals->v_right * RANDOM_LONG(-18, 18) + gpGlobals->v_up * RANDOM_LONG(-18, 18);
		CFlak::CreateFlak( vecSrc2, m_pPlayer->pev->v_angle, m_pPlayer );

		Vector vecSrc3 = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 4 + gpGlobals->v_right * RANDOM_LONG(-18, 18) + gpGlobals->v_up * RANDOM_LONG(-18, 18);
		CFlak::CreateFlak( vecSrc3, m_pPlayer->pev->v_angle, m_pPlayer );

		Vector vecSrc4 = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 4 + gpGlobals->v_right * RANDOM_LONG(-18, 18) + gpGlobals->v_up * RANDOM_LONG(-18, 18);
		CFlak::CreateFlak( vecSrc4, m_pPlayer->pev->v_angle, m_pPlayer );

		Vector vecSrc5 = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 4 + gpGlobals->v_right * RANDOM_LONG(-18, 18) + gpGlobals->v_up * RANDOM_LONG(-18, 18);
		CFlak::CreateFlak( vecSrc5, m_pPlayer->pev->v_angle, m_pPlayer );
#endif

		int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

		PLAYBACK_EVENT( flags, m_pPlayer->edict(), m_usCannonFlak );

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.75);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5;
	}
	else
	{
		PlayEmptySound( );
	}
}

void CCannon::WeaponIdle( void )
{
	ResetEmptySound( );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_pPlayer->pev->button & IN_IRONSIGHT )
		return;

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
		if (flRand <= 0.75)
		{
			iAnim = CANNON_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 90.0 / 15.0;
		}
		else
		{
			iAnim = CANNON_FIDGET;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3.0;
		}

		SendWeaponAnim( iAnim );
	}
	else
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1;
	}
}

//=========================================================
//=========================================================

class CCannonAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_rpgammo.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_rpgammo.mdl");
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
LINK_ENTITY_TO_CLASS( ammo_flak, CCannonAmmo );
