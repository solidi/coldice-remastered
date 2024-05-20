/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
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
// death notice
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "event_api.h"

#include <string.h>
#include <stdio.h>

#include "vgui_TeamFortressViewport.h"

DECLARE_MESSAGE( m_DeathNotice, DeathMsg );

struct DeathNoticeItem {
	char szKiller[MAX_PLAYER_NAME_LENGTH*2];
	char szAssist[MAX_PLAYER_NAME_LENGTH*2];
	char szVictim[MAX_PLAYER_NAME_LENGTH*2];
	int iId;	// the index number of the associated sprite
	int iSuicide;
	int iTeamKill;
	int iNonPlayerKill;
	float flDisplayTime;
	float *KillerColor;
	float *AssistColor;
	float *VictimColor;
	int iKillerIsMe;
};

#define MAX_DEATHNOTICES	4
static int DEATHNOTICE_DISPLAY_TIME = 6;

#define DEATHNOTICE_TOP		32

DeathNoticeItem rgDeathNoticeList[ MAX_DEATHNOTICES + 1 ];

float g_ColorBlue[3]	= { 0.6, 0.8, 1.0 };
float g_ColorRed[3]		= { 1.0, 0.25, 0.25 };
float g_ColorGreen[3]	= { 0.6, 1.0, 0.6 };
float g_ColorYellow[3]	= { 1.0, 0.7, 0.0 };
float g_ColorGrey[3]	= { 0.8, 0.8, 0.8 };

static float m_fLastKillTime = 0;
static float m_fNextKillSpeakTime = 0;
static int m_iKillCount = 0;

enum kills {
	DOUBLE_KILL = 2,
	MULTI_KILL,
	ULTRA_KILL,
	MONSTER_KILL,
	LUDICROUS_KILL,
	HS_KILL,
	WTF_KILL
};

#define KILL_SPREE_SECS 3
extern cvar_t *cl_achievements;
extern cvar_t *cl_playpoint;
extern float g_xP, g_yP;

float *GetClientColor( int clientIndex )
{
	switch ( g_PlayerExtraInfo[clientIndex].teamnumber )
	{
	case 1:	return g_ColorBlue;
	case 2: return g_ColorRed;
	case 3: return g_ColorYellow;
	case 4: return g_ColorGreen;
	case 0: return g_ColorBlue;

		default	: return g_ColorGrey;
	}

	return NULL;
}

int CHudDeathNotice :: Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( DeathMsg );

	CVAR_CREATE( "hud_deathnotice_time", "6", 0 );

	return 1;
}


void CHudDeathNotice :: InitHUDData( void )
{
	memset( rgDeathNoticeList, 0, sizeof(rgDeathNoticeList) );
}


int CHudDeathNotice :: VidInit( void )
{
	m_HUD_d_skull = gHUD.GetSpriteIndex( "d_skull" );
	m_HUD_killspree = gHUD.GetSpriteIndex( "killspree" );

	return 1;
}

