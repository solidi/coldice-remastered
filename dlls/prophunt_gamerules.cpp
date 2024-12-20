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
		((CBasePlayer *)(CBaseEntity *)m_hOwner)->m_iPropsDeployed--;

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_TAREXPLOSION );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
		MESSAGE_END();

		if (plr && plr->IsPlayer())
		{
			if (plr->pev != m_hOwner->pev)
			{
				MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
					WRITE_BYTE( ENTINDEX(plr->edict()) );
					WRITE_SHORT( ++plr->pev->frags );
					WRITE_SHORT( plr->m_iDeaths );
					WRITE_SHORT( plr->m_iRoundWins );
					WRITE_SHORT( g_pGameRules->GetTeamIndex( plr->m_szTeamName ) + 1 );
				MESSAGE_END();

				ClientPrint(m_hOwner->pev, HUD_PRINTCENTER, UTIL_VarArgs("+1 point for decoy touch.\n", plr->m_iPropsDeployed));
			}
			else
				ClientPrint(m_hOwner->pev, HUD_PRINTCENTER, UTIL_VarArgs("%d decoys are deployed.\n", plr->m_iPropsDeployed));
		}
		else
			ClientPrint(m_hOwner->pev, HUD_PRINTCENTER, UTIL_VarArgs("%d decoys are deployed.\n", plr->m_iPropsDeployed));
	}

	CBaseEntity::Killed( pevAttacker, iGib );
}

void CPropDecoy::PropDecoyThink( void )
{
	CBaseEntity *ent = NULL;
	BOOL yes = FALSE;

	while ( (ent = UTIL_FindEntityInSphere( ent, pev->origin, 32 )) != NULL )
	{
		if (FClassnameIs(ent->pev, "player") /*&& ent->pev != m_hOwner->pev*/)
		{
			PropDecoyTouch(ent);
			break;
		}
	}

	pev->nextthink = gpGlobals->time + 0.1;
}

BOOL CPropDecoy::ShouldCollide( CBaseEntity *pOther )
{
	if (pOther->pev == m_hOwner->pev)
		return FALSE;

	return TRUE;
}

