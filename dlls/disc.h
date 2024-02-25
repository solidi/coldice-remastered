#define DISC_VELOCITY 1000
#define MAX_DISCS 3
#define DISC_PUSH_MULTIPLIER 1200

class CDisc : public CGrenade
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	EXPORT DiscTouch( CBaseEntity *pOther );
	void	EXPORT DiscThink( void );
	static	CDisc *CreateDisc( Vector vecOrigin, Vector vecAngles, CBaseEntity *pOwner, bool bDecapitator, int iPowerupFlags );

	//void	SetObjectCollisionBox( void );
	void	ReturnToThrower( void );

	virtual BOOL	IsDisc( void ) { return TRUE; };

	float		m_fDontTouchEnemies;	// Prevent enemy touches for a bit
	float		m_fDontTouchOwner;		// Prevent friendly touches for a bit
	int			m_iBounces;		// Number of bounces
	EHANDLE		m_hOwner;		// Don't store in pev->owner, because it needs to hit its owner
	int			m_iTrail;
	int			m_iSpriteTexture;
	bool		m_bDecapitate;	// True if this is a decapitating shot
	bool		m_bRemoveSelf;  // True if the owner of this disc has died
	int			m_iPowerupFlags;// Flags for any powerups active on this disc
	bool		m_bTeleported;  // Disc has gone through a teleport

	EHANDLE m_pLockTarget;
	
	Vector	m_vecActualVelocity;
	Vector	m_vecSideVelocity;
	Vector	m_vecOrg;
};
