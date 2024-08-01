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

class CHalfLifeInstagib : public CHalfLifeMultiplay
{
public:
	CHalfLifeInstagib();
	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered Instagib"; }
	virtual void InitHUD( CBasePlayer *pPlayer );
	virtual BOOL MutatorAllowed(const char *mutator);
	virtual void PlayerThink( CBasePlayer *pPlayer );
	virtual BOOL IsAllowedToSpawn( CBaseEntity *pEntity );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual int DeadPlayerWeapons( CBasePlayer *pPlayer );
	virtual int DeadPlayerAmmo( CBasePlayer *pPlayer );
	virtual BOOL IsAllowedToDropWeapon( CBasePlayer *pPlayer );

};
