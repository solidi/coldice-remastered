/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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
/*

===== globals.cpp ========================================================

  DLL-wide global variable definitions.
  They're all defined here, for convenient centralization.
  Source files that need them should "extern ..." declare each
  variable, to better document what globals they care about.

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "soundent.h"

DLL_GLOBAL ULONG		g_ulFrameCount;
DLL_GLOBAL ULONG		g_ulModelIndexEyes;
DLL_GLOBAL ULONG		g_ulModelIndexPlayer;
DLL_GLOBAL Vector		g_vecAttackDir;
DLL_GLOBAL int			g_iSkillLevel;
DLL_GLOBAL int			gDisplayTitle;
DLL_GLOBAL BOOL			g_fGameOver;
DLL_GLOBAL const Vector	g_vecZero = Vector(0,0,0);
DLL_GLOBAL int			g_Language;

extern const char *g_szMutators;

DLL_GLOBAL const char *g_MutatorChaos = "chaos";
DLL_GLOBAL const char *g_MutatorRocketCrowbar = "rocketcrowbar";
DLL_GLOBAL const char *g_MutatorInstaGib = "instagib";
DLL_GLOBAL const char *g_MutatorVolatile = "volatile";
DLL_GLOBAL const char *g_MutatorPaintball = "paintball";
DLL_GLOBAL const char *g_MutatorPlumber = "plumber";
DLL_GLOBAL const char *g_MutatorDKMode = "dkmode";
DLL_GLOBAL const char *g_MutatorSuperJump = "superjump";
DLL_GLOBAL const char *g_MutatorMegaSpeed = "megarun";
DLL_GLOBAL const char *g_MutatorLightsOut = "lightsout";
DLL_GLOBAL const char *g_MutatorSlowmo = "slowmo";
DLL_GLOBAL const char *g_MutatorIce = "ice";
DLL_GLOBAL const char *g_MutatorTopsyTurvy = "topsyturvy";
DLL_GLOBAL const char *g_MutatorTurrets = "turrets";
DLL_GLOBAL const char *g_MutatorBarrels = "barrels";
DLL_GLOBAL const char *g_MutatorChumXplode = "chumxplode";
DLL_GLOBAL const char *g_MutatorSantaHat = "santahat";
DLL_GLOBAL const char *g_MutatorSanic = "sanic";
DLL_GLOBAL const char *g_MutatorCoolFlesh = "coolflesh";
DLL_GLOBAL const char *g_MutatorLoopback = "loopback";
DLL_GLOBAL const char *g_MutatorMaxPack = "maxpack";
DLL_GLOBAL const char *g_MutatorInfiniteAmmo = "infiniteammo";
DLL_GLOBAL const char *g_MutatorRandomWeapon = "randomweapon";
DLL_GLOBAL const char *g_MutatorSpeedUp = "speedup";
DLL_GLOBAL const char *g_MutatorRockets = "rockets";
DLL_GLOBAL const char *g_MutatorInvisible = "invisible";
DLL_GLOBAL const char *g_MutatorGrenades = "grenades";
DLL_GLOBAL const char *g_MutatorGravity = "astronaut";
DLL_GLOBAL const char *g_MutatorSnowball = "snowballs";
DLL_GLOBAL const char *g_MutatorPushy = "pushy";

DLL_GLOBAL int g_GameMode;
