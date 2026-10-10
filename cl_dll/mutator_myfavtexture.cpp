//
// mutator_myfavtexture.cpp
//
// "myfavtexture" mutator - client side
//
// While active, every world and brush-entity surface renders one replacement
// texture. Brush submodels share the world's texture list, so swapping the GL
// texture id on each world texture_t covers func_* entities as well. Original
// ids are restored when the mutator ends.
//

#ifndef __APPLE__

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "cl_entity.h"
#include "com_model.h"
#include "r_studioint.h"
#include <string.h>
#include <stddef.h>

#include "PlatformHeaders.h"
#include "SDL2/SDL.h"
#include "GL/gl.h"

extern engine_studio_api_t IEngineStudio;
extern qboolean g_fXashEngine;

// Replacement texture source. Set the name to kMyFavTextureChecker to skip the WAD and use the generated checker.
static const char kMyFavTextureWad[] = "xeno.wad";
static const char kMyFavTextureName[] = "AAATRIGGER";
static const char kMyFavTextureChecker[] = "*checker";

#define MYFAVTEXTURE_MAX_SWAPS		1024
#define MYFAVTEXTURE_MAX_VARIANTS	64
#define MYFAVTEXTURE_MAX_DIM		1024
#define MYFAVTEXTURE_CHECKER_SIZE	32
#define MYFAVTEXTURE_CHECKER_CELL	16
#define WAD3_LUMP_MIPTEX			0x43

// Leading fields of the hardware renderer's texture_t; com_model.h carries the software layout.
typedef struct
{
	char		name[16];
	unsigned	width, height;
	int			gl_texturenum;
} hw_texture_t;

static_assert(offsetof(hw_texture_t, gl_texturenum) == 24, "hardware texture_t layout drift");

typedef struct
{
	int				index;
	hw_texture_t	*tex;
	int				originalId;
	int				replacementId;
} texswap_t;

typedef struct
{
	int		repsX, repsY;
	GLuint	tex;
} texvariant_t;

static unsigned char *s_pSourcePixels = NULL;
static int s_iSourceWidth = 0;
static int s_iSourceHeight = 0;
static bool s_bSourceReady = false;

static texvariant_t s_Variants[MYFAVTEXTURE_MAX_VARIANTS];
static int s_iNumVariants = 0;

static texswap_t s_Swaps[MYFAVTEXTURE_MAX_SWAPS];
static int s_iNumSwaps = 0;
static bool s_bApplied = false;
static model_t *s_pSwappedWorld = NULL;
static texture_t **s_pSwappedTextures = NULL;
static int s_iSwappedCount = 0;

static texture_t **s_pRejectedTextures = NULL;

static int ReadInt(const unsigned char *p)
{
	int v;
	memcpy(&v, p, sizeof(v));
	return v;
}

