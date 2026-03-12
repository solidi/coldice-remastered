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

class CHalfLifeLoot : public CHalfLifeMultiplay
{
public:
	CHalfLifeLoot();
	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered Loot"; }

	virtual void  Think( void );
	virtual BOOL  HasGameTimerExpired( void );
	virtual void  InitHUD( CBasePlayer *pPlayer );
	virtual void  PlayerSpawn( CBasePlayer *pPlayer );
	virtual void  PlayerThink( CBasePlayer *pPlayer );
	virtual void  PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual void  ClientDisconnected( edict_t *pClient );
	virtual void  ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer );
	virtual BOOL  FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual BOOL  FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual BOOL  CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon );
	virtual int   DeadPlayerWeapons( CBasePlayer *pPlayer );
	virtual int   WeaponShouldRespawn( CBasePlayerItem *pWeapon );
	virtual BOOL  CanRandomizeWeapon( const char *name );
	virtual BOOL  IsAllowedToSpawn( CBaseEntity *pEntity );
	virtual BOOL  IsAllowedToDropWeapon( CBasePlayer *pPlayer );
	virtual int   GetTeamIndex( const char *pTeamName );
	virtual const char *GetTeamID( CBaseEntity *pEntity );
	virtual int   PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual BOOL  IsTeamplay( void ) { return TRUE; }
	virtual BOOL  IsRoundBased( void ) { return TRUE; }
	virtual BOOL  IsLoot( void ) { return TRUE; }
	virtual BOOL  MutatorAllowed( const char *mutator );
	virtual int   IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled );
	virtual BOOL  AllowRuneSpawn( const char *szRune );
	virtual BOOL  IsArmoredMan( CBasePlayer *pPlayer ) { return FALSE; }
	virtual BOOL  CanHaveNamedItem( CBasePlayer *pPlayer, const char *pszItemName );

	// CTC-style canonical charm methods (central authority for loot state changes)
	virtual void        CaptureCharm( CBasePlayer *pPlayer );
	virtual CBaseEntity *DropCharm( CBasePlayer *pPlayer, Vector origin );

	// Called by CLootCrate when it breaks and m_bHasLoot is set
	void SpawnLootAtPosition( Vector origin );

	// Called by CLootCrate whenever any crate breaks (loot or empty)
	void OnCrateBroken( void );

	// Called by CLootCrate when a crate falls out of map bounds
	void OnCrateLost( class CLootCrate *pCrate );

	// Called by CLootGoal when a loot-holder touches it
	void OnGoalReached( CBasePlayer *pPlayer );

	// Checked by CLootEntity to avoid triggering pickup when nothing is going on
	BOOL IsGameInProgress( void ) { return g_GameInProgress; }
	BOOL IsLootFree( void ) { return (CBaseEntity *)m_hLootHolder == NULL; }

	static const char *s_TeamNames[4]; // "iceman", "santa", "holo", "commando"

private:
	void StartRound( int clients );
	void EndRound( int winningTeam ); // -1 = tiebreaker; >= 0 = goal already scored by OnGoalReached
	void ExposeLoot( void );          // Break all crates, move loot to random spawn point
	void SpawnLootEntities( void );
	BOOL FindSafeFloorPosition( Vector &out, float minDistFromAvoid, Vector avoid, int maxAttempts );
	void GiveTeamWeapons( int teamIndex );
	void StripToFists( CBasePlayer *pPlayer );
	void ReduceToOneWeapon( CBasePlayer *pPlayer );
	void SendObjectiveUpdate( void );
	void CleanupRoundEntities( void );
	void SendGoalSpecialEntity( void );
	int  CountNonFistWeapons( CBasePlayer *pPlayer );

	EHANDLE m_hLootEntity;          // The free loot item in the world
	EHANDLE m_hGoalEntity;          // The scoring goal zone
	EHANDLE m_hLootHolder;          // The player currently holding loot (cast to CBasePlayer*)

	float   m_flTeamHoldTime[4];    // Accumulated seconds each team held loot (tiebreaker)
	float   m_flLootPickupTime;     // gpGlobals->time when current holder picked up loot

	int     m_iTeamPlayers[4][32];  // Player entity indices per team
	int     m_iTeamPlayerCount[4];  // Number of players per team
	Vector  m_vecTeamSpawnOrigin[4];// Base spawn position per team (set in StartRound)

	float   m_fSendBannerTime;      // When to broadcast the round-start banner
	float   m_flLastObjUpdate;      // Rate-limit for SendObjectiveUpdate

	// Spawn grouping: set SOLID_NOT, restore after this time
	// TODO: FALLBACK -- if SOLID_NOT on a living player causes a crash, remove the
	// pev->solid assignments in StartRound and accept a brief overlap at spawn.
	float   m_fSpawnGroupSolidRestoreTime;

	// Deferred round-end after goal celebration (4 second window)
	float   m_flCelebrationEndTime;  // gpGlobals->time when EndRound should fire; 0 = not pending
	int     m_iPendingWinnerTeam;    // team index to pass to EndRound when the timer fires

	// Loot exposure event (5-min no-pickup or 60s left)
	float   m_flRoundStartTime;      // gpGlobals->time when current round started
	BOOL    m_bLootExposed;          // TRUE once crates shattered and loot placed at deathmatch spawn

	// Crate count tracking
	int     m_iTotalCrates;          // How many crates were spawned this round
	int     m_iCratesLeft;           // How many are still standing

	Vector  m_vecUsedSpots[32];     // Spawn origins already assigned to a team (dedup)
	int     m_iUsedSpotCount;
};
