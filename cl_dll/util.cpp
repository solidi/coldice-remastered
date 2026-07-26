/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// util.cpp
//
// implementation of class-less helper functions
//

#include "stdio.h"
#include "stdlib.h"
#include "math.h"

#include "hud.h"
#include "cl_util.h"
#include <string.h>

#include "vgui_TeamFortressViewport.h"

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

vec3_t vec3_origin( 0, 0, 0 );

extern cvar_t *cl_weaponmodel;
extern cvar_t *cl_sleevemodel;

double sqrt(double x);

float Length(const float *v)
{
	int		i;
	float	length;
	
	length = 0;
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = sqrt (length);		// FIXME

	return length;
}

void VectorAngles( const float *forward, float *angles )
{
	float	tmp, yaw, pitch;
	
	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = sqrt (forward[0]*forward[0] + forward[1]*forward[1]);
		pitch = (atan2(forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}
	
	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}

float VectorNormalize (float *v)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
		
	return length;

}

void VectorInverse ( float *v )
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void VectorScale (const float *in, float scale, float *out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}

void VectorMA (const float *veca, float scale, const float *vecb, float *vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}

HSPRITE LoadSprite(const char *pszName)
{
	int i;
	char sz[256]; 

	if (ScreenWidth < 640)
		i = 320;
	else
		i = 640;

	sprintf(sz, pszName, i);

	return SPR_Load(sz);
}

static int ClampModelIndex(int value, int minValue, int maxValue)
{
	if (value < minValue)
		return minValue;
	if (value > maxValue)
		return maxValue;
	return value;
}

static bool ShouldForceTeamSleeveModel()
{
	switch (gHUD.m_GameMode)
	{
	case GAME_ARENA:
	case GAME_LMS:
	case GAME_BUSTERS:
	case GAME_CHILLDEMIC:
	case GAME_COLDSPOT:
	case GAME_CTC:
	case GAME_CTF:
	case GAME_HORDE:
	case GAME_ICEMAN:
	case GAME_KTS:
	case GAME_LOOT:
	case GAME_PROPHUNT:
	case GAME_SHIDDEN:
	case GAME_TEAMPLAY:
		return true;
	default:
		return false;
	}
}

bool IsHalfLifeModelOverrideActive()
{
	return MutatorEnabled(MUTATOR_HALFLIFE);
}

int GetWeaponModelIndex()
{
	if (cl_weaponmodel)
		return ClampModelIndex((int)cl_weaponmodel->value, SKIN_NORMAL, SKIN_GOLD);

	return ClampModelIndex(gHUD.m_WeaponModelIndex, SKIN_NORMAL, SKIN_GOLD);
}

int GetSleeveModelIndex()
{
	if (cl_sleevemodel)
		return ClampModelIndex((int)cl_sleevemodel->value, SLEEVE_ORANGE, SLEEVE_GREEN);

	return ClampModelIndex(gHUD.m_SleeveModelIndex, SLEEVE_ORANGE, SLEEVE_GREEN);
}

static bool GetTeamForcedSleeveModelIndex(int &sleeveIndex)
{
	cl_entity_t *local = gEngfuncs.GetLocalPlayer();
	if (!local)
		return false;

	if (!ShouldForceTeamSleeveModel())
		return false;

	const int special = (int)local->curstate.fuser4;
	if (special == RADAR_TEAM_RED ||
		special == RADAR_ARENA_RED ||
		special == RADAR_BUSTER ||
		special == RADAR_VIRUS ||
		(special > 0 && gHUD.m_GameMode == GAME_PROPHUNT))
	{
		sleeveIndex = SLEEVE_RED;
		return true;
	}

	if (special == RADAR_CHUMTOAD || special == RADAR_TEAM_GREEN)
	{
		sleeveIndex = SLEEVE_GREEN;
		return true;
	}

	if (special == RADAR_TEAM_YELLOW)
	{
		sleeveIndex = SLEEVE_YELLOW;
		return true;
	}

	if (special == RADAR_TEAM_BLUE || special == RADAR_ARENA_BLUE)
	{
		sleeveIndex = SLEEVE_BLUE;
		return true;
	}

	return false;
}

int GetEffectiveSleeveModelIndex()
{
	if (IsHalfLifeModelOverrideActive())
		return SLEEVE_ORANGE;

	int forcedSleeveIndex = SLEEVE_ORANGE;
	if (GetTeamForcedSleeveModelIndex(forcedSleeveIndex))
		return forcedSleeveIndex;

	return GetSleeveModelIndex();
}

int GetEffectiveWeaponModelIndex(const char *modelName, bool forViewModel)
{
	if (IsHalfLifeModelOverrideActive())
		return SKIN_NORMAL;

	int skin = GetWeaponModelIndex();
	if (forViewModel)
	{
		if (MutatorEnabled(MUTATOR_GOLDENGUNS))
			skin = SKIN_GOLD;
		else if (modelName && gHUD.m_Teamplay == GAME_GUNGAME && !stricmp(modelName, "models/v_knife.mdl"))
			skin = SKIN_GOLD;
	}

	return ClampModelIndex(skin, SKIN_NORMAL, SKIN_GOLD);
}

