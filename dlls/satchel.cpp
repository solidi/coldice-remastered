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
#include "gamerules.h"
#include "game.h"

enum satchel_e {
	SATCHEL_IDLE1 = 0,
	SATCHEL_FIDGET1,
	SATCHEL_DRAW,
	SATCHEL_DRAW2,
	SATCHEL_DROP
};

enum satchel_radio_e {
	SATCHEL_RADIO_IDLE1 = 0,
	SATCHEL_RADIO_FIDGET1,
	SATCHEL_RADIO_DRAW,
	SATCHEL_RADIO_FIRE,
	SATCHEL_RADIO_HOLSTER
};

enum decoy_e {
	IDLE_1 = 0,
	IDLE_3,
	IDLE_2,
	RUN,
	WALK,
	SHOOT,
	GRENADE_THROW,
	KICK,
	KICK_SHORT,
	DEATH_DURING_RUN,
	DIE_BACKWARDS,
	DIE_SIMPLE,
	JUMP,
	FLY_UP,
	FLY_DOWN,
	FLY_ATTACK,
	LAND_FROM_JUMP
};


class CSatchelCharge : public CGrenade
{
	void Spawn( void );
	void Precache( void );
	void BounceSound( void );

	void EXPORT SatchelSlide( CBaseEntity *pOther );
	void EXPORT SatchelThink( void );

	bool m_transformed;

public:
	void Deactivate( void );
};
LINK_ENTITY_TO_CLASS( monster_satchel, CSatchelCharge );

//=========================================================
// Deactivate - do whatever it is we do to an orphaned 
// satchel when we don't want it in the world anymore.
//=========================================================
void CSatchelCharge::Deactivate( void )
{
	pev->solid = SOLID_NOT;
	UTIL_Remove( this );
}


void CSatchelCharge :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_BOUNCE;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "models/w_satchel.mdl");
	//UTIL_SetSize(pev, Vector( -16, -16, -4), Vector(16, 16, 32));	// Old box -- size of headcrab monsters/players get blocked by this
	UTIL_SetSize(pev, Vector( -4, -4, -4), Vector(4, 4, 4));	// Uses point-sized, and can be stepped over
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CSatchelCharge::SatchelSlide );
	SetUse( &CSatchelCharge::DetonateUse );
	SetThink( &CSatchelCharge::SatchelThink );
	pev->nextthink = gpGlobals->time + 0.1;

	pev->gravity = 0.5;
	pev->friction = 0.8;

	pev->dmg = gSkillData.plrDmgSatchel;

	pev->sequence = 2;
	ResetSequenceInfo( );
}


void CSatchelCharge::SatchelSlide( CBaseEntity *pOther )
{
	entvars_t	*pevOther = pOther->pev;

	// don't hit the guy that launched this grenade
	if ( pOther->edict() == pev->owner )
		return;

	// pev->avelocity = Vector (300, 300, 300);
	pev->gravity = 1;// normal gravity now

	// HACKHACK - On ground isn't always set, so look for ground underneath
	TraceResult tr;
	UTIL_TraceLine( pev->origin, pev->origin - Vector(0,0,10), ignore_monsters, edict(), &tr );

	if ( tr.flFraction < 1.0 )
	{
		// add a bit of static friction
		pev->velocity = pev->velocity * 0.95;
		pev->avelocity = pev->avelocity * 0.9;
		// play sliding sound, volume based on velocity
	}
	if ( !(pev->flags & FL_ONGROUND) && pev->velocity.Length2D() > 10 )
	{
		BounceSound();
	}
	StudioFrameAdvance( );
}


