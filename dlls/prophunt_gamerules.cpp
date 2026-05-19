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
#include "prophunt_gamerules.h"
#include "game.h"
#include "items.h"
#include "voice_gamemgr.h"
#include "shake.h"

extern int gmsgObjective;
extern int gmsgTeamNames;
extern int gmsgTeamInfo;
extern int gmsgScoreInfo;
extern int gmsgStatusIcon;
extern int gmsgShowTimer;
extern int gmsgPlayClientSound;
extern int gmsgDEraser;
extern int gmsgBanner;

class CPropDecoy : public CBaseEntity
{
public:
	void Precache ( void );
	void Spawn( void );
	virtual int ObjectCaps( void ) { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_PORTAL; }
	void EXPORT PropDecoyTouch( CBaseEntity *pOther );
	void EXPORT PropDecoyThink( void );
	BOOL EXPORT ShouldCollide( CBaseEntity *pOther );
	void Killed( entvars_t *pevAttacker, int iGib );

private:
	EHANDLE m_hOwner;

};

LINK_ENTITY_TO_CLASS( monster_propdecoy, CPropDecoy );

void CPropDecoy::Precache( void )
{
	PRECACHE_MODEL("models/w_weapons.mdl");
}

void CPropDecoy::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = 1.0;
	pev->solid = SOLID_BBOX;
	pev->health = 1;
	pev->takedamage = DAMAGE_YES;
	pev->classname = MAKE_STRING("monster_propdecoy");

	SET_MODEL( edict(), "models/w_weapons.mdl");

	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, Vector(16,16,32));
	UTIL_SetOrigin( pev, pev->origin );

	SetTouch(&CPropDecoy::PropDecoyTouch);
	SetThink(&CPropDecoy::PropDecoyThink);
	pev->nextthink = gpGlobals->time + 0.25;

	pev->sequence = (pev->body * 2) + floatingweapons.value;
	pev->framerate = 1.0;

	if ( pev->owner )
	{
		m_hOwner = Instance(pev->owner);

		CBasePlayer *plr = (CBasePlayer *)Instance(pev->owner);
		plr->m_iPropsDeployed++;
		ClientPrint(plr->pev, HUD_PRINTCENTER, UTIL_VarArgs("%d decoys are deployed.\n", plr->m_iPropsDeployed));
	}

	//pev->owner = NULL;
}

void CPropDecoy::Killed( entvars_t *pevAttacker, int iGib )
{
	if (pevAttacker)
	{
		CBasePlayer *plr = GetClassPtr((CBasePlayer *)pevAttacker);

		if (m_hOwner)
			((CBasePlayer *)(CBaseEntity *)m_hOwner)->m_iPropsDeployed--;

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_TAREXPLOSION );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
		MESSAGE_END();

		if (plr && plr->IsPlayer())
		{
			if (!m_hOwner || plr->pev != m_hOwner->pev)
			{
				MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
					WRITE_BYTE( ENTINDEX(plr->edict()) );
					WRITE_SHORT( ++plr->pev->frags );
					WRITE_SHORT( plr->m_iDeaths );
					WRITE_SHORT( plr->m_iRoundWins );
					WRITE_SHORT( g_pGameRules->GetTeamIndex( plr->m_szTeamName ) + 1 );
				MESSAGE_END();

				if (m_hOwner)
				{
					CBasePlayer *pOwner = (CBasePlayer *)(CBaseEntity *)m_hOwner;
					ClientPrint(m_hOwner->pev, HUD_PRINTCENTER, UTIL_VarArgs("+1 point for decoy touch. (%d decoys left)\n", pOwner->m_iPropsDeployed));
				}
			}
			else
			{
				CBasePlayer *pOwner = (CBasePlayer *)(CBaseEntity *)m_hOwner;
				ClientPrint(m_hOwner->pev, HUD_PRINTCENTER, UTIL_VarArgs("%d decoys are deployed.\n", pOwner->m_iPropsDeployed));
			}
		}
		else if (m_hOwner)
		{
			CBasePlayer *pOwner = (CBasePlayer *)(CBaseEntity *)m_hOwner;
			ClientPrint(m_hOwner->pev, HUD_PRINTCENTER, UTIL_VarArgs("%d decoys are deployed.\n", pOwner->m_iPropsDeployed));
		}
	}

	CBaseEntity::Killed( pevAttacker, iGib );
}

