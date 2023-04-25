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

#ifndef GAME_H
#define GAME_H

extern void GameDLLInit( void );


extern cvar_t	displaysoundlist;

// multiplayer server rules
extern cvar_t	teamplay;
extern cvar_t	fraglimit;
extern cvar_t	timelimit;
extern cvar_t	friendlyfire;
extern cvar_t	falldamage;
extern cvar_t	weaponstay;
extern cvar_t	forcerespawn;
extern cvar_t	flashlight;
extern cvar_t	aimcrosshair;
extern cvar_t	decalfrequency;
extern cvar_t	teamlist;
extern cvar_t	teamoverride;
extern cvar_t	defaultteam;
extern cvar_t	allowmonsters;

#if defined( GRAPPLING_HOOK )
extern cvar_t	grapplinghook;
extern cvar_t	grapplinghookdeploytime;
#endif

extern cvar_t	spawnweaponlist;
extern cvar_t	allowrunes;
extern cvar_t	holsterweapons;
extern cvar_t   floatingweapons;
extern cvar_t	iceblood;
extern cvar_t   infiniteammo;
extern cvar_t   moreblood;
extern cvar_t   startwithall;
extern cvar_t   allowvoiceovers;
extern cvar_t   dualsonly;
extern cvar_t   jumpheight;
extern cvar_t   icesprites;
extern cvar_t   randomweapon;
extern cvar_t   interactiveitems;
extern cvar_t   snowballfight;
extern cvar_t   mutators;
extern cvar_t   spawnweapons;
extern cvar_t   spawnitems;
extern cvar_t   disallowlist;
extern cvar_t   nukemode;
extern cvar_t   acrobatics;
extern cvar_t   weather;
extern cvar_t   randommutators;
extern cvar_t   chaostime;
extern cvar_t   gamemode;
extern cvar_t   randomgamemodes;
extern cvar_t   roundlimit;
extern cvar_t   roundtimelimit;
extern cvar_t   roundtimeleft;
extern cvar_t   startwithlives;
extern cvar_t   roundfraglimit;
extern cvar_t   ggstartlevel;
extern cvar_t   ctcsecondsforpoint;

// Engine Cvars
extern cvar_t	*g_psv_gravity;
extern cvar_t	*g_psv_aim;
extern cvar_t	*g_footsteps;

// Debug Cvars
#ifdef _DEBUG
extern cvar_t	debug_disconnects;
extern cvar_t	debug_disconnects_time;
#endif

#endif		// GAME_H
