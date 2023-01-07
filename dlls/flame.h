#include "weapons.h"

class CFlame : public CPointEntity
{
public:
	static CFlame *CreateFlameStream( entvars_t *pevOwner, Vector vecStart,  Vector velocity );
	void Spawn( void );
	void EXPORT FlameThink( void );
	void EXPORT FlameTouch( CBaseEntity *pOther );
	void EXPORT DieThink( void );
	int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);

/*
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
*/

private:
	float starttime;
	float dmgtime;
	BOOL implacedmydecal;
};
