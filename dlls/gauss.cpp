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
#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "shake.h"
#include "gamerules.h"
#include "game.h"


#define	GAUSS_PRIMARY_CHARGE_VOLUME	256// how loud gauss is while charging
#define GAUSS_PRIMARY_FIRE_VOLUME	450// how loud gauss is when discharged

#define GAUSS_EMP_STATE_SPINUP		4
#define GAUSS_EMP_STATE_CHARGING	5
#define GAUSS_EMP_MIN_RADIUS		96.0f
#define GAUSS_EMP_MAX_RADIUS		512.0f
#define GAUSS_EMP_START_COST		5
#define GAUSS_EMP_AMMO_BURN_INTERVAL_MP	0.10f
#define GAUSS_EMP_AMMO_BURN_INTERVAL_SP	0.30f
#define GAUSS_EMP_RELEASE_COOLDOWN	0.65f
#define GAUSS_EMP_FAIL_COOLDOWN	0.35f

enum gauss_e {
	GAUSS_IDLE = 0,
	GAUSS_IDLE2,
	GAUSS_FIDGET,
	GAUSS_SPINUP,
	GAUSS_SPIN,
	GAUSS_FIRE,
	GAUSS_FIRE2,
	GAUSS_HOLSTER,
	GAUSS_DRAW_LOWKEY,
	GAUSS_DRAW
};

static float GaussEmpChargeFrac( float flChargeDuration, float flChargeTimeToMax )
{
	if (flChargeTimeToMax <= 0.01f)
		return 1.0f;

	float flChargeFrac = flChargeDuration / flChargeTimeToMax;
	if (flChargeFrac < 0.0f)
		flChargeFrac = 0.0f;
	if (flChargeFrac > 1.0f)
		flChargeFrac = 1.0f;

	return flChargeFrac;
}

static float GaussEmpRadiusForCharge( float flChargeDuration, float flChargeTimeToMax )
{
	float flRadius = GAUSS_EMP_MIN_RADIUS + ((GAUSS_EMP_MAX_RADIUS - GAUSS_EMP_MIN_RADIUS) * GaussEmpChargeFrac( flChargeDuration, flChargeTimeToMax ));
	if (flRadius < GAUSS_EMP_MIN_RADIUS)
		flRadius = GAUSS_EMP_MIN_RADIUS;
	else if (flRadius > GAUSS_EMP_MAX_RADIUS)
		flRadius = GAUSS_EMP_MAX_RADIUS;

	return flRadius;
}

#ifndef CLIENT_DLL
static BOOL GaussEmpIsGenericExplosive( CBaseEntity *pEntity )
{
	if (!pEntity)
		return FALSE;

	if (FClassnameIs( pEntity->pev, "grenade" ) ||
		FClassnameIs( pEntity->pev, "freezegrenade" ) ||
		FClassnameIs( pEntity->pev, "rpg_rocket" ) ||
		FClassnameIs( pEntity->pev, "nuke_rocket" ) ||
		FClassnameIs( pEntity->pev, "drunk_rocket" ) ||
		FClassnameIs( pEntity->pev, "flak" ) ||
		FClassnameIs( pEntity->pev, "flak_bomb" ) ||
		FClassnameIs( pEntity->pev, "flameball" ) ||
		FClassnameIs( pEntity->pev, "napalm_pool" ) ||
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

	return FALSE;
}

static void GaussEmpPulseFx( CBasePlayer *pPlayer, float flRadius, int iWarmPulseSprite, int iIcePulseSprite )
{
	if (!pPlayer)
		return;

	const float flRingRadius = (flRadius < GAUSS_EMP_MIN_RADIUS) ? GAUSS_EMP_MIN_RADIUS : ((flRadius > GAUSS_EMP_MAX_RADIUS) ? GAUSS_EMP_MAX_RADIUS : flRadius);

	const BOOL bIce = (icesprites.value != 0.0f);
	const int iSprite = bIce ? iIcePulseSprite : iWarmPulseSprite;
	const int iColorR = bIce ? 0 : 255;
	const int iColorG = bIce ? 113 : 160;
	const int iColorB = bIce ? 230 : 100;

	Vector vecCenter = pPlayer->pev->origin;
	vecCenter.z += 32;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecCenter );
		WRITE_BYTE( TE_BEAMDISK );
		WRITE_COORD( vecCenter.x );
		WRITE_COORD( vecCenter.y );
		WRITE_COORD( vecCenter.z );
		WRITE_COORD( vecCenter.x );
		WRITE_COORD( vecCenter.y );
		WRITE_COORD( vecCenter.z + flRingRadius );
		WRITE_SHORT( g_sModelLightning );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 10 );
		WRITE_BYTE( 52 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( iColorR );
		WRITE_BYTE( iColorG );
		WRITE_BYTE( iColorB );
		WRITE_BYTE( 255 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecCenter );
		WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( vecCenter.x );
		WRITE_COORD( vecCenter.y );
		WRITE_COORD( vecCenter.z );
		WRITE_COORD( vecCenter.x );
		WRITE_COORD( vecCenter.y );
		WRITE_COORD( vecCenter.z + flRingRadius );
		WRITE_SHORT( g_sModelLightning );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 8 );
		WRITE_BYTE( 28 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( iColorR );
		WRITE_BYTE( iColorG );
		WRITE_BYTE( iColorB );
		WRITE_BYTE( 210 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	if (iSprite > 0)
	{
		int iScale = (int)(flRingRadius * 0.08f);
		if (iScale < 16)
			iScale = 16;
		if (iScale > 64)
			iScale = 64;

		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecCenter );
			WRITE_BYTE( TE_SPRITE );
			WRITE_COORD( vecCenter.x );
			WRITE_COORD( vecCenter.y );
			WRITE_COORD( vecCenter.z );
			WRITE_SHORT( iSprite );
			WRITE_BYTE( iScale );
			WRITE_BYTE( 220 );
		MESSAGE_END();
	}
}

