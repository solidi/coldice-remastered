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

class CHalfLifeGunGame : public CHalfLifeMultiplay
{
public:
	CHalfLifeGunGame();

	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled );
	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered GunGame"; }
	virtual void UpdateGameMode( CBasePlayer *pPlayer );
	virtual void Think ( void );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual BOOL IsAllowedToSpawn( CBaseEntity *pEntity );
	virtual int DeadPlayerWeapons( CBasePlayer *pPlayer );
	virtual int DeadPlayerAmmo( CBasePlayer *pPlayer );
	virtual BOOL IsAllowedSingleWeapon( CBaseEntity *pEntity );
	virtual BOOL IsAllowedToDropWeapon( void );

private:
	float m_fGoToIntermission;
	int m_iTopLevel;
	float m_fRefreshStats;
	EHANDLE m_hLeader;
	EHANDLE m_hVoiceHandle;
	int m_iVoiceId;
};