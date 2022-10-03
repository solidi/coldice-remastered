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

#ifndef LIFE_BAR_H
#define LIFE_BAR_H
#pragma once

struct s_LifeBarData {
    int health;
    int armor;
	float refreshTime;
};

class CHudLifeBar: public CHudBase
{
public:
	CHudLifeBar();
	virtual int Init( void );
	virtual int VidInit( void );
	virtual int UpdateSprites();
	virtual void Reset( void );
	int MsgFunc_LifeBar(const char *pszName,  int iSize, void *pbuf);

private:
	HSPRITE m_LifeBarHeadModel;
	cl_entity_s		m_LifeBarModels[VOICE_MAX_PLAYERS+1];
	s_LifeBarData m_LifeBarData[VOICE_MAX_PLAYERS+1];
};

CHudLifeBar* GetLifeBar();

#endif
