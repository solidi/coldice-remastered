#if defined( GRAPPLING_HOOK )

#include "game.h"

class CHook : public CBaseEntity
{
public:
	static CHook *HookCreate( CBasePlayer *owner );
	void Spawn( void );
	int Classify( void );
	void Precache( void );
	void EXPORT HookTouch( CBaseEntity *pOther );
	void EXPORT Think ( void );
	void FireHook( void );
	void KillHook( void );

	virtual void UpdateOnRemove( void );

	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION | FCAP_PORTAL; }

private:
	void BeginFatalPull( CBasePlayer *pVictim );
	void UpdateFatalPull( void );
	void RestoreFatalVictimPhysics( void );
	void DoFatalUppercut( CBasePlayer *pOwner, CBasePlayer *pVictim );

	Vector m_vPlayerHangOrigin;
	BOOL m_fPlayerAtEnd;
	short ropesprite;
	BOOL m_fHookInWall;
	BOOL m_fActiveHook;
	Vector m_vVecDirHookMove;
	EHANDLE m_hFatalVictim;
	BOOL m_fFatalPull;
	BOOL m_fVictimPhysicsCaptured;
	int m_iVictimMoveType;
	float m_flVictimGravity;
	float m_flVictimFriction;
	float m_flFatalAbortTime;
	EHANDLE pevOwner;
};

#endif
