//
// mutator_predator.cpp
//
// "predator" mutator - client side
//
// The game as seen through the hunter's thermal vision in Predator (1987): a
// banded false colour image that runs from dark blue (cold) through cyan,
// green, yellow and orange to red and white (hot). Nothing in the engine
// knows about heat, so it is faked in two steps:
//
// 1. Heat mask. Every studio model body part is drawn a second time, right
//    after the engine draws it, into a private framebuffer: the colour is the
//    fragment depth in RGB and a heat value in alpha. The heat comes from what
//    the entity is (living players and monsters run hot, corpses cool down,
//    missiles burn, frozen targets go cold, props and items stay lukewarm)
//    plus a little texture and lighting detail.
// 2. Display. Once the 3D view is finished, it is redrawn from the scene
//    colour, the scene depth and that mask. Pixels whose mask depth matches
//    the scene depth are model pixels and take their heat, cooled towards the
//    silhouette and bleeding a halo into the surroundings. Everything else is
//    environment: cold, warmed by bright surfaces (lights) and warm colours
//    (fire), and cooling off with distance and into the sky.
//

#include "hud.h"
#include "cl_util.h"
#include "const.h"

#ifndef __APPLE__

#include "cl_entity.h"
#include "com_model.h"
#include "r_studioint.h"
#include "ref_params.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "PlatformHeaders.h"
#include "SDL2/SDL.h"
#include "GL/gl.h"

extern engine_studio_api_t IEngineStudio;
extern qboolean g_fXashEngine;

#ifdef _WIN32
#define PREDATOR_APIENTRY __stdcall
#else
#define PREDATOR_APIENTRY
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE			0x812F
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24		0x81A6
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0					0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1					0x84C1
#endif
#ifndef GL_TEXTURE2
#define GL_TEXTURE2					0x84C2
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE			0x84E0
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER			0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER			0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS			0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS				0x8B82
#endif
#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM			0x8B8D
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER				0x8D40
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER				0x8D41
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER			0x8CA9
#endif
#ifndef GL_DRAW_FRAMEBUFFER_BINDING
#define GL_DRAW_FRAMEBUFFER_BINDING	0x8CA6	// same value as GL_FRAMEBUFFER_BINDING(_EXT)
#endif
#ifndef GL_RENDERBUFFER_BINDING
#define GL_RENDERBUFFER_BINDING		0x8CA7
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0		0x8CE0
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT			0x8D00
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE		0x8CD5
#endif

// Heat of each kind of studio model, 0 = coldest, 1 = white hot.
#define PREDATOR_HEAT_PLAYER		0.89f	// living players: red core, orange and yellow towards the rim
#define PREDATOR_HEAT_MONSTER		0.87f	// living monsters
#define PREDATOR_HEAT_CORPSE		0.50f	// a corpse after PREDATOR_COOL_TIME
#define PREDATOR_COOL_TIME			25.0f	// seconds for a corpse to cool down
#define PREDATOR_HEAT_MISSILE		1.00f	// rockets, hornets, bolts, anything with a light
#define PREDATOR_HEAT_GRENADE		0.70f	// bouncing grenades, satchels, snarks in flight
#define PREDATOR_HEAT_TEMP			0.68f	// temp entity models: gibs, shells
#define PREDATOR_HEAT_VIEWMODEL		0.62f	// your own hands and weapon: green, so the view stays readable
#define PREDATOR_HEAT_FROZEN		0.38f	// players and monsters in an ice shell
#define PREDATOR_HEAT_OBJECT		0.50f	// items, weapons, props: still warmer than any wall
#define PREDATOR_MISSILE_SPEED		450.0f	// MOVETYPE_FLY entities faster than this are projectiles
#define PREDATOR_MIN_RENDERAMT		128		// translucent models below this are invisible to the sensor
#define PREDATOR_TEXTURE_DETAIL		0.04f	// heat added per unit of texture luminance around 0.45
#define PREDATOR_LIGHT_DETAIL		0.05f	// heat added per unit of vertex lighting around 0.5
#define PREDATOR_MAX_ENTITIES		4096

// The ice shell colour of CBaseMonster::Freeze (FREEZE_ICE_COLOR in dlls/basemonster.h).
#define PREDATOR_ICE_R				0
#define PREDATOR_ICE_G				115
#define PREDATOR_ICE_B				230

// Environment heat from the scene colour. Everything that is not a model stays in the blues.
#define PREDATOR_ENV_FLOOR			0.04f	// black surfaces
#define PREDATOR_ENV_LIT			0.22f	// added by ordinary lighting, white snow included
#define PREDATOR_ENV_BRIGHT			0.14f	// added by near white surfaces: light textures, lamps
#define PREDATOR_ENV_WARM			0.65f	// added by warm colours (red over blue): fire, explosions, lava
#define PREDATOR_ENV_DETAIL			0.35f	// surface detail kept through the softening
#define PREDATOR_ENV_BLUR			2.0f	// pixels; the sensor sees the world soft
#define PREDATOR_FADE_START			512.0f	// units; surfaces cool towards PREDATOR_AMBIENT from here ...
#define PREDATOR_FADE_END			3072.0f	// ... to here
#define PREDATOR_AMBIENT			0.10f
#define PREDATOR_SKY_FRACTION		0.5f	// beyond this fraction of the far plane it is sky
#define PREDATOR_SKY_HEAT			0.02f

// Look.
#define PREDATOR_RING_RADIUS		0.016f	// heat field ring, fraction of the view height (rings at 1x, 2x and 3.5x)
#define PREDATOR_EDGE_HEAT			0.80f	// a model's silhouette runs at this fraction of its heat
#define PREDATOR_HALO				1.15f	// strength of the glow that flares out of models
#define PREDATOR_BANDS				16.0f	// false colour bands
#define PREDATOR_GRAIN				0.025f	// sensor noise, in heat
#define PREDATOR_VIGNETTE			0.45f

// A model pixel counts unless the scene is nearer than the mask by more than this. Not an exact
// depth match: MSAA resolves, sample positions and 16-bit depth all move the scene depth a little.
#define PREDATOR_DEPTH_SLACK		0.01f	// fraction of the distance ...
#define PREDATOR_DEPTH_SLACK_UNITS	2.0f	// ... plus units
#define PREDATOR_MAX_GL_ERRORS		32

