#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "con_nprint.h"
#include <string.h>
#include <stdio.h>
#include "com_model.h"
#include "triangleapi.h"
#include "vgui_TeamFortressViewport.h"

#define PI_180 (3.14159265358979 / 180.0)
#define MAX_DISTANCE 1000

// Behind Indicator Configuration
#define BEHIND_INDICATOR_MAX_DISTANCE 		448.0f		// Max distance (~28 ft) for behind warning
#define BEHIND_INDICATOR_SCREEN_MARGIN 		20			// Pixel margin from screen edge
#define BEHIND_INDICATOR_SIZE_WIDTH 			XRES(20)	// Arrow width in pixels
#define BEHIND_INDICATOR_SIZE_HEIGHT 		YRES(20)	// Arrow height in pixels
#define BEHIND_INDICATOR_PULSE_FREQUENCY 	3.5f		// Pulse animation frequency (Hz)
#define BEHIND_INDICATOR_PULSE_AMPLITUDE 	0.3f		// Pulse amplitude modifier
#define BEHIND_INDICATOR_PULSE_OFFSET 		0.7f		// Pulse baseline offset
#define BEHIND_INDICATOR_ALPHA_MIN 			100			// Minimum alpha value
#define BEHIND_INDICATOR_ALPHA_MAX 			255			// Maximum alpha value

// Compass Bar Configuration
#define COMPASS_FOV 					120.0f		// Visible angular range in degrees
#define COMPASS_HALF_FOV 				(COMPASS_FOV / 2.0f)
#define COMPASS_BAR_WIDTH_FRAC 			0.5f		// Fraction of ScreenWidth
#define COMPASS_Y_OFFSET 				YRES(36)		// Top margin in pixels
#define COMPASS_LINE_THICKNESS 			1			// Horizontal line height
#define COMPASS_TICK_HEIGHT 			YRES(4)		// Small tick mark height
#define COMPASS_CARDINAL_TICK_HEIGHT 	YRES(6)		// Tall tick for N/E/S/W
#define COMPASS_TICK_SPACING 			15.0f		// Degrees between small ticks
#define COMPASS_TARGET_TRI_WIDTH 		XRES(6)		// Target triangle width
#define COMPASS_TARGET_TRI_HEIGHT 		YRES(6)		// Target triangle height
#define COMPASS_LINE_ALPHA 				140			// Compass line/tick alpha
#define COMPASS_CARDINAL_ALPHA 			180			// Cardinal letter alpha

extern cvar_t *cl_radar;
extern cvar_t *cl_compass;

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

static void GetRadarColor(int special, int &r, int &g, int &b)
{
	if (special == RADAR_TEAM_RED ||
		special == RADAR_BUSTER ||
		special == RADAR_VIRUS ||
		special == RADAR_HORDE ||
		special == RADAR_FLAG_RED ||
		special == RADAR_BASE_RED ||
		special == RADAR_ARENA_RED)
	{
		r = 240; g = 0; b = 0;
	}
	else if (special == RADAR_TEAM_BLUE)
	{
		r = 0; g = 160; b = 240;
	}
	else if (special == RADAR_JESUS ||
			 special == RADAR_FLAG_BLUE || special == RADAR_BASE_BLUE ||
			 special == RADAR_ARENA_BLUE)
	{
		r = 0; g = 0; b = 240;
	}
	else if (special == RADAR_COLD_SPOT || special == RADAR_CHUMTOAD)
	{
		r = 0; g = 240; b = 0;
	}
	else if (special == RADAR_LOOT)
	{
		r = 255; g = 117; b = 24;
	}
	else if (special == RADAR_SNOWBALL)
	{
		r = 255; g = 255; b = 255;
	}
	else if (special == RADAR_TEAM_YELLOW)
	{
		r = 240; g = 240; b = 0;
	}
	else if (special == RADAR_TEAM_GREEN)
	{
		r = 0; g = 240; b = 0;
	}
	else
	{
		UnpackRGB(r, g, b, HudColor());
	}
}

