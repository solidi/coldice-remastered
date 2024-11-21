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
//  hud_msg.cpp
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "r_efx.h"
#include "view.h"
#include "in_defs.h"
#include "particlesys.h"
#include "particlemgr.h"
#include "FlameSystem.h"

extern "C"
{
int CL_IsThirdPerson( void );
}

#include "particleman.h"
extern IParticleMan *g_pParticleMan;

#define MAX_CLIENTS 32

#if !defined( _TFC )
extern BEAM *pBeam;
extern BEAM *pBeam2;
#endif 

#if defined( _TFC )
void ClearEventList( void );
#endif

float g_SlideTime = 0;
float g_WallClimb = 0;
float g_AcrobatTime = 0;
float g_DeploySoundTime = 0;
float g_RetractDistance = 0;
float g_NotifyTime = 0;
float g_ScoreTime = 0;
extern cvar_t *cl_antivomit;
extern qboolean g_IronSight;

/// USER-DEFINED SERVER MESSAGE HANDLERS

int CHud :: MsgFunc_ResetHUD(const char *pszName, int iSize, void *pbuf )
{
	ASSERT( iSize == 0 );

	// clear all hud data
	HUDLIST *pList = m_pHudList;

	while ( pList )
	{
		if ( pList->p )
			pList->p->Reset();
		pList = pList->pNext;
	}

	// reset sensitivity
	m_flMouseSensitivity = 0;

	// reset concussion effect
	m_iConcussionEffect = 0;

	return 1;
}

void CAM_ToFirstPerson(void);

void CHud :: MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf )
{
	CAM_ToFirstPerson();
}

void CHud :: MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf )
{
	// prepare all hud data
	HUDLIST *pList = m_pHudList;

	while (pList)
	{
		if ( pList->p )
			pList->p->InitHUDData();
		pList = pList->pNext;
	}

#if defined( _TFC )
	ClearEventList();

	// catch up on any building events that are going on
	gEngfuncs.pfnServerCmd("sendevents");
#endif

	if ( g_pParticleMan )
		 g_pParticleMan->ResetParticles();

#if !defined( _TFC )
	//Probably not a good place to put this.
	pBeam = pBeam2 = NULL;
#endif

	g_WallClimb = 0;
	g_SlideTime = 0;
	g_AcrobatTime = 0;
	g_DeploySoundTime = 0;
	g_RetractDistance = 0;
	g_NotifyTime = 0;
	g_ScoreTime = 0;
	// Needed to reset death arms after map change
	gHUD.m_flExtraViewModelTime = 0;
	// Reset client values to render objectives
	gHUD.m_iShowingWeaponMenu = 0;
	gHUD.m_Scoreboard.m_iShowscoresHeld = FALSE;

	gHUD.m_Mutators = NULL;
	gEngfuncs.pfnClientCmd("firstperson\n");
	gHUD.m_StatusIcons.Reset();

	g_IronSight = FALSE;

	gHUD.local_player_index = 0;
	gHUD.m_ChaosTime = 0;
	gHUD.m_ChaosStartTime = 0;
	gHUD.m_ChaosIncrement = 0;
}


int CHud :: MsgFunc_GameMode(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_GameMode = m_Teamplay = READ_BYTE();

	return 1;
}


int CHud :: MsgFunc_Damage(const char *pszName, int iSize, void *pbuf )
{
	int		armor, blood;
	Vector	from;
	int		i;
	float	count;
	
	BEGIN_READ( pbuf, iSize );
	armor = READ_BYTE();
	blood = READ_BYTE();

	for (i=0 ; i<3 ; i++)
		from[i] = READ_COORD();

	count = (blood * 0.5) + (armor * 0.5);

	if (count < 10)
		count = 10;

	// TODO: kick viewangles,  show damage visually

	return 1;
}

int CHud :: MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iConcussionEffect = READ_BYTE();
	if (m_iConcussionEffect)
		this->m_StatusIcons.EnableIcon("dmg_concuss",255,160,0,0,0);
	else
		this->m_StatusIcons.DisableIcon("dmg_concuss");
	return 1;
}

