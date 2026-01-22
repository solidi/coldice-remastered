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
// Original code by Ryokeen, modified by Highlander.
// Originally for use with Raven City modification 
// Addopted for HL SDK 2.3 and FlatLine Arena by Napoleon

#ifndef __APPLE__

class CColorCorTexture
{
public:
	CColorCorTexture();
	void Init(int width, int height);
	void BindTexture(int width, int height);
	void DrawQuad(int width, int height,int of);
	void Draw(int width, int height);
	unsigned int g_texture;

	float alpha;
	float r,g,b;
	float of;

private:
	bool m_bGpuGrayscaleSupported;
	bool TestGpuGrayscaleSupport(int width, int height);
};

class CColorCor
{
public:
	void InitScreen(void);
	void DrawColorCor(void);

	CColorCorTexture m_pTextures;
};

extern CColorCor gColorCor;

#endif
