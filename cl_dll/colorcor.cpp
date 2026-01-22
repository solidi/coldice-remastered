/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

// Experimental color correction system for Half-Life
// Original code by Ryokeen, modified by Highlander and Fire64.
// Originally for use with Raven City Half-Life modification 
// Addopted for HL SDK 2.3 and FlatLine Arena by Napoleon

#include "hud.h"
#include "cl_util.h"

#ifndef __APPLE__

#ifdef _WIN32
#include "winsani_in.h" // fix this will resolve the HSPRITE conflict
#include <windows.h>
#include "winsani_out.h" // fix this will resolve the other conflicts resulting of windows.h
#endif

#include <stdio.h>
#include <stdlib.h>
#include <new>

#include "GL/gl.h" // Header File For The OpenGL32 (32 bits) Library

#include "colorcor.h"

CColorCorTexture::CColorCorTexture() : m_bGpuGrayscaleSupported(false) {};

void CColorCorTexture::ApplyCpuGrayscale(int width, int height)
{
	// CPU path: read pixels, convert to grayscale, and upload
	// Validate dimensions to prevent integer overflow
	// Check before multiplication to avoid overflow in the calculation itself
	if (width <= 0 || height <= 0 || width > 16384 || height > 16384)
		return; // Invalid or excessively large dimensions
	
	// Calculate buffer size with overflow check
	size_t bufferSize = (size_t)width * (size_t)height * 4;
	if (bufferSize > 16384 * 16384 * 4) // Double-check to prevent overflow
		return;
	
	unsigned char* pPixels = new (std::nothrow) unsigned char[bufferSize];
	if (!pPixels)
		return; // Allocation failed
	
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pPixels);
	
	// Convert to grayscale using luminance formula
	for (size_t i = 0; i < bufferSize; i += 4)
	{
		unsigned char gray = (unsigned char)(0.299f * pPixels[i] + 0.587f * pPixels[i+1] + 0.114f * pPixels[i+2]);
		pPixels[i] = gray;     // R
		pPixels[i+1] = gray;   // G
		pPixels[i+2] = gray;   // B
		// Keep alpha as-is (pPixels[i+3])
	}
	
	// Upload the grayscale texture
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pPixels);
	delete[] pPixels;
}

bool CColorCorTexture::TestGpuGrayscaleSupport(int width, int height)
{
	// Test if GL_LUMINANCE format works with glCopyTexImage2D on this device
	// Some devices/drivers don't support GL_LUMINANCE with GL_TEXTURE_RECTANGLE_NV
	
	// Create a temporary test texture
	GLuint testTexture;
	glGenTextures(1, &testTexture);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, testTexture);
	
	// Clear any previous errors with safety limit to prevent infinite loops
	// 100 iterations should be more than enough for any reasonable error queue
	const int ERROR_CLEAR_LIMIT = 100;
	int errorClearCount = ERROR_CLEAR_LIMIT;
	while (glGetError() != GL_NO_ERROR && --errorClearCount > 0);
	
	// Create a small test texture with RGBA data first
	unsigned char rgbaTestData[4] = {128, 128, 128, 255}; // Gray pixel
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaTestData);
	
	// Clear any errors from the setup with safety limit
	errorClearCount = ERROR_CLEAR_LIMIT;
	while (glGetError() != GL_NO_ERROR && --errorClearCount > 0);
	
	// Try to create a texture with GL_LUMINANCE format using single-channel test data
	unsigned char luminanceTestData[1] = {128}; // Gray value
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, luminanceTestData);
	
	// Check if an error occurred
	GLenum error = glGetError();
	bool supported = (error == GL_NO_ERROR);
	
	// Clean up test texture
	glDeleteTextures(1, &testTexture);
	
	// Rebind our main texture
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_texture);
	
	return supported;
}

void CColorCorTexture::Init(int width, int height)
{
	// create a load of blank pixels to create textures with
	unsigned char* pBlankTex = new unsigned char[width * height * 3];
	memset(pBlankTex, 0, width * height * 3);

	// Create the SCREEN-HOLDING TEXTURE
	glGenTextures(1, &g_texture);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_texture);

	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA8, width, height, 0, GL_RGBA8, GL_UNSIGNED_BYTE, 0);

	// free the memory
	delete[] pBlankTex;
	
	// Test if GPU-accelerated grayscale (GL_LUMINANCE) works on this device
	m_bGpuGrayscaleSupported = TestGpuGrayscaleSupport(width, height);
}

