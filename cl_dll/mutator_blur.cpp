//
// mutator_blur.cpp
//
// "blur" mutator - client side
//
// Frame feedback motion blur, after GSChaos' CFeatureBlur. Once the 3D view is
// finished, the previous blurred frame is blended over it at a frame-time based
// opacity and the result is copied back as the next frame's history, so
// anything that moves on screen leaves a decaying trail. Runs before the HUD,
// so the HUD stays sharp.
//

#include "hud.h"
#include "cl_util.h"
#include "const.h"

#ifndef __APPLE__

#include "cl_entity.h"
#include "com_model.h"
#include "r_studioint.h"
#include <math.h>
#include <string.h>

#include "PlatformHeaders.h"
#include "SDL2/SDL.h"
#include "GL/gl.h"

extern engine_studio_api_t IEngineStudio;
extern qboolean g_fXashEngine;

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE	0x812F
#endif

// GSChaos curve: history keeps 90% per frame at 100 fps, eased so low frame rates still smear.
#define BLUR_BASE_DECAY		0.9f
#define BLUR_REFERENCE_FPS	100.0f
#define BLUR_DECAY_SCALE	0.66667f
#define BLUR_DECAY_BIAS		0.3f
#define BLUR_MAX_FRAME_GAP	0.25	// seconds; longer stalls (loading, alt-tab) restart the history
#define BLUR_MAX_GL_ERRORS	32

static bool s_bFailed = false;

static GLuint s_Texture = 0;
static int s_iTextureWidth = 0;
static int s_iTextureHeight = 0;
static bool s_bVerifyCopy = false;

// The history texture holds a frame for s_Viewport only.
static bool s_bPrimed = false;
static GLint s_Viewport[4];
static double s_flLastTime = 0.0;

static void Blur_Fail( const char *reason )
{
	gEngfuncs.Con_Printf("blur: %s, effect disabled\n", reason);
	s_bFailed = true;
	s_bPrimed = false;
}

static bool Blur_Active( void )
{
	if (s_bFailed || g_fXashEngine || !MutatorEnabled(MUTATOR_BLUR))
		return false;

	return IEngineStudio.IsHardware && IEngineStudio.IsHardware() == 1;
}

// Leaves the history texture bound; a new allocation drops the history.
static void Blur_BindTexture( int width, int height )
{
	if (!s_Texture || !glIsTexture(s_Texture))
	{
		glGenTextures(1, &s_Texture);
		s_iTextureWidth = 0;
		s_iTextureHeight = 0;
	}

	glBindTexture(GL_TEXTURE_RECTANGLE_NV, s_Texture);

	if (width != s_iTextureWidth || height != s_iTextureHeight)
	{
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// RGB only: frame buffer alpha is not meaningful in GoldSrc and would skew the blend.
		glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

		s_iTextureWidth = width;
		s_iTextureHeight = height;
		s_bVerifyCopy = true;
		s_bPrimed = false;
	}
}

static void Blur_DrawHistory( int width, int height, float decay )
{
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_FOG);
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(1.0f, 1.0f, 1.0f, decay);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(-1.0f, -1.0f);
	glTexCoord2f((float)width, 0.0f);
	glVertex2f(1.0f, -1.0f);
	glTexCoord2f((float)width, (float)height);
	glVertex2f(1.0f, 1.0f);
	glTexCoord2f(0.0f, (float)height);
	glVertex2f(-1.0f, 1.0f);
	glEnd();

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}

// Called at the top of HUD_Redraw: the finished 3D frame is in the back buffer, the HUD is not.
void Blur_Draw( void )
{
	if (!Blur_Active())
	{
		s_bPrimed = false;
		return;
	}

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	if (viewport[2] <= 0 || viewport[3] <= 0)
		return;

	const double now = gEngfuncs.GetAbsoluteTime();
	const double frameTime = now - s_flLastTime;
	s_flLastTime = now;

	if (frameTime < 0.0 || frameTime > BLUR_MAX_FRAME_GAP || memcmp(viewport, s_Viewport, sizeof(s_Viewport)) != 0)
		s_bPrimed = false;
	memcpy(s_Viewport, viewport, sizeof(s_Viewport));

	const int x = viewport[0];
	const int y = viewport[1];
	const int width = viewport[2];
	const int height = viewport[3];

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT | GL_POLYGON_BIT | GL_TRANSFORM_BIT);

	Blur_BindTexture(width, height);

	if (s_bPrimed)
	{
		const float decay = powf(BLUR_BASE_DECAY, (float)frameTime * BLUR_REFERENCE_FPS) * BLUR_DECAY_SCALE + BLUR_DECAY_BIAS;
		Blur_DrawHistory(width, height, decay);
	}

	// Multisampled read buffers or missing rectangle texture support reject the copy; check once per allocation.
	if (s_bVerifyCopy)
	{
		for (int i = 0; i < BLUR_MAX_GL_ERRORS && glGetError() != GL_NO_ERROR; i++)
			;
	}

	glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, x, y, width, height);

	if (s_bVerifyCopy && glGetError() != GL_NO_ERROR)
	{
		Blur_Fail("frame buffer could not be copied");
	}
	else
	{
		s_bVerifyCopy = false;
		s_bPrimed = true;
	}

	glPopAttrib();
}

#else

void Blur_Draw( void )
{
}

#endif