static int GaussEmpDisruptEntities( CBasePlayer *pPlayer, float flRadius )
{
	if (!pPlayer)
		return 0;

	int iDisrupted = 0;
	BOOL portalOwnerProcessed[33];
	for (int i = 0; i < 33; ++i)
		portalOwnerProcessed[i] = FALSE;

	CBaseEntity *pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere( pEntity, pPlayer->pev->origin, flRadius )) != NULL)
	{
		if (pEntity == pPlayer)
			continue;

		if (FClassnameIs( pEntity->pev, "monster_proxmine" ))
		{
			CProxMine *pProxMine = (CProxMine *)pEntity;
			pProxMine->KillIndicator();
			STOP_SOUND( ENT(pProxMine->pev), CHAN_VOICE, "weapons/mine_deploy.wav" );
			STOP_SOUND( ENT(pProxMine->pev), CHAN_BODY, "weapons/mine_charge.wav" );
			pProxMine->pev->solid = SOLID_NOT;
			UTIL_Remove( pProxMine );
			iDisrupted++;
			continue;
		}

		if (FClassnameIs( pEntity->pev, "monster_tripmine" ))
		{
			CTripmineGrenade *pTripmine = (CTripmineGrenade *)pEntity;
			pTripmine->KillBeam();
			STOP_SOUND( ENT(pTripmine->pev), CHAN_VOICE, "weapons/mine_deploy.wav" );
			STOP_SOUND( ENT(pTripmine->pev), CHAN_BODY, "weapons/mine_charge.wav" );
			pTripmine->pev->solid = SOLID_NOT;
			UTIL_Remove( pTripmine );
			iDisrupted++;
			continue;
		}

		if (FClassnameIs( pEntity->pev, "monster_satchel" ))
		{
			pEntity->pev->solid = SOLID_NOT;
			UTIL_Remove( pEntity );
			iDisrupted++;
			continue;
		}

		if (FClassnameIs( pEntity->pev, "ent_portal" ))
		{
			CBaseEntity *pOwnerEnt = CBaseEntity::Instance( pEntity->pev->owner );
			if (pOwnerEnt && pOwnerEnt->IsPlayer())
			{
				int iOwnerIndex = ENTINDEX( pOwnerEnt->edict() );
				if (iOwnerIndex >= 1 && iOwnerIndex <= 32)
				{
					if (!portalOwnerProcessed[iOwnerIndex])
					{
						DeactivatePortals( (CBasePlayer *)pOwnerEnt );
						portalOwnerProcessed[iOwnerIndex] = TRUE;
						iDisrupted++;
					}
				}
				else
				{
					pEntity->pev->solid = SOLID_NOT;
					UTIL_Remove( pEntity );
					iDisrupted++;
				}
			}
			else
			{
				pEntity->pev->solid = SOLID_NOT;
				UTIL_Remove( pEntity );
				iDisrupted++;
			}
			continue;
		}

		if (GaussEmpIsGenericExplosive( pEntity ))
		{
			pEntity->pev->solid = SOLID_NOT;
			UTIL_Remove( pEntity );
			iDisrupted++;
		}
	}

	for (int iClient = 1; iClient <= gpGlobals->maxClients; ++iClient)
	{
		CBasePlayer *pTarget = (CBasePlayer *)UTIL_PlayerByIndex( iClient );
		if (!pTarget || !pTarget->IsPlayer() || !pTarget->IsAlive())
			continue;

		if ((pTarget->pev->origin - pPlayer->pev->origin).Length() > flRadius)
			continue;

		CBasePlayerItem *pActiveItem = pTarget->m_pActiveItem;
		if (!pActiveItem || pActiveItem->m_iId != WEAPON_VEST)
			continue;

		CVest *pVest = (CVest *)pActiveItem;
		if (pVest->DisarmForEMP())
			iDisrupted++;
	}

	return iDisrupted;
}
#endif