typedef struct
{
	GLuint		(PREDATOR_APIENTRY *CreateShader)(GLenum type);
	void		(PREDATOR_APIENTRY *ShaderSource)(GLuint shader, GLsizei count, const char *const *strings, const GLint *lengths);
	void		(PREDATOR_APIENTRY *CompileShader)(GLuint shader);
	void		(PREDATOR_APIENTRY *GetShaderiv)(GLuint shader, GLenum pname, GLint *params);
	void		(PREDATOR_APIENTRY *GetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei *length, char *infoLog);
	void		(PREDATOR_APIENTRY *DeleteShader)(GLuint shader);
	GLuint		(PREDATOR_APIENTRY *CreateProgram)(void);
	void		(PREDATOR_APIENTRY *AttachShader)(GLuint program, GLuint shader);
	void		(PREDATOR_APIENTRY *LinkProgram)(GLuint program);
	void		(PREDATOR_APIENTRY *GetProgramiv)(GLuint program, GLenum pname, GLint *params);
	void		(PREDATOR_APIENTRY *GetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei *length, char *infoLog);
	void		(PREDATOR_APIENTRY *DeleteProgram)(GLuint program);
	GLboolean	(PREDATOR_APIENTRY *IsProgram)(GLuint program);
	void		(PREDATOR_APIENTRY *UseProgram)(GLuint program);
	GLint		(PREDATOR_APIENTRY *GetUniformLocation)(GLuint program, const char *name);
	void		(PREDATOR_APIENTRY *Uniform1i)(GLint location, GLint v0);
	void		(PREDATOR_APIENTRY *Uniform2f)(GLint location, GLfloat v0, GLfloat v1);
	void		(PREDATOR_APIENTRY *Uniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
	void		(PREDATOR_APIENTRY *Uniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
	void		(PREDATOR_APIENTRY *ActiveTexture)(GLenum texture);

	void		(PREDATOR_APIENTRY *GenFramebuffers)(GLsizei n, GLuint *framebuffers);
	void		(PREDATOR_APIENTRY *DeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
	void		(PREDATOR_APIENTRY *BindFramebuffer)(GLenum target, GLuint framebuffer);
	void		(PREDATOR_APIENTRY *FramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
	void		(PREDATOR_APIENTRY *FramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
	GLenum		(PREDATOR_APIENTRY *CheckFramebufferStatus)(GLenum target);
	void		(PREDATOR_APIENTRY *GenRenderbuffers)(GLsizei n, GLuint *renderbuffers);
	void		(PREDATOR_APIENTRY *DeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers);
	void		(PREDATOR_APIENTRY *BindRenderbuffer)(GLenum target, GLuint renderbuffer);
	void		(PREDATOR_APIENTRY *RenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
} predator_gl_t;

typedef struct
{
	GLint texture;
	GLint heat;
} predator_heat_uniforms_t;

typedef struct
{
	GLint color;
	GLint depth;
	GLint mask;
	GLint origin;
	GLint texel;
	GLint maskTexel;
	GLint maskInfo;
	GLint range;
	GLint projZ;
	GLint env;
	GLint cool;
	GLint body;
	GLint fx;
	GLint envSoft;
} predator_scene_uniforms_t;

typedef struct
{
	GLuint texture;
	int width;
	int height;
} predator_copy_t;

// Frame state of the heat mask.
enum
{
	PREDATOR_FRAME_CLOSED = 0,	// HUD_Redraw ran, no view is being drawn
	PREDATOR_FRAME_OPEN,		// a view started, the mask is cleared by the first heat draw
	PREDATOR_FRAME_WRITING,		// the mask holds this frame's models
};

// Heat mask pass: the engine's own studio vertex stream, coloured by heat.
static const char kHeatVertexShader[] =
	"#version 110\n"
	"void main()\n"
	"{\n"
	"	gl_Position = ftransform();\n"
	"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
	"	gl_FrontColor = gl_Color;\n"
	"}\n";

// RGB = window depth in 24 bits, A = 0.5 + heat / 2 (always passes the engine's GL_GREATER 0.5 alpha test).
// u_heat = entity heat, texture detail, lighting detail.
static const char kHeatFragmentShader[] =
	"#version 110\n"
	"uniform sampler2D u_texture;\n"
	"uniform vec3 u_heat;\n"
	"\n"
	"void main()\n"
	"{\n"
	"	vec4 t = texture2D(u_texture, gl_TexCoord[0].st);\n"
	"	if (t.a < 0.5)\n"
	"		discard;\n"
	"	vec3 luma = vec3(0.299, 0.587, 0.114);\n"
	"	float heat = u_heat.x + (dot(t.rgb, luma) - 0.45) * u_heat.y + (dot(gl_Color.rgb, luma) - 0.5) * u_heat.z;\n"
	"	heat = clamp(heat, 0.02, 1.0);\n"
	"\n"
	"	float v = floor(gl_FragCoord.z * 16777215.0 + 0.5);\n"
	"	float r = floor(v / 65536.0);\n"
	"	v -= r * 65536.0;\n"
	"	float g = floor(v / 256.0);\n"
	"	float b = v - g * 256.0;\n"
	"	gl_FragColor = vec4(vec3(r, g, b) / 255.0, 0.5 + 0.5 * heat);\n"
	"}\n";

static const char kSceneVertexShader[] =
	"#version 110\n"
	"void main()\n"
	"{\n"
	"	gl_Position = gl_Vertex;\n"
	"}\n";

// Pixel positions are relative to the viewport origin and land on texel centres.
// u_maskInfo = mask present, depth slack (fraction of distance), depth slack (units).
// u_projZ = P[10], P[14], far plane. u_env = floor, lit, bright, warm.
// u_cool = fade start, fade end, ambient, sky fraction. u_body = ring radius, edge heat, halo gain, bands.
// u_fx = grain, noise seed, vignette, sky heat. u_envSoft = environment softening in pixels, detail kept.
static const char kSceneFragmentShader[] =
	"#version 110\n"
	"uniform sampler2D u_color;\n"
	"uniform sampler2D u_depth;\n"
	"uniform sampler2D u_mask;\n"
	"uniform vec2 u_origin;\n"
	"uniform vec2 u_texel;\n"
	"uniform vec2 u_maskTexel;\n"
	"uniform vec3 u_maskInfo;\n"
	"uniform vec2 u_range;\n"
	"uniform vec3 u_projZ;\n"
	"uniform vec4 u_env;\n"
	"uniform vec4 u_cool;\n"
	"uniform vec4 u_body;\n"
	"uniform vec4 u_fx;\n"
	"uniform vec2 u_envSoft;\n"
	"\n"
	"float SceneDepth(vec2 p)\n"
	"{\n"
	"	return texture2D(u_depth, clamp(p * u_texel, 0.0, 1.0)).r;\n"
	"}\n"
	"\n"
	"vec3 Scene(vec2 p)\n"
	"{\n"
	"	return texture2D(u_color, clamp(p * u_texel, 0.0, 1.0)).rgb;\n"
	"}\n"
	"\n"
	"// Eye distance of a window depth; also right for the viewmodel's squeezed range, relative to itself.\n"
	"float Distance(float depth)\n"
	"{\n"
	"	float ndc = (depth - u_range.x) / (u_range.y - u_range.x) * 2.0 - 1.0;\n"
	"	return u_projZ.y / min(ndc + u_projZ.x, -0.000001);\n"
	"}\n"
	"\n"
	"// Heat of the model drawn at p, or -1. Hidden only when the scene is clearly nearer than the model.\n"
	"float ModelHeat(vec2 p)\n"
	"{\n"
	"	if (u_maskInfo.x < 0.5)\n"
	"		return -1.0;\n"
	"	p = clamp(p, vec2(0.5), 1.0 / u_texel - 0.5);\n"
	"	vec2 uv = (p + u_origin) * u_maskTexel;\n"
	"	if (any(greaterThan(uv, vec2(1.0))))\n"
	"		return -1.0;\n"
	"	vec4 m = texture2D(u_mask, uv);\n"
	"	if (m.a < 0.25)\n"
	"		return -1.0;\n"
	"	vec3 b = floor(m.rgb * 255.0 + 0.5);\n"
	"	float model = Distance((b.r * 65536.0 + b.g * 256.0 + b.b) / 16777215.0);\n"
	"	float scene = Distance(SceneDepth(p));\n"
	"	if (model > scene * (1.0 + u_maskInfo.y) + u_maskInfo.z)\n"
	"		return -1.0;\n"
	"	return clamp(m.a * 2.0 - 1.0, 0.0, 1.0);\n"
	"}\n"
	"\n"
	"// acc = covered weight, heat sum, total weight.\n"
	"void Tap(vec2 p, float w, inout vec3 acc)\n"
	"{\n"
	"	float h = ModelHeat(p);\n"
	"	acc.z += w;\n"
	"	if (h >= 0.0)\n"
	"		acc.xy += vec2(w, w * h);\n"
	"}\n"
	"\n"
	"void Ring(vec2 p, float r, float w, float twist, inout vec3 acc)\n"
	"{\n"
	"	float c = r * cos(twist);\n"
	"	float s = r * sin(twist);\n"
	"	Tap(p + vec2(c, s), w, acc);\n"
	"	Tap(p + vec2(-s, c), w, acc);\n"
	"	Tap(p + vec2(-c, -s), w, acc);\n"
	"	Tap(p + vec2(s, -c), w, acc);\n"
	"	c = r * cos(twist + 0.7854);\n"
	"	s = r * sin(twist + 0.7854);\n"
	"	Tap(p + vec2(c, s), w, acc);\n"
	"	Tap(p + vec2(-s, c), w, acc);\n"
	"	Tap(p + vec2(-c, -s), w, acc);\n"
	"	Tap(p + vec2(s, -c), w, acc);\n"
	"}\n"
	"\n"
	"float EnvHeat(vec2 p)\n"
	"{\n"
	"	float e = u_envSoft.x;\n"
	"	vec3 c = Scene(p) * 0.2 + (Scene(p + vec2(-e, -e)) + Scene(p + vec2(e, -e)) + Scene(p + vec2(-e, e)) + Scene(p + vec2(e, e))\n"
	"		+ Scene(p + vec2(e * 2.0, 0.0)) + Scene(p + vec2(-e * 2.0, 0.0)) + Scene(p + vec2(0.0, e * 2.0)) + Scene(p + vec2(0.0, -e * 2.0))) * 0.1;\n"
	"	vec3 luma = vec3(0.299, 0.587, 0.114);\n"
	"	float l = dot(c, luma);\n"
	"	float h = u_env.x + u_env.y * smoothstep(0.0, 0.85, l) + u_env.z * smoothstep(0.9, 1.0, l);\n"
	"	h += u_envSoft.y * (dot(Scene(p), luma) - l);\n"
	"\n"
	"	float dist = Distance(SceneDepth(p));\n"
	"	h = mix(h, u_cool.z, smoothstep(u_cool.x, u_cool.y, dist) * 0.75);\n"
	"	h = mix(h, u_fx.w, smoothstep(u_cool.w, u_cool.w * 1.2, dist / u_projZ.z));\n"
	"\n"
	"	// Emitters carry through the distance fade: fire and explosion sprites draw no depth.\n"
	"	return h + u_env.w * clamp(c.r - c.b, 0.0, 1.0) * l;\n"
	"}\n"
	"\n"
	"vec3 Palette(float h)\n"
	"{\n"
	"	if (h < 0.15) return mix(vec3(0.00, 0.00, 0.05), vec3(0.03, 0.01, 0.30), h / 0.15);\n"
	"	if (h < 0.30) return mix(vec3(0.03, 0.01, 0.30), vec3(0.12, 0.04, 0.70), (h - 0.15) / 0.15);\n"
	"	if (h < 0.42) return mix(vec3(0.12, 0.04, 0.70), vec3(0.00, 0.42, 1.00), (h - 0.30) / 0.12);\n"
	"	if (h < 0.52) return mix(vec3(0.00, 0.42, 1.00), vec3(0.00, 0.90, 0.85), (h - 0.42) / 0.10);\n"
	"	if (h < 0.62) return mix(vec3(0.00, 0.90, 0.85), vec3(0.15, 1.00, 0.05), (h - 0.52) / 0.10);\n"
	"	if (h < 0.72) return mix(vec3(0.15, 1.00, 0.05), vec3(1.00, 0.95, 0.00), (h - 0.62) / 0.10);\n"
	"	if (h < 0.82) return mix(vec3(1.00, 0.95, 0.00), vec3(1.00, 0.45, 0.00), (h - 0.72) / 0.10);\n"
	"	if (h < 0.92) return mix(vec3(1.00, 0.45, 0.00), vec3(1.00, 0.02, 0.00), (h - 0.82) / 0.10);\n"
	"	return mix(vec3(1.00, 0.02, 0.00), vec3(1.00, 0.85, 0.85), (h - 0.92) / 0.08);\n"
	"}\n"
	"\n"
	"void main()\n"
	"{\n"
	"	vec2 pixel = gl_FragCoord.xy - u_origin;\n"
	"\n"
	"	// Heat field: three rings of eight taps, near ones weighted most. Its coverage says how deep\n"
	"	// inside a model this pixel is; the heat around flares out of the model as one contiguous glow.\n"
	"	float r = u_body.x;\n"
	"	vec3 acc = vec3(0.0);\n"
	"	Ring(pixel, r, 1.0, 0.0, acc);\n"
	"	Ring(pixel, r * 2.0, 0.6, 0.3927, acc);\n"
	"	Ring(pixel, r * 3.5, 0.3, 0.1963, acc);\n"
	"	float inside = acc.x / acc.z;\n"
	"	float near = acc.x > 0.0 ? acc.y / acc.x : 0.0;\n"
	"\n"
	"	// Models: hottest deep inside, cooling towards the rim. Elsewhere: the glow of the models\n"
	"	// around, fading slowly with distance, over the environment.\n"
	"	float h = ModelHeat(pixel);\n"
	"	float grain = u_fx.x;\n"
	"	if (h >= 0.0)\n"
	"	{\n"
	"		h *= mix(u_body.y, 1.0, smoothstep(0.3, 1.0, inside));\n"
	"		grain *= 0.4;\n"
	"	}\n"
	"	else\n"
	"	{\n"
	"		h = max(EnvHeat(pixel), near * pow(inside, 0.55) * u_body.z);\n"
	"	}\n"
	"\n"
	"	float noise = fract(sin(dot(gl_FragCoord.xy + u_fx.y, vec2(12.9898, 78.233))) * 43758.5453);\n"
	"	h = clamp(h + (noise - 0.5) * grain, 0.0, 0.999);\n"
	"	h = (floor(h * u_body.w) + 0.5) / u_body.w;\n"
	"\n"
	"	vec3 color = Palette(h);\n"
	"	vec2 v = pixel * u_texel * 2.0 - 1.0;\n"
	"	color *= 1.0 - u_fx.z * smoothstep(0.6, 2.0, dot(v, v));\n"
	"	gl_FragColor = vec4(color, 1.0);\n"
	"}\n";

class CPredator
{
public:
	CPredator( void );

	void BeginView( struct ref_params_s *pparams );
	float StudioHeat( cl_entity_t *ent, int rendermode );
	void DrawHeat( float heat );
	void CaptureScene( void );
	void EndFrame( void );
	void Draw( void );

private:
	bool Active( void ) const;
	bool Fail( const char *reason );
	bool LoadFunctions( void );
	GLuint Compile( GLenum type, const char *source );
	GLuint Link( const char *vertexSource, const char *fragmentSource, const char *name );
	bool EnsurePrograms( void );
	bool EnsureTarget( int width, int height );
	bool Copy( predator_copy_t &target, GLint internalFormat, GLenum format, GLenum type, const GLint *rect );
	float CreatureHeat( int index, bool alive, float liveHeat, float now );

	predator_gl_t m_GL = {};
	predator_heat_uniforms_t m_HeatUniforms = {};
	predator_scene_uniforms_t m_SceneUniforms = {};
	bool m_bFunctionsLoaded = false;
	bool m_bFailed = false;
	GLenum m_FramebufferTarget = GL_DRAW_FRAMEBUFFER;
	GLuint m_HeatProgram = 0;
	GLuint m_SceneProgram = 0;

	// Heat mask target, in window pixels like the engine's viewport.
	GLuint m_Framebuffer = 0;
	GLuint m_MaskTexture = 0;
	GLuint m_MaskDepth = 0;
	int m_iTargetWidth = 0;
	int m_iTargetHeight = 0;
	int m_iFrame = PREDATOR_FRAME_CLOSED;
	bool m_bMaskReady = false;

	predator_copy_t m_SceneColor = {};
	predator_copy_t m_SceneDepth = {};

	// 3D view state, captured mid-scene because HUD_Redraw runs in 2D state.
	bool m_bCaptured = false;
	GLint m_Viewport[4] = {};
	GLfloat m_DepthRange[2] = {};
	float m_ProjZ[2] = {};			// P[10], P[14]
	float m_flFar = 4096.0f;

	// Client time each entity was last seen alive and first seen dead (-1 = never), for corpse cooling.
	float m_flAliveTime[PREDATOR_MAX_ENTITIES];
	float m_flDeadTime[PREDATOR_MAX_ENTITIES];
};

static CPredator g_Predator;

template <typename T>
static bool Predator_Load( T &fn, const char *name )
{
	fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
	return fn != NULL;
}

template <typename T>
static bool Predator_LoadFramebuffer( T &fn, const char *name, bool ext )
{
	char full[64];
	strcpy(full, name);
	if (ext)
		strcat(full, "EXT");
	return Predator_Load(fn, full);
}

static bool Predator_HasExtension( const char *name )
{
	const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
	if (!extensions)
		return false;

	const size_t length = strlen(name);
	for (const char *p = strstr(extensions, name); p; p = strstr(p + length, name))
	{
		if ((p == extensions || p[-1] == ' ') && (p[length] == ' ' || p[length] == '\0'))
			return true;
	}
	return false;
}

CPredator::CPredator( void )
{
	for (int i = 0; i < PREDATOR_MAX_ENTITIES; i++)
	{
		m_flAliveTime[i] = -1.0f;
		m_flDeadTime[i] = -1.0f;
	}
}

bool CPredator::Fail( const char *reason )
{
	gEngfuncs.Con_Printf("predator: %s, effect disabled\n", reason);
	m_bFailed = true;
	return false;
}

bool CPredator::Active( void ) const
{
	if (m_bFailed || g_fXashEngine || !MutatorEnabled(MUTATOR_PREDATOR))
		return false;

	return IEngineStudio.IsHardware && IEngineStudio.IsHardware() == 1;
}

bool CPredator::LoadFunctions( void )
{
	const bool shaders = Predator_Load(m_GL.CreateShader, "glCreateShader")
		&& Predator_Load(m_GL.ShaderSource, "glShaderSource")
		&& Predator_Load(m_GL.CompileShader, "glCompileShader")
		&& Predator_Load(m_GL.GetShaderiv, "glGetShaderiv")
		&& Predator_Load(m_GL.GetShaderInfoLog, "glGetShaderInfoLog")
		&& Predator_Load(m_GL.DeleteShader, "glDeleteShader")
		&& Predator_Load(m_GL.CreateProgram, "glCreateProgram")
		&& Predator_Load(m_GL.AttachShader, "glAttachShader")
		&& Predator_Load(m_GL.LinkProgram, "glLinkProgram")
		&& Predator_Load(m_GL.GetProgramiv, "glGetProgramiv")
		&& Predator_Load(m_GL.GetProgramInfoLog, "glGetProgramInfoLog")
		&& Predator_Load(m_GL.DeleteProgram, "glDeleteProgram")
		&& Predator_Load(m_GL.IsProgram, "glIsProgram")
		&& Predator_Load(m_GL.UseProgram, "glUseProgram")
		&& Predator_Load(m_GL.GetUniformLocation, "glGetUniformLocation")
		&& Predator_Load(m_GL.Uniform1i, "glUniform1i")
		&& Predator_Load(m_GL.Uniform2f, "glUniform2f")
		&& Predator_Load(m_GL.Uniform3f, "glUniform3f")
		&& Predator_Load(m_GL.Uniform4f, "glUniform4f")
		&& Predator_Load(m_GL.ActiveTexture, "glActiveTexture");
	if (!shaders)
		return false;

	// Some loaders hand out pointers for anything, so pick the framebuffer API the context advertises.
	const char *version = (const char *)glGetString(GL_VERSION);
	bool ext = false;
	if ((version && atoi(version) >= 3) || Predator_HasExtension("GL_ARB_framebuffer_object"))
		ext = false;
	else if (Predator_HasExtension("GL_EXT_framebuffer_object"))
		ext = true;
	else
		return false;

	// EXT has a single framebuffer binding; ARB / core lets the engine's read buffer stay put.
	m_FramebufferTarget = ext ? GL_FRAMEBUFFER : GL_DRAW_FRAMEBUFFER;

	return Predator_LoadFramebuffer(m_GL.GenFramebuffers, "glGenFramebuffers", ext)
		&& Predator_LoadFramebuffer(m_GL.DeleteFramebuffers, "glDeleteFramebuffers", ext)
		&& Predator_LoadFramebuffer(m_GL.BindFramebuffer, "glBindFramebuffer", ext)
		&& Predator_LoadFramebuffer(m_GL.FramebufferTexture2D, "glFramebufferTexture2D", ext)
		&& Predator_LoadFramebuffer(m_GL.FramebufferRenderbuffer, "glFramebufferRenderbuffer", ext)
		&& Predator_LoadFramebuffer(m_GL.CheckFramebufferStatus, "glCheckFramebufferStatus", ext)
		&& Predator_LoadFramebuffer(m_GL.GenRenderbuffers, "glGenRenderbuffers", ext)
		&& Predator_LoadFramebuffer(m_GL.DeleteRenderbuffers, "glDeleteRenderbuffers", ext)
		&& Predator_LoadFramebuffer(m_GL.BindRenderbuffer, "glBindRenderbuffer", ext)
		&& Predator_LoadFramebuffer(m_GL.RenderbufferStorage, "glRenderbufferStorage", ext);
}

GLuint CPredator::Compile( GLenum type, const char *source )
{
	GLuint shader = m_GL.CreateShader(type);
	if (!shader)
		return 0;

	m_GL.ShaderSource(shader, 1, &source, NULL);
	m_GL.CompileShader(shader);

	GLint compiled = GL_FALSE;
	m_GL.GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		char log[1024];
		log[0] = '\0';
		m_GL.GetShaderInfoLog(shader, sizeof(log), NULL, log);
		gEngfuncs.Con_Printf("predator: shader compile log:\n%s\n", log);
		m_GL.DeleteShader(shader);
		return 0;
	}

	return shader;
}

GLuint CPredator::Link( const char *vertexSource, const char *fragmentSource, const char *name )
{
	const GLuint vertexShader = Compile(GL_VERTEX_SHADER, vertexSource);
	const GLuint fragmentShader = vertexShader ? Compile(GL_FRAGMENT_SHADER, fragmentSource) : 0;
	const GLuint program = fragmentShader ? m_GL.CreateProgram() : 0;
	if (!program)
	{
		if (vertexShader)
			m_GL.DeleteShader(vertexShader);
		if (fragmentShader)
			m_GL.DeleteShader(fragmentShader);
		gEngfuncs.Con_Printf("predator: %s shader did not compile\n", name);
		return 0;
	}

	m_GL.AttachShader(program, vertexShader);
	m_GL.AttachShader(program, fragmentShader);
	m_GL.LinkProgram(program);
	m_GL.DeleteShader(vertexShader);
	m_GL.DeleteShader(fragmentShader);

	GLint linked = GL_FALSE;
	m_GL.GetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		char log[1024];
		log[0] = '\0';
		m_GL.GetProgramInfoLog(program, sizeof(log), NULL, log);
		gEngfuncs.Con_Printf("predator: %s shader link log:\n%s\n", name, log);
		m_GL.DeleteProgram(program);
		return 0;
	}

	return program;
}

bool CPredator::EnsurePrograms( void )
{
	if (m_HeatProgram && m_SceneProgram && m_GL.IsProgram(m_HeatProgram) && m_GL.IsProgram(m_SceneProgram))
		return true;

	if (!m_bFunctionsLoaded)
	{
		if (!LoadFunctions())
			return Fail("OpenGL 2.0 shader or framebuffer object entry points not found");
		m_bFunctionsLoaded = true;
	}

	if (m_HeatProgram && m_GL.IsProgram(m_HeatProgram))
		m_GL.DeleteProgram(m_HeatProgram);
	if (m_SceneProgram && m_GL.IsProgram(m_SceneProgram))
		m_GL.DeleteProgram(m_SceneProgram);
	m_HeatProgram = 0;
	m_SceneProgram = 0;

	const GLuint heatProgram = Link(kHeatVertexShader, kHeatFragmentShader, "heat");
	if (!heatProgram)
		return Fail("heat shader did not build");

	const GLuint sceneProgram = Link(kSceneVertexShader, kSceneFragmentShader, "display");
	if (!sceneProgram)
	{
		m_GL.DeleteProgram(heatProgram);
		return Fail("display shader did not build");
	}

	m_HeatUniforms.texture = m_GL.GetUniformLocation(heatProgram, "u_texture");
	m_HeatUniforms.heat = m_GL.GetUniformLocation(heatProgram, "u_heat");

	m_SceneUniforms.color = m_GL.GetUniformLocation(sceneProgram, "u_color");
	m_SceneUniforms.depth = m_GL.GetUniformLocation(sceneProgram, "u_depth");
	m_SceneUniforms.mask = m_GL.GetUniformLocation(sceneProgram, "u_mask");
	m_SceneUniforms.origin = m_GL.GetUniformLocation(sceneProgram, "u_origin");
	m_SceneUniforms.texel = m_GL.GetUniformLocation(sceneProgram, "u_texel");
	m_SceneUniforms.maskTexel = m_GL.GetUniformLocation(sceneProgram, "u_maskTexel");
	m_SceneUniforms.maskInfo = m_GL.GetUniformLocation(sceneProgram, "u_maskInfo");
	m_SceneUniforms.range = m_GL.GetUniformLocation(sceneProgram, "u_range");
	m_SceneUniforms.projZ = m_GL.GetUniformLocation(sceneProgram, "u_projZ");
	m_SceneUniforms.env = m_GL.GetUniformLocation(sceneProgram, "u_env");
	m_SceneUniforms.cool = m_GL.GetUniformLocation(sceneProgram, "u_cool");
	m_SceneUniforms.body = m_GL.GetUniformLocation(sceneProgram, "u_body");
	m_SceneUniforms.envSoft = m_GL.GetUniformLocation(sceneProgram, "u_envSoft");
	m_SceneUniforms.fx = m_GL.GetUniformLocation(sceneProgram, "u_fx");

	m_HeatProgram = heatProgram;
	m_SceneProgram = sceneProgram;
	return true;
}

// Creates or resizes the mask framebuffer. Leaves every binding as it found it.
bool CPredator::EnsureTarget( int width, int height )
{
	if (m_Framebuffer && glIsTexture(m_MaskTexture) && width == m_iTargetWidth && height == m_iTargetHeight)
		return true;

	GLint previousTexture = 0;
	GLint previousRenderbuffer = 0;
	GLint previousFramebuffer = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
	glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFramebuffer);

	if (!m_Framebuffer)
		m_GL.GenFramebuffers(1, &m_Framebuffer);
	if (!m_MaskTexture || !glIsTexture(m_MaskTexture))
		glGenTextures(1, &m_MaskTexture);
	if (!m_MaskDepth)
		m_GL.GenRenderbuffers(1, &m_MaskDepth);

	for (int i = 0; i < PREDATOR_MAX_GL_ERRORS && glGetError() != GL_NO_ERROR; i++)
		;

	glBindTexture(GL_TEXTURE_2D, m_MaskTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	m_GL.BindRenderbuffer(GL_RENDERBUFFER, m_MaskDepth);
	m_GL.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

	m_GL.BindFramebuffer(m_FramebufferTarget, m_Framebuffer);
	m_GL.FramebufferTexture2D(m_FramebufferTarget, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_MaskTexture, 0);
	m_GL.FramebufferRenderbuffer(m_FramebufferTarget, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_MaskDepth);
	const GLenum status = m_GL.CheckFramebufferStatus(m_FramebufferTarget);

	m_GL.BindFramebuffer(m_FramebufferTarget, (GLuint)previousFramebuffer);
	m_GL.BindRenderbuffer(GL_RENDERBUFFER, (GLuint)previousRenderbuffer);
	glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture);

	if (status != GL_FRAMEBUFFER_COMPLETE || glGetError() != GL_NO_ERROR)
		return Fail("heat mask framebuffer could not be created");

	m_iTargetWidth = width;
	m_iTargetHeight = height;
	return true;
}

// Called last in V_CalcRefdef, right before the engine draws a view.
void CPredator::BeginView( struct ref_params_s *pparams )
{
	if (pparams->onlyClientDraw)
		return;

	// The first view after HUD_Redraw starts a new frame; spectator insets keep writing into it.
	if (m_iFrame == PREDATOR_FRAME_CLOSED)
		m_iFrame = PREDATOR_FRAME_OPEN;
}

float CPredator::CreatureHeat( int index, bool alive, float liveHeat, float now )
{
	if (index <= 0 || index >= PREDATOR_MAX_ENTITIES)
		return alive ? liveHeat : PREDATOR_HEAT_CORPSE;

	// Client time restarts on a level change.
	if (m_flAliveTime[index] > now + 1.0f || m_flDeadTime[index] > now + 1.0f)
	{
		m_flAliveTime[index] = -1.0f;
		m_flDeadTime[index] = -1.0f;
	}

	if (alive)
	{
		m_flAliveTime[index] = now;
		m_flDeadTime[index] = -1.0f;
		return liveHeat;
	}

	// A body first seen dead (late join, PVS entry) starts half cooled.
	if (m_flDeadTime[index] < 0.0f)
	{
		const bool justDied = m_flAliveTime[index] >= 0.0f && now - m_flAliveTime[index] < 1.0f;
		m_flDeadTime[index] = justDied ? m_flAliveTime[index] : now - PREDATOR_COOL_TIME * 0.5f;
	}

	float cooled = (now - m_flDeadTime[index]) / PREDATOR_COOL_TIME;
	cooled = cooled < 0.0f ? 0.0f : (cooled > 1.0f ? 1.0f : cooled);
	return liveHeat + (PREDATOR_HEAT_CORPSE - liveHeat) * cooled;
}

// Called once per StudioRenderFinal_Hardware. Returns the entity's heat, or < 0 to skip the heat pass.
float CPredator::StudioHeat( cl_entity_t *ent, int rendermode )
{
	if (!ent || !ent->model || m_iFrame == PREDATOR_FRAME_CLOSED || XRay_SecondView() || !Active())
		return -1.0f;

	// Additive models (glow shells, energy effects) and mostly invisible ones carry no heat.
	if (rendermode == kRenderTransAdd)
		return -1.0f;
	if (rendermode != kRenderNormal && ent->curstate.renderamt < PREDATOR_MIN_RENDERAMT)
		return -1.0f;

	const bool frozen = ent->curstate.rendercolor.r == PREDATOR_ICE_R
		&& ent->curstate.rendercolor.g == PREDATOR_ICE_G
		&& ent->curstate.rendercolor.b == PREDATOR_ICE_B;

	if (ent == gEngfuncs.GetViewModel() || (ent->curstate.effects & EF_VIEWMODEL) != 0)
		return frozen ? PREDATOR_HEAT_FROZEN : PREDATOR_HEAT_VIEWMODEL;

	const int index = ent->index;
	const float now = gEngfuncs.GetClientTime();
	const bool player = ent->player || (index >= 1 && index <= gEngfuncs.GetMaxClients());

	// Players and monsters; both switch to MOVETYPE_TOSS when they die.
	if (player || ent->curstate.solid == SOLID_SLIDEBOX)
	{
		const bool alive = ent->curstate.movetype != MOVETYPE_TOSS;
		const float heat = CreatureHeat(index, alive, player ? PREDATOR_HEAT_PLAYER : PREDATOR_HEAT_MONSTER, now);
		return frozen ? PREDATOR_HEAT_FROZEN : heat;
	}

	// Temp entities (gibs, shells) and static entities have no server index.
	if (index <= 0)
		return PREDATOR_HEAT_TEMP;

	if (ent->curstate.effects & (EF_BRIGHTLIGHT | EF_DIMLIGHT | EF_LIGHT))
		return PREDATOR_HEAT_MISSILE;

	switch (ent->curstate.movetype)
	{
	case MOVETYPE_FLYMISSILE:
	case MOVETYPE_BOUNCEMISSILE:
		return PREDATOR_HEAT_MISSILE;
	case MOVETYPE_BOUNCE:
		return PREDATOR_HEAT_GRENADE;
	case MOVETYPE_FLY:
	{
		// Rockets, hornets and bolts fly; so do tripmines and carried props, which sit still or move with a player.
		const float dt = ent->curstate.msg_time - ent->prevstate.msg_time;
		if (dt > 0.001f)
		{
			float delta[3];
			VectorSubtract(ent->curstate.origin, ent->prevstate.origin, delta);
			if (Length(delta) > PREDATOR_MISSILE_SPEED * dt)
				return PREDATOR_HEAT_MISSILE;
		}
		return PREDATOR_HEAT_OBJECT;
	}
	default:
		return PREDATOR_HEAT_OBJECT;
	}
}

// Called right after the engine drew one body part: draws it again into the heat mask.
void CPredator::DrawHeat( float heat )
{
	if (m_iFrame == PREDATOR_FRAME_CLOSED || !EnsurePrograms())
		return;

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	if (viewport[2] <= 0 || viewport[3] <= 0)
		return;

	const int right = viewport[0] + viewport[2];
	const int top = viewport[1] + viewport[3];

	GLint previousFramebuffer = 0;
	GLint previousProgram = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFramebuffer);
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);

	if (m_iFrame == PREDATOR_FRAME_OPEN)
	{
		if (!EnsureTarget(right, top))
			return;

		// Clear to the far end of whatever depth test the engine uses (gl_ztrick flips it every frame).
		GLint depthFunc = GL_LEQUAL;
		glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
		const bool reversed = depthFunc == GL_GREATER || depthFunc == GL_GEQUAL;

		m_GL.BindFramebuffer(m_FramebufferTarget, m_Framebuffer);
		glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_ENABLE_BIT | GL_SCISSOR_BIT);
		glDisable(GL_SCISSOR_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_TRUE);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClearDepth(reversed ? 0.0 : 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glPopAttrib();
		m_iFrame = PREDATOR_FRAME_WRITING;
	}
	else
	{
		if (right > m_iTargetWidth || top > m_iTargetHeight)
			return;
		m_GL.BindFramebuffer(m_FramebufferTarget, m_Framebuffer);
	}

	// Same transforms, depth range, depth test, culling and alpha test as the engine's pass; only blending and depth writes are forced.
	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	m_GL.UseProgram(m_HeatProgram);
	m_GL.Uniform1i(m_HeatUniforms.texture, 0);
	m_GL.Uniform3f(m_HeatUniforms.heat, heat, PREDATOR_TEXTURE_DETAIL, PREDATOR_LIGHT_DETAIL);
	IEngineStudio.StudioDrawPoints();
	m_GL.UseProgram((GLuint)previousProgram);

	glPopAttrib();
	m_GL.BindFramebuffer(m_FramebufferTarget, (GLuint)previousFramebuffer);
}

// Called from HUD_DrawNormalTriangles while the world projection is current.
void CPredator::CaptureScene( void )
{
	if (!Active())
		return;

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	if (viewport[2] <= 0 || viewport[3] <= 0)
		return;

	// Spectator inset views render after the main view; keep the largest pass.
	if (m_bCaptured && viewport[2] * viewport[3] < m_Viewport[2] * m_Viewport[3])
		return;

	GLfloat projection[16];
	glGetFloatv(GL_PROJECTION_MATRIX, projection);
	if (projection[11] > -0.5f || fabsf(projection[0]) < 0.0001f)
		return;

	GLfloat range[2];
	glGetFloatv(GL_DEPTH_RANGE, range);
	if (fabsf(range[1] - range[0]) < 0.01f)
		return;

	memcpy(m_Viewport, viewport, sizeof(m_Viewport));
	m_DepthRange[0] = range[0];
	m_DepthRange[1] = range[1];
	m_ProjZ[0] = projection[10];
	m_ProjZ[1] = projection[14];

	// far = P[14] / (P[10] + 1) for a standard perspective projection.
	const float denominator = projection[10] + 1.0f;
	m_flFar = denominator < -0.000001f ? projection[14] / denominator : 1000000.0f;
	if (m_flFar <= 0.0f)
		m_flFar = 1000000.0f;
	m_bCaptured = true;
}

// Called at the top of HUD_Redraw: no more studio models belong to this frame.
void CPredator::EndFrame( void )
{
	m_bMaskReady = m_iFrame == PREDATOR_FRAME_WRITING;
	m_iFrame = PREDATOR_FRAME_CLOSED;
}

// Copies rect of the read buffer into the target on the active unit and leaves it bound there.
bool CPredator::Copy( predator_copy_t &target, GLint internalFormat, GLenum format, GLenum type, const GLint *rect )
{
	bool specify = rect[2] != target.width || rect[3] != target.height;
	if (!target.texture || !glIsTexture(target.texture))
	{
		glGenTextures(1, &target.texture);
		specify = true;
	}

	glBindTexture(GL_TEXTURE_2D, target.texture);

	if (!specify)
	{
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rect[0], rect[1], rect[2], rect[3]);
		return true;
	}

	// Multisampled or depth-less read buffers reject the copy; check once per allocation.
	for (int i = 0; i < PREDATOR_MAX_GL_ERRORS && glGetError() != GL_NO_ERROR; i++)
		;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, rect[2], rect[3], 0, format, type, NULL);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rect[0], rect[1], rect[2], rect[3]);

	if (glGetError() != GL_NO_ERROR)
		return Fail("frame buffer could not be copied");

	target.width = rect[2];
	target.height = rect[3];
	return true;
}