void CPropDecoy::PropDecoyThink( void )
{
	CBaseEntity *ent = NULL;

	while ( (ent = UTIL_FindEntityInSphere( ent, pev->origin, 32 )) != NULL )
	{
		CBasePlayer *plr = (CBasePlayer *)ent;
		if (plr && plr->IsPlayer() && plr->IsAlive() && !plr->IsObserver() && plr->IsInArena)
		{
			PropDecoyTouch(plr);
			return; // PropDecoyTouch may remove this entity; do not access pev after
		}
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

BOOL CPropDecoy::ShouldCollide( CBaseEntity *pOther )
{
	if (m_hOwner && pOther->pev == m_hOwner->pev)
		return FALSE;

	return TRUE;
}

void CPropDecoy::PropDecoyTouch( CBaseEntity *pOther )
{
	if (pOther->IsPlayer() && pOther->IsAlive())
	{
		CBasePlayer *plr = (CBasePlayer *)pOther;

		if (m_hOwner)
			((CBasePlayer *)(CBaseEntity *)m_hOwner)->m_iPropsDeployed--;

		SetTouch(NULL);
		SetThink(NULL);

		if (!m_hOwner || ((CBaseEntity*)m_hOwner) != pOther)
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
				WRITE_BYTE( ENTINDEX(plr->edict()) );
				WRITE_SHORT( ++plr->pev->frags );
				WRITE_SHORT( plr->m_iDeaths );
				WRITE_SHORT( plr->m_iRoundWins );
				WRITE_SHORT( g_pGameRules->GetTeamIndex( plr->m_szTeamName ) + 1 );
			MESSAGE_END();

			if (m_hOwner)
			{
				CBasePlayer *pOwner = (CBasePlayer *)(CBaseEntity *)m_hOwner;
				ClientPrint(m_hOwner->pev, HUD_PRINTCENTER, UTIL_VarArgs("+1 point for decoy touch. (%d decoys left)\n", pOwner->m_iPropsDeployed));
			}
		}
		else
		{
			CBasePlayer *pOwner = (CBasePlayer *)(CBaseEntity *)m_hOwner;
			ClientPrint(m_hOwner->pev, HUD_PRINTCENTER, UTIL_VarArgs("%d decoys are deployed.\n", pOwner->m_iPropsDeployed));
		}

		// Play catch sound
		EMIT_SOUND_DYN(pOther->edict(), CHAN_WEAPON, "items/gunpickup2.wav", 1.0, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
		UTIL_Remove(this);
	}
}

void DeactivateDecoys( CBasePlayer *pOwner )
{
	edict_t *pFind; 

	pFind = FIND_ENTITY_BY_CLASSNAME( NULL, "monster_propdecoy" );

	while ( !FNullEnt( pFind ) )
	{
		CBaseEntity *pEnt = CBaseEntity::Instance( pFind );

		if ( pEnt )
		{
			if ( pEnt->pev->owner == pOwner->edict() )
			{
				pEnt->pev->solid = SOLID_NOT;
				UTIL_Remove( pEnt );
			}
		}

		pFind = FIND_ENTITY_BY_CLASSNAME( pFind, "monster_propdecoy" );
	}
}

// ----------

CHalfLifePropHunt::CHalfLifePropHunt()
{
	m_iHuntersRemain = m_iHuntersStarted = 0;
	m_iPropsRemain = m_iPropsStarted = 0;
	m_fNextLastPropGrenade = 0;
	PauseMutators();
}

void CHalfLifePropHunt::DetermineWinner( void )
{
	int highest = -9999;
	BOOL IsEqual = FALSE;
	CBasePlayer *highballer = NULL;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr && plr->IsPlayer() && plr->IsInArena )
		{
			if ( highest <= plr->pev->frags )
			{
				if ( highballer && highest == plr->pev->frags )
				{
					IsEqual = TRUE;
					continue;
				}

				IsEqual = FALSE;
				highest = plr->pev->frags;
				highballer = plr;
			}
		}
	}

	if ( highballer )
	{
		if (!IsEqual)
		{
			UTIL_ClientPrintAll(HUD_PRINTCENTER,
				UTIL_VarArgs("%s scored the most points!\n", STRING(highballer->pev->netname)));
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING("Prophunt Completed!");
				WRITE_STRING(UTIL_VarArgs("%s win!", m_iPropsRemain ? "Props" : "Hunters"));
				WRITE_BYTE(0);
				WRITE_STRING(UTIL_VarArgs("%s scored the most points!\n", STRING(highballer->pev->netname)));
			MESSAGE_END();

			DisplayWinnersGoods( highballer );
		}
		else
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && plr->IsInArena )
				{
					if ( plr->pev->frags == highest)
					{
						plr->m_iRoundWins++;
						plr->Celebrate();
					}
				}
			}

			UTIL_ClientPrintAll(HUD_PRINTCENTER, "Numerous victors!");
			UTIL_ClientPrintAll(HUD_PRINTTALK, "[PropHunt] Round ends with winners!\n");
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING("Prophunt Completed!");
				WRITE_STRING(UTIL_VarArgs("%s win!", m_iPropsRemain ? "Props" : "Hunters"));
				WRITE_BYTE(0);
				WRITE_STRING("Numerous victors!");
			MESSAGE_END();
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_CLAPPING);
			MESSAGE_END();
		}
	}
	else
	{
		UTIL_ClientPrintAll(HUD_PRINTCENTER, "Round is over!\nNo one has won!\n");
		UTIL_ClientPrintAll(HUD_PRINTTALK, "[PropHunt] Round ends with no winners!\n");
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("Prophunt Completed!");
			WRITE_STRING("");
			WRITE_BYTE(0);
			WRITE_STRING("No one has won!");
		MESSAGE_END();
	}
}

