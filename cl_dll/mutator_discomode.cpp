//
// mutator_discomode.cpp
//
// "discomode" mutator - client side
//
// After the GoldenEye 007 GameShark code that made the game "flash rapidly
// with shifting, psychedelic colors". The N64 lit every polygon with
// TEXEL0 * SHADE, so scrambling the shade colours repaints each polygon in a
// bright solid colour that changes every frame. Once the 3D view is finished,
// the depth buffer is unprojected to world space and each pixel's plane
// (normal and distance) picks a fully saturated hue, so every flat polygon gets
// one solid colour, modulated by the brightness of what was drawn there. The
// palette is re-rolled on a fixed beat, cycles between beats, and some beats
// sweep a world-space rainbow across the surfaces. Runs before the HUD, so the
// HUD draws normally on top.
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
#define DISCOMODE_APIENTRY __stdcall
#else
#define DISCOMODE_APIENTRY
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

#define DISCOMODE_BEAT_HZ			10.0f	// palette re-rolls per second, hard cuts like a per-frame N64 code
#define DISCOMODE_CYCLE_HZ			0.75f	// hue turns per second between beats
#define DISCOMODE_FLASH				0.35f	// extra brightness at the start of each beat, fades out within the beat
#define DISCOMODE_RAINBOW_CHANCE	0.5f	// share of beats that sweep a rainbow across the surfaces
#define DISCOMODE_DESATURATE		0.75f	// share of the scene's own colour replaced by its brightness
#define DISCOMODE_DETAIL			0.5f	// 0 = flat solid colours, 1 = full texture and lighting detail
#define DISCOMODE_AXIS_SNAP			0.996f	// world normals this close to an axis snap to it, so axial planes stay solid
#define DISCOMODE_SAMPLE_SCALE		720.0f	// viewport rows per pixel of normal sample offset
#define DISCOMODE_MAX_GL_ERRORS		32

