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

DECLARE_MESSAGE( m_StatusIcons, StatusIcon );

extern float g_xP, g_yP;

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
	m_iFlags &= ~HUD_ACTIVE;

	DrawMutators();
}

// Draw status icons along the left-hand side of the screen
int CHudStatusIcons::Draw( float flTime )
{
	if (gEngfuncs.IsSpectateOnly() || gHUD.m_iShowingWeaponMenu)
		return 1;

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
		EnableIcon( pszIconName, r, g, b );
		m_iFlags |= HUD_ACTIVE;
	}
	else
	{
		DisableIcon( pszIconName );
	}

	return 1;
}

// add the icon to the icon list, and set it's drawing color
void CHudStatusIcons::EnableIcon( char *pszIconName, unsigned char red, unsigned char green, unsigned char blue )
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
	ToggleMutatorIcon(MUTATOR_COOLFLESH, "coolflesh");
	ToggleMutatorIcon(MUTATOR_SANTAHAT, "santahat");
	ToggleMutatorIcon(MUTATOR_VOLATILE, "volatile");
	ToggleMutatorIcon(MUTATOR_PORTAL, "portal");
	ToggleMutatorIcon(MUTATOR_JOPE, "jope");
	ToggleMutatorIcon(MUTATOR_PLUMBER, "plumber");
	ToggleMutatorIcon(MUTATOR_INVERSE, "inverse");
	ToggleMutatorIcon(MUTATOR_OLDTIME, "oldtime");
	ToggleMutatorIcon(MUTATOR_SILDENAFIL, "sildenafil");
	ToggleMutatorIcon(MUTATOR_GRAVITY, "astronaut");
	ToggleMutatorIcon(MUTATOR_INVISIBLE, "invisible");
	ToggleMutatorIcon(MUTATOR_GRENADES, "grenades");
	ToggleMutatorIcon(MUTATOR_ROCKETS, "rockets");
	ToggleMutatorIcon(MUTATOR_SPEEDUP, "speedup");
	ToggleMutatorIcon(MUTATOR_CHUMXPLODE, "chumxplode");
	ToggleMutatorIcon(MUTATOR_LONGJUMP, "longjump");
	ToggleMutatorIcon(MUTATOR_SLOWBULLETS, "slowbullets");
	ToggleMutatorIcon(MUTATOR_EXPLOSIVEAI, "explosiveai");
	ToggleMutatorIcon(MUTATOR_PAINTBALL, "paintball");
	ToggleMutatorIcon(MUTATOR_ITEMSEXPLODE, "itemsexplode");
	ToggleMutatorIcon(MUTATOR_LIGHTSOUT, "lightsout");
	ToggleMutatorIcon(MUTATOR_ICE, "ice");
	ToggleMutatorIcon(MUTATOR_PUSHY, "pushy");
	ToggleMutatorIcon(MUTATOR_SUPERJUMP, "superjump");
	ToggleMutatorIcon(MUTATOR_BARRELS, "barrels");
	ToggleMutatorIcon(MUTATOR_SANIC, "sanic");
	ToggleMutatorIcon(MUTATOR_SANIC, "snowball");
	ToggleMutatorIcon(MUTATOR_LOOPBACK, "loopback");
	ToggleMutatorIcon(MUTATOR_TURRETS, "turrets");
	ToggleMutatorIcon(MUTATOR_INFINITEAMMO, "infiniteammo");
	ToggleMutatorIcon(MUTATOR_RANDOMWEAPON, "randomweapon");
	ToggleMutatorIcon(MUTATOR_ROCKETCROWBAR, "rocketcrowbar");
	ToggleMutatorIcon(MUTATOR_NOTTHEBEES, "notthebees");
	ToggleMutatorIcon(MUTATOR_INSTAGIB, "instagib");
	ToggleMutatorIcon(MUTATOR_MEGASPEED, "megarun");
	ToggleMutatorIcon(MUTATOR_MAXPACK, "maxpack");
	ToggleMutatorIcon(MUTATOR_BIGHEAD, "bighead");
	ToggleMutatorIcon(MUTATOR_SLOWMO, "slowmo");
	ToggleMutatorIcon(MUTATOR_DONTSHOOT, "dontshoot");
	ToggleMutatorIcon(MUTATOR_999, "999");
	ToggleMutatorIcon(MUTATOR_BERSERKER, "berserker");
	ToggleMutatorIcon(MUTATOR_AUTOAIM, "autoaim");
	ToggleMutatorIcon(MUTATOR_SLOWWEAPONS, "slowweapons");
	ToggleMutatorIcon(MUTATOR_FASTWEAPONS, "fastweapons");
	ToggleMutatorIcon(MUTATOR_JACK, "jack");
	ToggleMutatorIcon(MUTATOR_PIRATEHAT, "piratehat");
	ToggleMutatorIcon(MUTATOR_MARSHMELLO, "marshmellow");
	ToggleMutatorIcon(MUTATOR_CRATE, "crate");
	ToggleMutatorIcon(MUTATOR_PUMPKIN, "pumpkin");
	ToggleMutatorIcon(MUTATOR_JEEPATHON, "jeepathon");
	ToggleMutatorIcon(MUTATOR_TOILET, "toilet");
	ToggleMutatorIcon(MUTATOR_RICOCHET, "ricochet");
}

void CHudStatusIcons::ToggleMutatorIcon(int mutatorId, const char *mutator)
{
	int r, g, b;
	UnpackRGB(r,g,b, HudColor());
	ScaleColors(r, g, b, MIN_ALPHA);

	if (gHUD.szActiveMutators != NULL &&
		(strstr(gHUD.szActiveMutators, mutator) ||
		atoi(gHUD.szActiveMutators) == mutatorId))
	{
		m_iFlags |= HUD_ACTIVE;
		EnableIcon((char *)mutator,r,g,b);
	}
	else
		DisableIcon((char *)mutator);
}
