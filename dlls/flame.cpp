#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"
#include "decals.h"
#include "flame.h"

#define FLDMG 2

extern int gmsgParticle;

CFlame *CFlame::CreateFlameStream( entvars_t *pevOwner, Vector vecStart, Vector velocity, float damage )
{
	CFlame *pFlame = GetClassPtr( (CFlame *)NULL );
#ifndef CLIENT_DLL
	pFlame->Spawn();
#endif
	pFlame->pev->origin = vecStart;
	pFlame->pev->owner = ENT(pevOwner);
	pFlame->pev->velocity = velocity;
	pFlame->pev->velocity.x *= RANDOM_FLOAT(0.9, 1);
	pFlame->pev->velocity.y *= RANDOM_FLOAT(0.9, 1);
	pFlame->pev->velocity.z *= RANDOM_FLOAT(0.9, 1);
	pFlame->pev->rendercolor.x = 0;
	pFlame->pev->rendercolor.y = 0;
	pFlame->pev->rendercolor.z = 0;
	pFlame->pev->renderamt = 0;
	pFlame->pev->dmg = damage;
	pFlame->pev->classname = MAKE_STRING( "flamestream" );

	return pFlame;
}

void CFlame::Spawn( void )
{
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->takedamage = DAMAGE_NO;
	//view_additions = 0;
	SET_MODEL(ENT(pev), "sprites/null.spr"); 
	pev->rendermode = kRenderTransAdd;
	SetTouch( &CFlame::FlameTouch );
	pev->gravity = 0;
	pev->effects = EF_NODRAW;
	SetThink( &CFlame::FlameThink );
	pev->nextthink = gpGlobals->time + 0.05;
	//pev->renderfx = kRenderFxEntInPVS;
	pev->scale = 1;
	pev->friction = 0;
	pev->playerclass = 0;
	pev->flags = 0;
	UTIL_SetOrigin(pev, pev->origin);
	UTIL_SetSize(pev, Vector (0,0,0), Vector ( 0,0,0 ));
	starttime = gpGlobals->time + 0.25;
	dmgtime = gpGlobals->time;
	implacedmydecal = 0;
}

LINK_ENTITY_TO_CLASS( flamestream, CFlame );

/*
TYPEDESCRIPTION	CFlame::m_SaveData[] = 
{
	DEFINE_FIELD( CFlame, starttime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CFlame, CPointEntity );
*/

void CFlame::DieThink( void )
{
	SetTouch( NULL );

	if (gpGlobals->time <= starttime )
	{
		CBaseEntity *pEntity = NULL;
	
		while ((pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, pev->scale * 30 )) != NULL)
		{
			if ( FVisible( pEntity ) )
			{
				if (pEntity->pev->takedamage)
				{
					pEntity->m_fBurnTime = pEntity->m_fBurnTime + pev->scale / 16;
	
					if (pEntity->IsPlayer() && pEntity->m_hFlameOwner == NULL)
					{
						pEntity->m_hFlameOwner = Instance(pev->owner);
						pEntity->TakeDamage ( VARS( pev->owner ), pEntity->m_hFlameOwner->pev, FLDMG , DMG_BURN | DMG_NEVERGIB );
					}
					else
						pEntity->TakeDamage ( VARS( pev->owner ), VARS( pev->owner ), FLDMG , DMG_BURN | DMG_NEVERGIB );
	
				}
			}
		}
	}
	else
	{
		UTIL_Remove(this);
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

int CFlame::TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	return 0;
}

void CFlame::FlameThink( void )
{
	if (gpGlobals->time >= starttime)
	{
		UTIL_Remove(this);
		return;
	}

	if (dmgtime < gpGlobals->time)
	{
		CBaseEntity *pEntity = NULL;

		while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, 20)) != NULL)
		{
			if (pEntity->pev->takedamage)
			{
				if (FVisible(pEntity))
				{
					float flDist = (pEntity->Center() - pev->origin).Length();
					float flAdjustedDamage = pev->dmg;

					flAdjustedDamage *= 1 - (flDist / 20.);

					if (flAdjustedDamage < 1)
						flAdjustedDamage = 1;

					pEntity->TakeDamage(VARS(pev->owner), VARS(pev->owner), flAdjustedDamage, DMG_BURN | DMG_NEVERGIB);

					if (pEntity->edict() != pev->owner)
					{
						pEntity->m_fBurnTime = pEntity->m_fBurnTime + 0.2;

						if (pEntity->IsPlayer())
						{
							pEntity->m_hFlameOwner = Instance(pev->owner);
						}
					}
				}
			}
		}

		dmgtime = gpGlobals->time + 0.05;
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

void CFlame::FlameTouch( CBaseEntity *pOther )
{
	TraceResult tr = UTIL_GetGlobalTrace();

	//if (CanPlaceDecal(pOther))
	//	UTIL_CustomDecal( &tr, "burn", 0 );

	CBaseEntity *pEntity = NULL;

	while ((pEntity = UTIL_FindEntityInSphere(pEntity, pev->origin, 20)) != NULL)
	{
		if (pEntity->pev->takedamage)
		{
			if (FVisible(pEntity))
			{
				float flDist = (pEntity->Center() - pev->origin).Length();
				float flAdjustedDamage = pev->dmg;

				flAdjustedDamage *= 1 - (flDist / 20.);

				if (flAdjustedDamage < 1)
					flAdjustedDamage = 1;

				pEntity->TakeDamage(VARS(pev->owner), VARS(pev->owner), flAdjustedDamage, DMG_BURN | DMG_NEVERGIB);
			}
		}
	}

	if (pOther && pOther->pev->takedamage)
	{
		pOther->m_fBurnTime += 1;

		if (pOther->IsPlayer())
			pOther->m_hFlameOwner = Instance(pev->owner);
	}

	pev->velocity = Vector(0, 0, 0);
	UTIL_Remove(this);
}