void CHudRadar::DrawInWorldIndicator(Vector worldOrigin, float distance, int special)
{
	// Raise the indicator these units above the target's origin
	Vector elevatedOrigin = worldOrigin;
	elevatedOrigin.z += 24.0f;

	// Single player games, no indicators
	if (gHUD.m_GameMode == GAME_FFA ||
		gHUD.m_GameMode == GAME_GUNGAME ||
		gHUD.m_GameMode == GAME_SNOWBALL ||
		gHUD.m_GameMode == GAME_INSTAGIB ||
		gHUD.m_GameMode == GAME_COLDSKULL)
		return;

	// Project world position to screen coordinates
	vec3_t screenPos;
	if (gEngfuncs.pTriAPI->WorldToScreen((float*)elevatedOrigin, (float*)screenPos))
		return; // Behind the viewer
	
	// WorldToScreen returns [-1, 1] range, convert to pixel coordinates
	int x = (int)((screenPos[0] + 1.0f) * 0.5f * ScreenWidth);
	int y = (int)((1.0f - screenPos[1]) * 0.5f * ScreenHeight);
	
	// Only draw if on screen
	int margin = 20;
	if (x < margin || x > ScreenWidth - margin || y < margin || y > ScreenHeight - margin)
		return;
	
	// Color based on special type
	int r, g, b;
	GetRadarColor(special, r, g, b);
	
	// Alpha based on distance
	int alpha = (int)(255 - (distance / (MAX_DISTANCE * 2)) * 155);
	alpha = fmax(100, fmin(255, alpha));
	
	// Pulse effect
	float pulse = sin(gEngfuncs.GetClientTime() * 3.5f) * 0.3f + 0.7f;
	alpha = (int)(alpha * pulse);
	
	// Draw downward-pointing triangle (tip at bottom)
	int triW = XRES(10);
	int triH = YRES(8);
	for (int i = 0; i < triH; i++)
	{
		int width = triW - (i * triW) / triH;
		FillRGBA(x - width / 2, y + i, width, 2, r, g, b, alpha);
	}
	
	// Draw distance text below the triangle tip (only if > 12 ft)
	int distanceFeet = (int)(distance * 0.01904f * 3.28084f);
	if (distanceFeet > 12)
	{
		char distText[16];
		sprintf(distText, "%d ft", distanceFeet);
		int textWidth = gHUD.m_scrinfo.charWidths['0'] * strlen(distText);
		gHUD.DrawHudString(x - textWidth / 2, y + triH + 2, ScreenWidth, distText, r, g, b);
	}
}

static float NormalizeAngle180(float angle)
{
	while (angle > 180.0f) angle -= 360.0f;
	while (angle < -180.0f) angle += 360.0f;
	return angle;
}