void CHalfLifePropHunt::Think( void )
{
	CHalfLifeMultiplay::Think();

	if ( flUpdateTime > gpGlobals->time )
		return;

	CheckRounds();

	// No loop during intermission
	if ( m_flIntermissionEndTime )
		return;

	if ( m_flRoundTimeLimit )
	{
		if ( HasGameTimerExpired() )
			return;
	}

	if ( g_GameInProgress )
	{
		int hunters_left = 0;
		int props_left = 0;

		// Player accountings
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
			{
				// Force spectate on those that died.
				if ( plr->m_flForceToObserverTime && plr->m_flForceToObserverTime < gpGlobals->time )
				{
					SuckToSpectator( plr );
					plr->m_flForceToObserverTime = 0;
				}

				if ( plr->IsInArena && !plr->IsSpectator() )
				{
					if (plr->pev->fuser4 >= TEAM_PROPS)
						props_left++;
					else
						hunters_left++;
				}
			}
		}

		m_iHuntersRemain = hunters_left;
		m_iPropsRemain = props_left;

		// Last-prop buff: when exactly one prop remains in a round that started with
		// at least two props, top up their HP once and periodically resupply grenades.
		// State is signalled cross-DLL via pev->fuser3 = -1 (a sentinel that the bot
		// reads as DESPERATE).  fuser3 was previously the prop-freeze deadline
		// (positive value) so the negative sentinel is unambiguous and never
		// collides with the freeze window.  We used to set kRenderFxGlowShell here
		// but that field is shared with the player's freeze-rune mechanic (see
		// player.cpp), so it could either clobber an unrelated visual or fail to
		// trigger the first-time gate when the prop was already frozen.
		if (props_left == 1 && m_iPropsStarted >= 2)
		{
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex(i);
				if (!plr || !plr->IsPlayer() || plr->HasDisconnected) continue;
				if (!plr->IsInArena || plr->IsSpectator()) continue;
				if (plr->pev->fuser4 < TEAM_PROPS) continue;

				if (plr->pev->fuser3 >= 0)
				{
					// First-time trigger: stamp the DESPERATE sentinel, restore HP,
					// banner, and reset the resupply timer so the next tick gives an
					// immediate grenade (otherwise a stale timer from a prior round
					// could delay the first resupply by up to 5 s).
					plr->pev->fuser3 = -1.0f;
					m_fNextLastPropGrenade = 0;
					int topup = (int)prophealth.value * 2;
					if (topup < 20) topup = 20;
					plr->pev->health = topup;
					plr->pev->max_health = topup;
					if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
					{
						MESSAGE_BEGIN(MSG_ONE, gmsgBanner, NULL, plr->edict());
							WRITE_STRING("Last Prop Standing!");
							WRITE_STRING("Extra HP + grenade resupply. Survive the timer!");
							WRITE_BYTE(80);
						MESSAGE_END();
					}
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_EBELL);
					MESSAGE_END();
					UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[PropHunt] %s is the last prop standing!\n", STRING(plr->pev->netname)));
				}
				// Periodic grenade resupply every 5 seconds while last prop is alive.
				if (m_fNextLastPropGrenade < gpGlobals->time)
				{
					plr->GiveAmmo(1, "Hand Grenade", HANDGRENADE_MAX_CARRY);
					m_fNextLastPropGrenade = gpGlobals->time + 5.0;
				}
			}
		}

		if (m_fSendArmoredManMessage != -1 && m_fSendArmoredManMessage < gpGlobals->time)
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && !plr->HasDisconnected && !FBitSet(plr->pev->flags, FL_FAKECLIENT) && !plr->IsSpectator() )
				{
					MESSAGE_BEGIN(MSG_ONE, gmsgBanner, NULL, plr->edict());
						if (plr->pev->fuser4 >= TEAM_PROPS)
						{
							WRITE_STRING("You Are a Prop!");
							WRITE_STRING("Hide as an item (ATTACK) and throw decoys (RELOAD)!");
						}
						else
						{
							WRITE_STRING("You Are a Hunter!");
							WRITE_STRING("Find the Props by shooting items. Real items lose a point!");
						}
						WRITE_BYTE(80);
					MESSAGE_END();
				}
			}
			m_fSendArmoredManMessage = -1;
		}

		// Prop messages
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
			{
				if ( plr->IsInArena && !plr->IsSpectator() )
				{
					if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
					{
						if (plr->pev->fuser4 >= TEAM_PROPS)
						{
							if (hunters_left > 1)
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Hide!");
									WRITE_STRING(UTIL_VarArgs("Hunters alive: %d", hunters_left));
									WRITE_BYTE(m_iHuntersStarted > 0
										? (int)(float(hunters_left) / float(m_iHuntersStarted) * 100.0f)
										: 0);
									if (roundlimit.value > 0)
										WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
									else
										WRITE_STRING("");
								MESSAGE_END();
							}
							else if (hunters_left == 1)
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Hide!");
									WRITE_STRING("Wait out til time runs out!");
									WRITE_BYTE(0);
									if (roundlimit.value > 0)
										WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
									else
										WRITE_STRING("");
								MESSAGE_END();
							}
							else
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Props hid well!");
									WRITE_STRING("");
									WRITE_BYTE(0);
									if (roundlimit.value > 0)
										WRITE_STRING(UTIL_VarArgs("Props win round %d of %d!", m_iSuccessfulRounds+1, (int)roundlimit.value));
									else
										WRITE_STRING("Props win round!");
								MESSAGE_END();
							}
						}
						else
						{
							if (props_left > 1)
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Hunt the props!");
									WRITE_STRING(UTIL_VarArgs("Props remain: %d", props_left));
									WRITE_BYTE(m_iPropsStarted > 0
										? (int)(float(props_left) / float(m_iPropsStarted) * 100.0f)
										: 0);
									if (roundlimit.value > 0)
										WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
									else
										WRITE_STRING("");
								MESSAGE_END();
							}
							else
							{
								if (props_left > 0)
								{
									MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
										WRITE_STRING("Find props!");
										WRITE_STRING(UTIL_VarArgs("Props remain: %d", props_left));
										WRITE_BYTE(m_iPropsStarted > 0
											? (int)(float(props_left) / float(m_iPropsStarted) * 100.0f)
											: 0);
										if (roundlimit.value > 0)
											WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
										else
											WRITE_STRING("");
									MESSAGE_END();
								}
								else
								{
									MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
										WRITE_STRING("Props removed!");
										WRITE_STRING("");
										WRITE_BYTE(0);
										if (roundlimit.value > 0)
											WRITE_STRING(UTIL_VarArgs("Hunters win round %d of %d!", m_iSuccessfulRounds+1, (int)roundlimit.value));
										else
											WRITE_STRING("Hunters win round!");
									MESSAGE_END();
								}
							}
						}
					}
				}
				else
				{
					//for clients who connected while game in progress.
					if ( plr->IsSpectator() )
					{
						MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
							if (m_iHuntersRemain >= 1 && m_iPropsRemain <= 0)
							{
								WRITE_STRING("Hunters have won!");
								if (roundlimit.value > 0)
									WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
								else
									WRITE_STRING("");
								WRITE_BYTE(0);
								WRITE_STRING("");
							}
							else if (m_iPropsRemain >= 1 && m_iHuntersRemain <= 0)
							{
								WRITE_STRING("Props have won!");
								if (roundlimit.value > 0)
									WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
								else
									WRITE_STRING("");
								WRITE_BYTE(0);
								WRITE_STRING("");
							}
							else
							{
								WRITE_STRING(UTIL_VarArgs("Hunters left: %d", m_iHuntersRemain));
								WRITE_STRING(UTIL_VarArgs("Props left: %d", m_iPropsRemain));
								// Bar is attached to the "Props left" info line, so scale
								// against the snapshot of props that started this round
								// (captured during team balance).  Renders 100% at start.
								WRITE_BYTE(m_iPropsStarted > 0
									? (int)(float(m_iPropsRemain) / float(m_iPropsStarted) * 100.0f)
									: 0);
								if (roundlimit.value > 0)
									WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
								else
									WRITE_STRING("");
							}
						MESSAGE_END();
					} else {
						// Send them to observer
						plr->m_flForceToObserverTime = gpGlobals->time;
					}
				}
			}
		}

		if ( hunters_left < 1 || props_left < 1 )
		{
			//stop timer / end game.
			m_flRoundTimeLimit = 0;
			g_GameInProgress = FALSE;
			PauseMutators();
			MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
				WRITE_BYTE(0);
			MESSAGE_END();

			//everyone died.
			if ( hunters_left <= 0 && props_left <= 0 )
			{
				DetermineWinner();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
				MESSAGE_END();
			}
			else if ( hunters_left <= 0 )
			{
				DetermineWinner();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
				MESSAGE_END();
			}
			else if ( props_left <= 0 )
			{
				DetermineWinner();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
				MESSAGE_END();
			}

			m_iSuccessfulRounds++;
			flUpdateTime = gpGlobals->time + 3.0;
			return;
		}

		// Freezing
		if ( m_fUnFreezeHunters > gpGlobals->time )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
				if ( plr && plr->IsPlayer() && plr->IsCommittedToPlay() && plr->pev->fuser4 == 0 )
				{
					plr->EnableControl(FALSE);
					UTIL_ScreenFade( plr, Vector(0,0,0), 0.2, m_fUnFreezeHunters - gpGlobals->time, 255, FFADE_OUT | FFADE_MODULATE );
					ClientPrint(plr->pev, HUD_PRINTCENTER, UTIL_VarArgs("You are frozen... hunt in %.0f seconds\n", (m_fUnFreezeHunters - gpGlobals->time)));
				}
			}
		}
		else if (m_fUnFreezeHunters > 0)
		{
			CBaseEntity *musicEnt = UTIL_FindEntityByClassname(NULL, "trigger_mp3audio");
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && plr->IsCommittedToPlay() )
				{
					if ( plr->pev->fuser4 == 0 )
						plr->EnableControl(TRUE);

					if ( musicEnt && musicEnt->edict() )
						musicEnt->Use(plr, plr, USE_ON, plr->m_iPlayMusic);
				}
			}

			UTIL_ClientPrintAll(HUD_PRINTCENTER, "Hunters are free!");

			// Restore mutators when round begins
			RestoreMutators();

			MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
				WRITE_BYTE(CLIENT_SOUND_GOGOGO);
			MESSAGE_END();
			m_fUnFreezeHunters = 0;
		}

		flUpdateTime = gpGlobals->time + 1.5;
		return;
	}

	int clients = CheckClients();

	if ( m_fWaitForPlayersTime == -1 )
	{
		m_fWaitForPlayersTime = gpGlobals->time + roundwaittime.value;
		RemoveAndFillItems();
		extern void ClearBodyQue();
		ClearBodyQue();
		MESSAGE_BEGIN( MSG_ALL, gmsgDEraser );
		MESSAGE_END();
	}

	if ( clients > 1 )
	{
		if ( m_fWaitForPlayersTime > gpGlobals->time )
		{
			SuckAllToSpectator();
			flUpdateTime = gpGlobals->time + 1.0;
			UTIL_ClientPrintAll(HUD_PRINTCENTER, UTIL_VarArgs("Battle will begin in %.0f\n", (m_fWaitForPlayersTime + 5) - gpGlobals->time));
			return;
		}

		if ( m_iCountDown > 0 )
		{
			if (m_iCountDown == 2) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_PREPAREFORBATTLE);
				MESSAGE_END();
			} else if (m_iCountDown == 3) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_THREE);
				MESSAGE_END();
			} else if (m_iCountDown == 4) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_FOUR);
				MESSAGE_END();
			} else if (m_iCountDown == 5) {
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_FIVE);
				MESSAGE_END();
			}
			SuckAllToSpectator(); // in case players join during a countdown.
			UTIL_ClientPrintAll(HUD_PRINTCENTER,
				UTIL_VarArgs("Prepare for Prop Hunt\n\n%i...\n", m_iCountDown));
			m_iCountDown--;
			m_iFirstBloodDecided = FALSE;
			flUpdateTime = gpGlobals->time + 1.0;
			return;
		}

		//frags + time.
		SetRoundLimits();

		// Balance teams
  		// Implementing Fisher–Yates shuffle
		int i, j, tmp; // create local variables to hold values for shuffle
		int count = m_iPropsStarted = m_iHuntersStarted = 0;
		int player[32];

		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			// Limbo gating: only committed-to-play players enter the team-balance shuffle.
			if ( plr && plr->IsPlayer() && plr->IsCommittedToPlay() )
				player[count++] = i;
		}

		for (i = count - 1; i > 0; i--) { // for loop to shuffle
			j = rand() % (i + 1); //randomise j for shuffle with Fisher Yates
			tmp = player[j];
			player[j] = player[i];
			player[i] = tmp;
		}

		m_fUnFreezeHunters = gpGlobals->time + prophunttime.value;
		m_fNextLastPropGrenade = 0; // reset across rounds so the next last prop gets an immediate resupply

		for ( int i = 0; i < count; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( player[i] );

			// Pool was already filtered for committed-to-play above; defensive recheck.
			if ( plr && plr->IsPlayer() && plr->IsCommittedToPlay() ) {
				plr->pev->fuser4 = (i + 1) % 2;
				if (plr->pev->fuser4 >= TEAM_PROPS)
				{
					m_iPropsStarted++;
					plr->pev->fuser3 = m_fUnFreezeHunters;
					plr->pev->fuser4 = 31; // suit
					plr->m_flNextPropSound = gpGlobals->time + RANDOM_FLOAT(25,35);
				}
				else
				{
					m_iHuntersStarted++;
					plr->EnableControl(FALSE);
				}
				if (plr->m_iPlayMusic)
					CLIENT_COMMAND(plr->edict(), "mp3 loop media/pause-beat.mp3\n");
			}
		}

		g_GameInProgress = TRUE;

		InsertClientsIntoArena(0);

		m_iCountDown = 5;
		m_fWaitForPlayersTime = -1;
		m_fSendArmoredManMessage = gpGlobals->time + 1.0;

		// Resend team info
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamNames );
			WRITE_BYTE( 2 );
			WRITE_STRING( "hunters" );
			WRITE_STRING( "props" );
		MESSAGE_END();

		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("[PropHunt] %d players have entered the arena!\n", clients));

		flUpdateTime = gpGlobals->time; // force now, so hunter freeze immediately.
		return;
	}
	else
	{
		SuckAllToSpectator();
		m_flRoundTimeLimit = 0;
		MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
			WRITE_STRING("Prop Hunt");
			WRITE_STRING("Waiting for other players");
			WRITE_BYTE(0);
			if (roundlimit.value > 0)
				WRITE_STRING(UTIL_VarArgs("%d Rounds", (int)roundlimit.value));
			else
				WRITE_STRING("");
		MESSAGE_END();
		m_fWaitForPlayersTime = gpGlobals->time + roundwaittime.value;
	}

	flUpdateTime = gpGlobals->time + 1.0;
}

