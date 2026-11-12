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
#include "pm_shared.h"
#include "camera.h"

DECLARE_MESSAGE( m_StatusIcons, StatusIcon );

extern float g_xP, g_yP;
extern cvar_t *cl_hudbend;

extern qboolean g_IronSight;

// Icons besides cam_* and rune_* that belong in the bottom-center stack
static const char *s_szCenterIcons[] =
{
	"longjump",
};

static bool IsCenterIcon( const char *pszIconName )
{
	if ( !pszIconName )
		return false;

	if ( !strnicmp( pszIconName, "cam_", 4 ) || !strnicmp( pszIconName, "rune_", 5 ) )
		return true;

	for ( int i = 0; i < (int)(sizeof(s_szCenterIcons) / sizeof(s_szCenterIcons[0])); i++ )
	{
		if ( !stricmp( pszIconName, s_szCenterIcons[i] ) )
			return true;
	}

	return false;
}

static const char *IconLabel( const char *szSpriteName )
{
	if (strncmp(szSpriteName, "rune_", 5) == 0)
		return szSpriteName + 5;
	if (strncmp(szSpriteName, "cam_", 4) == 0)
		return szSpriteName + 4;
	return szSpriteName;
}

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
	memset( m_CenterIconList, 0, sizeof m_CenterIconList );
	memset( m_LastMutatorStatus, 0, sizeof m_LastMutatorStatus );
	m_iFlags |= HUD_ACTIVE;
	m_flCheckMutators = 0;
	if (!MutatorEnabled(MUTATOR_THIRDPERSON))
		CAM_ToFirstPerson();
	DrawMutators();
}

// Draw status icons along the left-hand side of the screen
int CHudStatusIcons::Draw( float flTime )
{
	const bool noHudMutator = MutatorEnabled(MUTATOR_NOHUD);

	if ((gEngfuncs.IsSpectateOnly() || gHUD.m_iShowingWeaponMenu) && !noHudMutator)
		return 1;

	if ((g_iUser3 >= OBS_BEGIN && g_iUser3 <= OBS_END) && !noHudMutator) // when there is a menu, don't show status icons
		return 1;

	// Chaos bar
	int time = gHUD.m_ChaosTime;
	if (!noHudMutator && time > gHUD.m_flTime)
	{
		int minutes = fmin(fmax(0, (int)( gHUD.m_ChaosTime - gHUD.m_flTime ) / 60), 99);
		int seconds = fmin(fmax(0, (int)( gHUD.m_ChaosTime - gHUD.m_flTime ) - (minutes * 60)), 59);
		gHUD.m_Timer.FVoxTimerCallOut(seconds, minutes);

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
			if (noHudMutator && stricmp(m_IconList[i].szSpriteName, "nohud") != 0)
				continue;

			// Label the icon for clarity
			const char *szSpriteName = IconLabel(m_IconList[i].szSpriteName);
			int size = ConsoleStringLen(szSpriteName);

			if (i < max)
			{
				y -= ( m_IconList[i].rc.bottom - m_IconList[i].rc.top ) + 18;

				int r, g, b;
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

				DrawConsoleString(x + (((m_IconList[i].rc.right - m_IconList[i].rc.left) / 2) - (size / 2)), y + (m_IconList[i].rc.bottom - m_IconList[i].rc.top), szSpriteName);
			}
			else
			{
				y -= 18;
				DrawConsoleString(x + (((m_IconList[i].rc.right - m_IconList[i].rc.left) / 2) - (size / 2)), y, szSpriteName);
			}
		}
	}

	if (!noHudMutator)
		DrawCenterIcons();

	// clean up mutators
	if (m_flCheckMutators < gHUD.m_flTime)
	{
		mutators_t *m = gHUD.m_Mutators;
		mutators_t *prev = NULL;
		while (m != NULL) {
			mutators_t *next = m->next;
			// gEngfuncs.Con_DPrintf(">>> delete? mutator[id=%d, ttl=%.2f < ? (combined)=%.2f]\n", m->mutatorId, m->timeToLive, gHUD.m_flTime );
			if (m->timeToLive < gHUD.m_flTime && m->timeToLive != -1) {
				if (m->mutatorId == MUTATOR_THIRDPERSON)
					CAM_ToFirstPerson();
				else if (m->mutatorId == MUTATOR_TINNITUS)
					gEngfuncs.pfnClientCmd("tinnitus_stop\n");
				else if (m->mutatorId == MUTATOR_CLOSEUP)
					g_IronSight = FALSE;

				if (prev) {
					prev->next = m->next;
				} else {
					gHUD.m_Mutators = m->next;
				}

				delete m;
			} else {
				prev = m;
			}
			m = next;
		}
		DrawMutators(); // update hud
		m_flCheckMutators = gHUD.m_flTime + 1.0;
	}

	return 1;
}

