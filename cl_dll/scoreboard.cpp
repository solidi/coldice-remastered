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
// Scoreboard.cpp
//
// implementation of CHudScoreboard class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>
#include "vgui_TeamFortressViewport.h"

//DECLARE_COMMAND( m_Scoreboard, ShowScores );
//DECLARE_COMMAND( m_Scoreboard, HideScores );

DECLARE_MESSAGE( m_Scoreboard, ScoreInfo2 );
DECLARE_MESSAGE( m_Scoreboard, TeamInfo2 );
DECLARE_MESSAGE( m_Scoreboard, TeamScore2 );

extern cvar_t *cl_oldscoreboard;

int CHudScoreboard :: Init( void )
{
	gHUD.AddHudElem( this );

	// Hook messages & commands here
	//HOOK_COMMAND( "+showscores", ShowScores );
	//HOOK_COMMAND( "-showscores", HideScores );

	HOOK_MESSAGE( ScoreInfo2 );
	HOOK_MESSAGE( TeamScore2 );
	HOOK_MESSAGE( TeamInfo2 );

	InitHUDData();

	cl_showpacketloss = CVAR_CREATE( "cl_showpacketloss", "0", FCVAR_ARCHIVE );

	return 1;
}


int CHudScoreboard :: VidInit( void )
{
	// Load sprites here

	return 1;
}

void CHudScoreboard :: InitHUDData( void )
{
	memset( g_PlayerExtraInfo, 0, sizeof g_PlayerExtraInfo );
	m_iLastKilledBy = 0;
	m_fLastKillTime = 0;
	m_iPlayerNum = 0;
	m_iNumTeams = 0;
	memset( g_TeamInfo, 0, sizeof g_TeamInfo );

	m_iFlags &= ~HUD_ACTIVE;  // starts out inactive

	m_iFlags |= HUD_INTERMISSION; // is always drawn during an intermission
}

/* The scoreboard
We have a minimum width of 1-320 - we could have the field widths scale with it?
*/

// X positions
// relative to the side of the scoreboard
#define NAME_RANGE_MIN  20
#define NAME_RANGE_MAX  265
#define KILLS_RANGE_MIN 250
#define KILLS_RANGE_MAX 290
#define DIVIDER_POS		300
#define DEATHS_RANGE_MIN  310
#define DEATHS_RANGE_MAX  350
#define SCORE_RANGE_MIN  370
#define SCORE_RANGE_MAX  410
#define PING_RANGE_MIN	445
#define PING_RANGE_MAX	475
#define PL_RANGE_MIN 495
#define PL_RANGE_MAX 555

int SCOREBOARD_WIDTH = 500;
		

// Y positions
#define ROW_GAP  21
#define ROW_RANGE_MIN 15
#define ROW_RANGE_MAX ( ScreenHeight - 50 )