int CHudDeathNotice :: Draw( float flTime )
{
	int x, y, r, g, b;

	for ( int i = 0; i < MAX_DEATHNOTICES; i++ )
	{
		if ( rgDeathNoticeList[i].iId == 0 )
			break;  // we've gone through them all

		if ( rgDeathNoticeList[i].flDisplayTime < flTime )
		{ // display time has expired
			// remove the current item from the list
			memmove( &rgDeathNoticeList[i], &rgDeathNoticeList[i+1], sizeof(DeathNoticeItem) * (MAX_DEATHNOTICES - i) );
			i--;  // continue on the next item;  stop the counter getting incremented
			continue;
		}

		rgDeathNoticeList[i].flDisplayTime = fmin( rgDeathNoticeList[i].flDisplayTime, gHUD.m_flTime + DEATHNOTICE_DISPLAY_TIME );

		// Only draw if the viewport will let me
		if ( gViewPort && gViewPort->AllowedToPrintText() )
		{
			int start = 0;
			int topMargin = DEATHNOTICE_TOP;

			if (g_iUser1)
			{
				topMargin += 44;
			}
			else if (!(gHUD.m_iHideHUDDisplay & HIDEHUD_FLASHLIGHT))
			{
				topMargin += 24;
			}

			// Draw the death notice
			y = (topMargin + 2 + (30 * i)) + g_yP;  //!!!

			int id = (rgDeathNoticeList[i].iId == -1) ? m_HUD_d_skull : rgDeathNoticeList[i].iId;
			start = x = (ScreenWidth - ConsoleStringLen(rgDeathNoticeList[i].szVictim) - (gHUD.GetSpriteRect(id).right - gHUD.GetSpriteRect(id).left) - 20) + g_xP;
			
			if (rgDeathNoticeList[i].iKillerIsMe )
				UnpackRGB( r, g, b, RGB_REDISH );
			else
				UnpackRGB( r, g, b, HudColor() );

			if ( !rgDeathNoticeList[i].iSuicide )
			{
				start = x -= (5 + ConsoleStringLen( rgDeathNoticeList[i].szKiller ) );
				if (rgDeathNoticeList[i].szAssist[0] != 0)
					start = x -= (15 + ConsoleStringLen( rgDeathNoticeList[i].szAssist ) );
				FillRGBA(start - 10, y - 5, (ScreenWidth - (start - g_xP)), (gHUD.GetSpriteRect(id).bottom - gHUD.GetSpriteRect(id).top) + 10, r, g, b, 20);

				// Draw killers name
				if ( rgDeathNoticeList[i].KillerColor )
					gEngfuncs.pfnDrawSetTextColor( rgDeathNoticeList[i].KillerColor[0], rgDeathNoticeList[i].KillerColor[1], rgDeathNoticeList[i].KillerColor[2] );
				x = 5 + DrawConsoleString( x, y, rgDeathNoticeList[i].szKiller );
				if ( rgDeathNoticeList[i].AssistColor )
					gEngfuncs.pfnDrawSetTextColor( rgDeathNoticeList[i].AssistColor[0], rgDeathNoticeList[i].AssistColor[1], rgDeathNoticeList[i].AssistColor[2] );
				
				if (rgDeathNoticeList[i].szAssist[0] != 0)
				{
					static char assist[64];
					sprintf(assist, "+ %s", rgDeathNoticeList[i].szAssist);
					x = 5 + DrawConsoleString( x, y, assist );
				}
			}
			else
			{
				FillRGBA(start - 10, y - 5, (ScreenWidth - (start - g_xP)), (gHUD.GetSpriteRect(id).bottom - gHUD.GetSpriteRect(id).top) + 10, r, g, b, 20);
			}

			if ( rgDeathNoticeList[i].iTeamKill )
			{
				r = 10;	g = 240; b = 10;  // display it in sickly green
			}

			// Draw death weapon
			SPR_Set( gHUD.GetSprite(id), r, g, b );
			SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(id) );

			x += (gHUD.GetSpriteRect(id).right - gHUD.GetSpriteRect(id).left);

			// Draw victims name (if it was a player that was killed)
			if (rgDeathNoticeList[i].iNonPlayerKill == FALSE)
			{
				if ( rgDeathNoticeList[i].VictimColor )
					gEngfuncs.pfnDrawSetTextColor( rgDeathNoticeList[i].VictimColor[0], rgDeathNoticeList[i].VictimColor[1], rgDeathNoticeList[i].VictimColor[2] );
				x = (DrawConsoleString( x, y, rgDeathNoticeList[i].szVictim )) + g_xP;
			}
			else
			{
				x = (DrawConsoleString( x, y, "Monster" )) + g_xP;
			}
		}
	}

	if (cl_achievements && !cl_achievements->value)
		return 1;

	PlayUTKills();

	// Visualize only if we are on a spree.
	if (m_iKillCount >= DOUBLE_KILL && m_fLastKillTime > gEngfuncs.GetClientTime()) {
		UnpackRGB( r, g, b, HudColor() );

		// Draw circle
		int hard_width = 128, hard_height = 128;

		if (cl_achievements && cl_achievements->value == 3) {
			wrect_t *t = &gHUD.GetSpriteRect(m_HUD_killspree);
			t->bottom = (hard_height * (((m_fLastKillTime - gEngfuncs.GetClientTime()) / KILL_SPREE_SECS) / 2)) + 64;
			t->top = hard_height - t->bottom;

			if (t->bottom > 0 && t->top < 64) {
				SPR_Set(gHUD.GetSprite(m_HUD_killspree), r, g, b);
				SPR_DrawAdditive(m_iKillCount, (ScreenWidth / 2) - (hard_width / 2), (ScreenHeight / 2) - ((t->bottom - t->top) / 2), t);
			}
		}

		int size = 0;
		// m_iKillCount = MULTI_KILL;
		switch (m_iKillCount)
		{
		case DOUBLE_KILL:
			size = ConsoleStringLen("Double Kill!");
			DrawConsoleString((ScreenWidth / 2) - (size / 2), (ScreenHeight / 2) + (hard_width / 2), "Double Kill!");
			break;
		case MULTI_KILL:
			size = ConsoleStringLen("Multi Kill!");
			DrawConsoleString((ScreenWidth / 2) - (size / 2), (ScreenHeight / 2) + (hard_width / 2), "Multi Kill!");
			break;
		case ULTRA_KILL:
			size = ConsoleStringLen("Ultra Kill!");
			DrawConsoleString((ScreenWidth / 2) - (size / 2), (ScreenHeight / 2) + (hard_width / 2), "Ultra Kill!");
			break;
		case MONSTER_KILL:
			size = ConsoleStringLen("MONSTER KILL!");
			DrawConsoleString((ScreenWidth / 2) - (size / 2), (ScreenHeight / 2) + (hard_width / 2), "MONSTER KILL!");
			break;
		case LUDICROUS_KILL:
			size = ConsoleStringLen("LUDICROUS KILL!!");
			DrawConsoleString((ScreenWidth / 2) - (size / 2),(ScreenHeight / 2) + (hard_width / 2), "LUDICROUS KILL!!");
			break;
		case HS_KILL:
			size = ConsoleStringLen("HOLY SHIT!!!");
			DrawConsoleString((ScreenWidth / 2) - (size / 2), (ScreenHeight / 2) + (hard_width / 2), "HOLY SHIT!!!");
			break;
		case WTF_KILL:
			size = ConsoleStringLen("WTF!!!!");
			DrawConsoleString((ScreenWidth / 2) - (size / 2), (ScreenHeight / 2) + (hard_width / 2), "WTF!!!!");
			break;
		}

		// Draw the bar
		if (cl_achievements && cl_achievements->value == 2) {
			y = (ScreenHeight / 2) + (hard_width / 2) - (gHUD.m_iFontHeight / 2);
			float delta = m_fLastKillTime - gEngfuncs.GetClientTime();
			float width = (size * 2) * (delta / 3);
			x = (ScreenWidth / 2) - (width / 2);
			FillRGBA(x, y, width, gHUD.m_iFontHeight / 2, r, g, b, 120 * delta);
		}
	}

	return 1;
}

