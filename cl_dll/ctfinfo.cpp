#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include <stdio.h>
#include "vgui_TeamFortressViewport.h"

DECLARE_MESSAGE( m_CtfInfo, CtfInfo );

int CHudCtfInfo::Init()
{
	m_iFlags |= HUD_ACTIVE;
	HOOK_MESSAGE(CtfInfo);
	gHUD.AddHudElem(this);
	return 1;
}

int CHudCtfInfo::VidInit()
{
	return 1;
}

int CHudCtfInfo::MsgFunc_CtfInfo(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	int bluescore = READ_BYTE();
	if (bluescore != 255)
		m_iBlueScore = bluescore;
	int redscore = READ_BYTE();
	if (redscore != 255)
		m_iRedScore = redscore;
	int bluemode = READ_BYTE();
	if (bluemode != 255)
		m_iBlueMode = bluemode;
	int redmode = READ_BYTE();
	if (redmode != 255)
		m_iRedMode = redmode;
	return 1;
}

int CHudCtfInfo::Draw(float flTime)
{
	if (gHUD.m_GameMode != GAME_CTF)
		return 1;

	if (gHUD.m_Scoreboard.m_iShowscoresHeld)
		return 1;

	//if (gHUD.m_Health.m_iHealth <= 0)
	//	return 1;

	if (gHUD.m_iIntermission)
		return 1;

	//if (gHUD.m_iShowingWeaponMenu)
	//	return 1;

	if (gViewPort->IsScoreBoardVisible())
		return 1;

	int r, g, b;
	int y = (ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2) - 64;
	int x = 24;

	// Red
	{
		UnpackRGB(r, g, b, RGB_REDISH);
		ScaleColors(r, g, b, MIN_ALPHA );
		switch (m_iRedMode)
		{
			case 0:
				SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("item_ctfflagh")), r, g, b);
				SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("item_ctfflagh")));
				break;
			 case 1:
				SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("item_ctfflagc")), r, g, b);
				SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("item_ctfflagc")));
				break;
			 case 2:
				SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("item_ctfflagd")), r, g, b);
				SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("item_ctfflagd")));
				break;
			 case 3:
				SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("item_ctfflagg")), r, g, b);
				SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("item_ctfflagg")));
				break;
		}
		gHUD.DrawHudNumber(x + 48, y + 16, DHN_2DIGITS | DHN_DRAWZERO, m_iRedScore, r, g, b);
	}

	y -= 48;

	// Blue
	{
		UnpackRGB(r, g, b, HudColor());
		ScaleColors(r, g, b, MIN_ALPHA );
		switch (m_iBlueMode)
		{
			case 0:
				SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("item_ctfflagh")), r, g, b);
				SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("item_ctfflagh")));
				break;
			 case 1:
				SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("item_ctfflagc")), r, g, b);
				SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("item_ctfflagc")));
				break;
			 case 2:
				SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("item_ctfflagd")), r, g, b);
				SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("item_ctfflagd")));
				break;
			 case 3:
				SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("item_ctfflagg")), r, g, b);
				SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("item_ctfflagg")));
				break;
		}
		gHUD.DrawHudNumber(x + 48, y + 16, DHN_2DIGITS | DHN_DRAWZERO, m_iBlueScore, r, g, b);
	}

	return 1;
}
