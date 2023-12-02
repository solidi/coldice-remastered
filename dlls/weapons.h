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
#ifndef WEAPONS_H
#define WEAPONS_H

#include "effects.h"

class CBasePlayer;
extern int gmsgWeapPickup;

void DeactivateSatchels( CBasePlayer *pOwner );
void DeactivateAssassins( CBasePlayer *pOwner );
void DeactivatePortals( CBasePlayer *pOwner );

// Contact Grenade / Timed grenade / Satchel Charge
class CGrenade : public CBaseMonster
{
public:
	void Spawn( void );

	typedef enum { SATCHEL_DETONATE = 0, SATCHEL_RELEASE } SATCHELCODE;

	static CGrenade *ShootTimed( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, float time );
	static CGrenade *ShootContact( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	static CGrenade *ShootSatchelCharge( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	static CGrenade *Vest( entvars_t *pevOwner, Vector vecStart );
	static CGrenade *ShootTimedCluster( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, float time );

	void EXPORT ClusterTumbleThink( void );
	void EXPORT ClusterDetonate( void );

	static void UseSatchelCharges( entvars_t *pevOwner, SATCHELCODE code );

	void Explode( Vector vecSrc, Vector vecAim );
	void Explode( TraceResult *pTrace, int bitsDamageType );
	void EXPORT Smoke( void );

	void EXPORT BounceTouch( CBaseEntity *pOther );
	void EXPORT SlideTouch( CBaseEntity *pOther );
	void EXPORT ExplodeTouch( CBaseEntity *pOther );
	void EXPORT DangerSoundThink( void );
	void EXPORT PreDetonate( void );
	void EXPORT Detonate( void );
	void EXPORT DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT TumbleThink( void );

	virtual void BounceSound( void );
	virtual int	BloodColor( void ) { return DONT_BLEED; }
	virtual void Killed( entvars_t *pevAttacker, int iGib );

	virtual int ObjectCaps( void ) { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_PORTAL; }

	BOOL m_fRegisteredSound;// whether or not this grenade has issued its DANGER sound to the world sound list yet.
};


// constant items
#define ITEM_HEALTHKIT		1
#define ITEM_ANTIDOTE		2
#define ITEM_SECURITY		3
#define ITEM_BATTERY		4

#define WEAPON_NONE					0
#define WEAPON_CROWBAR				1
#define WEAPON_KNIFE				2
#define WEAPON_WRENCH				3
#define WEAPON_CHAINSAW				4
#define WEAPON_ROCKETCROWBAR		5
#define WEAPON_GRAVITYGUN			6
#define WEAPON_ASHPOD				7
#define	WEAPON_GLOCK				8
#define WEAPON_PYTHON				9
#define	WEAPON_DEAGLE				10
#define WEAPON_MAG60				11
#define WEAPON_SMG					12
#define WEAPON_SAWEDOFF				13
#define WEAPON_MP5					14
#define WEAPON_12GAUGE				15
#define WEAPON_SHOTGUN				16
#define WEAPON_USAS					17
#define WEAPON_CROSSBOW				18
#define WEAPON_SNIPER_RIFLE			19
#define WEAPON_CHAINGUN				20
#define WEAPON_FREEZEGUN			21
#define WEAPON_RPG					22
#define WEAPON_GAUSS				23
#define WEAPON_EGON					24
#define WEAPON_HORNETGUN			25
#define WEAPON_RAILGUN				26
#define WEAPON_GLAUNCHER			27
#define WEAPON_CANNON				28
#define WEAPON_NUKE					29
#define WEAPON_SNOWBALL				30
#define WEAPON_SUIT					31
#define WEAPON_NONE_32				32
#define WEAPON_HANDGRENADE			33
#define	WEAPON_SATCHEL				34
#define WEAPON_TRIPMINE				35
#define	WEAPON_SNARK				36
#define	WEAPON_CHUMTOAD				37
#define	WEAPON_VEST					38
#define WEAPON_FLAMETHROWER			39
#define WEAPON_DUAL_WRENCH			40
#define WEAPON_DUAL_DEAGLE			41
#define WEAPON_DUAL_MAG60			42
#define WEAPON_DUAL_SMG				43
#define WEAPON_DUAL_SAWEDOFF		44
#define WEAPON_DUAL_USAS			45
#define WEAPON_DUAL_RAILGUN			46
#define WEAPON_DUAL_RPG				47
#define WEAPON_DUAL_FLAMETHROWER	48
#define WEAPON_FISTS				49

#define WEAPON_ALLWEAPONS		(~(1<<WEAPON_SUIT))

#define MAX_WEAPONS			64


#define MAX_NORMAL_BATTERY	100


// weapon weight factors (for auto-switching)   (-1 = noswitch)
#define FISTS_WEIGHT			0
#define CROWBAR_WEIGHT			1
#define KNIFE_WEIGHT			2
#define WRENCH_WEIGHT			3
#define CHAINSAW_WEIGHT			4
#define GRAVITYGUN_WEIGHT		5
#define ROCKETCROWBAR_WEIGHT 	6
#define ASHPOD_WEIGHT			7
#define SNOWBALL_WEIGHT			10
#define GLOCK_WEIGHT			11
#define PYTHON_WEIGHT			12
#define DEAGLE_WEIGHT			13
#define MAG60_WEIGHT			14
#define SMG_WEIGHT				15
#define SAWEDOFF_WEIGHT			16
#define MP5_WEIGHT				20
#define GAUGE_SHOTGUN_WEIGHT 	21
#define SHOTGUN_WEIGHT			22
#define USAS_WEIGHT				23
#define CROSSBOW_WEIGHT			24
#define SNIPERRIFLE_WEIGHT		25
#define CHAINGUN_WEIGHT			26
#define FREEZEGUN_WEIGHT		27
#define RPG_WEIGHT				31
#define GAUSS_WEIGHT			32
#define EGON_WEIGHT				33
#define HORNETGUN_WEIGHT		34
#define RAILGUN_WEIGHT			35
#define GLAUNCHER_WEIGHT		36
#define CANNON_WEIGHT			37
#define NUKE_WEIGHT 			38
#define HANDGRENADE_WEIGHT		10
#define SATCHEL_WEIGHT			-10
#define TRIPMINE_WEIGHT			-10
#define SNARK_WEIGHT			10
#define VEST_WEIGHT				40
#define FLAMETHROWER_WEIGHT		37

// weapon clip/carry ammo capacities
#define URANIUM_MAX_CARRY		250
#define	_9MM_MAX_CARRY			250
#define _357_MAX_CARRY			250
#define BUCKSHOT_MAX_CARRY		250
#define BOLT_MAX_CARRY			250
#define ROCKET_MAX_CARRY		20
#define HANDGRENADE_MAX_CARRY	20
#define SATCHEL_MAX_CARRY		20
#define TRIPMINE_MAX_CARRY		20
#define SNARK_MAX_CARRY			20
#define HORNET_MAX_CARRY		8
#define M203_GRENADE_MAX_CARRY	20
#define SNOWBALL_MAX_CARRY		20
#define NUKE_MAX_CARRY			3
#define FUEL_MAX_CARRY			250

// the maximum amount of ammo each weapon's clip can hold
#define WEAPON_NOCLIP			-1

//#define CROWBAR_MAX_CLIP		WEAPON_NOCLIP
#define GLOCK_MAX_CLIP			17
#define PYTHON_MAX_CLIP			6
#define MP5_MAX_CLIP			50
#define MP5_DEFAULT_AMMO		50
#define SHOTGUN_MAX_CLIP		8
#define CROSSBOW_MAX_CLIP		30
#define RPG_MAX_CLIP			5
#define GAUSS_MAX_CLIP			WEAPON_NOCLIP
#define EGON_MAX_CLIP			WEAPON_NOCLIP
#define HORNETGUN_MAX_CLIP		WEAPON_NOCLIP
#define HANDGRENADE_MAX_CLIP	WEAPON_NOCLIP
#define SATCHEL_MAX_CLIP		WEAPON_NOCLIP
#define TRIPMINE_MAX_CLIP		WEAPON_NOCLIP
#define SNARK_MAX_CLIP			WEAPON_NOCLIP
#define CANNON_MAX_CLIP			WEAPON_NOCLIP
#define MAG60_MAX_CLIP			22
#define CHAINGUN_MAX_CLIP		100
#define GLAUNCHER_MAX_CLIP		5
#define SMG_MAX_CLIP			25
#define USAS_MAX_CLIP			20
#define GAUGE_SHOTGUN_MAX_CLIP	5
#define DEAGLE_MAX_CLIP			9
#define FREEZEGUN_MAX_CLIP		30
#define FLAMETHROWER_MAX_CLIP	100
#define SAWEDOFF_MAX_CLIP		2


// the default amount of ammo that comes with each gun when it spawns
#define GLOCK_DEFAULT_GIVE			17
#define PYTHON_DEFAULT_GIVE			6
#define MP5_DEFAULT_GIVE			50
#define MP5_M203_DEFAULT_GIVE		0
#define SHOTGUN_DEFAULT_GIVE		12
#define CROSSBOW_DEFAULT_GIVE		30
#define RPG_DEFAULT_GIVE			5
#define GAUSS_DEFAULT_GIVE			20
#define EGON_DEFAULT_GIVE			20
#define HANDGRENADE_DEFAULT_GIVE	5
#define SATCHEL_DEFAULT_GIVE		1
#define TRIPMINE_DEFAULT_GIVE		1
#define SNARK_DEFAULT_GIVE			5
#define HIVEHAND_DEFAULT_GIVE		8
#define RAILGUN_DEFAULT_GIVE		10
#define CANNON_DEFAULT_GIVE			10
#define MAG60_DEFAULT_GIVE			44
#define CHAINGUN_DEFAULT_GIVE		100
#define GLAUNCHER_DEFAULT_GIVE		10
#define SMG_DEFAULT_GIVE			50
#define USAS_DEFAULT_GIVE			40
#define SNOWBALL_DEFAULT_GIVE		5
#define _12_GAUGE_DEFAULT_GIVE		12
#define NUKE_DEFAULT_GIVE			3
#define DEAGLE_DEFAULT_GIVE			9
#define FREEZEGUN_DEFAULT_GIVE		60
#define FLAMETHROWER_DEFAULT_GIVE	100

// The amount of ammo given to a player by an ammo item.
#define AMMO_URANIUMBOX_GIVE	20
#define AMMO_GLOCKCLIP_GIVE		GLOCK_MAX_CLIP
#define AMMO_357BOX_GIVE		PYTHON_MAX_CLIP
#define AMMO_MP5CLIP_GIVE		MP5_MAX_CLIP
#define AMMO_CHAINBOX_GIVE		200
#define AMMO_M203BOX_GIVE		2
#define AMMO_BUCKSHOTBOX_GIVE	12
#define AMMO_CROSSBOWCLIP_GIVE	CROSSBOW_MAX_CLIP
#define AMMO_RPGCLIP_GIVE		RPG_MAX_CLIP
#define AMMO_URANIUMBOX_GIVE	20
#define AMMO_SNARKBOX_GIVE		5

// bullet types
typedef	enum
{
	BULLET_NONE = 0,
	BULLET_PLAYER_9MM, // glock
	BULLET_PLAYER_MP5, // mp5
	BULLET_PLAYER_357, // python
	BULLET_PLAYER_RIFLE, // sniper rifle
	BULLET_PLAYER_BUCKSHOT, // shotgun
	BULLET_PLAYER_CROWBAR, // crowbar swipe
	BULLET_PLAYER_FIST,
	BULLET_PLAYER_WRENCH,
	BULLET_PLAYER_SNOWBALL,
	BULLET_PLAYER_CHAINSAW,
	BULLET_PLAYER_EXPLOSIVE_BUCKSHOT,
	BULLET_PLAYER_KNIFE,
	BULLET_PLAYER_BOOT,

	BULLET_MONSTER_9MM,
	BULLET_MONSTER_MP5,
	BULLET_MONSTER_12MM,
} Bullet;


#define ITEM_FLAG_SELECTONEMPTY		1
#define ITEM_FLAG_NOAUTORELOAD		2
#define ITEM_FLAG_NOAUTOSWITCHEMPTY	4
#define ITEM_FLAG_LIMITINWORLD		8
#define ITEM_FLAG_EXHAUSTIBLE		16 // A player can totally exhaust their ammo supply and lose this weapon

#define WEAPON_IS_ONTARGET 0x40

typedef struct
{
	int		iSlot;
	int		iPosition;
	const char	*pszAmmo1;	// ammo 1 type
	int		iMaxAmmo1;		// max ammo 1
	const char	*pszAmmo2;	// ammo 2 type
	int		iMaxAmmo2;		// max ammo 2
	const char	*pszName;
	int		iMaxClip;
	int		iId;
	int		iFlags;
	int		iWeight;// this value used to determine this weapon's importance in autoselection.
	const char	*pszDisplayName;
} ItemInfo;

typedef struct
{
	const char *pszName;
	int iId;
} AmmoInfo;

// Items that the player has in their inventory that they can use
class CBasePlayerItem : public CBaseAnimating
{
public:
	virtual void SetObjectCollisionBox( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

	virtual int AddToPlayer( CBasePlayer *pPlayer );	// return TRUE if the item you want the item added to the player inventory
	virtual int AddDuplicate( CBasePlayerItem *pItem ) { return FALSE; }	// return TRUE if you want your duplicate removed from world
	void EXPORT DestroyItem( void );
	void EXPORT DefaultTouch( CBaseEntity *pOther );	// default weapon touch
	void EXPORT FallThink ( void );// when an item is first spawned, this think is run to determine when the object has hit the ground.
	void EXPORT Materialize( void );// make a weapon visible and tangible
	void EXPORT AttemptToMaterialize( void );  // the weapon desires to become visible and tangible, if the game rules allow for it
	CBaseEntity* Respawn ( void );// copy a weapon
	void FallInit( void );
	void CheckRespawn( void );
	virtual int GetItemInfo(ItemInfo *p) { return 0; };	// returns 0 if struct not filled out
	virtual BOOL CanDeploy( void ) { return TRUE; };
	virtual BOOL Deploy( )								// returns is deploy was successful
		 { return TRUE; };
	virtual BOOL DeployLowKey( )
		 { return TRUE; };

	virtual BOOL CanHolster( void ) { return TRUE; };// can this weapon be put away right now?
	virtual void Holster( int skiplocal = 0 );
	virtual void UpdateItemInfo( void ) { return; };

	virtual void ItemPreFrame( void )	{ return; }		// called each frame by the player PreThink
	virtual void ItemPostFrame( void ) { return; }		// called each frame by the player PostThink

	virtual void Drop( void );
	virtual void Kill( void );
	virtual void AttachToPlayer ( CBasePlayer *pPlayer );

	virtual int PrimaryAmmoIndex() { return -1; };
	virtual int SecondaryAmmoIndex() { return -1; };

	virtual int UpdateClientData( CBasePlayer *pPlayer ) { return 0; }

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return NULL; };

	static ItemInfo ItemInfoArray[ MAX_WEAPONS ];
	static AmmoInfo AmmoInfoArray[ MAX_AMMO_SLOTS ];

	CBasePlayer	*m_pPlayer;
	CBasePlayerItem *m_pNext;
	int		m_iId;												// WEAPON_???

	virtual int iItemSlot( void ) { return 0; }			// return 0 to MAX_ITEMS_SLOTS, used in hud

	int			iItemPosition( void ) { return ItemInfoArray[ m_iId ].iPosition; }
	const char	*pszAmmo1( void )	{ return ItemInfoArray[ m_iId ].pszAmmo1; }
	int			iMaxAmmo1( void )	{ return ItemInfoArray[ m_iId ].iMaxAmmo1; }
	const char	*pszAmmo2( void )	{ return ItemInfoArray[ m_iId ].pszAmmo2; }
	int			iMaxAmmo2( void )	{ return ItemInfoArray[ m_iId ].iMaxAmmo2; }
	const char	*pszName( void )	{ return ItemInfoArray[ m_iId ].pszName; }
	int			iMaxClip( void )	{ return ItemInfoArray[ m_iId ].iMaxClip; }
	int			iWeight( void )		{ return ItemInfoArray[ m_iId ].iWeight; }
	int			iFlags( void )		{ return ItemInfoArray[ m_iId ].iFlags; }
	const char	*pszDisplayName( void )		{ return ItemInfoArray[ m_iId ].pszDisplayName; }

	// int		m_iIdPrimary;										// Unique Id for primary ammo
	// int		m_iIdSecondary;										// Unique Id for secondary ammo

	virtual void ProvideDualItem(CBasePlayer *pPlayer, const char *itemName) { return; }
	virtual void ProvideSingleItem(CBasePlayer *pPlayer, const char *itemName) { return; }
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
};


// inventory items that 
class CBasePlayerWeapon : public CBasePlayerItem
{
public:
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

	// generic weapon versions of CBasePlayerItem calls
	virtual int AddToPlayer( CBasePlayer *pPlayer );
	virtual int AddDuplicate( CBasePlayerItem *pItem );

	virtual int ExtractAmmo( CBasePlayerWeapon *pWeapon ); //{ return TRUE; };			// Return TRUE if you can add ammo to yourself when picked up
	virtual int ExtractClipAmmo( CBasePlayerWeapon *pWeapon );// { return TRUE; };			// Return TRUE if you can add ammo to yourself when picked up

	virtual int AddWeapon( void ) { ExtractAmmo( this ); return TRUE; };	// Return TRUE if you want to add yourself to the player

	// generic "shared" ammo handlers
	BOOL AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry );
	BOOL AddSecondaryAmmo( int iCount, char *szName, int iMaxCarry );

	virtual void UpdateItemInfo( void );	// updates HUD state

	int m_iPlayEmptySound;
	int m_fFireOnEmpty;		// True when the gun is empty and the player is still holding down the
							// attack key(s)
	virtual BOOL PlayEmptySound( void );
	virtual void ResetEmptySound( void );

	virtual void SendWeaponAnim( int iAnim, int skiplocal = 1, int body = 0 );  // skiplocal is 1 if client is predicting weapon animations

	virtual BOOL CanDeploy( void );
	virtual BOOL IsUseable( void );
	BOOL DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int skiplocal = 0, int body = 0 );
	void DefaultHolster( int iAnim );
	int DefaultReload( int iClipSize, int iAnim, float fDelay, int body = 0 );