// This message handler may be better off elsewhere
int CHudDeathNotice :: MsgFunc_DeathMsg( const char *pszName, int iSize, void *pbuf )
{
	m_iFlags |= HUD_ACTIVE;

	BEGIN_READ( pbuf, iSize );

	int killer = READ_BYTE();
	int assist = READ_BYTE();
	int victim = READ_BYTE();

	char killedwith[32];
	strcpy( killedwith, "d_" );
	strncat( killedwith, READ_STRING(), 32 );

	if (gViewPort)
		gViewPort->DeathMsg( killer, victim );

	gHUD.m_Spectator.DeathMessage(victim);
	int i;
	for ( i = 0; i < MAX_DEATHNOTICES; i++ )
	{
		if ( rgDeathNoticeList[i].iId == 0 )
			break;
	}
	if ( i == MAX_DEATHNOTICES )
	{ // move the rest of the list forward to make room for this item
		memmove( rgDeathNoticeList, rgDeathNoticeList+1, sizeof(DeathNoticeItem) * MAX_DEATHNOTICES );
		i = MAX_DEATHNOTICES - 1;
	}

	if (gViewPort)
		gViewPort->GetAllPlayersInfo();

	CalculateUTKills(killer, victim);

	if (m_iKillCount < DOUBLE_KILL)
		PlayKillSound(killer, victim);

	// Get the Killer's name
	char *killer_name = g_PlayerInfoList[ killer ].name;
	if ( !killer_name )
	{
		killer_name = "";
		rgDeathNoticeList[i].szKiller[0] = 0;
	}
	else
	{
		rgDeathNoticeList[i].KillerColor = GetClientColor( killer );
		strncpy( rgDeathNoticeList[i].szKiller, killer_name, MAX_PLAYER_NAME_LENGTH );
		rgDeathNoticeList[i].szKiller[MAX_PLAYER_NAME_LENGTH-1] = 0;
	}

	// Get the Victim's name
	char *victim_name = NULL;
	char *assist_name = NULL;
	// If victim is -1, the killer killed a specific, non-player object (like a sentrygun)
	if ( ((char)victim) != -1 )
		victim_name = g_PlayerInfoList[ victim ].name;
	if ( ((char)assist) != -1 )
		assist_name = g_PlayerInfoList[ assist ].name;
	if ( !assist_name )
	{
		assist_name = "";
		rgDeathNoticeList[i].szAssist[0] = 0;
	}
	else
	{
		rgDeathNoticeList[i].AssistColor = GetClientColor( assist );
		strncpy( rgDeathNoticeList[i].szAssist, assist_name, MAX_PLAYER_NAME_LENGTH );
		rgDeathNoticeList[i].szAssist[MAX_PLAYER_NAME_LENGTH-1] = 0;
	}

	if ( !victim_name )
	{
		victim_name = "";
		rgDeathNoticeList[i].szVictim[0] = 0;
	}
	else
	{
		rgDeathNoticeList[i].VictimColor = GetClientColor( victim );
		strncpy( rgDeathNoticeList[i].szVictim, victim_name, MAX_PLAYER_NAME_LENGTH );
		rgDeathNoticeList[i].szVictim[MAX_PLAYER_NAME_LENGTH-1] = 0;
	}

	// Is it a non-player object kill?
	if ( ((char)victim) == -1 )
	{
		rgDeathNoticeList[i].iNonPlayerKill = TRUE;

		// Store the object's name in the Victim slot (skip the d_ bit)
		strcpy( rgDeathNoticeList[i].szVictim, killedwith+2 );
	}
	else
	{
		if ( killer == victim || killer == 0 )
			rgDeathNoticeList[i].iSuicide = TRUE;
		
		if ( gEngfuncs.pEventAPI->EV_IsLocal( killer - 1 ) )
			rgDeathNoticeList[i].iKillerIsMe = TRUE;

		if ( !strcmp( killedwith, "d_teammate" ) )
			rgDeathNoticeList[i].iTeamKill = TRUE;
	}

	// Find the sprite in the list
	int spr = gHUD.GetSpriteIndex( killedwith );

	rgDeathNoticeList[i].iId = spr;

	DEATHNOTICE_DISPLAY_TIME = CVAR_GET_FLOAT( "hud_deathnotice_time" );
	rgDeathNoticeList[i].flDisplayTime = gHUD.m_flTime + DEATHNOTICE_DISPLAY_TIME;

	if (rgDeathNoticeList[i].iNonPlayerKill)
	{
		ConsolePrint( rgDeathNoticeList[i].szKiller );
		ConsolePrint( " killed a " );
		ConsolePrint( rgDeathNoticeList[i].szVictim );
		ConsolePrint( "\n" );
	}
	else
	{
		// record the death notice in the console
		if ( rgDeathNoticeList[i].iSuicide )
		{
			ConsolePrint( rgDeathNoticeList[i].szVictim );

			if ( !strcmp( killedwith, "d_world" ) )
			{
				ConsolePrint( " died" );
			}
			else
			{
				ConsolePrint( " killed self" );
			}
		}
		else if ( rgDeathNoticeList[i].iTeamKill )
		{
			ConsolePrint( rgDeathNoticeList[i].szKiller );
			ConsolePrint( " killed his teammate " );
			ConsolePrint( rgDeathNoticeList[i].szVictim );
		}
		else
		{
			ConsolePrint( rgDeathNoticeList[i].szKiller );
			if (rgDeathNoticeList[i].szAssist[0] != 0)
			{
				ConsolePrint( " assisted by " );
				ConsolePrint( rgDeathNoticeList[i].szAssist );
			}

			ConsolePrint( " killed " );
			ConsolePrint( rgDeathNoticeList[i].szVictim );
		}

		if ( killedwith && *killedwith && (*killedwith > 13 ) && strcmp( killedwith, "d_world" ) && !rgDeathNoticeList[i].iTeamKill )
		{
			ConsolePrint( " with " );

			// replace the code names with the 'real' names
			if ( !strcmp( killedwith+2, "egon" ) )
				strcpy( killedwith, "d_gluon gun" );
			if ( !strcmp( killedwith+2, "gauss" ) )
				strcpy( killedwith, "d_tau cannon" );

			ConsolePrint( killedwith+2 ); // skip over the "d_" part
		}

		ConsolePrint( "\n" );
	}

	return 1;
}

