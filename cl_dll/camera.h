//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Camera.h  --  defines and such for a 3rd person camera
// NOTE: must include quakedef.h first

#ifndef _CAMERA_H_
#define _CAMEA_H_

// pitch, yaw, dist
extern vec3_t cam_ofs;
// Using third person camera
extern int cam_thirdperson;

void CAM_Init( void );
void CAM_ClearStates( void );
void CAM_StartMouseMove(void);
void CAM_EndMouseMove(void);
void CAM_ToThirdPerson(void);
void CAM_ToFirstPerson(void);

// Third person body fade: the controlled player's own model turns see-through
// while the camera is pulled in close. Local rendering only, never networked.
struct cl_entity_s;
void CAM_UpdateBodyFade( float camDist );
void CAM_ResetBodyFade( void );
void CAM_ApplyBodyFade( int type, struct cl_entity_s *ent );
void CAM_RestoreBodyFade( void );
bool CAM_GetBodyFade( const struct cl_entity_s *ent, float *alpha, int *serverMode, int *serverAmt );

#endif		// _CAMERA_H_
