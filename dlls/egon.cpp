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
#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "effects.h"
#include "customentity.h"
#include "gamerules.h"
#include "shake.h"
#include "game.h"

#define	EGON_PRIMARY_VOLUME		450
#define EGON_BEAM_SPRITE		"sprites/xbeam1.spr"
#define EGON_FLARE_SPRITE		"sprites/XSpark1.spr"
#define EGON_SOUND_OFF			"weapons/egon_off1.wav"
#define EGON_SOUND_RUN			"weapons/egon_run3.wav"
#define EGON_SOUND_STARTUP		"weapons/egon_windup2.wav"
#define EGON_NOVA_BALL_SPRITE	"sprites/nhth1.spr"

#define EGON_NOVA_AMMO_COST		50
#define EGON_NOVA_CHARGE_TIME		3.0f
#define EGON_NOVA_LIFETIME		5.0f
#define EGON_NOVA_SPEED			350.0f
#define EGON_NOVA_AURA_RADIUS		280.0f
#define EGON_NOVA_AURA_INTERVAL	0.25f
#define EGON_NOVA_AURA_DAMAGE		24.0f
#define EGON_NOVA_BLAST_RADIUS		380.0f
#define EGON_NOVA_BLAST_DAMAGE		320.0f

#define EGON_SWITCH_NARROW_TIME			0.75			// Time it takes to switch fire modes
#define EGON_SWITCH_WIDE_TIME			1.5

enum egon_e {
	EGON_IDLE1 = 0,
	EGON_FIDGET1,
	EGON_ALTFIREON,
	EGON_ALTFIRECYCLE,
	EGON_ALTFIREOFF,
	EGON_FIRE1,
	EGON_FIRE2,
	EGON_FIRE3,
	EGON_FIRE4,
	EGON_DRAW_LOWKEY,
	EGON_DRAW,
	EGON_HOLSTER
};

#ifdef EGON
LINK_ENTITY_TO_CLASS( weapon_egon, CEgon );
#endif

#ifndef CLIENT_DLL
class CEgonNovaBall : public CBaseEntity
{
public:
	static CEgonNovaBall *CreateNovaBall( const Vector &vecOrigin, const Vector &vecVelocity, CBaseEntity *pOwner );

	void Spawn( void );
	void Precache( void );

	void EXPORT FlyThink( void );
	void EXPORT NovaTouch( CBaseEntity *pOther );

	void EmitAuraBeam( const Vector &vecTarget );
	void DealAuraDamage( void );
	void DoSupernova( void );

	BOOL IsEnemyLivingTarget( CBaseEntity *pTarget );
	BOOL IsBreakableTarget( CBaseEntity *pTarget );

private:
	EHANDLE m_hOwner;
	float m_flDieTime;
	float m_flNextAuraTick;
	int m_iTrailSprite;
	int m_iIceTrailSprite;
	int m_iNovaSprite;
	int m_iIceNovaSprite;
};

LINK_ENTITY_TO_CLASS( egon_nova_ball, CEgonNovaBall );
#endif

void CEgon::Spawn( )
{
	Precache( );
	m_iId = WEAPON_EGON;
	SET_MODEL(ENT(pev), "models/w_weapons.mdl");
	pev->body = WEAPON_EGON - 1;

	m_iDefaultAmmo = EGON_DEFAULT_GIVE;
	pev->dmg = gSkillData.plrDmgEgonWide;

	FallInit();// get ready to fall down.
}


void CEgon::Precache( void )
{
	PRECACHE_MODEL("models/v_egon.mdl");

	PRECACHE_MODEL("models/w_9mmclip.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");

	PRECACHE_SOUND( EGON_SOUND_OFF );
	PRECACHE_SOUND( EGON_SOUND_RUN );
	PRECACHE_SOUND( EGON_SOUND_STARTUP );

	PRECACHE_MODEL( EGON_BEAM_SPRITE );
	PRECACHE_MODEL( EGON_FLARE_SPRITE );

	PRECACHE_SOUND ("weapons/357_cock1.wav");

	m_usEgonFire = PRECACHE_EVENT ( 1, "events/egon_fire.sc" );
	m_usEgonStop = PRECACHE_EVENT ( 1, "events/egon_stop.sc" );

#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "egon_nova_ball" );
#endif
}