void CHudRadar::DrawCompass(void)
{
	int r, g, b;
	UnpackRGB(r, g, b, HudColor());

	// Get player yaw
	Vector vAngles;
	gEngfuncs.GetViewAngles((float*)vAngles);
	float playerYaw = vAngles.y;
	if (playerYaw < 0)
		playerYaw += 360.0f;

	// Compass bar geometry
	int barWidth = (int)(ScreenWidth * COMPASS_BAR_WIDTH_FRAC);
	int barX = (ScreenWidth - barWidth) / 2;
	int barY = COMPASS_Y_OFFSET;

	// Draw horizontal line
	FillRGBA(barX, barY, barWidth, COMPASS_LINE_THICKNESS, r, g, b, COMPASS_LINE_ALPHA);

	int barCenterX = barX + barWidth / 2;
	float pixelsPerDegree = (float)barWidth / COMPASS_FOV;

	// Draw tick marks and cardinal directions
	static const char *cardinals[] = { "N", "E", "S", "W" };
	static const float cardinalAngles[] = { 0.0f, 90.0f, 180.0f, 270.0f };

	for (float tickAngle = 0.0f; tickAngle < 360.0f; tickAngle += COMPASS_TICK_SPACING)
	{
		float diff = NormalizeAngle180(tickAngle - playerYaw);
		if (fabs(diff) > COMPASS_HALF_FOV)
			continue;

		int tickX = barCenterX + (int)(diff * pixelsPerDegree);

		// Check if this is a cardinal direction
		bool isCardinal = false;
		const char *cardinalLabel = NULL;
		for (int c = 0; c < 4; c++)
		{
			if (fabs(tickAngle - cardinalAngles[c]) < 0.1f)
			{
				isCardinal = true;
				cardinalLabel = cardinals[c];
				break;
			}
		}

		if (isCardinal)
		{
			int tickH = COMPASS_CARDINAL_TICK_HEIGHT;
			FillRGBA(tickX, barY - tickH, 1, tickH, r, g, b, COMPASS_CARDINAL_ALPHA);

			// Draw cardinal letter above the tick
			int charWidth = gHUD.m_scrinfo.charWidths[(unsigned char)cardinalLabel[0]];
			int textY = barY - tickH - gHUD.m_scrinfo.iCharHeight;
			char buf[2] = { cardinalLabel[0], '\0' };
			gHUD.DrawHudString(tickX - charWidth / 2, textY, ScreenWidth, buf, r, g, b);
		}
		else
		{
			int tickH = COMPASS_TICK_HEIGHT;
			FillRGBA(tickX, barY - tickH, 1, tickH, r, g, b, COMPASS_LINE_ALPHA);
		}
	}

	// Draw target markers from PVS radar data
	int num_players = gHUD.m_PlayersInRadar;
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();

	for (int index = 0; index < num_players; index++)
	{
		// Skip self
		if ((localPlayer->index - 1) == index)
			continue;

		int special = m_RadarInfo[index].special;

		// Only show special entities (same filter as edge indicators)
		//if (!special)
		//	continue;
		//if (special < RADAR_COLD_SPOT)
		//	continue;

		// Map the radar angle onto the compass
		// m_RadarInfo[].angle is already relative to view (view_angle - entity_angle)
		float angleDiff = NormalizeAngle180(m_RadarInfo[index].angle);
		if (fabs(angleDiff) > COMPASS_HALF_FOV)
			continue;

		int targetX = barCenterX + (int)(angleDiff * pixelsPerDegree);

		// Get target color
		int tr, tg, tb;
		GetRadarColor(special, tr, tg, tb);

		// Draw small downward triangle below the compass line
		int triW = COMPASS_TARGET_TRI_WIDTH;
		int triH = COMPASS_TARGET_TRI_HEIGHT;
		int triTop = barY + 2;
		for (int i = 0; i < triH; i++)
		{
			int width = triW - (i * triW) / triH;
			FillRGBA(targetX - width / 2, triTop + i, width, 1, tr, tg, tb, 200);
		}

		// Draw distance text below triangle (skip if <= 12 ft)
		float distance = m_RadarInfo[index].distance;
		int distanceFeet = (int)(distance * 0.01904f * 3.28084f);
		if (distanceFeet > 12)
		{
			char distText[16];
			sprintf(distText, "%d ft", distanceFeet);
			int textWidth = gHUD.m_scrinfo.charWidths['0'] * strlen(distText);
			gHUD.DrawHudString(targetX - textWidth / 2, triTop + triH + 1, ScreenWidth, distText, tr, tg, tb);
		}
	}
}