	virtual void ItemPostFrame( void );	// called each frame by the player PostThink
	// called by CBasePlayerWeapons ItemPostFrame()
	virtual void PrimaryAttack( void ) { return; }				// do "+ATTACK"
	virtual void SecondaryAttack( void ) { return; }			// do "+ATTACK2"
	virtual void Reload( void ) { return; }						// do "+RELOAD"
	virtual void WeaponIdle( void ) { return; }					// called when no buttons pressed
	virtual int UpdateClientData( CBasePlayer *pPlayer );		// sends hud info to client dll, if things have changed
	virtual void RetireWeapon( void );
	virtual BOOL ShouldWeaponIdle( void ) {return FALSE; };
	virtual void Holster( int skiplocal = 0 );
	virtual BOOL UseDecrement( void ) { return FALSE; };

	virtual BOOL SemiAuto( void ) { return FALSE; };
	virtual BOOL CanKick( void ) { return TRUE; };
	virtual BOOL CanPunch( void ) { return TRUE; };
	virtual BOOL CanSlide( void ) { return TRUE; };
	virtual void ProvideDualItem(CBasePlayer *pPlayer, const char *itemName);
	virtual void ProvideSingleItem(CBasePlayer *pPlayer, const char *itemName) { return; }
	virtual void SwapDualWeapon() { return; }
	virtual void WeaponPickup(CBasePlayer *pPlayer, int weaponId);

