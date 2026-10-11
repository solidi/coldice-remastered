//
// mutator_goldeneye.cpp
//
// "goldeneye" mutator - client side
//
// Replaces the health and armor numbers with GoldenEye 007 styled curved,
// segmented bars that flash up briefly when the local player is hurt. Health
// bends on the left (yellow -> red), armor on the right (light blue -> blue).
// Bright fill is the current value, dim fill is the remaining capacity. The
// geometry is generated every frame and scaled from the screen resolution.
//

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "cl_entity.h"
#include "com_model.h"
#include "r_studioint.h"
#include <math.h>

#ifndef __APPLE__
#include "PlatformHeaders.h"
#include "SDL2/SDL.h"
#include "GL/gl.h"
#endif

extern engine_studio_api_t IEngineStudio;
extern float g_xP, g_yP;

#define GOLDENEYE_HOLD_TIME		2.5f	// seconds fully visible after the last hit
#define GOLDENEYE_FADE_TIME		0.5f
#define GOLDENEYE_REVEAL_TIME	0.2f	// bottom-to-top sweep when the bars pop in
#define GOLDENEYE_DRAIN_RATE	6.0f	// how fast the shown fill chases the real value
#define GOLDENEYE_FILL_ALPHA	(235.0f / 255.0f)
#define GOLDENEYE_EMPTY_ALPHA	(70.0f / 255.0f)
#define GOLDENEYE_STEP_DEG		2.0f	// arc tessellation step

// Radii and centre drop are fractions of min(height, width * 0.75) so the arcs stay round on any aspect.
#define GOLDENEYE_OUTER_RADIUS	0.52f
#define GOLDENEYE_INNER_RADIUS	0.42f
#define GOLDENEYE_CENTER_DROP	0.03f

typedef struct
{
	float startDeg;
	float endDeg;
} goldeneye_segment_t;

// Degrees above (+) / below (-) the horizontal, bottom to top, measured off the N64 HUD:
// five short ticks under three long blocks.
static const goldeneye_segment_t kSegments[] =
{
	{ -50.0f, -44.5f },
	{ -40.5f, -35.0f },
	{ -31.0f, -25.5f },
	{ -21.5f, -16.0f },
	{ -12.0f,  -6.5f },
	{  -2.5f,  13.0f },
	{  16.5f,  32.0f },
	{  35.5f,  51.0f },
};

#define GOLDENEYE_NUM_SEGMENTS	(int)(sizeof(kSegments) / sizeof(kSegments[0]))
#define GOLDENEYE_ARC_BOTTOM	-50.0f
#define GOLDENEYE_ARC_TOP		51.0f

static float s_flShowStart = 0;
static float s_flShowUntil = 0;
static float s_flShownHealth = 0;
static float s_flShownArmor = 0;

bool GoldenEye_Active( void )
{
#ifdef __APPLE__
	return false;
#else
	if (!MutatorEnabled(MUTATOR_GOLDENEYE))
		return false;

	// The bars are raw GL; software renderers keep the classic numbers instead of showing nothing.
	return IEngineStudio.IsHardware && IEngineStudio.IsHardware() == 1;
#endif
}

void GoldenEye_Reset( void )
{
	s_flShowStart = 0;
	s_flShowUntil = 0;
}

static bool GoldenEye_IsShowing( float now )
{
	return s_flShowUntil > 0 && now < s_flShowUntil + GOLDENEYE_FADE_TIME;
}

void GoldenEye_OnDamage( void )
{
	if (!GoldenEye_Active())
		return;

	const float now = gHUD.m_flTime;
	if (!GoldenEye_IsShowing(now))
		s_flShowStart = now;

	s_flShowUntil = now + GOLDENEYE_HOLD_TIME;
}

#ifndef __APPLE__

static float GoldenEye_Clamp01( float value )
{
	if (value < 0.0f)
		return 0.0f;
	if (value > 1.0f)
		return 1.0f;
	return value;
}

// Gradients sampled from the original HUD: health yellow -> red, armor light blue -> deep blue.
static void GoldenEye_SetColor( bool armor, float deg, float alpha )
{
	const float t = GoldenEye_Clamp01((deg - GOLDENEYE_ARC_BOTTOM) / (GOLDENEYE_ARC_TOP - GOLDENEYE_ARC_BOTTOM));

	if (armor)
		glColor4f((172.0f - 151.0f * t) / 255.0f, (226.0f - 204.0f * t) / 255.0f, 1.0f, alpha);
	else
		glColor4f((255.0f - 20.0f * t) / 255.0f, (224.0f - 184.0f * t) / 255.0f, (60.0f - 40.0f * t) / 255.0f, alpha);
}

