//
// mutator_linemode.cpp
//
// "linemode" mutator - client side
//
// GoldenEye 007's hidden Line Mode. Once the 3D view is finished, the depth
// buffer is copied to a texture and a full-screen GLSL pass paints every pixel
// white except geometric edges, which are inked black. Window depth is affine
// in 1/z, so it is linear across any plane in screen space; a non-zero second
// difference marks a crease or a silhouette. Runs before the HUD. Additive HUD
// sprites vanish on white, so the image is drawn inverted while the HUD draws
// and inverted back afterwards, which turns the HUD into black ink.
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

#ifdef _WIN32
#define LINEMODE_APIENTRY __stdcall
#else
#define LINEMODE_APIENTRY
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE		0x812F
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24	0x81A6
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0				0x84C0
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE		0x84E0
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER		0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER		0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS		0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS			0x8B82
#endif
#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM		0x8B8D
#endif

#define LINEMODE_CREASE_SLOPE	0.5f	// change in screen-space surface slope (tan) where a crease starts to ink
#define LINEMODE_LINE_SCALE		720.0f	// viewport rows per pixel of sample offset, keeps line weight resolution independent
#define LINEMODE_DEPTH_ULPS		6.0f	// depth buffer quantization steps ignored as noise
#define LINEMODE_MAX_GL_ERRORS	32

typedef struct
{
	GLuint		(LINEMODE_APIENTRY *CreateShader)(GLenum type);
	void		(LINEMODE_APIENTRY *ShaderSource)(GLuint shader, GLsizei count, const char *const *strings, const GLint *lengths);
	void		(LINEMODE_APIENTRY *CompileShader)(GLuint shader);
	void		(LINEMODE_APIENTRY *GetShaderiv)(GLuint shader, GLenum pname, GLint *params);
	void		(LINEMODE_APIENTRY *GetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei *length, char *infoLog);
	void		(LINEMODE_APIENTRY *DeleteShader)(GLuint shader);
	GLuint		(LINEMODE_APIENTRY *CreateProgram)(void);
	void		(LINEMODE_APIENTRY *AttachShader)(GLuint program, GLuint shader);
	void		(LINEMODE_APIENTRY *LinkProgram)(GLuint program);
	void		(LINEMODE_APIENTRY *GetProgramiv)(GLuint program, GLenum pname, GLint *params);
	void		(LINEMODE_APIENTRY *GetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei *length, char *infoLog);
	void		(LINEMODE_APIENTRY *DeleteProgram)(GLuint program);
	GLboolean	(LINEMODE_APIENTRY *IsProgram)(GLuint program);
	void		(LINEMODE_APIENTRY *UseProgram)(GLuint program);
	GLint		(LINEMODE_APIENTRY *GetUniformLocation)(GLuint program, const char *name);
	void		(LINEMODE_APIENTRY *Uniform1i)(GLint location, GLint v0);
	void		(LINEMODE_APIENTRY *Uniform1f)(GLint location, GLfloat v0);
	void		(LINEMODE_APIENTRY *Uniform2f)(GLint location, GLfloat v0, GLfloat v1);
	void		(LINEMODE_APIENTRY *ActiveTexture)(GLenum texture);
} linemode_gl_t;

typedef struct
{
	GLint depth;
	GLint color;
	GLint origin;
	GLint texel;
	GLint step;
	GLint range;
	GLint farRatio;
	GLint threshold;
	GLint epsilon;
	GLint paper;
	GLint hud;
} linemode_uniforms_t;

static const char kVertexShader[] =
	"#version 110\n"
	"void main()\n"
	"{\n"
	"	gl_Position = gl_Vertex;\n"
	"}\n";