BOOL CHalfLifePropHunt::HasGameTimerExpired( void )
{
	if ( CHalfLifeMultiplay::HasGameTimerExpired() )
	{
		DetermineWinner();
		MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
			WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
		MESSAGE_END();

		g_GameInProgress = FALSE;
		MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
			WRITE_BYTE(0);
		MESSAGE_END();

		PauseMutators();

		m_iSuccessfulRounds++;
		flUpdateTime = gpGlobals->time + 3.0;
		m_flRoundTimeLimit = 0;
		return TRUE;
	}

	return FALSE;
}

void CHalfLifePropHunt::InitHUD( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::InitHUD( pPlayer );

	if (!FBitSet(pPlayer->pev->flags, FL_FAKECLIENT))
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgObjective, NULL, pPlayer->edict());
			WRITE_STRING("Prop Hunt");
			WRITE_STRING("");
			WRITE_BYTE(0);
			if (roundlimit.value > 0)
				WRITE_STRING(UTIL_VarArgs("Round %d of %d", m_iSuccessfulRounds+1, (int)roundlimit.value));
			else
				WRITE_STRING("");
		MESSAGE_END();

		MESSAGE_BEGIN(MSG_ONE, gmsgTeamNames, NULL, pPlayer->edict());
			WRITE_BYTE( 2 );
			WRITE_STRING( "hunters" );
			WRITE_STRING( "props" );
		MESSAGE_END();
	}

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );
		if ( plr && !FBitSet(pPlayer->pev->flags, FL_FAKECLIENT) )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgTeamInfo, NULL, pPlayer->edict() );
				WRITE_BYTE( plr->entindex() );
				WRITE_STRING( plr->TeamID() );
			MESSAGE_END();
		}
	}
}

