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

class CHalfLifeColdSpot : public CHalfLifeMultiplay
{
public:
	CHalfLifeColdSpot();
	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered Cold Spot"; }
	BOOL IsSpawnPointValid( CBaseEntity *pSpot );
	edict_t *EntSelectSpawnPoint( const char *szSpawnPoint );
	virtual void InitHUD( CBasePlayer *pPlayer );
	virtual void Think( void );
	virtual int GetTeamIndex( const char *pTeamName );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual const char *GetTeamID( CBaseEntity *pEntity );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual void ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer );
	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual BOOL ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	void UpdateHud( void );

private:
	BOOL m_DisableDeathPenalty;
	float m_fSpawnColdSpot;
	float m_fColdSpotTime;
	EHANDLE pLastSpawnPoint;
	EHANDLE pColdSpot;
	int m_iBlueScore;
	int m_iRedScore;
};
