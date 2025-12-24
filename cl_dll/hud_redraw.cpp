/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
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
// hud_redraw.cpp
//
#include <math.h>
#include "hud.h"
#include "cl_util.h"
#include "bench.h"
#include <string>

#include "vgui_TeamFortressViewport.h"

#ifndef __APPLE__
#include "GL/gl.h"
#endif

#define MAX_LOGO_FRAMES 56

int grgLogoFrame[MAX_LOGO_FRAMES] = 
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 14, 15,
	16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
	29, 29, 29, 29, 29, 28, 27, 26, 25, 24, 30, 31 
};


extern int g_iVisibleMouse;
extern float g_WallClimb;

float HUD_GetFOV( void );

extern cvar_t *sensitivity;
extern cvar_t *cl_showtips;
extern cvar_t *cl_hudbend;
extern cvar_t *cl_respawnbar;

extern float g_NotifyTime;

// Think
void CHud::Think(void)
{
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);

	int newfov;
	HUDLIST *pList = m_pHudList;

	while (pList)
	{
		if (pList->p->m_iFlags & HUD_ACTIVE)
			pList->p->Think();
		pList = pList->pNext;
	}

	newfov = HUD_GetFOV();
	if ( newfov == 0 )
	{
		m_iFOV = default_fov->value;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == default_fov->value )
	{  
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{  
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = sensitivity->value * ((float)newfov / (float)default_fov->value) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	// think about default fov
	if ( m_iFOV == 0 )
	{  // only let players adjust up in fov,  and only if they are not overriden by something else
		m_iFOV = fmax( default_fov->value, 90 );  
	}
	
	if ( gEngfuncs.IsSpectateOnly() )
	{
		m_iFOV = gHUD.m_Spectator.GetFOV();	// default_fov->value;
	}

	Bench_CheckStart();

	ShowTextTips();

	if (MutatorEnabled(MUTATOR_NOTIFY)) {
		if (g_NotifyTime < gEngfuncs.GetClientTime()) {
			char soundPath[32];
			sprintf(soundPath, "notify%d.wav", gEngfuncs.pfnRandomLong(1, 23));
			PlaySound(soundPath, 2);
			g_NotifyTime = gEngfuncs.GetClientTime() + gEngfuncs.pfnRandomFloat(3, 6);
		}
	}
}

void HUD_DrawOrthoTriangles();

void CHud :: Mirror()
{
#ifndef __APPLE__
	extern cvar_t *cl_screeneffects;
	if (cl_screeneffects && !cl_screeneffects->value)
		return;

	int width = ScreenWidth;
	int height = ScreenHeight;

	glBlendFunc(GL_SRC_ALPHA, GL_ZERO);
	glEnable(GL_BLEND);

	// bind texture
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, 32767);
	glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA8, 0, 0, width, height, 0);

	// enable some OpenGL stuff
	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glColor3f(1, 1, 1);
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, 1, 1, 0, 0.1, 100);

	glBindTexture(GL_TEXTURE_RECTANGLE_NV, 32767);

	glColor4f(1, 1, 1, 1);

	glBegin(GL_QUADS);

	glTexCoord2f(width, 0);
	glVertex3f(0, 1, -1);
	glTexCoord2f(width, height);
	glVertex3f(0, 0, -1);
	glTexCoord2f(0, height);
	glVertex3f(1, 0, -1);
	glTexCoord2f(0, 0);
	glVertex3f(1, 1, -1);

	glEnd();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glDisable(GL_TEXTURE_RECTANGLE_NV);
	glEnable(GL_DEPTH_TEST);


	glDisable(GL_BLEND);
#endif
}

