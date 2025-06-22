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
/*

===== weapons.cpp ========================================================

  functions governing the selection/use of weapons for players

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "soundent.h"
#include "decals.h"
#include "gamerules.h"
#include "items.h"
#include "game.h"
#include "animation.h"
#include "disc.h"
#include "shake.h"

extern CGraph	WorldGraph;
extern int gEvilImpulse101;

extern int g_ItemsExplode;

#define NOT_USED 255

DLL_GLOBAL	short	g_sModelIndexLaser;// holds the index for the laser beam
DLL_GLOBAL  const char *g_pModelNameLaser = "sprites/laserbeam.spr";
DLL_GLOBAL	short	g_sModelIndexLaserDot;// holds the index for the laser beam dot
DLL_GLOBAL	short	g_sModelIndexFireball;// holds the index for the fireball
DLL_GLOBAL	short	g_sModelIndexSmoke;// holds the index for the smoke cloud
DLL_GLOBAL	short	g_sModelIndexWExplosion;// holds the index for the underwater explosion
DLL_GLOBAL	short	g_sModelIndexBubbles;// holds the index for the bubbles model
DLL_GLOBAL	short	g_sModelIndexBloodDrop;// holds the sprite index for the initial blood
DLL_GLOBAL	short	g_sModelIndexBloodSpray;// holds the sprite index for splattered blood
DLL_GLOBAL	short	g_sModelIndexSnowballHit;
DLL_GLOBAL	short	g_sModelIndexGunsmoke;
DLL_GLOBAL	short 	g_sModelIndexIceFireball;
DLL_GLOBAL	short	g_sModelIndexFartSmoke;
DLL_GLOBAL	short 	g_sModelIndexFire;
DLL_GLOBAL	short 	g_sModelIndexIceFire;
DLL_GLOBAL	short	g_sModelConcreteGibs;
DLL_GLOBAL	short	g_sModelWoodGibs;
DLL_GLOBAL	short	g_sModelLightning;
DLL_GLOBAL	short	g_sModelIndexFlame;
DLL_GLOBAL	short	g_Glass;
DLL_GLOBAL	short	g_Steamball;

ItemInfo CBasePlayerItem::ItemInfoArray[MAX_WEAPONS];
AmmoInfo CBasePlayerItem::AmmoInfoArray[MAX_AMMO_SLOTS];

extern int gmsgCurWeapon;


MULTIDAMAGE gMultiDamage;

#define TRACER_FREQ		4			// Tracers fire every fourth bullet


//=========================================================
// MaxAmmoCarry - pass in a name and this function will tell
// you the maximum amount of that type of ammunition that a 
// player can carry.
//=========================================================
int MaxAmmoCarry( int iszName )
{
	for ( int i = 0;  i < MAX_WEAPONS; i++ )
	{
		if ( CBasePlayerItem::ItemInfoArray[i].pszAmmo1 && !strcmp( STRING(iszName), CBasePlayerItem::ItemInfoArray[i].pszAmmo1 ) )
			return CBasePlayerItem::ItemInfoArray[i].iMaxAmmo1;
		if ( CBasePlayerItem::ItemInfoArray[i].pszAmmo2 && !strcmp( STRING(iszName), CBasePlayerItem::ItemInfoArray[i].pszAmmo2 ) )
			return CBasePlayerItem::ItemInfoArray[i].iMaxAmmo2;
	}

	ALERT( at_console, "MaxAmmoCarry() doesn't recognize '%s'!\n", STRING( iszName ) );
	return -1;
}

	
/*
==============================================================================

MULTI-DAMAGE

Collects multiple small damages into a single damage

==============================================================================
*/

//
// ClearMultiDamage - resets the global multi damage accumulator
//
void ClearMultiDamage(void)
{
	gMultiDamage.pEntity = NULL;
	gMultiDamage.amount	= 0;
	gMultiDamage.type = 0;
	gMultiDamage.time = 0;
}


//
// ApplyMultiDamage - inflicts contents of global multi damage register on gMultiDamage.pEntity
//
// GLOBALS USED:
//		gMultiDamage

void ApplyMultiDamage(entvars_t *pevInflictor, entvars_t *pevAttacker )
{
	Vector		vecSpot1;//where blood comes from
	Vector		vecDir;//direction blood should go
	TraceResult	tr;
	
	if ( !gMultiDamage.pEntity )
		return;

	gMultiDamage.time = gpGlobals->time;

	gMultiDamage.pEntity->TakeDamage(pevInflictor, pevAttacker, gMultiDamage.amount, gMultiDamage.type );
}


// GLOBALS USED:
//		gMultiDamage

void AddMultiDamage( entvars_t *pevInflictor, CBaseEntity *pEntity, float flDamage, int bitsDamageType)
{
	if ( !pEntity )
		return;
	
	gMultiDamage.type |= bitsDamageType;

	if ( pEntity != gMultiDamage.pEntity )
	{
		ApplyMultiDamage(pevInflictor,pevInflictor); // UNDONE: wrong attacker!
		gMultiDamage.pEntity	= pEntity;
		gMultiDamage.amount		= 0;
	}

	gMultiDamage.amount += flDamage;
}

/*
================
SpawnBlood
================
*/
void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage)
{
	int max = moreblood.value > 5 ? 5 : moreblood.value;
	max = moreblood.value < 1 ? 1 : moreblood.value;

	UTIL_BloodDrips( vecSpot, g_vecAttackDir, bloodColor, (int)flDamage * max );

	// Streams in single player looks white?
	if (gpGlobals->deathmatch)
	{
		for (int i = 1; i <= max; i++) {
			UTIL_BloodStream(vecSpot, g_vecAttackDir, bloodColor, (int)flDamage * i);
		}
	}
}


int DamageDecal( CBaseEntity *pEntity, int bitsDamageType )
{
	if ( !pEntity )
		return (DECAL_GUNSHOT1 + RANDOM_LONG(0,4));
	
	return pEntity->DamageDecal( bitsDamageType );
}

void DecalGunshot( TraceResult *pTrace, int iBulletType )
{
	// Is the entity valid
	if ( !UTIL_IsValidEntity( pTrace->pHit ) )
		return;

	if ( VARS(pTrace->pHit)->solid == SOLID_BSP || VARS(pTrace->pHit)->movetype == MOVETYPE_PUSHSTEP )
	{
		CBaseEntity *pEntity = NULL;
		// Decal the wall with a gunshot
		if ( !FNullEnt(pTrace->pHit) )
			pEntity = CBaseEntity::Instance(pTrace->pHit);

		switch( iBulletType )
		{
		case BULLET_PLAYER_9MM:
		case BULLET_MONSTER_9MM:
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_PLAYER_BUCKSHOT:
		case BULLET_PLAYER_EXPLOSIVE_BUCKSHOT:
		case BULLET_PLAYER_357:
		case BULLET_PLAYER_RIFLE:
		default:
			// smoke and decal
			UTIL_GunshotDecalTrace( pTrace, DamageDecal( pEntity, DMG_BULLET ) );
			break;
		case BULLET_MONSTER_12MM:
			// smoke and decal
			UTIL_GunshotDecalTrace( pTrace, DamageDecal( pEntity, DMG_BULLET ) );
			break;
		case BULLET_PLAYER_CROWBAR:
			// wall decal
			UTIL_DecalTrace( pTrace, DECAL_CROWBAR1 + RANDOM_LONG(0,3) );
			break;
		case BULLET_PLAYER_FIST:
			// wall decal
			UTIL_DecalTrace( pTrace, DECAL_CRACK1 + RANDOM_LONG(0,1) );
			break;
		case BULLET_PLAYER_WRENCH:
			// wall decal
			UTIL_DecalTrace( pTrace, DECAL_CRACK1 + RANDOM_LONG(2,3) );
			break;
		case BULLET_PLAYER_SNOWBALL:
			// wall decal
			UTIL_DecalTrace( pTrace, DECAL_SNOW1 + RANDOM_LONG(0,3) );
			break;
		case BULLET_PLAYER_CHAINSAW:
			UTIL_DecalTrace( pTrace, DECAL_DING6 + RANDOM_LONG(0,3) );
			break;
		case BULLET_PLAYER_KNIFE:
			// wall decal
			UTIL_DecalTrace( pTrace, DECAL_SLASH1 + RANDOM_LONG(0,2) );
			break;
		case BULLET_PLAYER_BOOT:
			// wall decal
			UTIL_DecalTrace( pTrace, DECAL_FOOT_CRACK + RANDOM_LONG(0,0) );
			break;
		}
	}
}



//
// EjectBrass - tosses a brass shell from passed origin at passed velocity
//
void EjectBrass ( const Vector &vecOrigin, const Vector &vecVelocity, float rotation, int model, int soundtype )
{
	// FIX: when the player shoots, their gun isn't in the same position as it is on the model other players see.

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE( TE_MODEL);
		WRITE_COORD( vecOrigin.x);
		WRITE_COORD( vecOrigin.y);
		WRITE_COORD( vecOrigin.z);
		WRITE_COORD( vecVelocity.x);
		WRITE_COORD( vecVelocity.y);
		WRITE_COORD( vecVelocity.z);
		WRITE_ANGLE( rotation );
		WRITE_SHORT( model );
		WRITE_BYTE ( soundtype);
		WRITE_BYTE ( 25 );// 2.5 seconds
	MESSAGE_END();
}


#if 0
// UNDONE: This is no longer used?
void ExplodeModel( const Vector &vecOrigin, float speed, int model, int count )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE ( TE_EXPLODEMODEL );
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_COORD( speed );
		WRITE_SHORT( model );
		WRITE_SHORT( count );
		WRITE_BYTE ( 15 );// 1.5 seconds
	MESSAGE_END();
}
#endif


int giAmmoIndex = 0;

// Precaches the ammo and queues the ammo info for sending to clients
void AddAmmoNameToAmmoRegistry( const char *szAmmoname )
{
	// make sure it's not already in the registry
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( !CBasePlayerItem::AmmoInfoArray[i].pszName)
			continue;

		if ( stricmp( CBasePlayerItem::AmmoInfoArray[i].pszName, szAmmoname ) == 0 )
			return; // ammo already in registry, just quite
	}


	giAmmoIndex++;
	ASSERT( giAmmoIndex < MAX_AMMO_SLOTS );
	if ( giAmmoIndex >= MAX_AMMO_SLOTS )
		giAmmoIndex = 0;

	CBasePlayerItem::AmmoInfoArray[giAmmoIndex].pszName = szAmmoname;
	CBasePlayerItem::AmmoInfoArray[giAmmoIndex].iId = giAmmoIndex;   // yes, this info is redundant
}


// Precaches the weapon and queues the weapon info for sending to clients
void UTIL_PrecacheOtherWeapon( const char *szClassname )
{
	edict_t	*pent;

	pent = CREATE_NAMED_ENTITY( MAKE_STRING( szClassname ) );
	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in UTIL_PrecacheOtherWeapon\n" );
		return;
	}
	
	CBaseEntity *pEntity = CBaseEntity::Instance (VARS( pent ));

	if (pEntity)
	{
		ItemInfo II;
		pEntity->Precache( );
		memset( &II, 0, sizeof II );
		if ( ((CBasePlayerItem*)pEntity)->GetItemInfo( &II ) )
		{
			CBasePlayerItem::ItemInfoArray[II.iId] = II;

			if ( II.pszAmmo1 && *II.pszAmmo1 )
			{
				AddAmmoNameToAmmoRegistry( II.pszAmmo1 );
			}

			if ( II.pszAmmo2 && *II.pszAmmo2 )
			{
				AddAmmoNameToAmmoRegistry( II.pszAmmo2 );
			}

			memset( &II, 0, sizeof II );
		}
	}

	REMOVE_ENTITY(pent);
}

