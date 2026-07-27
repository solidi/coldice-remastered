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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "napalm_pool.h"
#include "game.h"
#include <math.h>

LINK_ENTITY_TO_CLASS( napalm_pool, CNapalmPool );

static const float NAPALM_DEFAULT_LIFETIME = 5.0f;
static const float NAPALM_DAMAGE_INTERVAL = 0.3f;
static const float NAPALM_FX_INTERVAL = 0.15f;
static const float NAPALM_SURFACE_OFFSET = 2.0f;
static const float NAPALM_TRACE_DISTANCE = 320.0f;

static BOOL IsValidNapalmSurface( CBaseEntity *pEntity )
{
    if (pEntity == NULL)
        return FALSE;

    if (pEntity->pev->flags & FL_CONVEYOR)
        return FALSE;

    return pEntity->pev->movetype == MOVETYPE_NONE || pEntity->pev->movetype == MOVETYPE_PUSH;
}

static Vector BuildNapalmTangent( const Vector &vecNormal )
{
    Vector vecUp = fabs( vecNormal.z ) > 0.9f ? Vector( 1, 0, 0 ) : Vector( 0, 0, 1 );
    Vector vecTangent = CrossProduct( vecNormal, vecUp );

    if (vecTangent.Length() < 0.001f)
        return Vector( 1, 0, 0 );

    return vecTangent.Normalize();
}

TYPEDESCRIPTION CNapalmPool::m_SaveData[] =
{
    DEFINE_FIELD( CNapalmPool, m_flExpireTime, FIELD_TIME ),
    DEFINE_FIELD( CNapalmPool, m_flNextDamageTime, FIELD_TIME ),
    DEFINE_FIELD( CNapalmPool, m_flNextFxTime, FIELD_TIME ),
    DEFINE_FIELD( CNapalmPool, m_flDamagePerTick, FIELD_FLOAT ),
    DEFINE_FIELD( CNapalmPool, m_flRadius, FIELD_FLOAT ),
    DEFINE_FIELD( CNapalmPool, m_vecSurfaceNormal, FIELD_VECTOR ),
    DEFINE_FIELD( CNapalmPool, m_hNapalmOwner, FIELD_EHANDLE ),
};

int CNapalmPool::Save( CSave &save )
{
    if (!CBaseEntity::Save( save ))
        return 0;

    return save.WriteFields( "CNapalmPool", this, m_SaveData, ARRAYSIZE( m_SaveData ) );
}

int CNapalmPool::Restore( CRestore &restore )
{
    if (!CBaseEntity::Restore( restore ))
        return 0;

    return restore.ReadFields( "CNapalmPool", this, m_SaveData, ARRAYSIZE( m_SaveData ) );
}

CNapalmPool *CNapalmPool::CreatePool( entvars_t *pevOwner, const Vector &vecOrigin,
    const Vector &vecSurfaceNormal, float flDamagePerTick, float flRadius, float flLifetime )
{
    CNapalmPool *pPool = GetClassPtr( (CNapalmPool *)NULL );
    if (pPool == NULL)
        return NULL;

    pPool->Spawn();
    UTIL_SetOrigin( pPool->pev, vecOrigin );

    pPool->m_vecSurfaceNormal = vecSurfaceNormal.Normalize();

    if (pPool->m_vecSurfaceNormal.Length() < 0.001f)
        pPool->m_vecSurfaceNormal = Vector( 0, 0, 1 );

    pPool->pev->angles = UTIL_VecToAngles( pPool->m_vecSurfaceNormal );

    pPool->m_flDamagePerTick = flDamagePerTick;
    pPool->m_flRadius = flRadius;
    pPool->m_flExpireTime = gpGlobals->time + flLifetime;
    pPool->m_flNextDamageTime = gpGlobals->time;
    pPool->m_flNextFxTime = gpGlobals->time;

    if (pevOwner)
    {
        pPool->pev->owner = ENT( pevOwner );
        pPool->m_hNapalmOwner = CBaseEntity::Instance( pPool->pev->owner );
    }
    else
    {
        pPool->pev->owner = NULL;
        pPool->m_hNapalmOwner = NULL;
    }

    return pPool;
}

