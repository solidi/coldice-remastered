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

static const int CANNON_SHOCKWAVE_AMMO_COST = 2;
static const float CANNON_SHOCKWAVE_RADIUS = 180.0f;
static const float CANNON_SHOCKWAVE_FORCE_MIN = 900.0f;
static const float CANNON_SHOCKWAVE_FORCE_MAX = 1450.0f;
static const float CANNON_SHOCKWAVE_UPLIFT_MIN = 150.0f;
static const float CANNON_SHOCKWAVE_UPLIFT_MAX = 280.0f;
static const float CANNON_SHOCKWAVE_DAMAGE = 6.0f;
static const float CANNON_SHOCKWAVE_COOLDOWN = 4.5f;
static const float CANNON_SHOCKWAVE_FAIL_COOLDOWN = 0.45f;

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

	SET_MODEL(ENT(pev), "models/w_items.mdl");
	pev->body = 8;
	pev->sequence = 9;
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );
	pev->avelocity.x = RANDOM_FLOAT ( -100, -500 );

	pev->classname = MAKE_STRING("flak_bomb");

	SetThink( &CFlakBomb::BlowUp );
	pev->nextthink = gpGlobals->time + 1.0;
	pev->velocity = pev->angles * RANDOM_LONG(700, 900);

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

	if ( pOther->IsAlive() && (pOther->pev->flags & (FL_CLIENT|FL_MONSTER)) )
	{
		SetThink( &CFlakBomb::BlowUp );
		pev->nextthink = gpGlobals->time;
	}
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
	m_iTrail = g_sModelIndexSmoke2;
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
	pev->velocity = pev->angles * RANDOM_LONG(1200, 1800);

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
		WRITE_SHORT(g_sModelIndexSmoke2);	// model
		WRITE_BYTE( 7 ); // life
		WRITE_BYTE( 2 );  // width
		WRITE_BYTE( 200 );   // r, g, b
		WRITE_BYTE( 160 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 100 );	// brightness
	MESSAGE_END();  // move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)
}

BOOL CFlak::ShouldCollide( CBaseEntity *pOther )
{
	if (pev->modelindex == pOther->pev->modelindex)
		return FALSE;

	return TRUE;
}