// Message handler for StatusIcon message
// accepts two values:
//		byte   : TRUE = ENABLE icon, FALSE = DISABLE icon
//		string : the sprite name to display
int CHudStatusIcons::MsgFunc_StatusIcon( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int ShouldEnable = READ_BYTE();
	char *pszIconName = READ_STRING();
	if ( ShouldEnable )
	{
		EnableIcon( pszIconName, 0, 0);
		m_iFlags |= HUD_ACTIVE;
	}
	else
	{
		DisableIcon( pszIconName );
	}

	// Special entities for perspective switching
	if (pszIconName &&
		(strstr(pszIconName, "cam_")))
	{
		if (ShouldEnable)
			CAM_ToThirdPerson();
		else
		{
			// Only revert to first person if no other cam_ icons are still active
			if (!HasCamIcon())
				CAM_ToFirstPerson();
		}
	}

	return 1;
}

bool CHudStatusIcons::HasCamIcon( void )
{
	for (int i = 0; i < MAX_CENTERICONS; i++)
	{
		if (m_CenterIconList[i].spr && strstr(m_CenterIconList[i].szSpriteName, "cam_"))
			return true;
	}

	for (int i = 0; i < MAX_ICONSPRITES; i++)
	{
		if (m_IconList[i].spr && strstr(m_IconList[i].szSpriteName, "cam_"))
			return true;
	}

	return false;
}

// Draw up to MAX_CENTERICONS_VISIBLE queued icons as a centered row whose
// bottom sits on the health / HEV / ammo baseline
void CHudStatusIcons::DrawCenterIcons( void )
{
	int visible[MAX_CENTERICONS_VISIBLE];
	int slotWidth[MAX_CENTERICONS_VISIBLE];
	int count = 0;
	int totalWidth = 0;
	int labelHeight = 0;

	for (int i = 0; i < MAX_CENTERICONS && count < MAX_CENTERICONS_VISIBLE; i++)
	{
		if (!m_CenterIconList[i].spr)
			continue;

		int labelWidth = 0, height = 0;
		GetConsoleStringSize(IconLabel(m_CenterIconList[i].szSpriteName), &labelWidth, &height);
		labelHeight = fmax(labelHeight, height);

		int iconWidth = m_CenterIconList[i].rc.right - m_CenterIconList[i].rc.left;
		visible[count] = i;
		slotWidth[count] = fmax(iconWidth, labelWidth);
		totalWidth += slotWidth[count];
		count++;
	}

	if (!count)
		return;

	const int gap = 16;
	const int barGap = 3;
	totalWidth += gap * (count - 1);

	// Same baseline as the health / battery numbers, including hud bend lift
	float up = cl_hudbend->value ? 1.5 : 1;
	float numScale = (24 * cl_hudbend->value) * 4;
	int rowBottom = (ScreenHeight - gHUD.m_iFontHeight * up - gHUD.m_iFontHeight / 2) + gHUD.m_iFontHeight + g_yP - numScale;
	int labelY = rowBottom - labelHeight;

	int x = (ScreenWidth / 2) - (totalWidth / 2) + g_xP;
	for (int k = 0; k < count; k++)
	{
		icon_sprite_t *icon = &m_CenterIconList[visible[k]];
		const char *label = IconLabel(icon->szSpriteName);
		int iconWidth = icon->rc.right - icon->rc.left;
		int iconHeight = icon->rc.bottom - icon->rc.top;
		int iconX = x + (slotWidth[k] - iconWidth) / 2;
		int iconY = labelY - barGap - iconHeight;

		int r, g, b;
		UnpackRGB(r, g, b, StatusIconColor(icon->szSpriteName));
		ScaleColors(r, g, b, MIN_ALPHA);
		SPR_Set(icon->spr, r, g, b);
		SPR_DrawAdditive(0, iconX, iconY, &icon->rc);

		// Timer bar
		if (icon->timeToLive > 0 && icon->startTime > 0)
		{
			UnpackRGB(r, g, b, StatusIconColor(icon->szSpriteName));
			ScaleColors(r, g, b, MAX_ALPHA);
			float start = (float)icon->startTime;
			int width = fmin(fmax(0, iconWidth - (((gHUD.m_flTime - start) / (icon->timeToLive - start)) * iconWidth)), iconWidth);
			FillRGBA(iconX, iconY + iconHeight, width, 2, r, g, b, MAX_ALPHA);
		}

		DrawConsoleString(x + (slotWidth[k] - ConsoleStringLen(label)) / 2, labelY, label);

		x += slotWidth[k] + gap;
	}
}