typedef struct
{
	GLuint		(DISCOMODE_APIENTRY *CreateShader)(GLenum type);
	void		(DISCOMODE_APIENTRY *ShaderSource)(GLuint shader, GLsizei count, const char *const *strings, const GLint *lengths);
	void		(DISCOMODE_APIENTRY *CompileShader)(GLuint shader);
	void		(DISCOMODE_APIENTRY *GetShaderiv)(GLuint shader, GLenum pname, GLint *params);
	void		(DISCOMODE_APIENTRY *GetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei *length, char *infoLog);
	void		(DISCOMODE_APIENTRY *DeleteShader)(GLuint shader);
	GLuint		(DISCOMODE_APIENTRY *CreateProgram)(void);
	void		(DISCOMODE_APIENTRY *AttachShader)(GLuint program, GLuint shader);
	void		(DISCOMODE_APIENTRY *LinkProgram)(GLuint program);
	void		(DISCOMODE_APIENTRY *GetProgramiv)(GLuint program, GLenum pname, GLint *params);
	void		(DISCOMODE_APIENTRY *GetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei *length, char *infoLog);
	void		(DISCOMODE_APIENTRY *DeleteProgram)(GLuint program);
	GLboolean	(DISCOMODE_APIENTRY *IsProgram)(GLuint program);
	void		(DISCOMODE_APIENTRY *UseProgram)(GLuint program);
	GLint		(DISCOMODE_APIENTRY *GetUniformLocation)(GLuint program, const char *name);
	void		(DISCOMODE_APIENTRY *Uniform1i)(GLint location, GLint v0);
	void		(DISCOMODE_APIENTRY *Uniform1f)(GLint location, GLfloat v0);
	void		(DISCOMODE_APIENTRY *Uniform2f)(GLint location, GLfloat v0, GLfloat v1);
	void		(DISCOMODE_APIENTRY *Uniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
	void		(DISCOMODE_APIENTRY *Uniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
	void		(DISCOMODE_APIENTRY *ActiveTexture)(GLenum texture);
} discomode_gl_t;

typedef struct
{
	GLint color;
	GLint depth;
	GLint origin;
	GLint texel;
	GLint step;
	GLint range;
	GLint proj;
	GLint projZ;
	GLint axisX;
	GLint axisY;
	GLint axisZ;
	GLint shift;
	GLint palette;
	GLint sweep;
	GLint phase;
	GLint gain;
	GLint look;
} discomode_uniforms_t;

static const char kVertexShader[] =
	"#version 110\n"
	"void main()\n"
	"{\n"
	"	gl_Position = gl_Vertex;\n"
	"}\n";

// u_axisX/Y/Z are the world axes in view space (modelview rotation columns), so
// dot(v, u_axis*) takes a view-space vector to world space. u_shift is the eye
// relative to the beat's anchor, which keeps coordinates small and colours steady for the beat.
static const char kFragmentShader[] =
	"#version 110\n"
	"uniform sampler2D u_color;\n"
	"uniform sampler2D u_depth;\n"
	"uniform vec2 u_origin;\n"
	"uniform vec2 u_texel;\n"
	"uniform float u_step;\n"
	"uniform vec2 u_range;\n"
	"uniform vec4 u_proj;\n"
	"uniform vec2 u_projZ;\n"
	"uniform vec3 u_axisX;\n"
	"uniform vec3 u_axisY;\n"
	"uniform vec3 u_axisZ;\n"
	"uniform vec3 u_shift;\n"
	"uniform vec4 u_palette;\n"
	"uniform vec4 u_sweep;\n"
	"uniform float u_phase;\n"
	"uniform float u_gain;\n"
	"uniform vec3 u_look;\n"
	"\n"
	"float Depth(vec2 uv)\n"
	"{\n"
	"	return clamp((texture2D(u_depth, uv).r - u_range.x) / (u_range.y - u_range.x), 0.0, 1.0);\n"
	"}\n"
	"\n"
	"vec3 ViewPos(vec2 uv, float depth)\n"
	"{\n"
	"	float z = -u_projZ.y / (depth * 2.0 - 1.0 + u_projZ.x);\n"
	"	vec2 ndc = uv * 2.0 - 1.0;\n"
	"	return vec3(-z * (ndc.x + u_proj.z) / u_proj.x, -z * (ndc.y + u_proj.w) / u_proj.y, z);\n"
	"}\n"
	"\n"
	"vec3 Tangent(vec2 uv, float center, vec3 pos, vec2 offset)\n"
	"{\n"
	"	float behind = Depth(uv - offset);\n"
	"	float ahead = Depth(uv + offset);\n"
	"	float behindError = abs(2.0 * behind - Depth(uv - 2.0 * offset) - center);\n"
	"	float aheadError = abs(2.0 * ahead - Depth(uv + 2.0 * offset) - center);\n"
	"	if (aheadError < behindError)\n"
	"		return ViewPos(uv + offset, ahead) - pos;\n"
	"	return pos - ViewPos(uv - offset, behind);\n"
	"}\n"
	"\n"
	"vec3 Hue(float h)\n"
	"{\n"
	"	return clamp(abs(fract(h + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);\n"
	"}\n"
	"\n"
	"void main()\n"
	"{\n"
	"	vec2 uv = (gl_FragCoord.xy - u_origin) * u_texel;\n"
	"	float center = Depth(uv);\n"
	"	vec3 pos = ViewPos(uv, center);\n"
	"	vec3 n = cross(Tangent(uv, center, pos, vec2(u_step * u_texel.x, 0.0)), Tangent(uv, center, pos, vec2(0.0, u_step * u_texel.y)));\n"
	"	float len = length(n);\n"
	"	n = len > 0.0 ? n / len : vec3(0.0, 0.0, 1.0);\n"
	"\n"
	"	vec3 normal = vec3(dot(n, u_axisX), dot(n, u_axisY), dot(n, u_axisZ));\n"
	"	vec3 rel = vec3(dot(pos, u_axisX), dot(pos, u_axisY), dot(pos, u_axisZ)) + u_shift;\n"
	"	vec3 a = abs(normal);\n"
	"	float m = max(a.x, max(a.y, a.z));\n"
	"	if (m > u_look.z)\n"
	"		normal = sign(normal) * step(vec3(m), a);\n"
	"\n"
	"	float hue = u_phase + dot(normal, u_palette.xyz) + dot(normal, rel) * u_palette.w + dot(rel, u_sweep.xyz) * u_sweep.w;\n"
	"	vec3 scene = texture2D(u_color, uv).rgb;\n"
	"	vec3 texel = mix(scene, vec3(dot(scene, vec3(0.299, 0.587, 0.114))), u_look.x);\n"
	"	vec3 base = mix(vec3(0.5), texel, u_look.y);\n"
	"	gl_FragColor = vec4(clamp(base * Hue(hue) * u_gain, 0.0, 1.0), 1.0);\n"
	"}\n";

class CDiscoMode
{
public:
	void CaptureScene( void );
	void Draw( void );

private:
	bool Active( void ) const;
	bool Fail( const char *reason );
	bool LoadFunctions( void );
	GLuint Compile( GLenum type, const char *source );
	bool EnsureProgram( void );
	bool CopyScene( int x, int y, int width, int height );
	void RollBeat( const float *eye );

	discomode_gl_t m_GL = {};
	discomode_uniforms_t m_Uniforms = {};
	bool m_bFunctionsLoaded = false;
	bool m_bFailed = false;
	GLuint m_Program = 0;

	GLuint m_ColorTexture = 0;
	GLuint m_DepthTexture = 0;
	int m_iTextureWidth = 0;
	int m_iTextureHeight = 0;
	bool m_bVerifyCopy = false;

	// 3D view state, captured mid-scene because HUD_Redraw runs in 2D state.
	bool m_bCaptured = false;
	GLint m_Viewport[4] = {};
	GLfloat m_DepthRange[2] = {};
	GLfloat m_Projection[16] = {};
	GLfloat m_ModelView[16] = {};

	// Palette of the current beat.
	double m_flBeat = -1.0;
	float m_Palette[4] = {};	// xyz = hue per world normal axis, w = hue per unit of plane distance
	float m_Sweep[4] = {};		// xyz = rainbow direction / wavelength, w = rainbow amount
	float m_flBeatPhase = 0.0f;
	float m_Anchor[3] = {};
};

static CDiscoMode g_DiscoMode;

template <typename T>
static bool DiscoMode_Load( T &fn, const char *name )
{
	fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
	return fn != NULL;
}

// Returns true when a new name had to be generated.
static bool DiscoMode_EnsureTextureName( GLuint &texture )
{
	if (texture && glIsTexture(texture))
		return false;

	glGenTextures(1, &texture);
	return true;
}

// Specifies the texture bound to GL_TEXTURE_2D on the active unit.
static void DiscoMode_SpecifyTexture( GLint internalFormat, GLenum format, GLenum type, int width, int height )
{
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, NULL);
}

bool CDiscoMode::Fail( const char *reason )
{
	gEngfuncs.Con_Printf("discomode: %s, effect disabled\n", reason);
	m_bFailed = true;
	return false;
}

bool CDiscoMode::Active( void ) const
{
	if (m_bFailed || g_fXashEngine || !MutatorEnabled(MUTATOR_DISCOMODE))
		return false;

	return IEngineStudio.IsHardware && IEngineStudio.IsHardware() == 1;
}

bool CDiscoMode::LoadFunctions( void )
{
	return DiscoMode_Load(m_GL.CreateShader, "glCreateShader")
		&& DiscoMode_Load(m_GL.ShaderSource, "glShaderSource")
		&& DiscoMode_Load(m_GL.CompileShader, "glCompileShader")
		&& DiscoMode_Load(m_GL.GetShaderiv, "glGetShaderiv")
		&& DiscoMode_Load(m_GL.GetShaderInfoLog, "glGetShaderInfoLog")
		&& DiscoMode_Load(m_GL.DeleteShader, "glDeleteShader")
		&& DiscoMode_Load(m_GL.CreateProgram, "glCreateProgram")
		&& DiscoMode_Load(m_GL.AttachShader, "glAttachShader")
		&& DiscoMode_Load(m_GL.LinkProgram, "glLinkProgram")
		&& DiscoMode_Load(m_GL.GetProgramiv, "glGetProgramiv")
		&& DiscoMode_Load(m_GL.GetProgramInfoLog, "glGetProgramInfoLog")
		&& DiscoMode_Load(m_GL.DeleteProgram, "glDeleteProgram")
		&& DiscoMode_Load(m_GL.IsProgram, "glIsProgram")
		&& DiscoMode_Load(m_GL.UseProgram, "glUseProgram")
		&& DiscoMode_Load(m_GL.GetUniformLocation, "glGetUniformLocation")
		&& DiscoMode_Load(m_GL.Uniform1i, "glUniform1i")
		&& DiscoMode_Load(m_GL.Uniform1f, "glUniform1f")
		&& DiscoMode_Load(m_GL.Uniform2f, "glUniform2f")
		&& DiscoMode_Load(m_GL.Uniform3f, "glUniform3f")
		&& DiscoMode_Load(m_GL.Uniform4f, "glUniform4f")
		&& DiscoMode_Load(m_GL.ActiveTexture, "glActiveTexture");
}

GLuint CDiscoMode::Compile( GLenum type, const char *source )
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
		gEngfuncs.Con_Printf("discomode: shader compile log:\n%s\n", log);
		m_GL.DeleteShader(shader);
		return 0;
	}

	return shader;
}