// called by worldspawn
void W_Precache(void)
{
	memset( CBasePlayerItem::ItemInfoArray, 0, sizeof(CBasePlayerItem::ItemInfoArray) );
	memset( CBasePlayerItem::AmmoInfoArray, 0, sizeof(CBasePlayerItem::AmmoInfoArray) );
	giAmmoIndex = 0;

	// Global models in cold ice
	PRECACHE_MODEL("models/w_weapons.mdl");
	PRECACHE_MODEL("models/p_weapons.mdl");
	PRECACHE_MODEL("models/w_items.mdl");
	PRECACHE_MODEL("models/w_shell.mdl");
	PRECACHE_MODEL("models/w_shotgunshell.mdl");

	// custom items...

	// common world objects
	UTIL_PrecacheOther( "item_suit" );
	UTIL_PrecacheOther( "item_battery" );
	//UTIL_PrecacheOther( "item_antidote" );
	//UTIL_PrecacheOther( "item_security" );
	UTIL_PrecacheOther( "item_longjump" );
#if defined( GRAPPLING_HOOK )
	UTIL_PrecacheOther( "grapple_hook" );
#endif

	// shotgun
	UTIL_PrecacheOtherWeapon( "weapon_shotgun" );
	UTIL_PrecacheOther( "ammo_buckshot" );

	// crowbar
	UTIL_PrecacheOtherWeapon( "weapon_crowbar" );

	// knife
	UTIL_PrecacheOtherWeapon( "weapon_knife" );

	// glock
	UTIL_PrecacheOtherWeapon( "weapon_9mmhandgun" );
	UTIL_PrecacheOther( "ammo_9mmclip" );

	// mp5
	UTIL_PrecacheOtherWeapon( "weapon_9mmAR" );
	UTIL_PrecacheOther( "ammo_9mmAR" );
	UTIL_PrecacheOther( "ammo_ARgrenades" );

#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
	// python
	UTIL_PrecacheOtherWeapon( "weapon_357" );
	UTIL_PrecacheOther( "ammo_357" );
#endif
	
#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
	// gauss
	UTIL_PrecacheOtherWeapon( "weapon_gauss" );
	UTIL_PrecacheOther( "ammo_gaussclip" );
#endif

#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
	// rpg
	UTIL_PrecacheOtherWeapon( "weapon_rpg" );
	UTIL_PrecacheOther( "ammo_rpgclip" );
#endif

#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
	// crossbow
	UTIL_PrecacheOtherWeapon( "weapon_crossbow" );
	UTIL_PrecacheOther( "ammo_crossbow" );
#endif

#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
	// egon
	UTIL_PrecacheOtherWeapon( "weapon_egon" );
#endif

	// tripmine
	UTIL_PrecacheOtherWeapon( "weapon_tripmine" );

#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
	// satchel charge
	UTIL_PrecacheOtherWeapon( "weapon_satchel" );
#endif

	// hand grenade
	UTIL_PrecacheOtherWeapon("weapon_handgrenade");

#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
	// squeak grenade
	UTIL_PrecacheOtherWeapon( "weapon_snark" );
#endif

#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
	// hornetgun
	UTIL_PrecacheOtherWeapon( "weapon_hornetgun" );
#endif

	UTIL_PrecacheOtherWeapon( "weapon_vest" );
	UTIL_PrecacheOtherWeapon( "weapon_chumtoad" );
	UTIL_PrecacheOtherWeapon( "weapon_sniperrifle" );
	UTIL_PrecacheOtherWeapon( "weapon_railgun" );
	UTIL_PrecacheOtherWeapon( "weapon_cannon" );
	UTIL_PrecacheOtherWeapon( "weapon_mag60" );
	UTIL_PrecacheOtherWeapon( "weapon_chaingun" );
	UTIL_PrecacheOtherWeapon( "weapon_glauncher" );
	UTIL_PrecacheOtherWeapon( "weapon_smg" );
	UTIL_PrecacheOtherWeapon( "weapon_usas" );
	UTIL_PrecacheOtherWeapon( "weapon_fists" );
	UTIL_PrecacheOtherWeapon( "weapon_wrench" );
	UTIL_PrecacheOtherWeapon( "weapon_snowball" );
	UTIL_PrecacheOtherWeapon( "weapon_chainsaw" );
	UTIL_PrecacheOtherWeapon( "weapon_12gauge" );
	UTIL_PrecacheOtherWeapon( "weapon_nuke" );
	UTIL_PrecacheOtherWeapon( "weapon_deagle" );
	UTIL_PrecacheOtherWeapon( "weapon_dual_deagle" );
	UTIL_PrecacheOtherWeapon( "weapon_dual_rpg" );
	UTIL_PrecacheOtherWeapon( "weapon_dual_mag60" );
	UTIL_PrecacheOtherWeapon( "weapon_dual_smg" );
	UTIL_PrecacheOtherWeapon( "weapon_dual_wrench" );
	UTIL_PrecacheOtherWeapon( "weapon_dual_usas" );
	UTIL_PrecacheOtherWeapon( "weapon_freezegun" );
	UTIL_PrecacheOtherWeapon( "weapon_rocketcrowbar" );
	UTIL_PrecacheOtherWeapon( "weapon_dual_railgun" );
	UTIL_PrecacheOtherWeapon( "weapon_gravitygun" );
	UTIL_PrecacheOtherWeapon( "weapon_flamethrower" );
	UTIL_PrecacheOtherWeapon( "weapon_dual_flamethrower" );
	UTIL_PrecacheOtherWeapon( "weapon_ashpod" );
	UTIL_PrecacheOtherWeapon( "weapon_sawedoff" );
	UTIL_PrecacheOtherWeapon( "weapon_dual_sawedoff" );
	UTIL_PrecacheOtherWeapon( "weapon_dual_chaingun" );
	UTIL_PrecacheOtherWeapon( "weapon_dual_hornetgun" );
	UTIL_PrecacheOtherWeapon( "weapon_fingergun" );
	UTIL_PrecacheOtherWeapon( "weapon_zapgun" );
	UTIL_PrecacheOtherWeapon( "weapon_dual_glock" );
	UTIL_PrecacheOtherWeapon( "weapon_vice" );

	UTIL_PrecacheOther( "monster_barrel" );
	UTIL_PrecacheOther( "monster_sentry" );
	UTIL_PrecacheOther( "monster_human_assassin" );
	UTIL_PrecacheOther( "tracer" );
	UTIL_PrecacheOther( "disc" );
	UTIL_PrecacheOther( "monster_tombstone" );
	UTIL_PrecacheOther( "monster_grabweapon" );
	UTIL_PrecacheOther( "monster_propdecoy" );

#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )
	UTIL_PrecacheOther( "weaponbox" );// container for dropped deathmatch weapons
#endif

	g_sModelIndexFireball = PRECACHE_MODEL ("sprites/zerogxplode.spr");// fireball
	g_sModelIndexWExplosion = PRECACHE_MODEL ("sprites/WXplo1.spr");// underwater fireball
	g_sModelIndexSmoke = PRECACHE_MODEL ("sprites/steam1.spr");// smoke
	g_sModelIndexBubbles = PRECACHE_MODEL ("sprites/bubble.spr");//bubbles
	g_sModelIndexBloodSpray = PRECACHE_MODEL ("sprites/bloodspray.spr"); // initial blood
	g_sModelIndexBloodDrop = PRECACHE_MODEL ("sprites/blood.spr"); // splattered blood 

	g_sModelIndexLaser = PRECACHE_MODEL( (char *)g_pModelNameLaser );
	g_sModelIndexLaserDot = PRECACHE_MODEL("sprites/laserdot.spr");

	g_sModelIndexSnowballHit = PRECACHE_MODEL ("sprites/snowballhit.spr");
	g_sModelIndexGunsmoke = PRECACHE_MODEL ("sprites/gunsmoke.spr");
	g_sModelIndexFartSmoke = PRECACHE_MODEL ("sprites/fart_smoke.spr");
	PRECACHE_MODEL ("sprites/sparks.spr");
	PRECACHE_MODEL ("sprites/ice_sparks.spr");
	PRECACHE_MODEL ("sprites/smokeball2.spr");
	PRECACHE_MODEL ("sprites/ice_muzzleflash1.spr");
	PRECACHE_MODEL ("sprites/ice_muzzleflash2.spr");
	PRECACHE_MODEL ("sprites/ice_muzzleflash3.spr");

	g_sModelIndexIceFireball = PRECACHE_MODEL ("sprites/ice_zerogxplode.spr");
	g_sModelIndexFire = PRECACHE_MODEL ("sprites/fire.spr");
	g_sModelIndexIceFire = PRECACHE_MODEL ("sprites/ice_fire.spr");
	g_sModelIndexFlame = PRECACHE_MODEL ("sprites/flamesteam.spr");
	PRECACHE_MODEL ("sprites/sanic.spr");
	PRECACHE_MODEL ("sprites/lifebar.spr");
	PRECACHE_SOUND ("fire1.wav");

	// Weather
	PRECACHE_MODEL ("sprites/rain.spr");
	PRECACHE_MODEL ("sprites/snowflake.spr");
	PRECACHE_MODEL ("sprites/ripple.spr");

	PRECACHE_MODEL ("models/v_leg.mdl");
	PRECACHE_MODEL ("models/v_dual_leg.mdl");
	PRECACHE_SOUND ("kick.wav");
	g_sModelConcreteGibs = PRECACHE_MODEL("models/w_concretegibs.mdl");
	g_sModelWoodGibs = PRECACHE_MODEL("models/woodgibs.mdl");

	g_sModelLightning = PRECACHE_MODEL("sprites/lgtning.spr");

	PRECACHE_SOUND("freezing.wav");
	PRECACHE_SOUND("wallclimb.wav");
	PRECACHE_SOUND("delicious.wav");
	PRECACHE_SOUND("hohoho.wav");
	PRECACHE_SOUND("merrychristmas.wav");
	PRECACHE_SOUND("sleighbell.wav");
	PRECACHE_SOUND("fart.wav");

	g_Glass = PRECACHE_MODEL("models/glassgibs.mdl");
	g_Steamball = PRECACHE_MODEL ("sprites/stmbal1.spr");

	// used by explosions
	PRECACHE_MODEL ("models/grenade.mdl");
	PRECACHE_MODEL ("sprites/explode1.spr");

	PRECACHE_SOUND ("weapons/debris1.wav");// explosion aftermaths
	PRECACHE_SOUND ("weapons/debris2.wav");// explosion aftermaths
	PRECACHE_SOUND ("weapons/debris3.wav");// explosion aftermaths

	PRECACHE_SOUND ("weapons/grenade_hit1.wav");//grenade
	PRECACHE_SOUND ("weapons/grenade_hit2.wav");//grenade
	PRECACHE_SOUND ("weapons/grenade_hit3.wav");//grenade

	PRECACHE_SOUND ("weapons/bullet_hit1.wav");	// hit by bullet
	PRECACHE_SOUND ("weapons/bullet_hit2.wav");	// hit by bullet
	
	PRECACHE_SOUND ("items/weapondrop1.wav");// weapon falls to the ground

}


 

TYPEDESCRIPTION	CBasePlayerItem::m_SaveData[] = 
{
	DEFINE_FIELD( CBasePlayerItem, m_pPlayer, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayerItem, m_pNext, FIELD_CLASSPTR ),
	//DEFINE_FIELD( CBasePlayerItem, m_fKnown, FIELD_INTEGER ),Reset to zero on load
	DEFINE_FIELD( CBasePlayerItem, m_iId, FIELD_INTEGER ),
	// DEFINE_FIELD( CBasePlayerItem, m_iIdPrimary, FIELD_INTEGER ),
	// DEFINE_FIELD( CBasePlayerItem, m_iIdSecondary, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CBasePlayerItem, CBaseAnimating );


TYPEDESCRIPTION	CBasePlayerWeapon::m_SaveData[] = 
{
#if defined( CLIENT_WEAPONS )
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextPrimaryAttack, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextSecondaryAttack, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flTimeWeaponIdle, FIELD_FLOAT ),
#else	// CLIENT_WEAPONS
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextSecondaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flTimeWeaponIdle, FIELD_TIME ),
#endif	// CLIENT_WEAPONS
	DEFINE_FIELD( CBasePlayerWeapon, m_iPrimaryAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iSecondaryAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iClip, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iDefaultAmmo, FIELD_INTEGER ),
//	DEFINE_FIELD( CBasePlayerWeapon, m_iClientClip, FIELD_INTEGER )	 , reset to zero on load so hud gets updated correctly
//  DEFINE_FIELD( CBasePlayerWeapon, m_iClientWeaponState, FIELD_INTEGER ), reset to zero on load so hud gets updated correctly
};

IMPLEMENT_SAVERESTORE( CBasePlayerWeapon, CBasePlayerItem );


void CBasePlayerItem :: SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector(-24, -24, 0);
	pev->absmax = pev->origin + Vector(24, 24, 16); 
}


//=========================================================
// Sets up movetype, size, solidtype for a new weapon. 
//=========================================================
void CBasePlayerItem :: FallInit( void )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;

	if (g_ItemsExplode)
	{
		pev->takedamage = DAMAGE_YES;
		pev->health = 1;
	}

	if (floatingweapons.value) {
		pev->sequence = ((m_iId - 1) * 2) + 1;
		pev->animtime = gpGlobals->time;
		pev->framerate = 1.0;
	}

	if (m_iId == WEAPON_ASHPOD || m_iId == WEAPON_HANDGRENADE)
	{
		pev->sequence = floatingweapons.value;
	}

	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0) );//pointsize until it lands on the ground.
	
	SetTouch( &CBasePlayerItem::DefaultTouch );
	SetThink( &CBasePlayerItem::FallThink );

	pev->nextthink = gpGlobals->time + 0.1;
}

//=========================================================
// FallThink - Items that have just spawned run this think
// to catch them when they hit the ground. Once we're sure
// that the object is grounded, we change its solid type
// to trigger and set it in a large box that helps the
// player get it.
//=========================================================
void CBasePlayerItem::FallThink ( void )
{
	pev->nextthink = gpGlobals->time + 0.1;

	if ( pev->flags & FL_ONGROUND )
	{
		// clatter if we have an owner (i.e., dropped by someone)
		// don't clatter if the gun is waiting to respawn (if it's waiting, it is invisible!)
		if ( !FNullEnt( pev->owner ) )
		{
			int pitch = 95 + RANDOM_LONG(0,29);
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "items/weapondrop1.wav", 1, ATTN_NORM, 0, pitch);	
		}

		// lie flat
		pev->angles.x = 0;
		pev->angles.z = 0;

		Materialize(); 
	}

	if ( g_pGameRules->IsBusters() && FNullEnt( pev->owner ) )
	{
		if ( !strcmp( "weapon_egon", STRING( pev->classname ) ) )
		{
			UTIL_Remove( this );
		}
	}
}

//=========================================================
// Materialize - make a CBasePlayerItem visible and tangible
//=========================================================
void CBasePlayerItem::Materialize( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		// changing from invisible state to visible.
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;

		if (g_ItemsExplode)
		{
			pev->health = 1;
			pev->takedamage = DAMAGE_YES;
		}
	}

	if (g_pGameRules->IsPropHunt())
	{
		pev->health = 1;
		pev->takedamage = DAMAGE_YES;
		UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);
	}
	else
	{
		pev->solid = SOLID_TRIGGER;
	}

	UTIL_SetOrigin( pev, pev->origin );// link into world.
	SetTouch (&CBasePlayerItem::DefaultTouch);
	SetThink (NULL);

}

int CBasePlayerItem::TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if ( !pev->takedamage )
		return 0;

	if ( pev->effects & EF_NODRAW )
		return 0;

	if (!g_pGameRules->IsPropHunt() && pevInflictor == pevAttacker)
		return 0;

