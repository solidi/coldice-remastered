//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "camera.h"
#include "kbutton.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "Exports.h"
#include "pmtrace.h"

#include "event_api.h"
#include "pm_defs.h"
#include "cl_util.h"

#define VectorSubtract(a,b,c) {(c)[0]=(a)[0]-(b)[0];(c)[1]=(a)[1]-(b)[1];(c)[2]=(a)[2]-(b)[2];}



#include "SDL2/SDL_mouse.h"
#include "port.h"

float CL_KeyState (kbutton_t *key);

extern cl_enginefunc_t gEngfuncs;

//-------------------------------------------------- Constants

#define CAM_DIST_DELTA 1.0
#define CAM_ANGLE_DELTA 2.5
#define CAM_ANGLE_SPEED 2.5
#define CAM_MIN_DIST 30.0
#define CAM_ANGLE_MOVE .5
#define MAX_ANGLE_DIFF 10.0
#define PITCH_MAX 90.0
#define PITCH_MIN 0
#define YAW_MAX  135.0
#define YAW_MIN	 -135.0

enum ECAM_Command
{
	CAM_COMMAND_NONE = 0,
	CAM_COMMAND_TOTHIRDPERSON = 1,
	CAM_COMMAND_TOFIRSTPERSON = 2
};

//-------------------------------------------------- Global Variables

cvar_t	*cam_command;
cvar_t	*cam_snapto;
cvar_t	*cam_idealyaw;
cvar_t	*cam_idealpitch;
cvar_t	*cam_idealdist;
cvar_t	*cam_contain;

cvar_t	*c_maxpitch;
cvar_t	*c_minpitch;
cvar_t	*c_maxyaw;
cvar_t	*c_minyaw;
cvar_t	*c_maxdistance;
cvar_t	*c_mindistance;

// pitch, yaw, dist
vec3_t cam_ofs;

extern cl_enginefunc_t gEngfuncs;


// In third person
int cam_thirdperson;
int cam_mousemove; //true if we are moving the cam with the mouse, False if not
int iMouseInUse=0;
int cam_distancemove;
extern int mouse_x, mouse_y;  //used to determine what the current x and y values are
int cam_old_mouse_x, cam_old_mouse_y; //holds the last ticks mouse movement
POINT		cam_mouse;
//-------------------------------------------------- Local Variables

static kbutton_t cam_pitchup, cam_pitchdown, cam_yawleft, cam_yawright;
static kbutton_t cam_in, cam_out, cam_move;

//-------------------------------------------------- Prototypes

void CAM_ToThirdPerson(void);
void CAM_ToFirstPerson(void);
void CAM_StartDistance(void);
void CAM_EndDistance(void);

void SDL_GetCursorPos( POINT *p )
{
	gEngfuncs.GetMousePosition( (int *)&p->x, (int *)&p->y );
//	SDL_GetMouseState( (int *)&p->x, (int *)&p->y );
}

void SDL_SetCursorPos( const int x, const int y )
{
}

//-------------------------------------------------- Local Functions

float MoveToward( float cur, float goal, float maxspeed )
{
	if( cur != goal )
	{
		if( fabs( cur - goal ) > 180.0 )
		{
			if( cur < goal )
				cur += 360.0;
			else
				cur -= 360.0;
		}

		if( cur < goal )
		{
			if( cur < goal - 1.0 )
				cur += ( goal - cur ) / 4.0;
			else
				cur = goal;
		}
		else
		{
			if( cur > goal + 1.0 )
				cur -= ( cur - goal ) / 4.0;
			else
				cur = goal;
		}
	}


	// bring cur back into range
	if( cur < 0 )
		cur += 360.0;
	else if( cur >= 360 )
		cur -= 360;

	return cur;
}


//-------------------------------------------------- Gobal Functions

typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	float		*mins, *maxs;	// size of the moving object
	vec3_t		mins2, maxs2;	// size when clipping against mosnters
	float		*start, *end;
	trace_t		trace;
	int			type;
	edict_t		*passedict;
	qboolean	monsterclip;
} moveclip_t;

extern trace_t SV_ClipMoveToEntity (edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end);

extern ref_params_t g_ViewParams;

