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
// pm_shared.h
//
#if !defined( PM_SHAREDH )
#define PM_SHAREDH
#pragma once

void PM_Init( struct playermove_s *ppmove );
void PM_Move ( struct playermove_s *ppmove, int server );
char PM_FindTextureType( char *name );

// Spectator Movement modes (stored in pev->iuser1, so the physics code can get at them)
#define OBS_NONE				0
#define OBS_CHASE_LOCKED		1
#define OBS_CHASE_FREE			2
#define OBS_ROAMING				3		
#define OBS_IN_EYE				4
#define OBS_MAP_FREE			5
#define OBS_MAP_CHASE			6

#define OBS_UNDECIDED_SIMPLE	7
#define OBS_BEGIN				OBS_UNDECIDED_SIMPLE
#define OBS_UNDECIDED_BLUE		8
#define OBS_UNDECIDED_RED		9
#define OBS_UNDECIDED_BOTH		10
#define OBS_END					OBS_UNDECIDED_BOTH

#define OBS_MENU_1				0
#define OBS_MENU_2				1
#define OBS_MENU_3				2
#define OBS_MENU_4				3

// Movement mutator bits, sent to pm_shared in physinfo "mv"
#define MV_NOJUMP				(1<<0)
#define MV_NODUCK				(1<<1)
#define MV_DUCKED				(1<<2)
#define MV_FLY					(1<<3)
#define MV_AUTORUN				(1<<4)
#define MV_BACKWARDS			(1<<5)
#define MV_HEAVY				(1<<6)
#define MV_PLANTED				(1<<7)
#define MV_PINBALL				(1<<8)
#define MV_POGO					(1<<9)

// hitstun mutator: the server sets pev->maxspeed to this while a player is stunned (sent as clientdata maxspeed)
#define HITSTUN_MAXSPEED		1.0f

#endif
