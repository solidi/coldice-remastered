/***
*
*    Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*    This product contains software technology licensed from Id
*    Software, Inc. ("Id Technology"). Id Technology (c) 1996 Id Software, Inc.
*    All Rights Reserved.
*
*    Use, distribution, and modification of this source code and/or resulting
*    object code is restricted to non-commercial enhancements to products from
*    Valve LLC. All other use, distribution, or modification is prohibited
*    without written permission from Valve LLC.
*
****/

#ifndef NAPALM_POOL_H
#define NAPALM_POOL_H

class CBasePlayer;

class CNapalmPool : public CBaseEntity
{
public:
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
    static BOOL CanDeployPools( CBasePlayer *pPlayer );

    static CNapalmPool *CreatePool( entvars_t *pevOwner, const Vector &vecOrigin,
        const Vector &vecSurfaceNormal, float flDamagePerTick, float flRadius, float flLifetime );
    static int DeployPools( CBasePlayer *pPlayer, int iDesiredPools, float flDamagePerTick, float flRadius );
    static int DeployExplosionPools( const Vector &vecOrigin, float flDamage, float flRadius,
        entvars_t *pevOwner, edict_t *pIgnoreEntity );

    void Spawn( void );
    void Precache( void );
    void EXPORT BurnThink( void );

    static TYPEDESCRIPTION m_SaveData[];

private:
    void EmitFlameFx( void );

    float m_flExpireTime;
    float m_flNextDamageTime;
    float m_flNextFxTime;
    float m_flDamagePerTick;
    float m_flRadius;
    Vector m_vecSurfaceNormal;
    EHANDLE m_hNapalmOwner;
};

#endif // NAPALM_POOL_H