// Called at the top of HUD_Redraw, after linemode and discomode: the finished 3D frame is in the back buffer, the HUD is not.
void CPredator::Draw( void )
{
	const bool captured = m_bCaptured;
	const bool maskReady = m_bMaskReady;
	m_bCaptured = false;
	m_bMaskReady = false;

	if (!captured || !Active() || !EnsurePrograms())
		return;

	const int width = m_Viewport[2];
	const int height = m_Viewport[3];
	const bool useMask = maskReady && m_MaskTexture && m_iTargetWidth > 0 && m_iTargetHeight > 0;

	GLint previousProgram = 0;
	GLint previousUnit = GL_TEXTURE0;
	GLint previousUnit1Texture = 0;
	GLint previousUnit2Texture = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &previousUnit);

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT | GL_VIEWPORT_BIT | GL_POLYGON_BIT | GL_SCISSOR_BIT);
	m_GL.ActiveTexture(GL_TEXTURE2);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousUnit2Texture);
	m_GL.ActiveTexture(GL_TEXTURE1);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousUnit1Texture);

	bool copied = Copy(m_SceneColor, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, m_Viewport);
	if (copied)
	{
		m_GL.ActiveTexture(GL_TEXTURE0);
		copied = Copy(m_SceneDepth, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, m_Viewport);
	}

	if (copied)
	{
		m_GL.ActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, useMask ? m_MaskTexture : 0);

		glViewport(m_Viewport[0], m_Viewport[1], width, height);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_STENCIL_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		const float radius = fmaxf(2.0f, floorf((float)height * PREDATOR_RING_RADIUS + 0.5f));
		const float envBlur = fmaxf(1.0f, floorf((float)height / 720.0f * PREDATOR_ENV_BLUR + 0.5f));
		const float seed = fmodf(gEngfuncs.GetClientTime() * 7.0f, 61.0f);

		m_GL.UseProgram(m_SceneProgram);
		m_GL.Uniform1i(m_SceneUniforms.depth, 0);
		m_GL.Uniform1i(m_SceneUniforms.color, 1);
		m_GL.Uniform1i(m_SceneUniforms.mask, 2);
		m_GL.Uniform2f(m_SceneUniforms.origin, (float)m_Viewport[0], (float)m_Viewport[1]);
		m_GL.Uniform2f(m_SceneUniforms.texel, 1.0f / (float)width, 1.0f / (float)height);
		m_GL.Uniform2f(m_SceneUniforms.maskTexel, useMask ? 1.0f / (float)m_iTargetWidth : 0.0f, useMask ? 1.0f / (float)m_iTargetHeight : 0.0f);
		m_GL.Uniform3f(m_SceneUniforms.maskInfo, useMask ? 1.0f : 0.0f, PREDATOR_DEPTH_SLACK, PREDATOR_DEPTH_SLACK_UNITS);
		m_GL.Uniform2f(m_SceneUniforms.range, m_DepthRange[0], m_DepthRange[1]);
		m_GL.Uniform3f(m_SceneUniforms.projZ, m_ProjZ[0], m_ProjZ[1], m_flFar);
		m_GL.Uniform4f(m_SceneUniforms.env, PREDATOR_ENV_FLOOR, PREDATOR_ENV_LIT, PREDATOR_ENV_BRIGHT, PREDATOR_ENV_WARM);
		m_GL.Uniform4f(m_SceneUniforms.cool, PREDATOR_FADE_START, PREDATOR_FADE_END, PREDATOR_AMBIENT, PREDATOR_SKY_FRACTION);
		m_GL.Uniform4f(m_SceneUniforms.body, radius, PREDATOR_EDGE_HEAT, PREDATOR_HALO, PREDATOR_BANDS);
		m_GL.Uniform4f(m_SceneUniforms.fx, PREDATOR_GRAIN, seed, PREDATOR_VIGNETTE, PREDATOR_SKY_HEAT);
		m_GL.Uniform2f(m_SceneUniforms.envSoft, envBlur, PREDATOR_ENV_DETAIL);

		glBegin(GL_QUADS);
		glVertex2f(-1.0f, -1.0f);
		glVertex2f(1.0f, -1.0f);
		glVertex2f(1.0f, 1.0f);
		glVertex2f(-1.0f, 1.0f);
		glEnd();

		m_GL.UseProgram((GLuint)previousProgram);
	}

	// The engine caches the binding of every unit it uses.
	m_GL.ActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, (GLuint)previousUnit2Texture);
	m_GL.ActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, (GLuint)previousUnit1Texture);

	glPopAttrib();
	m_GL.ActiveTexture((GLenum)previousUnit);
}

void Predator_BeginView( struct ref_params_s *pparams )
{
	g_Predator.BeginView(pparams);
}

float Predator_StudioHeat( struct cl_entity_s *ent, int rendermode )
{
	return g_Predator.StudioHeat(ent, rendermode);
}

void Predator_DrawHeat( float heat )
{
	g_Predator.DrawHeat(heat);
}

void Predator_CaptureScene( void )
{
	g_Predator.CaptureScene();
}

void Predator_EndFrame( void )
{
	g_Predator.EndFrame();
}

void Predator_Draw( void )
{
	g_Predator.Draw();
}

#else

void Predator_BeginView( struct ref_params_s *pparams )
{
}

float Predator_StudioHeat( struct cl_entity_s *ent, int rendermode )
{
	return -1.0f;
}

void Predator_DrawHeat( float heat )
{
}

void Predator_CaptureScene( void )
{
}

void Predator_EndFrame( void )
{
}

void Predator_Draw( void )
{
}

#endif
