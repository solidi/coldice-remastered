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

// Edge Indicator Configuration
#define EDGE_INDICATOR_MIN_DISTANCE 		64.0f		// Distance at which arrow disappears completely
#define EDGE_INDICATOR_FLIP_DISTANCE 		512.0f		// Distance at which arrow flips to point toward center
#define EDGE_INDICATOR_Z_CHECK_DISTANCE 	512.0f		// Distance threshold for Z-axis vertical edge forcing
#define EDGE_INDICATOR_Z_THRESHOLD 			96.0f		// Z-axis difference to force vertical edge (above/below)
#define EDGE_INDICATOR_Z_CLOSE_DISTANCE 	128.0f		// Z-axis distance to enable pull-in behavior
#define EDGE_INDICATOR_SCREEN_MARGIN 		20			// Pixel margin from screen edge
#define EDGE_INDICATOR_SIZE_WIDTH 			XRES(20)	// Arrow width in pixels
#define EDGE_INDICATOR_SIZE_HEIGHT 			YRES(20)	// Arrow height in pixels
#define EDGE_INDICATOR_PULSE_FREQUENCY 		3.5f		// Pulse animation frequency (Hz)
#define EDGE_INDICATOR_PULSE_AMPLITUDE 		0.3f		// Pulse amplitude modifier
#define EDGE_INDICATOR_PULSE_OFFSET 		0.7f		// Pulse baseline offset
#define EDGE_INDICATOR_ALPHA_MIN 			100			// Minimum alpha value
#define EDGE_INDICATOR_ALPHA_MAX 			255			// Maximum alpha value
#define EDGE_INDICATOR_ALPHA_RANGE 			155			// Alpha range for distance calculation
#define EDGE_INDICATOR_STALE_TIMEOUT 		60.0f		// Server data stale timeout (seconds)

extern cvar_t *cl_radar;

#ifdef _DEBUG
cvar_t *debug_radar = NULL;
#endif

DECLARE_MESSAGE( m_Radar, SpecEnt )

int CHudRadar::Init(void)
{
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem(this);
	
	HOOK_MESSAGE( SpecEnt );
	
	memset(m_SpecialEntities, 0, sizeof(m_SpecialEntities));

#ifdef _DEBUG
	if( !debug_radar )
		debug_radar = CVAR_CREATE( "Radar.debug", "0", 0 );
#endif

	return 1;
}

int CHudRadar::VidInit(void)
{
	memset(m_SpecialEntities, 0, sizeof(m_SpecialEntities));
	return 1;
}

int CHudRadar::MsgFunc_SpecEnt(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	
	int index = READ_BYTE(); // Which slot (0-MAX_SPECIAL_ENTITIES)
	if (index < 0 || index >= MAX_SPECIAL_ENTITIES)
		return 0;
	
	int active = READ_BYTE(); // 1 = active, 0 = inactive
	
	if (active)
	{
		m_SpecialEntities[index].origin.x = READ_COORD();
		m_SpecialEntities[index].origin.y = READ_COORD();
		m_SpecialEntities[index].origin.z = READ_COORD();
		m_SpecialEntities[index].special_type = READ_BYTE();
		m_SpecialEntities[index].last_update = gEngfuncs.GetClientTime();
		m_SpecialEntities[index].active = true;
	}
	else
	{
		m_SpecialEntities[index].active = false;
	}
	
	return 1;
}