int GetCombinedViewModelSkinIndex(const char *modelName)
{
	const int weaponIndex = GetEffectiveWeaponModelIndex(modelName, true);
	const int sleeveIndex = GetEffectiveSleeveModelIndex();
	return (weaponIndex * (SLEEVE_GREEN - SLEEVE_ORANGE + 1)) + sleeveIndex;
}

bool UseIceVisualStyle()
{
	const int weaponIndex = GetEffectiveWeaponModelIndex(NULL, false);
	const int GetSleeveIndex = GetEffectiveSleeveModelIndex();
	return (weaponIndex >= SKIN_INVERSE && weaponIndex <= SKIN_EDITION) || GetSleeveIndex > SLEEVE_ORANGE;
}

unsigned long HudColor()
{
	static unsigned long colorchange = RGB_BLUEISH;

	if (IsHalfLifeModelOverrideActive())
	{
		if (colorchange != RGB_YELLOWISH)
		{
			gEngfuncs.pfnClientCmd("con_color \"255 180 30\"\n");
			gEngfuncs.pfnClientCmd("tracerred \"1\"\ntracerblue \"0\"\ntracergreen \"0.8\"\n");
			colorchange = RGB_YELLOWISH;
		}

		return RGB_YELLOWISH;
	}

	// Special case for red team in game modes
	if (gHUD.m_GameMode)
	{
		cl_entity_t *local = gEngfuncs.GetLocalPlayer();
		if (local->curstate.fuser4 == RADAR_TEAM_RED || 
			local->curstate.fuser4 == RADAR_ARENA_RED ||
			local->curstate.fuser4 == RADAR_BUSTER ||
			local->curstate.fuser4 == RADAR_VIRUS ||
			(local->curstate.fuser4 > 0 && gHUD.m_GameMode == GAME_PROPHUNT))
		{
			if (colorchange != RGB_REDISH)
			{
				gEngfuncs.pfnClientCmd("con_color \"255 80 0\"\n");
				gEngfuncs.pfnClientCmd("tracerred \"1\"\ntracerblue \"0\"\ntracergreen \"0\"\n");
				colorchange = RGB_REDISH;
			}
			return RGB_REDISH;
		}
		else if (local->curstate.fuser4 == RADAR_CHUMTOAD ||
				 local->curstate.fuser4 == RADAR_TEAM_GREEN)
		{
			if (colorchange != RGB_GREENISH)
			{
				gEngfuncs.pfnClientCmd("con_color \"0 200 0\"\n");
				gEngfuncs.pfnClientCmd("tracerred \"0\"\ntracerblue \"0\"\ntracergreen \"1\"\n");
				colorchange = RGB_GREENISH;
			}
			return RGB_GREENISH;
		}
		else if (local->curstate.fuser4 == RADAR_TEAM_YELLOW)
		{
			if (colorchange != RGB_YELLOWISH)
			{
				gEngfuncs.pfnClientCmd("con_color \"255 180 30\"\n");
				gEngfuncs.pfnClientCmd("tracerred \"1\"\ntracerblue \"0\"\ntracergreen \"0.8\"\n");
				colorchange = RGB_YELLOWISH;
			}
			return RGB_YELLOWISH;
		}
		else if (local->curstate.fuser4 == RADAR_LOOT)
		{
			if (colorchange != RGB_ORANGEISH)
			{
				gEngfuncs.pfnClientCmd("con_color \"255 95 30\"\n");
				gEngfuncs.pfnClientCmd("tracerred \"1\"\ntracerblue \"0\"\ntracergreen \"0.65\"\n");
				colorchange = RGB_ORANGEISH;
			}
			return RGB_ORANGEISH;
		}
	}

	// General colors, if no specific gameplay team color is set
	if ((GetSleeveModelIndex() == SLEEVE_BLUE))
	{
		if (colorchange != RGB_BLUEISH)
		{
			gEngfuncs.pfnClientCmd("con_color \"0 160 255\"\n");
			gEngfuncs.pfnClientCmd("tracerred \"0\"\ntracerblue \"1\"\ntracergreen \"0\"\n");
			colorchange = RGB_BLUEISH;
		}
		return RGB_BLUEISH;
	}
	else if ((GetSleeveModelIndex() == SLEEVE_RED))
	{
		if (colorchange != RGB_REDISH)
		{
			gEngfuncs.pfnClientCmd("con_color \"255 80 0\"\n");
			gEngfuncs.pfnClientCmd("tracerred \"1\"\ntracerblue \"0\"\ntracergreen \"0\"\n");
			colorchange = RGB_REDISH;
		}
		return RGB_REDISH;
	}
	else if ((GetSleeveModelIndex() == SLEEVE_GREEN))
	{
		if (colorchange != RGB_GREENISH)
		{
			gEngfuncs.pfnClientCmd("con_color \"0 200 0\"\n");
			gEngfuncs.pfnClientCmd("tracerred \"0\"\ntracerblue \"0\"\ntracergreen \"1\"\n");
			colorchange = RGB_GREENISH;
		}
		return RGB_GREENISH;
	}

	if (colorchange != RGB_YELLOWISH)
	{
		gEngfuncs.pfnClientCmd("con_color \"255 180 30\"\n");
		gEngfuncs.pfnClientCmd("tracerred \"1\"\ntracerblue \"0\"\ntracergreen \"0.8\"\n");
		colorchange = RGB_YELLOWISH;
	}

	return RGB_YELLOWISH;
}