#ifndef CLIENT_DLL
	if (pev->health > 0 && flDamage > 0)
	{
		pev->takedamage = DAMAGE_NO;

		if (g_pGameRules->IsPropHunt())
		{
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_TAREXPLOSION );
				WRITE_COORD( pev->origin.x );
				WRITE_COORD( pev->origin.y );
				WRITE_COORD( pev->origin.z );
			MESSAGE_END();
			g_pGameRules->MonsterKilled( NULL, pevAttacker );
		}

		if (g_ItemsExplode)
		{
			pev->dmg = RANDOM_LONG(50, 100);
			int iContents = UTIL_PointContents ( pev->origin );
			MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
				WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
				WRITE_COORD( pev->origin.x );	// Send to PAS because of the sound
				WRITE_COORD( pev->origin.y );
				WRITE_COORD( pev->origin.z );
				if (iContents != CONTENTS_WATER)
				{
					if (icesprites.value) {
						WRITE_SHORT( g_sModelIndexIceFireball );
					} else {
						WRITE_SHORT( g_sModelIndexFireball );
					}
				}
				else
				{
					WRITE_SHORT( g_sModelIndexWExplosion );
				}
				WRITE_BYTE( (pev->dmg) * .60  ); // scale * 10
				WRITE_BYTE( 15 ); // framerate
				WRITE_BYTE( TE_EXPLFLAG_NONE );
			MESSAGE_END();
			TraceResult tr;
			UTIL_TraceLine ( pev->origin, pev->origin + Vector ( 0, 0, -128 ), ignore_monsters, ENT(pev), &tr);
			enum decal_e decal = DECAL_SCORCH1;
			int index = RANDOM_LONG(0, 1);
			if (g_pGameRules->MutatorEnabled(MUTATOR_PAINTBALL)) {
				decal = DECAL_PAINTL1;
				index = RANDOM_LONG(0, 7);
			}
			UTIL_DecalTrace(&tr, decal + index);
			::RadiusDamage( pev->origin, pev, pevAttacker, pev->dmg, pev->dmg  * 2.5, CLASS_NONE, DMG_BURN );
		}

		if ( g_pGameRules->WeaponShouldRespawn( this ) == GR_WEAPON_RESPAWN_YES )
		{
			Respawn();
			Kill();
		}
		else
		{
			Kill();
		}
	}
#endif
	return 1;
}

//=========================================================
// AttemptToMaterialize - the item is trying to rematerialize,
// should it do so now or wait longer?
//=========================================================
void CBasePlayerItem::AttemptToMaterialize( void )
{
	float time = g_pGameRules->FlWeaponTryRespawn( this );

	if ( time == 0 )
	{
		Materialize();
		return;
	}

	pev->nextthink = gpGlobals->time + time;
}

//=========================================================
// CheckRespawn - a player is taking this weapon, should 
// it respawn?
//=========================================================
void CBasePlayerItem :: CheckRespawn ( void )
{
	switch ( g_pGameRules->WeaponShouldRespawn( this ) )
	{
	case GR_WEAPON_RESPAWN_YES:
		Respawn();
		break;
	case GR_WEAPON_RESPAWN_NO:
		return;
		break;
	}
}

//=========================================================
// Respawn- this item is already in the world, but it is
// invisible and intangible. Make it visible and tangible.
//=========================================================
CBaseEntity* CBasePlayerItem::Respawn( void )
{
	CBaseEntity *pNewWeapon = NULL;
	const char* weaponsList[][12] = {
		{
		// swing
		"weapon_crowbar",
		"weapon_knife",
		"weapon_wrench",
		"weapon_chainsaw",
		"weapon_dual_wrench",
		"weapon_gravitygun",
		"weapon_ashpod"
		},

		{
		// hand
		"weapon_9mmhandgun",
		"weapon_dual_glock",
		"weapon_deagle",
		"weapon_dual_deagle",
		"weapon_python",
		"weapon_mag60",
		"weapon_dual_mag60"
		"weapon_smg",
		"weapon_dual_smg",
		"weapon_sawedoff",
		"weapon_dual_sawedoff",
		"weapon_zapgun",
		},

		{
		// long
		"weapon_9mmAR",
		"weapon_12gauge",
		"weapon_shotgun",
		"weapon_crossbow",
		"weapon_sniperrifle",
		"weapon_chaingun",
		"weapon_dual_chaingun",
		"weapon_usas",
		"weapon_dual_usas",
		"weapon_freezegun"
		},

		{
		// heavy
		"weapon_rpg",
		"weapon_dual_rpg",
		"weapon_railgun",
		"weapon_dual_railgun",
		"weapon_cannon",
		"weapon_gauss",
		"weapon_egon",
		"weapon_hornetgun",
		"weapon_dual_hornetgun",
		"weapon_glauncher",
		"weapon_nuke"
		},

		{
		// loose
		"weapon_snowball",
		"weapon_handgrenade",
		"weapon_satchel",
		"weapon_tripmine",
		"weapon_snark",
		"weapon_chumtoad",
		"weapon_vest",
		"weapon_flamethrower"
		},

		{
		// dual
		"weapon_dual_wrench",
		"weapon_dual_glock",
		"weapon_dual_deagle",
		"weapon_dual_mag60",
		"weapon_dual_smg",
		"weapon_dual_sawedoff",
		"weapon_dual_usas",
		"weapon_dual_chaingun",
		"weapon_dual_hornetgun",
		"weapon_dual_railgun",
		"weapon_dual_rpg",
		"weapon_dual_flamethrower"
		}
	};

	if (dualsonly.value) {
		if (strncmp(STRING(pev->classname), "weapon_dual_", 12) != 0) {
			const char *name = weaponsList[5][RANDOM_LONG(0, 11)];
			if (name)
			{
				pNewWeapon = CBaseEntity::Create((char *)STRING(ALLOC_STRING(name)), g_pGameRules->VecWeaponRespawnSpot(this), pev->angles, pev->owner);
			}
		}
	} else if (snowballfight.value) {
		if (strncmp(STRING(pev->classname), "weapon_snowball", 15) != 0) {
			pNewWeapon = CBaseEntity::Create("weapon_snowball", g_pGameRules->VecWeaponRespawnSpot(this), pev->angles, pev->owner);
		}
	/*
	} else if (g_pGameRules->MutatorEnabled(MUTATOR_INSTAGIB)) {
		if (strcmp(STRING(pev->classname), "ammo_gaussclip") != 0) {
			pNewWeapon = CBaseEntity::Create("ammo_gaussclip", g_pGameRules->VecWeaponRespawnSpot(this), pev->angles, pev->owner);
		}
	*/
	} else {
		for (int group = 0; (group < ARRAYSIZE(weaponsList) - 1) && pNewWeapon == NULL; group++) {
			int totalWeapons = ARRAYSIZE(weaponsList[group]);
			for (int weapon = 0; (weapon < totalWeapons) && pNewWeapon == NULL; weapon++) {
				if (weaponsList[group][weapon] != NULL && FStrEq(STRING(pev->classname), weaponsList[group][weapon])) {
#ifdef _DEBUG
					ALERT ( at_aiconsole, "Found %s to change... ", weaponsList[group][weapon] );
#endif
					char name[64] = "";
					const char *weapon = weaponsList[group][RANDOM_LONG(0, totalWeapons - 1)];
					if (weapon != NULL)
						sprintf(name, weapon);
					while (!g_pGameRules->CanRandomizeWeapon(name))
					{
						const char *innerWeapon = weaponsList[group][RANDOM_LONG(0, totalWeapons - 1)];
						if (innerWeapon != NULL)
							sprintf(name, innerWeapon);
					}

					if (strlen(name))
					{
						pNewWeapon = CBaseEntity::Create((char *)STRING(ALLOC_STRING(name)), g_pGameRules->VecWeaponRespawnSpot(this), pev->angles, pev->owner);
#ifdef _DEBUG
						ALERT ( at_aiconsole, "replaced with %s!\n", name );
#endif
					}
				}
			}
		}
	}

	// make a copy of this weapon that is invisible and inaccessible to players (no touch function). The weapon spawn/respawn code
	// will decide when to make the weapon visible and touchable.
	if (!pNewWeapon) {
		pNewWeapon = CBaseEntity::Create((char *)STRING( pev->classname ), g_pGameRules->VecWeaponRespawnSpot( this ), pev->angles, pev->owner );
	}

	if ( pNewWeapon )
	{
		pNewWeapon->pev->effects |= EF_NODRAW;// invisible for now
		pNewWeapon->SetTouch( NULL );// no touch
		pNewWeapon->SetThink( &CBasePlayerItem::AttemptToMaterialize );

		DROP_TO_FLOOR ( ENT(pev) );

		// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
		// but when it should respawn is based on conditions belonging to the weapon that was taken.
		pNewWeapon->pev->nextthink = g_pGameRules->FlWeaponRespawnTime( this );
	}
	else
	{
		ALERT ( at_console, "Respawn failed to create %s!\n", STRING( pev->classname ) );
	}

	return pNewWeapon;
}

void CBasePlayerItem::DefaultTouch( CBaseEntity *pOther )
{
	// Support if picked up and dropped
	pev->velocity = pev->velocity * 0.5;
	pev->avelocity = pev->avelocity * 0.5;

	// if it's not a player, ignore
	if ( !pOther->IsPlayer() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	// can I have this?
	if ( !g_pGameRules->CanHavePlayerItem( pPlayer, this ) )
	{
		if ( gEvilImpulse101 )
		{
			UTIL_Remove( this );
		}

		if ( pPlayer->pev->deadflag == DEAD_NO )
		{
			ProvideDualItem(pPlayer, STRING(this->pev->classname));
			Kill();
		}

		return;
	}

	if (pOther->AddPlayerItem( this ))
	{
		AttachToPlayer( pPlayer );
		EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM);
		if (g_pGameRules->IsAllowedSingleWeapon(this))
			ProvideSingleItem(pPlayer, STRING(this->pev->classname));
	}

	SUB_UseTargets( pOther, USE_TOGGLE, 0 ); // UNDONE: when should this happen?
}

BOOL CanAttack( float attack_time, float curtime, BOOL isPredicted )
{
#if defined( CLIENT_WEAPONS )
	if ( !isPredicted )
#else
	if ( 1 )
#endif
	{
		return ( attack_time <= curtime ) ? TRUE : FALSE;
	}
	else
	{
		return ( attack_time <= 0.0 ) ? TRUE : FALSE;
	}
}