BOOL CNapalmPool::CanDeployPools( CBasePlayer *pPlayer )
{
    if (pPlayer == NULL)
        return FALSE;

    UTIL_MakeVectors( pPlayer->pev->v_angle + pPlayer->pev->punchangle );

    Vector vecSrc = pPlayer->GetGunPosition();
    TraceResult tr;
    UTIL_TraceLine( vecSrc, vecSrc + gpGlobals->v_forward * NAPALM_TRACE_DISTANCE,
        dont_ignore_monsters, ENT( pPlayer->pev ), &tr );

    if (tr.fStartSolid || tr.flFraction >= 1.0)
        return FALSE;

    CBaseEntity *pHitEntity = CBaseEntity::Instance( tr.pHit );
    if (!IsValidNapalmSurface( pHitEntity ))
        return FALSE;

    return TRUE;
}

int CNapalmPool::DeployPools( CBasePlayer *pPlayer, int iDesiredPools, float flDamagePerTick, float flRadius )
{
#ifndef CLIENT_DLL
    if (pPlayer == NULL || iDesiredPools <= 0)
        return 0;

    if (!CanDeployPools( pPlayer ))
        return 0;

    UTIL_MakeVectors( pPlayer->pev->v_angle + pPlayer->pev->punchangle );

    Vector vecSrc = pPlayer->GetGunPosition();
    TraceResult tr;
    UTIL_TraceLine( vecSrc, vecSrc + gpGlobals->v_forward * NAPALM_TRACE_DISTANCE,
        dont_ignore_monsters, ENT( pPlayer->pev ), &tr );

    Vector vecBaseNormal = tr.vecPlaneNormal;
    if (vecBaseNormal.Length() < 0.001f)
        vecBaseNormal = Vector( 0, 0, 1 );
    vecBaseNormal = vecBaseNormal.Normalize();

    Vector vecTangent = BuildNapalmTangent( vecBaseNormal );
    Vector vecBitangent = CrossProduct( vecBaseNormal, vecTangent ).Normalize();
    float flSpread = flRadius * 0.75f;
    int iRingDivisor = iDesiredPools - 1;

    if (iRingDivisor < 1)
        iRingDivisor = 1;

    int iCreated = 0;
    for (int i = 0; i < iDesiredPools; i++)
    {
        Vector vecOffset = g_vecZero;
        if (i > 0)
        {
            float flAngle = (float)(i - 1) / (float)iRingDivisor * 6.283185307f;
            vecOffset = (vecTangent * cos( flAngle ) + vecBitangent * sin( flAngle )) * flSpread;
        }

        Vector vecProbeStart = tr.vecEndPos + vecOffset + vecBaseNormal * 24;
        Vector vecProbeEnd = tr.vecEndPos + vecOffset - vecBaseNormal * 32;

        TraceResult trPool;
        UTIL_TraceLine( vecProbeStart, vecProbeEnd, dont_ignore_monsters, ENT( pPlayer->pev ), &trPool );

        if (trPool.fStartSolid || trPool.flFraction >= 1.0)
            continue;

        CBaseEntity *pPoolSurface = CBaseEntity::Instance( trPool.pHit );
        if (!IsValidNapalmSurface( pPoolSurface ))
            continue;

        Vector vecPoolNormal = trPool.vecPlaneNormal;
        if (vecPoolNormal.Length() < 0.001f)
            vecPoolNormal = vecBaseNormal;

        vecPoolNormal = vecPoolNormal.Normalize();

        Vector vecPoolOrigin = trPool.vecEndPos + vecPoolNormal * NAPALM_SURFACE_OFFSET;
        if (CreatePool( pPlayer->pev, vecPoolOrigin, vecPoolNormal,
            flDamagePerTick, flRadius, NAPALM_DEFAULT_LIFETIME ))
        {
            iCreated++;
        }
    }

    return iCreated;
#else
    return 0;
#endif
}

void CNapalmPool::Spawn( void )
{
    Precache();

    pev->classname = MAKE_STRING( "napalm_pool" );
    pev->movetype = MOVETYPE_NONE;
    pev->solid = SOLID_NOT;
    pev->takedamage = DAMAGE_NO;
    pev->effects |= EF_NODRAW;
    pev->rendermode = kRenderTransAdd;

    SET_MODEL( ENT( pev ), "sprites/null.spr" );
    UTIL_SetSize( pev, g_vecZero, g_vecZero );
    UTIL_SetOrigin( pev, pev->origin );

    m_flDamagePerTick = 1.0f;
    m_flRadius = 48.0f;
    m_flExpireTime = gpGlobals->time + NAPALM_DEFAULT_LIFETIME;

    m_flNextDamageTime = gpGlobals->time;
    m_flNextFxTime = gpGlobals->time;
    m_vecSurfaceNormal = Vector( 0, 0, 1 );
    m_hNapalmOwner = NULL;

    SetTouch( NULL );
    SetThink( &CNapalmPool::BurnThink );
    pev->nextthink = gpGlobals->time + 0.05f;
}

