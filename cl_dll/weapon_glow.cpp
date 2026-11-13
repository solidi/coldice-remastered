//
// weapon_glow.cpp
//
// Pickup weapon glow - client side only
//
// Weapons lying in the world (map spawns and dropped weapon boxes) are hard to
// pick out in a fight. Each one that the studio renderer draws gets:
//   - a raised lighting floor, so the model reads lit even in dark corners,
//   - a small dynamic light that brightens the floor and walls around it,
//   - a camera facing, slowly turning serrated starburst drawn behind it in the
//     transparent triangle pass (additive, semi-transparent, depth tested).
// The glow and light colours follow the local HUD colour (HudColor), gold for a yellow HUD.
// Nothing is networked. "cl_weaponglow 0" turns all of it off.
//

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "cl_entity.h"
#include "com_model.h"
#include "dlight.h"
#include "r_efx.h"
#include "r_studioint.h"
#include <math.h>
#include <string.h>

#ifndef __APPLE__
#include "PlatformHeaders.h"
#include "SDL2/SDL.h"
#include "GL/gl.h"
#endif

extern engine_studio_api_t IEngineStudio;

// M_PI follows the negativepi mutator in this client, the starburst keeps real geometry.
#define WEAPONGLOW_PI				3.14159265f

#define WEAPONGLOW_MAX_QUEUED		64

#define WEAPONGLOW_MIN_AMBIENT		112		// lighting floor for the model itself
#define WEAPONGLOW_MIN_SHADE		96

#define WEAPONGLOW_LIGHT_DIST		512.0f	// dynamic lights only for nearby pickups
#define WEAPONGLOW_MAX_LIGHTS		8		// per view, leaves dlight slots for muzzle flashes
#define WEAPONGLOW_LIGHT_RADIUS		72.0f
#define WEAPONGLOW_LIGHT_KEY		0x5747	// keys are offset by entity index so lights are reused, not reallocated

#define WEAPONGLOW_MIN_RADIUS		10.0f
#define WEAPONGLOW_MAX_RADIUS		36.0f
#define WEAPONGLOW_SIZE_SCALE		1.35f	// starburst radius over half the model's longest side
#define WEAPONGLOW_PUSH_BACK		0.4f	// starburst sits this many radii behind the model centre

#define WEAPONGLOW_NEAR_CLEAR		32.0f	// faded out when the eye is this close
#define WEAPONGLOW_NEAR_SOLID		96.0f
#define WEAPONGLOW_FAR_SOLID		1024.0f
#define WEAPONGLOW_FAR_CLEAR		1536.0f

// Glow follows HudColor(). The starburst is additive, so each tint is lifted toward white
// to read as light. Dlight colours are toned down and kept at a similar brightness.
typedef struct
{
	unsigned long hud;
	float glow[3];
	color24 light;
} weaponglow_palette_t;

static const weaponglow_palette_t kPalettes[] =
{
	{ RGB_YELLOWISH,	{ 1.00f, 0.78f, 0.35f },	{ 160, 125,  55 } },	// warm gold, like the Boltgun pickups
	{ RGB_REDISH,		{ 1.00f, 0.32f, 0.22f },	{ 170,  45,  30 } },
	{ RGB_GREENISH,		{ 0.40f, 1.00f, 0.35f },	{  60, 150,  50 } },
	{ RGB_BLUEISH,		{ 0.35f, 0.72f, 1.00f },	{  55, 115, 180 } },
	{ RGB_ORANGEISH,	{ 1.00f, 0.55f, 0.18f },	{ 170,  85,  30 } },
	{ RGB_PURPLEISH,	{ 0.70f, 0.38f, 1.00f },	{ 110,  50, 160 } },
	{ RGB_MAGENTAISH,	{ 1.00f, 0.38f, 1.00f },	{ 150,  55, 150 } },
	{ RGB_WHITEISH,		{ 0.92f, 0.95f, 1.00f },	{ 130, 130, 135 } },
};

static void WeaponGlow_Colors( float *glow, color24 *light )
{
	const unsigned long hud = HudColor();

	for ( int i = 0; i < (int)( sizeof( kPalettes ) / sizeof( kPalettes[0] ) ); i++ )
	{
		if ( kPalettes[i].hud == hud )
		{
			VectorCopy( kPalettes[i].glow, glow );
			*light = kPalettes[i].light;
			return;
		}
	}

	// A HUD colour without an entry: lift it toward white for the glow, dim it for the light.
	int r, g, b;
	UnpackRGB( r, g, b, hud );
	const int rgb[3] = { r, g, b };
	for ( int i = 0; i < 3; i++ )
		glow[i] = 0.3f + 0.7f * ( rgb[i] / 255.0f );
	light->r = (byte)( r * 0.6f );
	light->g = (byte)( g * 0.6f );
	light->b = (byte)( b * 0.6f );
}

typedef struct
{
	int index;
	vec3_t center;
	float radius;
} weaponglow_t;