void CBasePlayerWeapon::ItemPostFrame( void )
{
	/*
	if ((m_fInReload) && ( m_pPlayer->m_flNextAttack <= UTIL_WeaponTimeBase() ) )
	{
		if (RANDOM_LONG(0,2) == 1)
		{
			PlayEmptySound();
			ThrowWeapon(TRUE);
			return;
		}
	}
	*/

	BOOL oktofire = TRUE;
	if (g_pGameRules->IsCtC() && m_pPlayer->pev->fuser4 > 0)
		oktofire = FALSE;

	if (g_pGameRules->IsPropHunt() && m_pPlayer->pev->fuser4 > 0) {
		if ((m_pPlayer->pev->button & IN_ATTACK) &&
			CanAttack( m_flNextPrimaryAttack, gpGlobals->time, UseDecrement() ))
		{
			if (m_pPlayer->pev->fuser4 >= 59) // 52 weapon + 7 ammo
				m_pPlayer->pev->fuser4 = 0;
			m_pPlayer->pev->fuser4 += 1;
			// Skip blank spaces in model
			if (m_pPlayer->pev->fuser4 == 32)
				m_pPlayer->pev->fuser4 = 35;

			m_flNextPrimaryAttack = m_flNextSecondaryAttack =  UTIL_WeaponTimeBase() + 0.25;
			m_pPlayer->pev->button &= ~IN_ATTACK;
			m_pPlayer->pev->button &= ~IN_ATTACK2;
		}
		else if ((m_pPlayer->pev->button & IN_ATTACK2) &&
				CanAttack( m_flNextSecondaryAttack, gpGlobals->time, UseDecrement() ))
		{
			m_pPlayer->pev->fuser4 -= 1;
			if (m_pPlayer->pev->fuser4 <= 0)
				m_pPlayer->pev->fuser4 = 59; // 52 weapon + 7 ammo
			// Skip blank spaces in model
			if (m_pPlayer->pev->fuser4 == 34)
				m_pPlayer->pev->fuser4 = 31;

			m_flNextPrimaryAttack = m_flNextSecondaryAttack =  UTIL_WeaponTimeBase() + 0.25;
			m_pPlayer->pev->button &= ~IN_ATTACK;
			m_pPlayer->pev->button &= ~IN_ATTACK2;
		}
		else if ((m_pPlayer->pev->button & IN_RELOAD) &&
				CanAttack( m_flNextPrimaryAttack, gpGlobals->time, UseDecrement() ))
		{
			UTIL_MakeVectors( m_pPlayer->pev->v_angle );
			Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 64 + gpGlobals->v_up * 18;
			if (m_pPlayer->m_iPropsDeployed < 10)
			{
				CBaseEntity *p = CBaseEntity::Create( "monster_propdecoy", vecSrc, Vector(0, m_pPlayer->pev->v_angle.y, 0), m_pPlayer->edict() );
				if (p)
				{
					if (m_pPlayer->pev->fuser4 >= 50)
					{
						SET_MODEL( p->edict(), "models/w_ammo.mdl");
					}
					p->pev->body = m_pPlayer->pev->fuser4 >= 50 ? m_pPlayer->pev->fuser4 - 49 : m_pPlayer->pev->fuser4;
					p->pev->sequence = p->pev->body >= 50 ? ((p->pev->body - 49) * 2) + floatingweapons.value : (p->pev->body * 2) + floatingweapons.value;
				}
			}
			else
				ClientPrint(m_pPlayer->pev, HUD_PRINTCENTER, "All decoys are deployed.\n");
			m_pPlayer->m_flNextAttack = m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
			m_pPlayer->pev->button &= ~IN_RELOAD;
		}
		else
		{
			m_bFired = FALSE;
			// no fire buttons down
			m_fFireOnEmpty = FALSE;
			WeaponIdle();
		}

		return;
	}

	if ((g_pGameRules->MutatorEnabled(MUTATOR_DEALTER) && oktofire) ||
		(g_pGameRules->IsShidden() && m_pPlayer->pev->fuser4 > 0)) {
		if ((m_pPlayer->pev->button & IN_ATTACK) &&
			CanAttack( m_flNextPrimaryAttack, gpGlobals->time, UseDecrement() ))
		{
			UTIL_ScreenFade( m_pPlayer, Vector(0, 255, 0), 0.25, 0.50, 96, FFADE_IN);
			EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "fart.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(-20,20)); 
			::RadiusDamage( m_pPlayer->pev->origin, m_pPlayer->pev, m_pPlayer->pev, 800, 256, CLASS_NONE, DMG_FART );
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, m_pPlayer->pev->origin );
				WRITE_BYTE( TE_SMOKE );
				WRITE_COORD( m_pPlayer->pev->origin.x );
				WRITE_COORD( m_pPlayer->pev->origin.y );
				WRITE_COORD( m_pPlayer->pev->origin.z );
				WRITE_SHORT( g_sModelIndexSmoke );
				WRITE_BYTE( 48 ); // scale * 10
				WRITE_BYTE( 4 ); // framerate
			MESSAGE_END();

			if ( !(m_pPlayer->pev->flags & FL_ONGROUND) )
			{
				UTIL_MakeVectors(m_pPlayer->pev->v_angle);
				int q = 1;
				if (signbit(DotProduct(gpGlobals->v_forward, m_pPlayer->pev->velocity))) q = -1;
				m_pPlayer->pev->velocity = m_pPlayer->pev->velocity + (gpGlobals->v_forward * 400 * q) + gpGlobals->v_up * 300;
			}

			m_flNextPrimaryAttack = m_flNextSecondaryAttack =  UTIL_WeaponTimeBase() + 0.75;
			m_pPlayer->pev->button &= ~IN_ATTACK;
			m_pPlayer->pev->button &= ~IN_ATTACK2;
		}
		else if ((m_pPlayer->pev->button & IN_ATTACK2) &&
				CanAttack( m_flNextSecondaryAttack, gpGlobals->time, UseDecrement() ))
		{
			UTIL_ScreenFade( m_pPlayer, Vector(0, 255, 0), 0.25, 0.50, 96, FFADE_IN);
			EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "fart.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(-20,20)); 
			::RadiusDamage( m_pPlayer->pev->origin, m_pPlayer->pev, m_pPlayer->pev, 800, 256, CLASS_NONE, DMG_FART );
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, m_pPlayer->pev->origin );
				WRITE_BYTE( TE_SMOKE );
				WRITE_COORD( m_pPlayer->pev->origin.x );
				WRITE_COORD( m_pPlayer->pev->origin.y );
				WRITE_COORD( m_pPlayer->pev->origin.z );
				WRITE_SHORT( g_sModelIndexSmoke );
				WRITE_BYTE( 96 ); // scale * 10
				WRITE_BYTE( 4 ); // framerate
			MESSAGE_END();

			m_flNextPrimaryAttack = m_flNextSecondaryAttack =  UTIL_WeaponTimeBase() + 0.75;
			m_pPlayer->pev->button &= ~IN_ATTACK;
			m_pPlayer->pev->button &= ~IN_ATTACK2;
		}
		else
		{
			m_bFired = FALSE;
			// no fire buttons down
			m_fFireOnEmpty = FALSE;
			WeaponIdle();
		}

		return;
	}

	if (g_pGameRules->MutatorEnabled(MUTATOR_RICOCHET) && oktofire) {
		if ((m_pPlayer->pev->button & IN_ATTACK) &&
			CanAttack( m_flNextPrimaryAttack, gpGlobals->time, UseDecrement() ))
		{
			if (m_pPlayer->m_iFlyingDiscs < 3)
			{
				m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
				Vector vecFireDir = m_pPlayer->pev->v_angle;
				UTIL_MakeVectors( vecFireDir );
				CDisc::CreateDisc( m_pPlayer->pev->origin + (m_pPlayer->pev->view_ofs) + gpGlobals->v_forward * 16, vecFireDir, m_pPlayer, FALSE, 0 );
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 

				m_flNextPrimaryAttack = m_flNextSecondaryAttack =  UTIL_WeaponTimeBase() + 0.5;
				m_pPlayer->pev->button &= ~IN_ATTACK;
				m_pPlayer->pev->button &= ~IN_ATTACK2;
				m_pPlayer->m_iFlyingDiscs += 1;
			}
		}
		else if ((m_pPlayer->pev->button & IN_ATTACK2) &&
				CanAttack( m_flNextSecondaryAttack, gpGlobals->time, UseDecrement() ))
		{
			if (m_pPlayer->m_iFlyingDiscs == 0)
			{
				m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
				Vector vecFireDir = m_pPlayer->pev->v_angle;
				UTIL_MakeVectors( vecFireDir );
				CDisc::CreateDisc( m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16, vecFireDir, m_pPlayer, FALSE, 0 );
				CDisc::CreateDisc( m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16 + gpGlobals->v_right * -24, vecFireDir, m_pPlayer, FALSE, 0 );
				CDisc::CreateDisc( m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16 + gpGlobals->v_right * 24, vecFireDir, m_pPlayer, FALSE, 0 );
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/cbar_miss1.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 

				m_flNextPrimaryAttack = m_flNextSecondaryAttack =  UTIL_WeaponTimeBase() + 0.5;
				m_pPlayer->pev->button &= ~IN_ATTACK;
				m_pPlayer->pev->button &= ~IN_ATTACK2;
				m_pPlayer->m_iFlyingDiscs = 3;
			}
		}
		else
		{
			m_bFired = FALSE;
			// no fire buttons down
			m_fFireOnEmpty = FALSE;
			WeaponIdle();
		}

		return;
	}

	if (infiniteammo.value) {
		if (infiniteammo.value == 1)
			m_iClip = iMaxClip();

		if (g_pGameRules->IsCtC())
		{
			if (pszAmmo1())
			{
				if (strcmp(pszAmmo1(), "Chumtoads"))
					m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = iMaxAmmo1();
			}
		}
		else
		{
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = iMaxAmmo1();
		}
		m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] = iMaxAmmo2();
	}

	if ((m_fInReload) && ( m_pPlayer->m_flNextAttack <= UTIL_WeaponTimeBase() ) )
	{
		// complete the reload. 
		int j = fmin( iMaxClip() - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);	

		// Add them to the clip
		m_iClip += j;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= j;

		m_pPlayer->TabulateAmmo();

		m_fInReload = FALSE;
	}

	if (m_pPlayer->m_flEjectBrass != 0 && m_pPlayer->m_flEjectBrass <= gpGlobals->time)
	{
		m_pPlayer->m_flEjectBrass = 0;
		EjectBrassLate();
	}

	if (m_pPlayer->m_flEjectShotShell != 0 && m_pPlayer->m_flEjectShotShell <= gpGlobals->time)
	{
		m_pPlayer->m_flEjectShotShell = 0;
		EjectShotShellLate();
	}

	if ( !(m_pPlayer->pev->button & IN_ATTACK ) )
	{
		m_flLastFireTime = 0.0f;
	}

	BOOL canFire = FALSE;
	float multipler = g_pGameRules->WeaponMultipler();

	if ((m_pPlayer->pev->button & IN_ATTACK2) && CanAttack( m_flNextSecondaryAttack, gpGlobals->time, UseDecrement() ) )
	{
		if ( pszAmmo2() && !m_pPlayer->m_rgAmmo[SecondaryAmmoIndex()] )
		{
			m_fFireOnEmpty = TRUE;
		}

		if (g_pGameRules->IsPropHunt() && m_pPlayer->pev->fuser4 == 0) {
			m_pPlayer->pev->health--;
		}

		if (m_pPlayer->m_iHoldingItem) {
			m_pPlayer->ReleaseHeldItem(100);
		}

		m_pPlayer->TabulateAmmo();

		if (SemiAuto()) {
			if (!m_bFired)
			{
				if (!m_fFireOnEmpty)
					canFire = g_pGameRules->WeaponMutators(this);
				if (canFire)
				{
					SecondaryAttack();
					m_flNextPrimaryAttack = m_flNextSecondaryAttack = (m_flNextSecondaryAttack * multipler);
				}
			}
			m_bFired = TRUE;
		} else {
			if (!m_fFireOnEmpty)
				canFire = g_pGameRules->WeaponMutators(this);
			if (canFire)
			{
				SecondaryAttack();
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = (m_flNextSecondaryAttack * multipler);
			}
		}
		m_pPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if ((m_pPlayer->pev->button & IN_ATTACK) && CanAttack( m_flNextPrimaryAttack, gpGlobals->time, UseDecrement() ) )
	{
		if ( (m_iClip == 0 && pszAmmo1()) || (iMaxClip() == -1 && !m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] ) )
		{
			m_fFireOnEmpty = TRUE;
			// Hack for portal and gravity gun jam
			if (iMaxClip() == -1 && PrimaryAmmoIndex() == -1)
				canFire = TRUE;
		}

		if (m_pPlayer->m_iHoldingItem) {
			m_pPlayer->ReleaseHeldItem(100);
		}

		if (g_pGameRules->IsPropHunt() && m_pPlayer->pev->fuser4 == 0) {
			m_pPlayer->pev->health--;
		}

		m_pPlayer->TabulateAmmo();
		if (SemiAuto()) {
			if (!m_bFired)
			{
				if (!m_fFireOnEmpty)
					canFire = g_pGameRules->WeaponMutators(this);
				if (canFire)
				{
					PrimaryAttack();
					m_flNextPrimaryAttack = m_flNextSecondaryAttack = (m_flNextPrimaryAttack * multipler);
				}
			}
			m_bFired = TRUE;
		} else {
			if (!m_fFireOnEmpty)
				g_pGameRules->WeaponMutators(this);

			// Allow passthru for satchels
			PrimaryAttack();
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = (m_flNextPrimaryAttack * multipler);
		}
	}
	else if ( m_pPlayer->pev->button & IN_RELOAD && iMaxClip() != WEAPON_NOCLIP && !m_fInReload ) 
	{
		if (m_pPlayer->m_iHoldingItem) {
			m_pPlayer->ReleaseHeldItem(100);
		}

		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
		m_pPlayer->m_flNextAttack = m_flNextPrimaryAttack = m_flNextSecondaryAttack = (m_pPlayer->m_flNextAttack * multipler);
	}
	else if ( !(m_pPlayer->pev->button & (IN_ATTACK|IN_ATTACK2) ) )
	{
		m_bFired = FALSE;
		// no fire buttons down

		m_fFireOnEmpty = FALSE;

		if ( !IsUseable() && m_flNextPrimaryAttack < ( UseDecrement() ? 0.0 : gpGlobals->time ) ) 
		{
			// weapon isn't useable, switch.
			if ( !(iFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && g_pGameRules->GetNextBestWeapon( m_pPlayer, this, FALSE, FALSE ) )
			{
				m_flNextPrimaryAttack = ( UseDecrement() ? 0.0 : gpGlobals->time ) + 0.3;
				return;
			}
		}
		else
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( m_iClip == 0 && !(iFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < ( UseDecrement() ? 0.0 : gpGlobals->time ) )
			{
				Reload();
				m_pPlayer->m_flNextAttack = m_flNextPrimaryAttack = m_flNextSecondaryAttack = (m_pPlayer->m_flNextAttack * multipler);
				return;
			}
		}

		WeaponIdle( );
		return;
	}

	// catch all
	if ( ShouldWeaponIdle() )
	{
		WeaponIdle();
	}
}

void CBasePlayerItem::DestroyItem( void )
{
	if ( m_pPlayer )
	{
		// if attached to a player, remove. 
		m_pPlayer->RemovePlayerItem( this );
	}

	Kill( );
}

int CBasePlayerItem::AddToPlayer( CBasePlayer *pPlayer )
{
	m_pPlayer = pPlayer;

	return TRUE;
}

void CBasePlayerItem::Drop( void )
{
	SetTouch( NULL );
	SetThink(&CBasePlayerItem::SUB_Remove);
	pev->nextthink = gpGlobals->time + .1;
}

void CBasePlayerItem::Kill( void )
{
	SetTouch( NULL );
	SetThink(&CBasePlayerItem::SUB_Remove);
	pev->nextthink = gpGlobals->time + .1;
}

void CBasePlayerItem::Holster( int skiplocal /* = 0 */ )
{ 
	m_pPlayer->pev->viewmodel = 0; 
	m_pPlayer->pev->weaponmodel = 0;
}

void CBasePlayerItem::AttachToPlayer ( CBasePlayer *pPlayer )
{
	pev->movetype = MOVETYPE_FOLLOW;
	pev->solid = SOLID_NOT;
	pev->aiment = pPlayer->edict();
	pev->effects = EF_NODRAW; // ??
	pev->modelindex = 0;// server won't send down to clients if modelindex == 0
	pev->model = iStringNull;
	pev->owner = pPlayer->edict();
	pev->nextthink = gpGlobals->time + .1;
	SetTouch( NULL );
}

// CALLED THROUGH the newly-touched weapon's instance. The existing player weapon is pOriginal
int CBasePlayerWeapon::AddDuplicate( CBasePlayerItem *pOriginal )
{
	if ( m_iDefaultAmmo )
	{
		return ExtractAmmo( (CBasePlayerWeapon *)pOriginal );
	}
	else
	{
		// a dead player dropped this.
		return ExtractClipAmmo( (CBasePlayerWeapon *)pOriginal );
	}
}


int CBasePlayerWeapon::AddToPlayer( CBasePlayer *pPlayer )
{
	int bResult = CBasePlayerItem::AddToPlayer( pPlayer );

	if (m_iId < 32)
		pPlayer->pev->weapons |= (1<<m_iId);
	else
		pPlayer->m_iWeapons2 |= (1<<(m_iId - 32));

	if ( !m_iPrimaryAmmoType )
	{
		m_iPrimaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo1() );
		m_iSecondaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo2() );
	}


	if (bResult)
		return AddWeapon( );
	return FALSE;
}

int CBasePlayerWeapon::UpdateClientData( CBasePlayer *pPlayer )
{
	BOOL bSend = FALSE;
	int state = 0;
	if ( pPlayer->m_pActiveItem == this )
	{
		if ( pPlayer->m_fOnTarget )
			state = WEAPON_IS_ONTARGET;
		else
			state = 1;
	}

	// Forcing send of all data!
	if ( !pPlayer->m_fWeapon )
	{
		bSend = TRUE;
	}
	
	// This is the current or last weapon, so the state will need to be updated
	if ( this == pPlayer->m_pActiveItem ||
		 this == pPlayer->m_pClientActiveItem )
	{
		if ( pPlayer->m_pActiveItem != pPlayer->m_pClientActiveItem )
		{
			bSend = TRUE;
		}
	}

	// If the ammo, state, or fov has changed, update the weapon
	if ( m_iClip != m_iClientClip || 
		 state != m_iClientWeaponState || 
		 pPlayer->m_iFOV != pPlayer->m_iClientFOV )
	{
		bSend = TRUE;
	}

	if ( bSend )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pPlayer->pev );
			WRITE_BYTE( state );
			WRITE_BYTE( m_iId );
			WRITE_BYTE( m_iClip );
		MESSAGE_END();

		m_iClientClip = m_iClip;
		m_iClientWeaponState = state;
		pPlayer->m_fWeapon = TRUE;
	}

	if ( m_pNext && m_pNext->m_pPlayer )
		m_pNext->UpdateClientData( pPlayer );

	return 1;
}