void CHalfLifePropHunt::PlayerSpawn( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerSpawn(pPlayer);

	// Place player in spectator mode if joining during a game
	// Or if the game begins that requires spectators
	if ((g_GameInProgress && !pPlayer->IsInArena) || (!g_GameInProgress && IsRoundBased()))
	{
		return;
	}

	char *key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict());

	PlayFootstepSounds(pPlayer, 1.0);

	if ( pPlayer->pev->fuser4 >= TEAM_PROPS )
	{
		strncpy( pPlayer->m_szTeamName, "props", TEAM_NAME_LENGTH );
		// Prop HP buffer — absorbs `mp_prophealth` worth of damage proxy (PROP_DAMAGE_PROXY per hit)
		// before the prop is converted into a hunter.  Default 20 → ~4 hits to convert.
		int propHp = (int)prophealth.value;
		if (propHp < 1) propHp = 1;
		pPlayer->pev->health = propHp;
		pPlayer->pev->max_health = propHp;
		pPlayer->pev->armorvalue = 0;
		pPlayer->pev->gaitsequence = 0;
		pPlayer->pev->fuser1 = 0; // hunter self-cost tracker (unused for props)
		pPlayer->pev->fuser2 = 0; // prop morph cooldown
		pPlayer->GiveNamedItem("weapon_handgrenade");
		CLIENT_COMMAND(pPlayer->edict(), "thirdperson\n");
	}
	else
	{
		strncpy( pPlayer->m_szTeamName, "hunters", TEAM_NAME_LENGTH );
		pPlayer->pev->fuser3 = 1; // bot timer to unfreeze
		pPlayer->pev->fuser1 = 0; // reset hunter self-cost shot tracker
		// Hunters always start with a flamethrower
		pPlayer->GiveNamedItem("weapon_flamethrower");
		pPlayer->GiveAmmo( FUEL_MAX_CARRY, "uranium", FUEL_MAX_CARRY );
	}

	// notify everyone's HUD of the team change
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_STRING( pPlayer->m_szTeamName );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
		WRITE_SHORT( pPlayer->pev->frags );
		WRITE_SHORT( pPlayer->m_iDeaths );
		WRITE_SHORT( pPlayer->m_iRoundWins );
		WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
	MESSAGE_END();
}