// u_paper = 1 draws black lines on white, 0 the inverted image the HUD is drawn over.
// u_hud = 1 inverts that image back from u_color, the HUD by its brightest channel.
static const char kFragmentShader[] =
	"#version 110\n"
	"uniform sampler2D u_depth;\n"
	"uniform sampler2D u_color;\n"
	"uniform vec2 u_origin;\n"
	"uniform vec2 u_texel;\n"
	"uniform float u_step;\n"
	"uniform vec2 u_range;\n"
	"uniform float u_farRatio;\n"
	"uniform float u_threshold;\n"
	"uniform float u_epsilon;\n"
	"uniform float u_paper;\n"
	"uniform float u_hud;\n"
	"\n"
	"float Depth(vec2 uv)\n"
	"{\n"
	"	return (texture2D(u_depth, uv).r - u_range.x) / (u_range.y - u_range.x);\n"
	"}\n"
	"\n"
	"float Bend(vec2 uv, float center, vec2 offset)\n"
	"{\n"
	"	vec2 a = uv - offset * u_texel;\n"
	"	vec2 b = uv + offset * u_texel;\n"
	"	if (any(lessThan(min(a, b), vec2(0.0))) || any(greaterThan(max(a, b), vec2(1.0))))\n"
	"		return 0.0;\n"
	"	return abs(Depth(a) + Depth(b) - 2.0 * center);\n"
	"}\n"
	"\n"
	"void main()\n"
	"{\n"
	"	vec2 uv = (gl_FragCoord.xy - u_origin) * u_texel;\n"
	"	if (u_hud > 0.5)\n"
	"	{\n"
	"		vec3 c = texture2D(u_color, uv).rgb;\n"
	"		gl_FragColor = vec4(vec3(1.0 - max(c.r, max(c.g, c.b))), 1.0);\n"
	"	}\n"
	"	else\n"
	"	{\n"
	"		float center = Depth(uv);\n"
	"		float bend = max(max(Bend(uv, center, vec2(u_step, 0.0)), Bend(uv, center, vec2(0.0, u_step))),\n"
	"			max(Bend(uv, center, vec2(u_step, u_step)), Bend(uv, center, vec2(u_step, -u_step))));\n"
	"		float slopeChange = (bend - u_epsilon) / max(u_farRatio - center, 0.000001);\n"
	"		float ink = smoothstep(u_threshold, u_threshold * 2.0, slopeChange);\n"
	"		gl_FragColor = vec4(vec3(mix(u_paper, 1.0 - u_paper, ink)), 1.0);\n"
	"	}\n"
	"}\n";

static linemode_gl_t s_GL;
static bool s_bFunctionsLoaded = false;
static bool s_bFailed = false;

static GLuint s_Program = 0;
static linemode_uniforms_t s_Uniforms;

static GLuint s_DepthTexture = 0;
static int s_iDepthWidth = 0;
static int s_iDepthHeight = 0;
static float s_flDepthUlp = 1.0f / 16777215.0f;
static bool s_bVerifyCopy = false;

static GLuint s_HudTexture = 0;
static int s_iHudWidth = 0;
static int s_iHudHeight = 0;

// Set while the viewport below holds the inverted image the HUD is drawn over.
static bool s_bHudInverted = false;
static GLint s_HudRect[4];

// 3D view state, captured mid-scene because HUD_Redraw runs in 2D state.
static bool s_bCaptured = false;
static GLint s_Viewport[4];
static GLfloat s_DepthRange[2];
static float s_flFarRatio = 1.0f;	// far / (far - near)
static float s_flPixelSlope = 0.0f;	// view-space tangent covered by one pixel

static bool LineMode_Fail( const char *reason )
{
	gEngfuncs.Con_Printf("linemode: %s, effect disabled\n", reason);
	s_bFailed = true;
	return false;
}

static bool LineMode_Active( void )
{
	if (s_bFailed || g_fXashEngine || !MutatorEnabled(MUTATOR_LINEMODE))
		return false;

	return IEngineStudio.IsHardware && IEngineStudio.IsHardware() == 1;
}

template <typename T>
static bool LineMode_Load( T &fn, const char *name )
{
	fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
	return fn != NULL;
}

