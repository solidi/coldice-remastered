//
// mutator_xray.cpp
//
// "xray" mutator - client side
//
// After the GoldenEye 007 GameShark "see-through geometry" code, which broke
// the texture layers on walls, floors and ceilings so solid surfaces turned
// semi-translucent and showed the rooms behind them. The view is drawn twice
// a frame through the engine's ref_params nextView loop: first normally, then
// again with a user clip plane that cuts the world and brush entities away
// within XRAY_PEEL_DISTANCE of the eye (studio models are exempt) and with
// r_novis forced on, so whatever lies behind the near surfaces is drawn over
// black void. At the end of the second view's transparent pass the first view
// is blended back on top: near surfaces at XRAY_SURFACE_ALPHA, their creases
// and silhouettes nearly solid, fading to fully solid at the peel distance.
// The viewmodel draws after that and the HUD after the frame, both untouched.
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

#include "PlatformHeaders.h"
#include "SDL2/SDL.h"
#include "GL/gl.h"

extern engine_studio_api_t IEngineStudio;
extern qboolean g_fXashEngine;

#ifdef _WIN32
#define XRAY_APIENTRY __stdcall
#else
#define XRAY_APIENTRY
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
#ifndef GL_TEXTURE1
#define GL_TEXTURE1				0x84C1
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

#define XRAY_PEEL_DISTANCE		384.0f	// eye-space depth inside which the world and brush entities turn see-through
#define XRAY_FADE_START			0.5f	// share of the peel distance where surfaces start turning solid again
#define XRAY_SURFACE_ALPHA		0.4f	// opacity of see-through surfaces
#define XRAY_EDGE_ALPHA			0.85f	// opacity of their creases and silhouettes, so walls read as panes of glass
#define XRAY_VIEWMODEL_DEPTH	0.3f	// GoldSrc squeezes the viewmodel into this share of the depth range
#define XRAY_CREASE_SLOPE		0.5f	// change in screen-space surface slope (tan) where an edge starts to show
#define XRAY_LINE_SCALE			720.0f	// viewport rows per pixel of edge sample offset
#define XRAY_DEPTH_ULPS			6.0f	// depth buffer quantization steps ignored as noise
#define XRAY_MAX_GL_ERRORS		32