// add the icon to the icon list, and set it's drawing color
void CHudStatusIcons::EnableIcon( char *pszIconName, float timeToLive, float startTime )
{
	if ( !pszIconName || !*pszIconName )
		return;

	const bool center = IsCenterIcon( pszIconName );
	icon_sprite_t *list = center ? m_CenterIconList : m_IconList;
	const int listSize = center ? MAX_CENTERICONS : MAX_ICONSPRITES;

	int i;
	// check to see if the sprite is in the current list
	for ( i = 0; i < listSize; i++ )
	{
		if ( !stricmp( list[i].szSpriteName, pszIconName ) )
			break;
	}

	if ( i == listSize )
	{
		// icon not in list, so find an empty slot to add to
		for ( i = 0; i < listSize; i++ )
		{
			if ( !list[i].spr )
				break;
		}
	}

	if ( i == listSize )
	{
		// a full center queue keeps its order, so drop the newcomer
		if ( center )
		{
			gEngfuncs.Con_DPrintf("[StatusIcons] Center icon queue full, dropping '%s'\n", pszIconName);
			return;
		}

		// if we've run out of space in the list, overwrite the first icon
		i = 0;
	}

	// Load the sprite and add it to the list
	// the sprite must be listed in hud.txt
	int spr_index = gHUD.GetSpriteIndex( pszIconName );
	if ( spr_index < 0 )
	{
		gEngfuncs.Con_DPrintf("[Mutators] Missing HUD sprite alias '%s' in sprites/hud.txt\n", pszIconName);
		return;
	}

	list[i].spr = gHUD.GetSprite( spr_index );
	if ( !list[i].spr )
		return;

	list[i].rc = gHUD.GetSpriteRect( spr_index );
	list[i].timeToLive = timeToLive;
	list[i].startTime = startTime;
	strncpy( list[i].szSpriteName, pszIconName, sizeof(list[i].szSpriteName) - 1 );
	list[i].szSpriteName[sizeof(list[i].szSpriteName) - 1] = '\0';
}

void CHudStatusIcons::DisableIcon( char *pszIconName )
{
	if ( !pszIconName )
		return;

	for ( int i = 0; i < MAX_CENTERICONS; i++ )
	{
		if ( !stricmp( m_CenterIconList[i].szSpriteName, pszIconName ) )
		{
			// close the gap so queued icons move up in order
			memmove( &m_CenterIconList[i], &m_CenterIconList[i + 1], (MAX_CENTERICONS - i - 1) * sizeof(icon_sprite_t) );
			memset( &m_CenterIconList[MAX_CENTERICONS - 1], 0, sizeof(icon_sprite_t) );
			return;
		}
	}

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
	for (int mutatorId = 1; mutatorId < (MAX_MUTATORS_CL - 1); mutatorId++)
	{
		// because chaos is 0 or 1
		int id = mutatorId + 1;
		ToggleMutatorIcon(id, sMutators[mutatorId].name);
	}
}

void CHudStatusIcons::ToggleMutatorIcon(int mutatorId, const char *mutator)
{
	if (mutatorId < 0 || mutatorId >= MAX_MUTATORS_CL || !mutator || !*mutator)
		return;

	int r, g, b;
	UnpackRGB(r,g,b, HudColor());
	ScaleColors(r, g, b, MIN_ALPHA);

	if (MutatorEnabled(mutatorId))
	{
		if (m_LastMutatorStatus[mutatorId] == 1)
			return; // already active, so no need to enable again

		m_LastMutatorStatus[mutatorId] = 1;
		mutators_t t = GetMutator(mutatorId);
		EnableIcon((char *)mutator, t.timeToLive, t.startTime);
	}
	else
	{
		if (m_LastMutatorStatus[mutatorId] == 0)
			return; // not currently active, so no need to disable

		if (mutatorId == MUTATOR_LONGJUMP)
		{
			if (atoi(gEngfuncs.PhysInfo_ValueForKey("slj")) != 1)
				DisableIcon((char *)mutator);
		}
		else
			DisableIcon((char *)mutator);

		m_LastMutatorStatus[mutatorId] = 0;
	}
}