	void ThrowGrenade( BOOL m_iCheckAmmo );
	void ThrowSnowball( BOOL m_iCheckAmmo );
	void ThrowRocket( BOOL m_iCheckAmmo );

	TraceResult m_trBootHit;
	void StartKick( BOOL holdingSomething );
	void KickAttack( BOOL holdingSomething );
	void EXPORT EndKick( void );

	void StartPunch( BOOL holdingSomething );
	void PunchAttack( BOOL holdingSomething );
	void EXPORT EndPunch( void );

	void EjectBrassLate( void );
	void EjectShotShellLate( void );
	
	int	PrimaryAmmoIndex(); 
	int	SecondaryAmmoIndex(); 

	void PrintState( void );

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return (CBasePlayerItem *)this; };
	float GetNextAttackDelay( float delay );

	float m_flPumpTime;
	int		m_fInSpecialReload;									// Are we in the middle of a reload for the shotguns
	float	m_flNextPrimaryAttack;								// soonest time ItemPostFrame will call PrimaryAttack
	float	m_flNextSecondaryAttack;							// soonest time ItemPostFrame will call SecondaryAttack
	float	m_flTimeWeaponIdle;									// soonest time ItemPostFrame will call WeaponIdle
	int		m_iPrimaryAmmoType;									// "primary" ammo index into players m_rgAmmo[]
	int		m_iSecondaryAmmoType;								// "secondary" ammo index into players m_rgAmmo[]
	int		m_iClip;											// number of shots left in the primary weapon clip, -1 it not used
	int		m_iClientClip;										// the last version of m_iClip sent to hud dll
	int		m_iClientWeaponState;								// the last version of the weapon state sent to hud dll (is current weapon, is on target)
	int		m_fInReload;										// Are we in the middle of a reload;

	int		m_iDefaultAmmo;// how much ammo you get when you pick up this weapon as placed by a level designer.
	int		m_bFired;

	// hle time creep vars
	float	m_flPrevPrimaryAttack;
	float	m_flLastFireTime;			

};