typedef struct
{
	GLuint		(XRAY_APIENTRY *CreateShader)(GLenum type);
	void		(XRAY_APIENTRY *ShaderSource)(GLuint shader, GLsizei count, const char *const *strings, const GLint *lengths);
	void		(XRAY_APIENTRY *CompileShader)(GLuint shader);
	void		(XRAY_APIENTRY *GetShaderiv)(GLuint shader, GLenum pname, GLint *params);
	void		(XRAY_APIENTRY *GetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei *length, char *infoLog);
	void		(XRAY_APIENTRY *DeleteShader)(GLuint shader);
	GLuint		(XRAY_APIENTRY *CreateProgram)(void);
	void		(XRAY_APIENTRY *AttachShader)(GLuint program, GLuint shader);
	void		(XRAY_APIENTRY *LinkProgram)(GLuint program);
	void		(XRAY_APIENTRY *GetProgramiv)(GLuint program, GLenum pname, GLint *params);
	void		(XRAY_APIENTRY *GetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei *length, char *infoLog);
	void		(XRAY_APIENTRY *DeleteProgram)(GLuint program);
	GLboolean	(XRAY_APIENTRY *IsProgram)(GLuint program);
	void		(XRAY_APIENTRY *UseProgram)(GLuint program);
	GLint		(XRAY_APIENTRY *GetUniformLocation)(GLuint program, const char *name);
	void		(XRAY_APIENTRY *Uniform1i)(GLint location, GLint v0);
	void		(XRAY_APIENTRY *Uniform1f)(GLint location, GLfloat v0);
	void		(XRAY_APIENTRY *Uniform2f)(GLint location, GLfloat v0, GLfloat v1);
	void		(XRAY_APIENTRY *Uniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
	void		(XRAY_APIENTRY *ActiveTexture)(GLenum texture);
} xray_gl_t;

typedef struct
{
	GLint color;
	GLint depth;
	GLint origin;
	GLint texel;
	GLint step;
	GLint range;
	GLint projZ;
	GLint peel;
	GLint edge;
} xray_uniforms_t;

enum xray_phase_e
{
	XRAY_IDLE = 0,
	XRAY_FIRST_VIEW,	// normal view is rendering and has asked the engine for one more
	XRAY_SECOND_VIEW,	// first view is copied out, the peeled view is rendering
};

static const char kVertexShader[] =
	"#version 110\n"
	"void main()\n"
	"{\n"
	"	gl_Position = gl_Vertex;\n"
	"}\n";

// u_peel = fade start, peel distance, viewmodel depth limit, surface opacity.
// u_edge = crease threshold, depth noise floor, far / (far - near), edge opacity.
static const char kFragmentShader[] =
	"#version 110\n"
	"uniform sampler2D u_color;\n"
	"uniform sampler2D u_depth;\n"
	"uniform vec2 u_origin;\n"
	"uniform vec2 u_texel;\n"
	"uniform float u_step;\n"
	"uniform vec2 u_range;\n"
	"uniform vec2 u_projZ;\n"
	"uniform vec4 u_peel;\n"
	"uniform vec4 u_edge;\n"
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
	"	float da = Depth(a);\n"
	"	float db = Depth(b);\n"
	"	if (min(da, db) <= u_peel.z)\n"
	"		return 0.0;\n"
	"	return abs(da + db - 2.0 * center);\n"
	"}\n"
	"\n"
	"void main()\n"
	"{\n"
	"	vec2 uv = (gl_FragCoord.xy - u_origin) * u_texel;\n"
	"	vec3 color = texture2D(u_color, uv).rgb;\n"
	"	float center = Depth(uv);\n"
	"	float opacity = 1.0;\n"
	"	if (center > u_peel.z)\n"
	"	{\n"
	"		float dist = u_projZ.y / (center * 2.0 - 1.0 + u_projZ.x);\n"
	"		float bend = max(max(Bend(uv, center, vec2(u_step, 0.0)), Bend(uv, center, vec2(0.0, u_step))),\n"
	"			max(Bend(uv, center, vec2(u_step, u_step)), Bend(uv, center, vec2(u_step, -u_step))));\n"
	"		float edge = smoothstep(u_edge.x, u_edge.x * 2.0, (bend - u_edge.y) / max(u_edge.z - center, 0.000001));\n"
	"		opacity = max(mix(u_peel.w, 1.0, smoothstep(u_peel.x, u_peel.y, dist)), edge * u_edge.w);\n"
	"	}\n"
	"	gl_FragColor = vec4(color, opacity);\n"
	"}\n";

class CXRay
{
public:
	bool BeginSecondView( ref_params_t *pparams );
	void RequestSecondView( ref_params_t *pparams );
	void NormalTriangles( void );
	void TransparentTriangles( void );
	void EndFrame( void );
	bool SecondView( void ) const { return m_iPhase == XRAY_SECOND_VIEW; }
	void BeginStudio( void );
	void EndStudio( void );

private:
	bool Active( void ) const;
	bool Fail( const char *reason );
	bool LoadFunctions( void );
	GLuint Compile( GLenum type, const char *source );
	bool EnsureProgram( void );
	void CaptureFirstView( void );
	bool CopyFirstView( void );
	void StartPeel( void );
	void StopPeel( void );
	void Composite( void );

	xray_gl_t m_GL = {};
	xray_uniforms_t m_Uniforms = {};
	bool m_bFunctionsLoaded = false;
	bool m_bFailed = false;
	GLuint m_Program = 0;

	GLuint m_ColorTexture = 0;
	GLuint m_DepthTexture = 0;
	int m_iTextureWidth = 0;
	int m_iTextureHeight = 0;
	float m_flDepthUlp = 1.0f / 16777215.0f;
	bool m_bVerifyCopy = false;

	int m_iPhase = XRAY_IDLE;
	ref_params_t m_SavedParams = {};

	// First view's 3D state, captured mid-scene because V_CalcRefdef runs outside it.
	bool m_bCaptured = false;
	GLint m_Viewport[4] = {};
	GLfloat m_DepthRange[2] = {};
	float m_ProjZ[2] = {};		// P[10], P[14]
	float m_flPixelSlope = 0.0f;	// view-space tangent covered by one pixel

	bool m_bClipping = false;
	cvar_t *m_pNoVis = NULL;
	bool m_bNoVisLookedUp = false;
	bool m_bNoVisForced = false;
	float m_flSavedNoVis = 0.0f;
};

static CXRay g_XRay;

template <typename T>
static bool XRay_Load( T &fn, const char *name )
{
	fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
	return fn != NULL;
}

// Returns true when a new name had to be generated.
static bool XRay_EnsureTextureName( GLuint &texture )
{
	if (texture && glIsTexture(texture))
		return false;

	glGenTextures(1, &texture);
	return true;
}

// Specifies the texture bound to GL_TEXTURE_2D on the active unit.
static void XRay_SpecifyTexture( GLint internalFormat, GLenum format, GLenum type, int width, int height )
{
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, NULL);
}