BOOL CHalfLifePropHunt::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if (pPlayer->pev->fuser4 >= TEAM_PROPS && m_fUnFreezeHunters > 0)
		return FALSE; // props cannot change to hunter yet.

	if ( pAttacker && pAttacker->IsPlayer() && PlayerRelationship( pPlayer, pAttacker ) == GR_TEAMMATE )
	{
		// my teammate hit me.
		if ( (friendlyfire.value == 0) && (pAttacker != pPlayer) )
		{
			// friendly fire is off, and this hit came from someone other than myself,  then don't get hurt
			return FALSE;
		}
	}

	if (pPlayer->pev->fuser4 >= TEAM_PROPS)
	{
		// HP buffer: every hit subtracts PROP_DAMAGE_PROXY from the prop's pev->health.
		// While the buffer is still positive we silently absorb the hit (return FALSE
		// so the real damage system never runs).  Only the killing tick converts.
		const int PROP_DAMAGE_PROXY = 5;
		pPlayer->pev->health -= PROP_DAMAGE_PROXY;
		if (pPlayer->pev->health > 0)
		{
			EMIT_SOUND_DYN(ENT(pPlayer->pev), CHAN_BODY, "player/pl_pain2.wav", 0.6, ATTN_IDLE, 0, 100);
			return FALSE;
		}

		// Buffer exhausted — convert prop into hunter.
		DeactivateDecoys(pPlayer);
		PlayFootstepSounds(pPlayer, 1.0);

		CLIENT_COMMAND(pPlayer->edict(), "firstperson\n");
		pPlayer->pev->fuser4 = 0;
		pPlayer->pev->fuser3 = 1; // bot timer to unfreeze
		pPlayer->pev->fuser1 = 0; // reset hunter self-cost shot tracker
		pPlayer->pev->health = 100;
		pPlayer->pev->max_health = 100;
		// Cancel prop haste
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "0");
		pPlayer->GiveRandomWeapon("weapon_nuke");

		strncpy( pPlayer->m_szTeamName, "hunters", TEAM_NAME_LENGTH );
		char *key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict());

		MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
			WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
			WRITE_STRING( pPlayer->m_szTeamName );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(pPlayer->edict()) );
			WRITE_SHORT( pPlayer->pev->frags );
			WRITE_SHORT( pPlayer->m_iDeaths );
			WRITE_SHORT( pPlayer->m_iRoundWins );
			WRITE_SHORT( g_pGameRules->GetTeamIndex( pPlayer->m_szTeamName ) + 1 );
		MESSAGE_END();

		if (pAttacker && pAttacker->IsPlayer() && (pAttacker != pPlayer))
		{
			CBasePlayer *kp = (CBasePlayer *)pAttacker;
			kp->pev->frags += 2;
			// Kill-heal: hunter who finished off a prop gets fully restored
			if (kp->pev->max_health > 0)
				kp->pev->health = kp->pev->max_health;
			else
				kp->pev->health = 100;
			MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(kp->edict()) );
			WRITE_SHORT( kp->pev->frags );
			WRITE_SHORT( kp->m_iDeaths );
			WRITE_SHORT( kp->m_iRoundWins );
			WRITE_SHORT( g_pGameRules->GetTeamIndex( kp->m_szTeamName ) + 1 );
			MESSAGE_END();

			if (!FBitSet(pAttacker->pev->flags, FL_FAKECLIENT))
			{
				MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, pAttacker->edict() );
					WRITE_BYTE(CLIENT_SOUND_LEVEL_UP);
				MESSAGE_END();
			}
		}

		return FALSE;
	}

	return CHalfLifeMultiplay::FPlayerCanTakeDamage( pPlayer, pAttacker );
}