BOOL CEgon::DeployLowKey( void )
{
	m_deployed = FALSE;
	m_fireState = FIRE_OFF;
	m_fInAttack = 0;
	m_fireMode = FIRE_WIDE;
	return DefaultDeploy( "models/v_egon.mdl", "models/p_weapons.mdl", EGON_DRAW_LOWKEY, "egon" );
}

BOOL CEgon::Deploy( void )
{
	m_deployed = FALSE;
	m_fireState = FIRE_OFF;
	m_fInAttack = 0;
	m_fireMode = FIRE_WIDE;
	return DefaultDeploy( "models/v_egon.mdl", "models/p_weapons.mdl", EGON_DRAW, "egon" );
}

int CEgon::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		WeaponPickup(pPlayer, m_iId);
		return TRUE;
	}
	return FALSE;
}



void CEgon::Holster( int skiplocal /* = 0 */ )
{
	CancelNovaCharge( FALSE );
	EndAttack();
	CBasePlayerWeapon::DefaultHolster(EGON_HOLSTER);
}

int CEgon::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "uranium";
	p->iMaxAmmo1 = URANIUM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_EGON;
	p->iFlags = 0;
	p->iWeight = EGON_WEIGHT;
	p->pszDisplayName = "Egon Spengler's Egon";

	return 1;
}

#define EGON_PULSE_INTERVAL			0.1
#define EGON_DISCHARGE_INTERVAL		0.1

float CEgon::GetPulseInterval( void )
{
	return EGON_PULSE_INTERVAL;
}

float CEgon::GetDischargeInterval( void )
{
	return EGON_DISCHARGE_INTERVAL;
}

BOOL CEgon::HasAmmo( void )
{
	if ( m_pPlayer->ammo_uranium <= 0 )
		return FALSE;

	return TRUE;
}

void CEgon::UseAmmo( int count )
{
#ifndef CLIENT_DLL
	if ( g_pGameRules->IsBusters() )
		return;
#endif

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= count )
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= count;
	else
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = 0;
}

void CEgon::Attack( void )
{
	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		
		if ( m_fireState != FIRE_OFF || m_pBeam )
		{
			EndAttack();
		}
		else
		{
			PlayEmptySound( );
		}
		return;
	}

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	switch( m_fireState )
	{
		case FIRE_OFF:
		{
			if ( !HasAmmo() )
			{
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;
				PlayEmptySound( );
				return;
			}

			m_flAmmoUseTime = gpGlobals->time;// start using ammo ASAP.

			PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usEgonFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, m_fireState, m_fireMode, 1, 0 );
						
			m_shakeTime = 0;

			m_pPlayer->m_iWeaponVolume = EGON_PRIMARY_VOLUME;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;
			pev->fuser1	= UTIL_WeaponTimeBase() + 2;

			pev->dmgtime = gpGlobals->time + GetPulseInterval();
			m_fireState = FIRE_CHARGE;
		}
		break;

		case FIRE_CHARGE:
		{
			Fire( vecSrc, vecAiming );
			m_pPlayer->m_iWeaponVolume = EGON_PRIMARY_VOLUME;
		
			if ( pev->fuser1 <= UTIL_WeaponTimeBase() )
			{
				PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usEgonFire, 0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, m_fireState, m_fireMode, 0, 0 );
				pev->fuser1 = 1000;
			}

			if ( !HasAmmo() )
			{
				EndAttack();
				m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.0;
			}

		}
		break;
	}
}

void CEgon::PrimaryAttack( void )
{
	if ( m_fInAttack == 1 )
	{
		if ( m_pPlayer->pev->button & IN_RELOAD )
			Reload();
		else
			CancelNovaCharge();
		return;
	}

	Attack();
}

void CEgon::SecondaryAttack( void )
{
	if ( m_fInAttack == 1 )
	{
		if ( m_pPlayer->pev->button & IN_RELOAD )
			Reload();
		else
			CancelNovaCharge();
		return;
	}

	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_fireMode == FIRE_WIDE)
		m_fireMode = FIRE_NARROW;
	else
		m_fireMode = FIRE_WIDE;

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.5;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.5;

	SendWeaponAnim(EGON_FIDGET1);
}

