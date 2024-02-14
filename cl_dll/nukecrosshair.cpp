#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include <stdio.h>

#define GUIDE_S SPR_Width(m_hCrosshair, 0)
#define READOUT_S 128

DECLARE_MESSAGE( m_NukeCrosshair, NukeCross );

int CHudNukeCrosshair::Init()
{
	m_iHudMode = 0;
	m_iFlags |= HUD_ACTIVE;
	HOOK_MESSAGE(NukeCross);
	gHUD.AddHudElem(this);
	return 1;
}

int CHudNukeCrosshair::VidInit()
{
	m_hCrosshair = SPR_Load("sprites/nuke_crosshair.spr");
	m_hStatic = SPR_Load("sprites/nuke_flicker.spr");
	m_iHudMode = 0;
	return 1;
}

extern float g_lastFOV;

int CHudNukeCrosshair::MsgFunc_NukeCross(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );
	m_iHudMode = READ_BYTE();
	if (m_iHudMode == 0) {
		g_lastFOV = 0;
		gHUD.m_iFOV = CVAR_GET_FLOAT( "default_fov" );
	}
	return 1;
}

int CHudNukeCrosshair::Draw(float flTime)
{
	int x, y, w, h;
	int frame;

	if (m_iHudMode == 1)
	{
		y = (ScreenWidth - GUIDE_S) / 2;
		x = (ScreenHeight - GUIDE_S) / 2;

		int r,g,b;
		UnpackRGB(r,g,b, HudColor());
		/*
		SPR_Set(m_hCrosshair, r, g, b);
		SPR_DrawAdditive(0, y, x, NULL);
		*/

		SPR_Set(m_hStatic, r, g, b);
		// play at 15fps
		frame = (int)(flTime * 15) % SPR_Frames(m_hStatic);
		y = x = 0;
		w = SPR_Width(m_hStatic, 0);
		h = SPR_Height(m_hStatic, 0);
		for(y = -(rand() % h); y < ScreenHeight; y += h) 
		for(x = -(rand() % w); x < ScreenWidth; x += w) 
		SPR_DrawAdditive(frame, x, y, NULL);
	}

	return 1;
}