bool CDiscoMode::EnsureProgram( void )
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
		return Fail("color shader did not compile");
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
		gEngfuncs.Con_Printf("discomode: shader link log:\n%s\n", log);
		m_GL.DeleteProgram(program);
		return Fail("color shader did not link");
	}

	m_Uniforms.color = m_GL.GetUniformLocation(program, "u_color");
	m_Uniforms.depth = m_GL.GetUniformLocation(program, "u_depth");
	m_Uniforms.origin = m_GL.GetUniformLocation(program, "u_origin");
	m_Uniforms.texel = m_GL.GetUniformLocation(program, "u_texel");
	m_Uniforms.step = m_GL.GetUniformLocation(program, "u_step");
	m_Uniforms.range = m_GL.GetUniformLocation(program, "u_range");
	m_Uniforms.proj = m_GL.GetUniformLocation(program, "u_proj");
	m_Uniforms.projZ = m_GL.GetUniformLocation(program, "u_projZ");
	m_Uniforms.axisX = m_GL.GetUniformLocation(program, "u_axisX");
	m_Uniforms.axisY = m_GL.GetUniformLocation(program, "u_axisY");
	m_Uniforms.axisZ = m_GL.GetUniformLocation(program, "u_axisZ");
	m_Uniforms.shift = m_GL.GetUniformLocation(program, "u_shift");
	m_Uniforms.palette = m_GL.GetUniformLocation(program, "u_palette");
	m_Uniforms.sweep = m_GL.GetUniformLocation(program, "u_sweep");
	m_Uniforms.phase = m_GL.GetUniformLocation(program, "u_phase");
	m_Uniforms.gain = m_GL.GetUniformLocation(program, "u_gain");
	m_Uniforms.look = m_GL.GetUniformLocation(program, "u_look");

	m_Program = program;
	return true;
}