void CBasePlayerWeapon::SendWeaponAnim( int iAnim, int skiplocal, int body )
{
	if ( m_pPlayer == NULL )
	{
		return;
	}

	if ( UseDecrement() )
		skiplocal = 1;
	else
		skiplocal = 0;

	m_pPlayer->pev->weaponanim = iAnim;

#if defined( CLIENT_WEAPONS )
	//Disabled for holster weapon support.
	//I am not sure the consequence of this change? I calculate that
	//is is no harm in showing animations when its not needed from client
	//prediction standpoint.
	//if ( skiplocal && ENGINE_CANSKIP( m_pPlayer->edict() ) )
	//	return;
#endif

	if ( m_pPlayer->pev->flags & FL_FAKECLIENT ) // No bots
		return;

	MESSAGE_BEGIN( MSG_ONE, SVC_WEAPONANIM, NULL, m_pPlayer->pev );
		WRITE_BYTE( iAnim );						// sequence number
		WRITE_BYTE( body );					// weaponmodel bodygroup.
	MESSAGE_END();
}

BOOL CBasePlayerWeapon :: AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry )
{
	int iIdAmmo;

	if (iMaxClip < 1)
	{
		m_iClip = -1;
		iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );
	}
	else if (m_iClip == 0)
	{
		int i;
		i = fmin( m_iClip + iCount, iMaxClip ) - m_iClip;
		m_iClip += i;
		iIdAmmo = m_pPlayer->GiveAmmo( iCount - i, szName, iMaxCarry );
	}
	else
	{
		iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );
	}
	
	// m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = iMaxCarry; // hack for testing

	if (iIdAmmo > 0)
	{
		m_iPrimaryAmmoType = iIdAmmo;
		if (m_pPlayer->HasPlayerItem( this ) )
		{
			// play the "got ammo" sound only if we gave some ammo to a player that already had this gun.
			// if the player is just getting this gun for the first time, DefaultTouch will play the "picked up gun" sound for us.
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
	}

	return iIdAmmo > 0 ? TRUE : FALSE;
}


BOOL CBasePlayerWeapon :: AddSecondaryAmmo( int iCount, char *szName, int iMax )
{
	int iIdAmmo;

	iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMax );

	//m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] = iMax; // hack for testing

	if (iIdAmmo > 0)
	{
		m_iSecondaryAmmoType = iIdAmmo;
		EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
	}
	return iIdAmmo > 0 ? TRUE : FALSE;
}

//=========================================================
// IsUseable - this function determines whether or not a 
// weapon is useable by the player in its current state. 
// (does it have ammo loaded? do I have any ammo for the 
// weapon?, etc)
//=========================================================
BOOL CBasePlayerWeapon :: IsUseable( void )
{
	if ( m_iClip <= 0 )
	{
		if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] <= 0 && iMaxAmmo1() != -1 )			
		{
			// clip is empty (or nonexistant) and the player has no more ammo of this type. 
			return FALSE;
		}
	}

	return TRUE;
}

BOOL CBasePlayerWeapon :: CanDeploy( void )
{
	BOOL bHasAmmo = 0;

	// Bot patch
	if (!m_pPlayer) {
		return FALSE;
	}

	if (m_pPlayer->pev->deadflag == DEAD_FAKING)
		return FALSE;

	if (m_pPlayer->m_fTauntCancelTime > gpGlobals->time)
		return FALSE;

	if (m_pPlayer->m_fForceGrabTime > gpGlobals->time)
		return FALSE;

	if ( !pszAmmo1() )
	{
		// this weapon doesn't use ammo, can always deploy.
		return TRUE;
	}

	if ( pszAmmo1() )
	{
		bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);
	}
	if ( pszAmmo2() )
	{
		bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] != 0);
	}
	if (m_iClip > 0)
	{
		bHasAmmo |= 1;
	}
	if (!bHasAmmo)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL CBasePlayerWeapon :: DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int skiplocal /* = 0 */, int body )
{
	if (!CanDeploy( ))
		return FALSE;

	// In case we are taunting and we pick up better weapon.
	m_pPlayer->m_fTauntFullTime = gpGlobals->time;

	m_pPlayer->TabulateAmmo();
	if (szWeaponModel == iStringNull)
		m_pPlayer->pev->weaponmodel = 0;
	else
		m_pPlayer->pev->weaponmodel = MAKE_STRING(szWeaponModel);
	m_pPlayer->pev->team = m_iId - 1;
	m_pPlayer->pev->viewmodel = MAKE_STRING(szViewModel);

	strcpy( m_pPlayer->m_szAnimExtention, szAnimExt );
	SendWeaponAnim( iAnim, skiplocal, body );

	float multipler = g_pGameRules->WeaponMultipler();
	m_pPlayer->m_flNextAttack = (UTIL_WeaponTimeBase() + 0.25) * multipler;
	m_flTimeWeaponIdle = (UTIL_WeaponTimeBase() + 1.0) * multipler;
	m_flLastFireTime = 0.0;

	return TRUE;
}

void CBasePlayerWeapon :: DefaultHolster(int iAnim, int body)
{
	m_fInReload = FALSE;// cancel any reload in progress.
	float multipler = g_pGameRules->WeaponMultipler();
	m_pPlayer->m_flNextAttack = (UTIL_WeaponTimeBase() + 0.25) * multipler;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	SendWeaponAnim(iAnim, 1, body);
}


BOOL CBasePlayerWeapon :: DefaultReload( int iClipSize, int iAnim, float fDelay, int body )
{
	if (g_pGameRules->MutatorEnabled(MUTATOR_NORELOAD))
		return FALSE;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		return FALSE;

	int j = fmin(iClipSize - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);	

	if (j == 0)
		return FALSE;

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + fDelay;

	//!!UNDONE -- reload sound goes here !!!
	SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0, body);

	m_fInReload = TRUE;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3;
	return TRUE;
}

BOOL CBasePlayerWeapon :: PlayEmptySound( void )
{
	if (m_iPlayEmptySound)
	{
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "weapons/357_cock1.wav", 0.8, ATTN_NORM);
		m_iPlayEmptySound = 0;
		return 0;
	}
	return 0;
}

void CBasePlayerWeapon :: ResetEmptySound( void )
{
	m_iPlayEmptySound = 1;
}

//=========================================================
//=========================================================
int CBasePlayerWeapon::PrimaryAmmoIndex( void )
{
	return m_iPrimaryAmmoType;
}

//=========================================================
//=========================================================
int CBasePlayerWeapon::SecondaryAmmoIndex( void )
{
	return -1;
}

void CBasePlayerWeapon::Holster( int skiplocal /* = 0 */ )
{
	m_fInReload = FALSE; // cancel any reload in progress.
	m_pPlayer->pev->viewmodel = 0; 
	m_pPlayer->pev->weaponmodel = 0;
}

int CBasePlayerAmmo::TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if ( !pev->takedamage )
		return 0;

	if ( pev->effects & EF_NODRAW )
		return 0;

#ifndef CLIENT_DLL
	if (pev->health > 0 && flDamage > 0)
	{
		pev->takedamage = DAMAGE_NO;

		if (g_pGameRules->IsPropHunt())
		{
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_TAREXPLOSION );
				WRITE_COORD( pev->origin.x );
				WRITE_COORD( pev->origin.y );
				WRITE_COORD( pev->origin.z );
			MESSAGE_END();
			g_pGameRules->MonsterKilled( NULL, pevAttacker );
		}

		if (g_ItemsExplode)
		{
			pev->dmg = RANDOM_LONG(50, 100);
			int iContents = UTIL_PointContents ( pev->origin );
			MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
				WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
				WRITE_COORD( pev->origin.x );	// Send to PAS because of the sound
				WRITE_COORD( pev->origin.y );
				WRITE_COORD( pev->origin.z );
				if (iContents != CONTENTS_WATER)
				{
					if (icesprites.value) {
						WRITE_SHORT( g_sModelIndexIceFireball );
					} else {
						WRITE_SHORT( g_sModelIndexFireball );
					}
				}
				else
				{
					WRITE_SHORT( g_sModelIndexWExplosion );
				}
				WRITE_BYTE( (pev->dmg) * .60  ); // scale * 10
				WRITE_BYTE( 15 ); // framerate
				WRITE_BYTE( TE_EXPLFLAG_NONE );
			MESSAGE_END();
			TraceResult tr;
			UTIL_TraceLine ( pev->origin, pev->origin + Vector ( 0, 0, -128 ), ignore_monsters, ENT(pev), &tr);
			enum decal_e decal = DECAL_SCORCH1;
			int index = RANDOM_LONG(0, 1);
			if (g_pGameRules->MutatorEnabled(MUTATOR_PAINTBALL)) {
				decal = DECAL_PAINTL1;
				index = RANDOM_LONG(0, 7);
			}
			UTIL_DecalTrace(&tr, decal + index);
			::RadiusDamage( pev->origin, pev, pevAttacker, pev->dmg, pev->dmg  * 2.5, CLASS_NONE, DMG_BURN );
		}

		if ( g_pGameRules->AmmoShouldRespawn( this ) == GR_AMMO_RESPAWN_YES )
		{
			Respawn();
		}
		else
		{
			UTIL_Remove( this );
		}
	}
#endif
	return 1;
}

void CBasePlayerAmmo::Spawn( void )
{
	pev->movetype = MOVETYPE_TOSS;

	if (g_pGameRules->IsPropHunt())
	{
		pev->solid = SOLID_BBOX;
		pev->health = 1;
		pev->takedamage = DAMAGE_YES;
		UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);
	}
	else
	{
		pev->solid = SOLID_TRIGGER;
		UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));
	}

	UTIL_SetOrigin( pev, pev->origin );

	pev->sequence = (pev->body * 2) + floatingweapons.value;
	pev->framerate = 1.0;

	if (g_ItemsExplode)
	{
		// Engine does not support traceline with solid_trigger
		// set owner to NULL if want the bbox to explode by own hand
		//pev->solid = SOLID_BBOX;
		//UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 72));
		//pev->owner = NULL;
		pev->takedamage = DAMAGE_YES;
		pev->health = 1;
	}

	SetTouch( &CBasePlayerAmmo::DefaultTouch );
}

CBaseEntity* CBasePlayerAmmo::Respawn( void )
{
	pev->effects |= EF_NODRAW;
	SetTouch( NULL );

	UTIL_SetOrigin( pev, g_pGameRules->VecAmmoRespawnSpot( this ) );// move to wherever I'm supposed to repawn.

	SetThink( &CBasePlayerAmmo::Materialize );
	pev->nextthink = g_pGameRules->FlAmmoRespawnTime( this );

	return this;
}

void CBasePlayerAmmo::Materialize( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		if (g_ItemsExplode)
		{
			pev->health = 1;
			pev->takedamage = DAMAGE_YES;
		}

		// changing from invisible state to visible.
		EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}

	SetTouch( &CBasePlayerAmmo::DefaultTouch );
}

void CBasePlayerAmmo :: DefaultTouch( CBaseEntity *pOther )
{
	// Support if picked up and dropped
	pev->velocity = pev->velocity * 0.5;
	pev->avelocity = pev->avelocity * 0.5;

	if ( !pOther->IsPlayer() )
	{
		return;
	}

	if ( !g_pGameRules->CanHavePlayerAmmo( (CBasePlayer *)pOther, this ) )
	{
		return;
	}

	if (AddAmmo( pOther ))
	{
		if ( g_pGameRules->AmmoShouldRespawn( this ) == GR_AMMO_RESPAWN_YES )
		{
			Respawn();
		}
		else
		{
			SetTouch( NULL );
			SetThink(&CBasePlayerAmmo::SUB_Remove);
			pev->nextthink = gpGlobals->time + .1;
		}
	}
	else if (gEvilImpulse101)
	{
		// evil impulse 101 hack, kill always
		SetTouch( NULL );
		SetThink(&CBasePlayerAmmo::SUB_Remove);
		pev->nextthink = gpGlobals->time + .1;
	}
}

//=========================================================
// called by the new item with the existing item as parameter
//
// if we call ExtractAmmo(), it's because the player is picking up this type of weapon for 
// the first time. If it is spawned by the world, m_iDefaultAmmo will have a default ammo amount in it.
// if  this is a weapon dropped by a dying player, has 0 m_iDefaultAmmo, which means only the ammo in 
// the weapon clip comes along. 
//=========================================================
int CBasePlayerWeapon::ExtractAmmo( CBasePlayerWeapon *pWeapon )
{
	int			iReturn = 0;

	if ( pszAmmo1() != NULL )
	{
		// blindly call with m_iDefaultAmmo. It's either going to be a value or zero. If it is zero,
		// we only get the ammo in the weapon's clip, which is what we want. 
		iReturn = pWeapon->AddPrimaryAmmo( m_iDefaultAmmo, (char *)pszAmmo1(), iMaxClip(), iMaxAmmo1() );
		m_iDefaultAmmo = 0;
	}

	if ( pszAmmo2() != NULL )
	{
		iReturn = pWeapon->AddSecondaryAmmo( 0, (char *)pszAmmo2(), iMaxAmmo2() );
	}

	return iReturn;
}

//=========================================================
// called by the new item's class with the existing item as parameter
//=========================================================
int CBasePlayerWeapon::ExtractClipAmmo( CBasePlayerWeapon *pWeapon )
{
	int			iAmmo;

	if ( m_iClip == WEAPON_NOCLIP )
	{
		iAmmo = 0;// guns with no clips always come empty if they are second-hand
	}
	else
	{
		iAmmo = m_iClip;
	}
	
	return pWeapon->m_pPlayer->GiveAmmo( iAmmo, (char *)pszAmmo1(), iMaxAmmo1() ); // , &m_iPrimaryAmmoType
}
	
