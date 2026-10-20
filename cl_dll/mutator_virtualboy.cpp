//
// mutator_virtualboy.cpp
//
// "virtualboy" mutator - client side
//
// The game as seen through a Nintendo Virtual Boy: a 384x224 display of red
// LEDs on black with four shades (off and three brightness levels). Once the
// 3D view is finished, it is redrawn on a grid of square LED cells about 224
// rows high: each cell takes the dithered brightness of the scene behind it,
// and geometric edges (the linemode depth test, at cell scale) light up as
// bright red wires that dim with distance. After the HUD is drawn, a second
// pass turns the whole window red monochrome, so the HUD matches.
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
#define VIRTUALBOY_APIENTRY __stdcall
#else
#define VIRTUALBOY_APIENTRY
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

#define VIRTUALBOY_ROWS				224.0f	// LED rows of the real display; cells stay square and whole pixels
#define VIRTUALBOY_GAIN				1.4f	// scene brightness boost before quantizing, most maps are dark
#define VIRTUALBOY_GAMMA			0.8f	// below 1 lifts the shadows
#define VIRTUALBOY_DITHER			1.0f	// 0 = plain rounding, 1 = full 4x4 ordered dither between shades
#define VIRTUALBOY_FILL_LEVELS		2.0f	// brightest shade a surface can reach, wires use the one above
#define VIRTUALBOY_SHADE_1			0.30f	// LED brightness of the three lit shades
#define VIRTUALBOY_SHADE_2			0.60f
#define VIRTUALBOY_SHADE_3			1.00f
#define VIRTUALBOY_CREASE_SLOPE		0.5f	// change in screen-space surface slope (tan) where a wire starts
#define VIRTUALBOY_DEPTH_ULPS		6.0f	// depth buffer quantization steps ignored as noise
#define VIRTUALBOY_WIRE_FADE_START	512.0f	// units; wires dim from the brightest shade here ...
#define VIRTUALBOY_WIRE_FADE_END	2048.0f	// ... to VIRTUALBOY_WIRE_FAR_LEVEL here
#define VIRTUALBOY_WIRE_FAR_LEVEL	2.0f
#define VIRTUALBOY_ROW_GAP			0.55f	// darkening of the gap between LED rows
#define VIRTUALBOY_COLUMN_GAP		0.25f	// darkening of the gap between mirror scan columns
#define VIRTUALBOY_MIN_GAP_CELL		3.0f	// cells smaller than this many pixels draw no gaps
#define VIRTUALBOY_VIGNETTE			0.5f	// corner darkening, the inside of the visor
#define VIRTUALBOY_MAX_GL_ERRORS	32