void CHudRadar::DrawEdgeIndicator(int centerX, int centerY, float angle, float distance, int special, float zDiff)
{
	float radians = angle * PI_180;
	
	// Calculate direction vector
	float dirX = sin(radians);
	float dirY = -cos(radians); // Negative because screen Y is inverted
	
	// Override edge calculation if target is significantly above or below
	// BUT only if within reasonable proximity (less than 256 units away)
	// Threshold: 96 units in Z-axis
	bool forceVerticalEdge = false;
	bool forceTop = false;
	bool forceBottom = false;
	
	if (distance < EDGE_INDICATOR_Z_CHECK_DISTANCE)
	{
		if (zDiff >= EDGE_INDICATOR_Z_THRESHOLD)
		{
			// Target is significantly above - force top edge
			forceVerticalEdge = true;
			forceTop = true;
		}
		else if (zDiff <= -EDGE_INDICATOR_Z_THRESHOLD)
		{
			// Target is significantly below - force bottom edge
			forceVerticalEdge = true;
			forceBottom = true;
		}
	}
	
	// Calculate edge intersection
	float edgeX, edgeY;
	float absX = fabs(dirX);
	float absY = fabs(dirY);
	
	// Margin from screen edge
	int margin = EDGE_INDICATOR_SCREEN_MARGIN;
	
	if (forceVerticalEdge)
	{
		// Force top or bottom edge based on Z difference
		if (forceTop)
		{
			edgeY = margin;
			edgeX = centerX + (edgeY - centerY) * (dirX / dirY);
		}
		else // forceBottom
		{
			edgeY = ScreenHeight - margin;
			edgeX = centerX + (edgeY - centerY) * (dirX / dirY);
		}
		// Clamp X to screen bounds
		edgeX = fmax((float)margin, fmin((float)(ScreenWidth - margin), edgeX));
	}
	else if (absX > absY)
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
	int alpha = (int)(EDGE_INDICATOR_ALPHA_MAX - (distance / (MAX_DISTANCE * 2)) * EDGE_INDICATOR_ALPHA_RANGE);
	alpha = fmax(EDGE_INDICATOR_ALPHA_MIN, fmin(EDGE_INDICATOR_ALPHA_MAX, alpha));
	
	// Pulse effect for visibility
	float pulse = sin(gEngfuncs.GetClientTime() * EDGE_INDICATOR_PULSE_FREQUENCY) * EDGE_INDICATOR_PULSE_AMPLITUDE + EDGE_INDICATOR_PULSE_OFFSET;
	alpha = (int)(alpha * pulse);
	
	// Color based on special type
	int r, g, b;
	if (special == RADAR_TEAM_RED ||
		special == RADAR_BUSTER || 
		special == RADAR_VIRUS ||
		special == RADAR_HORDE ||
		special == RADAR_FLAG_RED ||
		special == RADAR_BASE_RED ||
		special == RADAR_ARENA_RED)
	{
		// Red for special targets (chumtoad, etc.)
		r = 240; g = 0; b = 0;
	}
	else if (special == RADAR_TEAM_BLUE || special == RADAR_JESUS || 
			 special == RADAR_FLAG_BLUE || special == RADAR_BASE_BLUE ||
			 special == RADAR_ARENA_BLUE)
	{
		// Blue
		r = 0; g = 0; b = 240;
	}
	else if (special == RADAR_COLD_SPOT || special == RADAR_CHUMTOAD)
	{
		// Green
		r = 0; g = 240; b = 0;
	}
	else
	{
		UnpackRGB(r, g, b, RGB_YELLOWISH);
	}
	
	// Draw large triangle pointing toward target
	// Don't draw if too close (within min distance)
	if (distance < EDGE_INDICATOR_MIN_DISTANCE)
		return;
	
	// Flip direction when player is very close (within flip distance)
	// But only if also close in Z-axis (within close distance vertically)
	bool closeEnough = (distance < EDGE_INDICATOR_FLIP_DISTANCE) && (fabs(zDiff) < EDGE_INDICATOR_Z_CLOSE_DISTANCE);
	bool flip = closeEnough;
	
	// Move arrow toward center when close (between min and flip distance)
	// AND close vertically
	float interpolation = 0.0f;
	if (closeEnough)
	{
		// Interpolate from edge (at flip distance) to center (at min distance)
		interpolation = (EDGE_INDICATOR_FLIP_DISTANCE - distance) / (EDGE_INDICATOR_FLIP_DISTANCE - EDGE_INDICATOR_MIN_DISTANCE);
		interpolation = fmax(0.0f, fmin(1.0f, interpolation)); // Clamp 0-1
		
		// Fade out as it moves toward center (full alpha at flip, zero at min)
		float fadeMultiplier = 1.0f - interpolation;
		alpha = (int)(alpha * fadeMultiplier);
	}
	
	// Interpolate position from edge to center
	float finalX = edgeX + (centerX - edgeX) * interpolation;
	float finalY = edgeY + (centerY - edgeY) * interpolation;
	
	int triSize = EDGE_INDICATOR_SIZE_WIDTH;
	int x = (int)finalX;
	int y = (int)finalY;
	
	// Determine which edge we're on based on ORIGINAL edge position (not interpolated)
	int edgeXInt = (int)edgeX;
	int edgeYInt = (int)edgeY;
	
	// Calculate which edge we're on to orient triangle correctly
	if (edgeXInt < ScreenWidth / 4)
	{
		// Left edge - triangle points right (or left when flipped)
		if (flip)
		{
			// Point left (toward center)
			for (int i = 0; i < triSize; i++)
			{
				int height = (i * EDGE_INDICATOR_SIZE_HEIGHT) / triSize;
				FillRGBA(x - i, y - height / 2, 2, height, r, g, b, alpha);
			}
		}
		else
		{
			// Point right (toward object)
			for (int i = 0; i < triSize; i++)
			{
				int height = (i * EDGE_INDICATOR_SIZE_HEIGHT) / triSize;
				FillRGBA(x + i, y - height / 2, 2, height, r, g, b, alpha);
			}
		}
		
		// Draw distance text - position based on arrow direction (only if > 12 ft)
		char distText[16];
		int distanceFeet = (int)(distance * 0.01904f * 3.28084f);
		if (distanceFeet > 12)
		{
			sprintf(distText, "%d ft", distanceFeet);
			int textHeight = gHUD.m_scrinfo.iCharHeight;
			int textX = flip ? (x + 2) : (x + triSize + 2);  // When flipped (pointing left), text closer to arrow base
			gHUD.DrawHudString(textX, y - textHeight / 2, ScreenWidth, distText, r, g, b);
		}
	}
	else if (edgeXInt > ScreenWidth * 3 / 4)
	{
		// Right edge - triangle points left (or right when flipped)
		if (flip)
		{
			// Point right (toward center)
			for (int i = 0; i < triSize; i++)
			{
				int height = (i * EDGE_INDICATOR_SIZE_HEIGHT) / triSize;
				FillRGBA(x + i, y - height / 2, 2, height, r, g, b, alpha);
			}
		}
		else
		{
			// Point left (toward object)
			for (int i = 0; i < triSize; i++)
			{
				int height = (i * EDGE_INDICATOR_SIZE_HEIGHT) / triSize;
				FillRGBA(x - i, y - height / 2, 2, height, r, g, b, alpha);
			}
		}
		
		// Draw distance text - position based on arrow direction (only if > 12 ft)
		char distText[16];
		int distanceFeet = (int)(distance * 0.01904f * 3.28084f);
		if (distanceFeet > 12)
		{
			sprintf(distText, "%d ft", distanceFeet);
			int textWidth = gHUD.m_scrinfo.charWidths['0'] * strlen(distText);
			int textHeight = gHUD.m_scrinfo.iCharHeight;
			int textX = flip ? (x - textWidth - 2) : (x - triSize - textWidth - 2);  // When flipped (pointing right), text closer to arrow base
			gHUD.DrawHudString(textX, y - textHeight / 2, ScreenWidth, distText, r, g, b);
		}
	}
	else if (edgeYInt < ScreenHeight / 4)
	{
		// Top edge - triangle points down (or up when flipped)
		if (flip)
		{
			// Point up (toward center)
			for (int i = 0; i < triSize; i++)
			{
				int width = (i * EDGE_INDICATOR_SIZE_HEIGHT) / triSize;
				FillRGBA(x - width / 2, y - i, width, 2, r, g, b, alpha);
			}
		}
		else
		{
			// Point down (toward object)
			for (int i = 0; i < triSize; i++)
			{
				int width = (i * EDGE_INDICATOR_SIZE_HEIGHT) / triSize;
				FillRGBA(x - width / 2, y + i, width, 2, r, g, b, alpha);
			}
		}
		
		// Draw distance text - position based on arrow direction (only if > 12 ft)
		char distText[16];
		int distanceFeet = (int)(distance * 0.01904f * 3.28084f);
		if (distanceFeet > 12)
		{
			sprintf(distText, "%d ft", distanceFeet);
			int textWidth = gHUD.m_scrinfo.charWidths['0'] * strlen(distText);
			int textY = flip ? (y + 2) : (y + triSize + 2);  // When flipped (pointing up), text closer to arrow base
			gHUD.DrawHudString(x - textWidth / 2, textY, ScreenWidth, distText, r, g, b);
		}
	}
	else
	{
		// Bottom edge - triangle points up (or down when flipped)
		if (flip)
		{
			// Point down (toward center)
			for (int i = 0; i < triSize; i++)
			{
				int width = (i * EDGE_INDICATOR_SIZE_HEIGHT) / triSize;
				FillRGBA(x - width / 2, y + i, width, 2, r, g, b, alpha);
			}
		}
		else
		{
			// Point up (toward object)
			for (int i = 0; i < triSize; i++)
			{
				int width = (i * EDGE_INDICATOR_SIZE_HEIGHT) / triSize;
				FillRGBA(x - width / 2, y - i, width, 2, r, g, b, alpha);
			}
		}
		
		// Draw distance text - position based on arrow direction (only if > 12 ft)
		char distText[16];
		int distanceFeet = (int)(distance * 0.01904f * 3.28084f);
		if (distanceFeet > 12)
		{
			sprintf(distText, "%d ft", distanceFeet);
			int textWidth = gHUD.m_scrinfo.charWidths['0'] * strlen(distText);
			int textY = flip ? (y + triSize + 2) : (y + 2);  // When not flipped (pointing up), text closer to arrow base
			gHUD.DrawHudString(x - textWidth / 2, textY, ScreenWidth, distText, r, g, b);
		}
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

		if (gHUD.m_GameMode == GAME_PROPHUNT)
		{
			// No props
			if (pClient->curstate.fuser4 > 0)
				continue;
		}
		else if (gHUD.m_GameMode == GAME_CTF)
		{
			// No flags
			if (pClient->curstate.fuser4 == RADAR_FLAG_BLUE ||
				pClient->curstate.fuser4 == RADAR_FLAG_RED)
				continue;
		}

		b_specials[num_players] = pClient->curstate.fuser4;

		// No world entities without fvalue
		if (!b_specials[num_players])
			if (!pClient->player)
				continue;

		// No dead players
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

		if (gHUD.m_GameMode == GAME_ICEMAN && m_RadarInfo[index].special == 0)
			size *= 2;

		if (m_RadarInfo[index].special == RADAR_TEAM_RED ||
			m_RadarInfo[index].special == RADAR_VIRUS ||
			m_RadarInfo[index].special == RADAR_BUSTER ||
			m_RadarInfo[index].special == RADAR_HORDE ||
			m_RadarInfo[index].special == RADAR_FLAG_RED ||
			m_RadarInfo[index].special == RADAR_BASE_RED ||
			m_RadarInfo[index].special == RADAR_ARENA_RED)
		{
			if (gHUD.m_GameMode != GAME_ICEMAN &&
				m_RadarInfo[index].special != RADAR_TEAM_RED)
				size *= 2;
			fr = 240; fg = 0; fb = 0;
		}

		if (m_RadarInfo[index].special == RADAR_TEAM_BLUE ||
			m_RadarInfo[index].special == RADAR_JESUS ||
			m_RadarInfo[index].special == RADAR_FLAG_BLUE ||
			m_RadarInfo[index].special == RADAR_BASE_BLUE ||
			m_RadarInfo[index].special == RADAR_ARENA_BLUE)
		{
			if (m_RadarInfo[index].special != RADAR_TEAM_BLUE)
				size *= 2;
			fr = 0; fg = 0; fb = 240;
		}

		if (m_RadarInfo[index].special == RADAR_COLD_SPOT ||
			m_RadarInfo[index].special == RADAR_CHUMTOAD)
		{
			size *= 2;
			fr = 0; fg = 240; fb = 0;
		}

		// Highlight yourself in grey/white
		if ((gEngfuncs.GetLocalPlayer()->index - 1) == index)
		{
			fr = 200; fg = 200; fb = 200;
		}

		FillRGBA(pos_x, pos_y, size, size, fr, fg, fb, alpha);
	}

	// Draw edge indicators for special tracked entities
	int screenCenterX = ScreenWidth / 2;
	int screenCenterY = ScreenHeight / 2;
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	
	// First, draw edge indicators from PVS-based radar (for players)
	for (index = 0; index < num_players; index++)
	{
		// Show edge indicators for all special entities, always
		if (!m_RadarInfo[index].special)
			continue;
		
		// Don't show edge indicator for yourself
		if ((localPlayer->index - 1) == index)
			continue;
		
		// Don't draw team players
		if (m_RadarInfo[index].special < RADAR_COLD_SPOT)
			continue;
		
		// Calculate Z difference (height is normalized by 72, so multiply back)
		float zDiff = m_RadarInfo[index].height * 72.0f;
		
		DrawEdgeIndicator(screenCenterX, screenCenterY, 
			m_RadarInfo[index].angle, 
			m_RadarInfo[index].distance,
			m_RadarInfo[index].special,
			zDiff);
	}
	
	// Second, draw edge indicators from server-sent special entities (always visible)
	for (index = 0; index < MAX_SPECIAL_ENTITIES; index++)
	{
		if (!m_SpecialEntities[index].active)
			continue;
		
		// Check if data is stale (older than timeout)
		// if (gEngfuncs.GetClientTime() - m_SpecialEntities[index].last_update > EDGE_INDICATOR_STALE_TIMEOUT)
		//	continue;
		
		// Calculate angle and distance from local player to special entity
		Vector vToEntity = m_SpecialEntities[index].origin - localPlayer->origin;
		float distance = vToEntity.Length();
		float zDiff = m_SpecialEntities[index].origin.z - localPlayer->origin.z;
		
		Vector vAngles;
		VectorAngles(vToEntity, vAngles);
		
		// Convert to 0-360 degrees
		if (vAngles.y < 0)
			vAngles.y += 360;
		
		Vector vViewAngles;
		gEngfuncs.GetViewAngles((float*)vViewAngles);
		float view_angle = vViewAngles.y;
		
		// Convert to 0-360 degrees
		if (view_angle < 0)
			view_angle += 360;
		
		float angle = view_angle - vAngles.y;
		
		DrawEdgeIndicator(screenCenterX, screenCenterY, 
			angle, 
			distance,
			m_SpecialEntities[index].special_type,
			zDiff);
	}

	return 1;
}