static bool DecodeWadTexture(const unsigned char *pFile, int length, const char *texName)
{
	if (length < 12 || memcmp(pFile, "WAD3", 4) != 0)
		return false;

	const int numLumps = ReadInt(pFile + 4);
	const int infoOfs = ReadInt(pFile + 8);
	if (numLumps <= 0 || infoOfs < 12 || infoOfs > length || numLumps > (length - infoOfs) / 32)
		return false;

	for (int i = 0; i < numLumps; i++)
	{
		const unsigned char *pLump = pFile + infoOfs + i * 32;
		const int filePos = ReadInt(pLump);
		const int diskSize = ReadInt(pLump + 4);
		const unsigned char type = pLump[12];
		const unsigned char compression = pLump[13];

		char name[17];
		memcpy(name, pLump + 16, 16);
		name[16] = '\0';

		if (type != WAD3_LUMP_MIPTEX || compression != 0 || stricmp(name, texName) != 0)
			continue;

		// miptex_t: name[16], width, height, offsets[4]
		if (filePos < 0 || diskSize < 40 || filePos > length || diskSize > length - filePos)
			return false;

		const unsigned char *pMip = pFile + filePos;
		const int width = ReadInt(pMip + 16);
		const int height = ReadInt(pMip + 20);
		const int mip0 = ReadInt(pMip + 24);
		const int mip3 = ReadInt(pMip + 36);
		if (width < 8 || height < 8 || width > MYFAVTEXTURE_MAX_DIM || height > MYFAVTEXTURE_MAX_DIM)
			return false;

		const int pixelCount = width * height;
		if (mip0 < 40 || mip0 > diskSize || pixelCount > diskSize - mip0)
			return false;

		const int mip3Size = (width / 8) * (height / 8);
		if (mip3 < 40 || mip3 > diskSize || mip3Size + 2 > diskSize - mip3)
			return false;

		const int palOfs = mip3 + mip3Size;
		const int palCount = pMip[palOfs] | (pMip[palOfs + 1] << 8);
		if (palCount <= 0 || palCount > 256 || palCount * 3 > diskSize - palOfs - 2)
			return false;

		const unsigned char *pIndices = pMip + mip0;
		const unsigned char *pPalette = pMip + palOfs + 2;

		s_pSourcePixels = new unsigned char[pixelCount * 4];
		for (int p = 0; p < pixelCount; p++)
		{
			const int idx = pIndices[p];
			unsigned char *dst = s_pSourcePixels + p * 4;
			if (idx < palCount)
			{
				dst[0] = pPalette[idx * 3];
				dst[1] = pPalette[idx * 3 + 1];
				dst[2] = pPalette[idx * 3 + 2];
			}
			else
			{
				dst[0] = dst[1] = dst[2] = 0;
			}
			dst[3] = 255;
		}

		s_iSourceWidth = width;
		s_iSourceHeight = height;
		return true;
	}

	return false;
}

static bool LoadWadTexture(const char *wadName, const char *texName)
{
	char path[64];
	strncpy(path, wadName, sizeof(path) - 1);
	path[sizeof(path) - 1] = '\0';

	int length = 0;
	unsigned char *pFile = (unsigned char *)gEngfuncs.COM_LoadFile(path, 5, &length);
	if (!pFile)
		return false;

	const bool ok = DecodeWadTexture(pFile, length, texName);
	gEngfuncs.COM_FreeFile(pFile);
	return ok;
}

static void BuildCheckerTexture(void)
{
	const int size = MYFAVTEXTURE_CHECKER_SIZE;
	s_pSourcePixels = new unsigned char[size * size * 4];

	for (int y = 0; y < size; y++)
	{
		for (int x = 0; x < size; x++)
		{
			const bool magenta = ((x / MYFAVTEXTURE_CHECKER_CELL) + (y / MYFAVTEXTURE_CHECKER_CELL)) & 1;
			unsigned char *dst = s_pSourcePixels + (y * size + x) * 4;
			dst[0] = magenta ? 255 : 0;
			dst[1] = 0;
			dst[2] = magenta ? 255 : 0;
			dst[3] = 255;
		}
	}

	s_iSourceWidth = size;
	s_iSourceHeight = size;
}

static void EnsureSource(void)
{
	if (s_bSourceReady)
		return;

	s_bSourceReady = true;

	if (stricmp(kMyFavTextureName, kMyFavTextureChecker) != 0)
	{
		if (LoadWadTexture(kMyFavTextureWad, kMyFavTextureName))
		{
			gEngfuncs.Con_Printf("myfavtexture: using %s from %s (%dx%d)\n", kMyFavTextureName, kMyFavTextureWad, s_iSourceWidth, s_iSourceHeight);
			return;
		}

		gEngfuncs.Con_Printf("myfavtexture: %s not found in %s, using checker\n", kMyFavTextureName, kMyFavTextureWad);
	}

	BuildCheckerTexture();
}