void CEgon::Reload( void )
{
	if ( m_fInAttack != 1 && ( m_flNextPrimaryAttack > UTIL_WeaponTimeBase() || m_flNextSecondaryAttack > UTIL_WeaponTimeBase() ) )
		return;

	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_fInAttack == 1 )
			CancelNovaCharge();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < EGON_NOVA_AMMO_COST )
	{
		if ( m_fInAttack == 1 )
			CancelNovaCharge( FALSE );
		PlayEmptySound();
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25f;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.3f;
		return;
	}

	if ( m_fInAttack != 1 )
	{
		if ( m_fireState != FIRE_OFF || m_pBeam )
			EndAttack();

		m_fInAttack = 1;
		m_pPlayer->m_flStartCharge = gpGlobals->time;
		m_shootTime = gpGlobals->time;
		m_shakeTime = gpGlobals->time;
		SendWeaponAnim( EGON_ALTFIREON, 0 );
		m_pPlayer->m_iWeaponVolume = EGON_PRIMARY_VOLUME;
		EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_STATIC, EGON_SOUND_STARTUP, 0.95, ATTN_NORM, 0, 90 );
	}

	float flChargeFrac = ( gpGlobals->time - m_pPlayer->m_flStartCharge ) / EGON_NOVA_CHARGE_TIME;
	if ( flChargeFrac < 0.0f )
		flChargeFrac = 0.0f;
	else if ( flChargeFrac > 1.0f )
		flChargeFrac = 1.0f;

	if ( gpGlobals->time >= m_shootTime )
	{
		int iPitch = 90 + (int)( flChargeFrac * 95.0f );
		if ( iPitch > 185 )
			iPitch = 185;
		EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_STATIC, EGON_SOUND_STARTUP, 0.95, ATTN_NORM, SND_CHANGE_PITCH, iPitch );
		m_shootTime = gpGlobals->time + 0.08f;
	}

	if ( gpGlobals->time >= m_shakeTime )
	{
		SendWeaponAnim( EGON_FIRE1 + RANDOM_LONG( 0, 3 ), 0 );
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		m_pPlayer->pev->punchangle = m_pPlayer->pev->punchangle + Vector( RANDOM_FLOAT( -0.4f, 0.4f ), RANDOM_FLOAT( -0.8f, 0.8f ), 0 );
		m_shakeTime = gpGlobals->time + 0.42f;
	}

	if ( flChargeFrac >= 1.0f )
	{
		FireNovaShot();
		return;
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.05f;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1f;
}

void CEgon::CancelNovaCharge( BOOL bPlayStopSound )
{
	if ( m_fInAttack != 1 )
		return;

	m_fInAttack = 0;
	STOP_SOUND( ENT(m_pPlayer->pev), CHAN_STATIC, EGON_SOUND_STARTUP );

	if ( bPlayStopSound )
		EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, EGON_SOUND_OFF, 0.8, ATTN_NORM, 0, 110 );

	SendWeaponAnim( EGON_ALTFIREOFF, 0 );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.3f;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5f;
}

void CEgon::FireNovaShot( void )
{
	if ( m_fInAttack != 1 )
		return;

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < EGON_NOVA_AMMO_COST )
	{
		CancelNovaCharge( FALSE );
		PlayEmptySound();
		return;
	}

	m_fInAttack = 0;
	STOP_SOUND( ENT(m_pPlayer->pev), CHAN_STATIC, EGON_SOUND_STARTUP );
	UseAmmo( EGON_NOVA_AMMO_COST );

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );
	Vector vecForward = gpGlobals->v_forward;
	Vector vecSource = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + vecForward * 16 + gpGlobals->v_right * 6 - gpGlobals->v_up * 6;
	Vector vecVelocity = vecForward * EGON_NOVA_SPEED + m_pPlayer->pev->velocity * 0.35f;

#ifndef CLIENT_DLL
	CEgonNovaBall::CreateNovaBall( vecSource, vecVelocity, m_pPlayer );