typedef struct
{
	GLuint		(VIRTUALBOY_APIENTRY *CreateShader)(GLenum type);
	void		(VIRTUALBOY_APIENTRY *ShaderSource)(GLuint shader, GLsizei count, const char *const *strings, const GLint *lengths);
	void		(VIRTUALBOY_APIENTRY *CompileShader)(GLuint shader);
	void		(VIRTUALBOY_APIENTRY *GetShaderiv)(GLuint shader, GLenum pname, GLint *params);
	void		(VIRTUALBOY_APIENTRY *GetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei *length, char *infoLog);
	void		(VIRTUALBOY_APIENTRY *DeleteShader)(GLuint shader);
	GLuint		(VIRTUALBOY_APIENTRY *CreateProgram)(void);
	void		(VIRTUALBOY_APIENTRY *AttachShader)(GLuint program, GLuint shader);
	void		(VIRTUALBOY_APIENTRY *LinkProgram)(GLuint program);
	void		(VIRTUALBOY_APIENTRY *GetProgramiv)(GLuint program, GLenum pname, GLint *params);
	void		(VIRTUALBOY_APIENTRY *GetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei *length, char *infoLog);
	void		(VIRTUALBOY_APIENTRY *DeleteProgram)(GLuint program);
	GLboolean	(VIRTUALBOY_APIENTRY *IsProgram)(GLuint program);
	void		(VIRTUALBOY_APIENTRY *UseProgram)(GLuint program);
	GLint		(VIRTUALBOY_APIENTRY *GetUniformLocation)(GLuint program, const char *name);
	void		(VIRTUALBOY_APIENTRY *Uniform1i)(GLint location, GLint v0);
	void		(VIRTUALBOY_APIENTRY *Uniform1f)(GLint location, GLfloat v0);
	void		(VIRTUALBOY_APIENTRY *Uniform2f)(GLint location, GLfloat v0, GLfloat v1);
	void		(VIRTUALBOY_APIENTRY *Uniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
	void		(VIRTUALBOY_APIENTRY *Uniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
	void		(VIRTUALBOY_APIENTRY *ActiveTexture)(GLenum texture);
} virtualboy_gl_t;

typedef struct
{
	GLint color;
	GLint depth;
	GLint origin;
	GLint texel;
	GLint grid;
	GLint taps;
	GLint range;
	GLint projZ;
	GLint edge;
	GLint tone;
	GLint shades;
	GLint wire;
	GLint mask;
	GLint hud;
} virtualboy_uniforms_t;

typedef struct
{
	GLuint texture;
	int width;
	int height;
} virtualboy_copy_t;

static const char kVertexShader[] =
	"#version 110\n"
	"void main()\n"
	"{\n"
	"	gl_Position = gl_Vertex;\n"
	"}\n";

// Pixel positions are relative to the viewport origin and land on texel centres.
// u_grid = grid offset (xy), cell size, gap; u_taps = colour tap offset, edge sample offset.
// u_edge = crease threshold, depth noise floor, far / (far - near).
// u_tone = gain, gamma, dither amount, fill levels; u_wire = fade start, fade end, far level.
// u_mask = row gap, column gap, vignette.
static const char kFragmentShader[] =
	"#version 110\n"
	"uniform sampler2D u_color;\n"
	"uniform sampler2D u_depth;\n"
	"uniform vec2 u_origin;\n"
	"uniform vec2 u_texel;\n"
	"uniform vec4 u_grid;\n"
	"uniform vec2 u_taps;\n"
	"uniform vec2 u_range;\n"
	"uniform vec2 u_projZ;\n"
	"uniform vec3 u_edge;\n"
	"uniform vec4 u_tone;\n"
	"uniform vec3 u_shades;\n"
	"uniform vec3 u_wire;\n"
	"uniform vec3 u_mask;\n"
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
	"float Luma(vec2 pixel)\n"
	"{\n"
	"	return dot(texture2D(u_color, clamp(pixel * u_texel, 0.0, 1.0)).rgb, vec3(0.299, 0.587, 0.114));\n"
	"}\n"
	"\n"
	"float Bayer2(vec2 a)\n"
	"{\n"
	"	a = floor(a);\n"
	"	return fract(a.x * 0.5 + a.y * a.y * 0.75);\n"
	"}\n"
	"\n"
	"float Led(vec2 pixel)\n"
	"{\n"
	"	vec2 cell = floor((pixel - u_grid.xy) / u_grid.z);\n"
	"	vec2 center = floor((cell + 0.5) * u_grid.z) + u_grid.xy + 0.5;\n"
	"	vec2 uv = clamp(center * u_texel, 0.0, 1.0);\n"
	"\n"
	"	float t = u_taps.x;\n"
	"	float luma = 0.25 * (Luma(center + vec2(-t, -t)) + Luma(center + vec2(t, -t)) + Luma(center + vec2(-t, t)) + Luma(center + vec2(t, t)));\n"
	"	luma = clamp(pow(luma, u_tone.y) * u_tone.x, 0.0, 1.0);\n"
	"	float dither = 0.5 + (Bayer2(cell * 0.5) * 0.25 + Bayer2(cell) + 0.03125 - 0.5) * u_tone.z;\n"
	"	float level = min(floor(luma * u_tone.w + dither), u_tone.w);\n"
	"\n"
	"	float depth = Depth(uv);\n"
	"	float s = u_taps.y;\n"
	"	float bend = max(max(Bend(uv, depth, vec2(s, 0.0)), Bend(uv, depth, vec2(0.0, s))),\n"
	"		max(Bend(uv, depth, vec2(s, s)), Bend(uv, depth, vec2(s, -s))));\n"
	"	float ink = smoothstep(u_edge.x, u_edge.x * 2.0, (bend - u_edge.y) / max(u_edge.z - depth, 0.000001));\n"
	"	float dist = u_projZ.y / min(depth * 2.0 - 1.0 + u_projZ.x, -0.000001);\n"
	"	level = max(level, floor(ink * mix(3.0, u_wire.z, smoothstep(u_wire.x, u_wire.y, dist)) + 0.5));\n"
	"\n"
	"	float led = level > 2.5 ? u_shades.z : (level > 1.5 ? u_shades.y : (level > 0.5 ? u_shades.x : 0.0));\n"
	"	vec2 inCell = floor(pixel - u_grid.xy - cell * u_grid.z);\n"
	"	if (u_grid.w > 0.0)\n"
	"	{\n"
	"		if (inCell.y >= u_grid.z - u_grid.w)\n"
	"			led *= 1.0 - u_mask.x;\n"
	"		if (inCell.x >= u_grid.z - u_grid.w)\n"
	"			led *= 1.0 - u_mask.y;\n"
	"	}\n"
	"	vec2 v = pixel * u_texel * 2.0 - 1.0;\n"
	"	return led * (1.0 - u_mask.z * smoothstep(0.5, 2.0, dot(v, v)));\n"
	"}\n"
	"\n"
	"void main()\n"
	"{\n"
	"	vec2 pixel = gl_FragCoord.xy - u_origin;\n"
	"	float red;\n"
	"	if (u_hud > 0.5)\n"
	"	{\n"
	"		vec3 c = texture2D(u_color, pixel * u_texel).rgb;\n"
	"		red = max(c.r, max(c.g, c.b));\n"
	"	}\n"
	"	else\n"
	"	{\n"
	"		red = Led(pixel);\n"
	"	}\n"
	"	gl_FragColor = vec4(red, 0.0, 0.0, 1.0);\n"
	"}\n";

class CVirtualBoy
{
public:
	void CaptureScene( void );
	void Draw( void );
	void DrawHud( void );

private:
	bool Active( void ) const;
	bool Fail( const char *reason );
	bool LoadFunctions( void );
	GLuint Compile( GLenum type, const char *source );
	bool EnsureProgram( void );
	bool Copy( virtualboy_copy_t &target, GLint internalFormat, GLenum format, GLenum type, const GLint *rect );
	void DrawQuad( const GLint *rect, float hud );

	virtualboy_gl_t m_GL = {};
	virtualboy_uniforms_t m_Uniforms = {};
	bool m_bFunctionsLoaded = false;
	bool m_bFailed = false;
	GLuint m_Program = 0;

	virtualboy_copy_t m_SceneColor = {};
	virtualboy_copy_t m_SceneDepth = {};
	virtualboy_copy_t m_HudColor = {};
	float m_flDepthUlp = 1.0f / 16777215.0f;

	// 3D view state, captured mid-scene because HUD_Redraw runs in 2D state.
	bool m_bCaptured = false;
	GLint m_Viewport[4] = {};
	GLfloat m_DepthRange[2] = {};
	float m_ProjZ[2] = {};			// P[10], P[14]
	float m_flFarRatio = 1.0f;		// far / (far - near)
	float m_flPixelSlope = 0.0f;	// view-space tangent covered by one pixel

	// The HUD only turns red on frames whose 3D view did.
	bool m_bSceneDrawn = false;
};

static CVirtualBoy g_VirtualBoy;

template <typename T>
static bool VirtualBoy_Load( T &fn, const char *name )
{
	fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
	return fn != NULL;
}

bool CVirtualBoy::Fail( const char *reason )
{
	gEngfuncs.Con_Printf("virtualboy: %s, effect disabled\n", reason);
	m_bFailed = true;
	return false;
}

bool CVirtualBoy::Active( void ) const
{
	if (m_bFailed || g_fXashEngine || !MutatorEnabled(MUTATOR_VIRTUALBOY))
		return false;

	return IEngineStudio.IsHardware && IEngineStudio.IsHardware() == 1;
}

bool CVirtualBoy::LoadFunctions( void )
{
	return VirtualBoy_Load(m_GL.CreateShader, "glCreateShader")
		&& VirtualBoy_Load(m_GL.ShaderSource, "glShaderSource")
		&& VirtualBoy_Load(m_GL.CompileShader, "glCompileShader")
		&& VirtualBoy_Load(m_GL.GetShaderiv, "glGetShaderiv")
		&& VirtualBoy_Load(m_GL.GetShaderInfoLog, "glGetShaderInfoLog")
		&& VirtualBoy_Load(m_GL.DeleteShader, "glDeleteShader")
		&& VirtualBoy_Load(m_GL.CreateProgram, "glCreateProgram")
		&& VirtualBoy_Load(m_GL.AttachShader, "glAttachShader")
		&& VirtualBoy_Load(m_GL.LinkProgram, "glLinkProgram")
		&& VirtualBoy_Load(m_GL.GetProgramiv, "glGetProgramiv")
		&& VirtualBoy_Load(m_GL.GetProgramInfoLog, "glGetProgramInfoLog")
		&& VirtualBoy_Load(m_GL.DeleteProgram, "glDeleteProgram")
		&& VirtualBoy_Load(m_GL.IsProgram, "glIsProgram")
		&& VirtualBoy_Load(m_GL.UseProgram, "glUseProgram")
		&& VirtualBoy_Load(m_GL.GetUniformLocation, "glGetUniformLocation")
		&& VirtualBoy_Load(m_GL.Uniform1i, "glUniform1i")
		&& VirtualBoy_Load(m_GL.Uniform1f, "glUniform1f")
		&& VirtualBoy_Load(m_GL.Uniform2f, "glUniform2f")
		&& VirtualBoy_Load(m_GL.Uniform3f, "glUniform3f")
		&& VirtualBoy_Load(m_GL.Uniform4f, "glUniform4f")
		&& VirtualBoy_Load(m_GL.ActiveTexture, "glActiveTexture");
}

GLuint CVirtualBoy::Compile( GLenum type, const char *source )
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
		gEngfuncs.Con_Printf("virtualboy: shader compile log:\n%s\n", log);
		m_GL.DeleteShader(shader);
		return 0;
	}

	return shader;
}

