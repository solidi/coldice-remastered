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
// status_icons.cpp
//
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"
#include "event_api.h"
#include "vgui_TeamFortressViewport.h"

DECLARE_MESSAGE( m_StatusIcons, StatusIcon );

extern float g_xP, g_yP;

extern qboolean g_IronSight;

int CHudStatusIcons::Init( void )
{
	HOOK_MESSAGE( StatusIcon );

	gHUD.AddHudElem( this );

	Reset();

	return 1;
}

int CHudStatusIcons::VidInit( void )
{
	return 1;
}

void CHudStatusIcons::Reset( void )
{
	memset( m_IconList, 0, sizeof m_IconList );
	m_iFlags |= HUD_ACTIVE;
	m_flCheckMutators = 0;
	DrawMutators();
}

// Draw status icons along the left-hand side of the screen
int CHudStatusIcons::Draw( float flTime )
{
	if (gEngfuncs.IsSpectateOnly() || gHUD.m_iShowingWeaponMenu)
		return 1;

	// Chaos bar
	int time = gHUD.m_ChaosTime;
	if (time > gHUD.m_flTime)
	{
		int r, g, b;
		UnpackRGB(r,g,b, HudColor());
		ScaleColors(r, g, b, MIN_ALPHA);
		FillRGBA(0, ScreenHeight - (ScreenHeight * ((gHUD.m_flTime - gHUD.m_ChaosStartTime) / gHUD.m_ChaosIncrement)), XRES(3), ScreenHeight * ((gHUD.m_flTime - gHUD.m_ChaosStartTime) / gHUD.m_ChaosIncrement), r, g, b, MAX_ALPHA);
	}

	// find starting position to draw from, along right-hand side of screen
	int x = 32 + g_xP;
	int y = (ScreenHeight / 1.5) + g_yP;
	
	// loop through icon list, and draw any valid icons drawing up from the middle of screen
	int max = 4;
	if (ScreenHeight == 480)
		max = 2;
	else if (ScreenHeight == 600)
		max = 3;
	for ( int i = 0; i < MAX_ICONSPRITES; i++ )
	{
		if ( m_IconList[i].spr )
		{
			if (i < max)
			{
				y -= ( m_IconList[i].rc.bottom - m_IconList[i].rc.top ) + 18;
				
				int r = m_IconList[i].r;
				int g = m_IconList[i].g;
				int b = m_IconList[i].b;
				UnpackRGB(r,g,b, HudColor());
				ScaleColors(r, g, b, MIN_ALPHA);
				SPR_Set( m_IconList[i].spr, r, g, b );
				SPR_DrawAdditive( 0, x, y, &m_IconList[i].rc );

				// Timer bar
				if (m_IconList[i].timeToLive > 0 && m_IconList[i].startTime > 0)
				{
					UnpackRGB(r,g,b, HudColor());
					ScaleColors(r, g, b, MAX_ALPHA);
					float start = (float)m_IconList[i].startTime;
					int unit = 64;
					int width = fmin(fmax(0, unit - (((gHUD.m_flTime - start) / (m_IconList[i].timeToLive - start)) * unit)), unit);
					FillRGBA(x, y + 64, width, 2, r, g, b, MAX_ALPHA);
				}

				// Label the icon for clarity
				const char *szSpriteName = m_IconList[i].szSpriteName;
				if (strncmp(szSpriteName, "rune_", 5) == 0)
					szSpriteName += 5;
				int size = ConsoleStringLen(szSpriteName);
				DrawConsoleString(x + (((m_IconList[i].rc.right - m_IconList[i].rc.left) / 2) - (size / 2)), y + (m_IconList[i].rc.bottom - m_IconList[i].rc.top), szSpriteName);
			}
			else
			{
				y -= 18;
				const char *szSpriteName = m_IconList[i].szSpriteName;
				if (strncmp(szSpriteName, "rune_", 5) == 0)
					szSpriteName += 5;
				int size = ConsoleStringLen(szSpriteName);
				DrawConsoleString(x + (((m_IconList[i].rc.right - m_IconList[i].rc.left) / 2) - (size / 2)), y, szSpriteName);
			}
		}
	}

	// clean up mutators
	if (m_flCheckMutators < gHUD.m_flTime)
	{
		mutators_t *m = gHUD.m_Mutators;
		mutators_t *prev = NULL;
		while (m != NULL) {
			// gEngfuncs.Con_DPrintf(">>> delete? mutator[id=%d, ttl=%.2f < ? (combined)=%.2f]\n", m->mutatorId, m->timeToLive, gHUD.m_flTime );
			if (m->timeToLive < gHUD.m_flTime && m->timeToLive != -1) {
				if (m->mutatorId == MUTATOR_THIRDPERSON)
					gEngfuncs.pfnClientCmd("firstperson\n");
				else if (m->mutatorId == MUTATOR_CLOSEUP)
					g_IronSight = FALSE;

				if (prev) {
					mutators_t *temp;
					temp = prev->next;
					prev->next = temp->next;
				} else {
					mutators_t *temp;
					temp = gHUD.m_Mutators;
					gHUD.m_Mutators = temp->next;
				}
			}
			prev = m;
			m = m->next;
		}
		DrawMutators(); // update hud
		m_flCheckMutators = gHUD.m_flTime + 1.0;
	}

	return 1;
}