#endif

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->punchangle = Vector( -4, -2, 0 );
	m_pPlayer->m_iWeaponVolume = EGON_PRIMARY_VOLUME;
	EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, EGON_SOUND_RUN, 1.0, ATTN_NORM, 0, 150 );
	SendWeaponAnim( EGON_FIRE1 + RANDOM_LONG( 0, 3 ), 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.0f;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0f;
}

void CEgon::Fire( const Vector &vecOrigSrc, const Vector &vecDir )
{
	Vector vecDest = vecOrigSrc + vecDir * 2048;
	edict_t		*pentIgnore;
	TraceResult tr;

	pentIgnore = m_pPlayer->edict();
	Vector tmpSrc = vecOrigSrc + gpGlobals->v_up * -8 + gpGlobals->v_right * 3;

	// ALERT( at_console, "." );
	
	UTIL_TraceLine( vecOrigSrc, vecDest, dont_ignore_monsters, pentIgnore, &tr );

	if (tr.fAllSolid)
		return;

#ifndef CLIENT_DLL
	CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

	if (pEntity == NULL)
		return;

	if ( g_pGameRules->IsMultiplayer() )
	{
		if ( m_pSprite && pEntity->pev->takedamage )
		{
			m_pSprite->pev->effects &= ~EF_NODRAW;
		}
		else if ( m_pSprite )
		{
			m_pSprite->pev->effects |= EF_NODRAW;
		}
	}


#endif

	float timedist;

	switch ( m_fireMode )
	{
	case FIRE_NARROW:
#ifndef CLIENT_DLL
		if ( pev->dmgtime < gpGlobals->time )
		{
			// Narrow mode only does damage to the entity it hits
			ClearMultiDamage();
			if (pEntity->pev->takedamage)
			{
				pEntity->TraceAttack( m_pPlayer->pev, gSkillData.plrDmgEgonNarrow, vecDir, &tr, DMG_ENERGYBEAM );
			}
			ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// multiplayer uses 1 ammo every 1/10th second
				if ( gpGlobals->time >= m_flAmmoUseTime )
				{
					UseAmmo( 1 );
					m_flAmmoUseTime = gpGlobals->time + 0.1;
				}
			}
			else
			{
				// single player, use 3 ammo/second
				if ( gpGlobals->time >= m_flAmmoUseTime )
				{
					UseAmmo( 1 );
					m_flAmmoUseTime = gpGlobals->time + 0.166;
				}
			}

			pev->dmgtime = gpGlobals->time + GetPulseInterval();
		}
#endif
		timedist = ( pev->dmgtime - gpGlobals->time ) / GetPulseInterval();
		break;
	
	case FIRE_WIDE:
#ifndef CLIENT_DLL
		if ( pev->dmgtime < gpGlobals->time )
		{
			// wide mode does damage to the ent, and radius damage
			ClearMultiDamage();
			if (pEntity->pev->takedamage)
			{
				pEntity->TraceAttack( m_pPlayer->pev, gSkillData.plrDmgEgonWide, vecDir, &tr, DMG_ENERGYBEAM | DMG_ALWAYSGIB);
			}
			ApplyMultiDamage(m_pPlayer->pev, m_pPlayer->pev);

			if ( g_pGameRules->IsMultiplayer() )
			{
				// radius damage a little more potent in multiplayer.
				::RadiusDamage( tr.vecEndPos, pev, m_pPlayer->pev, gSkillData.plrDmgEgonWide/4, 128, CLASS_NONE, DMG_ENERGYBEAM | DMG_BLAST | DMG_ALWAYSGIB );
			}

			if ( !m_pPlayer->IsAlive() )
				return;

			if ( g_pGameRules->IsMultiplayer() )
			{
				//multiplayer uses 5 ammo/second
				if ( gpGlobals->time >= m_flAmmoUseTime )
				{
					UseAmmo( 1 );
					m_flAmmoUseTime = gpGlobals->time + 0.2;
				}
			}
			else
			{
				// Wide mode uses 10 charges per second in single player
				if ( gpGlobals->time >= m_flAmmoUseTime )
				{
					UseAmmo( 1 );
					m_flAmmoUseTime = gpGlobals->time + 0.1;
				}
			}

			pev->dmgtime = gpGlobals->time + GetDischargeInterval();
			if ( m_shakeTime < gpGlobals->time )
			{
				UTIL_ScreenShake( tr.vecEndPos, 5.0, 150.0, 0.75, 250.0 );
				m_shakeTime = gpGlobals->time + 1.5;
			}
		}
#endif
		timedist = ( pev->dmgtime - gpGlobals->time ) / GetDischargeInterval();
		break;
	}

	if ( timedist < 0 )
		timedist = 0;
	else if ( timedist > 1 )
		timedist = 1;
	timedist = 1-timedist;

	UpdateEffect( tmpSrc, tr.vecEndPos, timedist );
}