#ifdef GAUSS
LINK_ENTITY_TO_CLASS( weapon_gauss, CGauss );
#endif

float CGauss::GetFullChargeTime( void )
{
#ifdef CLIENT_DLL
	if ( bIsMultiplayer() )
#else
	if ( g_pGameRules->IsMultiplayer() )
#endif
	{
		return 1.5;
	}

	return 4;
}

#ifdef CLIENT_DLL
extern int g_irunninggausspred;
#endif

void CGauss::Spawn( )
{
	Precache( );
	m_iId = WEAPON_GAUSS;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_GAUSS - 1;

	m_iDefaultAmmo = GAUSS_DEFAULT_GIVE;
	pev->dmg = gSkillData.plrDmgGauss;
	m_fInAttack = 0;
	m_flStartThrow = 0;
	m_flReleaseThrow = -1;

	FallInit();// get ready to fall down.
}


void CGauss::Precache( void )
{
	PRECACHE_MODEL("models/v_gauss.mdl");

	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND("weapons/gauss2.wav");
	PRECACHE_SOUND("weapons/electro4.wav");
	PRECACHE_SOUND("weapons/electro5.wav");
	PRECACHE_SOUND("weapons/electro6.wav");
	PRECACHE_SOUND("ambience/pulsemachine.wav");
	
	m_iEmpPulseSprite = PRECACHE_MODEL( "sprites/hotglow.spr" );
	m_iEmpPulseIceSprite = PRECACHE_MODEL( "sprites/ice_hotglow.spr" );
	m_iBeam = g_sModelIndexSmoke2;

	m_usGaussFire = PRECACHE_EVENT( 1, "events/gauss.sc" );
	m_usGaussSpin = PRECACHE_EVENT( 1, "events/gaussspin.sc" );
}

int CGauss::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CGauss::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_GAUSS;
	p->iFlags = ITEM_FLAG_SINGLE_HAND;
	p->iWeight = GAUSS_WEIGHT;
	p->pszDisplayName = "Gauss";

	return 1;
}

BOOL CGauss::DeployLowKey( )
{
	m_pPlayer->m_flPlayAftershock = 0.0;
	m_flStartThrow = 0;
	return DefaultDeploy( "models/v_gauss.mdl", "models/p_weapons.mdl", GAUSS_DRAW_LOWKEY, "gauss" );
}

BOOL CGauss::Deploy( )
{
	m_pPlayer->m_flPlayAftershock = 0.0;
	m_flStartThrow = 0;
	return DefaultDeploy( "models/v_gauss.mdl", "models/p_weapons.mdl", GAUSS_DRAW, "gauss" );
}

void CGauss::Holster( int skiplocal /* = 0 */ )
{
	PLAYBACK_EVENT_FULL( FEV_RELIABLE | FEV_GLOBAL, m_pPlayer->edict(), m_usGaussFire, 0.01, (float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles, 0.0, 0.0, 0, 0, 0, 1 );
	CBasePlayerWeapon::DefaultHolster(GAUSS_HOLSTER);
	m_fInAttack = 0;
	m_flStartThrow = 0;
}


void CGauss::PrimaryAttack()
{
	if (m_fInAttack >= GAUSS_EMP_STATE_SPINUP)
	{
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.1f);
		return;
	}

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound( );
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] < 2 )
	{
		PlayEmptySound( );
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
		return;
	}

	m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
	m_fPrimaryFire = TRUE;

	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 2;

	StartFire();
	m_fInAttack = 0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.2;
}

