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
	return 1;
}


int count = 1;
int g_TotalCount = 0;
int g_PositionCount = 0;

int CHudObjective::CalcPosition( )
{
	gHUD.m_Scoreboard.GetAllPlayersInfo();
	count = 1;
	if ( !ScoreBased() && gHUD.m_Teamplay != GAME_TEAMPLAY )
	{
		// it's not teamplay,  so just draw a simple player list
		CalcPlayer( );
		g_TotalCount = count;
		return 1;
	}

	int i = 0;
	// clear out team scores
	for ( i = 1; i <= gHUD.m_Scoreboard.m_iNumTeams; i++ )
	{
		if ( !g_TeamInfo[i].scores_overriden )
			g_TeamInfo[i].frags = g_TeamInfo[i].deaths = g_TeamInfo[i].score = 0;
		g_TeamInfo[i].ping = g_TeamInfo[i].packetloss = 0;
	}

	// recalc the team scores, then draw them
	for ( i = 1; i < MAX_PLAYERS; i++ )
	{
		if ( g_PlayerInfoList[i].name == NULL )
			continue; // empty player slot, skip

		if ( g_PlayerExtraInfo[i].teamname[0] == 0 )
			continue; // skip over players who are not in a team

		int j = 0;
		// find what team this player is in
		for ( j = 1; j <= gHUD.m_Scoreboard.m_iNumTeams; j++ )
		{
			if ( !stricmp( g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name ) )
				break;
		}
		if ( j > gHUD.m_Scoreboard.m_iNumTeams )  // player is not in a team, skip to the next guy
			continue;

		if ( !g_TeamInfo[j].scores_overriden )
		{
			g_TeamInfo[j].frags += g_PlayerExtraInfo[i].frags;
			g_TeamInfo[j].deaths += g_PlayerExtraInfo[i].deaths;
			g_TeamInfo[j].score += g_PlayerExtraInfo[i].playerclass;
		}

		g_TeamInfo[j].ping += g_PlayerInfoList[i].ping;
		g_TeamInfo[j].packetloss += g_PlayerInfoList[i].packetloss;

		if ( g_PlayerInfoList[i].thisplayer )
			g_TeamInfo[j].ownteam = TRUE;
		else
			g_TeamInfo[j].ownteam = FALSE;
	}

	// find team ping/packetloss averages
	for ( i = 1; i <= gHUD.m_Scoreboard.m_iNumTeams; i++ )
	{
		g_TeamInfo[i].already_drawn = FALSE;

		if ( g_TeamInfo[i].players > 0 )
		{
			g_TeamInfo[i].ping /= g_TeamInfo[i].players;  // use the average ping of all the players in the team as the teams ping
			g_TeamInfo[i].packetloss /= g_TeamInfo[i].players;  // use the average ping of all the players in the team as the teams ping
		}
	}

	// Draw the teams
	while ( 1 )
	{
		int highest = -99999; int lowest_deaths = 99999;
		int best_team = 0;
		int which = 0;

		for ( i = 1; i <= gHUD.m_Scoreboard.m_iNumTeams; i++ )
		{
			if ( g_TeamInfo[i].players < 0 )
				continue;

			which = SortByWins() ? g_TeamInfo[i].score : g_TeamInfo[i].frags;

			if ( !g_TeamInfo[i].already_drawn && which >= highest )
			{
				if ( which > highest || g_TeamInfo[i].deaths < lowest_deaths )
				{
					best_team = i;
					lowest_deaths = g_TeamInfo[i].deaths;
					if (SortByWins())
						highest = g_TeamInfo[i].score;
					else
						highest = g_TeamInfo[i].frags;
				}
			}
		}

		// draw the best team on the scoreboard
		if ( !best_team )
			break;

		// draw out the best team
		team_info_t *team_info = &g_TeamInfo[best_team];
		team_info->already_drawn = TRUE;  // set the already_drawn to be TRUE, so this team won't get drawn again

		// draw all the players that belong to this team, indented slightly
		CalcPlayer( team_info->name );
	}

	CalcPlayer("");

	return 1;
}