int CHud :: MsgFunc_Acrobatics(const char *pszName, int iSize, void *pbuf )
{
	// Unstick slide after levelchange
	if ((gEngfuncs.GetClientTime() + 2) < g_SlideTime) {
		g_SlideTime = 0;
	}

	if ((gEngfuncs.GetClientTime() + 2) < g_AcrobatTime) {
		g_AcrobatTime = 0;
	}

	if (CL_IsThirdPerson())
		return 1;

	BEGIN_READ( pbuf, iSize );
	int mode = READ_BYTE();
	int axis = 0, amount = 0;
	float time = 1.0;
	switch (mode)
	{
	case ACROBATICS_ROLL_RIGHT:
		axis = ROLL;
		amount = -360;
		break;
	case ACROBATICS_ROLL_LEFT:
		axis = ROLL;
		amount = 360;
		break;
	case ACROBATICS_FLIP_BACK:
		axis = PITCH;
		amount = 360;
		break;
	case ACROBATICS_FLIP_FRONT:
		axis = PITCH;
		amount = -360;
		break;
	case ACROBATICS_WALLCLIMB:
		g_WallClimb = gEngfuncs.GetClientTime() + 1;
		gHUD.m_WallClimb.m_iFlags |= HUD_ACTIVE;
		break;
	case ACROBATICS_HURRICANE_KICK:
		axis = YAW;
		amount = -720;
		time = 1.35;
		break;
	default:
		g_SlideTime = gEngfuncs.GetClientTime() + 1;
	}

	if (amount > 0 || amount < 0) {
		if (!cl_antivomit->value)
			V_PunchAxis(axis, amount);
		g_AcrobatTime = gEngfuncs.GetClientTime() + time;
	}

	return 1;
}

int CHud :: MsgFunc_PlayCSound( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int index = READ_BYTE();

	// Mute any incoming deploy sounds
	g_DeploySoundTime = gEngfuncs.GetClientTime() + 1.0;

	switch (index)
	{
		case CLIENT_SOUND_PREPAREFORBATTLE:
			PlaySound("prepareforbattle.wav", 1);
			break;
		case CLIENT_SOUND_OUTSTANDING:
			PlaySound("outstanding.wav", 1);
			break;
		case CLIENT_SOUND_HULIMATING_DEAFEAT:
			PlaySound("humiliatingdefeat.wav", 1);
			break;
		case CLIENT_SOUND_MASSACRE:
			PlaySound("massacre.wav", 1);
			break;
		case CLIENT_SOUND_KILLINGMACHINE:
			PlaySound("killingmachine.wav", 1);
			break;
		case CLIENT_SOUND_PICKUPYOURWEAPON:
			PlaySound("pickupyourweaponsandfight.wav", 1);
			break;
		case CLIENT_SOUND_LMS:
			PlaySound("youarethelastmanstanding.wav", 1);
			break;
		case CLIENT_SOUND_PREPARETOFIGHT:
			PlaySound("preparetofight.wav", 1);
			break;
		case CLIENT_SOUND_TAKENLEAD:
			PlaySound("takenlead.wav", 1);
			break;
		case CLIENT_SOUND_TIEDLEAD:
			PlaySound("tiedlead.wav", 1);
			break;
		case CLIENT_SOUND_LOSTLEAD:
			PlaySound("lostlead.wav", 1);
			break;
		case CLIENT_SOUND_WHICKEDSICK:
			PlaySound("whickedsick.wav", 1);
			break;
		case CLIENT_SOUND_UNSTOPPABLE:
			PlaySound("unstoppable2.wav", 1);
			break;
		case CLIENT_SOUND_MANIAC:
			PlaySound("maniac.wav", 1);
			break;
		case CLIENT_SOUND_IMPRESSIVE:
			PlaySound("impressive.wav", 1);
			break;
		case CLIENT_SOUND_EXCELLENT:
			PlaySound("excellent.wav", 1);
			break;
		case CLIENT_SOUND_BULLSEYE:
			PlaySound("bullseye.wav", 1);
			break;
		case CLIENT_SOUND_HEADSHOT:
			PlaySound("headshot.wav", 1);
			break;
		case CLIENT_SOUND_FIRSTBLOOD:
			PlaySound("firstblood.wav", 1);
			break;
		case CLIENT_SOUND_VOTEGAME:
			PlaySound("votegame.wav", 1);
			break;
		case CLIENT_SOUND_VOTEMAP:
			PlaySound("votemap.wav", 1);
			break;
		case CLIENT_SOUND_GREATJOB:
			PlaySound("greatjob.wav", 1);
			break;
		case CLIENT_SOUND_VOTESTARTED:
			PlaySound("votestarted.wav", 1);
			break;
		case CLIENT_SOUND_VOTEMUTATOR:
			PlaySound("votemutator.wav", 1);
			break;
		case CLIENT_SOUND_CTF_TAKEN:
			PlaySound("ctf_flag_taken.wav", 1);
			break;
		case CLIENT_SOUND_CTF_CAPTURE:
			PlaySound("ctf_flag_capture.wav", 1);
			break;
		case CLIENT_SOUND_WAVE_BEGINS:
			PlaySound("wavebegins.wav", 1);
			break;
		case CLIENT_SOUND_WAVE_ENDED:
			PlaySound("waveended.wav", 1);
			break;
		case CLIENT_SOUND_GOGOGO:
			PlaySound("usas_gogogo.wav", 1);
			break;
		case CLIENT_SOUND_FIVE:
			PlaySound("fvox/five.wav", 1);
			break;
		case CLIENT_SOUND_FOUR:
			PlaySound("fvox/four.wav", 1);
			break;
		case CLIENT_SOUND_THREE:
			PlaySound("fvox/three.wav", 1);
			break;
	}
	return 1;
}

