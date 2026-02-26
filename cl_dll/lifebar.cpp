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
#include "triangleapi.h"

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
	// Initialize previous health tracking
	for (int i = 0; i < VOICE_MAX_PLAYERS+1; i++)
	{
		m_PreviousHealth[i] = 0;
		
		// Initialize all damage number slots
		for (int j = 0; j < MAX_DAMAGE_NUMBERS; j++)
		{
			m_DamageNumbers[i][j].active = false;
		}
	}
}

int CHudLifeBar::Init(void)
{
	HOOK_MESSAGE(LifeBar);

	m_iFlags = HUD_ACTIVE;  // Always active
	gHUD.AddHudElem(this);
	return 1;
}

void CHudLifeBar::Reset( void )
{
	// Reset all damage numbers
	for (int i = 0; i < VOICE_MAX_PLAYERS+1; i++)
	{
		m_PreviousHealth[i] = 0;
		
		for (int j = 0; j < MAX_DAMAGE_NUMBERS; j++)
		{
			m_DamageNumbers[i][j].active = false;
		}
	}
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
	int health = pClient ? pClient->curstate.health : 0;
	
	// Initialize previous health if not yet set (first time tracking this player)
	int prevHealth = GetLifeBar()->m_PreviousHealth[index];
	if (prevHealth == 0 && health > 0)
	{
		prevHealth = health;
	}
	
	// Detect damage (health decreased)
	if (prevHealth > health && prevHealth > 0 && health > 0)
	{
		int damageTaken = prevHealth - health;
		if (cl_lifemeter && cl_lifemeter->value > 1)
		{
			if (cl_lifemeter->value > 2)
				damageTaken = health; // Show current health instead of damage taken
			GetLifeBar()->AddDamageNumber(index, damageTaken);
		}
	}
	
	// Update previous health
	GetLifeBar()->m_PreviousHealth[index] = health;

	if (cl_playpoint && cl_playpoint->value)
	{
		if (health > 0 && index != iLocalPlayerIndex && iLocalPlayerIndex == hitindex)
		{
			PlaySound("ding.wav", 2);
			// gEngfuncs.Con_DPrintf(">>>>> ding Ding DING!\n");
		}
	}

	return 1;
}

extern bool IsPropHunt( void );

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

		if (IsPropHunt() && pClient && pClient->curstate.fuser4 > 0)
			return 0;
		
		// Initialize previous health for ALL visible players (before any other checks)
		// This ensures we track health even before the first lifebar message arrives
		if (pClient && pClient->player && !(pClient->curstate.effects & EF_NODRAW))
		{
			int health = pClient->curstate.health;
			if (m_PreviousHealth[i+1] == 0 && health > 0)
			{
				m_PreviousHealth[i+1] = health;
			}
		}
		
		if (m_LifeBarData[i+1].refreshTime < gEngfuncs.GetClientTime())
			continue;
		
		// Don't show an icon if the player is not in our PVS.
		if (!pClient || pClient->curstate.messagenum < localPlayer->curstate.messagenum)
			continue;

		int health = pClient->curstate.health;
		// int armor = m_LifeBarData[i+1].armor;

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

		if (MutatorEnabled(MUTATOR_MINIME))
			pEnt->origin[2] = 18;
		else
			pEnt->origin[2] = 45;

		VectorAdd(pEnt->origin, pClient->origin, pEnt->origin);

		if (cl_lifemeter && cl_lifemeter->value >= 1)
			gEngfuncs.CL_CreateVisibleEntity(ET_NORMAL, pEnt);
	}

	return 1;
}

int CHudLifeBar::Draw(float flTime)
{
	if (cl_lifemeter && !cl_lifemeter->value)
		return 0;

	// Render damage numbers during HUD draw phase
	RenderDamageNumbers();

	return 1;
}

