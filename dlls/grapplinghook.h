#if defined( GRAPPLING_HOOK )

#include "game.h"

extern cvar_t grapplesky;

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

private:
	Vector m_vPlayerHangOrigin;
	BOOL m_fPlayerAtEnd;
	short ropesprite;
	BOOL m_fHookInWall;
	BOOL m_fActiveHook;
	Vector m_vVecDirHookMove;
	EHANDLE pevOwner;
};

#endif
