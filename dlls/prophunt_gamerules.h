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

class CHalfLifePropHunt : public CHalfLifeMultiplay
{
public:
	CHalfLifePropHunt();
	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered Prop Hunt"; }
	virtual void Think( void );
	virtual BOOL HasGameTimerExpired( void );
	virtual void InitHUD( CBasePlayer *pl );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual BOOL FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual void FPlayerTookDamage( float flDamage, CBasePlayer *pVictim, CBaseEntity *pKiller);

	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual int GetTeamIndex( const char *pTeamName );
	virtual const char *GetTeamID( CBaseEntity *pEntity );
	virtual BOOL ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual BOOL CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem );
	virtual BOOL CanHaveItem( CBasePlayer *pPlayer, CItem *pItem );
	virtual BOOL IsAllowedToDropWeapon( CBasePlayer *pPlayer );
	virtual int DeadPlayerWeapons( CBasePlayer *pPlayer );
	virtual int DeadPlayerAmmo( CBasePlayer *pPlayer );
	virtual BOOL CanHavePlayerAmmo( CBasePlayer *pPlayer, CBasePlayerAmmo *pAmmo );
	virtual void MonsterKilled( CBaseMonster *pVictim, entvars_t *pKiller );
	virtual void PlayerThink( CBasePlayer *pPlayer );
	virtual BOOL IsRoundBased( void );
	virtual BOOL FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual BOOL MutatorAllowed(const char *mutator);

private:
	int m_iHuntersRemain;
	int m_iPropsRemain;
	float m_fUnFreezeHunters;
};