bool CXRay::Fail( const char *reason )
{
	gEngfuncs.Con_Printf("xray: %s, effect disabled\n", reason);
	m_bFailed = true;
	return false;
}

bool CXRay::Active( void ) const
{
	// nobackgrounds owns GL_CLIP_PLANE0 and leaves nothing to peel.
	if (m_bFailed || g_fXashEngine || !MutatorEnabled(MUTATOR_XRAY) || MutatorEnabled(MUTATOR_NOBACKGROUNDS))
		return false;

	return IEngineStudio.IsHardware && IEngineStudio.IsHardware() == 1;
}

bool CXRay::LoadFunctions( void )
{
	return XRay_Load(m_GL.CreateShader, "glCreateShader")
		&& XRay_Load(m_GL.ShaderSource, "glShaderSource")
		&& XRay_Load(m_GL.CompileShader, "glCompileShader")
		&& XRay_Load(m_GL.GetShaderiv, "glGetShaderiv")
		&& XRay_Load(m_GL.GetShaderInfoLog, "glGetShaderInfoLog")
		&& XRay_Load(m_GL.DeleteShader, "glDeleteShader")
		&& XRay_Load(m_GL.CreateProgram, "glCreateProgram")
		&& XRay_Load(m_GL.AttachShader, "glAttachShader")
		&& XRay_Load(m_GL.LinkProgram, "glLinkProgram")
		&& XRay_Load(m_GL.GetProgramiv, "glGetProgramiv")
		&& XRay_Load(m_GL.GetProgramInfoLog, "glGetProgramInfoLog")
		&& XRay_Load(m_GL.DeleteProgram, "glDeleteProgram")
		&& XRay_Load(m_GL.IsProgram, "glIsProgram")
		&& XRay_Load(m_GL.UseProgram, "glUseProgram")
		&& XRay_Load(m_GL.GetUniformLocation, "glGetUniformLocation")
		&& XRay_Load(m_GL.Uniform1i, "glUniform1i")
		&& XRay_Load(m_GL.Uniform1f, "glUniform1f")
		&& XRay_Load(m_GL.Uniform2f, "glUniform2f")
		&& XRay_Load(m_GL.Uniform4f, "glUniform4f")
		&& XRay_Load(m_GL.ActiveTexture, "glActiveTexture");
}

GLuint CXRay::Compile( GLenum type, const char *source )
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
		gEngfuncs.Con_Printf("xray: shader compile log:\n%s\n", log);
		m_GL.DeleteShader(shader);
		return 0;
	}

	return shader;
}

