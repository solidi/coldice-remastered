#if defined( GRAPPLING_HOOK )

#include "extdll.h"
#include "util.h"

#include "cbase.h"
#include "weapons.h"
#include "gamerules.h"
#include "player.h"
#include "monsters.h"

#include "grapplinghook.h"

//================================================================
// Cold Ice Grapple Hook
// Also see: https://developer.valvesoftware.com/wiki/Grapple_Hook
//================================================================

LINK_ENTITY_TO_CLASS( grapple_hook, CHook );

static const float HOOK_FATAL_PULL_TIMEOUT = 2.5f;
static const float HOOK_FATAL_PULL_SPEED_BASE = 550.0f;
static const float HOOK_FATAL_PULL_SPEED_MAX = 1400.0f;
static const float HOOK_FATAL_PULL_FINISH_DIST = 48.0f;
static const float HOOK_FATAL_UPPERCUT_LAUNCH_FWD = 180.0f;
static const float HOOK_FATAL_UPPERCUT_LAUNCH_Z = 900.0f;

CHook *CHook::HookCreate( CBasePlayer *owner )
{
	CHook *pHook = GetClassPtr( (CHook *)NULL );
	pHook->pevOwner = owner;
	pHook->pev->classname = MAKE_STRING("hook");
	pHook->m_fActiveHook = FALSE;
	pHook->m_fHookInWall = FALSE;
	pHook->m_fPlayerAtEnd = FALSE;
	pHook->m_hFatalVictim = NULL;
	pHook->m_fFatalPull = FALSE;
	pHook->m_fVictimPhysicsCaptured = FALSE;
	pHook->m_iVictimMoveType = MOVETYPE_WALK;
	pHook->m_iVictimSolid = SOLID_SLIDEBOX;
	pHook->m_flVictimGravity = 1;
	pHook->m_flVictimFriction = 1;
	pHook->m_flFatalAbortTime = 0;
	return pHook;
}

void CHook::Spawn( )
{
	Precache( );
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;
	pev->gravity = -1;

	SET_MODEL(ENT(pev), "models/w_items.mdl");
	pev->body = 7;
	pev->sequence = 8;
	UTIL_SetOrigin( pev, pev->origin );
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));
}

void CHook::Precache( )
{
	PRECACHE_SOUND("weapons/xbow_hitbod1.wav");
	PRECACHE_SOUND("grapple_hit.wav");
	PRECACHE_SOUND("get_over_here.wav");
	PRECACHE_SOUND("fists_hitbod.wav");
	PRECACHE_SOUND("fists_shoryuken.wav");

	PRECACHE_SOUND("weapons/xbow_fly1.wav");
	PRECACHE_SOUND("weapons/xbow_hit1.wav");
	PRECACHE_SOUND("grapple_deploy.wav");

	ropesprite = g_sModelIndexSmoke2;
}

int CHook :: Classify ( void )
{
	return  CLASS_NONE;
}

void CHook::RestoreFatalVictimPhysics( void )
{
	if ( !m_hFatalVictim || !m_fVictimPhysicsCaptured )
	{
		m_hFatalVictim = NULL;
		m_fFatalPull = FALSE;
		m_fVictimPhysicsCaptured = FALSE;
		m_flFatalAbortTime = 0;
		return;
	}

	CBaseEntity *pVictimEnt = m_hFatalVictim;
	if ( pVictimEnt && pVictimEnt->IsPlayer() )
	{
		CBasePlayer *pVictim = (CBasePlayer *)pVictimEnt;
		pVictim->pev->movetype = m_iVictimMoveType;
		pVictim->pev->solid = m_iVictimSolid;
		pVictim->pev->gravity = m_flVictimGravity;
		pVictim->pev->friction = m_flVictimFriction;
	}

	m_hFatalVictim = NULL;
	m_fFatalPull = FALSE;
	m_fVictimPhysicsCaptured = FALSE;
	m_flFatalAbortTime = 0;
}