//=========================================================
// RetireWeapon - no more ammo for this gun, put it away.
//=========================================================
void CBasePlayerWeapon::RetireWeapon( void )
{
	// first, no viewmodel at all.
	m_pPlayer->pev->viewmodel = iStringNull;
	m_pPlayer->pev->weaponmodel = iStringNull;
	//m_pPlayer->pev->viewmodelindex = NULL;

	g_pGameRules->GetNextBestWeapon( m_pPlayer, this, FALSE, FALSE );
}

//=========================================================================
// GetNextAttackDelay - An accurate way of calcualting the next attack time.
//=========================================================================
float CBasePlayerWeapon::GetNextAttackDelay( float delay ) 
{		
	if(m_flLastFireTime == 0 || m_flNextPrimaryAttack == -1) 
	{
		// At this point, we are assuming that the client has stopped firing
		// and we are going to reset our book keeping variables.
		m_flLastFireTime = gpGlobals->time;
		m_flPrevPrimaryAttack = delay;
	}
	// calculate the time between this shot and the previous
	float flTimeBetweenFires = gpGlobals->time - m_flLastFireTime;
	float flCreep = 0.0f;
	if(flTimeBetweenFires > 0)
			flCreep = flTimeBetweenFires - m_flPrevPrimaryAttack; // postive or negative
	
	// save the last fire time
	m_flLastFireTime = gpGlobals->time;		
	
	float flNextAttack = UTIL_WeaponTimeBase() + delay - flCreep;
	// we need to remember what the m_flNextPrimaryAttack time is set to for each shot, 
	// store it as m_flPrevPrimaryAttack.
	m_flPrevPrimaryAttack = flNextAttack - UTIL_WeaponTimeBase();
// 	char szMsg[256];
// 	_snprintf( szMsg, sizeof(szMsg), "next attack time: %0.4f\n", gpGlobals->time + flNextAttack );
// 	OutputDebugString( szMsg );
	return flNextAttack;
}


void CBasePlayerWeapon::UpdateItemInfo( void )
{
    ItemInfo iInfo;
    memset(&iInfo, 0, sizeof(iInfo));

    if (GetItemInfo(&iInfo) && iInfo.pszDisplayName)
	{
		m_pPlayer->DisplayHudMessage(iInfo.pszDisplayName, TXT_CHANNEL_WEAPON_TITLE, 0.03, 0.88, 210, 210, 210, 0, 0.2, 0.2, 0.75, 0.25);
    }
}

//*********************************************************
// weaponbox code:
//*********************************************************

LINK_ENTITY_TO_CLASS( weaponbox, CWeaponBox );

TYPEDESCRIPTION	CWeaponBox::m_SaveData[] = 
{
	DEFINE_ARRAY( CWeaponBox, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgiszAmmo, FIELD_STRING, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
	DEFINE_FIELD( CWeaponBox, m_cAmmoTypes, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CWeaponBox, CBaseEntity );

//=========================================================
//
//=========================================================
void CWeaponBox::Precache( void )
{
	// PRECACHE_MODEL("models/w_weaponbox.mdl");
}

//=========================================================
//=========================================================
void CWeaponBox :: KeyValue( KeyValueData *pkvd )
{
	if ( m_cAmmoTypes < MAX_AMMO_SLOTS )
	{
		PackAmmo( ALLOC_STRING(pkvd->szKeyName), atoi(pkvd->szValue) );
		m_cAmmoTypes++;// count this new ammo type.

		pkvd->fHandled = TRUE;
	}
	else
	{
		ALERT ( at_console, "WeaponBox too full! only %d ammotypes allowed\n", MAX_AMMO_SLOTS );
	}
}

//=========================================================
// CWeaponBox - Spawn 
//=========================================================
void CWeaponBox::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;

	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	SET_MODEL( ENT(pev), "models/w_weapons.mdl");

	if (g_ItemsExplode)
	{
		pev->health = 1;
		pev->takedamage = DAMAGE_YES;
		//pev->sequence = 1;
		//pev->flags		|= FL_MONSTER;
		//UTIL_SetSize(pev, Vector( -4, -4, -4), Vector(4, 4, 4));	// Uses point-sized, and can be stepped over
		//UTIL_SetOrigin(pev, pev->origin);
	}
}

int CWeaponBox::TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if ( !pev->takedamage )
		return 0;

	if ( pev->effects & EF_NODRAW )
		return 0;

#ifndef CLIENT_DLL
	if (pev->health > 0 && pev->health > 0 && flDamage > 0)
	{
		pev->takedamage = DAMAGE_NO;

		if (g_pGameRules->IsPropHunt())
		{
			MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
				WRITE_BYTE( TE_TAREXPLOSION );
				WRITE_COORD( pev->origin.x );
				WRITE_COORD( pev->origin.y );
				WRITE_COORD( pev->origin.z );
			MESSAGE_END();
			g_pGameRules->MonsterKilled( NULL, pevAttacker );
		}

		if (g_ItemsExplode)
		{
			pev->dmg = RANDOM_LONG(50, 100);
			int iContents = UTIL_PointContents ( pev->origin );
			MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
				WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
				WRITE_COORD( pev->origin.x );	// Send to PAS because of the sound
				WRITE_COORD( pev->origin.y );
				WRITE_COORD( pev->origin.z );
				if (iContents != CONTENTS_WATER)
				{
					if (icesprites.value) {
						WRITE_SHORT( g_sModelIndexIceFireball );
					} else {
						WRITE_SHORT( g_sModelIndexFireball );
					}
				}
				else
				{
					WRITE_SHORT( g_sModelIndexWExplosion );
				}
				WRITE_BYTE( (pev->dmg) * .60  ); // scale * 10
				WRITE_BYTE( 15 ); // framerate
				WRITE_BYTE( TE_EXPLFLAG_NONE );
			MESSAGE_END();
			TraceResult tr;
			UTIL_TraceLine ( pev->origin, pev->origin + Vector ( 0, 0, -128 ), ignore_monsters, ENT(pev), &tr);
			enum decal_e decal = DECAL_SCORCH1;
			int index = RANDOM_LONG(0, 1);
			if (g_pGameRules->MutatorEnabled(MUTATOR_PAINTBALL)) {
				decal = DECAL_PAINTL1;
				index = RANDOM_LONG(0, 7);
			}
			UTIL_DecalTrace(&tr, decal + index);
			::RadiusDamage( pev->origin, pev, pevAttacker, pev->dmg, pev->dmg  * 2.5, CLASS_NONE, DMG_BURN );
		}

		SetThink(&CWeaponBox::Kill);
		pev->nextthink = gpGlobals->time + 0.1;
	}
#endif
	return 1;
}

//=========================================================
// CWeaponBox - Kill - the think function that removes the
// box from the world.
//=========================================================
void CWeaponBox::Kill( void )
{
	CBasePlayerItem *pWeapon;
	int i;

	// destroy the weapons
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pWeapon = m_rgpPlayerItems[ i ];

		while ( pWeapon )
		{
			pWeapon->SetThink(&CBasePlayerItem::SUB_Remove);
			pWeapon->pev->nextthink = gpGlobals->time + 0.1;
			pWeapon = pWeapon->m_pNext;
		}
	}

	// remove the box
	UTIL_Remove( this );
}

//=========================================================
// CWeaponBox - Touch: try to add my contents to the toucher
// if the toucher is a player.
//=========================================================
void CWeaponBox::Touch( CBaseEntity *pOther )
{
	if ( !(pev->flags & FL_ONGROUND ) )
	{
		return;
	}

	if ( !pOther->IsPlayer() )
	{
		// only players may touch a weaponbox.
		return;
	}

	if ( !pOther->IsAlive() )
	{
		// no dead guys.
		return;
	}

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	int i;

// dole out ammo
	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( !FStringNull( m_rgiszAmmo[ i ] ) )
		{
			// there's some ammo of this type. 
			pPlayer->GiveAmmo( m_rgAmmo[ i ], (char *)STRING( m_rgiszAmmo[ i ] ), MaxAmmoCarry( m_rgiszAmmo[ i ] ) );

			//ALERT ( at_console, "Gave %d rounds of %s\n", m_rgAmmo[i], STRING(m_rgiszAmmo[i]) );

			// now empty the ammo from the weaponbox since we just gave it to the player
			m_rgiszAmmo[ i ] = iStringNull;
			m_rgAmmo[ i ] = 0;
		}
	}

// go through my weapons and try to give the usable ones to the player. 
// it's important the the player be given ammo first, so the weapons code doesn't refuse 
// to deploy a better weapon that the player may pick up because he has no ammo for it.
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			CBasePlayerItem *pItem;

			// have at least one weapon in this slot
			while ( m_rgpPlayerItems[ i ] )
			{
				//ALERT ( at_console, "trying to give %s\n", STRING( m_rgpPlayerItems[ i ]->pev->classname ) );

				pItem = m_rgpPlayerItems[ i ];
				m_rgpPlayerItems[ i ] = m_rgpPlayerItems[ i ]->m_pNext;// unlink this weapon from the box

				if ( pPlayer->AddPlayerItem( pItem ) )
				{
					pItem->AttachToPlayer( pPlayer );
					if (g_pGameRules->IsAllowedSingleWeapon(pItem))
						pItem->ProvideSingleItem(pPlayer, STRING(pItem->pev->classname));
				}
			}
		}
	}

	EMIT_SOUND( pOther->edict(), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );
	SetTouch(NULL);
	UTIL_Remove(this);
}

//=========================================================
// CWeaponBox - PackWeapon: Add this weapon to the box
//=========================================================
BOOL CWeaponBox::PackWeapon( CBasePlayerItem *pWeapon )
{
	// is one of these weapons already packed in this box?
	if ( HasWeapon( pWeapon ) )
	{
		return FALSE;// box can only hold one of each weapon type
	}

	if ( pWeapon->m_pPlayer )
	{
		if ( !pWeapon->m_pPlayer->RemovePlayerItem( pWeapon ) )
		{
			// failed to unhook the weapon from the player!
			return FALSE;
		}
	}

	// Don't pack fists
	if ( pWeapon->m_iId == WEAPON_FISTS ) {
		return FALSE;
	}

	// Don't pack nuke
	if ( pWeapon->m_iId == WEAPON_NUKE ) {
		return FALSE;
	}

	int iWeaponSlot = pWeapon->iItemSlot();
	
	if ( m_rgpPlayerItems[ iWeaponSlot ] )
	{
		// there's already one weapon in this slot, so link this into the slot's column
		pWeapon->m_pNext = m_rgpPlayerItems[ iWeaponSlot ];	
		m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
	}
	else
	{
		// first weapon we have for this slot
		m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
		pWeapon->m_pNext = NULL;	
	}

	pWeapon->pev->spawnflags |= SF_NORESPAWN;// never respawn
	pWeapon->pev->movetype = MOVETYPE_NONE;
	pWeapon->pev->solid = SOLID_NOT;
	pWeapon->pev->effects = EF_NODRAW;
	pWeapon->pev->modelindex = 0;
	pWeapon->pev->model = iStringNull;
	pWeapon->pev->owner = edict();
	pWeapon->SetThink( NULL );// crowbar may be trying to swing again, etc.
	pWeapon->SetTouch( NULL );
	pWeapon->m_pPlayer = NULL;

	//ALERT ( at_console, "packed %s\n", STRING(pWeapon->pev->classname) );

	int floating = floatingweapons.value ? 1 : 0;
	pev->sequence = 1;
	pev->animtime = gpGlobals->time;
	pev->framerate = 1.0;

	pev->body = pWeapon->m_iId - 1;
	pev->sequence = ((pWeapon->m_iId - 1) * 2) + floating;

	if (pWeapon->m_iId == WEAPON_SATCHEL)
	{
		SET_MODEL( ENT(pev), "models/w_satchel.mdl");
		pev->sequence = floating;
	}
	else if (pWeapon->m_iId == WEAPON_TRIPMINE)
	{
		SET_MODEL( ENT(pev), "models/v_tripmine.mdl");
		pev->body = 3;
		pev->sequence = floating;
	}
	else if (pWeapon->m_iId == WEAPON_ASHPOD)
	{
		SET_MODEL( ENT(pev), "models/w_portalgun.mdl");
		pev->sequence = floating;
		//SetBodygroup(GET_MODEL_PTR(ENT(pev)), pev, 0, WEAPON_ASHPOD - 1);
		//SetBodygroup(GET_MODEL_PTR(ENT(pev)), pev, 1, WEAPON_ASHPOD - 1);
	}
	else if (pWeapon->m_iId == WEAPON_HANDGRENADE)
	{
		SET_MODEL( ENT(pev), "models/w_grenade.mdl");
		pev->body = 0;
		pev->sequence = floating;
	}

	return TRUE;
}

//=========================================================
// CWeaponBox - PackAmmo
//=========================================================
BOOL CWeaponBox::PackAmmo( int iszName, int iCount )
{
	int iMaxCarry;

	if ( FStringNull( iszName ) )
	{
		// error here
		ALERT ( at_console, "NULL String in PackAmmo!\n" );
		return FALSE;
	}
	
	iMaxCarry = MaxAmmoCarry( iszName );

	if ( iMaxCarry != -1 && iCount > 0 )
	{
		//ALERT ( at_console, "Packed %d rounds of %s\n", iCount, STRING(iszName) );
		GiveAmmo( iCount, (char *)STRING( iszName ), iMaxCarry );
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// CWeaponBox - GiveAmmo
//=========================================================
int CWeaponBox::GiveAmmo( int iCount, char *szName, int iMax, int *pIndex/* = NULL*/ )
{
	int i;

	for (i = 1; i < MAX_AMMO_SLOTS && !FStringNull( m_rgiszAmmo[i] ); i++)
	{
		if (stricmp( szName, STRING( m_rgiszAmmo[i])) == 0)
		{
			if (pIndex)
				*pIndex = i;

			int iAdd = fmin( iCount, iMax - m_rgAmmo[i]);
			if (iCount == 0 || iAdd > 0)
			{
				m_rgAmmo[i] += iAdd;

				return i;
			}
			return -1;
		}
	}
	if (i < MAX_AMMO_SLOTS)
	{
		if (pIndex)
			*pIndex = i;

		m_rgiszAmmo[i] = MAKE_STRING( szName );
		m_rgAmmo[i] = iCount;

		return i;
	}
	ALERT( at_console, "out of named ammo slots\n");
	return i;
}

//=========================================================
// CWeaponBox::HasWeapon - is a weapon of this type already
// packed in this box?
//=========================================================
BOOL CWeaponBox::HasWeapon( CBasePlayerItem *pCheckItem )
{
	CBasePlayerItem *pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];

	while (pItem)
	{
		if (FClassnameIs( pItem->pev, STRING( pCheckItem->pev->classname) ))
		{
			return TRUE;
		}
		pItem = pItem->m_pNext;
	}

	return FALSE;
}

//=========================================================
// CWeaponBox::IsEmpty - is there anything in this box?
//=========================================================
BOOL CWeaponBox::IsEmpty( void )
{
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			return FALSE;
		}
	}

	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( !FStringNull( m_rgiszAmmo[ i ] ) )
		{
			// still have a bit of this type of ammo
			return FALSE;
		}
	}

	return TRUE;
}

