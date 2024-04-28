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

#include "stdio.h"
#include "stdlib.h"
#include "math.h"

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include "entity_types.h"

DECLARE_MESSAGE(m_LifeBar, LifeBar)

extern int cam_thirdperson;
extern cvar_t *cl_lifemeter;
extern cvar_t *cl_playpoint;

CHudLifeBar g_LifeBar;

CHudLifeBar* GetLifeBar()
{
	return &g_LifeBar;
}

CHudLifeBar::CHudLifeBar()
{

}

int CHudLifeBar::Init(void)
{
	HOOK_MESSAGE(LifeBar);

	gHUD.AddHudElem(this);
	return 1;
}

void CHudLifeBar::Reset( void )
{

}

int CHudLifeBar::VidInit(void)
{
	m_LifeBarHeadModel = gEngfuncs.pfnSPR_Load("sprites/lifebar.spr");
	return 1;
}

int CHudLifeBar::MsgFunc_LifeBar(const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int armor = READ_BYTE(); // armor
	int index = READ_BYTE(); // who renders
	int hitindex = READ_BYTE(); // who hit me

	if (armor > 100) armor = 0;
	armor = fmin(fmax(armor, 0), 100);

	GetLifeBar()->m_LifeBarData[index].health = 0;
	GetLifeBar()->m_LifeBarData[index].armor = armor;
	GetLifeBar()->m_LifeBarData[index].refreshTime = gEngfuncs.GetClientTime() + 2;

	int iLocalPlayerIndex = gEngfuncs.GetLocalPlayer()->index;
	cl_entity_s *pClient = gEngfuncs.GetEntityByIndex(index);
	// gEngfuncs.Con_DPrintf(">>>>> armor=%d, hit=%d, who=%d\n", armor, index, hitindex);
	int health = pClient->curstate.health;

	if (cl_playpoint && !cl_playpoint->value)
	{
		if (health > 0 && index != iLocalPlayerIndex && iLocalPlayerIndex == hitindex)
		{
			PlaySound("ding.wav", 2);
			// gEngfuncs.Con_DPrintf(">>>>> ding Ding DING!\n");
		}
	}

	return 1;
}

int CHudLifeBar::UpdateSprites()
{
	if (cl_lifemeter && !cl_lifemeter->value)
		return 0;

	if (!m_LifeBarHeadModel)
		return 0;

	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();

	int iOutModel = 0;
	int maxHealth = 100;

	if (MutatorEnabled(MUTATOR_999))
		maxHealth = 999;

	for (int i = 0; i < 32; i++)
	{
		cl_entity_s *pClient = gEngfuncs.GetEntityByIndex(i+1);
		int health = pClient->curstate.health;
		int armor = m_LifeBarData[i+1].armor;
		
		if (m_LifeBarData[i+1].refreshTime < gEngfuncs.GetClientTime())
			continue;
		
		// Don't show an icon if the player is not in our PVS.
		if (!pClient || pClient->curstate.messagenum < localPlayer->curstate.messagenum)
			continue;

		// Don't show an icon for dead or spectating players (ie: invisible entities).
		if (pClient->curstate.effects & EF_NODRAW)
			continue;

		// Show an icon for the local player unless we're in thirdperson mode.
		if (pClient == localPlayer && !cam_thirdperson)
			continue;
		
		if (!pClient->player)
			continue;

		cl_entity_s *pEnt = &m_LifeBarModels[iOutModel];
		++iOutModel;

		memset(pEnt, 0, sizeof(*pEnt));
		pEnt->curstate.rendermode = kRenderTransAlpha;
		pEnt->curstate.renderamt = 220;
		pEnt->baseline.renderamt = 220;
		pEnt->curstate.renderfx = kRenderFxNoDissipation;
		pEnt->curstate.framerate = 0;
		pEnt->model = (struct model_s*)gEngfuncs.GetSpritePointer(m_LifeBarHeadModel);
		int calcHealth = ((health / float(maxHealth)) * 10);
		if (health > 900)
			pEnt->curstate.frame = 12; // 999 icon
		else if (health > 0)
			pEnt->curstate.frame = fmin(fmax(calcHealth, 0), 10);
		else
			pEnt->curstate.frame = 13; // death icon
		pEnt->angles[0] = pEnt->angles[1] = pEnt->angles[2] = 0;
		pEnt->curstate.scale = 0.5f;

		pEnt->origin[0] = pEnt->origin[1] = 0;
		pEnt->origin[2] = 45;

		VectorAdd(pEnt->origin, pClient->origin, pEnt->origin);

		gEngfuncs.CL_CreateVisibleEntity(ET_NORMAL, pEnt);
	}

	return 1;
}