void CL_DLLEXPORT CAM_Think( void )
{
//	RecClCamThink();
	Vector vecDir;
	vec3_t camForward, camRight, camUp;
	pmtrace_t tr;
	float dist;
	vec3_t camAngles;
	float flSensitivity;
	vec3_t viewangles;

	Vector vieworg = g_ViewParams.simorg;
	VectorAdd(vieworg, g_ViewParams.viewheight, vieworg);

	switch( (int) cam_command->value )
	{
		case CAM_COMMAND_TOTHIRDPERSON:
			CAM_ToThirdPerson();
			break;

		case CAM_COMMAND_TOFIRSTPERSON:
			CAM_ToFirstPerson();
			break;

		case CAM_COMMAND_NONE:
		default:
			break;
	}

	if( !cam_thirdperson )
		return;

	camAngles[ PITCH ] = cam_idealpitch->value;
	camAngles[ YAW ] = cam_idealyaw->value;
	dist = cam_idealdist->value;
	//
	//movement of the camera with the mouse
	//
	if (cam_mousemove)
	{
	    //get windows cursor position
		SDL_GetCursorPos (&cam_mouse);
		//check for X delta values and adjust accordingly
		//eventually adjust YAW based on amount of movement
	  //don't do any movement of the cam using YAW/PITCH if we are zooming in/out the camera	
	  if (!cam_distancemove)
	  {
		
		//keep the camera within certain limits around the player (ie avoid certain bad viewing angles)  
		if (cam_mouse.x>gEngfuncs.GetWindowCenterX())
		{
			//if ((camAngles[YAW]>=225.0)||(camAngles[YAW]<135.0))
			if (camAngles[YAW]<c_maxyaw->value)
			{
				camAngles[ YAW ] += (CAM_ANGLE_MOVE)*((cam_mouse.x-gEngfuncs.GetWindowCenterX())/2);
			}
			if (camAngles[YAW]>c_maxyaw->value)
			{
				
				camAngles[YAW]=c_maxyaw->value;
			}
		}
		else if (cam_mouse.x<gEngfuncs.GetWindowCenterX())
		{
			//if ((camAngles[YAW]<=135.0)||(camAngles[YAW]>225.0))
			if (camAngles[YAW]>c_minyaw->value)
			{
			   camAngles[ YAW ] -= (CAM_ANGLE_MOVE)* ((gEngfuncs.GetWindowCenterX()-cam_mouse.x)/2);
			   	
			}
			if (camAngles[YAW]<c_minyaw->value)
			{
				camAngles[YAW]=c_minyaw->value;
				
			}
		}

		//check for y delta values and adjust accordingly
		//eventually adjust PITCH based on amount of movement
		//also make sure camera is within bounds
		if (cam_mouse.y>gEngfuncs.GetWindowCenterY())
		{
			if(camAngles[PITCH]<c_maxpitch->value)
			{
			    camAngles[PITCH] +=(CAM_ANGLE_MOVE)* ((cam_mouse.y-gEngfuncs.GetWindowCenterY())/2);
			}
			if (camAngles[PITCH]>c_maxpitch->value)
			{
				camAngles[PITCH]=c_maxpitch->value;
			}
		}
		else if (cam_mouse.y<gEngfuncs.GetWindowCenterY())
		{
			if (camAngles[PITCH]>c_minpitch->value)
			{
			   camAngles[PITCH] -= (CAM_ANGLE_MOVE)*((gEngfuncs.GetWindowCenterY()-cam_mouse.y)/2);
			}
			if (camAngles[PITCH]<c_minpitch->value)
			{
				camAngles[PITCH]=c_minpitch->value;
			}
		}

		//set old mouse coordinates to current mouse coordinates
		//since we are done with the mouse

		if ( ( flSensitivity = gHUD.GetSensitivity() ) != 0 )
		{
			cam_old_mouse_x=cam_mouse.x*flSensitivity;
			cam_old_mouse_y=cam_mouse.y*flSensitivity;
		}
		else
		{
			cam_old_mouse_x=cam_mouse.x;
			cam_old_mouse_y=cam_mouse.y;
		}
		SDL_SetCursorPos (gEngfuncs.GetWindowCenterX(), gEngfuncs.GetWindowCenterY());
	  }
	}

	//Nathan code here
	if( CL_KeyState( &cam_pitchup ) )
		camAngles[ PITCH ] += CAM_ANGLE_DELTA;
	else if( CL_KeyState( &cam_pitchdown ) )
		camAngles[ PITCH ] -= CAM_ANGLE_DELTA;

	if( CL_KeyState( &cam_yawleft ) )
		camAngles[ YAW ] -= CAM_ANGLE_DELTA;
	else if( CL_KeyState( &cam_yawright ) )
		camAngles[ YAW ] += CAM_ANGLE_DELTA;

	if( CL_KeyState( &cam_in ) )
	{
		dist -= CAM_DIST_DELTA;
		if( dist < CAM_MIN_DIST )
		{
			// If we go back into first person, reset the angle
			camAngles[ PITCH ] = 0;
			camAngles[ YAW ] = 0;
			dist = CAM_MIN_DIST;
		}

	}
	else if( CL_KeyState( &cam_out ) )
		dist += CAM_DIST_DELTA;

	if (cam_distancemove)
	{
		if (cam_mouse.y>gEngfuncs.GetWindowCenterY())
		{
			if(dist<c_maxdistance->value)
			{
			    dist +=CAM_DIST_DELTA * ((cam_mouse.y-gEngfuncs.GetWindowCenterY())/2);
			}
			if (dist>c_maxdistance->value)
			{
				dist=c_maxdistance->value;
			}
		}
		else if (cam_mouse.y<gEngfuncs.GetWindowCenterY())
		{
			if (dist>c_mindistance->value)
			{
			   dist -= (CAM_DIST_DELTA)*((gEngfuncs.GetWindowCenterY()-cam_mouse.y)/2);
			}
			if (dist<c_mindistance->value)
			{
				dist=c_mindistance->value;
			}
		}
		//set old mouse coordinates to current mouse coordinates
		//since we are done with the mouse
		cam_old_mouse_x=cam_mouse.x*gHUD.GetSensitivity();
		cam_old_mouse_y=cam_mouse.y*gHUD.GetSensitivity();
		SDL_SetCursorPos (gEngfuncs.GetWindowCenterX(), gEngfuncs.GetWindowCenterY());
	}

	// update ideal
	cam_idealpitch->value = camAngles[ PITCH ];
	cam_idealyaw->value = camAngles[ YAW ];
	cam_idealdist->value = dist;

	// Move towards ideal
	VectorCopy( cam_ofs, camAngles );
	AngleVectors(camAngles, camForward, camRight, camUp);

	gEngfuncs.GetViewAngles( (float *)viewangles );

	if( cam_snapto->value )
	{
		camAngles[ YAW ] = cam_idealyaw->value + viewangles[ YAW ];
		camAngles[ PITCH ] = cam_idealpitch->value + viewangles[ PITCH ];
		camAngles[ 2 ] = cam_idealdist->value;
	}
	else
	{
		if( camAngles[ YAW ] - viewangles[ YAW ] != cam_idealyaw->value )
			camAngles[ YAW ] = MoveToward( camAngles[ YAW ], cam_idealyaw->value + viewangles[ YAW ], CAM_ANGLE_SPEED );

		if( camAngles[ PITCH ] - viewangles[ PITCH ] != cam_idealpitch->value )
			camAngles[ PITCH ] = MoveToward( camAngles[ PITCH ], cam_idealpitch->value + viewangles[ PITCH ], CAM_ANGLE_SPEED );

		if( abs( camAngles[ 2 ] - cam_idealdist->value ) < 2.0 )
			camAngles[ 2 ] = cam_idealdist->value;
		else
			camAngles[ 2 ] += ( cam_idealdist->value - camAngles[ 2 ] ) / 4.0;
	}

	cam_ofs[ 0 ] = camAngles[ 0 ];
	cam_ofs[ 1 ] = camAngles[ 1 ];

	if (gHUD.local_player_index && !g_iUser1)
	{
		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);

		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();

		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(gHUD.local_player_index - 1);

		gEngfuncs.pEventAPI->EV_SetTraceHull(2);

		tr.fraction = 0.0f;

		float prevDist = dist;

		while (tr.fraction != 1.0f)
		{
			vecDir = -(dist * 1.5f) * camForward;
			gEngfuncs.pEventAPI->EV_PlayerTrace(vieworg, vieworg + vecDir, PM_WORLD_ONLY, -1, &tr);
			if (tr.fraction != 1.0f)
			{
				dist *= 0.98f;
			}
		}

		gEngfuncs.pEventAPI->EV_PopPMStates();
	}

	cam_ofs[ 2 ] = dist;
}