// In-place 2x2 box filter; each destination texel is written after its four sources are read.
static void UploadMipmapped(unsigned char *rgba, int width, int height)
{
	int level = 0;
	glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

	while (width > 1 || height > 1)
	{
		const int nextW = width > 1 ? width / 2 : 1;
		const int nextH = height > 1 ? height / 2 : 1;
		const int stepX = width > 1 ? 1 : 0;
		const int stepY = height > 1 ? 1 : 0;

		for (int y = 0; y < nextH; y++)
		{
			for (int x = 0; x < nextW; x++)
			{
				const unsigned char *a = rgba + ((y * 2) * width + x * 2) * 4;
				const unsigned char *b = a + stepX * 4;
				const unsigned char *c = a + stepY * width * 4;
				const unsigned char *d = c + stepX * 4;
				unsigned char out[4];
				for (int ch = 0; ch < 4; ch++)
					out[ch] = (unsigned char)((a[ch] + b[ch] + c[ch] + d[ch] + 2) >> 2);
				memcpy(rgba + (y * nextW + x) * 4, out, 4);
			}
		}

		width = nextW;
		height = nextH;
		glTexImage2D(GL_TEXTURE_2D, ++level, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	}
}

static GLuint CreateVariant(int repsX, int repsY)
{
	const int width = s_iSourceWidth * repsX;
	const int height = s_iSourceHeight * repsY;
	const int rowBytes = s_iSourceWidth * 4;

	unsigned char *pixels = new unsigned char[width * height * 4];
	for (int y = 0; y < height; y++)
	{
		const unsigned char *srcRow = s_pSourcePixels + (y % s_iSourceHeight) * rowBytes;
		unsigned char *dstRow = pixels + y * width * 4;
		for (int r = 0; r < repsX; r++)
			memcpy(dstRow + r * rowBytes, srcRow, rowBytes);
	}

	GLint prevTexture = 0, prevAlignment = 4;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlignment);

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	UploadMipmapped(pixels, width, height);

	glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlignment);
	glBindTexture(GL_TEXTURE_2D, (GLuint)prevTexture);

	delete[] pixels;
	return tex;
}

// Whole source repetitions per original texture span keep the wrap seamless at near 1:1 texel density.
static int RepsFor(unsigned originalDim, int sourceDim)
{
	int reps = ((int)originalDim + sourceDim / 2) / sourceDim;
	if (reps < 1)
		reps = 1;

	const int maxReps = MYFAVTEXTURE_MAX_DIM / sourceDim;
	if (reps > maxReps)
		reps = maxReps > 0 ? maxReps : 1;

	return reps;
}

static int GetVariant(unsigned originalWidth, unsigned originalHeight)
{
	const int repsX = RepsFor(originalWidth, s_iSourceWidth);
	const int repsY = RepsFor(originalHeight, s_iSourceHeight);

	for (int i = 0; i < s_iNumVariants; i++)
	{
		texvariant_t *v = &s_Variants[i];
		if (v->repsX != repsX || v->repsY != repsY)
			continue;

		if (!glIsTexture(v->tex))
			v->tex = CreateVariant(repsX, repsY);
		return (int)v->tex;
	}

	if (s_iNumVariants >= MYFAVTEXTURE_MAX_VARIANTS)
	{
		if (!glIsTexture(s_Variants[0].tex))
			s_Variants[0].tex = CreateVariant(s_Variants[0].repsX, s_Variants[0].repsY);
		return (int)s_Variants[0].tex;
	}

	texvariant_t *v = &s_Variants[s_iNumVariants++];
	v->repsX = repsX;
	v->repsY = repsY;
	v->tex = CreateVariant(repsX, repsY);
	return (int)v->tex;
}

static bool IsSkyTexture(const hw_texture_t *tex)
{
	return strnicmp(tex->name, "sky", 3) == 0;
}

static bool HasPlausibleHeader(const hw_texture_t *tex)
{
	return memchr(tex->name, '\0', sizeof(tex->name)) != NULL
		&& tex->width > 0 && tex->width <= 4096
		&& tex->height > 0 && tex->height <= 4096;
}