void CSatchelCharge :: SatchelThink( void )
{
	//StudioFrameAdvance( );
	pev->nextthink = gpGlobals->time + 0.1;

	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	if (pev->waterlevel == 3)
	{
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = pev->velocity * 0.8;
		pev->avelocity = pev->avelocity * 0.9;
		pev->velocity.z += 8;
	}
	else if (pev->waterlevel == 0)
	{
		pev->movetype = MOVETYPE_BOUNCE;
	}
	else
	{
		pev->velocity.z -= 8;
	}

	if ( !m_transformed && pev->velocity.Length() < 150 ) {
		MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
			WRITE_BYTE( TE_SMOKE );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_SHORT( g_sModelIndexSmoke );
			WRITE_BYTE( RANDOM_LONG(0,9) + 40 ); // scale * 10
			WRITE_BYTE( 12 ); // framerate
		MESSAGE_END();

		SET_MODEL(ENT(pev), "models/w_hassassin.mdl");
#ifndef CLIENT_DLL
		if (allowvoiceovers.value)
			EMIT_SOUND(ENT(pev), CHAN_VOICE, "decoy_pushthatbutton.wav", 1, ATTN_NORM);
#endif
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/explode4.wav", 0.20, ATTN_NORM);

		pev->sequence = RANDOM_LONG(0, 2);

		m_transformed = true;

#ifndef CLIENT_DLL
		if (g_pGameRules->FAllowMonsters())
		{
			CBaseEntity *pAssassin = CBaseEntity::Create( "monster_human_assassin", pev->origin, pev->angles, NULL );
			if (pAssassin)
			{
				pAssassin->pev->owner = pev->owner;
				UTIL_Remove( this );
			}
		}
#endif
	}
}

void CSatchelCharge :: Precache( void )
{
	PRECACHE_MODEL("models/grenade.mdl");
	PRECACHE_MODEL("models/w_hassassin.mdl");
	PRECACHE_SOUND("decoy_pushthatbutton.wav");
	PRECACHE_SOUND("weapons/explode4.wav");
	PRECACHE_SOUND("weapons/g_bounce1.wav");
	PRECACHE_SOUND("weapons/g_bounce2.wav");
	PRECACHE_SOUND("weapons/g_bounce3.wav");
}

void CSatchelCharge :: BounceSound( void )
{
	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce1.wav", 1, ATTN_NORM);	break;
	case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce2.wav", 1, ATTN_NORM);	break;
	case 2:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "weapons/g_bounce3.wav", 1, ATTN_NORM);	break;
	}
}

#ifdef DECOY
LINK_ENTITY_TO_CLASS( weapon_satchel, CSatchel );
#endif


//=========================================================
// CALLED THROUGH the newly-touched weapon's instance. The existing player weapon is pOriginal
//=========================================================
int CSatchel::AddDuplicate( CBasePlayerItem *pOriginal )
{
	CSatchel *pSatchel;

#ifdef CLIENT_DLL
	if ( bIsMultiplayer() )
#else
	if ( g_pGameRules->IsMultiplayer() )
#endif
	{
		pSatchel = (CSatchel *)pOriginal;

		if ( pSatchel->m_chargeReady != 0 )
		{
			// player has some satchels deployed. Refuse to add more.
			return FALSE;
		}
	}

	return CBasePlayerWeapon::AddDuplicate ( pOriginal );
}

//=========================================================
//=========================================================
int CSatchel::AddToPlayer( CBasePlayer *pPlayer )
{
	m_chargeReady = 0;// this satchel charge weapon now forgets that any satchels are deployed by it.

	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

void CSatchel::Spawn( )
{
	Precache( );
	m_iId = WEAPON_SATCHEL;
	SET_MODEL(ENT(pev), "models/w_satchel.mdl");

	m_iDefaultAmmo = SATCHEL_DEFAULT_GIVE;

#ifndef CLIENT_DLL
	int floating = floatingweapons.value ? 1 : 0;
	pev->sequence = floating;
#endif

	FallInit();// get ready to fall down.
}


void CSatchel::Precache( void )
{
	PRECACHE_MODEL("models/v_satchel.mdl");
	PRECACHE_MODEL("models/v_satchel_radio.mdl");
	PRECACHE_MODEL("models/w_satchel.mdl");
	PRECACHE_MODEL("models/p_weapons.mdl");
	PRECACHE_MODEL("models/p_satchel_radio.mdl");

	UTIL_PrecacheOther( "monster_satchel" );
}


int CSatchel::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Satchel Charge";
	p->iMaxAmmo1 = SATCHEL_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 2;
	p->iFlags = ITEM_FLAG_SELECTONEMPTY | ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	p->iId = m_iId = WEAPON_SATCHEL;
	p->iWeight = SATCHEL_WEIGHT;
	p->pszDisplayName = "Female Assassin Decoy Bombs";

	return 1;
}

