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

enum vest_radio_e {
	VEST_RADIO_IDLE1 = 0,
	VEST_RADIO_FIDGET1,
	VEST_RADIO_DRAW,
	VEST_RADIO_FIRE,
	VEST_RADIO_HOLSTER
};

int CVest::AddToPlayer( CBasePlayer *pPlayer )
{
	int bResult = CBasePlayerItem::AddToPlayer( pPlayer );

	pPlayer->pev->weapons |= (1<<m_iId);

	if ( bResult )
	{
		return AddWeapon( );
	}

	return FALSE;
}

void CVest::Spawn( )
{
	Precache( );
	m_iId = WEAPON_VEST;
	SET_MODEL(ENT(pev), "models/w_vest.mdl");

	m_iDefaultAmmo = SATCHEL_DEFAULT_GIVE;

	FallInit();
}

void CVest::Precache( void )
{
	PRECACHE_MODEL("models/v_vest_radio.mdl");
	PRECACHE_MODEL("models/w_vest.mdl");
	PRECACHE_MODEL("models/p_vest.mdl");

	PRECACHE_SOUND("vest_attack.wav");
	PRECACHE_SOUND("vest_alive.wav");
	PRECACHE_SOUND("vest_equip.wav");
}

int CVest::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Dynamite";
	p->iMaxAmmo1 = 1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 6;
	p->iFlags = ITEM_FLAG_SELECTONEMPTY | ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	p->iId = m_iId = WEAPON_VEST;
	p->iWeight = SATCHEL_WEIGHT;
	p->pszDisplayName = "Dynamite Vest";

	return 1;
}

BOOL CVest::IsUseable( void )
{
	if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] > 0 ) 
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CVest::CanDeploy( void )
{
	if ( m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] > 0 ) 
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CVest::Deploy( )
{
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "vest_equip.wav", 1, ATTN_NORM);
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );

	return DefaultDeploy( "models/v_vest_radio.mdl", "models/p_vest.mdl", VEST_RADIO_DRAW, "hive" );
}

void CVest::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	
	SendWeaponAnim( VEST_RADIO_HOLSTER );
}

void CVest::PrimaryAttack()
{
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "vest_attack.wav", 1, ATTN_NORM);

	SetThink( &CVest::BlowThink );
	pev->nextthink = gpGlobals->time + 1.0;

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 2.0;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase();
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
}

void CVest::BlowThink() {
	SendWeaponAnim( VEST_RADIO_FIRE );
	SetThink( &CVest::GoneThink );

	for ( int i = 0; i < RANDOM_LONG(1,3); i++ ) {
		Create( "spark_shower", pev->origin, pev->angles, NULL );
	}

	m_pPlayer->SetAnimation( PLAYER_JUMP );
	pev->nextthink = gpGlobals->time + 0.5;
}

void CVest::GoneThink() {
	m_pPlayer->pev->health = 0; // without this, player can walk as a ghost.
	m_pPlayer->Killed(m_pPlayer->pev, pev, GIB_ALWAYS);
	CGrenade::Vest( m_pPlayer->pev, pev->origin );

	STOP_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "vest_attack.wav");
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, m_pPlayer->pev->origin );
		WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( m_pPlayer->pev->origin.x);
		WRITE_COORD( m_pPlayer->pev->origin.y);
		WRITE_COORD( m_pPlayer->pev->origin.z);
		WRITE_COORD( m_pPlayer->pev->origin.x);
		WRITE_COORD( m_pPlayer->pev->origin.y);
		WRITE_COORD( m_pPlayer->pev->origin.z + 600 );
		WRITE_SHORT( PRECACHE_MODEL( "sprites/lgtning.spr" ) );
		WRITE_BYTE( 0 ); // startframe
		WRITE_BYTE( 0 ); // framerate
		WRITE_BYTE( 4 ); // life
		WRITE_BYTE( 32 );  // width
		WRITE_BYTE( 0 );   // noise
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 255 );   // r, g, b
		WRITE_BYTE( 192 );   // r, g, b
		WRITE_BYTE( 128 ); // brightness
		WRITE_BYTE( 0 );		// speed
	MESSAGE_END();
}

void CVest::SecondaryAttack()
{
	pev->nextthink = -1;
	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "vest_alive.wav", 1, ATTN_NORM);
	SendWeaponAnim( VEST_RADIO_HOLSTER );

	SetThink( &CVest::RetireThink );
	pev->nextthink = gpGlobals->time + 1.0;

	m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 2.0;
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
}

void CVest::RetireThink( )
{
	RetireWeapon();
}

void CVest::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	SendWeaponAnim( VEST_RADIO_FIDGET1 );
	strcpy( m_pPlayer->m_szAnimExtention, "hive" );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );// how long till we do this again.
}

#ifdef VEST
LINK_ENTITY_TO_CLASS( weapon_vest, CVest );
#endif