void CGauss::SecondaryAttack()
{
	if (m_fInAttack >= GAUSS_EMP_STATE_SPINUP)
	{
		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.1f);
		return;
	}

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_fInAttack != 0 )
		{
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/electro4.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f));
			SendWeaponAnim( GAUSS_IDLE );
			m_fInAttack = 0;
		}
		else
		{
			PlayEmptySound( );
		}

		m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		return;
	}

	if ( m_fInAttack == 0 )
	{
		if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		{
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM);
			m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
			return;
		}

		m_fPrimaryFire = FALSE;

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;// take one ammo just to start the spin
		m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase();

		// spin up
		m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_CHARGE_VOLUME;
		
		SendWeaponAnim( GAUSS_SPINUP );
		m_fInAttack = 1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
		m_pPlayer->m_flStartCharge = gpGlobals->time;
		m_pPlayer->m_flAmmoStartCharge = UTIL_WeaponTimeBase() + GetFullChargeTime();

		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usGaussSpin, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 110, 0, 0, 0 );

		m_iSoundState = SND_CHANGE_PITCH;
	}
	else if (m_fInAttack == 1)
	{
		if (m_flTimeWeaponIdle < UTIL_WeaponTimeBase())
		{
			SendWeaponAnim( GAUSS_SPIN );
			m_fInAttack = 2;
		}
	}
	else
	{
		// during the charging process, eat one bit of ammo every once in a while
		if ( UTIL_WeaponTimeBase() >= m_pPlayer->m_flNextAmmoBurn && m_pPlayer->m_flNextAmmoBurn != 1000 )
		{
#ifdef CLIENT_DLL
	if ( bIsMultiplayer() )
#else
	if ( g_pGameRules->IsMultiplayer() )
#endif
			{
				m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
				m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + 0.1;
			}
			else
			{
				m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
				m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + 0.3;
			}
		}

		if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		{
			// out of ammo! force the gun to fire
			StartFire();
			m_fInAttack = 0;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
			m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1;
			return;
		}
		
		if ( UTIL_WeaponTimeBase() >= m_pPlayer->m_flAmmoStartCharge )
		{
			// don't eat any more ammo after gun is fully charged.
			m_pPlayer->m_flNextAmmoBurn = 1000;
		}

		int pitch = ( gpGlobals->time - m_pPlayer->m_flStartCharge ) * ( 150 / GetFullChargeTime() ) + 100;
		if ( pitch > 250 ) 
			 pitch = 250;
		
		// ALERT( at_console, "%d %d %d\n", m_fInAttack, m_iSoundState, pitch );

		if ( m_iSoundState == 0 )
			ALERT( at_console, "sound state %d\n", m_iSoundState );

		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usGaussSpin, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, pitch, 0, ( m_iSoundState == SND_CHANGE_PITCH ) ? 1 : 0, 0 );

		m_iSoundState = SND_CHANGE_PITCH; // hack for going through level transitions

		m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_CHARGE_VOLUME;
		
		// m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;
		if ( m_pPlayer->m_flStartCharge < gpGlobals->time - 10 )
		{
			// Player charged up too long. Zap him.
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/electro4.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f));
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM,   "weapons/electro6.wav", 1.0, ATTN_NORM, 0, 75 + RANDOM_LONG(0,0x3f));
			
			m_fInAttack = 0;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
			m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
				
#ifndef CLIENT_DLL
			m_pPlayer->TakeDamage( VARS(eoNullEntity), VARS(eoNullEntity), 50, DMG_SHOCK );
			UTIL_ScreenFade( m_pPlayer, Vector(255,128,0), 2, 0.5, 128, FFADE_IN );
#endif
			SendWeaponAnim( GAUSS_IDLE );
			
			// Player may have been killed and this weapon dropped, don't execute any more code after this!
			return;
		}
	}
}