//=========================================================
//=========================================================
void CWeaponBox::SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector(-16, -16, 0);
	pev->absmax = pev->origin + Vector(16, 16, 16); 
}


void CBasePlayerWeapon::PrintState( void )
{
	ALERT( at_console, "primary:  %f\n", m_flNextPrimaryAttack );
	ALERT( at_console, "idle   :  %f\n", m_flTimeWeaponIdle );

//	ALERT( at_console, "nextrl :  %f\n", m_flNextReload );
//	ALERT( at_console, "nextpum:  %f\n", m_flPumpTime );

//	ALERT( at_console, "m_frt  :  %f\n", m_fReloadTime );
	ALERT( at_console, "m_finre:  %i\n", m_fInReload );
//	ALERT( at_console, "m_finsr:  %i\n", m_fInSpecialReload );

	ALERT( at_console, "m_iclip:  %i\n", m_iClip );
}

void CBasePlayerWeapon::ThrowGrenade(BOOL m_iCheckAmmo)
{
	// Bot crash
	if (!m_pPlayer)
		return;

	if (m_pPlayer->m_fGrenadeTime >= gpGlobals->time) {
		return;
	}

	if (m_iCheckAmmo)
	{
		int index = m_pPlayer->GetAmmoIndex("Hand Grenade");

		if (m_pPlayer->m_rgAmmo[index] <= 0)
			return;

		m_pPlayer->m_rgAmmo[index]--;
	}

	m_pPlayer->SetAnimation( PLAYER_PUNCH );
	m_pPlayer->m_fGrenadeTime = gpGlobals->time + (0.75 * g_pGameRules->WeaponMultipler());

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

	// alway explode seconds after the pin was pulled
	float time = RANDOM_FLOAT(4.0, 6.0);
	CGrenade::ShootTimedCluster(m_pPlayer->pev, vecSrc, vecThrow, time);

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_BODY, "grenade_throw.wav", 1, ATTN_NORM);

	m_pPlayer->pev->punchangle = Vector(-4, -2, -4);
}

void CBasePlayerWeapon::ThrowSnowball(BOOL m_iCheckAmmo)
{
	if (m_pPlayer->m_fGrenadeTime >= gpGlobals->time) {
		return;
	}

	if (m_iCheckAmmo)
	{
		int index = m_pPlayer->GetAmmoIndex("Hand Grenade");

		if (m_pPlayer->m_rgAmmo[index] <= 0)
			return;

		m_pPlayer->m_rgAmmo[index]--;
	}

	m_pPlayer->m_fGrenadeTime = gpGlobals->time + 0.75;

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

	// alway explode seconds after the pin was pulled
	CFlyingSnowball::Shoot(m_pPlayer->pev, vecSrc, vecThrow, m_pPlayer);

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_pPlayer->pev->punchangle = Vector(-4, -2, -4);
}

void CBasePlayerWeapon::ThrowRocket(BOOL m_iCheckAmmo)
{
	if (m_pPlayer->m_fGrenadeTime >= gpGlobals->time) {
		return;
	}

	if (m_iCheckAmmo)
	{
		int index = m_pPlayer->GetAmmoIndex("rockets");

		if (m_pPlayer->m_rgAmmo[index] <= 0)
			return;

		m_pPlayer->m_rgAmmo[index]--;
	}

	m_pPlayer->m_fGrenadeTime = gpGlobals->time + 0.75;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecAiming = gpGlobals->v_forward;
	Vector vecSrc = m_pPlayer->GetGunPosition( ) + (vecAiming * 16) + gpGlobals->v_right * 12 + gpGlobals->v_up * -8;

	CBaseEntity *pRocket = CRpgRocket::CreateRpgRocket( vecSrc, vecAiming, m_pPlayer, 0.0, FALSE );

	if (pRocket)
		pRocket->pev->velocity = pRocket->pev->velocity + vecAiming * DotProduct( m_pPlayer->pev->velocity, vecAiming );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_pPlayer->pev->punchangle = Vector(-4, -2, -4);
}

enum punch_e {
	IDLE1 = 0,
	DEPLOY,
	HOLSTER,
	RIGHT_HIT,
	RIGHT_MISS,
	LEFT_MISS
};

void CBasePlayerWeapon::StartPunch( BOOL holdingSomething )
{
	if (!CanPunch() || m_pPlayer == NULL) {
		return;
	}

	if (m_pPlayer->m_fOffhandTime >= gpGlobals->time)
		return;

	if (m_pPlayer->m_fForceGrabTime >= gpGlobals->time)
		return;

	if (!CanAttack( m_flNextPrimaryAttack, gpGlobals->time, UseDecrement() )) {
		return;
	}

	// Prop limitation
	if ( g_pGameRules->IsPropHunt() && m_pPlayer->pev->fuser4 > 0 )
		return;

	Holster();
	m_pPlayer->m_fOffhandTime = gpGlobals->time + 0.55;
	PunchAttack(holdingSomething);
}

void CBasePlayerWeapon::PunchAttack( BOOL holdingSomething )
{
	m_pPlayer->pev->viewmodel = MAKE_STRING("models/v_fists.mdl");

	TraceResult tr;
	UTIL_MakeVectors (m_pPlayer->pev->v_angle);
	Vector vecSrc	= m_pPlayer->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 96;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	m_pPlayer->pev->punchangle = Vector(-10, 0, 0);

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

	// Ignore all of this if holding something
	if (!holdingSomething) {
		CBaseEntity *pObject = NULL;
		CBaseEntity *pClosest = NULL;
		Vector		vecLOS;
		float flMaxDot = VIEW_FIELD_ULTRA_NARROW;
		float flDot;

		while ((pObject = UTIL_FindEntityInSphere( pObject, pev->origin, pev->flags & FL_FAKECLIENT ? 192 : 64 )) != NULL)
		{
			if (strstr(interactiveitems.string, STRING(pObject->pev->classname)) != NULL)
			{
				// allow bots to easily interact
				if (pev->flags & FL_FAKECLIENT)
				{
					pClosest = pObject;
				}
				vecLOS = (VecBModelOrigin( pObject->pev ) - (pev->origin + pev->view_ofs));
				vecLOS = UTIL_ClampVectorToBox( vecLOS, pObject->pev->size * 0.5 );
				flDot = DotProduct (vecLOS , gpGlobals->v_forward);
				if (flDot > flMaxDot)
				{
					pClosest = pObject;
					flMaxDot = flDot;
				}
			}
		}

		if ( pClosest ) {
			UTIL_MakeVectors( m_pPlayer->pev->v_angle );
			Vector smoke = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 25;
			CSprite *pSprite = CSprite::SpriteCreate("sprites/gunsmoke.spr", smoke, TRUE);
			if (pSprite != NULL)
			{
				pSprite->AnimateAndDie(12);
				pSprite->SetTransparency(kRenderTransAdd, 255, 255, 255, 80, kRenderFxNoDissipation);
				pSprite->SetScale(0.4);
			}

			pClosest->pev->velocity = gpGlobals->v_forward * RANDOM_LONG(400,600) + gpGlobals->v_up * RANDOM_LONG(300,500);
			pClosest->pev->gravity = 0.5;
			pClosest->pev->friction = 0.5;
			m_pPlayer->pev->punchangle = Vector(-4, -2, -4);
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_BODY, "fists_hit.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		}
	}
#endif

	switch( RANDOM_LONG(0,1) )
	{
	case 0:
		SendWeaponAnim( LEFT_MISS ); break;
	case 1:
		SendWeaponAnim( RIGHT_MISS ); break;
	}

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_PUNCH );

	// play thwack, smack, or dong sound
	float flVol = 1.0;
	int fHitWorld = TRUE;

	if ( tr.flFraction >= 1.0 )
	{
		if (holdingSomething)
		{
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_BODY, "fists_hitbod.wav", 1, ATTN_NORM);
			m_pPlayer->m_iWeaponVolume = 128;
			flVol = 0.1;
			fHitWorld = FALSE;

			UTIL_MakeVectors( m_pPlayer->pev->v_angle );
			Vector smoke = tr.vecEndPos - gpGlobals->v_forward * 10;
			CSprite *pSprite = CSprite::SpriteCreate( "sprites/gunsmoke.spr", smoke, TRUE );
			if (pSprite != NULL)
			{
				pSprite->AnimateAndDie( 12 );
				pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 80, kRenderFxNoDissipation );
				pSprite->SetScale( 0.4 );
			}
		} else {
			// miss
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_BODY, "fists_miss.wav", 1, ATTN_NORM);
		}
	}
	else
	{

#ifndef CLIENT_DLL

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);
		ClearMultiDamage( );

		pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgFists, gpGlobals->v_forward, &tr, DMG_PUNCH | DMG_NEVERGIB); 

		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// play thwack or smack sound
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_BODY, "fists_hitbod.wav", 1, ATTN_NORM);
				m_pPlayer->m_iWeaponVolume = 128;
				flVol = 0.1;
				pEntity->pev->velocity = (pEntity->pev->velocity + (gpGlobals->v_forward * RANDOM_LONG(100,200)));

				if (g_pGameRules->AllowMeleeDrop() && pEntity->IsPlayer())
				{
					CBasePlayer *pl = (CBasePlayer *)pEntity;
					if (pl->m_pActiveItem)
					{
						CBasePlayerItem *item = pl->m_pActiveItem;
						const char *name = STRING(item->pev->classname);
						if (strcmp(name, "weapon_fists"))
							pl->DropPlayerItem((char *)name);
					}
				}

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_FIST);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// Add smoke
			UTIL_MakeVectors( m_pPlayer->pev->v_angle );
			m_pPlayer->pev->velocity = m_pPlayer->pev->velocity + (gpGlobals->v_forward * -300);
			Vector smoke = tr.vecEndPos - gpGlobals->v_forward * 10;
			CSprite *pSprite = CSprite::SpriteCreate( "sprites/gunsmoke.spr", smoke, TRUE );
			if (pSprite != NULL)
			{
				pSprite->AnimateAndDie( 12 );
				pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 80, kRenderFxNoDissipation );
				pSprite->SetScale( 0.4 );
			}

			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_BODY, "fists_hit.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		}

		m_pPlayer->m_iWeaponVolume = flVol * 512;
#endif
	}

	// delay the decal a bit
	m_trBootHit = tr;

	SetThink( &CBasePlayerWeapon::EndPunch );
	pev->nextthink = gpGlobals->time + (0.28 * g_pGameRules->WeaponMultipler());

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = (GetNextAttackDelay(0.3) * g_pGameRules->WeaponMultipler());
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CBasePlayerWeapon::EndPunch( void )
{
	if (m_trBootHit.flFraction < 1.0) {
		DecalGunshot( &m_trBootHit, BULLET_PLAYER_FIST );
	}

	DeployLowKey();
}

void CBasePlayerWeapon::StartKick( BOOL holdingSomething )
{
	if (!CanKick() || m_pPlayer == NULL) {
		return;
	}

	if (m_pPlayer->m_fOffhandTime >= gpGlobals->time)
		return;

	if (m_pPlayer->m_fForceGrabTime >= gpGlobals->time)
		return;

	if ( FBitSet( m_pPlayer->pev->flags, FL_DUCKING ) ) {
		return;
	}

	// Prop limitation
	if ( g_pGameRules->IsPropHunt() && m_pPlayer->pev->fuser4 > 0 )
		return;

	m_pPlayer->m_EFlags &= ~EFLAG_CANCEL;
	m_pPlayer->m_EFlags |= EFLAG_PLAYERKICK;
	m_pPlayer->m_fOffhandTime = (gpGlobals->time + 0.55 * g_pGameRules->WeaponMultipler());
	KickAttack(holdingSomething);
}

