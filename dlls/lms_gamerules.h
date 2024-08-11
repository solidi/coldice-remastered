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

class CHalfLifeLastManStanding : public CHalfLifeMultiplay
{
public:
	CHalfLifeLastManStanding();
	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered Battle Royale"; }
	virtual void Think( void );
	virtual BOOL HasGameTimerExpired( void );
	virtual void InitHUD( CBasePlayer *pPlayer );
	virtual BOOL FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled );
	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual BOOL AllowRuneSpawn( const char *szRune );
	virtual int GetTeamIndex( const char *pTeamName );
	virtual void PlayerSpawn( CBasePlayer *pPlayer );
	virtual BOOL IsRoundBased( void );

};