static bool LineMode_LoadFunctions( void )
{
	return LineMode_Load(s_GL.CreateShader, "glCreateShader")
		&& LineMode_Load(s_GL.ShaderSource, "glShaderSource")
		&& LineMode_Load(s_GL.CompileShader, "glCompileShader")
		&& LineMode_Load(s_GL.GetShaderiv, "glGetShaderiv")
		&& LineMode_Load(s_GL.GetShaderInfoLog, "glGetShaderInfoLog")
		&& LineMode_Load(s_GL.DeleteShader, "glDeleteShader")
		&& LineMode_Load(s_GL.CreateProgram, "glCreateProgram")
		&& LineMode_Load(s_GL.AttachShader, "glAttachShader")
		&& LineMode_Load(s_GL.LinkProgram, "glLinkProgram")
		&& LineMode_Load(s_GL.GetProgramiv, "glGetProgramiv")
		&& LineMode_Load(s_GL.GetProgramInfoLog, "glGetProgramInfoLog")
		&& LineMode_Load(s_GL.DeleteProgram, "glDeleteProgram")
		&& LineMode_Load(s_GL.IsProgram, "glIsProgram")
		&& LineMode_Load(s_GL.UseProgram, "glUseProgram")
		&& LineMode_Load(s_GL.GetUniformLocation, "glGetUniformLocation")
		&& LineMode_Load(s_GL.Uniform1i, "glUniform1i")
		&& LineMode_Load(s_GL.Uniform1f, "glUniform1f")
		&& LineMode_Load(s_GL.Uniform2f, "glUniform2f")
		&& LineMode_Load(s_GL.ActiveTexture, "glActiveTexture");
}

static GLuint LineMode_Compile( GLenum type, const char *source )
{
	GLuint shader = s_GL.CreateShader(type);
	if (!shader)
		return 0;

	s_GL.ShaderSource(shader, 1, &source, NULL);
	s_GL.CompileShader(shader);

	GLint compiled = GL_FALSE;
	s_GL.GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		char log[1024];
		log[0] = '\0';
		s_GL.GetShaderInfoLog(shader, sizeof(log), NULL, log);
		gEngfuncs.Con_Printf("linemode: shader compile log:\n%s\n", log);
		s_GL.DeleteShader(shader);
		return 0;
	}

	return shader;
}

static bool LineMode_EnsureProgram( void )
{
	if (s_Program && s_GL.IsProgram(s_Program))
		return true;

	if (!s_bFunctionsLoaded)
	{
		if (!LineMode_LoadFunctions())
			return LineMode_Fail("OpenGL 2.0 shader entry points not found");
		s_bFunctionsLoaded = true;
	}

	const GLuint vertexShader = LineMode_Compile(GL_VERTEX_SHADER, kVertexShader);
	const GLuint fragmentShader = vertexShader ? LineMode_Compile(GL_FRAGMENT_SHADER, kFragmentShader) : 0;
	const GLuint program = fragmentShader ? s_GL.CreateProgram() : 0;
	if (!program)
	{
		if (vertexShader)
			s_GL.DeleteShader(vertexShader);
		if (fragmentShader)
			s_GL.DeleteShader(fragmentShader);
		return LineMode_Fail("edge shader did not compile");
	}

	s_GL.AttachShader(program, vertexShader);
	s_GL.AttachShader(program, fragmentShader);
	s_GL.LinkProgram(program);
	s_GL.DeleteShader(vertexShader);
	s_GL.DeleteShader(fragmentShader);

	GLint linked = GL_FALSE;
	s_GL.GetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		char log[1024];
		log[0] = '\0';
		s_GL.GetProgramInfoLog(program, sizeof(log), NULL, log);
		gEngfuncs.Con_Printf("linemode: shader link log:\n%s\n", log);
		s_GL.DeleteProgram(program);
		return LineMode_Fail("edge shader did not link");
	}

	s_Uniforms.depth = s_GL.GetUniformLocation(program, "u_depth");
	s_Uniforms.color = s_GL.GetUniformLocation(program, "u_color");
	s_Uniforms.origin = s_GL.GetUniformLocation(program, "u_origin");
	s_Uniforms.texel = s_GL.GetUniformLocation(program, "u_texel");
	s_Uniforms.step = s_GL.GetUniformLocation(program, "u_step");
	s_Uniforms.range = s_GL.GetUniformLocation(program, "u_range");
	s_Uniforms.farRatio = s_GL.GetUniformLocation(program, "u_farRatio");
	s_Uniforms.threshold = s_GL.GetUniformLocation(program, "u_threshold");
	s_Uniforms.epsilon = s_GL.GetUniformLocation(program, "u_epsilon");
	s_Uniforms.paper = s_GL.GetUniformLocation(program, "u_paper");
	s_Uniforms.hud = s_GL.GetUniformLocation(program, "u_hud");

	s_Program = program;
	return true;
}