void CHudLifeBar::AddDamageNumber(int playerIndex, int damage)
{
	cl_entity_s *pClient = gEngfuncs.GetEntityByIndex(playerIndex);
	if (!pClient)
		return;
	
	// Find an available slot
	for (int i = 0; i < MAX_DAMAGE_NUMBERS; i++)
	{
		if (playerIndex < 1 || playerIndex > VOICE_MAX_PLAYERS)
			break;

		if (!m_DamageNumbers[playerIndex][i].active)
		{
			m_DamageNumbers[playerIndex][i].damage = damage;
			m_DamageNumbers[playerIndex][i].spawnTime = gEngfuncs.GetClientTime();
			m_DamageNumbers[playerIndex][i].lifetime = 2.5f;  // longer so arc is visible
			m_DamageNumbers[playerIndex][i].active = true;
			
			// Store the world position where damage occurred (with random offset and above head)
			m_DamageNumbers[playerIndex][i].worldPosition[0] = pClient->origin[0] + gEngfuncs.pfnRandomFloat(-20, 20);
			m_DamageNumbers[playerIndex][i].worldPosition[1] = pClient->origin[1] + gEngfuncs.pfnRandomFloat(-20, 20);
			m_DamageNumbers[playerIndex][i].worldPosition[2] = pClient->origin[2] + gEngfuncs.pfnRandomFloat(40, 45);  // Above player's head
			
			// Random horizontal arc: pick a direction angle and a small drift speed
			float horizAngle = gEngfuncs.pfnRandomFloat(0.0f, 2.0f * 3.14159265f);
			float horizSpeed = gEngfuncs.pfnRandomFloat(15.0f, 35.0f);
			m_DamageNumbers[playerIndex][i].horizVelX = cosf(horizAngle) * horizSpeed;
			m_DamageNumbers[playerIndex][i].horizVelY = sinf(horizAngle) * horizSpeed;
			
			break;
		}
	}
}

void CHudLifeBar::RenderDamageNumbers()
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	float currentTime = gEngfuncs.GetClientTime();
	
	int entityIndex = 0;  // Track which entity slot we're using
	
	for (int i = 0; i < 32; i++)
	{
		cl_entity_s *pClient = gEngfuncs.GetEntityByIndex(i+1);
		
		if (!pClient || pClient->curstate.effects & EF_NODRAW)
			continue;

		// Don't render for the local player in first person (unless in thirdperson mode)
		if (localPlayer == pClient && !MutatorEnabled(MUTATOR_THIRDPERSON))
			continue;

		// Don't show an icon if the player is not in our PVS.
		if (pClient->curstate.messagenum < localPlayer->curstate.messagenum)
			continue;
			
		for (int j = 0; j < MAX_DAMAGE_NUMBERS; j++)
		{
			s_DamageNumber &dmg = m_DamageNumbers[i+1][j];
			
			if (!dmg.active)
				continue;
			
			float elapsed = currentTime - dmg.spawnTime;
			
			// Remove expired damage numbers
			if (elapsed > dmg.lifetime)
			{
				dmg.active = false;
				continue;
			}
			
			// Calculate fade effect (fade out over time)
			float fadeRatio = 1.0f - (elapsed / dmg.lifetime);
			int alpha = (int)(255 * fadeRatio);
			
			// Ballistic arc: rise then fall under gravity
			// floatOffset = v0*t - 0.5*g*t^2  (world units)
			const float kInitVel  = 60.0f;  // upward launch velocity (units/sec)
			const float kGravity  = 120.0f;  // downward acceleration (units/sec^2)
			float floatOffset = kInitVel * elapsed - 0.5f * kGravity * elapsed * elapsed;
			
			// Horizontal drift: simple linear (no gravity on horizontal)
			float horizOffsetX = dmg.horizVelX * elapsed;
			float horizOffsetY = dmg.horizVelY * elapsed;

			// Render each digit of the damage number using locked world position
			RenderDamageDigits(dmg.damage, dmg.worldPosition, floatOffset, horizOffsetX, horizOffsetY, alpha);
		}
	}
}