void CPropDecoy::PropDecoyTouch( CBaseEntity *pOther )
{
	if (pOther->IsPlayer() && pOther->IsAlive())
	{
		CBasePlayer *plr = (CBasePlayer *)pOther;
		((CBasePlayer *)(CBaseEntity *)m_hOwner)->m_iPropsDeployed--;

		SetTouch(NULL);
		SetThink(NULL);

		if (((CBaseEntity*)m_hOwner) != pOther)
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
				WRITE_BYTE( ENTINDEX(plr->edict()) );
				WRITE_SHORT( ++plr->pev->frags );
				WRITE_SHORT( plr->m_iDeaths );
				WRITE_SHORT( plr->m_iRoundWins );
				WRITE_SHORT( g_pGameRules->GetTeamIndex( plr->m_szTeamName ) + 1 );
			MESSAGE_END();

			ClientPrint(m_hOwner->pev, HUD_PRINTCENTER, UTIL_VarArgs("+1 point for decoy touch.\n", plr->m_iPropsDeployed));
		}
		else
			ClientPrint(m_hOwner->pev, HUD_PRINTCENTER, UTIL_VarArgs("%d decoys are deployed.\n", plr->m_iPropsDeployed));

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
	m_iHuntersRemain = 0;
	m_iPropsRemain = 0;
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
					edict_t *pentSpawnSpot = g_pGameRules->GetPlayerSpawnSpot( plr );
					plr->StartObserver(pentSpawnSpot->v.origin, VARS(pentSpawnSpot)->angles);
					plr->m_flForceToObserverTime = 0;
				}

				if ( plr->IsInArena && !plr->IsSpectator() )
				{
					if (plr->pev->fuser4 > 0)
						props_left++;
					else
						hunters_left++;
				}
			}
		}

		m_iHuntersRemain = hunters_left;
		m_iPropsRemain = props_left;

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
						if (plr->pev->fuser4 > 0)
						{
							if (hunters_left > 1)
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Hide!");
									WRITE_STRING(UTIL_VarArgs("Hunters alive: %d", hunters_left));
									WRITE_BYTE(float(hunters_left) / (m_iPlayersInGame) * 100);
								MESSAGE_END();
							}
							else if (hunters_left == 1)
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Hide!");
									WRITE_STRING("Wait out til time runs out!");
									WRITE_BYTE(0);
								MESSAGE_END();
							}
							else
							{
								MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
									WRITE_STRING("Props hid well!");
									WRITE_STRING("");
									WRITE_BYTE(0);
									WRITE_STRING(UTIL_VarArgs("Props win round %d of %d!", m_iSuccessfulRounds+1, (int)roundlimit.value));
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
									WRITE_BYTE(float(props_left) / (m_iPlayersInGame) * 100);
								MESSAGE_END();
							}
							else
							{
								if (props_left > 0)
								{
									MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
										WRITE_STRING("Find props!");
										WRITE_STRING(UTIL_VarArgs("Props remain: %d", props_left));
										WRITE_BYTE(float(props_left) / (m_iPlayersInGame) * 100);
									MESSAGE_END();
								}
								else
								{
									MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, gmsgObjective, NULL, plr->edict());
										WRITE_STRING("Props removed!");
										WRITE_STRING("");
										WRITE_BYTE(0);
										WRITE_STRING(UTIL_VarArgs("Hunters win round %d of %d!", m_iSuccessfulRounds+1, (int)roundlimit.value));
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
								WRITE_STRING("Hunters have won!");
							else if (m_iPropsRemain >= 1 && m_iHuntersRemain <= 0)
								WRITE_STRING("Props have won!");
							else
							{
								WRITE_STRING(UTIL_VarArgs("Hunters left: %d", m_iHuntersRemain));
								WRITE_STRING(UTIL_VarArgs("Props left: %d", m_iPropsRemain));
								WRITE_BYTE(float(m_iPropsRemain) / (m_iPlayersInGame) * 100);
							}
						MESSAGE_END();
					} else {
						// Send them to observer
						plr->m_flForceToObserverTime = gpGlobals->time;
					}
				}
			}
		}

		// Prop icon
		/*
		if (m_fSendArmoredManMessage < gpGlobals->time)
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && !plr->HasDisconnected && plr->pev->fuser4 > 0 )
				{
					if (!FBitSet(plr->pev->flags, FL_FAKECLIENT) && !plr->IsSpectator())
					{
						MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, plr->pev );
							WRITE_BYTE(1);
							WRITE_STRING("dealter");
							WRITE_BYTE(0);
							WRITE_BYTE(160);
							WRITE_BYTE(255);
						MESSAGE_END();
					}
				}
			}
		}
		*/

		if ( hunters_left < 1 || props_left < 1 )
		{
			//stop timer / end game.
			m_flRoundTimeLimit = 0;
			g_GameInProgress = FALSE;
			MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
				WRITE_BYTE(0);
			MESSAGE_END();

			/*
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && !plr->HasDisconnected && plr->pev->fuser4 > 0 )
				{
					if (!FBitSet(plr->pev->flags, FL_FAKECLIENT))
					{
						MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, plr->pev );
							WRITE_BYTE(0);
							WRITE_STRING("dealter");
						MESSAGE_END();
					}
				}
			}
			*/

			//everyone died.
			if ( hunters_left <= 0 && props_left <= 0 )
			{
				UTIL_ClientPrintAll(HUD_PRINTCENTER, "Everyone has been killed!\n");
				UTIL_ClientPrintAll(HUD_PRINTTALK, "* No winners in this round!\n");
				MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
					WRITE_STRING("Everyone died!");
					WRITE_STRING("");
					WRITE_BYTE(0);
					WRITE_STRING("No winners in this round!");
				MESSAGE_END();
				MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
					WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
				MESSAGE_END();
			}
			else if ( hunters_left <= 0 )
			{
				//find highest frag amount.
				float highest = 1;
				BOOL IsEqual = FALSE;
				CBasePlayer *highballer = NULL;

				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

					if ( plr && plr->IsPlayer() && plr->IsInArena && plr->pev->fuser4 > 0 )
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

				if ( !IsEqual && highballer )
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER,
						UTIL_VarArgs("Hunters have been defeated!\n\n%s doled the most hit decoys!\n",
						STRING(highballer->pev->netname)));
					DisplayWinnersGoods( highballer );
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
					MESSAGE_END();
				}
				else
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER, "Hunters have been defeated!\n");
					UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends in a tie!\n");
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
					MESSAGE_END();
				}
			}
			else if ( props_left <= 0 )
			{
				//find highest frag amount.
				float highest = 1;
				BOOL IsEqual = FALSE;
				CBasePlayer *highballer = NULL;

				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

					if ( plr && plr->IsPlayer() && plr->IsInArena && plr->pev->fuser4 == 0 )
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

				if ( !IsEqual && highballer )
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER,
						UTIL_VarArgs("Props have been defeated!\n\n%s removed the most props!\n",
						STRING(highballer->pev->netname)));
					DisplayWinnersGoods( highballer );
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
					MESSAGE_END();
				}
				else
				{
					UTIL_ClientPrintAll(HUD_PRINTCENTER, "Props have been defeated!\n");
					UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends in a tie!\n");
					MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
						WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
					MESSAGE_END();
				}
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
				if ( plr && plr->IsPlayer() && !plr->HasDisconnected && plr->pev->fuser4 == 0 )
				{
					plr->EnableControl(FALSE);
					UTIL_ScreenFade( plr, Vector(0,0,0), 0.2, m_fUnFreezeHunters - gpGlobals->time, 255, FFADE_OUT | FFADE_MODULATE );
					ClientPrint(plr->pev, HUD_PRINTCENTER, UTIL_VarArgs("You are frozen... hunt in %.0f seconds\n", (m_fUnFreezeHunters - gpGlobals->time)));
				}
			}
		}
		else if (m_fUnFreezeHunters > 0)
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsPlayer() && !plr->HasDisconnected && plr->pev->fuser4 == 0 )
					plr->EnableControl(TRUE);
			}

			UTIL_ClientPrintAll(HUD_PRINTCENTER, "Hunters are free!");

			MESSAGE_BEGIN(MSG_BROADCAST, gmsgPlayClientSound);
				WRITE_BYTE(CLIENT_SOUND_GOGOGO);
			MESSAGE_END();
			m_fUnFreezeHunters = 0;
		}

		flUpdateTime = gpGlobals->time + 1.5;
		return;
	}

	int clients = CheckClients();

	if ( clients > 1 )
	{
		if ( m_fWaitForPlayersTime == -1 )
			m_fWaitForPlayersTime = gpGlobals->time + roundwaittime.value;

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
		int count = 0;
		int player[32];

		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() && !plr->HasDisconnected )
				player[count++] = i;
		}

		for (i = count - 1; i > 0; i--) { // for loop to shuffle
			j = rand() % (i + 1); //randomise j for shuffle with Fisher Yates
			tmp = player[j];
			player[j] = player[i];
			player[i] = tmp;
		}

		m_fUnFreezeHunters = gpGlobals->time + 30.0;

		for ( int i = 0; i < count; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( player[i] );

			if ( plr && plr->IsPlayer() && !plr->HasDisconnected ) {
				plr->pev->fuser4 = i % 2;
				if (plr->pev->fuser4 > 0)
				{
					plr->pev->fuser3 = m_fUnFreezeHunters;
					plr->pev->fuser4 = RANDOM_LONG(1, 30);
					plr->m_flNextPropSound = gpGlobals->time + RANDOM_FLOAT(25,35);
				}
				else
				{
					plr->EnableControl(FALSE);
				}
			}
		}

		g_GameInProgress = TRUE;

		InsertClientsIntoArena(0);

		// m_fSendArmoredManMessage = gpGlobals->time + 1.0;

		m_iCountDown = 5;
		m_fWaitForPlayersTime = -1;

		// Resend team info
		MESSAGE_BEGIN( MSG_ALL, gmsgTeamNames );
			WRITE_BYTE( 2 );
			WRITE_STRING( "hunters" );
			WRITE_STRING( "props" );
		MESSAGE_END();

		UTIL_ClientPrintAll(HUD_PRINTTALK, UTIL_VarArgs("* %d players have entered the arena!\n", clients));

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
			WRITE_STRING(UTIL_VarArgs("%d Rounds", (int)roundlimit.value));
		MESSAGE_END();
		m_fWaitForPlayersTime = gpGlobals->time + roundwaittime.value;
	}

	flUpdateTime = gpGlobals->time + 1.0;
}

