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
#ifndef ITEMS_H
#define ITEMS_H


class CItem : public CBaseEntity
{
public:
	void	Spawn( void );
	CBaseEntity*	Respawn( void );
	void	EXPORT ItemTouch( CBaseEntity *pOther );
	void	EXPORT Materialize( void );
	virtual BOOL MyTouch( CBasePlayer *pPlayer ) { return FALSE; };
	int 	TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
};

#define RUNE_FRAG		1
#define RUNE_VAMPIRE	2
#define RUNE_PROTECT	3
#define RUNE_REGEN		4
#define RUNE_HASTE		5
#define RUNE_GRAVITY	6
#define RUNE_STRENGTH	7
#define RUNE_CLOAK		8
#define RUNE_AMMO		9

class CWorldRunes : public CBaseEntity
{
public:
	static void Create( void );
	static void DropRune(CBasePlayer *pPlayer);
	void Spawn( void );
	void EXPORT SpawnRunes( void );
	void Precache( void );
	void CreateRune(char *sz_RuneClass);

	CBaseEntity *SelectSpawnPoint(const char *spot);
};

class CRune : public CBaseEntity
{
public:
	void	Spawn( void );
	void	EXPORT MakeTrigger( void );
	void	EXPORT RuneTouch( CBaseEntity *pOther );
	void	EXPORT Materialize( void );
	virtual BOOL MyTouch( CBasePlayer *pPlayer ) { return FALSE; };
	void ShowStatus(CBasePlayer *pPlayer, const char *icon, int r, int g, int b);
	void ShellPlayer(CBasePlayer *pPlayer, int r, int g, int b);
};

#endif // ITEMS_H