static weaponglow_t s_Queue[WEAPONGLOW_MAX_QUEUED];
static int s_iQueued = 0;
static int s_iLights = 0;

cvar_t *cl_weaponglow = NULL;

static const char *kPickupModels[] =
{
	"models/w_weapons.mdl",
	"models/w_satchel.mdl",
	"models/v_tripmine.mdl",
	"models/w_grenade.mdl",
	"models/w_portalgun.mdl",
};

void WeaponGlow_Init( void )
{
	cl_weaponglow = gEngfuncs.pfnRegisterVariable( "cl_weaponglow", "1", FCVAR_ARCHIVE );
}

bool WeaponGlow_IsPickup( struct cl_entity_s *ent )
{
	if ( !cl_weaponglow || cl_weaponglow->value <= 0 )
		return false;

	if ( !ent || !ent->model || ent->player || ent->index <= 0 )
		return false;

	// Props disguise as weapons there, a glow on the real ones would give every prop away.
	if ( gHUD.m_GameMode == GAME_PROPHUNT )
		return false;

	// Spawned weapons (after landing) and weapon boxes are TOSS + TRIGGER. Thrown, grabbed and
	// projectile uses of the same models are BOUNCE, BOUNCEMISSILE or BBOX.
	if ( ent->curstate.movetype != MOVETYPE_TOSS || ent->curstate.solid != SOLID_TRIGGER )
		return false;

	if ( ent->curstate.effects & EF_NODRAW )
		return false;

	for ( int i = 0; i < (int)( sizeof( kPickupModels ) / sizeof( kPickupModels[0] ) ); i++ )
	{
		if ( !strcmp( ent->model->name, kPickupModels[i] ) )
			return true;
	}

	return false;
}

void WeaponGlow_Lighting( struct alight_s *lighting )
{
	if ( lighting->ambientlight < WEAPONGLOW_MIN_AMBIENT )
		lighting->ambientlight = WEAPONGLOW_MIN_AMBIENT;
	if ( lighting->shadelight < WEAPONGLOW_MIN_SHADE )
		lighting->shadelight = WEAPONGLOW_MIN_SHADE;
}

void WeaponGlow_Add( struct cl_entity_s *ent, const float *mins, const float *maxs, const float *viewOrigin, bool addLight )
{
	vec3_t center;
	float longest = 0;
	for ( int i = 0; i < 3; i++ )
	{
		center[i] = ( mins[i] + maxs[i] ) * 0.5f;
		longest = fmax( longest, maxs[i] - mins[i] );
	}

	float radius = longest * 0.5f * WEAPONGLOW_SIZE_SCALE;
	radius = fmin( fmax( radius, WEAPONGLOW_MIN_RADIUS ), WEAPONGLOW_MAX_RADIUS );

	// A model can be drawn more than once before the next transparent pass (extra views); keep one entry.
	weaponglow_t *glow = NULL;
	for ( int i = 0; i < s_iQueued; i++ )
	{
		if ( s_Queue[i].index == ent->index )
		{
			glow = &s_Queue[i];
			break;
		}
	}

	if ( !glow && s_iQueued < WEAPONGLOW_MAX_QUEUED )
		glow = &s_Queue[s_iQueued++];

	if ( glow )
	{
		glow->index = ent->index;
		VectorCopy( center, glow->center );
		glow->radius = radius;
	}

	if ( !addLight || s_iLights >= WEAPONGLOW_MAX_LIGHTS )
		return;

	vec3_t delta;
	VectorSubtract( center, viewOrigin, delta );
	if ( Length( delta ) > WEAPONGLOW_LIGHT_DIST )
		return;

	dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight( WEAPONGLOW_LIGHT_KEY + ent->index );
	if ( !dl )
		return;

	const float t = gEngfuncs.GetClientTime();
	VectorCopy( center, dl->origin );
	dl->radius = WEAPONGLOW_LIGHT_RADIUS + 6.0f * sinf( t * 2.5f + ent->index );
	float glowColor[3];
	WeaponGlow_Colors( glowColor, &dl->color );
	dl->die = t + 0.1f;
	dl->decay = 0;
	dl->minlight = 0;
	dl->dark = false;
	s_iLights++;
}

#ifndef __APPLE__

static float WeaponGlow_Hash( unsigned int n )
{
	n = ( n << 13 ) ^ n;
	n = n * ( n * n * 15731u + 789221u ) + 1376312589u;
	return (float)( n & 0xffff ) / 65535.0f;
}

static void WeaponGlow_Vertex( const float *c, const float *ax, const float *ay, float angle, float r )
{
	const float x = cosf( angle ) * r;
	const float y = sinf( angle ) * r;
	glVertex3f( c[0] + ax[0] * x + ay[0] * y,
				c[1] + ax[1] * x + ay[1] * y,
				c[2] + ax[2] * x + ay[2] * y );
}