class CBasePlayerAmmo : public CBaseEntity
{
public:
	virtual void Spawn( void );
	void EXPORT DefaultTouch( CBaseEntity *pOther ); // default weapon touch
	virtual BOOL AddAmmo( CBaseEntity *pOther ) { return TRUE; };
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );


	CBaseEntity* Respawn( void );
	void EXPORT Materialize( void );
};


extern DLL_GLOBAL	short	g_sModelIndexLaser;// holds the index for the laser beam
extern DLL_GLOBAL	const char *g_pModelNameLaser;

extern DLL_GLOBAL	short	g_sModelIndexLaserDot;// holds the index for the laser beam dot
extern DLL_GLOBAL	short	g_sModelIndexFireball;// holds the index for the fireball
extern DLL_GLOBAL	short	g_sModelIndexSmoke;// holds the index for the smoke cloud
extern DLL_GLOBAL	short	g_sModelIndexWExplosion;// holds the index for the underwater explosion
extern DLL_GLOBAL	short	g_sModelIndexBubbles;// holds the index for the bubbles model
extern DLL_GLOBAL	short	g_sModelIndexBloodDrop;// holds the sprite index for blood drops
extern DLL_GLOBAL	short	g_sModelIndexBloodSpray;// holds the sprite index for blood spray (bigger)
extern DLL_GLOBAL	short	g_sModelIndexSnowballHit;
extern DLL_GLOBAL	short 	g_sModelIndexIceFireball;
extern DLL_GLOBAL	short 	g_sModelIndexFire;
extern DLL_GLOBAL	short 	g_sModelIndexIceFire;
extern DLL_GLOBAL	short	g_sModelConcreteGibs;
extern DLL_GLOBAL	short	g_sModelWoodGibs;
extern DLL_GLOBAL	short	g_sModelLightning;
extern DLL_GLOBAL	short	g_sModelIndexFlame;

extern void ClearMultiDamage(void);
extern void ApplyMultiDamage(entvars_t* pevInflictor, entvars_t* pevAttacker );
extern void AddMultiDamage( entvars_t *pevInflictor, CBaseEntity *pEntity, float flDamage, int bitsDamageType);

extern void DecalGunshot( TraceResult *pTrace, int iBulletType );
extern void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage);
extern int DamageDecal( CBaseEntity *pEntity, int bitsDamageType );
extern void RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType );

typedef struct 
{
	CBaseEntity		*pEntity;
	float			amount;
	int				type;
} MULTIDAMAGE;

extern MULTIDAMAGE gMultiDamage;


#define LOUD_GUN_VOLUME			1000
#define NORMAL_GUN_VOLUME		600
#define QUIET_GUN_VOLUME		200

#define	BRIGHT_GUN_FLASH		512
#define NORMAL_GUN_FLASH		256
#define	DIM_GUN_FLASH			128

#define BIG_EXPLOSION_VOLUME	2048
#define NORMAL_EXPLOSION_VOLUME	1024
#define SMALL_EXPLOSION_VOLUME	512

#define	WEAPON_ACTIVITY_VOLUME	64

