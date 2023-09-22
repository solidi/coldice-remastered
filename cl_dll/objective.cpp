#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include <stdio.h>
#include "vgui_TeamFortressViewport.h"

DECLARE_MESSAGE( m_Objective, Objective );

int CHudObjective::Init()
{
	m_iFlags |= HUD_ACTIVE;
	HOOK_MESSAGE(Objective);
	gHUD.AddHudElem(this);
	return 1;
}

int CHudObjective::VidInit()
{
	return 1;
}

int CHudObjective::MsgFunc_Objective(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );
	show = READ_BYTE();
	strcpy(string1, READ_STRING());
	strcpy(string2, READ_STRING());
    percent = READ_BYTE();
	return 1;
}

int CHudObjective::Draw(float flTime)
{
	if (g_iUser1)
		return 1;
	
	if (gHUD.m_Scoreboard.m_iShowscoresHeld)
		return 1;

	if (gHUD.m_iShowingWeaponMenu)
		return 1;

	if (gViewPort->IsScoreBoardVisible())
		return 1;

	if (show)
	{
		int r, g, b;
		int y = 16;
		int x = 130;
        int padding = 32;
		UnpackRGB(r, g, b, HudColor());

		int size = ConsoleStringLen(string1);
		SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("target")), r, g, b);
		SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("target")));
		DrawConsoleString(x + (padding - 4), y + 6, string1);
		FillRGBA(x, y, size + padding, gHUD.m_iFontHeight, r, g, b, 20);

		if (strlen(string2))
		{
			size = ConsoleStringLen(string2);
			SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("info")), r, g, b);
			SPR_DrawAdditive(0, x, y + gHUD.m_iFontHeight, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("info")));
			DrawConsoleString(x + (padding - 4), y + gHUD.m_iFontHeight + 6, string2);

            float result = (size + padding) * (float(percent) / 100);
            //gEngfuncs.Con_DPrintf(">> percent=%d, result=%f\n", percent, result);
			FillRGBA(x, y + gHUD.m_iFontHeight, size + padding, gHUD.m_iFontHeight, r, g, b, 20);
			FillRGBA(x, y + gHUD.m_iFontHeight + gHUD.m_iFontHeight + 1, (int)result, 1, r, g, b, 200);
		}
	}
	return 1;
}