void CEgon::UpdateEffect( const Vector &startPoint, const Vector &endPoint, float timeBlend )
{
#ifndef CLIENT_DLL
	if ( !m_pBeam )
	{
		CreateEffect();
	}

	m_pBeam->SetStartPos( endPoint );
	m_pBeam->SetBrightness( 255 - (timeBlend*180) );
	m_pBeam->SetWidth( 40 - (timeBlend*20) );

	if ( m_fireMode == FIRE_WIDE )
		m_pBeam->SetColor( 30 + (25*timeBlend), 30 + (30*timeBlend), 64 + 80*fabs(sin(gpGlobals->time*10)) );
	else
		m_pBeam->SetColor( 60 + (25*timeBlend), 120 + (30*timeBlend), 64 + 80*fabs(sin(gpGlobals->time*10)) );


	UTIL_SetOrigin( m_pSprite->pev, endPoint );
	m_pSprite->pev->frame += 8 * gpGlobals->frametime;
	if ( m_pSprite->pev->frame > m_pSprite->Frames() )
		m_pSprite->pev->frame = 0;

	m_pNoise->SetStartPos( endPoint );

#endif

}

void CEgon::CreateEffect( void )
{

#ifndef CLIENT_DLL
	DestroyEffect();

	m_pBeam = CBeam::BeamCreate( EGON_BEAM_SPRITE, 40 );
	m_pBeam->PointEntInit( pev->origin, m_pPlayer->entindex() );
	m_pBeam->SetFlags( BEAM_FSINE );
	m_pBeam->SetEndAttachment( 1 );
	m_pBeam->pev->spawnflags |= SF_BEAM_TEMPORARY;	// Flag these to be destroyed on save/restore or level transition
	m_pBeam->pev->flags |= FL_SKIPLOCALHOST;
	m_pBeam->pev->owner = m_pPlayer->edict();

	m_pNoise = CBeam::BeamCreate( EGON_BEAM_SPRITE, 55 );
	m_pNoise->PointEntInit( pev->origin, m_pPlayer->entindex() );
	m_pNoise->SetScrollRate( 25 );
	m_pNoise->SetBrightness( 100 );
	m_pNoise->SetEndAttachment( 1 );
	m_pNoise->pev->spawnflags |= SF_BEAM_TEMPORARY;
	m_pNoise->pev->flags |= FL_SKIPLOCALHOST;
	m_pNoise->pev->owner = m_pPlayer->edict();

	m_pSprite = CSprite::SpriteCreate( EGON_FLARE_SPRITE, pev->origin, FALSE );
	if (m_pSprite != NULL)
	{
		m_pSprite->pev->scale = 1.0;
		m_pSprite->SetTransparency( kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation );
		m_pSprite->pev->spawnflags |= SF_SPRITE_TEMPORARY;
		m_pSprite->pev->flags |= FL_SKIPLOCALHOST;
		m_pSprite->pev->owner = m_pPlayer->edict();
	}

	if ( m_fireMode == FIRE_WIDE )
	{
		m_pBeam->SetScrollRate( 50 );
		m_pBeam->SetNoise( 20 );
		m_pNoise->SetColor( 50, 50, 255 );
		m_pNoise->SetNoise( 8 );
	}
	else
	{
		m_pBeam->SetScrollRate( 110 );
		m_pBeam->SetNoise( 5 );
		m_pNoise->SetColor( 80, 120, 255 );
		m_pNoise->SetNoise( 2 );
	}
#endif

}


