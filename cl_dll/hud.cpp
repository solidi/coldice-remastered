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
// hud.cpp
//
// implementation of CHud class
//

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"
#include "hud_servers.h"
#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "fog.h"

#include "demo.h"
#include "demo_api.h"
#include "vgui_ScorePanel.h"

#include "screenfade.h"
#include "shake.h"

#include "rain.h"
#include "colorcor.h"

hud_player_info_t	 g_PlayerInfoList[MAX_PLAYERS+1];	   // player info from the engine
extra_player_info_t  g_PlayerExtraInfo[MAX_PLAYERS+1];   // additional player info sent directly to the client dll

cvar_t *cl_oldmotd;
cvar_t *cl_oldscoreboard;
cvar_t *m_pCvarRighthand;
cvar_t *cl_bulletsmoke;
cvar_t *cl_gunsmoke;
cvar_t *cl_weaponsway;
cvar_t *cl_weaponfidget;
cvar_t *cl_weaponretract;
cvar_t *cl_playpoint;
cvar_t *cl_glasshud;
cvar_t *cl_announcehumor;
cvar_t *cl_showtips;
cvar_t *cl_flashonpickup;
cvar_t *cl_icemodels;
cvar_t *cl_lifemeter;
cvar_t *cl_achievements;
cvar_t *cl_antivomit;
cvar_t *cl_keyboardacrobatics;
cvar_t *cl_weather;
cvar_t *cl_hudscale;
cvar_t *cl_hudbend;
cvar_t *cl_wallclimbindicator;
cvar_t *cl_particlesystem;
cvar_t *cl_radar;
cvar_t *cl_portalmirror;
cvar_t *cl_screeneffects;
cvar_t *cl_customtempents;
cvar_t *cl_voiceoverpath;
cvar_t *cl_objectives;
cvar_t *cl_crosshairammo;

cvar_t *cl_vmx;
cvar_t *cl_vmy;
cvar_t *cl_vmz;
cvar_t *cl_vmf;
cvar_t *cl_vmu;
cvar_t *cl_vmr;
cvar_t *cl_vmpitch;
cvar_t *cl_vmroll;
cvar_t *cl_vmyaw;
cvar_t *cl_ifov;

class CHLVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	virtual void GetPlayerTextColor(int entindex, int color[3])
	{
		color[0] = color[1] = color[2] = 255;

		if( entindex >= 0 && entindex < sizeof(g_PlayerExtraInfo)/sizeof(g_PlayerExtraInfo[0]) )
		{
			int iTeam = g_PlayerExtraInfo[entindex].teamnumber;

			if ( iTeam < 0 )
			{
				iTeam = 0;
			}

			iTeam = iTeam % iNumberOfTeamColors;

			color[0] = iTeamColors[iTeam][0];
			color[1] = iTeamColors[iTeam][1];
			color[2] = iTeamColors[iTeam][2];
		}
	}

	virtual void UpdateCursorState()
	{
		gViewPort->UpdateCursorState();
	}

	virtual int	GetAckIconHeight()
	{
		return ScreenHeight - gHUD.m_iFontHeight*3 - 6;
	}

	virtual bool			CanShowSpeakerLabels()
	{
		if( gViewPort && gViewPort->m_pScoreBoard )
			return !gViewPort->m_pScoreBoard->isVisible();
		else
			return false;
	}
};
static CHLVoiceStatusHelper g_VoiceStatusHelper;


extern client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

extern cvar_t *sensitivity;
cvar_t *cl_lw = NULL;
cvar_t *cl_rollangle;
cvar_t *cl_rollspeed;
cvar_t *cl_viewroll;
cvar_t *cl_bobtilt;

void ShutdownInput (void);

//DECLARE_MESSAGE(m_Logo, Logo)
int __MsgFunc_Logo(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Logo(pszName, iSize, pbuf );
}

//DECLARE_MESSAGE(m_Logo, Logo)
int __MsgFunc_ResetHUD(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_ResetHUD(pszName, iSize, pbuf );
}

