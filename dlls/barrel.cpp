/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
#include "decals.h"
#include "effects.h"
#include "explode.h"
#include "game.h"
#include "weapons.h"
#include "monsters.h"

extern DLL_GLOBAL const char *g_MutatorPaintball;

#define SF_RANDOM		0x0001
#define SF_BARREL		0x0002
#define SF_CABINET		0x0004

class CBarrel : public CBaseEntity
{
public:
	void Precache ( void );
	void Spawn( void );
	void EXPORT BarrelThink( void );
	void EXPORT BarrelTouch( CBaseEntity *pOther );
	void StartFlames( void );
	void BarrelExplode( void );
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );

private:
	CSprite *pSprite = NULL;
	EHANDLE pLastAttacker;
	float m_fTimeToDie = 0;
};

LINK_ENTITY_TO_CLASS( monster_barrel, CBarrel );

void CBarrel::Precache( void )
{
	PRECACHE_MODEL("models/w_barrel.mdl");
	PRECACHE_MODEL("models/w_cabinet.mdl");
	PRECACHE_SOUND("debris/bustmetal1.wav");
	PRECACHE_SOUND("debris/bustmetal2.wav");
}

void CBarrel::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = 0.5;
	pev->nextthink = gpGlobals->time + 0.1;
	pev->solid = SOLID_BBOX;
	pev->health = 40;
	pev->takedamage = DAMAGE_AIM;
	pev->dmg = gSkillData.plrDmgHandGrenade;
	pev->classname = MAKE_STRING("monster_barrel");

	if ( pev->owner )
		pLastAttacker = Instance( pev->owner );

	if ( pev->spawnflags & SF_BARREL )
		SET_MODEL( edict(), "models/w_barrel.mdl");
	else if ( pev->spawnflags & SF_CABINET )
		SET_MODEL( edict(), "models/w_cabinet.mdl");
	else 
	{
		if (RANDOM_LONG(0,1))
			SET_MODEL( edict(), "models/w_cabinet.mdl");
		else
			SET_MODEL( edict(), "models/w_barrel.mdl");
	}
	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);
	//pev->angles = g_vecZero;
	m_fTimeToDie = gpGlobals->time + 25.0;

	SetTouch(&CBarrel::BarrelTouch);
	SetThink(&CBarrel::BarrelThink);
	pev->nextthink = gpGlobals->time + 0.1;

	//ALERT(at_aiconsole, "Created %s[%p]\n", STRING(pev->classname), pev);
}

void CBarrel::BarrelThink( void )
{
	if (!IsInWorld())
	{
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}

/*
	if (pSprite) {
		pSprite->pev->origin = pev->origin;
		pSprite->pev->origin.z += 32;

		if (pSprite->pev->dmgtime > 0 && pSprite->pev->dmgtime < gpGlobals->time)
		{
			pSprite = NULL;
			//ALERT(at_aiconsole, "called think %s[%p]\n", STRING(pev->classname), pev);
			SetThink(&CBarrel::BarrelExplode);
			pev->nextthink = gpGlobals->time + 0.1;
			return;
		}
	}
*/

	if (m_fTimeToDie && m_fTimeToDie < gpGlobals->time) {
		/*if (pSprite == NULL) {
			StartFlames();
		}*/
		SetThink(&CBarrel::BarrelExplode);
		pev->nextthink = gpGlobals->time + 0.1;
		m_fTimeToDie = 0;
	}

	pev->nextthink = gpGlobals->time + 0.25;
}

void CBarrel::BarrelTouch( CBaseEntity *pOther )
{
	//ALERT(at_aiconsole, "touched at vel=%.2f %s[%p]\n", pev->velocity.Length(), STRING(pev->classname), pev);
	if (pev->health <= 0)
		return;

	if (pev->velocity.Length() > 100)
	{
		switch ( RANDOM_LONG(0,1) )
		{
		case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/bustmetal1.wav", 1.0, 1.0);	
			break;
		case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/bustmetal2.wav", 1.0, 1.0);	
			break;
		}
	}

	if (pev->velocity.Length() > 500) {
		//ALERT(at_aiconsole, "called touch %s[%p]\n", STRING(pev->classname), pev);
		SetTouch(NULL);
		//pLastAttacker = pOther;
		SetThink(&CBarrel::BarrelExplode);
		pev->nextthink = gpGlobals->time + 0.1;
	}

	// Support if picked up and dropped
	pev->velocity = pev->velocity * 0.5;
	pev->avelocity = pev->avelocity * 0.5;
}