void CEgon::DestroyEffect( void )
{

#ifndef CLIENT_DLL
	if ( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}
	if ( m_pNoise )
	{
		UTIL_Remove( m_pNoise );
		m_pNoise = NULL;
	}
	if ( m_pSprite )
	{
		if ( m_fireMode == FIRE_WIDE )
			m_pSprite->Expand( 10, 500 );
		else
			UTIL_Remove( m_pSprite );
		m_pSprite = NULL;
	}
#endif

}



void CEgon::WeaponIdle( void )
{
	if ( m_fInAttack == 1 )
	{
		CancelNovaCharge();
		return;
	}

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	ResetEmptySound( );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_fireState != FIRE_OFF )
		 EndAttack();
	
	int iAnim;

	float flRand = RANDOM_FLOAT(0,1);

	if ( flRand <= 0.5 )
	{
		iAnim = EGON_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	}
	else 
	{
		iAnim = EGON_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3;
	}

	SendWeaponAnim( iAnim );
	m_deployed = TRUE;
}

BOOL CEgon::CanHolster( void )
{
#ifndef CLIENT_DLL
	if ( g_pGameRules->IsBusters() )
	{
		return FALSE;
	}
#endif

	return TRUE;
}

void CEgon::EndAttack( void )
{
	if ( m_fInAttack == 1 )
		CancelNovaCharge( FALSE );

	bool bMakeNoise = false;
		
	if ( m_fireState != FIRE_OFF ) //Checking the button just in case!.
		 bMakeNoise = true;

	PLAYBACK_EVENT_FULL( FEV_GLOBAL | FEV_RELIABLE, m_pPlayer->edict(), m_usEgonStop, 0, (float *)&m_pPlayer->pev->origin, (float *)&m_pPlayer->pev->angles, 0.0, 0.0, bMakeNoise, 0, 0, 0 );

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 2.0;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;

	m_fireState = FIRE_OFF;

	DestroyEffect();
}


#ifndef CLIENT_DLL
CEgonNovaBall *CEgonNovaBall::CreateNovaBall( const Vector &vecOrigin, const Vector &vecVelocity, CBaseEntity *pOwner )
{
	if ( !pOwner )
		return NULL;

	CEgonNovaBall *pBall = (CEgonNovaBall *)CBaseEntity::Create( "egon_nova_ball", vecOrigin, UTIL_VecToAngles( vecVelocity ), pOwner->edict() );
	if ( !pBall )
		return NULL;

	pBall->pev->velocity = vecVelocity;
	pBall->m_hOwner = pOwner;
	pBall->pev->owner = pOwner->edict();
	return pBall;
}

void CEgonNovaBall::Precache( void )
{
	PRECACHE_MODEL( EGON_NOVA_BALL_SPRITE );
	m_iTrailSprite = PRECACHE_MODEL( EGON_BEAM_SPRITE );
	m_iIceTrailSprite = PRECACHE_MODEL( "sprites/ice_plasmatrail.spr" );
	m_iNovaSprite = PRECACHE_MODEL( "sprites/nuke2.spr" );
	m_iIceNovaSprite = PRECACHE_MODEL( "sprites/ice_nuke2.spr" );
	PRECACHE_SOUND( "nuke_explosion.wav" );
	PRECACHE_SOUND( "weapons/electro4.wav" );
}

void CEgonNovaBall::Spawn( void )
{
	Precache();

	pev->classname = MAKE_STRING( "egon_nova_ball" );
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->gravity = 0;
	pev->friction = 1.0f;
	pev->dmg = EGON_NOVA_BLAST_DAMAGE;
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->scale = 2.0f;
	pev->frame = 0;
	pev->framerate = 12.0f;

	SET_MODEL( ENT(pev), EGON_NOVA_BALL_SPRITE );
	UTIL_SetSize( pev, Vector( -6, -6, -6 ), Vector( 6, 6, 6 ) );
	UTIL_SetOrigin( pev, pev->origin );

	m_flDieTime = gpGlobals->time + EGON_NOVA_LIFETIME;
	m_flNextAuraTick = gpGlobals->time + EGON_NOVA_AURA_INTERVAL;

	SetTouch( &CEgonNovaBall::NovaTouch );
	SetThink( &CEgonNovaBall::FlyThink );
	pev->nextthink = gpGlobals->time + 0.05f;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex() );
		if ( icesprites.value )
			WRITE_SHORT( m_iIceTrailSprite );
		else
			WRITE_SHORT( m_iTrailSprite );
		WRITE_BYTE( 8 );
		WRITE_BYTE( 10 );
		if ( icesprites.value )
		{
			WRITE_BYTE( 0 );
			WRITE_BYTE( 113 );
			WRITE_BYTE( 230 );
		}
		else
		{
			WRITE_BYTE( 80 );
			WRITE_BYTE( 180 );
			WRITE_BYTE( 255 );
		}
		WRITE_BYTE( 220 );
	MESSAGE_END();
}