extern void KeyDown (kbutton_t *b);	// HACK
extern void KeyUp (kbutton_t *b);	// HACK

void CAM_PitchUpDown(void) { KeyDown( &cam_pitchup ); }
void CAM_PitchUpUp(void) { KeyUp( &cam_pitchup ); }
void CAM_PitchDownDown(void) { KeyDown( &cam_pitchdown ); }
void CAM_PitchDownUp(void) { KeyUp( &cam_pitchdown ); }
void CAM_YawLeftDown(void) { KeyDown( &cam_yawleft ); }
void CAM_YawLeftUp(void) { KeyUp( &cam_yawleft ); }
void CAM_YawRightDown(void) { KeyDown( &cam_yawright ); }
void CAM_YawRightUp(void) { KeyUp( &cam_yawright ); }
void CAM_InDown(void) { KeyDown( &cam_in ); }
void CAM_InUp(void) { KeyUp( &cam_in ); }
void CAM_OutDown(void) { KeyDown( &cam_out ); }
void CAM_OutUp(void) { KeyUp( &cam_out ); }

void CAM_ToThirdPerson(void)
{ 
	vec3_t viewangles;

	if (!MutatorEnabled(MUTATOR_THIRDPERSON))
	{
		gEngfuncs.Con_Printf("Mutator \"thirdperson\" must be enabled by the server.\n");
		return;
	}

	gEngfuncs.GetViewAngles( (float *)viewangles );

	if( !cam_thirdperson )
	{
		cam_thirdperson = 1; 
		
		cam_ofs[ YAW ] = viewangles[ YAW ]; 
		cam_ofs[ PITCH ] = viewangles[ PITCH ]; 
		cam_ofs[ 2 ] = CAM_MIN_DIST; 
	}

	gEngfuncs.Cvar_SetValue( "cam_command", 0 );
}