void CHook::BeginFatalPull( CBasePlayer *pVictim )
{
	if ( !pVictim )
		return;

	if ( !pevOwner || !pevOwner->IsPlayer() )
		return;

	CBaseEntity *pOwnerEnt = pevOwner;
	CBasePlayer *pOwner = (CBasePlayer *)pOwnerEnt;
	if ( !pOwner || !pOwner->IsAlive() || !pVictim->IsAlive() )
		return;

	RestoreFatalVictimPhysics();

	m_hFatalVictim = pVictim;
	m_fFatalPull = TRUE;
	m_flFatalAbortTime = gpGlobals->time + HOOK_FATAL_PULL_TIMEOUT;
	m_iVictimMoveType = pVictim->pev->movetype;
	m_iVictimSolid = pVictim->pev->solid;
	m_flVictimGravity = pVictim->pev->gravity;
	m_flVictimFriction = pVictim->pev->friction;
	m_fVictimPhysicsCaptured = TRUE;

	pVictim->pev->movetype = MOVETYPE_NOCLIP;
	pVictim->pev->solid = SOLID_NOT;
	pVictim->pev->gravity = 0;
	pVictim->pev->friction = 0;
	ClearBits( pVictim->pev->flags, FL_ONGROUND );

	m_fActiveHook = FALSE;
	m_fHookInWall = FALSE;
	m_fPlayerAtEnd = FALSE;

	SetTouch( NULL );
	SetThink( &CHook::Think );
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_FLY;
	UTIL_SetOrigin( pev, pVictim->pev->origin + Vector( 0, 0, 36 ) );
	pev->velocity = g_vecZero;
	pev->nextthink = gpGlobals->time + 0.05;

	EMIT_SOUND_DYN( ENT(pOwner->pev), CHAN_VOICE, "get_over_here.wav", 1, ATTN_NORM, 0, 100 );
	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "grapple_hit.wav", 1, ATTN_NORM );
}

void CHook::DoFatalUppercut( CBasePlayer *pOwner, CBasePlayer *pVictim )
{
	if ( !pOwner || !pVictim )
		return;

	const float flPrevOffhandTime = pOwner->m_fOffhandTime;
	if ( pOwner->m_pActiveItem )
		((CBasePlayerWeapon *)pOwner->m_pActiveItem)->StartPunch( pOwner->m_iHoldingItem );

	BOOL bOffhandPunchStarted = ( pOwner->m_fOffhandTime > flPrevOffhandTime && pOwner->m_fOffhandTime > gpGlobals->time );

	UTIL_MakeVectors( pOwner->pev->v_angle + pOwner->pev->punchangle );
	if ( !bOffhandPunchStarted )
	{
		pOwner->SetAnimation( PLAYER_PUNCH );
		pOwner->pev->punchangle = Vector( -10, 0, 0 );
	}
	pOwner->pev->velocity = pOwner->pev->velocity + Vector( 0, 0, 300 );

	EMIT_SOUND_DYN( ENT(pOwner->pev), CHAN_BODY, "fists_shoryuken.wav", 1, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3) );
	EMIT_SOUND( ENT(pVictim->pev), CHAN_BODY, "fists_hitbod.wav", 1, ATTN_NORM );

	if ( m_fVictimPhysicsCaptured )
	{
		pVictim->pev->movetype = m_iVictimMoveType;
		pVictim->pev->solid = m_iVictimSolid;
		pVictim->pev->gravity = m_flVictimGravity;
		pVictim->pev->friction = m_flVictimFriction;
	}

	pVictim->ExpireSpawnProtection();
	ClearBits( pVictim->pev->flags, FL_ONGROUND );
	pVictim->pev->velocity = gpGlobals->v_forward * HOOK_FATAL_UPPERCUT_LAUNCH_FWD + Vector( 0, 0, HOOK_FATAL_UPPERCUT_LAUNCH_Z );

	const float flFatalDamage = pVictim->pev->health + pVictim->pev->max_health + 200.0f;
	pVictim->TakeDamage( pev, pOwner->pev, flFatalDamage, DMG_PUNCH | DMG_NEVERGIB );
	if ( pVictim->IsAlive() )
		pVictim->TakeDamage( pev, pOwner->pev, 10000.0f, DMG_PUNCH | DMG_NEVERGIB );

	m_hFatalVictim = NULL;
	m_fFatalPull = FALSE;
	m_fVictimPhysicsCaptured = FALSE;
	m_flFatalAbortTime = 0;
}