void CHalfLifePropHunt::FPlayerTookDamage( float flDamage, CBasePlayer *pVictim, CBaseEntity *pKiller)
{
	CBasePlayer *pPlayerAttacker = NULL;

	if (pKiller && pKiller->IsPlayer())
	{
		pPlayerAttacker = (CBasePlayer *)pKiller;
		if ( pPlayerAttacker != pVictim && !pPlayerAttacker->pev->fuser4 && !pVictim->pev->fuser4 )
		{
			ClientPrint(pPlayerAttacker->pev, HUD_PRINTCENTER, "Destroy the props!\nNot your teammate!");
		}
	}
}

float CHalfLifePropHunt::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	// Props take no fall damage
	if (pPlayer->pev->fuser4 >= TEAM_PROPS)
		return 0.0f;

	return CHalfLifeMultiplay::FlPlayerFallDamage( pPlayer );
}

int CHalfLifePropHunt::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() )
		return GR_NOTTEAMMATE;

	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
	{
		return GR_TEAMMATE;
	}

	return GR_NOTTEAMMATE;
}

int CHalfLifePropHunt::GetTeamIndex( const char *pTeamName )
{
	if ( pTeamName && *pTeamName != 0 )
	{
		if (!strcmp(pTeamName, "props"))
			return 1;
		else
			return 0;
	}
	
	return -1;	// No match
}

const char *CHalfLifePropHunt::GetTeamID( CBaseEntity *pEntity )
{
	if ( pEntity == NULL || pEntity->pev == NULL )
		return "";

	// return their team name
	return pEntity->TeamID();
}

BOOL CHalfLifePropHunt::ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target )
{
	// always autoaim, unless target is a teammate
	CBaseEntity *pTgt = CBaseEntity::Instance( target );
	if ( pTgt && pTgt->IsPlayer() )
	{
		if ( PlayerRelationship( pPlayer, pTgt ) == GR_TEAMMATE )
			return FALSE; // don't autoaim at teammates
	}

	return CHalfLifeMultiplay::ShouldAutoAim( pPlayer, target );
}

BOOL CHalfLifePropHunt::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if (pPlayer->pev->fuser4 >= TEAM_PROPS &&
		strcmp(STRING(pItem->pev->classname), "weapon_fists") &&
		strcmp(STRING(pItem->pev->classname), "weapon_handgrenade"))
		return FALSE;

	return CHalfLifeMultiplay::CanHavePlayerItem( pPlayer, pItem );
}

BOOL CHalfLifePropHunt::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
{
	if (pPlayer->pev->fuser4 >= TEAM_PROPS)
		return FALSE;

	return CHalfLifeMultiplay::CanHaveItem( pPlayer, pItem );
}

BOOL CHalfLifePropHunt::IsAllowedToDropWeapon( CBasePlayer *pPlayer )
{
	if (pPlayer->pev->fuser4 >= TEAM_PROPS)
		return FALSE;

	return CHalfLifeMultiplay::IsAllowedToDropWeapon( pPlayer );
}

int CHalfLifePropHunt::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	if (pPlayer->pev->fuser4 >= TEAM_PROPS)
		return GR_PLR_DROP_GUN_NO;

	return CHalfLifeMultiplay::DeadPlayerWeapons( pPlayer );
}

int CHalfLifePropHunt::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	if (pPlayer->pev->fuser4 >= TEAM_PROPS)
		return GR_PLR_DROP_AMMO_NO;

	return CHalfLifeMultiplay::DeadPlayerAmmo( pPlayer );
}

BOOL CHalfLifePropHunt::CanHavePlayerAmmo( CBasePlayer *pPlayer, CBasePlayerAmmo *pAmmo )
{
	if (pPlayer->pev->fuser4 >= TEAM_PROPS)
		return FALSE;

	return CHalfLifeMultiplay::CanHavePlayerAmmo( pPlayer, pAmmo );
}

void CHalfLifePropHunt::MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller )
{
	CBasePlayer *plr = (CBasePlayer *)CBaseEntity::Instance( pKiller );

	// Reduce frags if killed harmless item
	if (plr && plr->IsPlayer())
	{
		if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
		{
			MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgPlayClientSound, NULL, plr->edict() );
				WRITE_BYTE(CLIENT_SOUND_NOPE);
			MESSAGE_END();
		}
		ClientPrint(plr->pev, HUD_PRINTCENTER, "Decoy destroyed! -1 frag :(\n");

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(plr->edict()) );
			WRITE_SHORT( --plr->pev->frags );
			WRITE_SHORT( plr->m_iDeaths );
			WRITE_SHORT( plr->m_iRoundWins );
			WRITE_SHORT( g_pGameRules->GetTeamIndex( plr->m_szTeamName ) + 1 );
		MESSAGE_END();
	}
}

