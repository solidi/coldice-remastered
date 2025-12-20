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

unsigned long HudColor()
{
	static unsigned long colorchange = RGB_BLUEISH;

	// Mutator always overrides
	if (gHUD.m_IceModelsIndex == SKIN_MUTATOR)
	{
		if (colorchange != RGB_YELLOWISH)
		{
			gEngfuncs.pfnClientCmd("con_color \"255 180 30\"\n");
			gEngfuncs.pfnClientCmd("tracerred \"1\"\ntracerblue \"0\"\n");
			colorchange = RGB_YELLOWISH;
		}

		return RGB_YELLOWISH;
	}

	// Special case for red team in game modes
	if (gHUD.m_GameMode == GAME_CTF || gHUD.m_GameMode == GAME_ICEMAN || 
		gHUD.m_GameMode == GAME_COLDSPOT || gHUD.m_GameMode == GAME_LMS ||
		gHUD.m_GameMode == GAME_CHILLDEMIC || gHUD.m_GameMode == GAME_CTC)
	{
		cl_entity_t *local = gEngfuncs.GetLocalPlayer();
		if (local->curstate.fuser4 > 0)
		{
			if (colorchange != RGB_REDISH)
			{
				gEngfuncs.pfnClientCmd("con_color \"255 80 0\"\n");
				gEngfuncs.pfnClientCmd("tracerred \"1\"\ntracerblue \"0\"\n");
				colorchange = RGB_REDISH;
			}
			return RGB_REDISH;
		}
	}

	if ((gHUD.m_IceModelsIndex >= SKIN_INVERSE && gHUD.m_IceModelsIndex <= SKIN_GOLD))
	{
		if (colorchange != RGB_BLUEISH)
		{
			gEngfuncs.pfnClientCmd("con_color \"0 160 255\"\n");
			gEngfuncs.pfnClientCmd("tracerred \"0\"\ntracerblue \"1\"\n");
			colorchange = RGB_BLUEISH;
		}
		return RGB_BLUEISH;
	}

	if (colorchange != RGB_YELLOWISH)
	{
		gEngfuncs.pfnClientCmd("con_color \"255 180 30\"\n");
		gEngfuncs.pfnClientCmd("tracerred \"1\"\ntracerblue \"0\"\n");
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
			gHUD.m_Teamplay == GAME_GUNGAME);
}

bool SortByWins( void )
{
	return (gHUD.m_Teamplay == GAME_BUSTERS ||
			gHUD.m_Teamplay == GAME_COLDSKULL ||
			gHUD.m_Teamplay == GAME_COLDSPOT ||
			gHUD.m_Teamplay == GAME_CTC ||
			gHUD.m_Teamplay == GAME_CTF ||
			gHUD.m_Teamplay == GAME_GUNGAME);
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
		case GAME_HORDE:
			return "Horde";
		case GAME_ICEMAN:
			return "Santas vs. Jesus";
		case GAME_INSTAGIB:
			return "Instagib";
		case GAME_LMS:
			return "Battle Royale";
		case GAME_PROPHUNT:
			return "Prop Hunt";
		case GAME_SHIDDEN:
			return "Stealth Hidden";
		case GAME_SNOWBALL:
			return "Snowball Fight";
		case GAME_TEAMPLAY:
			return "Team Deathmatch";
		case GAME_GUNGAME:
			return "Gun Game";
		default:
			return "Free for All";
	}
}