void CHook::UpdateFatalPull( void )
{
	if ( !m_fFatalPull )
		return;

	CBaseEntity *pOwnerEnt = pevOwner;
	CBaseEntity *pVictimEnt = m_hFatalVictim;

	if ( !pOwnerEnt || !pOwnerEnt->IsPlayer() || !pVictimEnt || !pVictimEnt->IsPlayer() )
	{
		KillHook();
		return;
	}

	CBasePlayer *pOwner = (CBasePlayer *)pOwnerEnt;
	CBasePlayer *pVictim = (CBasePlayer *)pVictimEnt;

	if ( !pOwner->IsAlive() || !pVictim->IsAlive() || pOwner->pev->iuser1 || pVictim->pev->iuser1 )
	{
		KillHook();
		return;
	}

	if ( m_flFatalAbortTime > 0 && gpGlobals->time >= m_flFatalAbortTime )
	{
		KillHook();
		return;
	}

	UTIL_MakeVectors( pOwner->pev->v_angle );
	Vector vecTarget = pOwner->pev->origin + gpGlobals->v_forward * 34 + Vector( 0, 0, 24 );
	Vector vecDelta = vecTarget - pVictim->pev->origin;
	float flDistance = vecDelta.Length();

	pVictim->pev->movetype = MOVETYPE_NOCLIP;
	pVictim->pev->solid = SOLID_NOT;
	pVictim->pev->gravity = 0;
	pVictim->pev->friction = 0;
	ClearBits( pVictim->pev->flags, FL_ONGROUND );

	if ( flDistance > HOOK_FATAL_PULL_FINISH_DIST )
	{
		float flSpeed = HOOK_FATAL_PULL_SPEED_BASE + flDistance * 4.0f;
		if ( flSpeed > HOOK_FATAL_PULL_SPEED_MAX )
			flSpeed = HOOK_FATAL_PULL_SPEED_MAX;

		Vector vecDir = vecDelta.Normalize();
		float flStep = flSpeed * 0.05;
		if ( flStep > flDistance )
			flStep = flDistance;

		UTIL_SetOrigin( pVictim->pev, pVictim->pev->origin + vecDir * flStep );
		pVictim->pev->velocity = vecDir * flSpeed;
	}
	else
	{
		UTIL_SetOrigin( pVictim->pev, vecTarget );
		pVictim->pev->velocity = g_vecZero;
		DoFatalUppercut( pOwner, pVictim );
		KillHook();
		return;
	}

	UTIL_SetOrigin( pev, pVictim->pev->origin + Vector( 0, 0, 36 ) );
}