bool CVirtualBoy::EnsureProgram( void )
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
		return Fail("display shader did not compile");
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
		gEngfuncs.Con_Printf("virtualboy: shader link log:\n%s\n", log);
		m_GL.DeleteProgram(program);
		return Fail("display shader did not link");
	}

	m_Uniforms.color = m_GL.GetUniformLocation(program, "u_color");
	m_Uniforms.depth = m_GL.GetUniformLocation(program, "u_depth");
	m_Uniforms.origin = m_GL.GetUniformLocation(program, "u_origin");
	m_Uniforms.texel = m_GL.GetUniformLocation(program, "u_texel");
	m_Uniforms.grid = m_GL.GetUniformLocation(program, "u_grid");
	m_Uniforms.taps = m_GL.GetUniformLocation(program, "u_taps");
	m_Uniforms.range = m_GL.GetUniformLocation(program, "u_range");
	m_Uniforms.projZ = m_GL.GetUniformLocation(program, "u_projZ");
	m_Uniforms.edge = m_GL.GetUniformLocation(program, "u_edge");
	m_Uniforms.tone = m_GL.GetUniformLocation(program, "u_tone");
	m_Uniforms.shades = m_GL.GetUniformLocation(program, "u_shades");
	m_Uniforms.wire = m_GL.GetUniformLocation(program, "u_wire");
	m_Uniforms.mask = m_GL.GetUniformLocation(program, "u_mask");
	m_Uniforms.hud = m_GL.GetUniformLocation(program, "u_hud");

	m_Program = program;
	return true;
}

