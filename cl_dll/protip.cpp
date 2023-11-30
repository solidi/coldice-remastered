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

	if (gHUD.m_Scoreboard.m_iShowscoresHeld)
		return 1;

	if (gHUD.m_iIntermission)
		return 1;

	for (int i = 0; i < QUEUE_SIZE; i++)
	{
		//gEngfuncs.Con_DPrintf("q index=%d, time=%.2f, ctime=%.2f\n", i, m_MessageQueue[i].time, gEngfuncs.GetClientTime());

		if (m_MessageQueue[i].time > 0 && m_MessageQueue[i].time > gEngfuncs.GetClientTime())
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

			//int a = (int)(fabs(sin((m_MessageQueue[i].time - gEngfuncs.GetClientTime()) * 1)) * 256.0);
			int fade_time = 2;
			int start = (m_MessageQueue[i].time - fade_time);
			int a = 200, r = 200, g = 200, b = 200;
			if (gEngfuncs.GetClientTime() >= start)
				a = fmin(200 * ((m_MessageQueue[i].time - gEngfuncs.GetClientTime()) / fade_time), 200);
			ScaleColors(r, g, b, a);

			int size = ConsoleStringLen(m_MessageQueue[i].message);
			int x = (ScreenWidth / 2) - (size / 2);
			gHUD.DrawHudString(x, m_MessageQueue[i].y_pos, ScreenWidth, (char *)m_MessageQueue[i].message, r, g, b );

			UnpackRGB(r,g,b, HudColor());
			ScaleColors(r, g, b, a);
			SPR_Set(m_hMouseClick, r, g, b);
			SPR_DrawAdditive(0, x - 48, y - 12, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("mouse")));

			//gEngfuncs.Con_DPrintf("drawing msg=%s, index=%d, x=%d, y=%d\n", m_MessageQueue[i].message, i, x, m_MessageQueue[i].y_pos);
		}
		else
			m_MessageQueue[i].time = 0;
	}

	return 1;
}