float lerp(float a, float b, float f)
{
    return (a * (1.0 - f)) + (b * f);
}

bool ScoreBased( void )
{
	return (gHUD.m_Teamplay == GAME_ARENA ||
			gHUD.m_Teamplay == GAME_LMS ||
			gHUD.m_Teamplay == GAME_BUSTERS ||
			gHUD.m_Teamplay == GAME_CHILLDEMIC ||
			gHUD.m_Teamplay == GAME_COLDSKULL ||
			gHUD.m_Teamplay == GAME_COLDSPOT ||
			gHUD.m_Teamplay == GAME_CTC ||
			gHUD.m_Teamplay == GAME_CTF ||
			gHUD.m_Teamplay == GAME_HORDE ||
			gHUD.m_Teamplay == GAME_ICEMAN ||
			gHUD.m_Teamplay == GAME_PROPHUNT ||
			gHUD.m_Teamplay == GAME_SHIDDEN ||
			gHUD.m_Teamplay == GAME_GUNGAME ||
			gHUD.m_Teamplay == GAME_LOOT ||
			gHUD.m_Teamplay == GAME_KTS);
}

bool SortByWins( void )
{
	return (gHUD.m_Teamplay == GAME_BUSTERS ||
			gHUD.m_Teamplay == GAME_COLDSKULL ||
			gHUD.m_Teamplay == GAME_COLDSPOT ||
			gHUD.m_Teamplay == GAME_CTC ||
			gHUD.m_Teamplay == GAME_CTF ||
			gHUD.m_Teamplay == GAME_GUNGAME ||
			gHUD.m_Teamplay == GAME_LOOT ||
			gHUD.m_Teamplay == GAME_KTS);
}

bool IndividualPlayer( void )
{
	return (gHUD.m_Teamplay == GAME_COLDSKULL ||
			gHUD.m_Teamplay == GAME_GUNGAME);
}

const char *GetServerName( void )
{
	if (gViewPort && gViewPort->m_szServerName && gViewPort->m_szServerName[0])
	{
		return gViewPort->m_szServerName;
	}
	else
	{
		return "Unknown server name";
	}
}

const char *GetMapName( void )
{
    char sz[MAX_SERVERNAME_LENGTH + 32] = {0};
    static char szTitle[256] = {0}; // Use static to ensure it persists after the function returns
    char *ch;

    // Update the level name
    if (gEngfuncs.pfnGetLevelName && gEngfuncs.pfnGetLevelName()[0])
    {
        const char *level = gEngfuncs.pfnGetLevelName();
        strncpy(sz, level, sizeof(sz) - 1); // Use strncpy for safety
        ch = strchr(sz, '/');
        if (!ch)
            ch = strchr(sz, '\\');
        if (ch)
        {
            strncpy(szTitle, ch + 1, sizeof(szTitle) - 1);
            ch = strchr(szTitle, '.');
            if (ch)
                *ch = '\0';
        }
        else
        {
            strncpy(szTitle, "unknown", sizeof(szTitle) - 1);
        }
    }
    else
    {
        strncpy(szTitle, "unknown", sizeof(szTitle) - 1);
    }

    return szTitle;
}

const char *GetGameName( void )
{
	switch (gHUD.m_Teamplay)
	{
		case GAME_ARENA:
			return "1 vs. 1";
		case GAME_LMS:
			return "Battle Royale";
		case GAME_BUSTERS:
			return "Busters";
		case GAME_CHILLDEMIC:	
			return "Chilldemic";
		case GAME_COLDSKULL:
			return "Coldskull";
		case GAME_COLDSPOT:
			return "Coldspot";
		case GAME_CTC:
			return "Capture the Chumtoad";
		case GAME_CTF:
			return "Capture the Flag";
		case GAME_GUNGAME:
			return "Gun Game";
		case GAME_HORDE:
			return "Horde";
		case GAME_INSTAGIB:
			return "Instagib";
		case GAME_ICEMAN:
			return "Santas vs. Jesus";
		case GAME_KTS:
			return "Kick the Snowball";
		case GAME_LOOT:
			return "Loot";
		case GAME_PROPHUNT:
			return "Prop Hunt";
		case GAME_SHIDDEN:
			return "Stealth Hidden";
		case GAME_SNOWBALL:
			return "Snowball Fight";
		case GAME_TEAMPLAY:
			return "Team Deathmatch";
		default:
			return "Free for All";
	}
}
