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

class CHalfLifeShidden : public CHalfLifeMultiplay
{
public:
	CHalfLifeShidden();
	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered Shidden"; }
	virtual void Think( void );
	virtual void InitHUD( CBasePlayer *pPlayer );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual void PlayerThink( CBasePlayer *pPlayer );
	virtual BOOL HasGameTimerExpired( void );
	virtual BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual void FPlayerTookDamage( float flDamage, CBasePlayer *pVictim, CBaseEntity *pKiller);
	virtual BOOL FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual BOOL CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon );
	virtual int GetTeamIndex( const char *pTeamName );
	virtual BOOL CanRandomizeWeapon( const char *name );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual const char *GetTeamID( CBaseEntity *pEntity );
	virtual BOOL ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual BOOL MutatorAllowed(const char *mutator);
	virtual BOOL IsRoundBased( void );
	virtual void DetermineWinner( void );
	virtual BOOL IsTeamplay( void );

private:
	int m_iSmeltersRemain;
	int m_iDealtersRemain;
};
