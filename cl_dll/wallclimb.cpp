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

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"

extern cvar_t *cl_wallclimbindicator;

int CHudWallClimb::Init(void)
{
	m_iPos = 0;
	m_iFlags = 0;
	gHUD.AddHudElem(this);

	return 1;
};

int CHudWallClimb::VidInit(void)
{
	m_hSprite = 0;

	return 1;
};

int CHudWallClimb::Draw(float fTime)
{
	if ( !m_hSprite )
		m_hSprite = LoadSprite("sprites/wallclimb.spr");

	// No observer
	if (g_iUser1)
		return 1;

	if (cl_wallclimbindicator && cl_wallclimbindicator->value)
	{
		int r, g, b, x, y;

		UnpackRGB(r,g,b, HudColor());
		SPR_Set(m_hSprite, r, g, b );

		// This should show up to the right and part way up the armor number
		y = SPR_Height(m_hSprite,0) - gHUD.m_iFontHeight;
		x = (ScreenWidth/2) - (SPR_Width(m_hSprite,0)/2);

		SPR_DrawAdditive(0, x, y, NULL);
	}

	return 1;
}