void CFlak :: FlakTouch ( CBaseEntity *pOther )
{
	TraceResult tr = UTIL_GetGlobalTrace( );

	// it's not another flak
	if (tr.pHit && tr.pHit->v.modelindex == pev->modelindex)
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

static BOOL CannonShockwaveHasLOS( CBasePlayer *pOwner, CBaseEntity *pTarget, const Vector &vecStart )
{
	if (!pOwner || !pTarget)
		return FALSE;

	TraceResult tr;
	UTIL_TraceLine( vecStart, pTarget->Center(), dont_ignore_monsters, ENT( pOwner->pev ), &tr );

	return (tr.flFraction >= 1.0f || tr.pHit == pTarget->edict());
}

static BOOL CannonShockwaveIsSwattableProjectile( CBaseEntity *pEntity )
{
	if (!pEntity || !pEntity->pev)
		return FALSE;

	const char *pszClassname = STRING( pEntity->pev->classname );
	if (!pszClassname)
		return FALSE;

	if (!strncmp( pszClassname, "weapon_", 7 ) ||
		!strncmp( pszClassname, "ammo_", 5 ) ||
		!strncmp( pszClassname, "item_", 5 ))
	{
		return FALSE;
	}

	if (FClassnameIs( pEntity->pev, "grenade" ) ||
		FClassnameIs( pEntity->pev, "freezegrenade" ) ||
		FClassnameIs( pEntity->pev, "rpg_rocket" ) ||
		FClassnameIs( pEntity->pev, "nuke_rocket" ) ||
		FClassnameIs( pEntity->pev, "drunk_rocket" ) ||
		FClassnameIs( pEntity->pev, "flak" ) ||
		FClassnameIs( pEntity->pev, "flak_bomb" ) ||
		FClassnameIs( pEntity->pev, "flameball" ) ||
		FClassnameIs( pEntity->pev, "snowbomb" ) ||
		FClassnameIs( pEntity->pev, "plasma" ) ||
		FClassnameIs( pEntity->pev, "monster_snark" ) ||
		FClassnameIs( pEntity->pev, "monster_chumtoad" ) ||
		FClassnameIs( pEntity->pev, "hornet" ) ||
		FClassnameIs( pEntity->pev, "disc" ) ||
		FClassnameIs( pEntity->pev, "kts_snowball" ) ||
		FClassnameIs( pEntity->pev, "flying_crowbar" ) ||
		FClassnameIs( pEntity->pev, "flying_wrench" ) ||
		FClassnameIs( pEntity->pev, "flying_knife" ))
	{
		return TRUE;
	}

	if ((pEntity->pev->movetype == MOVETYPE_BOUNCE ||
		 pEntity->pev->movetype == MOVETYPE_BOUNCEMISSILE ||
		 pEntity->pev->movetype == MOVETYPE_FLY ||
		 pEntity->pev->movetype == MOVETYPE_TOSS) &&
		pEntity->pev->solid != SOLID_NOT &&
		pEntity->pev->solid != SOLID_BSP)
	{
		return TRUE;
	}

	return FALSE;
}

static BOOL CannonShockwaveCanAffectTarget( CBasePlayer *pOwner, CBaseEntity *pTarget )
{
	if (!pOwner || !pTarget || !pTarget->pev || pTarget == pOwner)
		return FALSE;

	if (FClassnameIs( pTarget->pev, "worldspawn" ))
		return FALSE;

	if (pTarget->IsPlayer() || (pTarget->pev->flags & FL_MONSTER))
		return pTarget->IsAlive();

	return CannonShockwaveIsSwattableProjectile( pTarget );
}

static BOOL CannonShockwaveAffectTarget( CBasePlayer *pOwner, CBaseEntity *pTarget, const Vector &vecBurstOrigin, BOOL &bDidDamage )
{
	bDidDamage = FALSE;

	if (!CannonShockwaveCanAffectTarget( pOwner, pTarget ) || !CannonShockwaveHasLOS( pOwner, pTarget, vecBurstOrigin ))
		return FALSE;

	Vector vecToTarget = pTarget->Center() - vecBurstOrigin;
	const float flDistance = vecToTarget.Length();

	if (flDistance > CANNON_SHOCKWAVE_RADIUS)
		return FALSE;

	Vector vecLaunchDir = vecToTarget;
	vecLaunchDir.z = 0.0f;

	if (vecLaunchDir.Length() <= 0.01f)
	{
		vecLaunchDir = gpGlobals->v_forward;
		vecLaunchDir.z = 0.0f;
	}

	if (vecLaunchDir.Length() <= 0.01f)
		vecLaunchDir = Vector(1, 0, 0);
	else
		vecLaunchDir = vecLaunchDir.Normalize();

	float flScale = 1.0f - (flDistance / CANNON_SHOCKWAVE_RADIUS);
	if (flScale < 0.0f)
		flScale = 0.0f;
	if (flScale > 1.0f)
		flScale = 1.0f;

	const float flForce = CANNON_SHOCKWAVE_FORCE_MIN + (CANNON_SHOCKWAVE_FORCE_MAX - CANNON_SHOCKWAVE_FORCE_MIN) * flScale;
	const float flUplift = CANNON_SHOCKWAVE_UPLIFT_MIN + (CANNON_SHOCKWAVE_UPLIFT_MAX - CANNON_SHOCKWAVE_UPLIFT_MIN) * flScale;

	pTarget->pev->flags &= ~FL_ONGROUND;
	pTarget->pev->velocity = vecLaunchDir * flForce + pOwner->pev->velocity * 0.30f;
	pTarget->pev->velocity.z += flUplift;

	if ((pTarget->IsPlayer() || (pTarget->pev->flags & FL_MONSTER)) && pTarget->pev->takedamage != DAMAGE_NO)
	{
		pTarget->TakeDamage( pOwner->pev, pOwner->pev, CANNON_SHOCKWAVE_DAMAGE, DMG_SONIC | DMG_NEVERGIB );
		bDidDamage = TRUE;
	}

	return TRUE;
}

static void CannonShockwaveBurstFX( CBasePlayer *pOwner, int iBeamSprite, int iFlashSprite, const Vector &vecOrigin )
{
	if (!pOwner || (iBeamSprite <= 0 && iFlashSprite <= 0))
		return;

	if (iBeamSprite > 0)
	{
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMDISK );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z + CANNON_SHOCKWAVE_RADIUS );
			WRITE_SHORT( iBeamSprite );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 10 );
			WRITE_BYTE( 34 );
			WRITE_BYTE( 0 );
			if ( icesprites.value )
			{
				WRITE_BYTE( 0 );
				WRITE_BYTE( 113 );
				WRITE_BYTE( 230 );
			}
			else
			{
				WRITE_BYTE( 255 );
				WRITE_BYTE( 180 );
				WRITE_BYTE( 48 );
			}
			WRITE_BYTE( 255 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMCYLINDER );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z + 8 );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z + CANNON_SHOCKWAVE_RADIUS );
			WRITE_SHORT( iBeamSprite );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 10 );
			WRITE_BYTE( 24 );
			WRITE_BYTE( 0 );
			if ( icesprites.value )
			{
				WRITE_BYTE( 0 );
				WRITE_BYTE( 113 );
				WRITE_BYTE( 230 );
			}
			else
			{
				WRITE_BYTE( 255 );
				WRITE_BYTE( 210 );
				WRITE_BYTE( 72 );
			}
			WRITE_BYTE( 240 );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	if (iFlashSprite > 0)
	{
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_SPRITE );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z + 12 );
			WRITE_SHORT( iFlashSprite );
			WRITE_BYTE( 22 );
			WRITE_BYTE( 180 );
		MESSAGE_END();
	}

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_DLIGHT );
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z + 8 );
		WRITE_BYTE( 42 );
		if ( icesprites.value )
		{
			WRITE_BYTE( 0 );
			WRITE_BYTE( 113 );
			WRITE_BYTE( 230 );
		}
		else
		{
			WRITE_BYTE( 255 );
			WRITE_BYTE( 180 );
			WRITE_BYTE( 48 );
		}
		WRITE_BYTE( 6 );
		WRITE_BYTE( 30 );
	MESSAGE_END();
}