int CBarrel::TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if (!pev->takedamage)
		return 0;

	switch ( RANDOM_LONG(0,1) )
	{
	case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/bustmetal1.wav", 1.0, 1.0);	
		break;
	case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/bustmetal2.wav", 1.0, 1.0);	
		break;
	}

	pLastAttacker = CBaseEntity::Instance(pevAttacker);
	pev->health -= flDamage;

/*
	if (pev->health <= 20)
	{
		if (pSprite == NULL) {
			StartFlames();
		}
	}
*/

	if (pev->health <= 0)
	{
		//ALERT(at_aiconsole, "take damage %s[%p]\n", STRING(pev->classname), pev);
		pev->takedamage = DAMAGE_NO;
		SetThink(&CBarrel::BarrelExplode);
		pev->nextthink = gpGlobals->time + 0.1;
		return 0;
	}

	return 1;
}

void CBarrel::StartFlames( void )
{
	if (pSprite)
		return;

	Vector origin = pev->origin;
	if (icesprites.value)
		pSprite = CSprite::SpriteCreate( "sprites/ice_fire.spr", origin, TRUE );
	else
		pSprite = CSprite::SpriteCreate( "sprites/fire.spr", origin, TRUE );
	if (pSprite != NULL)
	{
		pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
		pSprite->SetScale( RANDOM_FLOAT(0.75, 1.0) );
		float time = RANDOM_FLOAT(3.0, 6.0);
		pSprite->pev->dmgtime = gpGlobals->time + time;
		pSprite->pev->dmg_save = 1;
		pSprite->pev->framerate = 16;
		pSprite->TurnOn();
		pSprite->AnimateUntilDead();
	}
}

void CBarrel::BarrelExplode( void ) {
	//ALERT(at_aiconsole, "SUB_Remove %s[%p]\n", STRING(pev->classname), pev);
	SetTouch( NULL );
	pev->effects |= EF_NODRAW;
	pev->solid = SOLID_NOT;
	pev->health = 0;
	pev->takedamage = DAMAGE_NO;

/*
	if (pSprite) {
		pSprite->TurnOff();
	}
*/

	Vector vecSpot = pev->origin;
	Vector speed = -(pev->velocity) / 4;
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSpot );
		WRITE_BYTE(TE_BREAKMODEL);

		// position
		WRITE_COORD( vecSpot.x );
		WRITE_COORD( vecSpot.y );
		WRITE_COORD( vecSpot.z );

		// size
		WRITE_COORD( 16 );
		WRITE_COORD( 16 );
		WRITE_COORD( 16 );

		// velocity
		WRITE_COORD( speed.x );
		WRITE_COORD( speed.y );
		WRITE_COORD( speed.z );

		// randomization
		WRITE_BYTE( 10 );

		// Model
		WRITE_SHORT( g_sModelConcreteGibs );	//model id#

		// # of shards
		WRITE_BYTE( 5 );

		// duration
		WRITE_BYTE( RANDOM_LONG(5,20) );

		// flags
		WRITE_BYTE( BREAK_METAL );
	MESSAGE_END();

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
	if (strstr(mutators.string, g_MutatorPaintball) ||
		atoi(mutators.string) == MUTATOR_PAINTBALL) {
		decal = DECAL_PAINTL1;
		index = RANDOM_LONG(0, 7);
	}
	UTIL_DecalTrace(&tr, decal + index);

	entvars_t *attacker = NULL;
	if (pLastAttacker != NULL)
		attacker = VARS(pLastAttacker->edict());
	::RadiusDamage( pev->origin, pev, attacker, pev->dmg, pev->dmg  * 2.5, CLASS_NONE, DMG_BURN );
	pLastAttacker = NULL;

	SetThink( &CBaseEntity::SUB_Remove );
	pev->nextthink = gpGlobals->time + 0.1;
}