int CHudScoreboard :: Draw( float fTime )
{
	if (cl_oldscoreboard && !cl_oldscoreboard->value) {
		return 1;
	}

	int can_show_packetloss = 0;
	int FAR_RIGHT;

	if ( !m_iShowscoresHeld && gHUD.m_Health.m_iHealth > 0 && !gHUD.m_iIntermission )
		return 1;

	GetAllPlayersInfo();

	//  Packetloss removed on Kelly 'shipping nazi' Bailey's orders
	if ( cl_showpacketloss && cl_showpacketloss->value && ( ScreenWidth >= 400 ) )
	{
		can_show_packetloss = 1;
		SCOREBOARD_WIDTH = 560;
	}
	else
	{
		SCOREBOARD_WIDTH = 500;
	}

	// just sort the list on the fly
	// list is sorted first by frags, then by deaths
	float list_slot = 0;
	int xpos_rel = (ScreenWidth - SCOREBOARD_WIDTH) / 2;

	// Shift for spectators
	int margin = 0;
	if (g_iUser1)
		margin += 54;

	// print the heading line
	int ypos = ROW_RANGE_MIN + margin + (list_slot * ROW_GAP);
	int	xpos = NAME_RANGE_MIN + xpos_rel;

	int r, g, b;
	UnpackRGB(r, g, b, HudColor());

	// gEngfuncs.Con_DPrintf("gHUD.m_Teamplay = %d\n", gHUD.m_Teamplay);

	if ( !ScoreBased() )
		gHUD.DrawHudString( xpos, ypos, NAME_RANGE_MAX + xpos_rel, "Player", r, g, b );
	else
		gHUD.DrawHudString( xpos, ypos, NAME_RANGE_MAX + xpos_rel, "Teams", r, g, b );

	if (gHUD.m_Teamplay == GAME_LMS)
		gHUD.DrawHudStringReverse( KILLS_RANGE_MAX + xpos_rel, ypos, 0, "Lives", r, g, b );
	else if (gHUD.m_Teamplay == GAME_GUNGAME)
		gHUD.DrawHudStringReverse( KILLS_RANGE_MAX + xpos_rel, ypos, 0, "Level", r, g, b );
	else if (gHUD.m_Teamplay == GAME_CTC || gHUD.m_Teamplay == GAME_PROPHUNT)
		gHUD.DrawHudStringReverse( KILLS_RANGE_MAX + xpos_rel, ypos, 0, "Points", r, g, b );
	else if (gHUD.m_Teamplay == GAME_COLDSKULL)
		gHUD.DrawHudStringReverse( KILLS_RANGE_MAX + xpos_rel, ypos, 0, "Skulls", r, g, b );
	else
		gHUD.DrawHudStringReverse( KILLS_RANGE_MAX + xpos_rel, ypos, 0, "K ills", r, g, b );
	gHUD.DrawHudString( DIVIDER_POS + xpos_rel, ypos, ScreenWidth, "/", r, g, b );
	gHUD.DrawHudString( DEATHS_RANGE_MIN + xpos_rel + 5, ypos, ScreenWidth, "Deaths", r, g, b );
	if (ScoreBased())
		gHUD.DrawHudString( SCORE_RANGE_MIN + xpos_rel + 5, ypos, ScreenWidth, "Score", r, g, b );
	gHUD.DrawHudString( PING_RANGE_MAX + xpos_rel - 35, ypos, ScreenWidth, "Latency", r, g, b );

	if ( can_show_packetloss )
	{
		gHUD.DrawHudString( PL_RANGE_MAX + xpos_rel - 35, ypos, ScreenWidth, "pkt loss", r, g, b );
	}

	FAR_RIGHT = can_show_packetloss ? PL_RANGE_MAX : PING_RANGE_MAX;
	FAR_RIGHT += 5;

	list_slot += 1.2;
	ypos = ROW_RANGE_MIN + margin + (list_slot * ROW_GAP);
	xpos = NAME_RANGE_MIN + xpos_rel;
	FillRGBA( xpos - 5, ypos, FAR_RIGHT, 1, r, g, b, 255);  // draw the seperator line
	
	list_slot += 0.8;

	if ( !ScoreBased() )
	{
		// it's not teamplay,  so just draw a simple player list
		DrawPlayers( xpos_rel, list_slot );
		return 1;
	}

	int i = 0;
	// clear out team scores
	for ( i = 1; i <= m_iNumTeams; i++ )
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
		for ( j = 1; j <= m_iNumTeams; j++ )
		{
			if ( !stricmp( g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name ) )
				break;
		}
		if ( j > m_iNumTeams )  // player is not in a team, skip to the next guy
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
	for ( i = 1; i <= m_iNumTeams; i++ )
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
		int which = ScoreBased() ? g_TeamInfo[i].score : g_TeamInfo[i].frags;

		for ( i = 1; i <= m_iNumTeams; i++ )
		{
			if ( g_TeamInfo[i].players < 0 )
				continue;

			if ( !g_TeamInfo[i].already_drawn && which >= highest )
			{
				if ( which > highest || g_TeamInfo[i].deaths < lowest_deaths )
				{
					best_team = i;
					lowest_deaths = g_TeamInfo[i].deaths;
					if (ScoreBased())
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

		ypos = ROW_RANGE_MIN + margin + (list_slot * ROW_GAP);

		// check we haven't drawn too far down
		if ( ypos > ROW_RANGE_MAX )  // don't draw to close to the lower border
			break;

		xpos = NAME_RANGE_MIN + xpos_rel;
		
		if ( team_info->ownteam ) // if it is their team, draw the background different color
		{
			// overlay the background in blue,  then draw the score text over it
			FillRGBA( NAME_RANGE_MIN + xpos_rel - 5, ypos, FAR_RIGHT, ROW_GAP, 0, 0, 255, 70 );
		}

		// draw their name (left to right)
		gHUD.DrawHudString( xpos, ypos, NAME_RANGE_MAX + xpos_rel, team_info->name, r, g, b );

		// draw kills (right to left)
		xpos = KILLS_RANGE_MAX + xpos_rel;
		gHUD.DrawHudNumberString( xpos, ypos, KILLS_RANGE_MIN + xpos_rel, team_info->frags, r, g, b );

		// draw divider
		xpos = DIVIDER_POS + xpos_rel;
		gHUD.DrawHudString( xpos, ypos, xpos + 20, "/", r, g, b );

		// draw deaths
		xpos = DEATHS_RANGE_MAX + xpos_rel;
		gHUD.DrawHudNumberString( xpos, ypos, DEATHS_RANGE_MIN + xpos_rel, team_info->deaths, r, g, b );

		// draw score
		xpos = SCORE_RANGE_MAX + xpos_rel;

		if (ScoreBased())
		{
			gHUD.DrawHudNumberString( xpos, ypos, SCORE_RANGE_MIN + xpos_rel, team_info->score, r, g, b );
		}

		// draw ping
		// draw ping & packetloss
		static char buf[64];
		sprintf( buf, "%d", team_info->ping );
		xpos = ((PING_RANGE_MAX - PING_RANGE_MIN) / 2) + PING_RANGE_MIN + xpos_rel + 25;
		UnpackRGB( r, g, b, HudColor() );
		gHUD.DrawHudStringReverse( xpos, ypos, xpos - 50, buf, r, g, b );

	//  Packetloss removed on Kelly 'shipping nazi' Bailey's orders
		if ( can_show_packetloss )
		{
			xpos = ((PL_RANGE_MAX - PL_RANGE_MIN) / 2) + PL_RANGE_MIN + xpos_rel + 25;
		
			sprintf( buf, "  %d", team_info->packetloss );
			gHUD.DrawHudString( xpos, ypos, xpos+50, buf, r, g, b );
		}

		team_info->already_drawn = TRUE;  // set the already_drawn to be TRUE, so this team won't get drawn again
		list_slot++;

		// draw all the players that belong to this team, indented slightly
		list_slot = DrawPlayers( xpos_rel, list_slot, 10, team_info->name );
	}

	// draw all the players who are not in a team
	list_slot += 0.5;
	DrawPlayers( xpos_rel, list_slot, 0, "" );

	return 1;
}

// returns the ypos where it finishes drawing
int CHudScoreboard :: DrawPlayers( int xpos_rel, float list_slot, int nameoffset, char *team )
{
	int can_show_packetloss = 0;
	int FAR_RIGHT;

	// Shift for spectators
	int margin = 0;
	if (g_iUser1)
		margin += 54;

	//  Packetloss removed on Kelly 'shipping nazi' Bailey's orders
	if ( cl_showpacketloss && cl_showpacketloss->value && ( ScreenWidth >= 400 ) )
	{
		can_show_packetloss = 1;
		SCOREBOARD_WIDTH = 560;
	}
	else
	{
		SCOREBOARD_WIDTH = 500;
	}

	FAR_RIGHT = can_show_packetloss ? PL_RANGE_MAX : PING_RANGE_MAX;
	FAR_RIGHT += 5;

	// draw the players, in order,  and restricted to team if set
	while ( 1 )
	{
		// Find the top ranking player
		int highest = -99999;	int lowest_deaths = 99999;
		int best_player = 0;

		for ( int i = 1; i < MAX_PLAYERS; i++ )
		{
			int which = ScoreBased() && team != NULL && strlen(team) ? g_PlayerExtraInfo[i].playerclass : g_PlayerExtraInfo[i].frags;

			if ( g_PlayerInfoList[i].name && (which >= highest) )
			{
				if ( !(team && stricmp(g_PlayerExtraInfo[i].teamname, team)) )  // make sure it is the specified team
				{
					extra_player_info_t *pl_info = &g_PlayerExtraInfo[i];
					if ( which > highest || pl_info->deaths < lowest_deaths )
					{
						best_player = i;
						lowest_deaths = pl_info->deaths;
						if (ScoreBased() && team != NULL && strlen(team))
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

		int ypos = ROW_RANGE_MIN + margin + (list_slot * ROW_GAP);

		// check we haven't drawn too far down
		if ( ypos > ROW_RANGE_MAX )  // don't draw to close to the lower border
			break;

		int xpos = NAME_RANGE_MIN + xpos_rel;
		int r = 255, g = 255, b = 255;
		if ( best_player == m_iLastKilledBy && m_fLastKillTime && m_fLastKillTime > gHUD.m_flTime )
		{
			if ( pl_info->thisplayer )
			{  // green is the suicide color? i wish this could do grey...
				FillRGBA( NAME_RANGE_MIN + xpos_rel - 5, ypos, FAR_RIGHT, ROW_GAP, 80, 155, 0, 70 );
			}
			else
			{  // Highlight the killers name - overlay the background in red,  then draw the score text over it
				FillRGBA( NAME_RANGE_MIN + xpos_rel - 5, ypos, FAR_RIGHT, ROW_GAP, 255, 0, 0, ((float)15 * (float)(m_fLastKillTime - gHUD.m_flTime)) );
			}
		}
		else if ( pl_info->thisplayer ) // if it is their name, draw it a different color
		{
			// overlay the background in blue,  then draw the score text over it
			FillRGBA( NAME_RANGE_MIN + xpos_rel - 5, ypos, FAR_RIGHT, ROW_GAP, 0, 0, 255, 70 );
		}

		// draw their name (left to right)
		gHUD.DrawHudString( xpos + nameoffset, ypos, NAME_RANGE_MAX + xpos_rel, pl_info->name, r, g, b );

		// draw kills (right to left)
		xpos = KILLS_RANGE_MAX + xpos_rel;
		gHUD.DrawHudNumberString( xpos, ypos, KILLS_RANGE_MIN + xpos_rel, g_PlayerExtraInfo[best_player].frags, r, g, b );

		// draw divider
		xpos = DIVIDER_POS + xpos_rel;
		gHUD.DrawHudString( xpos, ypos, xpos + 20, "/", r, g, b );

		// draw deaths
		xpos = DEATHS_RANGE_MAX + xpos_rel;
		gHUD.DrawHudNumberString( xpos, ypos, DEATHS_RANGE_MIN + xpos_rel, g_PlayerExtraInfo[best_player].deaths, r, g, b );

		// draw score
		if (ScoreBased())
		{
			xpos = SCORE_RANGE_MAX + xpos_rel;
			gHUD.DrawHudNumberString( xpos, ypos, SCORE_RANGE_MIN + xpos_rel, g_PlayerExtraInfo[best_player].playerclass, r, g, b );
		}

		// draw ping & packetloss
		static char buf[64];
		if (!g_PlayerInfoList[best_player].ping && !g_PlayerInfoList[best_player].thisplayer)
			strcpy( buf, "bot" );
		else
			sprintf( buf, "%d", g_PlayerInfoList[best_player].ping );
		xpos = ((PING_RANGE_MAX - PING_RANGE_MIN) / 2) + PING_RANGE_MIN + xpos_rel + 25;
		gHUD.DrawHudStringReverse( xpos, ypos, xpos - 50, buf, r, g, b );

		//  Packetloss removed on Kelly 'shipping nazi' Bailey's orders
		if ( can_show_packetloss )
		{
			if ( g_PlayerInfoList[best_player].packetloss >= 63 )
			{
				UnpackRGB( r, g, b, RGB_REDISH );
				sprintf( buf, " !!!!" );
			}
			else
			{
				sprintf( buf, "  %d", g_PlayerInfoList[best_player].packetloss );
			}

			xpos = ((PL_RANGE_MAX - PL_RANGE_MIN) / 2) + PL_RANGE_MIN + xpos_rel + 25;
		
			gHUD.DrawHudString( xpos, ypos, xpos+50, buf, r, g, b );
		}
	
		pl_info->name = NULL;  // set the name to be NULL, so this client won't get drawn again
		list_slot++;
	}

	return list_slot;
}

void CHudScoreboard :: GetAllPlayersInfo( void )
{
	for ( int i = 1; i < MAX_PLAYERS; i++ )
	{
		gEngfuncs.pfnGetPlayerInfo( i, &g_PlayerInfoList[i] );

		if ( g_PlayerInfoList[i].thisplayer )
			m_iPlayerNum = i;  // !!!HACK: this should be initialized elsewhere... maybe gotten from the engine
	}
}

int CHudScoreboard :: MsgFunc_ScoreInfo2( const char *pszName, int iSize, void *pbuf )
{
	m_iFlags |= HUD_ACTIVE;

	BEGIN_READ( pbuf, iSize );
	short cl = READ_BYTE();
	short frags = READ_SHORT();
	short deaths = READ_SHORT();
	short playerclass = READ_SHORT();
	short teamnumber = READ_SHORT();

	if ( cl > 0 && cl <= MAX_PLAYERS )
	{
		g_PlayerExtraInfo[cl].frags = frags;
		g_PlayerExtraInfo[cl].deaths = deaths;
		g_PlayerExtraInfo[cl].playerclass = playerclass;
		g_PlayerExtraInfo[cl].teamnumber = teamnumber;

		gViewPort->UpdateOnPlayerInfo();
	}

	return 1;
}

// Message handler for TeamInfo message
// accepts two values:
//		byte: client number
//		string: client team name
int CHudScoreboard :: MsgFunc_TeamInfo2( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	short cl = READ_BYTE();
	
	if ( cl > 0 && cl <= MAX_PLAYERS )
	{  // set the players team
		strncpy( g_PlayerExtraInfo[cl].teamname, READ_STRING(), MAX_TEAM_NAME );
	}

	// rebuild the list of teams

	int i = 0;
	// clear out player counts from teams
	for ( i = 1; i <= m_iNumTeams; i++ )
	{
		g_TeamInfo[i].players = 0;
	}

	// rebuild the team list
	GetAllPlayersInfo();
	m_iNumTeams = 0;
	for ( i = 1; i < MAX_PLAYERS; i++ )
	{
		if ( g_PlayerInfoList[i].name == NULL )
			continue;

		if ( g_PlayerExtraInfo[i].teamname[0] == 0 )
			continue; // skip over players who are not in a team

		int j = 0;
		// is this player in an existing team?
		for ( j = 1; j <= m_iNumTeams; j++ )
		{
			if ( g_TeamInfo[j].name[0] == '\0' )
				break;

			if ( !stricmp( g_PlayerExtraInfo[i].teamname, g_TeamInfo[j].name ) )
				break;
		}

		if ( j > m_iNumTeams )
		{ // they aren't in a listed team, so make a new one
			// search through for an empty team slot
			for ( int j = 1; j <= m_iNumTeams; j++ )
			{
				if ( g_TeamInfo[j].name[0] == '\0' )
					break;
			}
			m_iNumTeams = fmax( j, m_iNumTeams );

			strncpy( g_TeamInfo[j].name, g_PlayerExtraInfo[i].teamname, MAX_TEAM_NAME );
			g_TeamInfo[j].players = 0;
		}

		g_TeamInfo[j].players++;
	}

	// clear out any empty teams
	for ( i = 1; i <= m_iNumTeams; i++ )
	{
		if ( g_TeamInfo[i].players < 1 )
			memset( &g_TeamInfo[i], 0, sizeof(team_info_t) );
	}

	return 1;
}

// Message handler for TeamScore message
// accepts three values:
//		string: team name
//		short: teams kills
//		short: teams deaths 
// if this message is never received, then scores will simply be the combined totals of the players.
int CHudScoreboard :: MsgFunc_TeamScore2( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	char *TeamName = READ_STRING();

	int i = 0;
	// find the team matching the name
	for ( i = 1; i <= m_iNumTeams; i++ )
	{
		if ( !stricmp( TeamName, g_TeamInfo[i].name ) )
			break;
	}
	if ( i > m_iNumTeams )
		return 1;

	// use this new score data instead of combined player scores
	g_TeamInfo[i].scores_overriden = TRUE;
	g_TeamInfo[i].frags = READ_SHORT();
	g_TeamInfo[i].deaths = READ_SHORT();
	
	return 1;
}

void CHudScoreboard :: DeathMsg( int killer, int victim )
{
	// if we were the one killed,  or the world killed us, set the scoreboard to indicate suicide
	if ( victim == m_iPlayerNum || killer == 0 )
	{
		m_iLastKilledBy = killer ? killer : m_iPlayerNum;
		m_fLastKillTime = gHUD.m_flTime + 10;	// display who we were killed by for 10 seconds

		if ( killer == m_iPlayerNum )
			m_iLastKilledBy = m_iPlayerNum;
	}
}



void CHudScoreboard :: UserCmd_ShowScores( void )
{
	m_iShowscoresHeld = TRUE;
	m_iFlags |= HUD_ACTIVE;
}

void CHudScoreboard :: UserCmd_HideScores( void )
{
	m_iShowscoresHeld = FALSE;
	m_iFlags &= ~HUD_ACTIVE;
}