void CGauss::Reload( void )
{
	if (m_fInAttack > 0 && m_fInAttack < GAUSS_EMP_STATE_SPINUP)
		return;

#ifdef CLIENT_DLL
	const float flEmpBurnInterval = bIsMultiplayer() ? GAUSS_EMP_AMMO_BURN_INTERVAL_MP : GAUSS_EMP_AMMO_BURN_INTERVAL_SP;
#else
	const float flEmpBurnInterval = g_pGameRules->IsMultiplayer() ? GAUSS_EMP_AMMO_BURN_INTERVAL_MP : GAUSS_EMP_AMMO_BURN_INTERVAL_SP;
#endif

	if (m_pPlayer->pev->waterlevel == 3)
	{
		if (m_fInAttack >= GAUSS_EMP_STATE_SPINUP)
		{
			EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/electro4.wav", 1.0, ATTN_NORM, 0, 80 + RANDOM_LONG(0,0x3f) );
			SendWeaponAnim( GAUSS_IDLE );
			m_fInAttack = 0;
			m_flStartThrow = 0;
		}
		else
		{
			PlayEmptySound();
		}

		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + GAUSS_EMP_FAIL_COOLDOWN;
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( GAUSS_EMP_FAIL_COOLDOWN );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + GAUSS_EMP_FAIL_COOLDOWN;
		return;
	}

	if (m_fInAttack == 0)
	{
		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < GAUSS_EMP_START_COST)
		{
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM);
			m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + GAUSS_EMP_FAIL_COOLDOWN;
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( GAUSS_EMP_FAIL_COOLDOWN );
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + GAUSS_EMP_FAIL_COOLDOWN;
			return;
		}

		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= GAUSS_EMP_START_COST;
		m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + flEmpBurnInterval;

		m_fPrimaryFire = FALSE;
		m_fInAttack = GAUSS_EMP_STATE_SPINUP;
		m_flStartThrow = gpGlobals->time;

		m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_CHARGE_VOLUME;

		SendWeaponAnim( GAUSS_SPINUP );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5f;

		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usGaussSpin, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 110, 0, 0, 0 );

		m_iSoundState = SND_CHANGE_PITCH;
	}
	else if (m_fInAttack == GAUSS_EMP_STATE_SPINUP)
	{
		if (m_flTimeWeaponIdle < UTIL_WeaponTimeBase())
		{
			SendWeaponAnim( GAUSS_SPIN );
			m_fInAttack = GAUSS_EMP_STATE_CHARGING;
		}
	}

	if (m_fInAttack >= GAUSS_EMP_STATE_SPINUP)
	{
		if (UTIL_WeaponTimeBase() >= m_pPlayer->m_flNextAmmoBurn)
		{
			if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
			{
				m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
			}
			m_pPlayer->m_flNextAmmoBurn = UTIL_WeaponTimeBase() + flEmpBurnInterval;
		}

		float flChargeDuration = gpGlobals->time - m_flStartThrow;
		float flChargeFrac = GaussEmpChargeFrac( flChargeDuration, GetFullChargeTime() );

		int pitch = (int)(100.0f + (150.0f * flChargeFrac));
		if (pitch > 250)
			pitch = 250;

		PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usGaussSpin, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, pitch, 0, ( m_iSoundState == SND_CHANGE_PITCH ) ? 1 : 0, 0 );

		m_iSoundState = SND_CHANGE_PITCH;
		m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_CHARGE_VOLUME;

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1f;
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.05f;
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.05f);
	}
}

//=========================================================
// StartFire- since all of this code has to run and then 
// call Fire(), it was easier at this point to rip it out 
// of weaponidle() and make its own function then to try to
// merge this into Fire(), which has some identical variable names 
//=========================================================
void CGauss::StartFire( void )
{
	float flDamage;

	Vector vecSrc = m_pPlayer->GetGunPosition( ); // + gpGlobals->v_up * -8 + gpGlobals->v_right * 8;
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( gpGlobals->time - m_pPlayer->m_flStartCharge > GetFullChargeTime() )
	{
		flDamage = 200;
	}
	else
	{
		flDamage = 200 * (( gpGlobals->time - m_pPlayer->m_flStartCharge) / GetFullChargeTime() );
	}

	if ( m_fPrimaryFire )
	{
		// fixed damage on primary attack
#ifdef CLIENT_DLL
		flDamage = 20;
#else 
		flDamage = gSkillData.plrDmgGauss;
#endif
	}

	if (m_fInAttack != 3)
	{
		//ALERT ( at_console, "Time:%f Damage:%f\n", gpGlobals->time - m_pPlayer->m_flStartCharge, flDamage );

#ifndef CLIENT_DLL
		float flZVel = m_pPlayer->pev->velocity.z;

		if ( !m_fPrimaryFire )
		{
			m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * flDamage * 5;
		}

		if ( !g_pGameRules->IsMultiplayer() )

		{
			// in deathmatch, gauss can pop you up into the air. Not in single play.
			m_pPlayer->pev->velocity.z = flZVel;
		}
#endif
		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	}

	// time until aftershock 'static discharge' sound
	m_pPlayer->m_flPlayAftershock = gpGlobals->time + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.3, 0.8 );

	Fire( vecSrc, vecAiming, flDamage );
}

