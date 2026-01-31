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

class CHalfLifeCaptureTheFlag : public CHalfLifeMultiplay
{
public:
	CHalfLifeCaptureTheFlag();
	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered Capture The Flag"; }
	virtual edict_t *EntSelectSpawnPoint( const char *szSpawnPoint );
	virtual BOOL IsSpawnPointValid( CBaseEntity *pSpot );
	virtual void InitHUD( CBasePlayer *pPlayer );
	virtual void Think( void );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual int GetTeamIndex( const char *pTeamName );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual const char *GetTeamID( CBaseEntity *pEntity );
	virtual void ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer );
	virtual CBaseEntity *DropCharm( CBasePlayer *pPlayer, Vector origin );
	virtual void ClientDisconnected( edict_t *pClient );
	virtual void UpdateHud( int bluemode, int redmode, CBasePlayer *pPlayer = NULL );
	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual BOOL ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual BOOL IsTeamplay( void );

	EHANDLE pRedBase;
	EHANDLE pBlueBase;

private:
	float m_fSpawnBlueHardware = -1;
	float m_fSpawnRedHardware = -1;
	int m_iBlueScore;
	int m_iRedScore;
	int m_iBlueMode;
	int m_iRedMode;
	BOOL m_DisableDeathPenalty;
};
