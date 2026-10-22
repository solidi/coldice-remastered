//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <assert.h>
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#include "studio_util.h"
#include "r_studioint.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
#include "Exports.h"

//
// Override the StudioModelRender virtual member functions here to implement custom bone
// setup, blending, etc.
//

// Global engine <-> studio model rendering code interface
extern engine_studio_api_t IEngineStudio;

// The renderer object, created on the stack.
CGameStudioModelRenderer g_StudioRenderer;
/*
====================
CGameStudioModelRenderer

====================
*/
CGameStudioModelRenderer::CGameStudioModelRenderer( void )
{
}

////////////////////////////////////
// Hooks to class implementation
////////////////////////////////////

/*
====================
R_StudioDrawPlayer

====================
*/
int R_StudioDrawPlayer( int flags, entity_state_t *pplayer )
{
	if ( !XRay_SecondView() )
	{
		NoBackgrounds_BeginStudio();
		int result = g_StudioRenderer.StudioDrawPlayer( flags, pplayer );
		NoBackgrounds_EndStudio();
		return result;
	}

	// xray's second view of the frame only renders: no events, and gait stays on the first view's clock
	if ( MutatorEnabled( MUTATOR_SANIC ) )
		return 1;

	if ( !( flags & STUDIO_RENDER ) )
		return 0;

	player_info_t *info = NULL;
	if ( pplayer && pplayer->number >= 1 && pplayer->number <= gEngfuncs.GetMaxClients() )
		info = IEngineStudio.PlayerInfo( pplayer->number - 1 );

	float gaitframe = 0.0f;
	float gaityaw = 0.0f;
	vec3_t prevgaitorigin = { 0.0f, 0.0f, 0.0f };
	if ( info )
	{
		gaitframe = info->gaitframe;
		gaityaw = info->gaityaw;
		VectorCopy( info->prevgaitorigin, prevgaitorigin );
	}

	XRay_BeginStudio();
	int result = g_StudioRenderer.StudioDrawPlayer( flags & ~STUDIO_EVENTS, pplayer );
	XRay_EndStudio();

	if ( info )
	{
		info->gaitframe = gaitframe;
		info->gaityaw = gaityaw;
		VectorCopy( prevgaitorigin, info->prevgaitorigin );
	}

	return result;
}

/*
====================
R_StudioDrawModel

====================
*/
int R_StudioDrawModel( int flags )
{
	if ( !XRay_SecondView() )
	{
		NoBackgrounds_BeginStudio();
		int result = g_StudioRenderer.StudioDrawModel( flags );
		NoBackgrounds_EndStudio();
		return result;
	}

	if ( !( flags & STUDIO_RENDER ) )
		return 0;

	XRay_BeginStudio();
	int result = g_StudioRenderer.StudioDrawModel( flags & ~STUDIO_EVENTS );
	XRay_EndStudio();
	return result;
}

/*
====================
R_StudioInit

====================
*/
void R_StudioInit( void )
{
	g_StudioRenderer.Init();
}

// The simple drawing interface we'll pass back to the engine
r_studio_interface_t studio =
{
	STUDIO_INTERFACE_VERSION,
	R_StudioDrawModel,
	R_StudioDrawPlayer,
};

/*
====================
HUD_GetStudioModelInterface

Export this function for the engine to use the studio renderer class to render objects.
====================
*/
int CL_DLLEXPORT HUD_GetStudioModelInterface( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio )
{
//	RecClStudioInterface(version, ppinterface, pstudio);

	if ( version != STUDIO_INTERFACE_VERSION )
		return 0;

	// Point the engine to our callbacks
	*ppinterface = &studio;

	// Copy in engine helper functions
	memcpy( &IEngineStudio, pstudio, sizeof( IEngineStudio ) );

	// Initialize local variables, etc.
	R_StudioInit();

	// Success
	return 1;
}