void CHook::FireHook( ) {
	if ( m_fActiveHook ) {
		return;
	}

	if ( !pevOwner )
		return;

	if ( !pevOwner->IsAlive() )
		return;

	if ( pevOwner->pev->iuser1 )
		return;

	m_hFatalVictim = NULL;
	m_fFatalPull = FALSE;
	m_fVictimPhysicsCaptured = FALSE;
	m_flFatalAbortTime = 0;

	pev->owner = ENT(pevOwner->pev);
	pev->effects &= ~EF_NODRAW;

	TraceResult tr;
	Vector anglesAim = pevOwner->pev->v_angle + pevOwner->pev->punchangle;
	UTIL_MakeVectors( anglesAim );

	if (!grabsky.value)
	{
		edict_t	*pWorld = g_engfuncs.pfnPEntityOfEntIndex(0);
		Vector start = pevOwner->pev->origin + pevOwner->pev->view_ofs;
		Vector end = start + gpGlobals->v_forward * 4096;
		UTIL_TraceLine( start, end, ignore_monsters, edict(), &tr );
		if ( tr.pHit )
			pWorld = tr.pHit;
		const char *pTextureName = TRACE_TEXTURE( pWorld, start, end );
		if (pTextureName && stricmp(pTextureName, "sky") == 0) {
			ClientPrint(pevOwner->pev, HUD_PRINTCENTER, "Cannot grapple sky!\n");
			return;
		}
	}

	EMIT_SOUND_DYN(ENT(pevOwner->pev), CHAN_WEAPON, "grapple_deploy.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 93 + RANDOM_LONG(0,0xF));

	Spawn();

	anglesAim.x = -anglesAim.x;
	Vector vecDir = gpGlobals->v_forward;
	Vector trace_origin = pevOwner->pev->origin + pevOwner->pev->view_ofs;
	if ( pevOwner->pev->flags & FL_DUCKING ) {
		trace_origin = trace_origin - ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
	}
	UTIL_TraceLine( trace_origin + gpGlobals->v_forward * 20, trace_origin + gpGlobals->v_forward * 64, dont_ignore_monsters, NULL, &tr );

	UTIL_SetOrigin(pev, tr.vecEndPos);
	pev->angles = anglesAim;
	pev->velocity = vecDir * gSkillData.plrSpeedHook;
	m_vVecDirHookMove = vecDir;

	m_fActiveHook = TRUE;
	if (pevOwner->IsPlayer())
	{
		CBaseEntity *pOwner = pevOwner;
		((CBasePlayer *)pOwner)->ExpireSpawnProtection();
	}

	SetTouch( &CHook::HookTouch );
	pev->nextthink = gpGlobals->time + 0.2;
}

void CHook::HookTouch( CBaseEntity *pOther )
{
	if (pOther == pevOwner) {
		pev->nextthink = gpGlobals->time + 0.2;
		return;
	}

	if ( pOther && pOther->IsPlayer() && pevOwner && pevOwner->IsPlayer() )
	{
		CBaseEntity *pOwnerEnt = pevOwner;
		CBasePlayer *pOwner = (CBasePlayer *)pOwnerEnt;
		CBasePlayer *pVictim = (CBasePlayer *)pOther;

		if ( pOwner && pVictim && pOwner != pVictim && pOwner->IsAlive() && pVictim->IsAlive() &&
			g_pGameRules->FPlayerCanTakeDamage( pVictim, pOwner ) )
		{
			BeginFatalPull( pVictim );
			return;
		}
	}

	SetTouch( NULL );
	SetThink( NULL );

	if (pOther->pev->takedamage)
	{
		TraceResult tr = UTIL_GetGlobalTrace( );
		ClearMultiDamage();
		pOther->TraceAttack(VARS(pev->owner), gSkillData.plrDmgHook, pev->velocity.Normalize(), &tr, DMG_NEVERGIB );
		ApplyMultiDamage( pev, VARS(pev->owner));
		pev->velocity = Vector( 0, 0, 0 );

		if (pOther->IsPlayer())
		{
			switch( RANDOM_LONG(0,1) )
			{
				case 0:
					EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM);
					break;
				case 1:
					EMIT_SOUND(ENT(pev), CHAN_WEAPON, "grapple_hit.wav", 1, ATTN_NORM);
					break;
			}
		}
		else
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,7));
		}

		if (pevOwner)
		{
			pevOwner->pev->movetype = MOVETYPE_WALK;
			pevOwner->pev->gravity = 1;
		}

		m_fActiveHook = FALSE;
		m_fHookInWall = FALSE;
		m_fPlayerAtEnd = FALSE;

		if ( !g_pGameRules->IsMultiplayer() )
		{
			Killed( pev, GIB_NEVER );
		}

		pev->effects |= EF_NODRAW;
		pev->nextthink = -1;
	}
	else
	{
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "weapons/xbow_hit1.wav", RANDOM_FLOAT(0.95, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG(0,7));

		Vector vecDir = pev->velocity.Normalize( );
		UTIL_SetOrigin( pev, pev->origin - vecDir * 12 );
		pev->angles = UTIL_VecToAngles( vecDir );
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_FLY;
		pev->velocity = Vector( 0, 0, 0 );
		pev->avelocity.z = 0;
		pev->angles.z = RANDOM_LONG(0,360);
		SetThink( &CHook::Think );
		pev->nextthink = gpGlobals->time + 0.1;

		m_fHookInWall = TRUE;
		m_fActiveHook = TRUE;

		if ( !pevOwner )
		{
			// Owner vanished mid-flight (disconnect / level transition).
			// Don't try to drag a dead reference; just self-destruct.
			SetThink( &CBaseEntity::SUB_Remove );
			pev->nextthink = gpGlobals->time + 0.1;
			return;
		}

		pevOwner->pev->movetype = MOVETYPE_FLY;

		UTIL_Sparks( pev->origin );
	}
}