// Copies rect of the read buffer into the target on the active unit and leaves it bound there.
bool CVirtualBoy::Copy( virtualboy_copy_t &target, GLint internalFormat, GLenum format, GLenum type, const GLint *rect )
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
	for (int i = 0; i < VIRTUALBOY_MAX_GL_ERRORS && glGetError() != GL_NO_ERROR; i++)
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

void CVirtualBoy::DrawQuad( const GLint *rect, float hud )
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

	m_GL.Uniform1i(m_Uniforms.depth, 0);
	m_GL.Uniform1i(m_Uniforms.color, 1);
	m_GL.Uniform2f(m_Uniforms.origin, (float)rect[0], (float)rect[1]);
	m_GL.Uniform2f(m_Uniforms.texel, 1.0f / (float)rect[2], 1.0f / (float)rect[3]);
	m_GL.Uniform1f(m_Uniforms.hud, hud);

	glBegin(GL_QUADS);
	glVertex2f(-1.0f, -1.0f);
	glVertex2f(1.0f, -1.0f);
	glVertex2f(1.0f, 1.0f);
	glVertex2f(-1.0f, 1.0f);
	glEnd();
}

// Called from HUD_DrawNormalTriangles while the world projection is current.
void CVirtualBoy::CaptureScene( void )
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
	m_flFarRatio = (1.0f - projection[10]) * 0.5f;
	m_flPixelSlope = 2.0f / (fabsf(projection[0]) * (float)viewport[2]);
	m_bCaptured = true;
}

