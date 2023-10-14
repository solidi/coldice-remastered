#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include <stdio.h>
#include "vgui_TeamFortressViewport.h"

DECLARE_MESSAGE( m_Timer, RoundTime )
DECLARE_MESSAGE( m_Timer, ShowTimer )

int CHudTimer::Init()
{
	HOOK_MESSAGE( RoundTime );
	HOOK_MESSAGE( ShowTimer );
	m_iFlags = 0;
	m_bPanicColorChange = false;
	gHUD.AddHudElem(this);
	//m_iFlags |= HUD_ACTIVE;
    //m_iTime = 75;
	return 1;
}

int CHudTimer::VidInit()
{
	//m_HUD_timer = gHUD.GetSpriteIndex( "stopwatch" );
	return 1;
}

int CHudTimer::Draw( float fTime )
{
	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH))
		return 1;

	if (!(gHUD.m_iWeaponBits & (1<<(WEAPON_SUIT))))
		return 1;

	if (gHUD.m_WallClimb.m_iFlags & HUD_ACTIVE)
		return 1;

	if (gHUD.m_Scoreboard.m_iShowscoresHeld)
		return 1;

	if (gViewPort->IsScoreBoardVisible())
		return 1;

	int r, g, b;
	// time must be positive
	int minutes = fmax( 0, (int)( m_iTime + m_fStartTime - gHUD.m_flTime ) / 60);
	int seconds = fmax( 0, (int)( m_iTime + m_fStartTime - gHUD.m_flTime ) - (minutes * 60));
    float percent = fmax(0, (m_iTime + m_fStartTime - gHUD.m_flTime) / (m_iTime + m_fStartTime));
    //gEngfuncs.Con_DPrintf("percent = %.2f\n", percent);

	if ( minutes * 60 + seconds > 20 )
	{
		UnpackRGB(r, g, b, HudColor());
	}
	else
	{
		m_flPanicTime += gHUD.m_flTimeDelta;
		// add 0.1 sec, so it's not flicker fast
		if ( m_flPanicTime > ((float)seconds / 40.0f) + 0.1f)
		{
			m_flPanicTime = 0;
			m_bPanicColorChange = !m_bPanicColorChange;
		}
		UnpackRGB(r, g, b, m_bPanicColorChange ? HudColor() : RGB_REDISH);
	}

	ScaleColors(r, g, b, MIN_ALPHA);

	int iWatchWidth = 0;// gHUD.GetSpriteRect(m_HUD_timer).right - gHUD.GetSpriteRect(m_HUD_timer).left;

	int x = ScreenWidth / 2 - ((gHUD.m_iFontHeight * 4) / 2);
	int y = 28; //ScreenHeight - 1.5 * gHUD.m_iFontHeight;

	//SPR_Set(gHUD.GetSprite(m_HUD_timer), r, g, b);
	//SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_timer));

	FillRGBA(x - gHUD.m_iFontHeight / 4, y - gHUD.m_iFontHeight / 2, gHUD.m_iFontHeight * 4, gHUD.m_iFontHeight * 2, r, g, b, 20);
	
    // Bar indicator
    int width = (gHUD.m_iFontHeight * 4) * percent;
    int difference = (gHUD.m_iFontHeight * 4) - width;
	FillRGBA((x - gHUD.m_iFontHeight / 4) + (difference / 2), (y - gHUD.m_iFontHeight / 2) + gHUD.m_iFontHeight * 2, width, 1, r, g, b, 200);

	x = gHUD.DrawHudNumber(x + iWatchWidth / 4, y, DHN_2DIGITS | DHN_DRAWZERO | DHN_PADZERO, minutes, r, g, b);

    // Colon
	FillRGBA(x + iWatchWidth / 4, y + gHUD.m_iFontHeight / 4, 2, 2, r, g, b, 100);
	FillRGBA(x + iWatchWidth / 4, y + gHUD.m_iFontHeight - gHUD.m_iFontHeight / 4, 2, 2, r, g, b, 100);

	gHUD.DrawHudNumber(x + iWatchWidth / 2, y, DHN_2DIGITS | DHN_DRAWZERO | DHN_PADZERO, seconds, r, g, b);

	return 1;
}

int CHudTimer::MsgFunc_RoundTime(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );
	m_iTime = READ_SHORT();
	m_fStartTime = gHUD.m_flTime;
	m_iFlags |= HUD_ACTIVE;
	return 1;
}

int CHudTimer::MsgFunc_ShowTimer(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );
	int show = READ_BYTE();
	if (show)
		m_iFlags |= HUD_ACTIVE;
	else
		m_iFlags &= ~HUD_ACTIVE;

	return 1;
}