int CHud :: MsgFunc_AddMut( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int mutatorId = READ_BYTE();
	int mtime = READ_BYTE();
	int time = (int)gHUD.m_flTime + mtime;

	if (mutatorId == 254)
	{
		m_ChaosTime = m_ChaosStartTime;
		mutators_t *t = m_Mutators;
		while (t != NULL)
		{
			t->timeToLive = t->startTime;
			t = t->next;
		}
	} else {
		// add
		while (mutatorId != -1)
		{
			if (mutatorId != 255)
			{
				mutators_t *mutator = new mutators_t();
				mutator->mutatorId = mutatorId;
				mutator->startTime = gHUD.m_flTime;
				mutator->timeToLive = mtime == 253 ? -1 : time;
				mutator->next = m_Mutators ? m_Mutators : NULL;
				m_Mutators = mutator;

				// Specific for client
				if (mutatorId == MUTATOR_THIRDPERSON)
					gEngfuncs.pfnClientCmd("thirdperson\n");
				else if (mutatorId == MUTATOR_CLOSEUP)
					g_IronSight = TRUE;

				// gEngfuncs.Con_DPrintf(">>> got mutator[id=%d, start=%.2f, ttl=%.2f]\n", mutator->mutatorId, mutator->startTime, mutator->timeToLive );
			}

			mutatorId = READ_BYTE();
			time = (int)gHUD.m_flTime + READ_BYTE();
		}
	}

	m_StatusIcons.DrawMutators();

	return 1;
}

int CHud :: MsgFunc_Chaos( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_ChaosStartTime = gHUD.m_flTime;
	m_ChaosIncrement = READ_BYTE();
	m_ChaosTime = (int)gHUD.m_flTime + m_ChaosIncrement;
	return 1;
}

int CHud :: MsgFunc_Particle( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int entindex = READ_SHORT();
	char *sz = READ_STRING();
	char fileName[64];
	strcpy(fileName, "aurora/");

	if ((gHUD.m_IceModelsIndex >= SKIN_INVERSE && gHUD.m_IceModelsIndex <= SKIN_EDITION))
	{
		strcat(fileName, "ice_");
	}

	strcat(fileName, sz);

	// gEngfuncs.Con_Printf("MsgFunc_Particle entindex=%d,fileName=%s\n", entindex, fileName);

	ParticleSystem *pSystem = new ParticleSystem(entindex, fileName);
	g_pParticleSystems.AddSystem(pSystem, fileName);
	
	return 1;
}

void CHud :: MsgFunc_DelPart( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int entindex = READ_SHORT();
	int del = READ_BYTE();

	// gEngfuncs.Con_Printf("MsgFunc_DelPart entindex=%d,del=%d\n", entindex, del);

	for (int i = 0; i <= del + 1; i++)
		g_pParticleSystems.DeleteSystemWithEntity(entindex);

	return;
}

void CHud :: MsgFunc_FlameMsg( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int n;
	int s;

	n = READ_SHORT();
	s = READ_BYTE();
	
	FlameSystem.SetState(n, s);
}

void CHud :: MsgFunc_FlameKill( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int n;

	n = READ_SHORT();
	
	FlameSystem.Extinguish(n);
}

void CHud :: MsgFunc_MParticle( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	char *sz = READ_STRING();
	int count = READ_BYTE();
	int entindex;

	char fileName[64];
	strcpy(fileName, "aurora/");
	if ((gHUD.m_IceModelsIndex >= SKIN_INVERSE && gHUD.m_IceModelsIndex <= SKIN_EDITION))
	{
		strcat(fileName, "ice_");
	}
	strcat(fileName, sz);

	for (int i = 0; i < count; i++)
	{
		entindex = READ_SHORT();

		// gEngfuncs.Con_Printf("MsgFunc_MParticle entindex=%d,fileName=%s\n", entindex, fileName);

		ParticleSystem *pSystem = new ParticleSystem(entindex, fileName);
		g_pParticleSystems.AddSystem(pSystem, fileName); 
	}
}
