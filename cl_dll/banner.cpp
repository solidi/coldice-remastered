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

//
// Banner HUD Element
// 
// Displays a centered banner with title and subtitle text for a specified duration.
// Useful for team notifications, round start messages, and gameplay instructions.
//
// Usage from client-side:
//   gHUD.m_Banner.ShowBanner("You are on team Smelters", "Do you smell what I smell?", 5.0f);
//
// Server-side message format (not implemented yet):
//   MESSAGE_BEGIN(MSG_ONE, gmsgBanner, NULL, pPlayer);
//     WRITE_STRING("You are on team Smelters");  // Title
//     WRITE_STRING("Do you smell what I smell?");  // Subtitle
//     WRITE_BYTE(50);  // Duration in tenths of a second (50 = 5.0 seconds)
//   MESSAGE_END();
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include <stdio.h>
#include "triangleapi.h"

DECLARE_MESSAGE( m_Banner, Banner )

int CHudBanner::Init()
{
	HOOK_MESSAGE( Banner );

	m_iFlags = 0;
	m_bActive = false;
	m_flShowUntil = 0.0f;
	m_szTitle[0] = '\0';
	m_szSubtitle[0] = '\0';

	gHUD.AddHudElem(this);

	return 1;
}

int CHudBanner::VidInit()
{
	m_bActive = false;
	m_flShowUntil = 0.0f;
	return 1;
}

void CHudBanner::Reset()
{
	// Reset banner state on map change or respawn
	m_bActive = false;
	m_flShowUntil = 0.0f;
	m_szTitle[0] = '\0';
	m_szSubtitle[0] = '\0';
	m_iFlags &= ~HUD_ACTIVE;
}

int CHudBanner::MsgFunc_Banner(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );
	
	// Read title
	const char *title = READ_STRING();
	strncpy(m_szTitle, title, sizeof(m_szTitle) - 1);
	m_szTitle[sizeof(m_szTitle) - 1] = '\0';
	
	// Read subtitle
	const char *subtitle = READ_STRING();
	strncpy(m_szSubtitle, subtitle, sizeof(m_szSubtitle) - 1);
	m_szSubtitle[sizeof(m_szSubtitle) - 1] = '\0';
	
	// Read duration (in seconds)
	float duration = READ_BYTE() / 10.0f;  // Server sends as byte (duration * 10)
	
	ShowBanner(m_szTitle, m_szSubtitle, duration);
	
	return 1;
}

void CHudBanner::ShowBanner(const char *title, const char *subtitle, float duration)
{
	strncpy(m_szTitle, title, sizeof(m_szTitle) - 1);
	m_szTitle[sizeof(m_szTitle) - 1] = '\0';
	
	strncpy(m_szSubtitle, subtitle, sizeof(m_szSubtitle) - 1);
	m_szSubtitle[sizeof(m_szSubtitle) - 1] = '\0';
	
	m_flShowUntil = gHUD.m_flTime + duration;
	m_bActive = true;
	m_iFlags |= HUD_ACTIVE;
}

int CHudBanner::Draw(float fTime)
{
	if (!m_bActive)
		return 1;
	
	// Check if banner should still be displayed
	if (gHUD.m_flTime >= m_flShowUntil)
	{
		m_bActive = false;
		m_iFlags &= ~HUD_ACTIVE;
		return 1;
	}
	
	// Calculate fade effect (fade in first 0.3s, fade out last 0.5s)
	float timeRemaining = m_flShowUntil - gHUD.m_flTime;
	float totalDuration = m_flShowUntil - (gHUD.m_flTime - timeRemaining);
	float fadeInTime = 0.3f;
	float fadeOutTime = 0.5f;
	
	int alpha = 255;
	
	// Fade in
	if (totalDuration > 0 && (gHUD.m_flTime - (m_flShowUntil - totalDuration)) < fadeInTime)
	{
		float fadeProgress = (gHUD.m_flTime - (m_flShowUntil - totalDuration)) / fadeInTime;
		alpha = (int)(255 * fadeProgress);
	}
	// Fade out
	else if (timeRemaining < fadeOutTime)
	{
		float fadeProgress = timeRemaining / fadeOutTime;
		alpha = (int)(255 * fadeProgress);
	}
	
	// Banner dimensions - full width, 1/8 screen height
	int bannerHeight = ScreenHeight / 8;
	int bannerY = (ScreenHeight - bannerHeight) / 4;  // Centered vertically above
	
	// Draw semi-transparent black background
    int ra, ga, ba;
    UnpackRGB(ra, ga, ba, HudColor());
	int bgAlpha = (int)(alpha * 0.15f);
	FillRGBA(0, bannerY, ScreenWidth, bannerHeight, ra, ga, ba, bgAlpha);
	
	// Draw title text (centered) - use console font for larger text
	int titleWidth, titleHeight;
	gEngfuncs.pfnDrawConsoleStringLen(m_szTitle, &titleWidth, &titleHeight);
	
	int titleX = (ScreenWidth - titleWidth) / 2;
	int titleY = bannerY + (bannerHeight / 2) - (titleHeight / 2);
	
	// Set title color to white with fade
	float textAlpha = alpha / 255.0f;
	gEngfuncs.pfnDrawSetTextColor(ra / 255.0f, ga / 255.0f, ba / 255.0f);
	DrawConsoleString(titleX, titleY, m_szTitle);

	// Draw subtitle text (centered, below title)
	if (m_szSubtitle[0] != '\0')
	{
		int subtitleWidth = 0;
		for (const char *p = m_szSubtitle; *p; p++)
			subtitleWidth += gHUD.m_scrinfo.charWidths[*p];
		
		int subtitleX = (ScreenWidth - subtitleWidth) / 2;
		int subtitleY = titleY + 24;  // 8 pixels below title
		
		// Draw subtitle with lighter gray color and slightly reduced alpha
		int subtitleAlpha = (int)(alpha * 0.85f);
		int r = 180; int g = 180; int b = 180;
		ScaleColors(r, g, b, subtitleAlpha);
		gHUD.DrawHudString(subtitleX, subtitleY, ScreenWidth, m_szSubtitle, r, g, b);
	}
	
	return 1;
}