int __MsgFunc_InitHUD(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_InitHUD( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_ViewMode(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_ViewMode( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_SetFOV(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_SetFOV( pszName, iSize, pbuf );
}

int __MsgFunc_Concuss(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Concuss( pszName, iSize, pbuf );
}

int __MsgFunc_GameMode(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_GameMode( pszName, iSize, pbuf );
}

int __MsgFunc_Portal(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	//gPortalRenderer.portal1index = READ_BYTE();
	//gPortalRenderer.portal2index = READ_BYTE();

	Vector portal1org;
	for (int i = 0; i < 3; i++)
		portal1org[i] = READ_COORD();

	Vector portal1ang;
	for (int i = 0; i < 3; i++)
		portal1ang[i] = READ_COORD();

	Vector portal2org;
	for (int i = 0; i < 3; i++)
		portal2org[i] = READ_COORD();

	Vector portal2ang;
	for (int i = 0; i < 3; i++)
		portal2ang[i] = READ_COORD();

#ifndef __APPLE__
	gPortalRenderer.m_Portal1[0] = portal1org;
	gPortalRenderer.m_Portal1[1] = portal1ang;

	gPortalRenderer.m_Portal2[0] = portal2org;
	gPortalRenderer.m_Portal2[1] = portal2ang;

	// set rendering status
	gPortalRenderer.m_bIsDrawingPortal = portal1org != vec3_origin && portal2org != vec3_origin;
#endif

	return 1;
}

// TFFree Command Menu
void __CmdFunc_OpenCommandMenu(void)
{
	if ( gViewPort )
	{
		gViewPort->ShowCommandMenu( gViewPort->m_StandardMenu );
	}
}

// TFC "special" command
void __CmdFunc_InputPlayerSpecial(void)
{
	if ( gViewPort )
	{
		gViewPort->InputPlayerSpecial();
	}
}

void __CmdFunc_CloseCommandMenu(void)
{
	if ( gViewPort )
	{
		gViewPort->InputSignalHideCommandMenu();
	}
}

void __CmdFunc_ForceCloseCommandMenu( void )
{
	if ( gViewPort )
	{
		gViewPort->HideCommandMenu();
	}
}

void __CmdFunc_ToggleServerBrowser( void )
{
	if ( gViewPort )
	{
		gViewPort->ToggleServerBrowser();
	}
}

void __CmdFunc_ImguiChapter( void )
{
	if (gEngfuncs.GetMaxClients() > 1) {
		gEngfuncs.Con_Printf("Must be in single player mode!\n");
		return;
	}
	EngineClientCmd("map c0a0.bsp");
}

// TFFree Command Menu Message Handlers
int __MsgFunc_ValClass(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ValClass( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamNames(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamNames( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_Feign(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Feign( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_Detpack(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Detpack( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_VGUIMenu(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_VGUIMenu( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_MOTD(const char *pszName, int iSize, void *pbuf)
{
	if (cl_oldmotd && cl_oldmotd->value) {
		return gHUD.m_MOTD.MsgFunc_MOTD( pszName, iSize, pbuf );
	} else if (gViewPort) {
		return gViewPort->MsgFunc_MOTD( pszName, iSize, pbuf );
	}
	return 0;
}

int __MsgFunc_VoteFor(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_VoteFor( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_VoteGame(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_VoteGame( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_VoteMap(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_VoteMap( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_VoteMutator(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_VoteMutator( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_BuildSt(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_BuildSt( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_RandomPC(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_RandomPC( pszName, iSize, pbuf );
	return 0;
}
 
int __MsgFunc_ServerName(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ServerName( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_ScoreInfo(const char *pszName, int iSize, void *pbuf)
{
	gHUD.m_Scoreboard.MsgFunc_ScoreInfo2( pszName, iSize, pbuf );
	if (gViewPort) {
		return gViewPort->MsgFunc_ScoreInfo( pszName, iSize, pbuf );
	}
	return 0;
}

int __MsgFunc_TeamScore(const char *pszName, int iSize, void *pbuf)
{
	gHUD.m_Scoreboard.MsgFunc_TeamScore2( pszName, iSize, pbuf );
	if (gViewPort) {
		return gViewPort->MsgFunc_TeamScore( pszName, iSize, pbuf );
	}
	return 0;
}

int __MsgFunc_TeamInfo(const char *pszName, int iSize, void *pbuf)
{
	gHUD.m_Scoreboard.MsgFunc_TeamInfo2( pszName, iSize, pbuf );
	if (gViewPort) {
		return gViewPort->MsgFunc_TeamInfo( pszName, iSize, pbuf );
	}
	return 0;
}

int __MsgFunc_Spectator(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Spectator( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_SpecFade(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_SpecFade( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_ResetFade(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ResetFade( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_AllowSpec(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_AllowSpec( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_Acrobatics(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Acrobatics(pszName, iSize, pbuf );
}

int __MsgFunc_PlayCSound(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_PlayCSound(pszName, iSize, pbuf );
}

int __MsgFunc_AddMut(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_AddMut(pszName, iSize, pbuf );
}

int __MsgFunc_Chaos(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Chaos(pszName, iSize, pbuf );
}

int __MsgFunc_Particle(const char *pszName,  int iSize, void *pbuf )
{
	gHUD.MsgFunc_Particle( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_DelPart(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_DelPart( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_FlameMsg(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_FlameMsg( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_FlameKill(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_FlameKill( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_MParticle(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_MParticle( pszName, iSize, pbuf );
	return 1;
}

// This is called every time the DLL is loaded
void CHud :: Init( void )
{
	HOOK_MESSAGE( Logo );
	HOOK_MESSAGE( ResetHUD );
	HOOK_MESSAGE( GameMode );
	HOOK_MESSAGE( InitHUD );
	HOOK_MESSAGE( ViewMode );
	HOOK_MESSAGE( SetFOV );
	HOOK_MESSAGE( Concuss );

	// TFFree CommandMenu
	HOOK_COMMAND( "+commandmenu", OpenCommandMenu );
	HOOK_COMMAND( "-commandmenu", CloseCommandMenu );
	HOOK_COMMAND( "ForceCloseCommandMenu", ForceCloseCommandMenu );
	HOOK_COMMAND( "special", InputPlayerSpecial );
	HOOK_COMMAND( "togglebrowser", ToggleServerBrowser );

#ifndef _WIN32
	HOOK_COMMAND( "imgui_chapter", ImguiChapter );
#endif

	HOOK_MESSAGE( ValClass );
	HOOK_MESSAGE( TeamNames );
	HOOK_MESSAGE( Feign );
	HOOK_MESSAGE( Detpack );
	HOOK_MESSAGE( MOTD );
	HOOK_MESSAGE( BuildSt );
	HOOK_MESSAGE( RandomPC );
	HOOK_MESSAGE( ServerName );
	HOOK_MESSAGE( ScoreInfo );
	HOOK_MESSAGE( TeamScore );
	HOOK_MESSAGE( TeamInfo );

	HOOK_MESSAGE( Spectator );
	HOOK_MESSAGE( AllowSpec );
	
	HOOK_MESSAGE( SpecFade );
	HOOK_MESSAGE( ResetFade );

	// VGUI Menus
	HOOK_MESSAGE( VGUIMenu );

	// Cold Ice Remastered
	HOOK_MESSAGE( Acrobatics );
	HOOK_MESSAGE( PlayCSound );
	HOOK_MESSAGE( Particle );
	HOOK_MESSAGE( DelPart );
	HOOK_MESSAGE( FlameMsg );
	HOOK_MESSAGE( FlameKill );
	HOOK_MESSAGE( MParticle );
	HOOK_MESSAGE( Portal );
	HOOK_MESSAGE( VoteFor );
	HOOK_MESSAGE( VoteGame );
	HOOK_MESSAGE( VoteMap );
	HOOK_MESSAGE( VoteMutator );
	HOOK_MESSAGE( AddMut );
	HOOK_MESSAGE( Chaos );

	CVAR_CREATE( "hud_classautokill", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );		// controls whether or not to suicide immediately on TF class switch
	CVAR_CREATE( "hud_takesshots", "0", FCVAR_ARCHIVE );		// controls whether or not to automatically take screenshots at the end of a round
	cl_rollangle = CVAR_CREATE( "cl_rollangle", "0.65", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	cl_rollspeed = CVAR_CREATE( "cl_rollspeed", "300", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	cl_viewroll = CVAR_CREATE( "cl_viewroll", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
	cl_bobtilt = CVAR_CREATE( "cl_bobtilt", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );

	cl_oldmotd = CVAR_CREATE( "cl_oldmotd", "1", FCVAR_ARCHIVE );
	cl_oldscoreboard = CVAR_CREATE( "cl_oldscoreboard", "1", FCVAR_ARCHIVE );
	m_pCvarRighthand = CVAR_CREATE( "cl_righthand", "1", FCVAR_ARCHIVE );
	cl_bulletsmoke = CVAR_CREATE( "cl_bulletsmoke", "1", FCVAR_ARCHIVE );
	cl_gunsmoke = CVAR_CREATE( "cl_gunsmoke", "1", FCVAR_ARCHIVE );
	cl_weaponsway = CVAR_CREATE( "cl_weaponsway", "1", FCVAR_ARCHIVE );
	cl_weaponfidget = CVAR_CREATE( "cl_weaponfidget", "1", FCVAR_ARCHIVE );
	cl_weaponretract = CVAR_CREATE( "cl_weaponretract", "1", FCVAR_ARCHIVE );
	cl_playpoint = CVAR_CREATE( "cl_playpoint", "1", FCVAR_ARCHIVE );
	cl_glasshud = CVAR_CREATE( "cl_glasshud", "1", FCVAR_ARCHIVE );
	cl_announcehumor = CVAR_CREATE( "cl_announcehumor", "1", FCVAR_ARCHIVE );
	cl_showtips = CVAR_CREATE( "cl_showtips", "1", FCVAR_ARCHIVE );
	cl_flashonpickup = CVAR_CREATE( "cl_flashonpickup", "1", FCVAR_ARCHIVE );
	CVAR_CREATE("cl_shadows", "0", FCVAR_ARCHIVE);
	CVAR_CREATE( "cl_autowepswitch", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	CVAR_CREATE( "cl_infomessage", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	CVAR_CREATE( "cl_autowepthrow", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	cl_lifemeter = CVAR_CREATE( "cl_lifemeters", "1", FCVAR_ARCHIVE );
	cl_achievements = CVAR_CREATE( "cl_achievements", "3", FCVAR_ARCHIVE );
	cl_antivomit = CVAR_CREATE( "cl_antivomit", "0", FCVAR_ARCHIVE );
	cl_keyboardacrobatics = CVAR_CREATE( "cl_keyboardacrobatics", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	cl_weather = CVAR_CREATE( "cl_weather", "3", FCVAR_ARCHIVE | FCVAR_USERINFO );
	cl_hudscale = CVAR_CREATE( "cl_hudscale", "0", FCVAR_ARCHIVE );
	cl_hudbend = CVAR_CREATE( "cl_hudbend", "0.0", FCVAR_ARCHIVE );
	cl_wallclimbindicator = CVAR_CREATE( "cl_wallclimbindicator", "1", FCVAR_ARCHIVE );
	cl_particlesystem = CVAR_CREATE( "cl_particlesystem", "1", FCVAR_ARCHIVE );
	cl_radar = CVAR_CREATE( "cl_radar", "1", FCVAR_ARCHIVE );
	cl_portalmirror = CVAR_CREATE("cl_portalmirror", "0", FCVAR_ARCHIVE);
	cl_screeneffects = CVAR_CREATE("cl_screeneffects", "0", FCVAR_ARCHIVE);
	cl_customtempents = CVAR_CREATE("cl_customtempents", "1", FCVAR_ARCHIVE);
	cl_voiceoverpath = CVAR_CREATE("cl_voiceoverpath", "", FCVAR_ARCHIVE);
	cl_objectives = CVAR_CREATE("cl_objectives", "1", FCVAR_ARCHIVE);
	CVAR_CREATE( "cl_automelee", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	CVAR_CREATE( "cl_autotaunt", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	CVAR_CREATE( "cl_playmusic", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	cl_crosshairammo = CVAR_CREATE("cl_crosshairammo", "1", FCVAR_ARCHIVE);

	cl_vmx = CVAR_CREATE( "cl_vmx", "0", FCVAR_ARCHIVE );
	cl_vmy = CVAR_CREATE( "cl_vmy", "0", FCVAR_ARCHIVE );
	cl_vmz = CVAR_CREATE( "cl_vmz", "0", FCVAR_ARCHIVE );
	cl_vmf = CVAR_CREATE( "cl_vmf", "0", FCVAR_ARCHIVE );
	cl_vmu = CVAR_CREATE( "cl_vmu", "0", FCVAR_ARCHIVE );
	cl_vmr = CVAR_CREATE( "cl_vmr", "0", FCVAR_ARCHIVE );
	cl_vmpitch = CVAR_CREATE( "cl_vmpitch", "0", FCVAR_ARCHIVE );
	cl_vmyaw = CVAR_CREATE( "cl_vmyaw", "0", FCVAR_ARCHIVE );
	cl_vmroll = CVAR_CREATE( "cl_vmroll", "0", FCVAR_ARCHIVE );
	cl_ifov = CVAR_CREATE( "cl_ifov", "90", FCVAR_ARCHIVE );

	// Experimental color correction system
	CVAR_CREATE( "colorcor_r", "1", FCVAR_ARCHIVE ); // Red color correction
	CVAR_CREATE( "colorcor_g", "1", FCVAR_ARCHIVE ); // Green color correction
	CVAR_CREATE( "colorcor_b", "1", FCVAR_ARCHIVE ); // Blue color correction
	CVAR_CREATE( "colorcor_alpha", "1.0", FCVAR_ARCHIVE ); // Alpha chanel color correction
	CVAR_CREATE( "colorcor_inverse", "0", FCVAR_ARCHIVE ); // Inverse color correction
	CVAR_CREATE( "colorcor_blackwhite", "0", FCVAR_ARCHIVE ); // Black and white

	m_iLogo = 0;
	m_iFOV = 0;

	CVAR_CREATE( "zoom_sensitivity_ratio", "1.2", 0 );
	default_fov = CVAR_CREATE( "default_fov", "90", 0 );
	m_pCvarStealMouse = CVAR_CREATE( "hud_capturemouse", "1", FCVAR_ARCHIVE );
	m_pCvarDraw = CVAR_CREATE( "hud_draw", "1", FCVAR_ARCHIVE );
	cl_lw = gEngfuncs.pfnGetCvarPointer( "cl_lw" );

	m_pSpriteList = NULL;

	m_iShownHelpMessage = gEngfuncs.pfnRandomFloat( 15, 30 );

	// Clear any old HUD list
	if ( m_pHudList )
	{
		HUDLIST *pList;
		while ( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = NULL;
	}

	// In case we get messages before the first update -- time will be valid
	m_flTime = 1.0;

	m_Ammo.Init();
	m_Health.Init();
	m_SayText.Init();
	m_Spectator.Init();
	m_Geiger.Init();
	m_Train.Init();
	m_Battery.Init();
	m_Flash.Init();
	m_Message.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_TextMessage.Init();
	m_StatusIcons.Init();
	m_MOTD.Init();
	m_Scoreboard.Init();
	GetClientVoiceMgr()->Init(&g_VoiceStatusHelper, (vgui::Panel**)&gViewPort);
	GetLifeBar()->Init();
	m_WallClimb.Init();
	m_NukeCrosshair.Init();
	m_Radar.Init();
	m_Objective.Init();
	m_Timer.Init();
	m_ProTip.Init();
	m_CtfInfo.Init();
	gFog.Init();
	InitRain();
#ifdef _WIN32
	g_ImGUIManager.Init();
#endif
#ifndef __APPLE__
	gPortalRenderer.Init();
#endif
	m_Particle.Init();

	m_Menu.Init();
	
	ServersInit();

	MsgFunc_ResetHUD(0, 0, NULL );
}

// CHud destructor
// cleans up memory allocated for m_rg* arrays
CHud :: ~CHud()
{
	delete [] m_rghSprites;
	delete [] m_rgrcRects;
	delete [] m_rgszSpriteNames;

	if ( m_pHudList )
	{
		HUDLIST *pList;
		while ( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = NULL;
	}

	ServersShutdown();
}

// GetSpriteIndex()
// searches through the sprite list loaded from hud.txt for a name matching SpriteName
// returns an index into the gHUD.m_rghSprites[] array
// returns 0 if sprite not found
int CHud :: GetSpriteIndex( const char *SpriteName )
{
	// look through the loaded sprite name list for SpriteName
	for ( int i = 0; i < m_iSpriteCount; i++ )
	{
		if ( strncmp( SpriteName, m_rgszSpriteNames + (i * MAX_SPRITE_NAME_LENGTH), MAX_SPRITE_NAME_LENGTH ) == 0 )
			return i;
	}

	return -1; // invalid sprite
}

void CHud :: VidInit( void )
{
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);

	if (cl_customtempents->value)
	{
		TempEntity_Initialize();
		CL_TempEntInit();
	}

	// ----------
	// Load Sprites
	// ---------
//	m_hsprFont = LoadSprite("sprites/%d_font.spr");
	
	m_hsprLogo = 0;	
	m_hsprCursor = 0;

	if (ScreenWidth < 640)
		m_iRes = 320;
	else
		m_iRes = 640;

	// Only load this once
	if ( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList("sprites/hud.txt", &m_iSpriteCountAllRes);

		if (m_pSpriteList)
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			client_sprite_t *p = m_pSpriteList;
			int j;
			for ( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == m_iRes )
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
 			m_rghSprites = new HSPRITE[m_iSpriteCount];
			m_rgrcRects = new wrect_t[m_iSpriteCount];
			m_rgszSpriteNames = new char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];

			p = m_pSpriteList;
			int index = 0;
			for ( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == m_iRes )
				{
					char sz[256];
					sprintf(sz, "sprites/%s.spr", p->szSprite);
					m_rghSprites[index] = SPR_Load(sz);
					m_rgrcRects[index] = p->rc;
					strncpy( &m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH );

					index++;
				}

				p++;
			}
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t *p = m_pSpriteList;
		int index = 0;
		for ( int j = 0; j < m_iSpriteCountAllRes; j++ )
		{
			if ( p->iRes == m_iRes )
			{
				char sz[256];
				sprintf( sz, "sprites/%s.spr", p->szSprite );
				m_rghSprites[index] = SPR_Load(sz);
				index++;
			}

			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );

	m_iFontHeight = m_rgrcRects[m_HUD_number_0].bottom - m_rgrcRects[m_HUD_number_0].top;

	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Spectator.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Flash.VidInit();
	m_Message.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_TextMessage.VidInit();
	m_StatusIcons.VidInit();
	m_MOTD.VidInit();
	m_Scoreboard.VidInit();
	GetClientVoiceMgr()->VidInit();
	GetLifeBar()->VidInit();
	m_WallClimb.VidInit();
	m_NukeCrosshair.VidInit();
	m_Radar.VidInit();
	m_Objective.VidInit();
	m_Timer.VidInit();
	m_ProTip.VidInit();
	m_CtfInfo.VidInit();
	gFog.VidInit();
#ifdef _WIN32
	g_ImGUIManager.VidInit();
#endif
	m_Particle.VidInit();
}

int CHud::MsgFunc_Logo(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	// update Train data
	m_iLogo = READ_BYTE();

	return 1;
}

float g_lastFOV = 0.0;

/*
============
COM_FileBase
============
*/
// Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
void COM_FileBase ( const char *in, char *out)
{
	int len, start, end;

	len = strlen( in );
	
	// scan backward for '.'
	end = len - 1;
	while ( end && in[end] != '.' && in[end] != '/' && in[end] != '\\' )
		end--;
	
	if ( in[end] != '.' )		// no '.', copy to end
		end = len-1;
	else 
		end--;					// Found ',', copy to left of '.'


	// Scan backward for '/'
	start = len-1;
	while ( start >= 0 && in[start] != '/' && in[start] != '\\' )
		start--;

	if ( in[start] != '/' && in[start] != '\\' )
		start = 0;
	else 
		start++;

	// Length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy( out, &in[start], len );
	// Terminate it
	out[len] = 0;
}

/*
=================
HUD_IsGame

=================
*/
int HUD_IsGame( const char *game )
{
	const char *gamedir;
	char gd[ 1024 ];

	gamedir = gEngfuncs.pfnGetGameDirectory();
	if ( gamedir && gamedir[0] )
	{
		COM_FileBase( gamedir, gd );
		if ( !stricmp( gd, game ) )
			return 1;
	}
	return 0;
}

/*
=====================
HUD_GetFOV

Returns last FOV
=====================
*/
float HUD_GetFOV( void )
{
	if ( gEngfuncs.pDemoAPI->IsRecording() )
	{
		// Write it
		int i = 0;
		unsigned char buf[ 100 ];

		// Active
		*( float * )&buf[ i ] = g_lastFOV;
		i += sizeof( float );

		Demo_WriteBuffer( TYPE_ZOOM, i, buf );
	}

	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
	{
		g_lastFOV = g_demozoom;
	}
	return g_lastFOV;
}

int CHud::MsgFunc_SetFOV(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	int newfov = READ_BYTE();
	int def_fov = CVAR_GET_FLOAT( "default_fov" );

	//Weapon prediction already takes care of changing the fog. ( g_lastFOV ).
	if ( cl_lw && cl_lw->value )
		return 1;

	g_lastFOV = newfov;

	if ( newfov == 0 )
	{
		m_iFOV = def_fov;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == def_fov )
	{  
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{  
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = sensitivity->value * ((float)newfov / (float)def_fov) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	return 1;
}


void CHud::AddHudElem(CHudBase *phudelem)
{
	HUDLIST *pdl, *ptemp;

//phudelem->Think();

	if (!phudelem)
		return;

	pdl = (HUDLIST *)malloc(sizeof(HUDLIST));
	if (!pdl)
		return;

	memset(pdl, 0, sizeof(HUDLIST));
	pdl->p = phudelem;

	if (!m_pHudList)
	{
		m_pHudList = pdl;
		return;
	}

	ptemp = m_pHudList;

	while (ptemp->pNext)
		ptemp = ptemp->pNext;

	ptemp->pNext = pdl;
}

float CHud::GetSensitivity( void )
{
	return m_flMouseSensitivity;
}

void CHud::FlashHud( void ) {
	if (cl_flashonpickup && !cl_flashonpickup->value)
		return;

	screenfade_t sf;
	gEngfuncs.pfnGetScreenFade( &sf );
	sf.fader = 255;
	sf.fadeg = 255;
	sf.fadeb = 255;
	sf.fadealpha = 255;
	sf.fadeFlags = FFADE_IN;

	sf.fadeEnd = gEngfuncs.GetClientTime();
	sf.fadeReset = 0.0;
	sf.fadeSpeed = 0.0;

	sf.fadeSpeed = sf.fadealpha;
	sf.fadeReset += 0.25;
	sf.fadeEnd += sf.fadeReset;

	gEngfuncs.pfnSetScreenFade( &sf );
}

void ProTip(int id, const char *message)
{
	gHUD.m_ProTip.AddMessage(id, message);
}

bool MutatorEnabled(int mutatorId)
{
	mutators_t *m = gHUD.m_Mutators;
	while (m != NULL) {
		if (m->mutatorId == mutatorId && m->timeToLive > gHUD.m_flTime)
			return true;
		m = m->next;
	}

	return false;
}

mutators_t GetMutator(int mutatorId)
{
	mutators_t *m = gHUD.m_Mutators;
	while (m != NULL) {
		if (m->mutatorId == mutatorId && m->timeToLive > gHUD.m_flTime)
			return *m;
		m = m->next;
	}

	return mutators_t();
}

bool IsShidden( void )
{
	return gHUD.m_GameMode == GAME_SHIDDEN;
}

bool IsPropHunt( void )
{
	return gHUD.m_GameMode == GAME_PROPHUNT;
}