// Rejects the swap unless nearly every texture carries a unique live GL id at the expected offset.
static bool ValidateLayout(model_t *world)
{
	int candidates = 0;
	int valid = 0;

	for (int i = 0; i < world->numtextures; i++)
	{
		hw_texture_t *tex = (hw_texture_t *)world->textures[i];
		if (!tex || !HasPlausibleHeader(tex) || IsSkyTexture(tex))
			continue;

		candidates++;

		const int id = tex->gl_texturenum;
		if (id <= 0 || !glIsTexture((GLuint)id))
			continue;

		bool unique = true;
		for (int j = 0; j < i && unique; j++)
		{
			hw_texture_t *other = (hw_texture_t *)world->textures[j];
			if (other && other->gl_texturenum == id)
				unique = false;
		}

		if (unique)
			valid++;
	}

	return candidates > 0 && valid * 10 >= candidates * 9;
}

static bool IsSwapLive(model_t *world)
{
	if (!world || world != s_pSwappedWorld || world->textures != s_pSwappedTextures || world->numtextures != s_iSwappedCount)
		return false;

	for (int i = 0; i < s_iNumSwaps; i++)
	{
		const texswap_t *swap = &s_Swaps[i];
		if ((hw_texture_t *)world->textures[swap->index] != swap->tex || swap->tex->gl_texturenum != swap->replacementId)
			return false;
	}

	return true;
}

static void DiscardSwap(void)
{
	s_iNumSwaps = 0;
	s_bApplied = false;
	s_pSwappedWorld = NULL;
	s_pSwappedTextures = NULL;
	s_iSwappedCount = 0;
}

static void ApplySwap(model_t *world)
{
	if (!world->textures || world->numtextures <= 0 || world->textures == s_pRejectedTextures)
		return;

	if (world->numtextures > MYFAVTEXTURE_MAX_SWAPS || !ValidateLayout(world))
	{
		gEngfuncs.Con_Printf("myfavtexture: world textures not recognized (%d), effect disabled for this map\n", world->numtextures);
		s_pRejectedTextures = world->textures;
		return;
	}

	EnsureSource();

	s_iNumSwaps = 0;
	for (int i = 0; i < world->numtextures; i++)
	{
		hw_texture_t *tex = (hw_texture_t *)world->textures[i];
		if (!tex || !HasPlausibleHeader(tex) || IsSkyTexture(tex) || tex->gl_texturenum <= 0)
			continue;

		texswap_t *swap = &s_Swaps[s_iNumSwaps++];
		swap->index = i;
		swap->tex = tex;
		swap->originalId = tex->gl_texturenum;
		swap->replacementId = GetVariant(tex->width, tex->height);
		tex->gl_texturenum = swap->replacementId;
	}

	s_pSwappedWorld = world;
	s_pSwappedTextures = world->textures;
	s_iSwappedCount = world->numtextures;
	s_bApplied = true;
}

static void RestoreSwap(model_t *world)
{
	if (IsSwapLive(world))
	{
		for (int i = 0; i < s_iNumSwaps; i++)
			s_Swaps[i].tex->gl_texturenum = s_Swaps[i].originalId;
	}

	DiscardSwap();
}

// Called while the world is being rendered, so the GL context is current.
void MyFavTexture_Frame(void)
{
	if (g_fXashEngine || !IEngineStudio.IsHardware || IEngineStudio.IsHardware() != 1)
		return;

	cl_entity_t *pWorld = gEngfuncs.GetEntityByIndex(0);
	model_t *world = (pWorld && pWorld->model && pWorld->model->type == mod_brush) ? pWorld->model : NULL;

	// A reloaded map hands us fresh texture_t data that the engine already owns.
	if (s_bApplied && !IsSwapLive(world))
		DiscardSwap();

	const bool wanted = world != NULL && MutatorEnabled(MUTATOR_MYFAVTEXTURE);
	if (wanted && !s_bApplied)
		ApplySwap(world);
	else if (!wanted && s_bApplied)
		RestoreSwap(world);
}

#endif
