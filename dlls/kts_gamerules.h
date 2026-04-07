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

class CHalfLifeKickTheSnowball : public CHalfLifeMultiplay
{
public:
	CHalfLifeKickTheSnowball();
	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered Kick the Snowball"; }
	virtual edict_t *EntSelectSpawnPoint( const char *szSpawnPoint );
	virtual BOOL IsSpawnPointValid( CBaseEntity *pSpot );
	virtual void InitHUD( CBasePlayer *pPlayer );
	virtual void Think( void );
	virtual void PlayerThink( CBasePlayer *pPlayer );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual int GetTeamIndex( const char *pTeamName );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual const char *GetTeamID( CBaseEntity *pEntity );
	virtual void ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer );
	virtual void ClientDisconnected( edict_t *pClient );
	virtual void UpdateHud( int bluemode, int redmode, CBasePlayer *pPlayer = NULL );
	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual BOOL ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual void FPlayerTookDamage( float flDamage, CBasePlayer *pVictim, CBaseEntity *pKiller );
	virtual void CaptureCharm( CBasePlayer *pPlayer );
	virtual CBaseEntity *DropCharm( CBasePlayer *pPlayer, Vector origin );
	virtual BOOL IsTeamplay( void );
	virtual BOOL MutatorAllowed(const char *mutator);
	virtual BOOL CanHaveNamedItem( CBasePlayer *pPlayer, const char *pszItemName );
	virtual int DeadPlayerWeapons( CBasePlayer *pPlayer );
	virtual int DeadPlayerAmmo( CBasePlayer *pPlayer );
	virtual BOOL IsAllowedToDropWeapon( CBasePlayer *pPlayer );
	virtual BOOL AllowRuneSpawn( const char *szRune );
	virtual BOOL IsAllowedToSpawn( CBaseEntity *pEntity );

	void AutoJoin( CBasePlayer *pPlayer, int team );
	void OnGoalScored( int scoringTeam, CBaseEntity *pBall );
	void SpawnBallAtMidpoint( void );

	EHANDLE pRedGoal;
	EHANDLE pBlueGoal;
	EHANDLE pBall;

private:
	float m_fSpawnBlueGoal;
	float m_fSpawnRedGoal;
	float m_fBallRespawnTime;
	int m_iBlueScore;
	int m_iRedScore;
	int m_iBlueMode;
	int m_iRedMode;
	BOOL m_DisableDeathPenalty;
};