#endif

//=========================================================
//=========================================================

void CCannon::Precache( void )
{
	PRECACHE_MODEL("models/v_cannon.mdl");
	m_iShockwaveBurstSprite = PRECACHE_MODEL("sprites/lgtning.spr");
	m_iShockwaveFlashSprite = PRECACHE_MODEL("sprites/glowbig.spr");

	PRECACHE_SOUND("items/9mmclip1.wav");

	UTIL_PrecacheOther( "flak" );
	UTIL_PrecacheOther( "flak_bomb" );

	PRECACHE_SOUND("weapons/rocketfire1.wav");
	PRECACHE_SOUND("weapons/glauncher.wav"); // alternative fire sound
	PRECACHE_SOUND("cannon_fire.wav");
	PRECACHE_SOUND("common/wpn_denyselect.wav");

	m_usCannon = PRECACHE_EVENT ( 1, "events/cannon.sc" );
	m_usCannonFlak = PRECACHE_EVENT ( 1, "events/cannon_flak.sc" );
}

void CCannon::Spawn( )
{
	m_iShockwaveBurstSprite = 0;
	m_iShockwaveFlashSprite = 0;
	m_flNextShockwaveTime = 0;

	Precache( );
	m_iId = WEAPON_CANNON;

	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_CANNON - 1;

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
	pev->dmg = gSkillData.plrDmgFlak;

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
	p->iFlags = ITEM_FLAG_SINGLE_HAND;
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
	return DefaultDeploy( "models/v_cannon.mdl", "models/p_weapons.mdl", CANNON_DRAW_LOWKEY, "rpg" );
}

BOOL CCannon::Deploy( )
{
	return DefaultDeploy( "models/v_cannon.mdl", "models/p_weapons.mdl", CANNON_DRAW1, "rpg" );
}

