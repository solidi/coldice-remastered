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

#ifdef VICE
LINK_ENTITY_TO_CLASS( weapon_vice, CVice );
#endif

enum vice_e {
	VICE_IDLE = 0,
	VICE_SMOKE,
	VICE_DRINK,
	VICE_DRAW_LOWKEY,
	VICE_DRAW,
	VICE_HOLSTER,
};

void CVice::Spawn( )
{
	Precache( );
	m_iId = WEAPON_VICE;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_CROWBAR - 1;

	m_iClip = -1;
	FallInit();// get ready to fall down.
}

void CVice::Precache( void )
{
	PRECACHE_MODEL("models/v_vice.mdl");
	PRECACHE_SOUND("drinking.wav");
	PRECACHE_SOUND("smoking.wav");

	m_usVice = PRECACHE_EVENT ( 1, "events/vice.sc" );
}

int CVice::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}

int CVice::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 1; // Allow server-side reload
	p->iSlot = 6;
	p->iPosition = 5;
	p->iId = WEAPON_VICE;
	p->iFlags = ITEM_FLAG_NODROP;
	p->iWeight = VICE_WEIGHT;
	p->pszDisplayName = "Vice";
	return 1;
}

BOOL CVice::DeployLowKey( )
{
	return DefaultDeploy( "models/v_vice.mdl", iStringNull, VICE_DRAW_LOWKEY, "crowbar" );
}

BOOL CVice::Deploy( )
{
	return DefaultDeploy( "models/v_vice.mdl", iStringNull, VICE_DRAW, "crowbar" );
}

void CVice::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayerWeapon::DefaultHolster(VICE_HOLSTER);
}

void CVice::PrimaryAttack()
{
	SendWeaponAnim( VICE_SMOKE );
	m_ViceMode = 1;
	SetThink( &CVice::ReduceHealth );
	pev->nextthink = gpGlobals->time + 1.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(4.0);
}

void CVice::SecondaryAttack()
{
	SendWeaponAnim( VICE_DRINK );
	m_ViceMode = 2;
	SetThink( &CVice::ReduceHealth );
	pev->nextthink = gpGlobals->time + 2.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(4.0);

}

void CVice::ReduceHealth()
{
	if (m_ViceMode == 1)
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "smoking.wav", 1, ATTN_NORM);
	else
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_VOICE, "drinking.wav", 1, ATTN_NORM);
	m_pPlayer->TakeDamage( pev, pev, RANDOM_LONG(1, 5), DMG_POISON );
	pev->nextthink = -1;
}

void CVice::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
}