void CHudRadar::DrawBehindIndicator(float angle, float distance, int special)
{
	// Only show if target is outside the compass FOV (behind the player)
	float normalizedAngle = NormalizeAngle180(angle);
	if (fabs(normalizedAngle) <= COMPASS_HALF_FOV)
		return;

	// Only show if within close range
	if (distance > BEHIND_INDICATOR_MAX_DISTANCE)
		return;

	// Color based on special type
	int r, g, b;
	GetRadarColor(special, r, g, b);

	// Alpha: closer = more opaque, pulse for visibility
	float distFrac = distance / BEHIND_INDICATOR_MAX_DISTANCE;
	int alpha = (int)(BEHIND_INDICATOR_ALPHA_MAX - distFrac * (BEHIND_INDICATOR_ALPHA_MAX - BEHIND_INDICATOR_ALPHA_MIN));
	float pulse = sin(gEngfuncs.GetClientTime() * BEHIND_INDICATOR_PULSE_FREQUENCY) * BEHIND_INDICATOR_PULSE_AMPLITUDE + BEHIND_INDICATOR_PULSE_OFFSET;
	alpha = (int)(alpha * pulse);

	int margin = BEHIND_INDICATOR_SCREEN_MARGIN;
	int triW = BEHIND_INDICATOR_SIZE_WIDTH;
	int triH = BEHIND_INDICATOR_SIZE_HEIGHT;

	// Map behind angle to a U-shaped path along screen edges:
	//   Side edge (mid-height -> bottom corner) then bottom edge (corner -> center)
	float absAngle = fabs(normalizedAngle);
	float behindRange = 180.0f - COMPASS_HALF_FOV; // degrees of behind arc
	float behindFrac = (absAngle - COMPASS_HALF_FOV) / behindRange; // 0 at compass edge, 1 at directly behind
	bool isLeft = (normalizedAngle < 0); // negative angle = target to left of view

	int x, y;

	if (behindFrac <= 0.5f)
	{
		// Phase 1: Side edge — slide from mid-height down to bottom corner
		float t = behindFrac * 2.0f;
		y = (int)(ScreenHeight * 0.5f + t * (ScreenHeight - margin - ScreenHeight * 0.5f));

		// Draw arrow pointing toward the screen edge (tip at edge, base inward)
		if (isLeft)
		{
			for (int i = 0; i < triW; i++)
			{
				int height = (i * triH) / triW;
				FillRGBA(margin + i, y - height / 2, 2, height, r, g, b, alpha);
			}
		}
		else
		{
			for (int i = 0; i < triW; i++)
			{
				int height = (i * triH) / triW;
				FillRGBA(ScreenWidth - margin - i, y - height / 2, 2, height, r, g, b, alpha);
			}
		}

		// Distance text inward from arrow
		int distanceFeet = (int)(distance * 0.01904f * 3.28084f);
		if (distanceFeet > 0)
		{
			char distText[16];
			sprintf(distText, "%d ft", distanceFeet);
			int textWidth = gHUD.m_scrinfo.charWidths['0'] * strlen(distText);
			int textHeight = gHUD.m_scrinfo.iCharHeight;
			if (isLeft)
				gHUD.DrawHudString(margin + triW + 2, y - textHeight / 2, ScreenWidth, distText, r, g, b);
			else
				gHUD.DrawHudString(ScreenWidth - margin - triW - textWidth - 2, y - textHeight / 2, ScreenWidth, distText, r, g, b);
		}
	}
	else
	{
		// Phase 2: Bottom edge — slide from corner toward center
		float t = (behindFrac - 0.5f) * 2.0f;
		if (isLeft)
			x = (int)(margin + t * (ScreenWidth * 0.5f - margin));
		else
			x = (int)((ScreenWidth - margin) - t * (ScreenWidth * 0.5f - margin));

		// Draw downward-pointing arrow with tip at margin, base above
		int baseY = ScreenHeight - margin - triH;
		for (int i = 0; i < triH; i++)
		{
			int width = triW - (i * triW) / triH;
			FillRGBA(x - width / 2, baseY + i, width, 2, r, g, b, alpha);
		}

		// Distance text above arrow
		int distanceFeet = (int)(distance * 0.01904f * 3.28084f);
		if (distanceFeet > 0)
		{
			char distText[16];
			sprintf(distText, "%d ft", distanceFeet);
			int textWidth = gHUD.m_scrinfo.charWidths['0'] * strlen(distText);
			int textHeight = gHUD.m_scrinfo.iCharHeight;
			gHUD.DrawHudString(x - textWidth / 2, baseY - textHeight - 2, ScreenWidth, distText, r, g, b);
		}
	}
}

