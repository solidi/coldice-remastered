#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "con_nprint.h"
#include <string.h>
#include <stdio.h>
#include "com_model.h"
#include "vgui_TeamFortressViewport.h"

#define PI_180 (3.14159265358979 / 180.0)
#define MAX_DISTANCE 1000

extern cvar_t *cl_radar;

#ifdef _DEBUG
cvar_t *debug_radar = NULL;
#endif

int CHudRadar::Init(void)
{
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem(this);

#ifdef _DEBUG
	if( !debug_radar )
		debug_radar = CVAR_CREATE( "Radar.debug", "0", 0 );
#endif

	return 1;
}

int CHudRadar::VidInit(void)
{
	return 1;
}

void CHudRadar::ProcessPlayerState(void)
{
	Vector v_player[MAX_RADAR_DOTS], v_other, v_radar;
	int b_specials[MAX_RADAR_DOTS] = {false};
	float distanceLocal, player_distance[MAX_RADAR_DOTS], player_height[MAX_RADAR_DOTS], view_angle;
	int num_players = 0;
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();

	con_nprint_t info =
	{
		1,
		0.5f,
		{1.0f, 0.6f, 0.0f }
	};

	for (int i = 1; i < MAX_EDICTS; i++)
	{
		cl_entity_s *pClient = gEngfuncs.GetEntityByIndex(i);

		// Max radar dots
		if (num_players >= MAX_RADAR_DOTS)
			break;
		
		if (!pClient)
			continue;

		if (!pClient->model)
			continue;

		// Don't show an icon for dead or spectating players (ie: invisible entities).
		if (pClient->curstate.effects & EF_NODRAW)
			continue;

		if (gHUD.m_GameMode == GAME_BUSTERS ||
			gHUD.m_GameMode == GAME_CHILLDEMIC ||
			gHUD.m_GameMode == GAME_ICEMAN ||
			gHUD.m_GameMode == GAME_SHIDDEN)
		{
			if (pClient->curstate.fuser4 > 0)
				b_specials[num_players] = true;
			else
				b_specials[num_players] = false;
		}
		else if (gHUD.m_GameMode == GAME_PROPHUNT)
		{
			if (pClient->curstate.fuser4 > 0)
				continue;
			else
				b_specials[num_players] = false;
		}
		else if (gHUD.m_GameMode == GAME_CTC)
		{
			if (!strcmp(pClient->model->name, "models/w_chumtoad.mdl")) // loose toad
				b_specials[num_players] = true;
			else if (pClient->curstate.fuser4 > 0) // running with toad
				b_specials[num_players] = true;
			else
				b_specials[num_players] = false;
		}
		else if (gHUD.m_GameMode == GAME_CTF)
		{
			if (pClient->curstate.fuser4 > 1)
				b_specials[num_players] = pClient->curstate.fuser4;
			else
				b_specials[num_players] = false;
		}
		else if (gHUD.m_GameMode == GAME_COLDSPOT)
		{
			if (pClient->curstate.fuser4 > 1)
				b_specials[num_players] = pClient->curstate.fuser4;
			else
				b_specials[num_players] = false;
		}
		else if (gHUD.m_GameMode == GAME_COLDSKULL)
		{
			if (!strcmp(pClient->model->name, "models/w_runes.mdl") && pClient->curstate.fuser4 > 0)
				b_specials[num_players] = true;
			else
				b_specials[num_players] = false;
		}
		else if (gHUD.m_GameMode == GAME_HORDE)
		{
			if (pClient->curstate.fuser4 > 1) // monster
				b_specials[num_players] = true;
			else
				b_specials[num_players] = false;
		}

		if (!b_specials[num_players])
			if (!pClient->player)
				continue;

		if (pClient->player)
			if (pClient->curstate.health <= 0)
				continue;

		// Bot is not sending messages, due to disconnect or spectator
		if (pClient->curstate.messagenum < localPlayer->curstate.messagenum)
			continue;

		v_other = pClient->origin - localPlayer->origin;
		distanceLocal = v_other.Length();

		v_player[num_players] = v_other;
		player_distance[num_players] = distanceLocal <= MAX_DISTANCE ? distanceLocal : MAX_DISTANCE;
		// Player is 72 units high, adjust height diff to linear scale
		player_height[num_players] = ((pClient->origin.z - localPlayer->origin.z) / 72);
		num_players++;
	}

#ifdef _DEBUG
	if (debug_radar->value)
	{
		gEngfuncs.Con_NXPrintf(&info, "num_players = %d\n", num_players);
	}
#endif

	for (int index = 0; index < num_players; index++)
	{
#ifdef _DEBUG
		if (debug_radar->value)
		{
			info.index = 2;
			gEngfuncs.Con_NXPrintf(&info, "distance[%d] = %.2f\n", index, m_RadarInfo[index].distance);
		}
#endif

		VectorAngles(v_player[index], v_radar);
		// if yaw is negative, make 0 to 360 degrees
		if (v_radar.y < 0)
			v_radar.y += 360;
		Vector vAngles;
		gEngfuncs.GetViewAngles( (float*)vAngles );
		view_angle = vAngles.y;
		// make view angle 0 to 360 degrees if negative...
		if (view_angle < 0)
			view_angle += 360;

		view_angle = (view_angle - v_radar.y);

		RADAR radarInfo;
		radarInfo.distance = player_distance[index];
		radarInfo.height = player_height[index];
		radarInfo.angle = view_angle;
		radarInfo.special = b_specials[index];
		m_RadarInfo[index] = radarInfo;

#ifdef _DEBUG
		if (debug_radar->value)
		{
			info.index = 3;
			gEngfuncs.Con_NXPrintf(&info, "angle[%d] = %.2f\n", index, m_RadarInfo[index].angle);
		}
#endif
	}

	gHUD.m_PlayersInRadar = num_players;
}