// Called at the top of HUD_Redraw, after the other screen-space passes: the finished 3D frame is in the back buffer, the HUD is not.
void CVirtualBoy::Draw( void )
{
	const bool captured = m_bCaptured;
	m_bCaptured = false;
	m_bSceneDrawn = false;

	if (!captured || !Active() || !EnsureProgram())
		return;

	const int width = m_Viewport[2];
	const int height = m_Viewport[3];

	float cell = floorf((float)height / VIRTUALBOY_ROWS + 0.5f);
	if (cell < 1.0f)
		cell = 1.0f;
	const float edgeStep = fmaxf(1.0f, floorf(cell * 0.5f));
	const float colorTap = floorf(cell * 0.25f);
	const float gap = cell >= VIRTUALBOY_MIN_GAP_CELL ? fmaxf(1.0f, floorf(cell / 5.0f)) : 0.0f;
	const float rangeSpan = fabsf(m_DepthRange[1] - m_DepthRange[0]);

	GLint previousProgram = 0;
	GLint previousUnit = GL_TEXTURE0;
	GLint previousUnit1Texture = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &previousUnit);

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT | GL_VIEWPORT_BIT | GL_POLYGON_BIT | GL_SCISSOR_BIT);
	m_GL.ActiveTexture(GL_TEXTURE1);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousUnit1Texture);

	bool copied = Copy(m_SceneColor, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, m_Viewport);
	if (copied)
	{
		m_GL.ActiveTexture(GL_TEXTURE0);
		const bool newDepth = width != m_SceneDepth.width || height != m_SceneDepth.height;
		copied = Copy(m_SceneDepth, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, m_Viewport);
		if (copied && newDepth)
		{
			GLint bits = 24;
			glGetIntegerv(GL_DEPTH_BITS, &bits);
			if (bits < 16 || bits > 24)
				bits = 24;
			m_flDepthUlp = 1.0f / (float)((1 << bits) - 1);
		}
	}

	if (copied)
	{
		m_GL.UseProgram(m_Program);
		m_GL.Uniform4f(m_Uniforms.grid, floorf(fmodf((float)width, cell) * 0.5f), floorf(fmodf((float)height, cell) * 0.5f), cell, gap);
		m_GL.Uniform2f(m_Uniforms.taps, colorTap, edgeStep);
		m_GL.Uniform2f(m_Uniforms.range, m_DepthRange[0], m_DepthRange[1]);
		m_GL.Uniform2f(m_Uniforms.projZ, m_ProjZ[0], m_ProjZ[1]);
		m_GL.Uniform3f(m_Uniforms.edge, VIRTUALBOY_CREASE_SLOPE * edgeStep * m_flPixelSlope, VIRTUALBOY_DEPTH_ULPS * m_flDepthUlp / rangeSpan, m_flFarRatio);
		m_GL.Uniform4f(m_Uniforms.tone, VIRTUALBOY_GAIN, VIRTUALBOY_GAMMA, VIRTUALBOY_DITHER, VIRTUALBOY_FILL_LEVELS);
		m_GL.Uniform3f(m_Uniforms.shades, VIRTUALBOY_SHADE_1, VIRTUALBOY_SHADE_2, VIRTUALBOY_SHADE_3);
		m_GL.Uniform3f(m_Uniforms.wire, VIRTUALBOY_WIRE_FADE_START, VIRTUALBOY_WIRE_FADE_END, VIRTUALBOY_WIRE_FAR_LEVEL);
		m_GL.Uniform3f(m_Uniforms.mask, VIRTUALBOY_ROW_GAP, VIRTUALBOY_COLUMN_GAP, VIRTUALBOY_VIGNETTE);
		DrawQuad(m_Viewport, 0.0f);
		m_GL.UseProgram((GLuint)previousProgram);
		m_bSceneDrawn = true;
	}

	// The engine caches the binding of every unit it uses.
	m_GL.ActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, (GLuint)previousUnit1Texture);

	glPopAttrib();
	m_GL.ActiveTexture((GLenum)previousUnit);
}