void CEgonNovaBall::NovaTouch( CBaseEntity *pOther )
{
	if ( pOther && pOther->edict() == pev->owner )
		return;

	DoSupernova();
}

void CEgonNovaBall::FlyThink( void )
{
	if ( pev->waterlevel == 3 || UTIL_PointContents( pev->origin ) == CONTENT_WATER )
	{
		DoSupernova();
		return;
	}

	if ( gpGlobals->time >= m_flNextAuraTick )
	{
		DealAuraDamage();
		m_flNextAuraTick = gpGlobals->time + EGON_NOVA_AURA_INTERVAL;
	}

	if ( gpGlobals->time >= m_flDieTime )
	{
		DoSupernova();
		return;
	}

	pev->frame += pev->framerate * 0.05f;
	if ( pev->frame > 10 )
		pev->frame = 0;

	pev->nextthink = gpGlobals->time + 0.05f;
}

BOOL CEgonNovaBall::IsEnemyLivingTarget( CBaseEntity *pTarget )
{
	if ( !pTarget || !pTarget->pev || pTarget->pev->takedamage == DAMAGE_NO || pTarget->pev->health <= 0 )
		return FALSE;

	if ( !pTarget->IsPlayer() && !(pTarget->pev->flags & FL_MONSTER) )
		return FALSE;

	CBaseEntity *pOwner = m_hOwner;
	if ( pOwner )
	{
		if ( pTarget->edict() == pOwner->edict() )
			return FALSE;

		if ( g_pGameRules->PlayerRelationship( pOwner, pTarget ) == GR_TEAMMATE )
			return FALSE;
	}

	return TRUE;
}

BOOL CEgonNovaBall::IsBreakableTarget( CBaseEntity *pTarget )
{
	if ( !pTarget || !pTarget->pev || pTarget->pev->takedamage == DAMAGE_NO )
		return FALSE;

	return FClassnameIs( pTarget->pev, "func_breakable" ) ||
		FClassnameIs( pTarget->pev, "func_pushable" ) ||
		FClassnameIs( pTarget->pev, "monster_barrel" );
}

void CEgonNovaBall::EmitAuraBeam( const Vector &vecTarget )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMPOINTS );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z );
		WRITE_COORD( vecTarget.x );
		WRITE_COORD( vecTarget.y );
		WRITE_COORD( vecTarget.z );
		WRITE_SHORT( g_sModelLightning );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 12 );
		WRITE_BYTE( 3 );
		WRITE_BYTE( 20 );
		WRITE_BYTE( 25 );
		if ( icesprites.value )
		{
			WRITE_BYTE( 128 );
			WRITE_BYTE( 200 );
			WRITE_BYTE( 255 );
		}
		else
		{
			WRITE_BYTE( 80 );
			WRITE_BYTE( 200 );
			WRITE_BYTE( 255 );
		}
		WRITE_BYTE( 220 );
		WRITE_BYTE( 25 );
	MESSAGE_END();
}

void CEgonNovaBall::DealAuraDamage( void )
{
	CBaseEntity *pTarget = NULL;
	CBaseEntity *pOwner = m_hOwner;
	entvars_t *pevAttacker = pOwner ? pOwner->pev : pev;

	while ( ( pTarget = UTIL_FindEntityInSphere( pTarget, pev->origin, EGON_NOVA_AURA_RADIUS ) ) != NULL )
	{
		if ( !IsEnemyLivingTarget( pTarget ) )
			continue;

		TraceResult tr;
		UTIL_TraceLine( pev->origin, pTarget->Center(), dont_ignore_monsters, ENT(pev), &tr );
		if ( tr.flFraction < 1.0f && tr.pHit != pTarget->edict() )
			continue;

		pTarget->TakeDamage( pev, pevAttacker, EGON_NOVA_AURA_DAMAGE, DMG_ENERGYBEAM | DMG_NEVERGIB );
		EmitAuraBeam( pTarget->Center() );
	}
}