void CBasePlayerWeapon::KickAttack( BOOL holdingSomething )
{
	TraceResult tr;
	UTIL_MakeVectors (m_pPlayer->pev->v_angle);
	Vector vecSrc	= m_pPlayer->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 96;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	m_pPlayer->pev->punchangle = Vector(-10, 0, 0);

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

	// Ignore all of this if holding something
	if (!holdingSomething) {
		CBaseEntity *pObject = NULL;
		CBaseEntity *pClosest = NULL;
		Vector		vecLOS;
		float flMaxDot = VIEW_FIELD_NARROW;
		float flDot;

		while ((pObject = UTIL_FindEntityInSphere( pObject, pev->origin, pev->flags & FL_FAKECLIENT ? 192 : 96 )) != NULL)
		{
			if (strstr(interactiveitems.string, STRING(pObject->pev->classname)) != NULL)
			{
				// allow bots to easily interact
				if (pev->flags & FL_FAKECLIENT)
				{
					pClosest = pObject;
				}
				vecLOS = (VecBModelOrigin( pObject->pev ) - (pev->origin + pev->view_ofs));
				vecLOS = UTIL_ClampVectorToBox( vecLOS, pObject->pev->size * 0.5 );
				flDot = DotProduct (vecLOS , gpGlobals->v_forward);
				if (flDot > flMaxDot)
				{
					pClosest = pObject;
					flMaxDot = flDot;
				}
			}
		}

		if ( pClosest ) {
			UTIL_MakeVectors( m_pPlayer->pev->v_angle );
			Vector smoke = m_pPlayer->GetGunPosition() + gpGlobals->v_forward * 25;
			CSprite *pSprite = CSprite::SpriteCreate("sprites/gunsmoke.spr", smoke, TRUE);
			if (pSprite != NULL)
			{
				pSprite->AnimateAndDie(12);
				pSprite->SetTransparency(kRenderTransAdd, 255, 255, 255, 80, kRenderFxNoDissipation);
				pSprite->SetScale(0.4);
			}

			pClosest->pev->velocity = gpGlobals->v_forward * RANDOM_LONG(600,800) + gpGlobals->v_up * RANDOM_LONG(400,600);
			pClosest->pev->gravity = 0.5;
			pClosest->pev->friction = 0.5;
			m_pPlayer->pev->punchangle = Vector(-4, -2, -4);
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_BODY, "fists_hit.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		}
	}
#endif

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_KICK );

	float flVol = 1.0;
	int fHitWorld = TRUE;

	if ( tr.flFraction >= 1.0 )
	{
		if (holdingSomething)
		{
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_BODY, "fists_hitbod.wav", 1, ATTN_NORM);
			m_pPlayer->m_iWeaponVolume = 128;
			flVol = 0.1;
			fHitWorld = FALSE;

			UTIL_MakeVectors( m_pPlayer->pev->v_angle );
			Vector smoke = tr.vecEndPos - gpGlobals->v_forward * 10;
			CSprite *pSprite = CSprite::SpriteCreate( "sprites/gunsmoke.spr", smoke, TRUE );
			if (pSprite != NULL)
			{
				pSprite->AnimateAndDie( 12 );
				pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 80, kRenderFxNoDissipation );
				pSprite->SetScale( 0.4 );
			}
		} else {
			// miss
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_BODY, "kick.wav", 1, ATTN_NORM);
		}
	}
	else
	{

#ifndef CLIENT_DLL

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);
		ClearMultiDamage( );

		pEntity->TraceAttack(m_pPlayer->pev, gSkillData.plrDmgKick, gpGlobals->v_forward, &tr, DMG_KICK | DMG_NEVERGIB ); 

		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		if (g_pGameRules->MutatorEnabled(MUTATOR_VOLATILE)) {
			::RadiusDamage( tr.vecEndPos, m_pPlayer->pev, m_pPlayer->pev, gSkillData.plrDmgKick, 75, CLASS_NONE, DMG_KICK, TRUE );
			int iContents = UTIL_PointContents ( tr.vecEndPos );
			MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, tr.vecEndPos );
				WRITE_BYTE( TE_EXPLOSION );		// This makes a dynamic light and the explosion sprites/sound
				WRITE_COORD( tr.vecEndPos.x );	// Send to PAS because of the sound
				WRITE_COORD( tr.vecEndPos.y );
				WRITE_COORD( tr.vecEndPos.z );
				if (iContents != CONTENTS_WATER)
				{
					if (icesprites.value) {
						WRITE_SHORT( g_sModelIndexIceFireball );
					} else {
						WRITE_SHORT( g_sModelIndexFireball );
					}
				}
				else
				{
					WRITE_SHORT( g_sModelIndexWExplosion );
				}
				WRITE_BYTE( gSkillData.plrDmgKick * .60 ); // scale * 10
				WRITE_BYTE( 15 ); // framerate
				WRITE_BYTE( TE_EXPLFLAG_NONE );
			MESSAGE_END();
		}

		int speed = -300;
		if ( !FNullEnt(tr.pHit) && VARS(tr.pHit)->rendermode != 0)
			speed = 300;

		// play thwack, smack, or dong sound
		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// play thwack or smack sound
				EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_BODY, "fists_hitbod.wav", 1, ATTN_NORM);
				m_pPlayer->m_iWeaponVolume = 128;
				flVol = 0.1;
				pEntity->pev->flags &= ~FL_ONGROUND;
				pEntity->pev->velocity = (pEntity->pev->velocity + (gpGlobals->v_forward * RANDOM_LONG(200,300)) + gpGlobals->v_up);

				if (g_pGameRules->AllowMeleeDrop() && pEntity->IsPlayer())
				{
					CBasePlayer *pl = (CBasePlayer *)pEntity;
					if (pl->m_pActiveItem)
					{
						CBasePlayerItem *item = pl->m_pActiveItem;
						const char *name = STRING(item->pev->classname);
						if (strcmp(name, "weapon_fists"))
							pl->DropPlayerItem((char *)name);
					}
				}

				fHitWorld = FALSE;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_BOOT);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// Add smoke
			UTIL_MakeVectors( m_pPlayer->pev->v_angle );
			m_pPlayer->pev->velocity = m_pPlayer->pev->velocity + (gpGlobals->v_forward * speed);
			Vector smoke = tr.vecEndPos - gpGlobals->v_forward * 10;
			CSprite *pSprite = CSprite::SpriteCreate( "sprites/gunsmoke.spr", smoke, TRUE );
			if (pSprite != NULL)
			{
				pSprite->AnimateAndDie( 12 );
				pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 80, kRenderFxNoDissipation );
				pSprite->SetScale( 0.4 );
			}

			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_BODY, "fists_hit.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
		}

		m_pPlayer->m_iWeaponVolume = flVol * 512;
#endif
	}

	// delay the decal a bit
	m_trBootHit = tr;

	m_pPlayer->m_fKickEndTime = gpGlobals->time + (0.28 * g_pGameRules->WeaponMultipler());
}

void CBasePlayerWeapon::EndKick( void )
{
	if (m_trBootHit.flFraction < 1.0) {
		DecalGunshot( &m_trBootHit, BULLET_PLAYER_BOOT );
	}

	m_pPlayer->m_EFlags &= ~EFLAG_PLAYERKICK;
}

class CThrowWeapon : public CBaseEntity
{
public:
	void Precache ( void );
	void Spawn( void );
	void EXPORT ThrowWeaponThink( void );
	void EXPORT ThrowWeaponTouch( CBaseEntity *pOther );
	virtual int ObjectCaps( void ) { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_PORTAL; }

private:
	EHANDLE m_hOwner;
};

LINK_ENTITY_TO_CLASS( monster_throwweapon, CThrowWeapon );

void CThrowWeapon::Precache( void )
{
	PRECACHE_MODEL("models/w_weapons.mdl");
}

void CThrowWeapon::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_BOUNCE;
	pev->gravity = 0.5;
	pev->solid = SOLID_TRIGGER;
	pev->health = 1;
	pev->takedamage = DAMAGE_NO;
	pev->classname = MAKE_STRING("monster_throwweapon");

	SET_MODEL( edict(), "models/w_weapons.mdl");

	UTIL_SetSize(pev, Vector( -4,-4,-4 ), Vector(4, 4, 4));

	if ( pev->owner )
		m_hOwner = Instance(pev->owner);
	pev->owner = NULL;

	SetThink(&CThrowWeapon::ThrowWeaponThink);
	SetTouch(&CThrowWeapon::ThrowWeaponTouch);

	pev->nextthink = gpGlobals->time + 2.0;
}

void CThrowWeapon::ThrowWeaponTouch( CBaseEntity *pOther )
{
	// Support if picked up and dropped
	pev->velocity = pev->velocity * 0.8;
	pev->avelocity = pev->avelocity * 0.8;
}

void CThrowWeapon::ThrowWeaponThink( void )
{
	if (m_hOwner)
	{
		CGrenade::Vest( m_hOwner->pev, pev->origin, gSkillData.plrDmgVest / 2 );
	}

	pev->nextthink = -1;
	SetThink(NULL);
	UTIL_Remove(this);
}

void CBasePlayerWeapon::ThrowWeapon( BOOL holdingSomething )
{
	if (!CanKick() || m_pPlayer == NULL) {
		return;
	}

	if (m_iId == WEAPON_FISTS || m_iId == WEAPON_FINGERGUN)
		return;

	if (m_pPlayer->m_fOffhandTime >= gpGlobals->time)
		return;

	if (g_pGameRules->IsGunGame() || g_pGameRules->IsInstagib())
	{
		ClientPrint(m_pPlayer->pev, HUD_PRINTCENTER, "Throw weapon is disabled in this gamemode.");
		m_pPlayer->m_fOffhandTime = gpGlobals->time + 0.5;
		return;
	}

	if (m_pPlayer->m_fForceGrabTime >= gpGlobals->time)
		return;

	m_pPlayer->SetAnimation( PLAYER_PUNCH );
	m_pPlayer->m_EFlags &= ~EFLAG_CANCEL;
	m_pPlayer->m_EFlags |= EFLAG_THROW;
	m_pPlayer->m_fOffhandTime = (gpGlobals->time + 0.55 * g_pGameRules->WeaponMultipler());

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	CBaseEntity *m_Banana = CBaseEntity::Create("monster_throwweapon", vecSrc + gpGlobals->v_forward * 32, Vector(-90, pev->angles.y + 90, -90), m_pPlayer->edict());
	if (m_Banana && m_pPlayer->m_pActiveItem)
	{
		m_Banana->pev->velocity = gpGlobals->v_forward * RANDOM_LONG(400,600);
		m_Banana->pev->body = m_pPlayer->m_pActiveItem->m_iId - 1;
		m_pPlayer->DropPlayerItem("", FALSE, TRUE);
	}
}

void CBasePlayerWeapon::ProvideDualItem(CBasePlayer *pPlayer, const char *pszName) {
	if (pPlayer->m_iShownDualMessage != 1 && pszName != NULL &&
		(stricmp(pszName, "weapon_wrench") == 0 ||
		stricmp(pszName, "weapon_deagle") == 0 ||
		stricmp(pszName, "weapon_smg") == 0 ||
		stricmp(pszName, "weapon_usas") == 0 ||
		stricmp(pszName, "weapon_rpg") == 0 ||
		stricmp(pszName, "weapon_chaingun") == 0 ||
		stricmp(pszName, "weapon_hgun") == 0
		)) {
		ClientPrint( pPlayer->pev, HUD_PRINTTALK, "You picked up a dual weapon. Swap between dual and single using \"impulse 205\".\n" );
		pPlayer->m_iShownDualMessage = 1;
	}
}

void CBasePlayerWeapon::EjectBrassLate()
{
	Vector vecUp, vecRight, vecShellVelocity;
	int m_iShell = PRECACHE_MODEL("models/w_shell.mdl");

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

	vecUp = RANDOM_FLOAT(50, 70) * gpGlobals->v_up;
	vecRight = RANDOM_FLOAT(-100, -130) * gpGlobals->v_right;

	vecShellVelocity = (m_pPlayer->pev->velocity + vecRight + vecUp) + gpGlobals->v_forward * 25;

	EjectBrass(pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_up * -9 + gpGlobals->v_forward * 20 + gpGlobals->v_right * 8,
		vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL);
}

void CBasePlayerWeapon::EjectShotShellLate()
{
	Vector vecUp, vecRight, vecShellVelocity;
	int m_iShell = PRECACHE_MODEL("models/w_shotgunshell.mdl");

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

	vecUp = RANDOM_FLOAT(50, 70) * gpGlobals->v_up;
	vecRight = RANDOM_FLOAT(-100, -130) * gpGlobals->v_right;

	vecShellVelocity = (m_pPlayer->pev->velocity + vecRight + vecUp) + gpGlobals->v_forward * 25;

	EjectBrass(pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_up * -9 + gpGlobals->v_forward * 14 + gpGlobals->v_right * 4,
		vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHOTSHELL);
}

void CBasePlayerWeapon::WeaponPickup(CBasePlayer *pPlayer, int weaponId) {
	if ( pPlayer->pev->flags & FL_FAKECLIENT ) // No bots
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
		WRITE_BYTE( weaponId );
	MESSAGE_END();
}

TYPEDESCRIPTION	CRpg::m_SaveData[] = 
{
	DEFINE_FIELD( CRpg, m_fSpotActive, FIELD_INTEGER ),
	DEFINE_FIELD( CRpg, m_cActiveRockets, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CRpg, CBasePlayerWeapon );

TYPEDESCRIPTION	CRpgRocket::m_SaveData[] = 
{
	DEFINE_FIELD( CRpgRocket, m_flIgniteTime, FIELD_TIME ),
	DEFINE_FIELD( CRpgRocket, m_pLauncher, FIELD_CLASSPTR ),
};
IMPLEMENT_SAVERESTORE( CRpgRocket, CGrenade );

TYPEDESCRIPTION	CShotgun::m_SaveData[] = 
{
	DEFINE_FIELD( CShotgun, m_flNextReload, FIELD_TIME ),
	DEFINE_FIELD( CShotgun, m_fInSpecialReload, FIELD_INTEGER ),
	DEFINE_FIELD( CShotgun, m_flNextReload, FIELD_TIME ),
	// DEFINE_FIELD( CShotgun, m_iShell, FIELD_INTEGER ),
	DEFINE_FIELD( CShotgun, m_flPumpTime, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CShotgun, CBasePlayerWeapon );

TYPEDESCRIPTION	CGauss::m_SaveData[] = 
{
	DEFINE_FIELD( CGauss, m_fInAttack, FIELD_INTEGER ),
//	DEFINE_FIELD( CGauss, m_flStartCharge, FIELD_TIME ),
//	DEFINE_FIELD( CGauss, m_flPlayAftershock, FIELD_TIME ),
//	DEFINE_FIELD( CGauss, m_flNextAmmoBurn, FIELD_TIME ),
	DEFINE_FIELD( CGauss, m_fPrimaryFire, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CGauss, CBasePlayerWeapon );

TYPEDESCRIPTION	CEgon::m_SaveData[] = 
{
//	DEFINE_FIELD( CEgon, m_pBeam, FIELD_CLASSPTR ),
//	DEFINE_FIELD( CEgon, m_pNoise, FIELD_CLASSPTR ),
//	DEFINE_FIELD( CEgon, m_pSprite, FIELD_CLASSPTR ),
	DEFINE_FIELD( CEgon, m_shootTime, FIELD_TIME ),
	DEFINE_FIELD( CEgon, m_fireState, FIELD_INTEGER ),
	DEFINE_FIELD( CEgon, m_fireMode, FIELD_INTEGER ),
	DEFINE_FIELD( CEgon, m_shakeTime, FIELD_TIME ),
	DEFINE_FIELD( CEgon, m_flAmmoUseTime, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CEgon, CBasePlayerWeapon );

TYPEDESCRIPTION	CSatchel::m_SaveData[] = 
{
	DEFINE_FIELD( CSatchel, m_chargeReady, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CSatchel, CBasePlayerWeapon );