bool CXRay::EnsureProgram( void )
{
	if (m_Program && m_GL.IsProgram(m_Program))
		return true;

	if (!m_bFunctionsLoaded)
	{
		if (!LoadFunctions())
			return Fail("OpenGL 2.0 shader entry points not found");
		m_bFunctionsLoaded = true;
	}

	const GLuint vertexShader = Compile(GL_VERTEX_SHADER, kVertexShader);
	const GLuint fragmentShader = vertexShader ? Compile(GL_FRAGMENT_SHADER, kFragmentShader) : 0;
	const GLuint program = fragmentShader ? m_GL.CreateProgram() : 0;
	if (!program)
	{
		if (vertexShader)
			m_GL.DeleteShader(vertexShader);
		if (fragmentShader)
			m_GL.DeleteShader(fragmentShader);
		return Fail("blend shader did not compile");
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
		gEngfuncs.Con_Printf("xray: shader link log:\n%s\n", log);
		m_GL.DeleteProgram(program);
		return Fail("blend shader did not link");
	}

	m_Uniforms.color = m_GL.GetUniformLocation(program, "u_color");
	m_Uniforms.depth = m_GL.GetUniformLocation(program, "u_depth");
	m_Uniforms.origin = m_GL.GetUniformLocation(program, "u_origin");
	m_Uniforms.texel = m_GL.GetUniformLocation(program, "u_texel");
	m_Uniforms.step = m_GL.GetUniformLocation(program, "u_step");
	m_Uniforms.range = m_GL.GetUniformLocation(program, "u_range");
	m_Uniforms.projZ = m_GL.GetUniformLocation(program, "u_projZ");
	m_Uniforms.peel = m_GL.GetUniformLocation(program, "u_peel");
	m_Uniforms.edge = m_GL.GetUniformLocation(program, "u_edge");

	m_Program = program;
	return true;
}

// From HUD_DrawNormalTriangles of the first view, while its projection is current.
void CXRay::CaptureFirstView( void )
{
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	if (viewport[2] <= 0 || viewport[3] <= 0)
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
	m_flPixelSlope = 2.0f / (fabsf(projection[0]) * (float)viewport[2]);
	m_bCaptured = true;
}

// Copies the finished first view (colour and depth) and clears its colour to the void.
bool CXRay::CopyFirstView( void )
{
	const int x = m_Viewport[0];
	const int y = m_Viewport[1];
	const int width = m_Viewport[2];
	const int height = m_Viewport[3];

	const bool resized = width != m_iTextureWidth || height != m_iTextureHeight;
	const bool newColor = XRay_EnsureTextureName(m_ColorTexture);
	const bool newDepth = XRay_EnsureTextureName(m_DepthTexture);
	if (resized || newColor || newDepth)
		m_bVerifyCopy = true;

	m_iTextureWidth = width;
	m_iTextureHeight = height;

	GLint previousUnit = GL_TEXTURE0;
	GLint previousUnit1Texture = 0;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &previousUnit);

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT | GL_SCISSOR_BIT);
	m_GL.ActiveTexture(GL_TEXTURE1);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousUnit1Texture);

	// Multisampled or depth-less read buffers reject the copy; check once per allocation.
	if (m_bVerifyCopy)
	{
		for (int i = 0; i < XRAY_MAX_GL_ERRORS && glGetError() != GL_NO_ERROR; i++)
			;
	}

	glBindTexture(GL_TEXTURE_2D, m_ColorTexture);
	if (resized || newColor)
		XRay_SpecifyTexture(GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, width, height);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, width, height);

	m_GL.ActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_DepthTexture);
	if (resized || newDepth)
	{
		XRay_SpecifyTexture(GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, width, height);

		GLint bits = 24;
		glGetIntegerv(GL_DEPTH_BITS, &bits);
		if (bits < 16 || bits > 24)
			bits = 24;
		m_flDepthUlp = 1.0f / (float)((1 << bits) - 1);
	}
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, width, height);

	bool copied = true;
	if (m_bVerifyCopy)
	{
		copied = glGetError() == GL_NO_ERROR;
		m_bVerifyCopy = !copied;
	}

	if (copied)
	{
		// Where the second view finds no geometry it shows black void, like outside the N64's rooms.
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, width, height);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	// The engine caches the binding of every unit it uses.
	m_GL.ActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, (GLuint)previousUnit1Texture);

	glPopAttrib();
	m_GL.ActiveTexture((GLenum)previousUnit);

	return copied ? true : Fail("frame buffer could not be copied");
}