int CHudRadar::Draw(float flTime)
{
	int r, g, b;
	int x, y;
	int radar_x, radar_y; // origin of radar sprite
	int pos_x, pos_y; // positions of the spots
	int index;
	float radians;
	float dist;
	int num_players = gHUD.m_PlayersInRadar;

	if (!cl_radar->value || g_iUser1 || gHUD.m_iShowingWeaponMenu)
		return 1;

	if (gHUD.m_Scoreboard.m_iShowscoresHeld)
		return 1;

	if (gHUD.m_Health.m_iHealth <= 0)
		return 1;

	if (gHUD.m_iIntermission)
		return 1;
	
	if (gEngfuncs.GetMaxClients() == 1)
		return 1;

	if (gViewPort->IsScoreBoardVisible())
		return 1;

	ProcessPlayerState();

	UnpackRGB(r, g, b, HudColor());

	x = 0;
	y = 16;
	radar_height = radar_width = 128;

	FillRGBA(x, y, radar_width, radar_height, r, g, b, 10);

	// calculate center of radar circle (it's actually off by one pixel
	radar_x = x + (radar_width / 2) - 1;
	radar_y = y + (radar_width / 2) - 1;

	for (index = 0; index < num_players; index++)
	{
		float angle = m_RadarInfo[index].angle;
		float distance = m_RadarInfo[index].distance;
		float height = m_RadarInfo[index].height;
		radians = angle * PI_180; // convert degrees to radians

		// calculate distance from center of circle (max = MAX_DISTANCE units)
		dist = (distance / MAX_DISTANCE) * (radar_width / 2);

		x = (int)(sin(radians) * dist);
		y = (int)(cos(radians) * dist);

		pos_x = radar_x + x;
		pos_y = radar_y - y;

		int size = 5, alpha = 160;
		if (height < 0) {
			size = fmax(size + (height), 2);
			alpha = fmax(160 + (height * 10), 100);
		} else {
			size = fmin(size + (height), 8);
			alpha = fmax(alpha + (height * 10), 200);
		}

		int fr = r, fg = g, fb = b;
		if ((gEngfuncs.GetLocalPlayer()->index - 1) == index)
		{
			fr = 200; fg = 200; fb = 200;
		}

		if (m_RadarInfo[index].special == 1 ||
			m_RadarInfo[index].special == 3)
		{
			size *= 2;
			fr = 240; fg = 0; fb = 0;
		}

		if (m_RadarInfo[index].special == 2)
		{
			size *= 2;
			fr = 0; fg = 0; fb = 240;
		}

		if (m_RadarInfo[index].special == 4)
		{
			size *= 2;
			fr = 0; fg = 240; fb = 0;
		}

		FillRGBA(pos_x, pos_y, size, size, fr, fg, fb, alpha);
	}

	return 1;
}
