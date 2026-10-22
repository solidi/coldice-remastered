//
// mutator_nobackgrounds.cpp
//
// "nobackgrounds" mutator - client side
//
// After the GoldenEye 007 GameShark "no background" code: the level geometry
// stops drawing while characters, props, doors and glass still do, over a flat
// backdrop. Between V_CalcRefdef and HUD_DrawNormalTriangles the engine only
// draws the world and the solid entities, so a user clip plane that rejects
// every vertex is on for that stretch: the world, sky, world water and decals
// leave no colour and no depth, and players see across the map. The
// GameStudioModelRenderer wrappers turn the plane off around studio models,
// and opaque brush entities and sprites are moved to the transparent pass,
// which draws after the plane is off. Collision is untouched; only drawing
// changes.
//

#include "hud.h"
#include "cl_util.h"
#include "const.h"

#ifndef __APPLE__

#include "cl_entity.h"
#include "com_model.h"
#include "entity_types.h"
#include "r_studioint.h"
#include "ref_params.h"
#include "fog.h"

#include "PlatformHeaders.h"
#include "GL/gl.h"

extern engine_studio_api_t IEngineStudio;
extern qboolean g_fXashEngine;

// GoldenEye blue below the sky; while fog is on the backdrop takes the fog colour instead.
#define NOBACKGROUNDS_BACKDROP_R	0.09f
#define NOBACKGROUNDS_BACKDROP_G	0.24f
#define NOBACKGROUNDS_BACKDROP_B	0.50f

#define NOBACKGROUNDS_MAX_MOVED		2048

typedef struct
{
	cl_entity_t	*ent;
	int			renderamt;
} nobackgrounds_moved_t;

static bool s_bClipping = false;
static nobackgrounds_moved_t s_Moved[NOBACKGROUNDS_MAX_MOVED];
static int s_iMoved = 0;

static bool NoBackgrounds_Active( void )
{
	if (g_fXashEngine || !MutatorEnabled(MUTATOR_NOBACKGROUNDS))
		return false;

	return IEngineStudio.IsHardware && IEngineStudio.IsHardware() == 1;
}

static void NoBackgrounds_StopClip( void )
{
	if (s_bClipping)
	{
		glDisable(GL_CLIP_PLANE0);
		s_bClipping = false;
	}
}

// Called last in V_CalcRefdef, right before the engine draws the view.
void NoBackgrounds_BeginView( struct ref_params_s *pparams )
{
	NoBackgrounds_StopClip();

	// Spectators keep the world: their inset owns nextView and the overview map is onlyClientDraw.
	if (pparams->onlyClientDraw || pparams->spectator || g_iUser1 || gEngfuncs.IsSpectateOnly() || !NoBackgrounds_Active())
		return;

	float color[3] = { NOBACKGROUNDS_BACKDROP_R, NOBACKGROUNDS_BACKDROP_G, NOBACKGROUNDS_BACKDROP_B };
	gFog.GetActiveColor(color);

	// The engine only clears depth (gl_clear 0), so the void would smear the previous frame.
	glPushAttrib(GL_COLOR_BUFFER_BIT | GL_SCISSOR_BIT);
	glDisable(GL_SCISSOR_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(color[0], color[1], color[2], 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glPopAttrib();

	// -w < 0 for every vertex, so nothing passes while the plane is on.
	static const GLdouble plane[4] = { 0.0, 0.0, 0.0, -1.0 };

	GLint matrixMode = GL_MODELVIEW;
	glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glClipPlane(GL_CLIP_PLANE0, plane);
	glPopMatrix();
	glMatrixMode((GLenum)matrixMode);

	glEnable(GL_CLIP_PLANE0);
	s_bClipping = true;
}

// Only the world and solid entities have been drawn; everything after this draws normally.
void NoBackgrounds_NormalTriangles( void )
{
	NoBackgrounds_StopClip();
}

void NoBackgrounds_EndFrame( void )
{
	NoBackgrounds_StopClip();

	// Temp and static entities are client owned, so no server update would undo the move.
	for (int i = 0; i < s_iMoved; i++)
	{
		cl_entity_t *ent = s_Moved[i].ent;

		// Left alone if something else changed it this frame (the sanic death fade).
		if (ent->curstate.rendermode == kRenderTransAlpha && ent->curstate.renderamt == 255)
		{
			ent->curstate.rendermode = kRenderNormal;
			ent->curstate.renderamt = s_Moved[i].renderamt;
		}
	}
	s_iMoved = 0;
}

void NoBackgrounds_BeginStudio( void )
{
	if (s_bClipping)
		glDisable(GL_CLIP_PLANE0);
}

void NoBackgrounds_EndStudio( void )
{
	if (s_bClipping)
		glEnable(GL_CLIP_PLANE0);
}

void NoBackgrounds_AddEntity( int type, struct cl_entity_s *ent )
{
	// Beams keep their beam type and flags in rendermode; players are studio models.
	if (type != ET_NORMAL && type != ET_TEMPENTITY && type != ET_FRAGMENTED)
		return;

	if (!ent->model || (ent->model->type != mod_brush && ent->model->type != mod_sprite) || ent->curstate.rendermode != kRenderNormal)
		return;

	if (!NoBackgrounds_Active() || s_iMoved >= NOBACKGROUNDS_MAX_MOVED)
		return;

	s_Moved[s_iMoved].ent = ent;
	s_Moved[s_iMoved].renderamt = ent->curstate.renderamt;
	s_iMoved++;

	// "Solid" mode draws with the transparent entities, after the clip plane; amount 0 would be skipped.
	ent->curstate.rendermode = kRenderTransAlpha;
	ent->curstate.renderamt = 255;
}

#else

void NoBackgrounds_BeginView( struct ref_params_s *pparams )
{
}

void NoBackgrounds_NormalTriangles( void )
{
}

void NoBackgrounds_EndFrame( void )
{
}

void NoBackgrounds_BeginStudio( void )
{
}

void NoBackgrounds_EndStudio( void )
{
}

void NoBackgrounds_AddEntity( int type, struct cl_entity_s *ent )
{
}

#endif
