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

#include "GL/gl.h" // Header File For The OpenGL32 (32 bits) Library

#include "colorcor.h"

#ifndef GL_TEXTURE_RECTANGLE_NV
#define GL_TEXTURE_RECTANGLE_NV 0x84F5
#endif

CColorCorTexture::CColorCorTexture() {};

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
}

void CColorCorTexture::BindTexture(int width, int height)
{ 
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_texture);

	if( CVAR_GET_FLOAT( "colorcor_blackwhite" ) == 1 ||
		(CheckMutator(MUTATOR_OLDTIME)))
	{
		glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_LUMINANCE, 0, 0, ScreenWidth, ScreenHeight, 0);
	}
	else
	{
		glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA8, 0, 0, ScreenWidth, ScreenHeight, 0);
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

	glColor4f(r,g,b,alpha);

	// this will inverse the color 
	if ( CVAR_GET_FLOAT( "colorcor_inverse" ) == 1 ||
		(CheckMutator(MUTATOR_INVERSE)))
	{
		glEnable(GL_COLOR_LOGIC_OP);
		glLogicOp(GL_COPY_INVERTED);
	}

	glBegin(GL_QUADS);
		DrawQuad(width, height, of);
	glEnd();

	// this will inverse the color
	if (CVAR_GET_FLOAT( "colorcor_inverse" ) == 1 ||
		(CheckMutator(MUTATOR_INVERSE)))
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
	int r = m_pTextures.r = CVAR_GET_FLOAT( "colorcor_r" );
	int g = m_pTextures.g = CVAR_GET_FLOAT( "colorcor_g" );
	int b = m_pTextures.b = CVAR_GET_FLOAT( "colorcor_b" );
	if (CheckMutator(MUTATOR_SILDENAFIL))
	{
		r = 0;
		g = 113;
		b = 230;
	}
	m_pTextures.r = r;
	m_pTextures.g = g;
	m_pTextures.b = b;

	glBlendFunc(GL_SRC_ALPHA, GL_ZERO);
	glEnable(GL_BLEND);

	m_pTextures.BindTexture(ScreenWidth, ScreenHeight);
	m_pTextures.Draw(ScreenWidth, ScreenHeight);

	m_pTextures.alpha = CVAR_GET_FLOAT("colorcor_alpha");
	m_pTextures.of = 0;

	glDisable(GL_BLEND);
}

#endif
