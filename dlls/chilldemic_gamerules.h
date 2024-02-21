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

class CHalfLifeChilldemic : public CHalfLifeMultiplay
{
public:
	CHalfLifeChilldemic();
	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered Chilldemic"; }
	virtual void Think ( void );
	virtual void InitHUD( CBasePlayer *pl );
	virtual BOOL CheckGameTimer( void );
	virtual BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual BOOL FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual BOOL CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon );
	virtual int GetTeamIndex( const char *pTeamName );
	virtual BOOL CanRandomizeWeapon( const char *name );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual const char *GetTeamID( CBaseEntity *pEntity );

private:
	int m_iSurvivorsRemain;
};
