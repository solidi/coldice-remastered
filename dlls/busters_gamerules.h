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

class CMultiplayBusters : public CHalfLifeMultiplay
{
public:
	CMultiplayBusters();
	virtual const char *GetGameDescription( void ) { return "Cold Ice Cold Busters"; }
	virtual void InitHUD( CBasePlayer *pPlayer );
	virtual void Think( void );
	virtual int IPointsForKill( CBasePlayer* pAttacker, CBasePlayer* pKilled );
	virtual void PlayerKilled( CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor );
	virtual void DeathNotice( CBasePlayer* pVictim, entvars_t* pKiller, entvars_t* pInflictor );
	virtual int WeaponShouldRespawn( CBasePlayerItem* pWeapon );
	virtual BOOL CanHavePlayerItem( CBasePlayer* pPlayer, CBasePlayerItem* pWeapon );
	virtual BOOL CanHaveItem( CBasePlayer* pPlayer, CItem* pItem );
	virtual void PlayerGotWeapon( CBasePlayer* pPlayer, CBasePlayerItem* pWeapon );
	virtual void ClientUserInfoChanged( CBasePlayer* pPlayer, char* infobuffer );
	virtual void PlayerSpawn( CBasePlayer* pPlayer );
	virtual int GetTeamIndex( const char *pTeamName );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual const char *GetTeamID( CBaseEntity *pEntity );
	virtual BOOL ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual BOOL MutatorAllowed(const char *mutator);
	virtual BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );

	void SetPlayerModel( CBasePlayer* pPlayer );

protected:
	
	float m_flEgonBustingCheckTime = -1.0f;
	void CheckForEgons( void );
};