void CCannon::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(CANNON_HOLSTER1);
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

		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
		Vector vecSrc = m_pPlayer->GetGunPosition( ) + vecAiming * 4 + gpGlobals->v_right * 8 + gpGlobals->v_up * -4;
		CFlakBomb::CreateFlakBomb( vecSrc, vecAiming, m_pPlayer );
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

		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
		Vector vecSrc1 = m_pPlayer->GetGunPosition( ) + vecAiming * 4 + gpGlobals->v_right * RANDOM_LONG(-18, 18) + gpGlobals->v_up * RANDOM_LONG(-18, 18);
		CFlak::CreateFlak( vecSrc1, vecAiming, m_pPlayer );

		Vector vecSrc2 = m_pPlayer->GetGunPosition( ) + vecAiming * 4 + gpGlobals->v_right * RANDOM_LONG(-18, 18) + gpGlobals->v_up * RANDOM_LONG(-18, 18);
		CFlak::CreateFlak( vecSrc2, vecAiming, m_pPlayer );

		Vector vecSrc3 = m_pPlayer->GetGunPosition( ) + vecAiming * 4 + gpGlobals->v_right * RANDOM_LONG(-18, 18) + gpGlobals->v_up * RANDOM_LONG(-18, 18);
		CFlak::CreateFlak( vecSrc3, vecAiming, m_pPlayer );

		Vector vecSrc4 = m_pPlayer->GetGunPosition( ) + vecAiming * 4 + gpGlobals->v_right * RANDOM_LONG(-18, 18) + gpGlobals->v_up * RANDOM_LONG(-18, 18);
		CFlak::CreateFlak( vecSrc4, vecAiming, m_pPlayer );

		Vector vecSrc5 = m_pPlayer->GetGunPosition( ) + vecAiming * 4 + gpGlobals->v_right * RANDOM_LONG(-18, 18) + gpGlobals->v_up * RANDOM_LONG(-18, 18);
		CFlak::CreateFlak( vecSrc5, vecAiming, m_pPlayer );
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

void CCannon::Reload()
{
	if (!m_pPlayer)
		return;

	if (!(m_pPlayer->m_afButtonPressed & IN_RELOAD))
		return;

	if (gpGlobals->time < m_flNextShockwaveTime)
		return;

	const float flWeaponMultiplier = (g_pGameRules ? g_pGameRules->WeaponMultipler() : 1.0f);
	const float flFailCooldown = CANNON_SHOCKWAVE_FAIL_COOLDOWN * flWeaponMultiplier;
	const float flCooldown = CANNON_SHOCKWAVE_COOLDOWN * flWeaponMultiplier;

	if (m_pPlayer->pev->waterlevel == 3)
	{
#ifndef CLIENT_DLL
		EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_ITEM, "common/wpn_denyselect.wav", 0.8f, ATTN_NORM );
#endif
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + flFailCooldown;
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flFailCooldown);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flFailCooldown;
		m_flNextShockwaveTime = gpGlobals->time + flFailCooldown;
		return;
	}

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < CANNON_SHOCKWAVE_AMMO_COST)
	{
		PlayEmptySound();
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + flFailCooldown;
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flFailCooldown);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + flFailCooldown;
		m_flNextShockwaveTime = gpGlobals->time + flFailCooldown;
		return;
	}

	SendWeaponAnim( CANNON_SPINUP );

#ifdef CLIENT_DLL
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + flCooldown;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCooldown);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.65f;
	m_flNextShockwaveTime = gpGlobals->time + flCooldown;
	return;
#else
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	const Vector vecBurstOrigin = m_pPlayer->pev->origin + gpGlobals->v_up * 36.0f;

	int iTargetsPushed = 0;
	CBaseEntity *pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere( pEntity, vecBurstOrigin, CANNON_SHOCKWAVE_RADIUS )) != NULL)
	{
		BOOL bDidDamage = FALSE;
		if (CannonShockwaveAffectTarget( m_pPlayer, pEntity, vecBurstOrigin, bDidDamage ))
			iTargetsPushed++;
	}

	m_pPlayer->ExpireSpawnProtection();
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= CANNON_SHOCKWAVE_AMMO_COST;
	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/rocketfire1.wav", 1.0f, ATTN_NORM );
	CannonShockwaveBurstFX( m_pPlayer, m_iShockwaveBurstSprite, m_iShockwaveFlashSprite, vecBurstOrigin );

	if (iTargetsPushed > 0)
	{
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	}

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + flCooldown;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCooldown);
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.65f;
	m_flNextShockwaveTime = gpGlobals->time + flCooldown;
#endif
}

void CCannon::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

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