// Leaves the scene colour bound on GL_TEXTURE1 and the depth on GL_TEXTURE0 (active).
bool CDiscoMode::CopyScene( int x, int y, int width, int height )
{
	const bool resized = width != m_iTextureWidth || height != m_iTextureHeight;
	const bool newColor = DiscoMode_EnsureTextureName(m_ColorTexture);
	const bool newDepth = DiscoMode_EnsureTextureName(m_DepthTexture);
	if (resized || newColor || newDepth)
		m_bVerifyCopy = true;

	m_iTextureWidth = width;
	m_iTextureHeight = height;

	// Multisampled or depth-less read buffers reject the copy; check once per allocation.
	if (m_bVerifyCopy)
	{
		for (int i = 0; i < DISCOMODE_MAX_GL_ERRORS && glGetError() != GL_NO_ERROR; i++)
			;
	}

	m_GL.ActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_ColorTexture);
	if (resized || newColor)
		DiscoMode_SpecifyTexture(GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, width, height);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, width, height);

	m_GL.ActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_DepthTexture);
	if (resized || newDepth)
		DiscoMode_SpecifyTexture(GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, width, height);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, x, y, width, height);

	if (m_bVerifyCopy)
	{
		if (glGetError() != GL_NO_ERROR)
			return Fail("frame buffer could not be copied");
		m_bVerifyCopy = false;
	}

	return true;
}