void CEgonNovaBall::DoSupernova( void )
{
	if ( pev->solid == SOLID_NOT )
		return;

	SetTouch( NULL );
	SetThink( NULL );

	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;

	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "nuke_explosion.wav", 0.85f, ATTN_NORM, 0, PITCH_NORM );
	UTIL_ScreenShake( pev->origin, 20.0f, 180.0f, 1.2f, 400.0f );

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMDISK );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 16 );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + EGON_NOVA_BLAST_RADIUS * 2.0f );
		WRITE_SHORT( g_sModelLightning );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 16 );
		WRITE_BYTE( 60 );
		WRITE_BYTE( 64 );
		if ( icesprites.value )
		{
			WRITE_BYTE( 0 );
			WRITE_BYTE( 113 );
			WRITE_BYTE( 230 );
		}
		else
		{
			WRITE_BYTE( 120 );
			WRITE_BYTE( 200 );
			WRITE_BYTE( 255 );
		}
		WRITE_BYTE( 255 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 16 );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + EGON_NOVA_BLAST_RADIUS * 2.0f );
		WRITE_SHORT( g_sModelLightning );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 16 );
		WRITE_BYTE( 60 );
		WRITE_BYTE( 64 );
		if ( icesprites.value )
		{
			WRITE_BYTE( 0 );
			WRITE_BYTE( 113 );
			WRITE_BYTE( 230 );
		}
		else
		{
			WRITE_BYTE( 120 );
			WRITE_BYTE( 200 );
			WRITE_BYTE( 255 );
		}
		WRITE_BYTE( 255 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_EXPLOSION );
		WRITE_COORD( pev->origin.x );
		WRITE_COORD( pev->origin.y );
		WRITE_COORD( pev->origin.z + 128 );
		if ( icesprites.value )
			WRITE_SHORT( m_iIceNovaSprite );
		else
			WRITE_SHORT( m_iNovaSprite );
		WRITE_BYTE( 32 );
		WRITE_BYTE( 24 );
		WRITE_BYTE( TE_EXPLFLAG_NONE );
	MESSAGE_END();

	CBaseEntity *pTarget = NULL;
	CBaseEntity *pOwner = m_hOwner;
	entvars_t *pevAttacker = pOwner ? pOwner->pev : pev;

	while ( ( pTarget = UTIL_FindEntityInSphere( pTarget, pev->origin, EGON_NOVA_BLAST_RADIUS ) ) != NULL )
	{
		if ( pTarget == this || !pTarget->pev || pTarget->pev->takedamage == DAMAGE_NO )
			continue;

		const BOOL bLiving = ( pTarget->IsPlayer() || ( pTarget->pev->flags & FL_MONSTER ) );
		if ( bLiving )
		{
			if ( !IsEnemyLivingTarget( pTarget ) )
				continue;
		}
		else if ( !IsBreakableTarget( pTarget ) )
		{
			continue;
		}

		TraceResult tr;
		UTIL_TraceLine( pev->origin, pTarget->Center(), dont_ignore_monsters, ENT(pev), &tr );
		if ( tr.flFraction < 1.0f && tr.pHit != pTarget->edict() )
			continue;

		float flDist = ( pTarget->Center() - pev->origin ).Length();
		float flScale = 1.0f - ( flDist / EGON_NOVA_BLAST_RADIUS );
		if ( flScale <= 0.0f )
			continue;

		float flDamage = EGON_NOVA_BLAST_DAMAGE * flScale;
		if ( flDamage < 1.0f )
			flDamage = 1.0f;

		pTarget->TakeDamage( pev, pevAttacker, flDamage, DMG_ENERGYBEAM | DMG_BLAST | DMG_ALWAYSGIB );
	}

	UTIL_Remove( this );
}
#endif



class CEgonAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_chainammo.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_chainammo.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_URANIUMBOX_GIVE, "uranium", URANIUM_MAX_CARRY ) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_egonclip, CEgonAmmo );

#endif