void CColorCorTexture::BindTexture(int width, int height)
{ 
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_texture);
	
	// Check if we need grayscale conversion (oldtime mutator or blackwhite cvar)
	if (CVAR_GET_FLOAT("colorcor_blackwhite") == 1 || MutatorEnabled(MUTATOR_OLDTIME))
	{
		if (m_bGpuGrayscaleSupported)
		{
			// GPU path: use hardware-accelerated grayscale conversion
			glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_LUMINANCE, 0, 0, width, height, 0);
			
			// Check if it failed at runtime (rare, but possible)
			if (glGetError() != GL_NO_ERROR)
			{
				// Mark as unsupported for future frames
				// Note: This state change is safe because Half-Life's rendering
				// happens on a single thread in the client DLL
				m_bGpuGrayscaleSupported = false;
				
				// Fall back to CPU path for this frame
				ApplyCpuGrayscale(width, height);
			}
		}
		else
		{
			// CPU path: read pixels, convert to grayscale, and upload
			ApplyCpuGrayscale(width, height);
		}
	}
	else
	{
		// Normal color - just copy directly from framebuffer
		glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA8, 0, 0, width, height, 0);
	}
}

void CColorCorTexture::Draw(int width, int height)
{
	// enable some OpenGL stuff
	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glColor3f(1,1,1);
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, 1, 1, 0, 0.1, 100);

	glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_texture);

	// Set color modulation (grayscale already applied in BindTexture if needed)
	glColor4f(r, g, b, alpha);

	// this will inverse the color 
	if ( CVAR_GET_FLOAT( "colorcor_inverse" ) == 1 ||
		(MutatorEnabled(MUTATOR_INVERSE)))
	{
		glEnable(GL_COLOR_LOGIC_OP);
		glLogicOp(GL_COPY_INVERTED);
	}

	glBegin(GL_QUADS);
		DrawQuad(width, height, of);
	glEnd();

	// this will inverse the color
	if (CVAR_GET_FLOAT( "colorcor_inverse" ) == 1 ||
		(MutatorEnabled(MUTATOR_INVERSE)))
	{
		glDisable(GL_COLOR_LOGIC_OP);
	}

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glDisable(GL_TEXTURE_RECTANGLE_NV);
	glEnable(GL_DEPTH_TEST);
}

void CColorCorTexture::DrawQuad(int width, int height, int of)
{
	glTexCoord2f(0-of,0-of);
	glVertex3f(0, 1, -1);
	glTexCoord2f(0-of,height+of);
	glVertex3f(0, 0, -1);
	glTexCoord2f(width+of,height+of);
	glVertex3f(1, 0, -1);
	glTexCoord2f(width+of,0-of);
	glVertex3f(1, 1, -1);
}

CColorCor gColorCor;

void CColorCor::InitScreen()
{
	m_pTextures.Init(ScreenWidth,ScreenHeight);
}

void CColorCor::DrawColorCor()
{
	int r = CVAR_GET_FLOAT( "colorcor_r" );
	int g = CVAR_GET_FLOAT( "colorcor_g" );
	int b = CVAR_GET_FLOAT( "colorcor_b" );
	
	// Default to white if no color is set
	if (r == 0 && g == 0 && b == 0)
	{
		r = 255;
		g = 255;
		b = 255;
	}
	
	// Special color for sildenafil mutator
	if (MutatorEnabled(MUTATOR_SILDENAFIL))
	{
		r = 0;
		g = 113;
		b = 230;
	}
	
	// For oldtime (black and white), set neutral color
	if (MutatorEnabled(MUTATOR_OLDTIME))
	{
		r = 255;
		g = 255;
		b = 255;
	}
	
	m_pTextures.r = r;
	m_pTextures.g = g;
	m_pTextures.b = b;

	float alphaValue = CVAR_GET_FLOAT("colorcor_alpha");
	if (alphaValue < 0.01f) alphaValue = 1.0f; // Default to 1.0 if not set
	m_pTextures.alpha = alphaValue;
	m_pTextures.of = 0;

	glBlendFunc(GL_SRC_ALPHA, GL_ZERO);
	glEnable(GL_BLEND);

	m_pTextures.BindTexture(ScreenWidth, ScreenHeight);
	m_pTextures.Draw(ScreenWidth, ScreenHeight);

	glDisable(GL_BLEND);
}

#endif
