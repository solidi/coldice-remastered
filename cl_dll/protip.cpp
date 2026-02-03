#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include <stdio.h>

#define QUEUE_SIZE 3
#define START_POSITION (ScreenHeight / 2) - (ScreenHeight * .25)
#define SECONDS_TO_LIVE 10

extern cvar_t *cl_showtips;

int CHudProTip::Init()
{
	memset(&m_MessageQueue, 0, sizeof(m_MessageQueue));
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem(this);
	return 1;
}

int CHudProTip::VidInit()
{
	int spr_index = gHUD.GetSpriteIndex( "mouse" );
	m_hMouseClick = gHUD.GetSprite( spr_index );
	return 1;
}

void CHudProTip::AddMessage(int id, const char *message)
{
	if (m_ShownTip[id])
		return;

	if (g_iUser1)
		return;

	if (gHUD.m_iIntermission)
		return;

	// Filter out unwanted messaages.
	if (gHUD.m_GameMode == GAME_PROPHUNT)
	{
		cl_entity_t *player = gEngfuncs.GetLocalPlayer();
		if (player)
		{
			if (player->curstate.fuser4 > 0)
			{
				if (id != PROP_TIP)
					return;
			}
			else
			{
				if (id == PROP_TIP)
					return;
			}
		}
	} else {
		if (id == PROP_TIP)
		{
			m_ShownTip[id] = true;
			return;
		}
	}

	m_ShownTip[id] = true;

	float time = gEngfuncs.GetClientTime() + SECONDS_TO_LIVE;
	int oldest = 999999, insert = 0;

	for (int i = 0; i < QUEUE_SIZE; i++)
	{
		if (oldest > m_MessageQueue[i].time)
		{
			insert = i;
			oldest = m_MessageQueue[i].time;
		}
	}

	int y = (int)START_POSITION;

	protip_s tip = {message, time, y};
	m_MessageQueue[insert] = tip;
	//gEngfuncs.Con_DPrintf("added message=%s, index=%d, time=%.2f, y_pos=%d!\n",
	//	m_MessageQueue[insert].message, insert, m_MessageQueue[insert].time, m_MessageQueue[insert].y_pos);

	// Push back the text tip
	gHUD.m_iShownHelpMessage = gHUD.m_flTime + gEngfuncs.pfnRandomFloat( 120, 240 );
}

int CHudProTip::Draw(float flTime)
{
	if (cl_showtips && !cl_showtips->value) {
		return 1;
	}

	if (g_iUser1)
		return 1;

	if (gHUD.m_Scoreboard.m_iShowscoresHeld || gHUD.m_iShowingWeaponMenu)
		return 1;

	if (gHUD.m_iIntermission)
		return 1;

	for (int i = 0; i < QUEUE_SIZE; i++)
	{
		if (m_MessageQueue[i].time > 0 &&
			m_MessageQueue[i].time > gEngfuncs.GetClientTime() &&
			m_MessageQueue[i].time <= (gEngfuncs.GetClientTime() + SECONDS_TO_LIVE + 3)) // in case of large values
		{
			int y = m_MessageQueue[i].y_pos;
			y += 10;

			int max = ScreenHeight * 0.4;
			if (i == 1)
				max -= 42;
			if (i == 2)
				max -= 84;

			if (y > ScreenHeight - max)
				y = ScreenHeight - max;

			m_MessageQueue[i].y_pos = y;

			int fade_time = 2;
			int start = (m_MessageQueue[i].time - fade_time);
			int a = MAX_ALPHA, r = 200, g = 200, b = 200;
			if (gEngfuncs.GetClientTime() >= start)
				a = fmin(MAX_ALPHA * ((m_MessageQueue[i].time - gEngfuncs.GetClientTime()) / fade_time), MAX_ALPHA);
			ScaleColors(r, g, b, a);

			// Get actual sprite dimensions
			wrect_t *rect = &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("mouse"));
			int iconWidth = rect->right - rect->left;
			int gap = 8; // Gap between icon and text
			
			// Calculate text width using HUD font character widths
			int textWidth = 0;
			for (const char *p = m_MessageQueue[i].message; *p; p++)
				textWidth += gHUD.m_scrinfo.charWidths[*p];
			
			int totalWidth = iconWidth + gap + textWidth;
			
			// Center the entire protip (icon + gap + text) on screen
			int startX = (ScreenWidth - totalWidth) / 2;
			int textX = startX + iconWidth + gap;
			
			gHUD.DrawHudString(textX, m_MessageQueue[i].y_pos, ScreenWidth, (char *)m_MessageQueue[i].message, r, g, b );

			UnpackRGB(r,g,b, HudColor());
			ScaleColors(r, g, b, a);
			SPR_Set(m_hMouseClick, r, g, b);
			SPR_DrawAdditive(0, startX, y - 12, rect);
		}
		else
			m_MessageQueue[i].time = 0;
	}

	return 1;
}