void CHudLifeBar::RenderDamageDigits(int damage, vec3_t worldPosition, float floatOffset, float horizOffsetX, float horizOffsetY, int alpha)
{
	char damageStr[16];
	snprintf(damageStr, sizeof(damageStr), "%d", damage);  // digits only, no "-"
	int numDigits = strlen(damageStr);
	
	// Use the locked world position with ballistic and horizontal arc offsets
	vec3_t worldPos;
	worldPos[0] = worldPosition[0] + horizOffsetX;
	worldPos[1] = worldPosition[1] + horizOffsetY;
	worldPos[2] = worldPosition[2] + floatOffset;
	
	// Convert world position to screen coordinates
	vec3_t screen;
	if (gEngfuncs.pTriAPI->WorldToScreen(worldPos, screen))
		return;  // Behind camera or off screen
	
	// Convert normalized screen coords (-1 to 1) to pixel coords
	int screenX = (int)((1.0f + screen[0]) * ScreenWidth * 0.5f);
	int screenY = (int)((1.0f - screen[1]) * ScreenHeight * 0.5f);
	
	// Get digit width from HUD number sprites
	int iWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;
	int iHeight = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).bottom - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).top;
	
	// Scaling factor
	float scale = ScreenWidth <= 768 ? 1.0f : 1.5f;
	int scaledWidth = (int)(iWidth * scale);
	int scaledHeight = (int)(iHeight * scale);

	// Clamp rendered size beyond a distance threshold so numbers don't dominate at range
	const float kSizeClampDist = 350.0f;  // units; numbers are full size up to this distance
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	float dx = worldPos[0] - localPlayer->origin[0];
	float dy = worldPos[1] - localPlayer->origin[1];
	float dz = worldPos[2] - localPlayer->origin[2];
	float dist = sqrtf(dx*dx + dy*dy + dz*dz);
	if (dist > kSizeClampDist)
	{
		float clampFactor = kSizeClampDist / dist;
		scaledWidth  = (int)(scaledWidth  * clampFactor);
		scaledHeight = (int)(scaledHeight * clampFactor);
	}

	// Minus sign dimensions: proportional to digit size
	int minusW   = (int)(scaledWidth  * 0.55f);  // slightly narrower than a digit
	int minusH   = (int)(scaledHeight * 0.12f);  // thin horizontal bar
	int minusGap = (int)(scaledWidth  * 0.05f);  // gap between minus and first digit

	// Center the whole group (minus + gap + digits) around screenX
	int totalWidth  = minusW + minusGap + (numDigits * scaledWidth);
	int startX      = screenX - totalWidth / 2;
	int minusY      = screenY + (scaledHeight - minusH) / 2;  // vertically centered on digits

	// Draw minus sign as a filled rectangle
	if (cl_lifemeter && cl_lifemeter->value == 2)
		gEngfuncs.pfnFillRGBA(startX, minusY, minusW, minusH, 255, 255, 255, alpha);

	int digitStartX = startX + minusW + minusGap;

	// Calculate color with alpha
	int r = (int)(255 * (alpha / 255.0f));
	int g = (int)(255 * (alpha / 255.0f));
	int b = (int)(255 * (alpha / 255.0f));
	
	// Draw each digit using 2D sprite rendering
	for (int i = 0; i < numDigits; i++)
	{
		int digit = damageStr[i] - '0';
		
		// Safety check: ensure digit is valid
		if (digit < 0 || digit > 9)
			continue;
		
		HSPRITE hSprite = gHUD.GetSprite(gHUD.m_HUD_number_0 + digit);
		if (!hSprite)
			continue;
		
		wrect_t rect = gHUD.GetSpriteRect(gHUD.m_HUD_number_0 + digit);
		
		// Manually draw scaled sprite using TriAPI
		SPR_Set(hSprite, r, g, b);
		
		// Get sprite model
		model_s* pModel = (model_s*)gEngfuncs.GetSpritePointer(hSprite);
		if (!pModel)
			continue;
		
		// Get the actual texture dimensions from the sprite
		int spriteSheetWidth = SPR_Width(hSprite, 0);
		int spriteSheetHeight = SPR_Height(hSprite, 0);
		
		if (spriteSheetWidth == 0 || spriteSheetHeight == 0)
			continue;
		
		// Calculate texture coordinates using the rect and actual texture size
		float s1 = (float)rect.left / (float)spriteSheetWidth;
		float t1 = (float)rect.top / (float)spriteSheetHeight;
		float s2 = (float)rect.right / (float)spriteSheetWidth;
		float t2 = (float)rect.bottom / (float)spriteSheetHeight;
		
		// Draw scaled quad starting after the minus sign
		int drawX = digitStartX + (i * scaledWidth);
		int drawY = screenY;
		
		gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
		gEngfuncs.pTriAPI->SpriteTexture(pModel, 0);
		gEngfuncs.pTriAPI->Color4ub(r, g, b, alpha);
		
		gEngfuncs.pTriAPI->Begin(TRI_QUADS);
		
		gEngfuncs.pTriAPI->TexCoord2f(s1, t1);
		gEngfuncs.pTriAPI->Vertex3f(drawX, drawY, 0);
		
		gEngfuncs.pTriAPI->TexCoord2f(s2, t1);
		gEngfuncs.pTriAPI->Vertex3f(drawX + scaledWidth, drawY, 0);
		
		gEngfuncs.pTriAPI->TexCoord2f(s2, t2);
		gEngfuncs.pTriAPI->Vertex3f(drawX + scaledWidth, drawY + scaledHeight, 0);
		
		gEngfuncs.pTriAPI->TexCoord2f(s1, t2);
		gEngfuncs.pTriAPI->Vertex3f(drawX, drawY + scaledHeight, 0);
		
		gEngfuncs.pTriAPI->End();
		gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
	}
}