// Triangle fan whose rim alternates spike tips (clear) and valleys (dim), the serrated ring.
static void WeaponGlow_DrawSpikes( const float *c, const float *ax, const float *ay, const float *rgb, float radius,
	int spikes, float valley, float rotation, unsigned int seed, float t, float centerAlpha, float valleyAlpha )
{
	const float step = WEAPONGLOW_PI / spikes;

	glBegin( GL_TRIANGLE_FAN );
	glColor4f( rgb[0], rgb[1], rgb[2], centerAlpha );
	glVertex3fv( c );

	for ( int i = 0; i <= spikes * 2; i++ )
	{
		const int k = i % ( spikes * 2 );
		const float angle = rotation + k * step;

		if ( k & 1 )
		{
			glColor4f( rgb[0], rgb[1], rgb[2], valleyAlpha );
			WeaponGlow_Vertex( c, ax, ay, angle, radius * valley );
		}
		else
		{
			const float h = WeaponGlow_Hash( seed + k );
			const float tip = 0.75f + 0.45f * h + 0.08f * sinf( t * 4.0f + h * 2.0f * WEAPONGLOW_PI );
			glColor4f( rgb[0], rgb[1], rgb[2], 0.0f );
			WeaponGlow_Vertex( c, ax, ay, angle, radius * tip );
		}
	}

	glEnd();
}

static void WeaponGlow_DrawCore( const float *c, const float *ax, const float *ay, const float *rgb, float radius, float alpha )
{
	const int segments = 20;

	glBegin( GL_TRIANGLE_FAN );
	glColor4f( rgb[0], rgb[1], rgb[2], alpha );
	glVertex3fv( c );
	glColor4f( rgb[0], rgb[1], rgb[2], 0.0f );
	for ( int i = 0; i <= segments; i++ )
		WeaponGlow_Vertex( c, ax, ay, i * ( 2.0f * WEAPONGLOW_PI / segments ), radius );
	glEnd();
}

static float WeaponGlow_Ramp( float value, float from, float to )
{
	return fmin( fmax( ( value - from ) / ( to - from ), 0.0f ), 1.0f );
}

#endif

void WeaponGlow_TransparentTriangles( void )
{
#ifndef __APPLE__
	if ( s_iQueued > 0 && IEngineStudio.IsHardware && IEngineStudio.IsHardware() == 1 )
	{
		// Camera axes from the current view's modelview, so extra views (xray, portals) stay correct.
		GLfloat mv[16];
		glGetFloatv( GL_MODELVIEW_MATRIX, mv );
		const Vector right( mv[0], mv[4], mv[8] );
		const Vector up( mv[1], mv[5], mv[9] );
		const Vector back( mv[2], mv[6], mv[10] );
		Vector eye;
		for ( int i = 0; i < 3; i++ )
			eye[i] = -( mv[12] * right[i] + mv[13] * up[i] + mv[14] * back[i] );

		const float t = gEngfuncs.GetClientTime();

		// one HUD colour lookup per view
		float glowColor[3];
		color24 unusedLight;
		WeaponGlow_Colors( glowColor, &unusedLight );

		glPushAttrib( GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_LIGHTING_BIT );

		glDisable( GL_TEXTURE_2D );
		glDisable( GL_CULL_FACE );
		glDisable( GL_ALPHA_TEST );
		// additive glow under fog would add the fog colour instead of fading out
		glDisable( GL_FOG );
		glEnable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE );
		glShadeModel( GL_SMOOTH );

		for ( int n = 0; n < s_iQueued; n++ )
		{
			const weaponglow_t *glow = &s_Queue[n];

			vec3_t toEye;
			VectorSubtract( eye, glow->center, toEye );
			const float dist = Length( toEye );
			if ( dist < 1.0f )
				continue;

			const float fade = WeaponGlow_Ramp( dist, WEAPONGLOW_NEAR_CLEAR, WEAPONGLOW_NEAR_SOLID ) *
				( 1.0f - WeaponGlow_Ramp( dist, WEAPONGLOW_FAR_SOLID, WEAPONGLOW_FAR_CLEAR ) );
			if ( fade <= 0.0f )
				continue;

			const float pulse = 1.0f + 0.06f * sinf( t * 3.0f + glow->index );
			const float radius = glow->radius * pulse;

			vec3_t c;
			VectorMA( glow->center, -( radius * WEAPONGLOW_PUSH_BACK ) / dist, toEye, c );

			const unsigned int seed = (unsigned int)glow->index * 97u;
			WeaponGlow_DrawCore( c, right, up, glowColor, radius * 0.6f, 0.30f * fade );
			WeaponGlow_DrawSpikes( c, right, up, glowColor, radius, 12, 0.45f, t * 0.45f, seed, t, 0.35f * fade, 0.16f * fade );
			WeaponGlow_DrawSpikes( c, right, up, glowColor, radius * 0.8f, 9, 0.35f, -t * 0.3f + 0.5f, seed + 31u, t, 0.22f * fade, 0.10f * fade );
		}

		glPopAttrib();
	}
#endif

	s_iQueued = 0;
	s_iLights = 0;
}