int CHudObjective::CalcPlayer( char *team )
{
	// draw the players, in order,  and restricted to team if set
	while ( 1 )
	{
		// Find the top ranking player
		int highest = -99999;	int lowest_deaths = 99999;
		int best_player = 0;

		for ( int i = 1; i < MAX_PLAYERS; i++ )
		{
			int which = SortByWins() && team != NULL && strlen(team) ? g_PlayerExtraInfo[i].playerclass : g_PlayerExtraInfo[i].frags;
			if ( g_PlayerInfoList[i].name && (which >= highest) )
			{
				if ( !(team && stricmp(g_PlayerExtraInfo[i].teamname, team)) )  // make sure it is the specified team
				{
					extra_player_info_t *pl_info = &g_PlayerExtraInfo[i];
					if ( which > highest || pl_info->deaths < lowest_deaths )
					{
						best_player = i;
						lowest_deaths = pl_info->deaths;
						if (SortByWins())
							highest = pl_info->playerclass;
						else
							highest = pl_info->frags;
					}
				}
			}
		}

		if ( !best_player )
			break;

		// draw out the best player
		hud_player_info_t *pl_info = &g_PlayerInfoList[best_player];

		if ( best_player == gHUD.m_Scoreboard.m_iLastKilledBy && 
			gHUD.m_Scoreboard.m_fLastKillTime && gHUD.m_Scoreboard.m_fLastKillTime > gHUD.m_flTime )
		{
			// noop
		}
		else if ( pl_info->thisplayer ) // if it is their name, draw it a different color
		{
			g_PositionCount = count;
		}

		count++;

		pl_info->name = NULL;  // set the name to be NULL, so this client won't get drawn again
	}

	return g_PositionCount;
}

extern float g_xP, g_yP;

int CHudObjective::Draw(float flTime)
{
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

	// position
	extern float g_ScoreTime;
	extern cvar_t *cl_showposition;
	if (cl_showposition && cl_showposition->value)
	{
		if (!g_iUser1 && gHUD.m_GameMode != GAME_CTF && strlen(m_szGoalMessage))
		{
			if (g_ScoreTime < gEngfuncs.GetClientTime())
			{
				int old = g_PositionCount;
				CalcPosition();
				//if (old != 1 && g_PositionCount == 1)
				//	PlaySound("takenlead.wav", 1);
				//if (old == 1 && g_PositionCount != 1)
				//	PlaySound("lostlead.wav", 1);
				g_ScoreTime = gEngfuncs.GetClientTime() + gEngfuncs.pfnRandomFloat(3, 5);
			}

			if (!gHUD.m_Health.m_bitsDamage)
			{
				int r, g, b;
				UnpackRGB(r, g, b, HudColor());
				ScaleColors(r, g, b, MIN_ALPHA);
				int y = ((ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2) - 64) + g_yP;
				int x = 24 + g_xP;
				SPR_Set(gHUD.GetSprite(gHUD.GetSpriteIndex("item_ctfflagg")), r, g, b);
				SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(gHUD.GetSpriteIndex("item_ctfflagg")));
				gHUD.DrawHudNumber(x + 48, y + 12, DHN_2DIGITS | DHN_DRAWZERO, g_PositionCount, r, g, b);
				//gHUD.DrawHudNumber(x + 64, y + 12, DHN_2DIGITS | DHN_DRAWZERO, g_TotalCount, r, g, b);
			}
		}
	}

	if (!cl_objectives->value)
		return 1;

	int r, g, b;
	int size = 0;
	int y = 16;
	int x = cl_radar->value ? 130 : 0;
	int padding = 32;

	// Shift for spectators
	if (g_iUser1)
	{
		x = 10;
		y += 56;
	}

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
		FillRGBA(x, y + gHUD.m_iFontHeight + 2, (int)result, 2, r, g, b, 200);
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
