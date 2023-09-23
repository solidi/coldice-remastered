#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include <stdio.h>
#include "vgui_TeamFortressViewport.h"

DECLARE_MESSAGE( m_Objective, Objective );

extern cvar_t *cl_radar;
extern cvar_t *cl_objectives;

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
	strcpy(m_szGoalMessage, READ_STRING());
	strcpy(m_szInfoMessage, READ_STRING());
	m_iPercent = READ_BYTE();
	strcpy(m_szWinsMessage, READ_STRING());

	//m_iRoundWins = READ_BYTE();
	//m_iRoundPlays = READ_BYTE();

	return 1;
}

int CHudObjective::Draw(float flTime)
{
	if (g_iUser1)
		return 1;
	
	if (gHUD.m_Scoreboard.m_iShowscoresHeld)
		return 1;

	if (gHUD.m_Health.m_iHealth <= 0)
		return 1;

	if (gHUD.m_iIntermission)
		return 1;

	if (gHUD.m_iShowingWeaponMenu)
		return 1;

	if (gViewPort->IsScoreBoardVisible())
		return 1;

	if (!cl_objectives->value)
		return 1;

	int r, g, b;
	int size = 0;
	int y = 16;
	int x = cl_radar->value ? 130 : 0;
	int padding = 32;
	UnpackRGB(r, g, b, HudColor());

	if (strlen(m_szGoalMessage))
	{
		size = ConsoleStringLen(m_szGoalMessage);
		SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("target")), r, g, b);
		SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("target")));
		DrawConsoleString(x + (padding - 4), y + 6, m_szGoalMessage);
		FillRGBA(x, y, size + padding, gHUD.m_iFontHeight, r, g, b, 20);
	}

	if (strlen(m_szInfoMessage))
	{
		y += gHUD.m_iFontHeight;
		size = ConsoleStringLen(m_szInfoMessage);
		SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("info")), r, g, b);
		SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("info")));
		DrawConsoleString(x + (padding - 4), y + 6, m_szInfoMessage);

		float result = (size + padding) * (float(m_iPercent) / 100);
		FillRGBA(x, y, size + padding, gHUD.m_iFontHeight, r, g, b, 20);
		FillRGBA(x, y + gHUD.m_iFontHeight + 1, (int)result, 1, r, g, b, 200);
	}

	if (strlen(m_szWinsMessage))
	{
		y += gHUD.m_iFontHeight;
		size = ConsoleStringLen(m_szWinsMessage);
		SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("trophy")), r, g, b);
		SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("trophy")));
		DrawConsoleString(x + (padding - 4), y + 6, m_szWinsMessage);
		FillRGBA(x, y, size + padding, gHUD.m_iFontHeight, r, g, b, 20);
	}

	return 1;
}