#define VECTOR_CONE_1DEGREES	Vector( 0.00873, 0.00873, 0.00873 )
#define VECTOR_CONE_2DEGREES	Vector( 0.01745, 0.01745, 0.01745 )
#define VECTOR_CONE_3DEGREES	Vector( 0.02618, 0.02618, 0.02618 )
#define VECTOR_CONE_4DEGREES	Vector( 0.03490, 0.03490, 0.03490 )
#define VECTOR_CONE_5DEGREES	Vector( 0.04362, 0.04362, 0.04362 )
#define VECTOR_CONE_6DEGREES	Vector( 0.05234, 0.05234, 0.05234 )
#define VECTOR_CONE_7DEGREES	Vector( 0.06105, 0.06105, 0.06105 )
#define VECTOR_CONE_8DEGREES	Vector( 0.06976, 0.06976, 0.06976 )
#define VECTOR_CONE_9DEGREES	Vector( 0.07846, 0.07846, 0.07846 )
#define VECTOR_CONE_10DEGREES	Vector( 0.08716, 0.08716, 0.08716 )
#define VECTOR_CONE_15DEGREES	Vector( 0.13053, 0.13053, 0.13053 )
#define VECTOR_CONE_20DEGREES	Vector( 0.17365, 0.17365, 0.17365 )

// special deathmatch shotgun spreads
#define VECTOR_CONE_DM_SHOTGUN	Vector( 0.08716, 0.04362, 0.00  )// 10 degrees by 5 degrees
#define VECTOR_CONE_DM_DOUBLESHOTGUN Vector( 0.17365, 0.04362, 0.00 ) // 20 degrees by 5 degrees

//=========================================================
// CWeaponBox - a single entity that can store weapons
// and ammo. 
//=========================================================
class CWeaponBox : public CBaseEntity
{
	void Precache( void );
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	BOOL IsEmpty( void );
	int  GiveAmmo( int iCount, char *szName, int iMax, int *pIndex = NULL );
	void SetObjectCollisionBox( void );
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );

public:
	void EXPORT Kill ( void );
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL HasWeapon( CBasePlayerItem *pCheckItem );
	BOOL PackWeapon( CBasePlayerItem *pWeapon );
	BOOL PackAmmo( int iszName, int iCount );
	
	CBasePlayerItem	*m_rgpPlayerItems[MAX_ITEM_TYPES];// one slot for each 

	int m_rgiszAmmo[MAX_AMMO_SLOTS];// ammo names
	int	m_rgAmmo[MAX_AMMO_SLOTS];// ammo quantities

	int m_cAmmoTypes;// how many ammo types packed into this box (if packed by a level designer)
};

#ifdef CLIENT_DLL
bool bIsMultiplayer ( void );
void LoadVModel ( char *szViewModel, CBasePlayer *m_pPlayer );
#endif

class CGlock : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 2; }
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void GlockFire( float flSpread, float flCycleTime, int silencer );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal );
	void Reload( void );
	void WeaponIdle( void );
	void EXPORT AddSilencer( void );
	BOOL ChangeModel( void );

	int m_iSilencer;

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	virtual BOOL SemiAuto( void ) { return TRUE; }

private:
	int m_iShell;

	unsigned short m_usFireGlock1;
	unsigned short m_usFireGlock2;
};


class CCrowbar : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int AddToPlayer( CBasePlayer *pPlayer );
	int iItemSlot( void ) { return 1; }
	void EXPORT SwingAgain( void );
	void EXPORT Smack( void );
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void Throw( void );
	int Swing( int fFirst );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	int m_iSwing;
	TraceResult m_trHit;

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	BOOL CanSlide();
	virtual BOOL SemiAuto( void ) { return TRUE; }

private:
	unsigned short m_usCrowbar;
};

class CRocketCrowbar : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int AddToPlayer( CBasePlayer *pPlayer );
	int iItemSlot( void ) { return 1; }
	void EXPORT SwingAgain( void );
	void EXPORT Smack( void );
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int Swing( int fFirst );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	int m_iSwing;
	TraceResult m_trHit;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	virtual BOOL SemiAuto( void ) { return TRUE; }

private:
	unsigned short m_usRocketCrowbar;
};

class CDrunkRocket : public CGrenade
{
public:
	void Spawn( float startEngineTime );
	void Precache( void );
	void EXPORT FollowThink( void );
	void EXPORT IgniteThink( void );
	void EXPORT RocketTouch( CBaseEntity *pOther );
	static CDrunkRocket *CreateDrunkRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, float startEngineTime );

	int m_iTrail;
	float m_flIgniteTime;
};

class CKnife : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int AddToPlayer( CBasePlayer *pPlayer );
	int iItemSlot( void ) { return 1; }
	void EXPORT SwingAgain( void );
	void EXPORT Smack( void );
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void Throw( void );
	int Swing( int fFirst );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	int m_iSwing;
	TraceResult m_trHit;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	BOOL CanSlide();
	virtual BOOL SemiAuto( void ) { return TRUE; }

private:
	unsigned short m_usKnife;
};

class CPython : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 2; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );

	BOOL m_fInZoom;// don't save this. 

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	virtual BOOL SemiAuto( void ) { return TRUE; }

private:
	unsigned short m_usFirePython;
};

class CMP5 : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 3; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int SecondaryAmmoIndex( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	float m_flNextAnimTime;
	int m_iShell;

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	unsigned short m_usMP5;
	unsigned short m_usMP52;
};

class CCrossbow : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( ) { return 3; }
	int GetItemInfo(ItemInfo *p);

	void FireBolt( void );
	void FireSniperBolt( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int AddToPlayer( CBasePlayer *pPlayer );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );

	int m_fInZoom; // don't save this

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	unsigned short m_usCrossbow;
	unsigned short m_usCrossbow2;
};

class CShotgun : public CBasePlayerWeapon
{
public:

#ifndef CLIENT_DLL
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
#endif


	void Spawn( void );
	void Precache( void );
	int iItemSlot( ) { return 3; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	float m_flNextReload;
	int m_iShell;

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	virtual BOOL SemiAuto( void ) { return TRUE; }

private:
	unsigned short m_usDoubleFire;
	unsigned short m_usSingleFire;
};

class CSniperRifle : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 3; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	float m_flNextAnimTime;
	int m_iShell;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	virtual BOOL SemiAuto( void ) { return FALSE; }

private:
	unsigned short m_usSniperRifle;
};