void CHalfLifePropHunt::PlayerThink( CBasePlayer *pPlayer )
{
	CHalfLifeMultiplay::PlayerThink(pPlayer);

	if (pPlayer->pev->fuser4 >= TEAM_PROPS)
	{	
		pPlayer->pev->air_finished = gpGlobals->time + 10; // never drown

		// Prop haste — equivalent to the Haste rune, ~1.5x movement speed
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "1");

		// Body/morph cycling is handled entirely in CBasePlayerWeapon::ItemPostFrame
		// (weapons.cpp:1106) so the IN_ATTACK / IN_ATTACK2 button is consumed exactly
		// once.  Doing it here too would advance the body twice per press because
		// PlayerThink runs in PreThink, before ItemPostFrame, and both paths look at
		// the same m_afButtonPressed / pev->button state.

		if (pPlayer->m_flNextPropSound && pPlayer->m_flNextPropSound < gpGlobals->time)
		{
			EMIT_SOUND(ENT(pPlayer->pev), CHAN_VOICE, "sprayer.wav", 1, ATTN_NORM);
			pPlayer->m_flNextPropSound = gpGlobals->time + RANDOM_FLOAT(25,35);
		}
	}
	else
	{
		pPlayer->m_flNextPropSound = 0;
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "0");

		// Hunter self-cost: each weapon shot drains a small amount of HP.
		// Detect a shot by watching m_flNextPrimaryAttack increase on the active item.
		// Excludes melee/grenade.  Clamps health to a minimum of 5 so hunters can't
		// suicide-fire.
		if (pPlayer->m_pActiveItem && hunterselfcost.value > 0 && pPlayer->IsAlive())
		{
			const char *cn = STRING(pPlayer->m_pActiveItem->pev->classname);
			bool exclude = (cn && (!strcmp(cn, "weapon_fists") || !strcmp(cn, "weapon_handgrenade")));
			CBasePlayerWeapon *w = (CBasePlayerWeapon *)pPlayer->m_pActiveItem;
			float npa = w->m_flNextPrimaryAttack;
			float prev = pPlayer->pev->fuser1;
			if (!exclude && prev > 0 && npa > prev && (npa - prev) < 5.0f)
			{
				int cost = (int)hunterselfcost.value;
				if (pPlayer->pev->health - cost > 5)
					pPlayer->pev->health -= cost;
				else if (pPlayer->pev->health > 5)
					pPlayer->pev->health = 5;
			}
			pPlayer->pev->fuser1 = npa;
		}
	}
}

BOOL CHalfLifePropHunt::IsRoundBased( void )
{
	return TRUE;
}

BOOL CHalfLifePropHunt::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	if ( !pPlayer->IsAlive() && !pPlayer->m_flForceToObserverTime )
		pPlayer->m_flForceToObserverTime = gpGlobals->time + 3.0;

	return FALSE;
}

BOOL CHalfLifePropHunt::MutatorAllowed(const char *mutator)
{
	if (strstr(mutator, g_szMutators[MUTATOR_SANTAHAT - 1]) || atoi(mutator) == MUTATOR_SANTAHAT)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_PIRATEHAT - 1]) || atoi(mutator) == MUTATOR_PIRATEHAT)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_COWBOY - 1]) || atoi(mutator) == MUTATOR_COWBOY)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_MARSHMELLO - 1]) || atoi(mutator) == MUTATOR_MARSHMELLO)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_JACK - 1]) || atoi(mutator) == MUTATOR_JACK)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_PUMPKIN - 1]) || atoi(mutator) == MUTATOR_PUMPKIN)
		return FALSE;

	if (strstr(mutator, g_szMutators[MUTATOR_TOILET - 1]) || atoi(mutator) == MUTATOR_TOILET)
		return FALSE;

	return CHalfLifeMultiplay::MutatorAllowed(mutator);
}

void CHalfLifePropHunt::ClientDisconnected( edict_t *pClient )
{
	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );
		if ( pPlayer && pPlayer->pev->fuser4 >= TEAM_PROPS )
			DeactivateDecoys( pPlayer );
	}

	CHalfLifeMultiplay::ClientDisconnected( pClient );
}

void CHalfLifePropHunt::PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	pVictim->pev->frags = 0; // clear immediately for winner determination

	CHalfLifeMultiplay::PlayerKilled(pVictim, pKiller, pInflictor);
}

BOOL CHalfLifePropHunt::AllowRuneSpawn( const char *szRune )
{
	return FALSE;
}

int CHalfLifePropHunt::WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	return GR_WEAPON_RESPAWN_NO;
}

int CHalfLifePropHunt::ItemShouldRespawn( CItem *pItem )
{
	return GR_ITEM_RESPAWN_NO;
}

int CHalfLifePropHunt::AmmoShouldRespawn( CBasePlayerAmmo *pAmmo )
{
	return GR_AMMO_RESPAWN_NO;
}

BOOL CHalfLifePropHunt::IsTeamplay( void )
{
	return TRUE;
}

BOOL CHalfLifePropHunt :: PlayFootstepSounds( CBasePlayer *pl, float fvol )
{
	if ( pl->pev->fuser4 >= TEAM_PROPS )
	{
		g_engfuncs.pfnSetPhysicsKeyValue(pl->edict(), "prop", "1");
		return FALSE;
	}

	g_engfuncs.pfnSetPhysicsKeyValue(pl->edict(), "prop", "0");

	return CHalfLifeMultiplay::PlayFootstepSounds( pl, fvol );
}

BOOL CHalfLifePropHunt::CanHaveNamedItem( CBasePlayer *pPlayer, const char *pszItemName )
{
	if (pPlayer->pev->fuser4 >= TEAM_PROPS && 
		stricmp(pszItemName, "weapon_fists") != 0 &&
		stricmp(pszItemName, "weapon_handgrenade") != 0)
	{
		return FALSE;
	}

	return CHalfLifeMultiplay::CanHaveNamedItem( pPlayer, pszItemName );
}
