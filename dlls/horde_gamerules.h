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

class CHalfLifeHorde : public CHalfLifeMultiplay
{
public:
	CHalfLifeHorde();
	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered Horde"; }
	virtual BOOL IsSpawnPointValid( CBaseEntity *pSpot );
	virtual edict_t *EntSelectSpawnPoint( const char *szSpawnPoint );
	virtual void Think( void );
	virtual void InitHUD( CBasePlayer *pPlayer );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual BOOL HasGameTimerExpired( void );
	virtual BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual void FPlayerTookDamage( float flDamage, CBasePlayer *pVictim, CBaseEntity *pKiller);
	virtual BOOL FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual int GetTeamIndex( const char *pTeamName );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual const char *GetTeamID( CBaseEntity *pEntity );
	virtual void MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller );
	virtual BOOL ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual BOOL IsRoundBased( void );

private:
	int m_iSurvivorsRemain;
	int m_iEnemiesRemain;
	int m_iTotalEnemies;
	int m_iWaveNumber;
	float m_fBeginWaveTime;

	EHANDLE pLastSpawnPoint;
};