void CAM_ToFirstPerson(void) 
{ 
	cam_thirdperson = 0;
	
	gEngfuncs.Cvar_SetValue( "cam_command", 0 );
}

void CAM_ToggleSnapto( void )
{ 
	cam_snapto->value = !cam_snapto->value;
}

void CAM_Init( void )
{
	gEngfuncs.pfnAddCommand( "+campitchup", CAM_PitchUpDown );
	gEngfuncs.pfnAddCommand( "-campitchup", CAM_PitchUpUp );
	gEngfuncs.pfnAddCommand( "+campitchdown", CAM_PitchDownDown );
	gEngfuncs.pfnAddCommand( "-campitchdown", CAM_PitchDownUp );
	gEngfuncs.pfnAddCommand( "+camyawleft", CAM_YawLeftDown );
	gEngfuncs.pfnAddCommand( "-camyawleft", CAM_YawLeftUp );
	gEngfuncs.pfnAddCommand( "+camyawright", CAM_YawRightDown );
	gEngfuncs.pfnAddCommand( "-camyawright", CAM_YawRightUp );
	gEngfuncs.pfnAddCommand( "+camin", CAM_InDown );
	gEngfuncs.pfnAddCommand( "-camin", CAM_InUp );
	gEngfuncs.pfnAddCommand( "+camout", CAM_OutDown );
	gEngfuncs.pfnAddCommand( "-camout", CAM_OutUp );
	gEngfuncs.pfnAddCommand( "thirdperson", CAM_ToThirdPerson );
	gEngfuncs.pfnAddCommand( "firstperson", CAM_ToFirstPerson );
	gEngfuncs.pfnAddCommand( "+cammousemove",CAM_StartMouseMove);
	gEngfuncs.pfnAddCommand( "-cammousemove",CAM_EndMouseMove);
	gEngfuncs.pfnAddCommand( "+camdistance", CAM_StartDistance );
	gEngfuncs.pfnAddCommand( "-camdistance", CAM_EndDistance );
	gEngfuncs.pfnAddCommand( "snapto", CAM_ToggleSnapto );

	cam_command				= gEngfuncs.pfnRegisterVariable ( "cam_command", "0", 0 );	 // tells camera to go to thirdperson
	cam_snapto				= gEngfuncs.pfnRegisterVariable ( "cam_snapto", "0", 0 );	 // snap to thirdperson view
	cam_idealyaw			= gEngfuncs.pfnRegisterVariable ( "cam_idealyaw", "0", 0 );	 // thirdperson yaw
	cam_idealpitch			= gEngfuncs.pfnRegisterVariable ( "cam_idealpitch", "0", 0 );	 // thirperson pitch
	cam_idealdist			= gEngfuncs.pfnRegisterVariable ( "cam_idealdist", "60", 0 );	 // thirdperson distance
	cam_contain				= gEngfuncs.pfnRegisterVariable ( "cam_contain", "0", 0 );	// contain camera to world

	c_maxpitch				= gEngfuncs.pfnRegisterVariable ( "c_maxpitch", "90.0", 0 );
	c_minpitch				= gEngfuncs.pfnRegisterVariable ( "c_minpitch", "0.0", 0 );
	c_maxyaw				= gEngfuncs.pfnRegisterVariable ( "c_maxyaw",   "135.0", 0 );
	c_minyaw				= gEngfuncs.pfnRegisterVariable ( "c_minyaw",   "-135.0", 0 );
	c_maxdistance			= gEngfuncs.pfnRegisterVariable ( "c_maxdistance",   "200.0", 0 );
	c_mindistance			= gEngfuncs.pfnRegisterVariable ( "c_mindistance",   "30.0", 0 );
}