BOOL CHalfLifePropHunt::HasGameTimerExpired( void )
{
	if ( CHalfLifeMultiplay::HasGameTimerExpired() )
	{
		int highest = -9999; // any frag amount wins.
		BOOL IsEqual = FALSE;
		CBasePlayer *highballer = NULL;

		//find highest prop damage amount.
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->IsPlayer() && plr->IsInArena && plr->pev->fuser4 > 0)
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

		if ( !IsEqual && highballer )
		{
			UTIL_ClientPrintAll(HUD_PRINTCENTER, 
				UTIL_VarArgs("Time is up!\n\n%s doled the most points!\n",
				STRING(highballer->pev->netname)));
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING("Time is up!");
				WRITE_STRING("");
				WRITE_BYTE(0);
				WRITE_STRING(UTIL_VarArgs("%s doled the most points!\n", STRING(highballer->pev->netname)));
			MESSAGE_END();
			DisplayWinnersGoods( highballer );

			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_OUTSTANDING);
			MESSAGE_END();
		}
		else
		{
			UTIL_ClientPrintAll(HUD_PRINTCENTER, "Time is up!\nNo one has won!\n");
			UTIL_ClientPrintAll(HUD_PRINTTALK, "* Round ends in a tie!\n");
			MESSAGE_BEGIN(MSG_BROADCAST, gmsgObjective);
				WRITE_STRING("Time is up!");
				WRITE_STRING("");
				WRITE_BYTE(0);
				WRITE_STRING("No one has won!");
			MESSAGE_END();
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
				WRITE_BYTE(CLIENT_SOUND_HULIMATING_DEAFEAT);
			MESSAGE_END();
		}

		g_GameInProgress = FALSE;
		MESSAGE_BEGIN(MSG_ALL, gmsgShowTimer);
			WRITE_BYTE(0);
		MESSAGE_END();

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

	if ( pPlayer->pev->fuser4 > 0 )
	{
		strncpy( pPlayer->m_szTeamName, "props", TEAM_NAME_LENGTH );
		//SET_MODEL(ENT(pPlayer->pev), "models/w_weapons.mdl");
		// UTIL_SetSize(pPlayer->pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
		pPlayer->pev->health = 1;
		pPlayer->pev->armorvalue = 0;
		pPlayer->GiveNamedItem("weapon_handgrenade");
		CLIENT_COMMAND(pPlayer->edict(), "thirdperson\n");
		ClientPrint(pPlayer->pev, HUD_PRINTCENTER, "You are a prop, hide!");
	}
	else
	{
		strncpy( pPlayer->m_szTeamName, "hunters", TEAM_NAME_LENGTH );
		pPlayer->pev->fuser3 = 1; // bot timer to unfreeze
	}

	//g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "team", pPlayer->m_szTeamName);

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
	if (pPlayer->pev->fuser4 > 0 && m_fUnFreezeHunters > 0)
		return FALSE; // props cannot change to hunter yet.

	if ( pAttacker && pPlayer->pev->fuser4 == pAttacker->pev->fuser4 )
	{
		// my teammate hit me.
		if ( (friendlyfire.value == 0) && (pAttacker != pPlayer) )
		{
			// friendly fire is off, and this hit came from someone other than myself,  then don't get hurt
			return FALSE;
		}
	}

	if (pPlayer->pev->fuser4 > 0)
	{
		DeactivateDecoys(pPlayer);

		CLIENT_COMMAND(pPlayer->edict(), "firstperson\n");
		pPlayer->pev->fuser4 = 0;
		pPlayer->pev->fuser3 = 1; // bot timer to unfreeze
		pPlayer->pev->health = 100;
		pPlayer->GiveRandomWeapon("weapon_nuke");

		strncpy( pPlayer->m_szTeamName, "hunters", TEAM_NAME_LENGTH );
		char *key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict());
		//g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pPlayer->edict()), key, "team", pPlayer->m_szTeamName);

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
			MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(kp->edict()) );
			WRITE_SHORT( kp->pev->frags );
			WRITE_SHORT( kp->m_iDeaths );
			WRITE_SHORT( kp->m_iRoundWins );
			WRITE_SHORT( g_pGameRules->GetTeamIndex( kp->m_szTeamName ) + 1 );
			MESSAGE_END();
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
	if (pPlayer->pev->fuser4 > 0 &&
		strcmp(STRING(pItem->pev->classname), "weapon_fists") &&
		strcmp(STRING(pItem->pev->classname), "weapon_handgrenade"))
		return FALSE;

	return CHalfLifeMultiplay::CanHavePlayerItem( pPlayer, pItem );
}