// Called after gHUD.Redraw: maps every pixel of the window to red by its brightest channel.
// The 3D view is already pure red, so only the HUD changes.
void CVirtualBoy::DrawHud( void )
{
	const bool sceneDrawn = m_bSceneDrawn;
	m_bSceneDrawn = false;

	if (!sceneDrawn || !Active() || !EnsureProgram())
		return;

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	if (viewport[2] <= 0 || viewport[3] <= 0)
		return;

	GLint previousProgram = 0;
	GLint previousUnit = GL_TEXTURE0;
	GLint previousUnit1Texture = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &previousUnit);

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT | GL_VIEWPORT_BIT | GL_POLYGON_BIT | GL_SCISSOR_BIT);
	m_GL.ActiveTexture(GL_TEXTURE1);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousUnit1Texture);

	if (Copy(m_HudColor, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, viewport))
	{
		m_GL.UseProgram(m_Program);
		DrawQuad(viewport, 1.0f);
		m_GL.UseProgram((GLuint)previousProgram);
	}

	glBindTexture(GL_TEXTURE_2D, (GLuint)previousUnit1Texture);

	glPopAttrib();
	m_GL.ActiveTexture((GLenum)previousUnit);
}

void VirtualBoy_CaptureScene( void )
{
	g_VirtualBoy.CaptureScene();
}

void VirtualBoy_Draw( void )
{
	g_VirtualBoy.Draw();
}

void VirtualBoy_DrawHud( void )
{
	g_VirtualBoy.DrawHud();
}

#else

void VirtualBoy_CaptureScene( void )
{
}

void VirtualBoy_Draw( void )
{
}

void VirtualBoy_DrawHud( void )
{
}

#endif