// Redraw
// step through the local data,  placing the appropriate graphics & text as appropriate
// returns 1 if they've changed, 0 otherwise
int CHud :: Redraw( float flTime, int intermission )
{
#ifdef _WIN32
	extern cvar_t *cl_portalmirror;
	if (cl_portalmirror && cl_portalmirror->value)
	{
		std::string port1 = "Portal 1 origin: " + std::to_string(gHUD.portal1finalorg.x) + " " + std::to_string(gHUD.portal1finalorg.y) + " " + std::to_string(gHUD.portal1finalorg.z);
		std::string port2 = "Portal 2 origin: " + std::to_string(gHUD.portal2finalorg.x) + " " + std::to_string(gHUD.portal2finalorg.y) + " " + std::to_string(gHUD.portal2finalorg.z);
		gHUD.DrawHudString(100, 100, 512, (char *)port1.c_str(), 0, 0, 255);
		gHUD.DrawHudString(100, 130, 512, (char *)port2.c_str(), 255, 0, 0);
	}
#endif

	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = flTime;
	m_flTimeDelta = (double)m_flTime - m_fOldTime;
	static float m_flShotTime = 0;
	
	// Clock was reset, reset delta
	if ( m_flTimeDelta < 0 )
		m_flTimeDelta = 0;

	// Bring up the scoreboard during intermission
	if (gViewPort)
	{
		if ( m_iIntermission && !intermission )
		{
			// Have to do this here so the scoreboard goes away
			m_iIntermission = intermission;
			gViewPort->HideCommandMenu();
			gViewPort->HideScoreBoard();
			gViewPort->UpdateSpectatorPanel();
		}
		else if ( !m_iIntermission && intermission )
		{
			m_iIntermission = intermission;
			gViewPort->HideCommandMenu();
			// Allow GUI for voting at end of map
			// gViewPort->HideVGUIMenu();
			gViewPort->ShowScoreBoard();
			gViewPort->UpdateSpectatorPanel();

			// Take a screenshot if the client's got the cvar set
			if ( CVAR_GET_FLOAT( "hud_takesshots" ) != 0 )
				m_flShotTime = flTime + 1.0;	// Take a screenshot in a second
		}
	}

	if (m_flShotTime && m_flShotTime < flTime)
	{
		gEngfuncs.pfnClientCmd("snapshot\n");
		m_flShotTime = 0;
	}

	m_iIntermission = intermission;

	// if no redrawing is necessary
	// return 0;

#ifdef _WIN32
	if (cl_portalmirror && cl_portalmirror->value)
		glDepthRange(0.0f, 0.0f);
#endif

	int r, g, b;
	UnpackRGB(r, g, b, HudColor());
	iTeamColors[0][0] = r;
	iTeamColors[0][1] = g;
	iTeamColors[0][2] = b;
	
	// draw all registered HUD elements
	if ( m_pCvarDraw->value )
	{
		HUDLIST *pList = m_pHudList;

		while (pList)
		{
			if ( !Bench_Active() )
			{
				if ( !intermission )
				{
					if ( (pList->p->m_iFlags & HUD_ACTIVE) && !(m_iHideHUDDisplay & HIDEHUD_ALL) )
						pList->p->Draw(flTime);
				}
				else
				{  // it's an intermission,  so only draw hud elements that are set to draw during intermissions
					if ( pList->p->m_iFlags & HUD_INTERMISSION )
						pList->p->Draw( flTime );
				}
			}
			else
			{
				if ( ( pList->p == &m_Benchmark ) &&
					 ( pList->p->m_iFlags & HUD_ACTIVE ) &&
					 !( m_iHideHUDDisplay & HIDEHUD_ALL ) )
				{
					pList->p->Draw(flTime);
				}
			}

			pList = pList->pNext;
		}
	}

	if (MutatorEnabled(MUTATOR_MIRROR) && !g_iUser1)
		Mirror();

	HUD_DrawOrthoTriangles();

	// are we in demo mode? do we need to draw the logo in the top corner?
	if (m_iLogo)
	{
		int x, y, i;

		if (m_hsprLogo == 0)
			m_hsprLogo = LoadSprite("sprites/%d_logo.spr");

		SPR_Set(m_hsprLogo, 250, 250, 250 );
		
		x = SPR_Width(m_hsprLogo, 0);
		x = ScreenWidth - x;
		y = SPR_Height(m_hsprLogo, 0)/2;

		// Draw the logo at 20 fps
		int iFrame = (int)(flTime * 20) % MAX_LOGO_FRAMES;
		i = grgLogoFrame[iFrame] - 1;

		SPR_DrawAdditive(i, x, y, NULL);
	}

#ifdef _WIN32
	if (cl_portalmirror && cl_portalmirror->value)
		glDepthRange(0.0f, 1.0f);
#endif

	/*
	if ( g_iVisibleMouse )
	{
		void IN_GetMousePos( int *mx, int *my );
		int mx, my;

		IN_GetMousePos( &mx, &my );
		
		if (m_hsprCursor == 0)
		{
			char sz[256];
			sprintf( sz, "sprites/cursor.spr" );
			m_hsprCursor = SPR_Load( sz );
		}

		SPR_Set(m_hsprCursor, 250, 250, 250 );
		
		// Draw the logo at 20 fps
		SPR_DrawAdditive( 0, mx, my, NULL );
	}
	*/

	if (m_ShowKeyboard)
	{
		HSPRITE m_hStatic = SPR_Load("sprites/keyboard.spr");
		SPR_Set(m_hStatic, r, g, b);
		int w = SPR_Width(m_hStatic, 0);
		int h = SPR_Height(m_hStatic, 0);
		SPR_DrawAdditive(0, (ScreenWidth / 2) - 256, ((ScreenHeight / 2) - 128), NULL);
	}

	if (cl_respawnbar && cl_respawnbar->value)
	{
		if (m_fPlayerDeadTime && m_fPlayerDeadTime > gEngfuncs.GetClientTime())
		{
			int max = fmin(ScreenWidth / 2, 350), height = 18, w = 0, h = 0;
			float timeLeft = fmin((m_fPlayerDeadTime - gEngfuncs.GetClientTime()) * 100, max);
			char message[64];
			sprintf(message, "Can respawn in %d ...\n",  (int)m_fPlayerDeadTime - (int)gEngfuncs.GetClientTime());
			GetConsoleStringSize(message, &w, &h);
			DrawConsoleString(ScreenWidth / 2 - (w / 2), ScreenHeight - (ScreenHeight * .2) + (height), message);
			FillRGBA(ScreenWidth / 2 - (max / 2), ScreenHeight - (ScreenHeight * .2), max, height, r, g, b, 20);
			FillRGBA(ScreenWidth / 2 - (timeLeft / 2), ScreenHeight - (ScreenHeight * .2), timeLeft, height, r, g, b, 164);
		}
	}

	if (g_WallClimb && g_WallClimb < gEngfuncs.GetClientTime())
	{
		gHUD.m_WallClimb.m_iFlags &= ~HUD_ACTIVE;
		g_WallClimb = 0;
	}

	if (!g_WallClimb && gHUD.m_WallClimb.m_iFlags & HUD_ACTIVE)
		gHUD.m_WallClimb.m_iFlags &= ~HUD_ACTIVE;

	local_player_index = gEngfuncs.GetLocalPlayer()->index;

	return 1;
}