// Expects GL_TEXTURE0 active; leaves the depth texture bound there.
static bool LineMode_CopyDepth( int x, int y, int width, int height )
{
	if (!s_DepthTexture || !glIsTexture(s_DepthTexture))
	{
		glGenTextures(1, &s_DepthTexture);
		s_iDepthWidth = 0;
		s_iDepthHeight = 0;
	}

	glBindTexture(GL_TEXTURE_2D, s_DepthTexture);

	if (width != s_iDepthWidth || height != s_iDepthHeight)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);

		GLint bits = 24;
		glGetIntegerv(GL_DEPTH_BITS, &bits);
		if (bits < 16 || bits > 24)
			bits = 24;
		s_flDepthUlp = 1.0f / (float)((1 << bits) - 1);

		s_iDepthWidth = width;
		s_iDepthHeight = height;
		s_bVerifyCopy = true;
	}

	// Multisampled or depth-less read buffers reject the copy; check once per allocation.
	if (s_bVerifyCopy)
	{
		for (int i = 0; i < LINEMODE_MAX_GL_ERRORS && glGetError() != GL_NO_ERROR; i++)
			;
	}

	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, width, height);

	if (s_bVerifyCopy)
	{
		if (glGetError() != GL_NO_ERROR)
			return LineMode_Fail("depth buffer could not be copied");
		s_bVerifyCopy = false;
	}

	return true;
}

// Expects GL_TEXTURE0 active; leaves the HUD copy bound there.
static bool LineMode_CopyHud( const GLint *rect )
{
	bool specify = rect[2] != s_iHudWidth || rect[3] != s_iHudHeight;
	if (!s_HudTexture || !glIsTexture(s_HudTexture))
	{
		glGenTextures(1, &s_HudTexture);
		specify = true;
	}

	glBindTexture(GL_TEXTURE_2D, s_HudTexture);

	if (specify)
	{
		for (int i = 0; i < LINEMODE_MAX_GL_ERRORS && glGetError() != GL_NO_ERROR; i++)
			;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, rect[2], rect[3], 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	}

	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rect[0], rect[1], rect[2], rect[3]);

	if (specify)
	{
		if (glGetError() != GL_NO_ERROR)
			return LineMode_Fail("frame buffer could not be copied");
		s_iHudWidth = rect[2];
		s_iHudHeight = rect[3];
	}

	return true;
}

// Expects the program bound and its uniforms set.
static void LineMode_DrawQuad( const GLint *rect )
{
	glViewport(rect[0], rect[1], rect[2], rect[3]);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glBegin(GL_QUADS);
	glVertex2f(-1.0f, -1.0f);
	glVertex2f(1.0f, -1.0f);
	glVertex2f(1.0f, 1.0f);
	glVertex2f(-1.0f, 1.0f);
	glEnd();
}

// Called from HUD_DrawNormalTriangles while the world projection is current.
void LineMode_CaptureScene( void )
{
	if (!LineMode_Active())
		return;

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	if (viewport[2] <= 0 || viewport[3] <= 0)
		return;

	// Spectator inset views render after the main view; keep the largest pass.
	if (s_bCaptured && viewport[2] * viewport[3] < s_Viewport[2] * s_Viewport[3])
		return;

	GLfloat projection[16];
	glGetFloatv(GL_PROJECTION_MATRIX, projection);
	if (projection[11] > -0.5f || fabsf(projection[0]) < 0.0001f)
		return;

	GLfloat range[2];
	glGetFloatv(GL_DEPTH_RANGE, range);
	if (fabsf(range[1] - range[0]) < 0.01f)
		return;

	memcpy(s_Viewport, viewport, sizeof(s_Viewport));
	s_DepthRange[0] = range[0];
	s_DepthRange[1] = range[1];
	s_flFarRatio = (1.0f - projection[10]) * 0.5f;
	s_flPixelSlope = 2.0f / (fabsf(projection[0]) * (float)viewport[2]);
	s_bCaptured = true;
}

