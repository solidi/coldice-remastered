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

void CHudRadar::DrawEdgeIndicator(int centerX, int centerY, float angle, float distance, int special)
{
	float radians = angle * PI_180;
	
	// Calculate direction vector
	float dirX = sin(radians);
	float dirY = -cos(radians); // Negative because screen Y is inverted
	
	// Calculate edge intersection
	float edgeX, edgeY;
	float absX = fabs(dirX);
	float absY = fabs(dirY);
	
	// Margin from screen edge
	int margin = 40;
	
	if (absX > absY)
	{
		// Hit left or right edge
		if (dirX > 0)
			edgeX = ScreenWidth - margin;
		else
			edgeX = margin;
		edgeY = centerY + (edgeX - centerX) * (dirY / dirX);
		
		// Clamp Y to screen bounds
		edgeY = fmax((float)margin, fmin((float)(ScreenHeight - margin), edgeY));
	}
	else
	{
		// Hit top or bottom edge
		if (dirY > 0)
			edgeY = ScreenHeight - margin;
		else
			edgeY = margin;
		edgeX = centerX + (edgeY - centerY) * (dirX / dirY);
		
		// Clamp X to screen bounds
		edgeX = fmax((float)margin, fmin((float)(ScreenWidth - margin), edgeX));
	}
	
	// Calculate alpha based on distance (closer = more opaque)
	int alpha = (int)(255 - (distance / (MAX_DISTANCE * 2)) * 155);
	alpha = fmax(100, fmin(255, alpha));
	
	// Pulse effect for visibility
	float pulse = sin(gEngfuncs.GetClientTime() * 3.5f) * 0.3f + 0.7f;
	alpha = (int)(alpha * pulse);
	
	// Color based on special type
	int r, g, b;
	if (special == 1 || special == 3)
	{
		// Red for special targets (chumtoad, etc.)
		r = 240; g = 0; b = 0;
	}
	else if (special == 2)
	{
		// Blue
		r = 0; g = 0; b = 240;
	}
	else if (special == 4)
	{
		// Green
		r = 0; g = 240; b = 0;
	}
	else
	{
		UnpackRGB(r, g, b, RGB_YELLOWISH);
	}
	
	// Draw arrow pointing toward target
	int arrowSize = 24;
	int arrowThickness = 6;
	int x = (int)edgeX;
	int y = (int)edgeY;
	
	// Simple directional indicator
	// Vertical line (stem)
	FillRGBA(x - 1, y - arrowSize, arrowThickness, arrowSize * 2, r, g, b, alpha);
	
	// Horizontal line (perpendicular)
	FillRGBA(x - arrowSize, y - 1, arrowSize * 2, arrowThickness, r, g, b, alpha);
	
	// Draw arrowhead pointing inward (toward target)
	// Calculate which edge we're on to orient arrow correctly
	if (x < ScreenWidth / 4)
	{
		// Left edge - arrow points right
		for (int i = 0; i < 8; i++)
			FillRGBA(x + 6 + i, y - i, 2, i * 2 + 1, r, g, b, alpha);
	}
	else if (x > ScreenWidth * 3 / 4)
	{
		// Right edge - arrow points left
		for (int i = 0; i < 8; i++)
			FillRGBA(x - 6 - i, y - i, 2, i * 2 + 1, r, g, b, alpha);
	}
	else if (y < ScreenHeight / 4)
	{
		// Top edge - arrow points down
		for (int i = 0; i < 8; i++)
			FillRGBA(x - i, y + 6 + i, i * 2 + 1, 2, r, g, b, alpha);
	}
	else
	{
		// Bottom edge - arrow points up
		for (int i = 0; i < 8; i++)
			FillRGBA(x - i, y - 6 - i, i * 2 + 1, 2, r, g, b, alpha);
	}
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
			if (pClient->curstate.fuser4 > 0) // running with toad
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
			else if (pClient->curstate.fuser4 == 1)
				b_specials[num_players] = true;
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
		else if (gHUD.m_GameMode == GAME_LMS)
		{
			if (pClient->curstate.fuser4 == 1)
				b_specials[num_players] = 1;
			else if (pClient->curstate.fuser4 == 99) //safe spot
				b_specials[num_players] = 4;
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

		// Default dots set
		UnpackRGB(r, g, b, RGB_BLUEISH);
		int fr = r, fg = g, fb = b;
		if ((gEngfuncs.GetLocalPlayer()->index - 1) == index)
		{
			fr = 200; fg = 200; fb = 200;
		}

		if (gHUD.m_GameMode == GAME_ICEMAN && m_RadarInfo[index].special == 0)
			size *= 2;

		if (m_RadarInfo[index].special == 1 ||
			m_RadarInfo[index].special == 3)
		{
			if (gHUD.m_GameMode != GAME_ICEMAN)
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

	// Draw edge indicators for special tracked entities
	int screenCenterX = ScreenWidth / 2;
	int screenCenterY = ScreenHeight / 2;
	
	for (index = 0; index < num_players; index++)
	{
		// Show edge indicators for all special entities, always
		if (!m_RadarInfo[index].special)
			continue;
		
		// Don't show edge indicator for yourself
		if ((gEngfuncs.GetLocalPlayer()->index - 1) == index)
			continue;
		
		DrawEdgeIndicator(screenCenterX, screenCenterY, 
			m_RadarInfo[index].angle, 
			m_RadarInfo[index].distance,
			m_RadarInfo[index].special);
	}

	return 1;
}