void CAM_ClearStates( void )
{
	vec3_t viewangles;

	gEngfuncs.GetViewAngles( (float *)viewangles );

	cam_pitchup.state = 0;
	cam_pitchdown.state = 0;
	cam_yawleft.state = 0;
	cam_yawright.state = 0;
	cam_in.state = 0;
	cam_out.state = 0;

	cam_thirdperson = 0;
	cam_command->value = 0;
	cam_mousemove=0;

	cam_snapto->value = 0;
	cam_distancemove = 0;

	cam_ofs[ 0 ] = 0.0;
	cam_ofs[ 1 ] = 0.0;
	cam_ofs[ 2 ] = CAM_MIN_DIST;

	cam_idealpitch->value = viewangles[ PITCH ];
	cam_idealyaw->value = viewangles[ YAW ];
	cam_idealdist->value = CAM_MIN_DIST;
}

void CAM_StartMouseMove(void)
{
	float flSensitivity;
		
	//only move the cam with mouse if we are in third person.
	if (cam_thirdperson)
	{
		//set appropriate flags and initialize the old mouse position
		//variables for mouse camera movement
		if (!cam_mousemove)
		{
			cam_mousemove=1;
			iMouseInUse=1;
			SDL_GetCursorPos (&cam_mouse);

			if ( ( flSensitivity = gHUD.GetSensitivity() ) != 0 )
			{
				cam_old_mouse_x=cam_mouse.x*flSensitivity;
				cam_old_mouse_y=cam_mouse.y*flSensitivity;
			}
			else
			{
				cam_old_mouse_x=cam_mouse.x;
				cam_old_mouse_y=cam_mouse.y;
			}
		}
	}
	//we are not in 3rd person view..therefore do not allow camera movement
	else
	{   
		cam_mousemove=0;
		iMouseInUse=0;
	}
}

//the key has been released for camera movement
//tell the engine that mouse camera movement is off
void CAM_EndMouseMove(void)
{
   cam_mousemove=0;
   iMouseInUse=0;
}


//----------------------------------------------------------
//routines to start the process of moving the cam in or out 
//using the mouse
//----------------------------------------------------------
void CAM_StartDistance(void)
{
	//only move the cam with mouse if we are in third person.
	if (cam_thirdperson)
	{
	  //set appropriate flags and initialize the old mouse position
	  //variables for mouse camera movement
	  if (!cam_distancemove)
	  {
		  cam_distancemove=1;
		  cam_mousemove=1;
		  iMouseInUse=1;
		  SDL_GetCursorPos (&cam_mouse);
		  cam_old_mouse_x=cam_mouse.x*gHUD.GetSensitivity();
		  cam_old_mouse_y=cam_mouse.y*gHUD.GetSensitivity();
	  }
	}
	//we are not in 3rd person view..therefore do not allow camera movement
	else
	{   
		cam_distancemove=0;
		cam_mousemove=0;
		iMouseInUse=0;
	}
}

//the key has been released for camera movement
//tell the engine that mouse camera movement is off
void CAM_EndDistance(void)
{
   cam_distancemove=0;
   cam_mousemove=0;
   iMouseInUse=0;
}

int CL_DLLEXPORT CL_IsThirdPerson( void )
{
//	RecClCL_IsThirdPerson();

	return (cam_thirdperson ? 1 : 0) || (g_iUser1 && (g_iUser2 == gEngfuncs.GetLocalPlayer()->index) );
}

void CL_DLLEXPORT CL_CameraOffset( float *ofs )
{
//	RecClCL_GetCameraOffsets(ofs);

	VectorCopy( cam_ofs, ofs );
}