BOOL CHalfLifePropHunt::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
{
	if (pPlayer->pev->fuser4 > 0)
		return FALSE;

	return CHalfLifeMultiplay::CanHaveItem( pPlayer, pItem );
}

BOOL CHalfLifePropHunt::IsAllowedToDropWeapon( CBasePlayer *pPlayer )
{
	if (pPlayer->pev->fuser4 > 0)
		return FALSE;

	return CHalfLifeMultiplay::IsAllowedToDropWeapon( pPlayer );
}

int CHalfLifePropHunt::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	if (pPlayer->pev->fuser4 > 0)
		return GR_PLR_DROP_GUN_NO;

	return CHalfLifeMultiplay::DeadPlayerWeapons( pPlayer );
}

int CHalfLifePropHunt::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	if (pPlayer->pev->fuser4 > 0)
		return GR_PLR_DROP_AMMO_NO;

	return CHalfLifeMultiplay::DeadPlayerAmmo( pPlayer );
}

BOOL CHalfLifePropHunt::CanHavePlayerAmmo( CBasePlayer *pPlayer, CBasePlayerAmmo *pAmmo )
{
	if (pPlayer->pev->fuser4 > 0)
		return FALSE;

	return CHalfLifeMultiplay::CanHavePlayerAmmo( pPlayer, pAmmo );
}

void CHalfLifePropHunt::MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller )
{
	CBasePlayer *plr = (CBasePlayer *)CBaseEntity::Instance( pKiller );

	// Reduce frags if killed harmless item
	if (plr && plr->IsPlayer())
	{
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

	if (pPlayer->pev->fuser4 > 0)
	{
		if (pPlayer->m_flNextPropSound && pPlayer->m_flNextPropSound < gpGlobals->time)
		{
			EMIT_SOUND(ENT(pPlayer->pev), CHAN_VOICE, "sprayer.wav", 1, ATTN_NORM);
			pPlayer->m_flNextPropSound = gpGlobals->time + RANDOM_FLOAT(25,35);
		}
	}
	else
	{
		pPlayer->m_flNextPropSound = 0;
	}
}

BOOL CHalfLifePropHunt::IsRoundBased( void )
{
	return TRUE;
}

BOOL CHalfLifePropHunt::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	if ( !pPlayer->m_flForceToObserverTime )
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