void CXRay::StartPeel( void )
{
	// Specified under an identity modelview, the plane stays in eye space: keep what is at least the peel distance ahead.
	const GLdouble plane[4] = { 0.0, 0.0, -1.0, -XRAY_PEEL_DISTANCE };

	GLint matrixMode = GL_MODELVIEW;
	glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glClipPlane(GL_CLIP_PLANE0, plane);
	glPopMatrix();
	glMatrixMode((GLenum)matrixMode);

	glEnable(GL_CLIP_PLANE0);
	m_bClipping = true;

	if (!m_bNoVisLookedUp)
	{
		m_pNoVis = gEngfuncs.pfnGetCvarPointer("r_novis");
		m_bNoVisLookedUp = true;
	}

	// Without it only leaves in the PVS are drawn and most walls would hide nothing but void.
	// Written directly: the renderer reads .value, and this only lasts until the world is drawn.
	if (m_pNoVis)
	{
		m_flSavedNoVis = m_pNoVis->value;
		m_pNoVis->value = 1.0f;
		m_bNoVisForced = true;
	}
}

void CXRay::StopPeel( void )
{
	if (m_bClipping)
	{
		glDisable(GL_CLIP_PLANE0);
		m_bClipping = false;
	}

	if (m_bNoVisForced)
	{
		m_pNoVis->value = m_flSavedNoVis;
		m_bNoVisForced = false;
	}
}

// Called first in V_CalcRefdef. Returns true for the extra view this mutator asked for.
bool CXRay::BeginSecondView( ref_params_t *pparams )
{
	if (m_iPhase == XRAY_FIRST_VIEW && pparams->nextView)
	{
		// Same view again; V_CalcNormalRefdef must not run twice a frame (punch decay, bob, smoothing).
		*pparams = m_SavedParams;
		pparams->nextView = 0;
		pparams->onlyClientDraw = 0;

		m_iPhase = XRAY_IDLE;
		if (m_bCaptured && Active() && CopyFirstView())
		{
			StartPeel();
			m_iPhase = XRAY_SECOND_VIEW;
		}
		return true;
	}

	// A view that was asked for never came; never let the peel leak into another frame.
	StopPeel();
	m_iPhase = XRAY_IDLE;
	return false;
}

// Called last in V_CalcRefdef, once the view of this frame is final.
void CXRay::RequestSecondView( ref_params_t *pparams )
{
	// Spectator and HLTV views run their own nextView inset.
	if (pparams->nextView || pparams->intermission || pparams->spectator || pparams->onlyClientDraw || g_iUser1 || gEngfuncs.IsSpectateOnly())
		return;

	if (!Active() || !EnsureProgram())
		return;

	m_SavedParams = *pparams;
	m_bCaptured = false;
	m_iPhase = XRAY_FIRST_VIEW;
	pparams->nextView = 1;
}

void CXRay::NormalTriangles( void )
{
	if (m_iPhase == XRAY_FIRST_VIEW)
	{
		CaptureFirstView();
	}
	else if (m_iPhase == XRAY_SECOND_VIEW)
	{
		// Only the world and solid brush entities have been drawn; transparent entities show in both views.
		StopPeel();
	}
}

void CXRay::TransparentTriangles( void )
{
	if (m_iPhase != XRAY_SECOND_VIEW)
		return;

	m_iPhase = XRAY_IDLE;
	StopPeel();
	Composite();
}

void CXRay::EndFrame( void )
{
	StopPeel();
	m_iPhase = XRAY_IDLE;
}

void CXRay::BeginStudio( void )
{
	if (m_bClipping)
		glDisable(GL_CLIP_PLANE0);
}

void CXRay::EndStudio( void )
{
	if (m_bClipping)
		glEnable(GL_CLIP_PLANE0);
}