void CGauss::Fire( Vector vecOrigSrc, Vector vecDir, float flDamage )
{
	m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;

	Vector vecSrc = vecOrigSrc;
	Vector vecDest = vecSrc + vecDir * 8192;
	edict_t		*pentIgnore;
	TraceResult tr, beam_tr;
	float flMaxFrac = 1.0;
	int	nTotal = 0;
	int fHasPunched = 0;
	int fFirstBeam = 1;
	int	nMaxHits = 10;

	pentIgnore = ENT( m_pPlayer->pev );

#ifdef CLIENT_DLL
	if ( m_fPrimaryFire == false )
		 g_irunninggausspred = true;
#endif
	
	// The main firing event is sent unreliably so it won't be delayed.
	PLAYBACK_EVENT_FULL( FEV_NOTHOST, m_pPlayer->edict(), m_usGaussFire, 0.0, (float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles, flDamage, 0.0, 0, 0, m_fPrimaryFire ? 1 : 0, 0 );

	// This reliable event is used to stop the spinning sound
	// It's delayed by a fraction of second to make sure it is delayed by 1 frame on the client
	// It's sent reliably anyway, which could lead to other delays

	PLAYBACK_EVENT_FULL( FEV_NOTHOST | FEV_RELIABLE, m_pPlayer->edict(), m_usGaussFire, 0.01, (float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles, 0.0, 0.0, 0, 0, 0, 1 );

	
	/*ALERT( at_console, "%f %f %f\n%f %f %f\n", 
		vecSrc.x, vecSrc.y, vecSrc.z, 
		vecDest.x, vecDest.y, vecDest.z );*/
	

//	ALERT( at_console, "%f %f\n", tr.flFraction, flMaxFrac );

#ifndef CLIENT_DLL
	while (flDamage > 10 && nMaxHits > 0)
	{
		nMaxHits--;

		// ALERT( at_console, "." );
		UTIL_TraceLine(vecSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr);

		if (tr.fAllSolid)
			break;

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		if (pEntity == NULL)
			break;

		if ( fFirstBeam )
		{
			m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
			fFirstBeam = 0;
	
			nTotal += 26;
		}
		
		if (pEntity->pev->takedamage)
		{
			ClearMultiDamage();
			pEntity->TraceAttack( m_pPlayer->pev, flDamage, vecDir, &tr, DMG_BULLET );
			ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);
		}

		if ( pEntity->ReflectGauss() )
		{
			float n;

			pentIgnore = NULL;

			n = -DotProduct(tr.vecPlaneNormal, vecDir);

			if (n < 0.5) // 60 degrees
			{
				// ALERT( at_console, "reflect %f\n", n );
				// reflect
				Vector r;
			
				r = 2.0 * tr.vecPlaneNormal * n + vecDir;
				flMaxFrac = flMaxFrac - tr.flFraction;
				vecDir = r;
				vecSrc = tr.vecEndPos + vecDir * 8;
				vecDest = vecSrc + vecDir * 8192;

				// explode a bit
				m_pPlayer->RadiusDamage( tr.vecEndPos, pev, m_pPlayer->pev, flDamage * n, CLASS_NONE, DMG_BLAST );

				nTotal += 34;
				
				// lose energy
				if (n == 0) n = 0.1;
				flDamage = flDamage * (1 - n);
			}
			else
			{
				nTotal += 13;

				// limit it to one hole punch
				if (fHasPunched)
					break;
				fHasPunched = 1;

				// try punching through wall if secondary attack (primary is incapable of breaking through)
				if ( !m_fPrimaryFire )
				{
					UTIL_TraceLine( tr.vecEndPos + vecDir * 8, vecDest, dont_ignore_monsters, pentIgnore, &beam_tr);
					if (!beam_tr.fAllSolid)
					{
						// trace backwards to find exit point
						UTIL_TraceLine( beam_tr.vecEndPos, tr.vecEndPos, dont_ignore_monsters, pentIgnore, &beam_tr);

						float n = (beam_tr.vecEndPos - tr.vecEndPos).Length( );

						if (n < flDamage)
						{
							if (n == 0) n = 1;
							flDamage -= n;

							// ALERT( at_console, "punch %f\n", n );
							nTotal += 21;

							// exit blast damage
							//m_pPlayer->RadiusDamage( beam_tr.vecEndPos + vecDir * 8, pev, m_pPlayer->pev, flDamage, CLASS_NONE, DMG_BLAST );
							float damage_radius;
							

							if ( g_pGameRules->IsMultiplayer() )
							{
								damage_radius = flDamage * 1.75;  // Old code == 2.5
							}
							else
							{
								damage_radius = flDamage * 2.5;
							}

							::RadiusDamage( beam_tr.vecEndPos + vecDir * 8, pev, m_pPlayer->pev, flDamage, damage_radius, CLASS_NONE, DMG_BLAST );

							CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );

							nTotal += 53;

							vecSrc = beam_tr.vecEndPos + vecDir;
						}
					}
					else
					{
						 //ALERT( at_console, "blocked %f\n", n );
						flDamage = 0;
					}
				}
				else
				{
					//ALERT( at_console, "blocked solid\n" );
					
					flDamage = 0;
				}

			}
		}
		else
		{
			vecSrc = tr.vecEndPos + vecDir;
			pentIgnore = ENT( pEntity->pev );
		}
	}
#endif
	// ALERT( at_console, "%d bytes\n", nTotal );
}