// Called at the top of HUD_Redraw: the finished 3D frame is in the back buffer, the HUD is not.
void LineMode_Draw( void )
{
	const bool captured = s_bCaptured;
	s_bCaptured = false;
	s_bHudInverted = false;

	if (!captured || !LineMode_Active() || !LineMode_EnsureProgram())
		return;

	// discomode recolours the image and virtualboy turns the HUD red, so they keep it upright.
	const bool invert = !MutatorEnabled(MUTATOR_DISCOMODE) && !MutatorEnabled(MUTATOR_VIRTUALBOY);

	const int width = s_Viewport[2];
	const int height = s_Viewport[3];

	float step = floorf((float)height / LINEMODE_LINE_SCALE + 0.5f);
	if (step < 1.0f)
		step = 1.0f;

	GLint previousProgram = 0;
	GLint previousUnit = GL_TEXTURE0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &previousUnit);

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT | GL_VIEWPORT_BIT | GL_POLYGON_BIT | GL_SCISSOR_BIT);
	s_GL.ActiveTexture(GL_TEXTURE0);

	if (LineMode_CopyDepth(s_Viewport[0], s_Viewport[1], width, height))
	{
		const float rangeSpan = fabsf(s_DepthRange[1] - s_DepthRange[0]);

		s_GL.UseProgram(s_Program);
		s_GL.Uniform1i(s_Uniforms.depth, 0);
		s_GL.Uniform1i(s_Uniforms.color, 0);
		s_GL.Uniform2f(s_Uniforms.origin, (float)s_Viewport[0], (float)s_Viewport[1]);
		s_GL.Uniform2f(s_Uniforms.texel, 1.0f / (float)width, 1.0f / (float)height);
		s_GL.Uniform1f(s_Uniforms.step, step);
		s_GL.Uniform2f(s_Uniforms.range, s_DepthRange[0], s_DepthRange[1]);
		s_GL.Uniform1f(s_Uniforms.farRatio, s_flFarRatio);
		s_GL.Uniform1f(s_Uniforms.threshold, LINEMODE_CREASE_SLOPE * step * s_flPixelSlope);
		s_GL.Uniform1f(s_Uniforms.epsilon, LINEMODE_DEPTH_ULPS * s_flDepthUlp / rangeSpan);
		s_GL.Uniform1f(s_Uniforms.paper, invert ? 0.0f : 1.0f);
		s_GL.Uniform1f(s_Uniforms.hud, 0.0f);
		LineMode_DrawQuad(s_Viewport);
		s_GL.UseProgram((GLuint)previousProgram);

		if (invert)
		{
			memcpy(s_HudRect, s_Viewport, sizeof(s_HudRect));
			s_bHudInverted = true;
		}
	}

	glPopAttrib();
	s_GL.ActiveTexture((GLenum)previousUnit);
}

// Called right after gHUD.Redraw: inverts the image back, and the HUD drawn over it turns into black ink.
void LineMode_DrawHud( void )
{
	if (!s_bHudInverted)
		return;
	s_bHudInverted = false;

	// No mutator check: it may have expired during gHUD.Redraw, and the image is still inverted.
	if (!s_Program || !s_GL.IsProgram(s_Program))
		return;

	GLint previousProgram = 0;
	GLint previousUnit = GL_TEXTURE0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &previousUnit);

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT | GL_VIEWPORT_BIT | GL_POLYGON_BIT | GL_SCISSOR_BIT);
	s_GL.ActiveTexture(GL_TEXTURE0);

	if (LineMode_CopyHud(s_HudRect))
	{
		s_GL.UseProgram(s_Program);
		s_GL.Uniform1i(s_Uniforms.color, 0);
		s_GL.Uniform2f(s_Uniforms.origin, (float)s_HudRect[0], (float)s_HudRect[1]);
		s_GL.Uniform2f(s_Uniforms.texel, 1.0f / (float)s_HudRect[2], 1.0f / (float)s_HudRect[3]);
		s_GL.Uniform1f(s_Uniforms.hud, 1.0f);
		LineMode_DrawQuad(s_HudRect);
		s_GL.UseProgram((GLuint)previousProgram);
	}

	glPopAttrib();
	s_GL.ActiveTexture((GLenum)previousUnit);
}

// True while the HUD is being drawn over the inverted image; dark overlays must draw white.
bool LineMode_HudInverted( void )
{
	return s_bHudInverted;
}

#else

void LineMode_CaptureScene( void )
{
}

void LineMode_Draw( void )
{
}

void LineMode_DrawHud( void )
{
}

bool LineMode_HudInverted( void )
{
	return false;
}

#endif