class CLaserSpot : public CBaseEntity
{
	void Spawn( void );
	void Precache( void );

	int	ObjectCaps( void ) { return FCAP_DONT_SAVE; }

public:
	void Suspend( float flSuspendTime );
	void EXPORT Revive( void );
	
	static CLaserSpot *CreateSpot( void );
};

class CRpg : public CBasePlayerWeapon
{
public:

#ifndef CLIENT_DLL
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
#endif

	void Spawn( void );
	void Precache( void );
	void Reload( void );
	int iItemSlot( void ) { return 4; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	BOOL CanHolster( void );
	void Holster( int skiplocal = 0 );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int SecondaryAmmoIndex( void );
	void WeaponIdle( void );

	void UpdateSpot( void );
	BOOL ShouldWeaponIdle( void ) { return TRUE; };

	CLaserSpot *m_pSpot;
	int m_fSpotActive;
	int m_cActiveRockets;// how many missiles in flight from this launcher right now?

	void ProvideDualItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	unsigned short m_usRpg;
	unsigned short m_usRpgExtreme;

	float m_flNextReload;
};

class CRpgRocket : public CGrenade
{
public:
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	void Spawn( float startEngineTime );
	void Precache( void );
	void EXPORT FollowThink( void );
	void EXPORT IgniteThink( void );
	void EXPORT RocketTouch( CBaseEntity *pOther );
	static CRpgRocket *CreateRpgRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, float startEngineTime, BOOL redRocket );

	void Spawn( void );
	int m_iTrail;
	float m_flIgniteTime;
	BOOL m_redRocket;
	CRpg *m_pLauncher;// pointer back to the launcher that fired me. 
};

class CGauss : public CBasePlayerWeapon
{
public:

#ifndef CLIENT_DLL
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
#endif

	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 4; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0  );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void WeaponIdle( void );
	
	void StartFire( void );
	void Fire( Vector vecOrigSrc, Vector vecDirShooting, float flDamage );
	float GetFullChargeTime( void );
	int m_iBeam;
	int m_iSoundState; // don't save this

	// was this weapon just fired primary or secondary?
	// we need to know so we can pick the right set of effects. 
	BOOL m_fPrimaryFire;

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	unsigned short m_usGaussFire;
	unsigned short m_usGaussSpin;
};

class CEgon : public CBasePlayerWeapon
{
public:
#ifndef CLIENT_DLL
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
#endif

	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 4; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

 	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );

	void UpdateEffect( const Vector &startPoint, const Vector &endPoint, float timeBlend );

	void CreateEffect ( void );
	void DestroyEffect ( void );

	void EndAttack( void );
	void Attack( void );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void WeaponIdle( void );

	float m_flAmmoUseTime;// since we use < 1 point of ammo per update, we subtract ammo on a timer.

	float GetPulseInterval( void );
	float GetDischargeInterval( void );

	void Fire( const Vector &vecOrigSrc, const Vector &vecDir );

	BOOL HasAmmo( void );

	void UseAmmo( int count );
	
	enum EGON_FIREMODE { FIRE_NARROW, FIRE_WIDE};

	CBeam				*m_pBeam;
	CBeam				*m_pNoise;
	CSprite				*m_pSprite;

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	unsigned short m_usEgonStop;

private:
	float				m_shootTime;
	EGON_FIREMODE		m_fireMode;
	float				m_shakeTime;
	BOOL				m_deployed;

	unsigned short m_usEgonFire;
};

class CHgun : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 4; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	BOOL IsUseable( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	float m_flNextAnimTime;

	float m_flRechargeTime;
	
	int m_iFirePhase;// don't save me.

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}
private:
	unsigned short m_usHornetFire;
};



class CHandGrenade : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 5; }
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	BOOL CanHolster( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	
	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}
};

class CSatchel : public CBasePlayerWeapon
{
public:

#ifndef CLIENT_DLL
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
#endif

	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 5; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int AddDuplicate( CBasePlayerItem *pOriginal );
	BOOL CanDeploy( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	BOOL IsUseable( void );
	
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	void Throw( void );
	
	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}
};


class CTripmine : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 5; }
	int GetItemInfo(ItemInfo *p);
	void SetObjectCollisionBox( void )
	{
		//!!!BUGBUG - fix the model!
		pev->absmin = pev->origin + Vector(-16, -16, -5);
		pev->absmax = pev->origin + Vector(16, 16, 28); 
	}

	void PrimaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	unsigned short m_usTripFire;

};

class CSqueak : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 5; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	int m_fJustThrown;

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	unsigned short m_usSnarkFire;
	unsigned short m_usSnarkRelease;
};

class CVest : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 5; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL CanDeploy( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	BOOL IsUseable( void );
	void EXPORT BlowThink( void );
	void EXPORT GoneThink( void );
	void RetireThink( void );

	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	int m_iLightning;
};

class CChumtoad : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 5; }
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	BOOL CanHolster( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	int m_fJustThrown;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	unsigned short m_usChumtoadFire;
	unsigned short m_usChumtoadRelease;
};

class CRailgun : public CBasePlayerWeapon
{
public:

	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 4; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void WeaponIdle( void );

	void StartFire( void );
	void Fire( Vector vecSrc, Vector vecDirShooting, Vector effectSrc, float flDamage );

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	// rail, rail, rail
	void CreateTrail(Vector,Vector);

	void ProvideDualItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	int m_iBalls;
	int m_iGlow;
	int m_iBeam;
};

class CDualRailgun : public CBasePlayerWeapon
{
public:

	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 6; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void FireThink( void );
	void WeaponIdle( void );

	void StartFire( Vector vecAiming, Vector vecSrc, Vector effectSrc);
	void Fire( Vector vecSrc, Vector vecDirShooting, Vector effectSrc, float flDamage );

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	// rail, rail, rail
	void CreateTrail(Vector,Vector);

	void ProvideSingleItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	int m_iBalls;
	int m_iGlow;
	int m_iBeam;
	int m_iAltFire;
};

class CCannon : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 4; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void WeaponIdle( void );

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	unsigned short m_usCannon;
	unsigned short m_usCannonFlak;
};

class CFlak : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	BOOL EXPORT ShouldCollide( CBaseEntity *pOther );
	void EXPORT FlakTouch( CBaseEntity *pOther );
	static CFlak *CreateFlak( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner );
};

