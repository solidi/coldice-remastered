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

class CHalfLifeCaptureTheChumtoad : public CHalfLifeMultiplay
{
public:
	CHalfLifeCaptureTheChumtoad();

	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered Capture The Chumtoad"; }
	virtual void Think( void );
	virtual void PlayerThink( CBasePlayer *pPlayer );
	virtual int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled );
	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual BOOL AllowGrapplingHook( CBasePlayer *pPlayer );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual void ClientDisconnected( edict_t *pClient );
	virtual int DeadPlayerWeapons( CBasePlayer *pPlayer );
	virtual BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual BOOL CreateChumtoad();
	virtual void CaptureCharm( CBasePlayer *pPlayer );
	virtual CBaseEntity *DropCharm( CBasePlayer *pPlayer, Vector origin );
	virtual BOOL IsAllowedToSpawn( CBaseEntity *pEntity );
	virtual int WeaponShouldRespawn( CBasePlayerItem *pWeapon );
	virtual BOOL CanRandomizeWeapon(const char *name);

private:
	BOOL m_fChumtoadInPlay;
	float m_fChumtoadPlayTimer;
	float m_fCreateChumtoadTimer;
	float m_fMoveChumtoadTimer;
};