// Blends the first view over the peeled one, before the viewmodel and before any screen fade.
void CXRay::Composite( void )
{
	if (!EnsureProgram())
		return;

	const int x = m_Viewport[0];
	const int y = m_Viewport[1];
	const int width = m_Viewport[2];
	const int height = m_Viewport[3];

	float step = floorf((float)height / XRAY_LINE_SCALE + 0.5f);
	if (step < 1.0f)
		step = 1.0f;

	const float rangeSpan = fabsf(m_DepthRange[1] - m_DepthRange[0]);

	GLint previousProgram = 0;
	GLint previousUnit = GL_TEXTURE0;
	GLint previousUnit1Texture = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &previousUnit);

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT | GL_VIEWPORT_BIT | GL_POLYGON_BIT | GL_SCISSOR_BIT);
	m_GL.ActiveTexture(GL_TEXTURE1);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousUnit1Texture);
	glBindTexture(GL_TEXTURE_2D, m_ColorTexture);
	m_GL.ActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_DepthTexture);

	glViewport(x, y, width, height);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	m_GL.UseProgram(m_Program);
	m_GL.Uniform1i(m_Uniforms.depth, 0);
	m_GL.Uniform1i(m_Uniforms.color, 1);
	m_GL.Uniform2f(m_Uniforms.origin, (float)x, (float)y);
	m_GL.Uniform2f(m_Uniforms.texel, 1.0f / (float)width, 1.0f / (float)height);
	m_GL.Uniform1f(m_Uniforms.step, step);
	m_GL.Uniform2f(m_Uniforms.range, m_DepthRange[0], m_DepthRange[1]);
	m_GL.Uniform2f(m_Uniforms.projZ, m_ProjZ[0], m_ProjZ[1]);
	m_GL.Uniform4f(m_Uniforms.peel, XRAY_PEEL_DISTANCE * XRAY_FADE_START, XRAY_PEEL_DISTANCE, XRAY_VIEWMODEL_DEPTH, XRAY_SURFACE_ALPHA);
	m_GL.Uniform4f(m_Uniforms.edge, XRAY_CREASE_SLOPE * step * m_flPixelSlope, XRAY_DEPTH_ULPS * m_flDepthUlp / rangeSpan,
		(1.0f - m_ProjZ[0]) * 0.5f, XRAY_EDGE_ALPHA);

	glBegin(GL_QUADS);
	glVertex2f(-1.0f, -1.0f);
	glVertex2f(1.0f, -1.0f);
	glVertex2f(1.0f, 1.0f);
	glVertex2f(-1.0f, 1.0f);
	glEnd();

	m_GL.UseProgram((GLuint)previousProgram);

	// The engine caches the binding of every unit it uses.
	m_GL.ActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, (GLuint)previousUnit1Texture);

	glPopAttrib();
	m_GL.ActiveTexture((GLenum)previousUnit);
}

bool XRay_BeginSecondView( struct ref_params_s *pparams )
{
	return g_XRay.BeginSecondView(pparams);
}

void XRay_RequestSecondView( struct ref_params_s *pparams )
{
	g_XRay.RequestSecondView(pparams);
}

void XRay_NormalTriangles( void )
{
	g_XRay.NormalTriangles();
}

void XRay_TransparentTriangles( void )
{
	g_XRay.TransparentTriangles();
}

void XRay_EndFrame( void )
{
	g_XRay.EndFrame();
}

bool XRay_SecondView( void )
{
	return g_XRay.SecondView();
}

void XRay_BeginStudio( void )
{
	g_XRay.BeginStudio();
}

void XRay_EndStudio( void )
{
	g_XRay.EndStudio();
}

#else

bool XRay_BeginSecondView( struct ref_params_s *pparams )
{
	return false;
}

void XRay_RequestSecondView( struct ref_params_s *pparams )
{
}

void XRay_NormalTriangles( void )
{
}

void XRay_TransparentTriangles( void )
{
}

void XRay_EndFrame( void )
{
}

bool XRay_SecondView( void )
{
	return false;
}

void XRay_BeginStudio( void )
{
}

void XRay_EndStudio( void )
{
}

#endif