void CGauss::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	ResetEmptySound( );

	// play aftershock static discharge
	if ( m_pPlayer->m_flPlayAftershock && m_pPlayer->m_flPlayAftershock < gpGlobals->time )
	{
		switch (RANDOM_LONG(0,3))
		{
		case 0:	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/electro4.wav", RANDOM_FLOAT(0.7, 0.8), ATTN_NORM); break;
		case 1:	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/electro5.wav", RANDOM_FLOAT(0.7, 0.8), ATTN_NORM); break;
		case 2:	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/electro6.wav", RANDOM_FLOAT(0.7, 0.8), ATTN_NORM); break;
		case 3:	break; // no sound
		}
		m_pPlayer->m_flPlayAftershock = 0.0;
	}

	if (m_fInAttack >= GAUSS_EMP_STATE_SPINUP)
	{
		const float flRadius = GaussEmpRadiusForCharge( gpGlobals->time - m_flStartThrow, GetFullChargeTime() );

		PLAYBACK_EVENT_FULL( FEV_NOTHOST | FEV_RELIABLE, m_pPlayer->edict(), m_usGaussFire, 0.01, (float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles, 0.0, 0.0, 0, 0, 0, 1 );
		SendWeaponAnim( GAUSS_FIRE2 );
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/gauss2.wav", 1.0, ATTN_NORM, 0, 110 + RANDOM_LONG(0,20) );

#ifndef CLIENT_DLL
		int iDisrupted = GaussEmpDisruptEntities( m_pPlayer, flRadius );
		GaussEmpPulseFx( m_pPlayer, flRadius, m_iEmpPulseSprite, m_iEmpPulseIceSprite );
		if (iDisrupted > 0)
			EMIT_SOUND( ENT(m_pPlayer->pev), CHAN_ITEM, "buttons/blip1.wav", 0.7, ATTN_NORM );
		CSoundEnt::InsertSound( bits_SOUND_COMBAT, m_pPlayer->pev->origin, NORMAL_GUN_VOLUME, 0.4 );
#endif

		m_fInAttack = 0;
		m_flStartThrow = 0;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75f;
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + GAUSS_EMP_RELEASE_COOLDOWN;
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay( GAUSS_EMP_RELEASE_COOLDOWN );
		return;
	}

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if (m_fInAttack != 0)
	{
		StartFire();
		m_fInAttack = 0;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
	}
	else
	{
		int iAnim;
		float flRand = RANDOM_FLOAT(0, 1);
		if (flRand <= 0.5)
		{
			iAnim = GAUSS_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		}
		else if (flRand <= 0.75)
		{
			iAnim = GAUSS_IDLE2;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		}
		else
		{
			iAnim = GAUSS_FIDGET;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3;
		}

		return;
		SendWeaponAnim( iAnim );
		
	}
}






class CGaussAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_gaussammo.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_gaussammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_URANIUMBOX_GIVE, "uranium", URANIUM_MAX_CARRY ) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_gaussclip, CGaussAmmo );

#endif