//=========================================================
//=========================================================
BOOL CSatchel::IsUseable( void )
{
	if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] > 0 ) 
	{
		// player is carrying some satchels
		return TRUE;
	}

	if ( m_chargeReady != 0 )
	{
		// player isn't carrying any satchels, but has some out
		return TRUE;
	}

	return FALSE;
}

BOOL CSatchel::CanDeploy( void )
{
	if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] > 0 ) 
	{
		// player is carrying some satchels
		return TRUE;
	}

	if ( m_chargeReady != 0 )
	{
		// player isn't carrying any satchels, but has some out
		return TRUE;
	}

	return FALSE;
}

BOOL CSatchel::DeployLowKey( )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.25;

	BOOL result = FALSE;

	if ( m_chargeReady )
		result = DefaultDeploy( "models/v_satchel_radio.mdl", "models/p_satchel_radio.mdl", SATCHEL_RADIO_DRAW, "hive" );
	else
		result = DefaultDeploy( "models/v_satchel.mdl", "models/p_weapons.mdl", SATCHEL_DRAW, "trip" );

	if (result)
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.1;
	}

	return result;
}

BOOL CSatchel::Deploy( )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.25;

	BOOL result = FALSE;

	if ( m_chargeReady )
		result = DefaultDeploy( "models/v_satchel_radio.mdl", "models/p_satchel_radio.mdl", SATCHEL_RADIO_DRAW, "hive" );
	else
		result = DefaultDeploy( "models/v_satchel.mdl", "models/p_weapons.mdl", SATCHEL_DRAW2, "trip" );

	if (result)
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.1;
	}

	return result;
}


void CSatchel::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(-1);
	
	if ( m_chargeReady )
	{
		SendWeaponAnim( SATCHEL_RADIO_HOLSTER );
	}
	else
	{
		SendWeaponAnim( SATCHEL_DROP );
	}
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);

	if ( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] && !m_chargeReady )
	{
		m_pPlayer->m_iWeapons2 &= ~(1<<(WEAPON_SATCHEL - 32));
		SetThink( &CSatchel::DestroyItem );
		pev->nextthink = gpGlobals->time + 0.1;
	}
}



void CSatchel::PrimaryAttack()
{
	switch (m_chargeReady)
	{
	case 0:
		{
		Throw( );
		}
		break;
	case 1:
		{
		SendWeaponAnim( SATCHEL_RADIO_FIRE );

		edict_t *pPlayer = m_pPlayer->edict( );

		CBaseEntity *pSatchel = NULL;

		while ((pSatchel = UTIL_FindEntityInSphere( pSatchel, m_pPlayer->pev->origin, 4096 )) != NULL)
		{
			if (FClassnameIs( pSatchel->pev, "monster_satchel"))
			{
				if (pSatchel->pev->owner == pPlayer)
				{
					pSatchel->Use( m_pPlayer, m_pPlayer, USE_ON, 0 );
					m_chargeReady = 2;
				}
			}
		}

#ifndef CLIENT_DLL
		if (g_pGameRules->FAllowMonsters())
		{
			CBaseEntity *pAssassin = NULL;
			while ((pAssassin = UTIL_FindEntityInSphere( pAssassin, m_pPlayer->pev->origin, 4096 )) != NULL)
			{
				if (FClassnameIs( pAssassin->pev, "monster_human_assassin"))
				{
					if (pAssassin->pev->owner == pPlayer)
					{
						CGrenade::Vest( VARS(pAssassin->pev->owner), pAssassin->pev->origin, gSkillData.plrDmgVest );
						pAssassin->pev->solid = SOLID_NOT;
						((CBaseMonster *)pAssassin)->GibMonster();
						pAssassin->pev->effects |= EF_NODRAW;
					}
				}
			}
		}
#endif

		m_chargeReady = 2;
		m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
		break;
		}

	case 2:
		// we're reloading, don't allow fire
		{
		}
		break;
	}
}