void CHudRadar::ProcessPlayerState(void)
{
	Vector v_player[MAX_RADAR_DOTS], v_other, v_radar, v_origin[MAX_RADAR_DOTS];
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
			if (localPlayer != pClient && pClient->curstate.fuser4 > 0)
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
		v_origin[num_players] = pClient->origin + Vector(0, 0, int(pClient->curstate.maxs.z) - int(pClient->curstate.maxs.z * 0.3)); // Raise origin for better visibility on radar
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
		radarInfo.origin = v_origin[index];
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

	if (MutatorEnabled(MUTATOR_NORADAR))
		return 1;

	ProcessPlayerState();

	if (cl_compass->value)
		DrawCompass();

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
		}

		if (m_RadarInfo[index].special == RADAR_TEAM_BLUE ||
			m_RadarInfo[index].special == RADAR_JESUS ||
			m_RadarInfo[index].special == RADAR_FLAG_BLUE ||
			m_RadarInfo[index].special == RADAR_BASE_BLUE ||
			m_RadarInfo[index].special == RADAR_ARENA_BLUE)
		{
			if (m_RadarInfo[index].special != RADAR_TEAM_BLUE)
				size *= 2;
		}

		if (m_RadarInfo[index].special == RADAR_COLD_SPOT ||
			m_RadarInfo[index].special == RADAR_CHUMTOAD ||
			m_RadarInfo[index].special == RADAR_TEAM_GREEN)
		{
			if (m_RadarInfo[index].special != RADAR_TEAM_GREEN)
				size *= 2;
		}

		if (m_RadarInfo[index].special == RADAR_LOOT)
		{
			size *= 2;
		}

		if (m_RadarInfo[index].special == RADAR_SNOWBALL)
		{
			size *= 2;
		}

		GetRadarColor(m_RadarInfo[index].special, fr, fg, fb);

		// Highlight yourself in grey/white
		if ((gEngfuncs.GetLocalPlayer()->index - 1) == index)
		{
			fr = 200; fg = 200; fb = 200;
		}

		FillRGBA(pos_x, pos_y, size, size, fr, fg, fb, alpha);
	}

	// Draw indicators for special tracked entities
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	
	// First, draw indicators from PVS-based radar (for players)
	for (index = 0; index < num_players; index++)
	{
		// Don't show indicator for yourself
		if ((localPlayer->index - 1) == index)
			continue;
		
		// These use world-space projection (shows only when facing target) via server-sent entities
		if (m_RadarInfo[index].special == RADAR_COLD_SPOT ||
			m_RadarInfo[index].special == RADAR_BASE_RED ||
			m_RadarInfo[index].special == RADAR_BASE_BLUE)
			continue; // Come from server-sent entities, skip here
		
		if (m_RadarInfo[index].special == RADAR_HORDE ||
			(localPlayer->curstate.fuser4 == m_RadarInfo[index].special))
		{
			DrawInWorldIndicator(m_RadarInfo[index].origin, m_RadarInfo[index].distance, m_RadarInfo[index].special);
			continue;
		}

		// Behind indicators only show when compass is enabled
		if (!cl_compass->value)
			continue;

		if (!m_RadarInfo[index].special)
			continue;

		// Don't draw team players
		if (m_RadarInfo[index].special < RADAR_COLD_SPOT)
			continue;

		DrawBehindIndicator(
			m_RadarInfo[index].angle, 
			m_RadarInfo[index].distance,
			m_RadarInfo[index].special);
	}
	
	// Second, draw indicators from server-sent special entities
	for (index = 0; index < MAX_SPECIAL_ENTITIES; index++)
	{
		if (!m_SpecialEntities[index].active)
			continue;
		
		Vector vToEntity = m_SpecialEntities[index].origin - localPlayer->origin;
		float distance = vToEntity.Length();

		// In-world indicators are always visible
		if (m_SpecialEntities[index].special_type == RADAR_COLD_SPOT ||
			m_SpecialEntities[index].special_type == RADAR_BASE_RED ||
			m_SpecialEntities[index].special_type == RADAR_BASE_BLUE)
		{
			DrawInWorldIndicator(m_SpecialEntities[index].origin, distance, m_SpecialEntities[index].special_type);
			continue;
		}

		// Behind indicators only show when compass is enabled
		if (!cl_compass->value)
			continue;
		
		Vector vAngles;
		VectorAngles(vToEntity, vAngles);
		if (vAngles.y < 0)
			vAngles.y += 360;
		
		Vector vViewAngles;
		gEngfuncs.GetViewAngles((float*)vViewAngles);
		float view_angle = vViewAngles.y;
		if (view_angle < 0)
			view_angle += 360;
		
		float angle = view_angle - vAngles.y;
		
		DrawBehindIndicator(
			angle, 
			distance,
			m_SpecialEntities[index].special_type);
	}

	return 1;
}