void CHook::KillHook( void )
{
	SetTouch( NULL );
	SetThink( NULL );

	RestoreFatalVictimPhysics();

	if (pevOwner)
	{
		pevOwner->pev->movetype = MOVETYPE_WALK;
		pevOwner->pev->gravity = 1;
	}

	m_fActiveHook = FALSE;
	m_fHookInWall = FALSE;
	m_fPlayerAtEnd = FALSE;
	m_fFatalPull = FALSE;
	m_fVictimPhysicsCaptured = FALSE;
	m_hFatalVictim = NULL;
	m_flFatalAbortTime = 0;

	SUB_Remove();
}

// Detach from the owning player so a dangling pointer can never be deref'd.
// Covers all removal paths: KillHook, Think death-branch, level transitions,
// and engine-side cleanup. Also restores player physics if the hook was
// torn down while still controlling the player (engine-forced removal).
void CHook::UpdateOnRemove( void )
{
	RestoreFatalVictimPhysics();

	CBaseEntity *pOwner = pevOwner;
	if ( pOwner && pOwner->IsPlayer() )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)pOwner;
		if ( pPlayer->pGrapplingHook == this )
			pPlayer->pGrapplingHook = NULL;

		// If we still claim the player's movetype (didn't go through KillHook),
		// release them so they don't get stuck in MOVETYPE_FLY.
		if ( m_fActiveHook && pPlayer->IsAlive() )
		{
			pPlayer->pev->movetype = MOVETYPE_WALK;
			pPlayer->pev->gravity = 1;
		}
	}

	m_fActiveHook = FALSE;
	m_fHookInWall = FALSE;
	m_fPlayerAtEnd = FALSE;
	m_fFatalPull = FALSE;
	m_fVictimPhysicsCaptured = FALSE;
	m_hFatalVictim = NULL;
	m_flFatalAbortTime = 0;

	CBaseEntity::UpdateOnRemove();
}

void CHook::Think ( void )
{
	if ( !pevOwner || !pevOwner->IsAlive() )
	{
		// Owner is gone (disconnected / freed) or dead: tear down safely.
		KillHook();
		return;
	}

	if ( m_fFatalPull )
	{
		UpdateFatalPull();
		if ( !m_fFatalPull )
			return;
	}

	if ( !pevOwner || !pevOwner->IsAlive() )
	{
		KillHook();
		return;
	}

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMPOINTS);
		WRITE_COORD( pevOwner->pev->origin.x);
		WRITE_COORD( pevOwner->pev->origin.y);
		WRITE_COORD( pevOwner->pev->origin.z + 18);
		WRITE_COORD( this->pev->origin.x);
		WRITE_COORD( this->pev->origin.y);
		WRITE_COORD( this->pev->origin.z);
		WRITE_SHORT( ropesprite );
		WRITE_BYTE( 1 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 1 );
		WRITE_BYTE( 10 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 ); // Red
		WRITE_BYTE( 113 ); // Green
		WRITE_BYTE( 230 ); // Blue
		WRITE_BYTE( 185 ); // Brightness
		WRITE_BYTE( 10 );
	MESSAGE_END( );

	if ( m_fFatalPull )
	{
		pev->nextthink = gpGlobals->time + 0.05;
		return;
	}

	if ( m_fHookInWall )
	{
		if ( !m_fPlayerAtEnd )
		{
			if (( pev->origin - pevOwner->pev->origin ).Length() >= 50.0)
			{
				pevOwner->pev->velocity = (( pev->origin - pevOwner->pev->origin ) + m_vVecDirHookMove) * 3.0;
				pevOwner->pev->speed = 150;
			}
			else
			{
				m_vPlayerHangOrigin = pevOwner->pev->origin;
				m_fPlayerAtEnd = TRUE;
			}
		}
		if ( m_fPlayerAtEnd )
		{
			UTIL_SetOrigin(pevOwner->pev, m_vPlayerHangOrigin);
			pevOwner->pev->velocity = Vector(0, 0, 0);
			pevOwner->pev->gravity = -.001;
			pevOwner->pev->speed = -.001;
		}
	}

	pev->nextthink = gpGlobals->time + 0.1;
}
#endif