void CNapalmPool::Precache( void )
{
    PRECACHE_MODEL( "sprites/null.spr" );
    PRECACHE_MODEL( "sprites/flamesteam.spr" );
    PRECACHE_MODEL( "sprites/ice_fire.spr" );
    PRECACHE_SOUND( "flame_hitwall.wav" );
}

void CNapalmPool::EmitFlameFx( void )
{
#ifndef CLIENT_DLL
    Vector vecNormal = m_vecSurfaceNormal;
    if (vecNormal.Length() < 0.001f)
        vecNormal = Vector( 0, 0, 1 );

    Vector vecTangent = BuildNapalmTangent( vecNormal );
    Vector vecBitangent = CrossProduct( vecNormal, vecTangent ).Normalize();

    Vector vecFxPos = pev->origin + vecNormal * RANDOM_FLOAT( 1.0f, 4.0f );
    float flSpan = m_flRadius * 0.35f;
    vecFxPos = vecFxPos + vecTangent * RANDOM_FLOAT( -flSpan, flSpan );
    vecFxPos = vecFxPos + vecBitangent * RANDOM_FLOAT( -flSpan, flSpan );

    MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecFxPos );
        WRITE_BYTE( TE_SPRITE );
        WRITE_COORD( vecFxPos.x );
        WRITE_COORD( vecFxPos.y );
        WRITE_COORD( vecFxPos.z );
        if (icesprites.value)
            WRITE_SHORT( g_sModelIndexIceFire );
        else
            WRITE_SHORT( g_sModelIndexFlame );
        WRITE_BYTE( RANDOM_LONG( 4, 8 ) );
        WRITE_BYTE( RANDOM_LONG( 120, 180 ) );
    MESSAGE_END();
#endif
}

void CNapalmPool::BurnThink( void )
{
    if (!IsInWorld())
    {
        UTIL_Remove( this );
        return;
    }

    if (pev->waterlevel == 3 || UTIL_PointContents( pev->origin ) == CONTENTS_WATER)
    {
        UTIL_Remove( this );
        return;
    }

    if (gpGlobals->time >= m_flExpireTime)
    {
        UTIL_Remove( this );
        return;
    }

    if (gpGlobals->time >= m_flNextFxTime)
    {
        EmitFlameFx();
        m_flNextFxTime = gpGlobals->time + NAPALM_FX_INTERVAL;
    }

    if (gpGlobals->time >= m_flNextDamageTime)
    {
        CBaseEntity *pTarget = NULL;

        while ((pTarget = UTIL_FindEntityInSphere( pTarget, pev->origin, m_flRadius )) != NULL)
        {
            if (pTarget->edict() == edict())
                continue;

            if (!pTarget->pev->takedamage || pTarget->pev->takedamage == DAMAGE_NO)
                continue;

            if (pTarget->pev->health <= 0)
                continue;

            if (!pTarget->IsPlayer() && !(pTarget->pev->flags & FL_MONSTER))
                continue;

            TraceResult tr;
            UTIL_TraceLine( pev->origin + m_vecSurfaceNormal * 2, pTarget->Center(),
                dont_ignore_monsters, ENT( pev ), &tr );

            if (tr.flFraction < 1.0f && tr.pHit != pTarget->edict())
                continue;

            entvars_t *pevAttacker = pev;
            if (m_hNapalmOwner)
                pevAttacker = m_hNapalmOwner->pev;
            else if (pev->owner)
                pevAttacker = VARS( pev->owner );

            pTarget->TakeDamage( pev, pevAttacker, m_flDamagePerTick, DMG_BURN | DMG_NEVERGIB );

            if (m_hNapalmOwner)
            {
                pTarget->m_hFlameOwner = m_hNapalmOwner;
                pTarget->m_fBurnTime += 0.2f;
            }
        }

        m_flNextDamageTime = gpGlobals->time + NAPALM_DAMAGE_INTERVAL;
    }

    pev->nextthink = gpGlobals->time + 0.05f;
}