void CDiscoMode::RollBeat( const float *eye )
{
	// Odd twelfths put the six axial faces of a box on six different hues, whatever the order and signs.
	static const float kAxisHues[3] = { 1.0f / 12.0f, 3.0f / 12.0f, 5.0f / 12.0f };

	int order[3] = { 0, 1, 2 };
	for (int i = 2; i > 0; i--)
	{
		const int j = gEngfuncs.pfnRandomLong(0, i);
		const int swap = order[i];
		order[i] = order[j];
		order[j] = swap;
	}

	const float frequency = gEngfuncs.pfnRandomLong(0, 1) ? 5.0f : 1.0f;
	for (int i = 0; i < 3; i++)
		m_Palette[i] = kAxisHues[order[i]] * frequency * (gEngfuncs.pfnRandomLong(0, 1) ? 1.0f : -1.0f);
	m_Palette[3] = gEngfuncs.pfnRandomFloat(1.0f / 256.0f, 1.0f / 48.0f) * (gEngfuncs.pfnRandomLong(0, 1) ? 1.0f : -1.0f);

	const float yaw = gEngfuncs.pfnRandomFloat(0.0f, 6.2831853f);
	const float pitch = gEngfuncs.pfnRandomFloat(-0.6f, 0.6f);
	const float wavelength = gEngfuncs.pfnRandomFloat(128.0f, 512.0f);
	m_Sweep[0] = cosf(pitch) * cosf(yaw) / wavelength;
	m_Sweep[1] = cosf(pitch) * sinf(yaw) / wavelength;
	m_Sweep[2] = sinf(pitch) / wavelength;
	m_Sweep[3] = gEngfuncs.pfnRandomFloat(0.0f, 1.0f) < DISCOMODE_RAINBOW_CHANCE ? 1.0f : 0.0f;

	m_flBeatPhase = gEngfuncs.pfnRandomFloat(0.0f, 1.0f);
	memcpy(m_Anchor, eye, sizeof(m_Anchor));
}

// Called from HUD_DrawNormalTriangles while the world projection and view matrix are current.
void CDiscoMode::CaptureScene( void )
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
	if (projection[11] > -0.5f || fabsf(projection[0]) < 0.0001f || fabsf(projection[5]) < 0.0001f)
		return;

	GLfloat range[2];
	glGetFloatv(GL_DEPTH_RANGE, range);
	if (fabsf(range[1] - range[0]) < 0.01f)
		return;

	memcpy(m_Viewport, viewport, sizeof(m_Viewport));
	memcpy(m_Projection, projection, sizeof(m_Projection));
	glGetFloatv(GL_MODELVIEW_MATRIX, m_ModelView);
	m_DepthRange[0] = range[0];
	m_DepthRange[1] = range[1];
	m_bCaptured = true;
}