class CFlakBomb : public CGrenade
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT BombTouch( CBaseEntity *pOther );
	void EXPORT BlowUp( void );
	static CFlakBomb *CreateFlakBomb( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner );

	CBaseEntity *owner;
	int m_iTrail;
};

class CMag60 : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 2; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void Fire( float flSpread, float flCycleTime, int rotated );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal );
	void Reload( void );
	void WeaponIdle( void );

	int m_iRotated;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	static const char *pRotateUpBladeSounds[];
	static const char *pRotateDownBladeSounds[];

	void ProvideDualItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	int m_iShell;

	unsigned short m_useFireMag60;
};

class CChaingun : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 3; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void Fire( float flSpread, float flCycleTime );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal );
	void Reload( void );
	void WeaponIdle( void );

	void SlowDownPlayer( void );

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	int m_iWeaponMode;
	int m_fFireMagnitude;
	int m_useFireChaingun;
};

class CGrenadeLauncher : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 3; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	unsigned short m_usGrenadeLauncher;
};

class CSMG : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 3; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	float m_flNextAnimTime;
	int m_iShell;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	void ProvideDualItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

	static const char *pHansSounds[];

private:
	short m_sFireCount;
	short m_sMode;
	unsigned short m_usSmg;
};

class CUsas : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( ) { return 3; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );

	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	int m_iShell;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	void ProvideDualItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	unsigned short m_usSingleFire;
};

class CFists : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int AddToPlayer( CBasePlayer *pPlayer );
	int iItemSlot( void ) { return 1; }
	void EXPORT SwingAgain( void );
	void EXPORT Smack( void );
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int Swing( int fFirst );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	int m_iSwing;
	TraceResult m_trHit;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	virtual BOOL SemiAuto( void ) { return TRUE; }

private:
	unsigned short m_usFists;
};

class CWrench : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int AddToPlayer( CBasePlayer *pPlayer );
	int iItemSlot( void ) { return 1; }
	void EXPORT SwingAgain( void );
	void EXPORT Smack( void );
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void Throw( void );
	int Swing( int fFirst );
	BOOL Deploy( void );
	BOOL DeployLowKey( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	int m_iSwing;
	TraceResult m_trHit;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	void ProvideDualItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

	BOOL CanSlide();
	virtual BOOL SemiAuto( void ) { return TRUE; }

private:
	unsigned short m_usWrench;
};

class CSnowball : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 5; }
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	BOOL CanHolster( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	void EXPORT Throw( void );

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}
};

class CChainsaw : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int AddToPlayer( CBasePlayer *pPlayer );
	int iItemSlot( void ) { return 1; }
	void EXPORT SwingAgain( void );
	void EXPORT Smack( void );
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	int Swing( int fFirst, BOOL animation );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	int m_iSwing;
	TraceResult m_trHit;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	unsigned short m_usChainsaw;
};


class C12Gauge : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( ) { return 3; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	float m_flNextReload;
	int m_iShell;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	static const char *pJacksonSounds[];

	virtual BOOL SemiAuto( void ) { return TRUE; }

private:
	unsigned short m_usSingleFire;
};


class CNuke : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	void Reload( void );
	int iItemSlot( void ) { return 4; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void FireNuke( BOOL withCamera );
	void Shart();
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void WeaponIdle( void );

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	virtual BOOL CanKick( void ) { return FALSE; }
	virtual BOOL CanPunch( void ) { return FALSE; }

private:
	unsigned short m_usNuke;
	EHANDLE pNukeRocket;
};

class CNukeRocket : public CGrenade
{
public:
	void Spawn( float startEngineTime );
	void Precache( void );
	void EXPORT FollowThink( void );
	void EXPORT IgniteThink( void );
	void EXPORT RocketTouch( CBaseEntity *pOther );
	static CNukeRocket *CreateNukeRocket( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, float startEngineTime, BOOL hasCamera );
	virtual void Killed( entvars_t *pevAttacker, int iGib );

	int m_iTrail;
	int m_iExp, m_iIceExp;
	float m_flIgniteTime;
	float m_yawCenter, m_pitchCenter;
	BOOL m_iCamera;
};

class CDeagle : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 2; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );
	void PrimaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	virtual BOOL SemiAuto( void ) { return TRUE; }
	void ProvideDualItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	unsigned short m_usFireDeagle;
};

class CDualDeagle : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 6; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );
	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );

	int m_iAltFire;

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	virtual BOOL SemiAuto( void ) { return TRUE; }
	void ProvideSingleItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	unsigned short m_usFireDeagle;
	unsigned short m_usFireDeagleBoth;
};


class CDualRpg : public CBasePlayerWeapon
{
public:

	void Spawn( void );
	void Precache( void );
	void Reload( void );
	int iItemSlot( void ) { return 6; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	BOOL CanHolster( void );
	void Holster( int skiplocal = 0 );

	void PrimaryAttack( void );
	void FireSecondRocket( void );
	void SecondaryAttack( void );
	int SecondaryAmmoIndex( void );
	void WeaponIdle( void );

	void UpdateSpot( void );
	BOOL ShouldWeaponIdle( void ) { return TRUE; };

	CLaserSpot *m_pSpot;
	int m_fSpotActive;
	int m_cActiveRockets;// how many missiles in flight from this launcher right now?

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	void ProvideSingleItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	unsigned short m_usRpg;
	unsigned short m_usRpgExtreme;

	float m_flNextReload;
};

class CDualMag60 : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 6; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	float m_flNextAnimTime;
	int m_iShell;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	static const char *pBladeSounds[];

	void ProvideSingleItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	unsigned short m_usMag60;
};

class CDualSMG : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( void ) { return 6; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	float m_flNextAnimTime;
	int m_iShell;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	void ProvideSingleItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

	static const char *pHansSounds[];

private:
	short m_sFireCount;
	short m_sMode;
	unsigned short m_usSmg;
};

class CFlyingWrench : public CBaseEntity
{
public:

	void Spawn( void );
	void Precache( void );
	void EXPORT BubbleThink( void );
	void EXPORT SpinTouch( CBaseEntity *pOther );

	virtual int ObjectCaps( void ) { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_PORTAL; }

private:

	EHANDLE m_hOwner;		  // Original owner is stored here so we can
							  // allow the wrench to hit the user.
};

