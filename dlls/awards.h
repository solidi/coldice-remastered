//
// awards.h
//
// Server-side multiplayer award system modeled after GoldenEye 007 (N64).
// Entity code reports raw gameplay events; this class owns every statistic,
// every award rule and the network update. See ai/award_system.md.
//

#ifndef AWARDS_H
#define AWARDS_H

#include "awards_shared.h"
#include "cdll_dll.h"

#define AWARDS_MAX_PLAYERS	32

class CBaseEntity;
class CBasePlayer;
class CBaseMonster;

class CMultiplayAwards
{
public:
	CMultiplayAwards();

	// Lifecycle
	void Reset( void );										// new map: every slot back to baseline
	void ClientPutInServer( edict_t *pEntity );				// (re)connect: fresh baseline for the slot
	void ClientDisconnected( edict_t *pEntity );			// wipe the slot
	void ClientHUDInitialized( CBasePlayer *pPlayer );		// client HUD was reset; resync it
	void Think( void );										// once per server frame (StartFrame)

	// Gameplay events
	void ShotFired( CBasePlayer *pShooter );
	void Headshot( CBaseEntity *pVictim, entvars_t *pevAttacker );
	void PlayerDamaged( CBasePlayer *pVictim, entvars_t *pevInflictor, entvars_t *pevAttacker );
	void PlayerKilled( CBasePlayer *pVictim, entvars_t *pevAttacker );
	void MonsterKilled( CBaseMonster *pVictim, entvars_t *pevAttacker );
	void ArmorPickedUp( CBasePlayer *pPlayer );

	// Last computed packed value (see awards_shared.h) for a player slot.
	int GetPackedAwards( int iPlayerIndex ) const;

private:
	struct stats_t
	{
		BOOL	tracked;				// connected player (human or bot)
		BOOL	bot;					// ranked as a rival, never awarded or sent anything
		int		shots;
		int		headshots;
		int		kills;
		int		deaths;
		int		suicides;
		int		armorPickups;
		int		hits;					// damaging hits landed on other players
		int		backHits;				// ... of which landed from behind
		int		bestMultiKill;
		float	lastKillTimes[4];
		float	lastShotTime;
		float	lastHeadshotTime;
		float	timeOthersInView;		// player-seconds other players were on screen
		float	distance;				// horizontal units travelled while alive
		float	aliveTime;
		float	lifeStart;
		float	longestLife;
		float	shortestLife;			// < 0 until the first completed life
		BOOL	alive;
		BOOL	hasLastPos;
		Vector	lastPos;
		float	weaponTime[MAX_WEAPONS];	// weapon of choice: kills, then shots, then time held
		int		weaponKills[MAX_WEAPONS];
		int		weaponShots[MAX_WEAPONS];
		int		tieBreak;				// stable random tie-breaker
		int		order[AWARD_COUNT - 1];	// stable random award pick order
		int		packed;
		int		lastSentPacked;			// -1 forces a send
	};

	BOOL IsActive( void ) const;
	BOOL IsInPlay( CBasePlayer *pPlayer ) const;
	BOOL IsCountedMonster( CBaseEntity *pEntity ) const;
	stats_t *PlayerStats( CBaseEntity *pEntity );
	void ResetSlot( int iIndex, BOOL bTracked, BOOL bBot = FALSE );
	void EndLife( stats_t &s, BOOL bDied );
	void CreditKill( CBaseEntity *pAttacker );
	void Sample( float flInterval );
	int CountOthersInView( CBasePlayer *pPlayer );
	void Refresh( void );
	int FindBest( const int *pPool, int iCount, const float *pValues, BOOL bHighest, const BOOL *pAllowed ) const;
	int ActiveWeaponId( CBaseEntity *pEntity ) const;
	int WeaponOfChoice( const stats_t &s ) const;

	stats_t	m_Stats[AWARDS_MAX_PLAYERS + 1];
	float	m_flNextSample;
	float	m_flLastSample;
	float	m_flNextRefresh;
	float	m_flLastRefresh;
	BOOL	m_bDirty;
};

extern CMultiplayAwards g_Awards;

#endif // AWARDS_H