// Called at the top of HUD_Redraw: the finished 3D frame is in the back buffer, the HUD is not.
void CDiscoMode::Draw( void )
{
	const bool captured = m_bCaptured;
	m_bCaptured = false;

	if (!captured || !Active() || !EnsureProgram())
		return;

	const int x = m_Viewport[0];
	const int y = m_Viewport[1];
	const int width = m_Viewport[2];
	const int height = m_Viewport[3];

	// Modelview columns are the world axes in view space; the eye is -R^T * t.
	const float *axisX = &m_ModelView[0];
	const float *axisY = &m_ModelView[4];
	const float *axisZ = &m_ModelView[8];
	const float *t = &m_ModelView[12];
	const float eye[3] =
	{
		-(axisX[0] * t[0] + axisX[1] * t[1] + axisX[2] * t[2]),
		-(axisY[0] * t[0] + axisY[1] * t[1] + axisY[2] * t[2]),
		-(axisZ[0] * t[0] + axisZ[1] * t[1] + axisZ[2] * t[2])
	};

	const double now = gEngfuncs.GetAbsoluteTime();
	const double beat = floor(now * DISCOMODE_BEAT_HZ);
	if (beat != m_flBeat)
	{
		m_flBeat = beat;
		RollBeat(eye);
	}

	const float beatFade = 1.0f - (float)(now * DISCOMODE_BEAT_HZ - beat);
	const float gain = 2.0f * (1.0f + DISCOMODE_FLASH * beatFade * beatFade);
	const float phase = m_flBeatPhase + (float)fmod(now * DISCOMODE_CYCLE_HZ, 1.0);

	float step = floorf((float)height / DISCOMODE_SAMPLE_SCALE + 0.5f);
	if (step < 1.0f)
		step = 1.0f;

	GLint previousProgram = 0;
	GLint previousUnit = GL_TEXTURE0;
	GLint previousUnit1Texture = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &previousUnit);

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT | GL_VIEWPORT_BIT | GL_POLYGON_BIT | GL_SCISSOR_BIT);
	m_GL.ActiveTexture(GL_TEXTURE1);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousUnit1Texture);

	if (CopyScene(x, y, width, height))
	{
		glViewport(x, y, width, height);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glDisable(GL_BLEND);
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
		m_GL.Uniform4f(m_Uniforms.proj, m_Projection[0], m_Projection[5], m_Projection[8], m_Projection[9]);
		m_GL.Uniform2f(m_Uniforms.projZ, m_Projection[10], m_Projection[14]);
		m_GL.Uniform3f(m_Uniforms.axisX, axisX[0], axisX[1], axisX[2]);
		m_GL.Uniform3f(m_Uniforms.axisY, axisY[0], axisY[1], axisY[2]);
		m_GL.Uniform3f(m_Uniforms.axisZ, axisZ[0], axisZ[1], axisZ[2]);
		m_GL.Uniform3f(m_Uniforms.shift, eye[0] - m_Anchor[0], eye[1] - m_Anchor[1], eye[2] - m_Anchor[2]);
		m_GL.Uniform4f(m_Uniforms.palette, m_Palette[0], m_Palette[1], m_Palette[2], m_Palette[3]);
		m_GL.Uniform4f(m_Uniforms.sweep, m_Sweep[0], m_Sweep[1], m_Sweep[2], m_Sweep[3]);
		m_GL.Uniform1f(m_Uniforms.phase, phase);
		m_GL.Uniform1f(m_Uniforms.gain, gain);
		m_GL.Uniform3f(m_Uniforms.look, DISCOMODE_DESATURATE, DISCOMODE_DETAIL, DISCOMODE_AXIS_SNAP);

		glBegin(GL_QUADS);
		glVertex2f(-1.0f, -1.0f);
		glVertex2f(1.0f, -1.0f);
		glVertex2f(1.0f, 1.0f);
		glVertex2f(-1.0f, 1.0f);
		glEnd();

		m_GL.UseProgram((GLuint)previousProgram);
	}

	// The engine caches the binding of every unit it uses.
	m_GL.ActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, (GLuint)previousUnit1Texture);

	glPopAttrib();
	m_GL.ActiveTexture((GLenum)previousUnit);
}

void DiscoMode_CaptureScene( void )
{
	g_DiscoMode.CaptureScene();
}

void DiscoMode_Draw( void )
{
	g_DiscoMode.Draw();
}

#else

void DiscoMode_CaptureScene( void )
{
}

void DiscoMode_Draw( void )
{
}

#endif