// Message handler for StatusIcon message
// accepts five values:
//		byte   : TRUE = ENABLE icon, FALSE = DISABLE icon
//		string : the sprite name to display
//		byte   : red
//		byte   : green
//		byte   : blue
int CHudStatusIcons::MsgFunc_StatusIcon( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int ShouldEnable = READ_BYTE();
	char *pszIconName = READ_STRING();
	if ( ShouldEnable )
	{
		int r = READ_BYTE();
		int g = READ_BYTE();
		int b = READ_BYTE();
		EnableIcon( pszIconName, r, g, b, 0, 0);
		m_iFlags |= HUD_ACTIVE;
	}
	else
	{
		DisableIcon( pszIconName );
	}

	return 1;
}

// add the icon to the icon list, and set it's drawing color
void CHudStatusIcons::EnableIcon( char *pszIconName, unsigned char red, unsigned char green, unsigned char blue, float timeToLive, float startTime )
{
	int i;
	// check to see if the sprite is in the current list
	for ( i = 0; i < MAX_ICONSPRITES; i++ )
	{
		if ( !stricmp( m_IconList[i].szSpriteName, pszIconName ) )
			break;
	}

	if ( i == MAX_ICONSPRITES )
	{
		// icon not in list, so find an empty slot to add to
		for ( i = 0; i < MAX_ICONSPRITES; i++ )
		{
			if ( !m_IconList[i].spr )
				break;
		}
	}

	// if we've run out of space in the list, overwrite the first icon
	if ( i == MAX_ICONSPRITES )
	{
		i = 0;
	}

	// Load the sprite and add it to the list
	// the sprite must be listed in hud.txt
	int spr_index = gHUD.GetSpriteIndex( pszIconName );
	m_IconList[i].spr = gHUD.GetSprite( spr_index );
	m_IconList[i].rc = gHUD.GetSpriteRect( spr_index );
	m_IconList[i].r = red;
	m_IconList[i].g = green;
	m_IconList[i].b = blue;
	m_IconList[i].timeToLive = timeToLive;
	m_IconList[i].startTime = startTime;
	strcpy( m_IconList[i].szSpriteName, pszIconName );

	// Hack: Play Timer sound when a grenade icon is played (in 0.8 seconds)
	// if ( strstr(m_IconList[i].szSpriteName, "grenade") )
	// {
	//	cl_entity_t *pthisplayer = gEngfuncs.GetLocalPlayer();
	//	gEngfuncs.pEventAPI->EV_PlaySound( pthisplayer->index, pthisplayer->origin, CHAN_STATIC, "weapons/timer.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
	//}
}

void CHudStatusIcons::DisableIcon( char *pszIconName )
{
	// find the sprite is in the current list
	for ( int i = 0; i < MAX_ICONSPRITES; i++ )
	{
		if ( !stricmp( m_IconList[i].szSpriteName, pszIconName ) )
		{
			// clear the item from the list
			memset( &m_IconList[i], 0, sizeof(icon_sprite_t) );
			return;
		}
	}
}

void CHudStatusIcons::DrawMutators( void )
{
	// No chaos or random.
	for (int mutatorId = 1; mutatorId < (MAX_MUTATORS - 1); mutatorId++)
	{
		// because chaos is 0 or 1
		int id = mutatorId + 1;
		ToggleMutatorIcon(id, sMutators[mutatorId]);
	}
}

void CHudStatusIcons::ToggleMutatorIcon(int mutatorId, const char *mutator)
{
	int r, g, b;
	UnpackRGB(r,g,b, HudColor());
	ScaleColors(r, g, b, MIN_ALPHA);

	if (MutatorEnabled(mutatorId))
	{
		mutators_t t = GetMutator(mutatorId);
		EnableIcon((char *)mutator,r,g,b,t.timeToLive,t.startTime);
	}
	else
		DisableIcon((char *)mutator);
}
