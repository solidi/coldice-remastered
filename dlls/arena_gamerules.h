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

class CHalfLifeArena : public CHalfLifeMultiplay
{
public:
	CHalfLifeArena();
	virtual const char *GetGameDescription( void ) { return "Cold Ice Remastered Arena"; }
	virtual void Think ( void );
	virtual BOOL HasGameTimerExpired( void );
	virtual void InitHUD( CBasePlayer *pl );
	virtual BOOL FPlayerCanRespawn( CBasePlayer *pPlayer );
	virtual void PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor );

private:
	int m_iPlayer1, m_iPlayer2;
};