void CHudDeathNotice::PlayKillSound(int killer, int victim)
{
	if (cl_playpoint && !cl_playpoint->value) {
		return;
	}

	if (g_PlayerInfoList[killer].thisplayer && killer != victim) {
		PlaySound("point.wav", 2);
	}
}

void CHudDeathNotice::CalculateUTKills(int killer, int victim)
{
	if (!g_PlayerInfoList[killer].thisplayer)
		return;

	if (killer == victim)
		return;

	if (m_fLastKillTime < gEngfuncs.GetClientTime())
		m_iKillCount = 0;
	
	m_iKillCount++;
	m_iKillCount = fmin(m_iKillCount, WTF_KILL);
	// Give some time for kills to add if there is more than one, then play.
	m_fNextKillSpeakTime = gEngfuncs.GetClientTime() + 0.25;
	m_fLastKillTime = gEngfuncs.GetClientTime() + KILL_SPREE_SECS;
}

void CHudDeathNotice::PlayUTKills() {
	// Unstick kills
	if ((gEngfuncs.GetClientTime() + 5) < m_fLastKillTime) {
		m_fLastKillTime = 0;
		m_iKillCount = 0;
		m_fNextKillSpeakTime = 0;
	}

	if (m_fNextKillSpeakTime == 0 || m_fNextKillSpeakTime > gEngfuncs.GetClientTime())
		return;

	// sound and print
	switch (m_iKillCount)
	{
	case DOUBLE_KILL:
		PlaySound("doublekill.wav", 1);
		break;
	case MULTI_KILL:
		PlaySound("multikill.wav", 1);
		break;
	case ULTRA_KILL:
		PlaySound("ultrakill.wav", 1);
		break;
	case MONSTER_KILL:
		PlaySound("monsterkill.wav", 1);
		break;
	case LUDICROUS_KILL:
		PlaySound("ludicrouskill.wav", 1);
		break;
	case HS_KILL:
		PlaySound("holyshitkill.wav", 1);
		break;
	case WTF_KILL:
		PlaySound("wtfkill.wav", 1);
		break;
	}

	m_fNextKillSpeakTime = 0;
}