void CSatchel::SecondaryAttack( void )
{
	if ( m_chargeReady != 2 )
	{
		Throw( );
	}
}


void CSatchel::Throw( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
		Vector vecSrc = m_pPlayer->pev->origin;
		Vector vecThrow = vecAiming * 274 + m_pPlayer->pev->velocity;

#ifndef CLIENT_DLL
		CBaseEntity *pSatchel = Create( "monster_satchel", vecSrc, vecAiming, m_pPlayer->edict() );
		if (pSatchel)
		{
			pSatchel->pev->velocity = vecThrow;
			pSatchel->pev->avelocity.y = 400;
		}

		m_pPlayer->pev->viewmodel = MAKE_STRING("models/v_satchel_radio.mdl");
		m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_satchel_radio.mdl");
#else
		LoadVModel ( "models/v_satchel_radio.mdl", m_pPlayer );
#endif

		SendWeaponAnim( SATCHEL_RADIO_DRAW );

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		m_pPlayer->pev->punchangle = Vector(-4, -2, -4);

		m_chargeReady = 1;
		
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		m_flNextPrimaryAttack = GetNextAttackDelay(1.0);
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
	}
}


void CSatchel::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	switch( m_chargeReady )
	{
	case 0:
		SendWeaponAnim( SATCHEL_FIDGET1 );
		// use tripmine animations
		strcpy( m_pPlayer->m_szAnimExtention, "trip" );
		break;
	case 1:
		SendWeaponAnim( SATCHEL_RADIO_FIDGET1 );
		// use hivehand animations
		strcpy( m_pPlayer->m_szAnimExtention, "hive" );
		break;
	case 2:
		if ( !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
		{
			m_chargeReady = 0;
			RetireWeapon();
			return;
		}

#ifndef CLIENT_DLL
		m_pPlayer->pev->viewmodel = MAKE_STRING("models/v_satchel.mdl");
		m_pPlayer->pev->weaponmodel = MAKE_STRING("models/p_weapons.mdl");
#else
		LoadVModel ( "models/v_satchel.mdl", m_pPlayer );
#endif

		SendWeaponAnim( SATCHEL_DRAW );

		// use tripmine animations
		strcpy( m_pPlayer->m_szAnimExtention, "trip" );

		m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
		m_chargeReady = 0;
		break;
	}
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );// how long till we do this again.
}

//=========================================================
// DeactivateSatchels - removes all satchels owned by
// the provided player. Should only be used upon death.
//
// Made this global on purpose.
//=========================================================
void DeactivateSatchels( CBasePlayer *pOwner )
{
	edict_t *pFind; 

	pFind = FIND_ENTITY_BY_CLASSNAME( NULL, "monster_satchel" );

	while ( !FNullEnt( pFind ) )
	{
		CBaseEntity *pEnt = CBaseEntity::Instance( pFind );
		CSatchelCharge *pSatchel = (CSatchelCharge *)pEnt;

		if ( pSatchel )
		{
			if ( pSatchel->pev->owner == pOwner->edict() )
			{
				pSatchel->Deactivate();
			}
		}

		pFind = FIND_ENTITY_BY_CLASSNAME( pFind, "monster_satchel" );
	}
}

void DeactivateItems( CBasePlayer *pOwner, const char *item )
{
	edict_t *pFind; 

	pFind = FIND_ENTITY_BY_CLASSNAME( NULL, item );

	while ( !FNullEnt( pFind ) )
	{
		CBaseEntity *pEnt = CBaseEntity::Instance( pFind );

		if ( pEnt )
		{
			if ( pEnt->pev->owner == pOwner->edict() ||
				 pEnt->pev->euser1 == pOwner->edict() )
			{
				pEnt->pev->solid = SOLID_NOT;
				UTIL_Remove( pEnt );
			}
		}

		pFind = FIND_ENTITY_BY_CLASSNAME( pFind, item );
	}
}

#endif
