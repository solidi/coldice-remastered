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
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "coldskull_gamerules.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"

extern int gmsgObjective;
extern int gmsgScoreInfo;
extern int gmsgItemPickup;

class CSkullCharm : public CBaseEntity
{
public:
	void Precache( void );
	void Spawn( void );
	void EXPORT SkullTouch( CBaseEntity *pOther );
	void EXPORT ThinkSolid();
	BOOL ShouldCollide( CBaseEntity *pOther );
};

void CSkullCharm::Precache( void )
{
	PRECACHE_MODEL ("models/w_runes.mdl");
	PRECACHE_SOUND ("rune_pickup.wav");
}

void CSkullCharm::Spawn( void )
{
	Precache();
	SET_MODEL(ENT(pev), "models/w_runes.mdl");

	pev->body = 10;
	pev->fuser4 = 1;
	
	pev->renderfx = kRenderFxGlowShell;
	pev->renderamt = 5;

	pev->angles.x = 0;
	pev->angles.z = 0;
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch( &CSkullCharm::SkullTouch );
	SetThink( &CSkullCharm::ThinkSolid );
	pev->nextthink = gpGlobals->time + 0.5;

	//animate
	pev->sequence = 0;
	pev->animtime = gpGlobals->time + RANDOM_FLOAT(0.0, 0.75);
	pev->framerate = 1.0;
}

void CSkullCharm::ThinkSolid( void )
{
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-16, -16, 0), Vector(16, 16, 16));
	SetThink( &CBaseEntity::SUB_StartFadeOut );
	pev->nextthink = gpGlobals->time + 25.0;
}

BOOL CSkullCharm::ShouldCollide( CBaseEntity *pOther )
{
	if (pev->modelindex == pOther->pev->modelindex)
		return FALSE;

	return TRUE;
}

void CSkullCharm::SkullTouch( CBaseEntity *pOther )
{
	// Support if picked up and dropped
	pev->velocity = pev->velocity * 0.5;
	pev->avelocity = pev->avelocity * 0.5;

	if ( !(pev->flags & FL_ONGROUND ) )
		return;

	// if it's not a player, ignore
	if ( !pOther->IsPlayer() )
		return;

	if ( pOther->pev->deadflag != DEAD_NO )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	if (!pPlayer->HasDisconnected)
	{
		EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, "rune_pickup.wav", 1, ATTN_NORM );

		pPlayer->pev->frags += pev->fuser1;
		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
			WRITE_SHORT( pPlayer->pev->frags );
			WRITE_SHORT( pPlayer->m_iDeaths );
			WRITE_SHORT( pPlayer->m_iRoundWins );
			WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
		MESSAGE_END();

		if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
		{
			int frags = fraglimit.value;
			int myfrags = (int)pPlayer->pev->frags;
			if (frags == 0)
				frags = 100;
			int result = (myfrags / frags) * 100;
			MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pPlayer->edict());
				WRITE_STRING("Collect the skulls");
				WRITE_STRING(UTIL_VarArgs("Your progress: %d of %d", myfrags, frags));
				WRITE_BYTE(result);
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_ONE, gmsgItemPickup, NULL, pPlayer->pev );
				WRITE_STRING( STRING(pev->classname) );
			MESSAGE_END();

			// End session if hit skull limit
			if ( myfrags >= frags )
			{
				g_pGameRules->EndMultiplayerGame();
			}
		}

		SetTouch( NULL );
		UTIL_Remove( this );
	}
}

LINK_ENTITY_TO_CLASS( skull, CSkullCharm );

CHalfLifeColdSkull::CHalfLifeColdSkull()
{
	UTIL_PrecacheOther("skull");
}

void CHalfLifeColdSkull::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		int frags = fraglimit.value;
		if (frags == 0)
			frags = 100;
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Collect the skulls");
			WRITE_STRING(UTIL_VarArgs("Your progress: 0 of %d", frags));
			WRITE_BYTE(0);
		MESSAGE_END();
	}
}

void CreateSkull( CBasePlayer *pVictim, int amount )
{
	CSkullCharm *pSkull = (CSkullCharm *)CBaseEntity::Create("skull", pVictim->GetGunPosition( ), pVictim->pev->angles, NULL);
	if (pSkull != NULL)
	{
		pSkull->pev->velocity.x = RANDOM_FLOAT( -300, 300 );
		pSkull->pev->velocity.y = RANDOM_FLOAT( -300, 300 );
		pSkull->pev->velocity.z = RANDOM_FLOAT( 0, 300 );
		pSkull->pev->fuser1 = amount;

		/*
		1 - Bronze Skull
		5 - Silver Skull
		10 - Gold Skull
		20 - Purple Crystal Skull
		40 - Flaming Red Crystal Skull
		*/
		if (amount == 40)
			pSkull->pev->rendercolor = Vector(255, 0, 0);
		else if (amount == 20)
			pSkull->pev->rendercolor = Vector(255, 0, 128);
		else if (amount == 10)
			pSkull->pev->rendercolor = Vector(255, 0, 128);
		else if (amount == 5)
			pSkull->pev->rendercolor = Vector(128, 128, 128);
		else if (amount == 1)
			pSkull->pev->rendercolor = Vector(255, 128, 128);
	}
}

void CHalfLifeColdSkull::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	CHalfLifeMultiplay::PlayerKilled( pVictim, pKiller, pInflictor );

	{
		UTIL_MakeVectors(pVictim->pev->v_angle);
		//DropCharm(pVictim, pVictim->pev->origin + gpGlobals->v_forward * 64);
		int remain = (int)pVictim->pev->frags;
		if (remain > 0)
			pVictim->pev->frags /= 2;
		else
			pVictim->pev->frags = 0;

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(pVictim->edict()) );
			WRITE_SHORT( pVictim->pev->frags );
			WRITE_SHORT( pVictim->m_iDeaths );
			WRITE_SHORT( 0 );
			WRITE_SHORT( GetTeamIndex( pVictim->m_szTeamName ) + 1 );
		MESSAGE_END();

		int frags = fraglimit.value;
		int myfrags = (int)pVictim->pev->frags;
		if (frags == 0)
			frags = 100;
		int result = (myfrags / frags) * 100;
		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, pVictim->edict());
			WRITE_STRING("Collect the skulls");
			WRITE_STRING(UTIL_VarArgs("Your progress: %d of %d", myfrags, frags));
			WRITE_BYTE(result);
		MESSAGE_END();

		if (remain <= 0)
		{
			//ALERT(at_aiconsole, ">>> create 1 skull\n");
			CreateSkull( pVictim, 1 );
		}
		else
		{
			while (remain > 0)
			{
				int giveout = 0;
				if (remain >= 40)
					giveout = 40;
				else if (remain >= 20 && remain < 40)
					giveout = 20;
				else if (remain >= 10 && remain < 20)
					giveout = 10;
				else if (remain >= 5 && remain < 10)
					giveout = 5;
				else if (remain >= 1 && remain < 5)
					giveout = 1;

				//ALERT(at_aiconsole, ">>> create %d skull\n", giveout);
				remain -= giveout;
				CreateSkull( pVictim, giveout );
			}
		}
	}
}

int CHalfLifeColdSkull::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return 0;
}