class CDualWrench : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int AddToPlayer( CBasePlayer *pPlayer );
	int iItemSlot( void ) { return 6; }
	void EXPORT SwingAgain( void );
	void EXPORT Smack( void );
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void Throw( void );
	int Swing( int fFirst );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void WeaponIdle( void );
	int m_iSwing;
	TraceResult m_trHit;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	BOOL CanSlide();
	virtual BOOL SemiAuto( void ) { return TRUE; }
	void ProvideSingleItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	unsigned short m_usWrench;
};

class CDualUsas : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( ) { return 6; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	int m_iShell;
	int m_iAltFire;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	void ProvideSingleItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	unsigned short m_usSingleFire;
	unsigned short m_usDualFire;
};

class CFreezeGun : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( ) { return 3; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );

	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	int m_iShell;

	virtual BOOL UseDecrement( void )
	{
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

private:
	int m_iPlasmaSprite;
	int m_iIceMuzzlePlasma, m_iMuzzlePlasma;
	int m_fInAttack;
	unsigned short m_usPlasmaFire;
};

class CGravityGun : public CBasePlayerWeapon
{
public:
	void Spawn();
	void Precache();
	int AddToPlayer( CBasePlayer *pPlayer );
	int iItemSlot() { return 1; }
	int GetItemInfo(ItemInfo* p);

	void PrimaryAttack();
	void SecondaryAttack();
	BOOL DeployLowKey( void );
	BOOL Deploy();
	void Holster(int skiplocal);

	void ItemPostFrame();
	void WeaponIdle();

	CBaseEntity* GetEntity(float dist, bool m_bTakeDamage = false);

	#ifdef CLIENT_DLL
	CBaseEntity* m_pCurrentEntity;
	#else
	EHANDLE m_pCurrentEntity;
	#endif
	float m_flNextIdleTime;

	bool m_bResetIdle;
	bool m_bFoundPotentialTarget;

	virtual BOOL UseDecrement( void )
	{
#if defined(CLIENT_WEAPONS)
		return true;
#else
		return false;
#endif
	}

	virtual BOOL SemiAuto( void ) { return TRUE; }

private:
	unsigned short m_usGravGun;
};

class CFlameThrower : public CBasePlayerWeapon
{
public:
   void Spawn( void );
   void Precache( void );
   int iItemSlot( void ) { return 5; }
   int GetItemInfo(ItemInfo *p);
   int AddToPlayer( CBasePlayer *pPlayer );

   void Fire( void );
   void UseAmmo( int count );
   void EndAttack( void );
   void PrimaryAttack( void );
   void SecondaryAttack( void );
   BOOL DeployLowKey( void );
   BOOL Deploy( void );
   void Holster( int skiplocal = 0 );
   void WeaponIdle( void );

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	void ProvideDualItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	unsigned short m_usFlameStream;
	unsigned short m_usFlameThrower;
	unsigned short m_usFlameThrowerEnd;

	int m_fireMode;
	float sctime;
	float DangerSoundTime;
	float m_flAmmoUseTime;// since we use < 1 point of ammo per update, we subtract ammo on a timer.
};

class CDualFlameThrower : public CBasePlayerWeapon
{
public:
   void Spawn( void );
   void Precache( void );
   int iItemSlot( void ) { return 6; }
   int GetItemInfo(ItemInfo *p);
   int AddToPlayer( CBasePlayer *pPlayer );

   void Fire( void );
   void UseAmmo( int count );
   void EndAttack( void );
   void PrimaryAttack( void );
   void SecondaryAttack( void );
   BOOL DeployLowKey( void );
   BOOL Deploy( void );
   void Holster( int skiplocal = 0 );
   void WeaponIdle( void );

   // Secondary flameball launch while holding fire down
   BOOL ShouldWeaponIdle( void ) { return TRUE; };

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	void ProvideSingleItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	unsigned short m_usFlameStream;
	unsigned short m_usFlameThrower;
	unsigned short m_usFlameThrowerEnd;

	int m_fireMode;
	float sctime;
	float DangerSoundTime;
	float m_flAmmoUseTime;
	float m_fSecondaryFireTime;
};

class CAshpod : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	BOOL AddToPlayer( CBasePlayer *pPlayer );
	int iItemSlot( void ) { return 1; }
	int GetItemInfo(ItemInfo *p);

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	void PortalFire( int state );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal );
	void WeaponIdle( void );

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	virtual BOOL SemiAuto( void ) { return TRUE; }

private:
	int m_iTrail;
	int m_iIceTrail;
	int m_iShell;
};

class CSawedOff : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( ) { return 2; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	int m_iShell;
	int m_iAltFire;

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	void ProvideDualItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

	virtual BOOL SemiAuto( void ) { return TRUE; }

private:
	unsigned short m_usDoubleFire;
	unsigned short m_usSingleFire;
};


class CDualSawedOff : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int iItemSlot( ) { return 2; }
	int GetItemInfo(ItemInfo *p);
	int AddToPlayer( CBasePlayer *pPlayer );

	void PrimaryAttack( void );
	void SecondaryAttack( void );
	BOOL DeployLowKey( void );
	BOOL Deploy( void );
	void Holster( int skiplocal = 0 );
	void Reload( void );
	void WeaponIdle( void );
	int m_iShell;
	int m_iAltFire;

	virtual BOOL UseDecrement( void )
	{ 
#if defined( CLIENT_WEAPONS )
		return TRUE;
#else
		return FALSE;
#endif
	}

	virtual BOOL SemiAuto( void ) { return TRUE; }

	void ProvideSingleItem(CBasePlayer *pPlayer, const char *itemName);
	void SwapDualWeapon( void );

private:
	unsigned short m_usDoubleFire;
	unsigned short m_usSingleFire;
};

class CFlyingSnowball : public CBaseEntity
{
public:

	void Spawn( void );
	void Precache( void );
	void EXPORT BubbleThink( void );
	void EXPORT SpinTouch( CBaseEntity *pOther );

	static CFlyingSnowball *Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, CBasePlayer *m_pPlayer );

	virtual int ObjectCaps( void ) { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_PORTAL; }

private:

	EHANDLE m_hOwner;		 // Original owner is stored here so we can
							// allow the wrench to hit the user.
};

#endif // WEAPONS_H
