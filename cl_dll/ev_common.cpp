/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
// shared event functions
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"

#include "r_efx.h"

#include "eventscripts.h"
#include "event_api.h"
#include "pm_shared.h"

#include "pmtrace.h"
#include "pm_defs.h"

#define IS_FIRSTPERSON_SPEC ( g_iUser1 == OBS_IN_EYE || (g_iUser1 && (gHUD.m_Spectator.m_pip->value == INSET_IN_EYE)) )
/*
=================
GetEntity

Return's the requested cl_entity_t
=================
*/
struct cl_entity_s *GetEntity( int idx )
{
	return gEngfuncs.GetEntityByIndex( idx );
}

/*
=================
GetViewEntity

Return's the current weapon/view model
=================
*/
struct cl_entity_s *GetViewEntity( void )
{
	return gEngfuncs.GetViewModel();
}

/*
=================
EV_CreateTracer

Creates a tracer effect
=================
*/
void EV_CreateTracer( float *start, float *end )
{
	gEngfuncs.pEfxAPI->R_TracerEffect( start, end );
}

/*
=================
EV_IsPlayer

Is the entity's index in the player range?
=================
*/
qboolean EV_IsPlayer( int idx )
{
	if ( idx >= 1 && idx <= gEngfuncs.GetMaxClients() )
		return true;

	return false;
}

/*
=================
EV_IsLocal

Is the entity == the local player
=================
*/
qboolean EV_IsLocal( int idx )
{
	// check if we are in some way in first person spec mode
	if ( IS_FIRSTPERSON_SPEC  )
		return (g_iUser2 == idx);
	else
		return gEngfuncs.pEventAPI->EV_IsLocal( idx - 1 ) ? true : false;
}

/*
=================
EV_GetGunPosition

Figure out the height of the gun
=================
*/
void EV_GetGunPosition( event_args_t *args, float *pos, float *origin )
{
	int idx;
	vec3_t view_ofs;

	idx = args->entindex;

	VectorClear( view_ofs );
	view_ofs[2] = DEFAULT_VIEWHEIGHT;

	if ( EV_IsPlayer( idx ) )
	{
		// in spec mode use entity viewheigh, not own
		if ( EV_IsLocal( idx ) && !IS_FIRSTPERSON_SPEC )
		{
			// Grab predicted result for local player
			gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );
		}
		else if ( args->ducking == 1 )
		{
			view_ofs[2] = VEC_DUCK_VIEW;
		}
	}

	VectorAdd( origin, view_ofs, pos );

	extern int cam_thirdperson;
	if (EV_IsLocal(idx) && cam_thirdperson)
	{
		extern ref_params_t g_ViewParams;
		Vector right;
		AngleVectors(g_ViewParams.viewangles, NULL, right, NULL);

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);

		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();

		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(gEngfuncs.GetLocalPlayer()->index - 1);

		gEngfuncs.pEventAPI->EV_SetTraceHull(2);

		pmtrace_t tr;
		Vector vieworg = origin;

		gEngfuncs.pEventAPI->EV_PlayerTrace(vieworg, vieworg - (right * 50.0f), PM_STUDIO_IGNORE | PM_STUDIO_BOX, -1, &tr);

		gEngfuncs.pEventAPI->EV_PopPMStates();

		VectorSubtract(pos, (right * (-14.0f * tr.fraction)), pos);
	}
}

/*
=================
EV_EjectBrass

Bullet shell casings
=================
*/
void EV_EjectBrass( float *origin, float *velocity, float rotation, int model, int soundtype )
{
	vec3_t endpos;
	VectorClear( endpos );
	endpos[1] = rotation;
	TEMPENTITY *t = gEngfuncs.pEfxAPI->R_TempModel( origin, velocity, endpos, 2.5, model, soundtype );
	if (t)
	{
		t->entity.curstate.body = 0; // always 0, otherwise the random is randomized.
	}
}

/*
=================
EV_GetDefaultShellInfo

Determine where to eject shells from
=================
*/
void EV_GetDefaultShellInfo( event_args_t *args, float *origin, float *velocity, float *ShellVelocity, float *ShellOrigin, float *forward, float *right, float *up, float forwardScale, float upScale, float rightScale, float fU, float fR  )
{
	int i;
	vec3_t view_ofs;

	int idx;

	idx = args->entindex;

	VectorClear( view_ofs );
	view_ofs[2] = DEFAULT_VIEWHEIGHT;

	if ( EV_IsPlayer( idx ) )
	{
		if ( EV_IsLocal( idx ) )
		{
			gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );
		}
		else if ( args->ducking == 1 )
		{
			view_ofs[2] = VEC_DUCK_VIEW;
		}
	}

	for ( i = 0; i < 3; i++ )
	{
		ShellVelocity[i] = velocity[i] + right[i] * fR + up[i] * fU + forward[i] * 25;
		ShellOrigin[i]   = origin[i] + view_ofs[i] + up[i] * upScale + forward[i] * forwardScale + right[i] * rightScale;
	}
}

/*
=================
EV_MuzzleFlash

Flag weapon/view model for muzzle flash
=================
*/
void EV_MuzzleFlash( void )
{
	// Add muzzle flash to current weapon model
	cl_entity_t *ent = GetViewEntity();
	if ( !ent )
	{
		return;
	}

	// Or in the muzzle flash
	ent->curstate.effects |= EF_MUZZLEFLASH;
}
