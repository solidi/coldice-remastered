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

#define MAX_DAMAGE_NUMBERS 5  // Allow up to 5 simultaneous damage numbers per player

struct s_LifeBarData {
    int health;
    int armor;
	float refreshTime;
};

struct s_DamageNumber {
    int damage;               // Amount of damage
    float spawnTime;          // When it was created
    float lifetime;           // How long it should exist
    vec3_t worldPosition;     // Locked world position where damage occurred
    bool active;              // Whether this slot is in use
    float horizVelX;          // World-space horizontal drift velocity (X)
    float horizVelY;          // World-space horizontal drift velocity (Y)
};

class CHudLifeBar: public CHudBase
{
public:
	CHudLifeBar();
	virtual int Init( void );
	virtual int VidInit( void );
	virtual int UpdateSprites();
	virtual int Draw(float flTime);
	virtual void Reset( void );
	int MsgFunc_LifeBar(const char *pszName,  int iSize, void *pbuf);

private:
	void AddDamageNumber(int playerIndex, int damage);
	void RenderDamageNumbers();
	void RenderDamageDigits(int damage, vec3_t worldPosition, float floatOffset, float horizOffsetX, float horizOffsetY, int alpha);
	
	HSPRITE m_LifeBarHeadModel;
	cl_entity_s		m_LifeBarModels[VOICE_MAX_PLAYERS+1];
	s_LifeBarData m_LifeBarData[VOICE_MAX_PLAYERS+1];
	s_DamageNumber m_DamageNumbers[VOICE_MAX_PLAYERS+1][MAX_DAMAGE_NUMBERS];
	int m_PreviousHealth[VOICE_MAX_PLAYERS+1];  // Track previous health to calculate delta
};

CHudLifeBar* GetLifeBar();

#endif
