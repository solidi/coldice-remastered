//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "Exports.h"

#include "particleman.h"
#include "tri.h"
#include "rain.h"
#include "particlemgr.h"

extern IParticleMan *g_pParticleMan;

extern cvar_t *cl_weather;
extern cvar_t *cl_particlesystem;

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void CL_DLLEXPORT HUD_DrawNormalTriangles( void )
{
//	RecClDrawNormalTriangles();

	gHUD.m_Spectator.DrawOverview();
}

#if defined( _TFC )
void RunEventList( void );
#endif

/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
static float fOldTime, fTime;
extern ParticleSystemManager g_pParticleSystems; 

void CL_DLLEXPORT HUD_DrawTransparentTriangles( void )
{
//	RecClDrawTransparentTriangles();

#if defined( _TFC )
	RunEventList();
#endif

	if ( g_pParticleMan )
		 g_pParticleMan->Update();

	if (cl_particlesystem->value)
	{
		fOldTime = fTime;
		fTime = gEngfuncs.GetClientTime();
		float timedelta = fTime - fOldTime;
		if (timedelta < 0) 
			timedelta = 0;
		if (timedelta > 0.5) 
			timedelta = 0.5;
		g_pParticleSystems.UpdateSystems(timedelta, FALSE);
	}

	if (cl_weather->value)
	{
		ProcessFXObjects();
		ProcessRain();
		DrawRain();
		DrawFXObjects();
	}
}

void DrawCrosshair()
{
	if (gHUD.crossspr.spr != 0)
	{
		static float oldtime = 0;
		float flTime = gEngfuncs.GetClientTime();

		int y = ScreenHeight;
		int x = ScreenWidth;

		if (oldtime != flTime)
		{
			SPR_Set(gHUD.crossspr.spr, 128, 128, 128);
			SPR_DrawAdditive(0, x, y, &gHUD.crossspr.rc, false);
		}
		oldtime = flTime;
	}
}

/*
=================
HUD_DrawOrthoTriangles
Orthogonal Triangles -- (relative to resolution,
smackdab on the screen) add them here
=================
*/
void HUD_DrawOrthoTriangles()
{
	//DrawCrosshair();
}

#ifndef __APPLE__
#ifndef GL_TEXTURE_RECTANGLE_NV
#define GL_TEXTURE_RECTANGLE_NV 0x84F5
#endif // !GL_TEXTURE_RECTANGLE_NV

void R_SetupScreenStuff()
{
	// enable some OpenGL stuff
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glColor3f(1, 1, 1);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, 1, 1, 0, 0.1, 100);
}

void R_ResetScreenStuff()
{
	// reset state
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glDisable(GL_TEXTURE_RECTANGLE_NV);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
}
#endif