static void GoldenEye_DrawPiece( float side, bool armor, float startDeg, float endDeg, float cx, float cy, float rIn, float rOut, float alpha )
{
	const float degToRad = (float)(M_PI / 180.0);
	int steps = (int)ceilf((endDeg - startDeg) / GOLDENEYE_STEP_DEG);
	if (steps < 1)
		steps = 1;

	glBegin(GL_QUAD_STRIP);
	for (int i = 0; i <= steps; i++)
	{
		const float deg = startDeg + (endDeg - startDeg) * ((float)i / (float)steps);
		const float c = cosf(deg * degToRad) * side;
		const float s = sinf(deg * degToRad);

		GoldenEye_SetColor(armor, deg, alpha);
		glVertex2f(cx + c * rOut, cy - s * rOut);
		glVertex2f(cx + c * rIn, cy - s * rIn);
	}
	glEnd();
}

static void GoldenEye_DrawBar( float side, bool armor, float fill, float cx, float cy, float rIn, float rOut, float revealDeg, float visibility )
{
	float totalDeg = 0;
	for (int i = 0; i < GOLDENEYE_NUM_SEGMENTS; i++)
		totalDeg += kSegments[i].endDeg - kSegments[i].startDeg;

	// Fill follows arc length so every point of health/armor moves visible bar area.
	float remaining = fill * totalDeg;

	for (int i = 0; i < GOLDENEYE_NUM_SEGMENTS; i++)
	{
		const float start = kSegments[i].startDeg;
		const float length = kSegments[i].endDeg - start;
		const float end = fmin(kSegments[i].endDeg, revealDeg);
		const float split = start + fmax(0.0f, fmin(remaining, length));
		remaining -= length;

		if (end <= start)
			break;

		if (split > start)
			GoldenEye_DrawPiece(side, armor, start, fmin(split, end), cx, cy, rIn, rOut, GOLDENEYE_FILL_ALPHA * visibility);

		if (end > split)
			GoldenEye_DrawPiece(side, armor, split, end, cx, cy, rIn, rOut, GOLDENEYE_EMPTY_ALPHA * visibility);
	}
}

#endif

void GoldenEye_Draw( int health, int armor )
{
#ifndef __APPLE__
	const float now = gHUD.m_flTime;

	// Client clock restarts on a new map; drop a timer that now sits in the future.
	if (s_flShowUntil - now > GOLDENEYE_HOLD_TIME || s_flShowStart > now)
		GoldenEye_Reset();

	const float maxValue = MutatorEnabled(MUTATOR_999) ? 999.0f : 100.0f;
	const float targetHealth = GoldenEye_Clamp01((float)health / maxValue);
	const float targetArmor = GoldenEye_Clamp01((float)armor / maxValue);

	if (!GoldenEye_IsShowing(now))
	{
		// Track silently so the next hit drains from the pre-hit value.
		s_flShownHealth = targetHealth;
		s_flShownArmor = targetArmor;
		return;
	}

	const float blend = 1.0f - expf(-(float)gHUD.m_flTimeDelta * GOLDENEYE_DRAIN_RATE);
	s_flShownHealth += (targetHealth - s_flShownHealth) * blend;
	s_flShownArmor += (targetArmor - s_flShownArmor) * blend;

	float visibility = 1.0f;
	if (now > s_flShowUntil)
		visibility = GoldenEye_Clamp01(1.0f - (now - s_flShowUntil) / GOLDENEYE_FADE_TIME);

	const float reveal = GoldenEye_Clamp01((now - s_flShowStart) / GOLDENEYE_REVEAL_TIME);
	const float revealDeg = GOLDENEYE_ARC_BOTTOM + (GOLDENEYE_ARC_TOP - GOLDENEYE_ARC_BOTTOM) * reveal;

	const float base = fmin((float)ScreenHeight, (float)ScreenWidth * 0.75f);
	const float cx = ScreenWidth * 0.5f + g_xP;
	const float cy = ScreenHeight * 0.5f + base * GOLDENEYE_CENTER_DROP + g_yP;
	const float rOut = base * GOLDENEYE_OUTER_RADIUS;
	const float rIn = base * GOLDENEYE_INNER_RADIUS;

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT | GL_LIGHTING_BIT | GL_TRANSFORM_BIT);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, ScreenWidth, ScreenHeight, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glShadeModel(GL_SMOOTH);

	GoldenEye_DrawBar(-1.0f, false, s_flShownHealth, cx, cy, rIn, rOut, revealDeg, visibility);
	GoldenEye_DrawBar(1.0f, true, s_flShownArmor, cx, cy, rIn, rOut, revealDeg, visibility);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glPopAttrib();
#endif
}