void ScaleColors( int &r, int &g, int &b, int a )
{
	float x = (float)a / 255;
	r = (int)(r * x);
	g = (int)(g * x);
	b = (int)(b * x);
}

int CHud :: DrawHudString(int xpos, int ypos, int iMaxX, char *szIt, int r, int g, int b )
{
	return xpos + gEngfuncs.pfnDrawString( xpos, ypos, szIt, r, g, b);
}

int CHud :: DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b )
{
	char szString[32];
	sprintf( szString, "%d", iNumber );
	// TRI_SprAdjustSize(&xpos, &ypos, 0, 0);
	return DrawHudStringReverse( xpos, ypos, iMinX, szString, r, g, b );

}

// draws a string from right to left (right-aligned)
int CHud :: DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b )
{
	return xpos - gEngfuncs.pfnDrawStringReverse( xpos, ypos, szString, r, g, b);
}

int CHud :: DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b)
{
	int iWidth = GetSpriteRect(m_HUD_number_0).right - GetSpriteRect(m_HUD_number_0).left;
	int k;
	float drift = 1;
	if (x > (ScreenWidth / 2))
		drift *= -1;
	// No drift when off
	if (!cl_hudbend->value)
		drift = 0;
	float numScale = 24 * cl_hudbend->value;
	
	if (iNumber > 0)
	{
		// 4th digit 
		if (iNumber >= 1000)
		{
			k = iNumber / 1000;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if (iFlags & (DHN_4DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw 100's
		if (iNumber >= 100)
		{
			k = (iNumber % 1000) / 100;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y - (drift), &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if (iFlags & (DHN_3DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw 10's
		if (iNumber >= 10)
		{
			k = (iNumber % 100)/10;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y - (drift * numScale), &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if (iFlags & (DHN_3DIGITS | DHN_2DIGITS))
		{
			if (iFlags & DHN_PADZERO)
			{
				SPR_Set(GetSprite(m_HUD_number_0), r, g, b);
				SPR_DrawAdditive(0, x, y, &GetSpriteRect(m_HUD_number_0));
			}
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
		SPR_DrawAdditive(0,  x, y - (drift * (numScale * 2)), &GetSpriteRect(m_HUD_number_0 + k));
		x += iWidth;
	} 
	else if (iFlags & DHN_DRAWZERO) 
	{
		SPR_Set(GetSprite(m_HUD_number_0), r, g, b );

		if (iFlags & (DHN_4DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw 100's
		if (iFlags & (DHN_3DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		if (iFlags & (DHN_3DIGITS | DHN_2DIGITS))
		{
			if (iFlags & DHN_PADZERO)
			{
				SPR_Set(GetSprite(m_HUD_number_0), r, g, b);
				SPR_DrawAdditive(0, x, y, &GetSpriteRect(m_HUD_number_0));
			}
			x += iWidth;
		}

		// SPR_Draw ones
		
		SPR_DrawAdditive( 0,  x, y - (drift * (numScale * 2)), &GetSpriteRect(m_HUD_number_0));
		x += iWidth;
	}

	return x;
}


int CHud::GetNumWidth( int iNumber, int iFlags )
{
	if (iFlags & (DHN_4DIGITS))
		return 4;

	if (iFlags & (DHN_3DIGITS))
		return 3;

	if (iFlags & (DHN_2DIGITS))
		return 2;

	if (iNumber <= 0)
	{
		if (iFlags & (DHN_DRAWZERO))
			return 1;
		else
			return 0;
	}

	if (iNumber < 10)
		return 1;

	if (iNumber < 100)
		return 2;

	if (iNumber < 1000)
		return 3;

	return 4;

}	


void CHud::ShowTextTips( void ) {
	if (cl_showtips && !cl_showtips->value) {
		return;
	}

	const int MESSAGE_SIZE = 39;

	const char* messageList[MESSAGE_SIZE] = {
		"Tired of blue skins? Type \"cl_icemodels 0\" in the console switches to real-life skins.\n",
		"To five-high your friend with your leg, bind \"impulse 206\" to a button to kick.\n",
		"Did you know you can shut off the humour? Type \"cl_announcehumor 0\" to make them shut up.\n",
		"To stop these tips, type \"cl_showtips 0\".\n",
		"For updates to this mod, see http://moddb.com/mods/cold-ice-remastered\n",
		"This mod supports a grappling hook. If allowed by the server, bind \"+hook\" to deploy.\n",
		"Cold Ice Remastered contains works from the community. For all credits, see readme.txt.\n",
		"Swap between single and dual weapons, if available, bind \"impulse 205\".\n",
		"To enable experimental model shadows, type \"cl_shadows 1\".\n",
		"To five-high your friend with your hand, bind \"impulse 207\" to a button to punch.\n",
		"While looking ahead, tap \"+forward\" three times to perform a slide kick.\n",
		"Did you know you can pick up live grenades? Look at a live grenade and tap \"+use\".\n",
		"Did you know you can kick live grenades? Walk up to a live grenade and kick it!\n",
		"Type \"cl_righthand 0\" for weapons held in the left hand.\n",
		"To drop a rune, type \"drop_rune\" in the console or bind it to a key.\n",
		"To turn off lifemeters of other players, type \"cl_lifemeters 0\" in the console.\n",
		"Fast fragging announcements can be reduced by typing \"cl_achievements 1\" in the console.\n",
		"To turn off weapon and rune messages, type \"cl_minfo 0\" in the console.\n",
		"Throw a grenade if you have one, bind \"impulse 209\" to a button.\n",
		"To turn off keyboard flips and slide, type \"cl_kacro 0\".\n",
		"To turn screen roll while flipping, type \"cl_antivomit 0\" in the console.\n",
		"Bind \"impulse 210\" to a button to perform a right flip.\n",
		"Tap \"+back\" three times to perform a backflip.\n",
		"Tap \"+jump\" against a wall to perform a wall climb.\n",
		"To turn off weather effects, type \"cl_weather 0\" into the console.\n",
		"Type \"cl_wallclimbindicator 0\" to turn off the hint above.\n",
		"Join our Cold Ice Discord at discord.com/invite/Hu2Q6pcJn3\n",
		"To fake your death, type \"feign\" in the console or bind it to a key.\n",
		"Custom voiceover support is available. Try cl_voiceoverpath \"hev\".\n",
		"Type \"cl_icemodels 4\" in the console for real-life skins with a blue hev.\n",
		"Tap \"+jump\" two times to perform a double jump.\n",
		"Tap \"+jump\" three times while running to perform a frontflip.\n",
		"Bind \"impulse 211\" to a button to perform a left flip.\n",
		"Bind \"impulse 212\" to a button to perform a back flip.\n",
		"Bind \"impulse 213\" to a button to perform a front flip.\n",
		"Bind \"feign\" to pass out, at least for a minute.\n",
		"Type \"vote\" in the chat to start a vote request.\n",
		"Bind \"taunt\" to let the competition know how you feel.\n",
		"Type \"mutator\" in the chat to start a rtv request.\n",
	};

	// Unstick after a level change
	if  (m_iShownHelpMessage > (m_flTime + 300)) {
		m_iShownHelpMessage = gEngfuncs.pfnRandomFloat(15, 30);
	}

	if (m_iShownHelpMessage < m_flTime) {
		gHUD.m_SayText.SayTextPrint(messageList[gEngfuncs.pfnRandomLong(0, MESSAGE_SIZE - 1)], 128);
		m_iShownHelpMessage = m_flTime + gEngfuncs.pfnRandomFloat( 120, 240 );
	}
}

