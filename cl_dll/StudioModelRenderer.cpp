// studio_model.cpp
// routines for setting up to draw 3DStudio models

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"
#include "fog.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#include "studio_util.h"
#include "r_studioint.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"

#include "event_api.h"

#include "view.h"
#include "PlatformHeaders.h"

#ifndef __APPLE__
#define GL_CLAMP_TO_EDGE 0x812F
static GLuint g_iBlankTex = 0;
#endif

extern cvar_t *tfc_newmodels;
extern cvar_t *cl_icemodels;
extern cvar_t *cl_portalmirror;

extern extra_player_info_t  g_PlayerExtraInfo[MAX_PLAYERS+1];

// team colors for old TFC models
#define TEAM1_COLOR		150
#define TEAM2_COLOR		250
#define TEAM3_COLOR		45
#define TEAM4_COLOR		100

int m_nPlayerGaitSequences[MAX_CLIENTS];

// Global engine <-> studio model rendering code interface
engine_studio_api_t IEngineStudio;

#ifdef _WIN32
void (*GL_StudioDrawShadow)(void);

__declspec(naked) void ShadowHack(void)
{
    _asm
    {
        push ebp;
        mov ebp, esp;
        push ecx;
        jmp[GL_StudioDrawShadow];
    }
}
#endif

/////////////////////
// Implementation of CStudioModelRenderer.h

/*
====================
Init

====================
*/
void CStudioModelRenderer::Init( void )
{
	// Set up some variables shared with engine
	m_pCvarHiModels			= IEngineStudio.GetCvar( "cl_himodels" );
	m_pCvarDeveloper		= IEngineStudio.GetCvar( "developer" );
	m_pCvarDrawEntities		= IEngineStudio.GetCvar( "r_drawentities" );
	cl_icemodels			= gEngfuncs.pfnRegisterVariable ( "cl_icemodels", "2", FCVAR_ARCHIVE );
	m_pCvarGlowModels		= gEngfuncs.pfnRegisterVariable ( "cl_glowmodels", "1", FCVAR_ARCHIVE );

	m_pChromeSprite			= IEngineStudio.GetChromeSprite();

	IEngineStudio.GetModelCounters( &m_pStudioModelCount, &m_pModelsDrawn );

	// Get pointers to engine data structures
	m_pbonetransform		= (float (*)[MAXSTUDIOBONES][3][4])IEngineStudio.StudioGetBoneTransform();
	m_plighttransform		= (float (*)[MAXSTUDIOBONES][3][4])IEngineStudio.StudioGetLightTransform();
	m_paliastransform		= (float (*)[3][4])IEngineStudio.StudioGetAliasTransform();
	m_protationmatrix		= (float (*)[3][4])IEngineStudio.StudioGetRotationMatrix();

#ifdef _WIN32
	// Portal blank texture
	GLubyte pixels[3] = {0, 0, 0};

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(1, &g_iBlankTex);
	glBindTexture(GL_TEXTURE_2D, g_iBlankTex);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif
}

/*
====================
CStudioModelRenderer

====================
*/
CStudioModelRenderer::CStudioModelRenderer( void )
{
	m_fDoInterp			= 1;
	m_fGaitEstimation	= 1;
	m_pCurrentEntity	= NULL;
	m_pCvarHiModels		= NULL;
	m_pCvarDeveloper	= NULL;
	m_pCvarDrawEntities	= NULL;
	m_pChromeSprite		= NULL;
	m_pStudioModelCount	= NULL;
	m_pModelsDrawn		= NULL;
	m_protationmatrix	= NULL;
	m_paliastransform	= NULL;
	m_pbonetransform	= NULL;
	m_plighttransform	= NULL;
	m_pStudioHeader		= NULL;
	m_pBodyPart			= NULL;
	m_pSubModel			= NULL;
	m_pPlayerInfo		= NULL;
	m_pRenderModel		= NULL;
}

/*
====================
~CStudioModelRenderer

====================
*/
CStudioModelRenderer::~CStudioModelRenderer( void )
{
}

/*
====================
StudioCalcBoneAdj

====================
*/
void CStudioModelRenderer::StudioCalcBoneAdj( float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2, byte mouthopen )
{
	int					i, j;
	float				value;
	mstudiobonecontroller_t *pbonecontroller;
	
	pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	for (j = 0; j < m_pStudioHeader->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;
		if (i <= 3)
		{
			// check for 360% wrapping
			if (pbonecontroller[j].type & STUDIO_RLOOP)
			{
				if (abs(pcontroller1[i] - pcontroller2[i]) > 128)
				{
					int a, b;
					a = (pcontroller1[j] + 128) % 256;
					b = (pcontroller2[j] + 128) % 256;
					value = ((a * dadt) + (b * (1 - dadt)) - 128) * (360.0/256.0) + pbonecontroller[j].start;
				}
				else 
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0 - dadt))) * (360.0/256.0) + pbonecontroller[j].start;
				}
			}
			else 
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0 - dadt)) / 255.0;
				if (value < 0) value = 0;
				if (value > 1.0) value = 1.0;
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
			// Con_DPrintf( "%d %d %f : %f\n", m_pCurrentEntity->curstate.controller[j], m_pCurrentEntity->latched.prevcontroller[j], value, dadt );
		}
		else
		{
			value = mouthopen / 64.0;
			if (value > 1.0) value = 1.0;				
			value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			// Con_DPrintf("%d %f\n", mouthopen, value );
		}
		switch(pbonecontroller[j].type & STUDIO_TYPES)
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}


/*
====================
StudioCalcBoneQuaterion

====================
*/
void CStudioModelRenderer::StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *q )
{
	int					j, k;
	vec4_t				q1, q2;
	vec3_t				angle1, angle2;
	mstudioanimvalue_t	*panimvalue;

	for (j = 0; j < 3; j++)
	{
		if (panim->offset[j+3] == 0)
		{
			angle2[j] = angle1[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// Bah, missing blend!
			if (panimvalue->num.valid > k)
			{
				angle1[j] = panimvalue[k+1].value;

				if (panimvalue->num.valid > k + 1)
				{
					angle2[j] = panimvalue[k+2].value;
				}
				else
				{
					if (panimvalue->num.total > k + 1)
						angle2[j] = angle1[j];
					else
						angle2[j] = panimvalue[panimvalue->num.valid+2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if (panimvalue->num.total > k + 1)
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			angle1[j] = pbone->value[j+3] + angle1[j] * pbone->scale[j+3];
			angle2[j] = pbone->value[j+3] + angle2[j] * pbone->scale[j+3];
		}

		if (pbone->bonecontroller[j+3] != -1)
		{
			angle1[j] += adj[pbone->bonecontroller[j+3]];
			angle2[j] += adj[pbone->bonecontroller[j+3]];
		}
	}

	if (!VectorCompare( angle1, angle2 ))
	{
		AngleQuaternion( angle1, q1 );
		AngleQuaternion( angle2, q2 );
		QuaternionSlerp( q1, q2, s, q );
	}
	else
	{
		AngleQuaternion( angle1, q );
	}
}

/*
====================
StudioCalcBonePosition

====================
*/
void CStudioModelRenderer::StudioCalcBonePosition( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *pos )
{
	int					j, k;
	mstudioanimvalue_t	*panimvalue;

	for (j = 0; j < 3; j++)
	{
		pos[j] = pbone->value[j]; // default;
		if (panim->offset[j] != 0)
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			/*
			if (i == 0 && j == 0)
				Con_DPrintf("%d  %d:%d  %f\n", frame, panimvalue->num.valid, panimvalue->num.total, s );
			*/
			
			k = frame;
			// DEBUG
			if (panimvalue->num.total < panimvalue->num.valid)
				k = 0;
			// find span of values that includes the frame we want
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
  				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// if we're inside the span
			if (panimvalue->num.valid > k)
			{
				// and there's more data in the span
				if (panimvalue->num.valid > k + 1)
				{
					pos[j] += (panimvalue[k+1].value * (1.0 - s) + s * panimvalue[k+2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[k+1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if (panimvalue->num.total <= k + 1)
				{
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if ( pbone->bonecontroller[j] != -1 && adj )
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

/*
====================
StudioSlerpBones

====================
*/
void CStudioModelRenderer::StudioSlerpBones( vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s )
{
	int			i;
	vec4_t		q3;
	float		s1;

	if (s < 0) s = 0;
	else if (s > 1.0) s = 1.0;

	s1 = 1.0 - s;

	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		QuaternionSlerp( q1[i], q2[i], s, q3 );
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

/*
====================
StudioGetAnim

====================
*/
mstudioanim_t *CStudioModelRenderer::StudioGetAnim( model_t *m_pSubModel, mstudioseqdesc_t *pseqdesc )
{
	mstudioseqgroup_t	*pseqgroup;
	cache_user_t *paSequences;

	pseqgroup = (mstudioseqgroup_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;

	if (pseqdesc->seqgroup == 0)
	{
		return (mstudioanim_t *)((byte *)m_pStudioHeader + pseqdesc->animindex);
	}

	paSequences = (cache_user_t *)m_pSubModel->submodels;

	if (paSequences == NULL)
	{
		paSequences = (cache_user_t *)IEngineStudio.Mem_Calloc( 16, sizeof( cache_user_t ) ); // UNDONE: leak!
		m_pSubModel->submodels = (dmodel_t *)paSequences;
	}

	if (!IEngineStudio.Cache_Check( (struct cache_user_s *)&(paSequences[pseqdesc->seqgroup])))
	{
		gEngfuncs.Con_DPrintf("loading %s\n", pseqgroup->name );
		IEngineStudio.LoadCacheFile( pseqgroup->name, (struct cache_user_s *)&paSequences[pseqdesc->seqgroup] );
	}
	return (mstudioanim_t *)((byte *)paSequences[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

/*
====================
StudioPlayerBlend

====================
*/
void CStudioModelRenderer::StudioPlayerBlend( mstudioseqdesc_t *pseqdesc, int *pBlend, float *pPitch )
{
	// calc up/down pointing
	*pBlend = (*pPitch * 3);
	if (*pBlend < pseqdesc->blendstart[0])
	{
		*pPitch -= pseqdesc->blendstart[0] / 3.0;
		*pBlend = 0;
	}
	else if (*pBlend > pseqdesc->blendend[0])
	{
		*pPitch -= pseqdesc->blendend[0] / 3.0;
		*pBlend = 255;
	}
	else
	{
		if (pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1) // catch qc error
			*pBlend = 127;
		else
			*pBlend = 255 * (*pBlend - pseqdesc->blendstart[0]) / (pseqdesc->blendend[0] - pseqdesc->blendstart[0]);
		*pPitch = 0;
	}
}

/*
====================
StudioSetUpTransform

====================
*/
void CStudioModelRenderer::StudioSetUpTransform (int trivial_accept)
{
	int				i;
	vec3_t			angles;
	vec3_t			modelpos;

// tweek model origin	
	//for (i = 0; i < 3; i++)
	//	modelpos[i] = m_pCurrentEntity->origin[i];

	VectorCopy( m_pCurrentEntity->origin, modelpos );

// TODO: should really be stored with the entity instead of being reconstructed
// TODO: should use a look-up table
// TODO: could cache lazily, stored in the entity
	angles[ROLL] = m_pCurrentEntity->curstate.angles[ROLL];
	angles[PITCH] = m_pCurrentEntity->curstate.angles[PITCH];
	angles[YAW] = m_pCurrentEntity->curstate.angles[YAW];

	//Con_DPrintf("Angles %4.2f prev %4.2f for %i\n", angles[PITCH], m_pCurrentEntity->index);
	//Con_DPrintf("movetype %d %d\n", m_pCurrentEntity->movetype, m_pCurrentEntity->aiment );
	if (m_pCurrentEntity->curstate.movetype == MOVETYPE_STEP) 
	{
		float			f = 0;
		float			d;

		// don't do it if the goalstarttime hasn't updated in a while.

		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.

		if ( ( m_clTime < m_pCurrentEntity->curstate.animtime + 1.0f ) &&
			 ( m_pCurrentEntity->curstate.animtime != m_pCurrentEntity->latched.prevanimtime ) )
		{
			f = (m_clTime - m_pCurrentEntity->curstate.animtime) / (m_pCurrentEntity->curstate.animtime - m_pCurrentEntity->latched.prevanimtime);
			//Con_DPrintf("%4.2f %.2f %.2f\n", f, m_pCurrentEntity->curstate.animtime, m_clTime);
		}

		if (m_fDoInterp)
		{
			// ugly hack to interpolate angle, position. current is reached 0.1 seconds after being set
			f = f - 1.0;
		}
		else
		{
			f = 0;
		}

		for (i = 0; i < 3; i++)
		{
			modelpos[i] += (m_pCurrentEntity->origin[i] - m_pCurrentEntity->latched.prevorigin[i]) * f;
		}

		// NOTE:  Because multiplayer lag can be relatively large, we don't want to cap
		//  f at 1.5 anymore.
		//if (f > -1.0 && f < 1.5) {}

//			Con_DPrintf("%.0f %.0f\n",m_pCurrentEntity->msg_angles[0][YAW], m_pCurrentEntity->msg_angles[1][YAW] );
		for (i = 0; i < 3; i++)
		{
			float ang1, ang2;

			ang1 = m_pCurrentEntity->angles[i];
			ang2 = m_pCurrentEntity->latched.prevangles[i];

			d = ang1 - ang2;
			if (d > 180)
			{
				d -= 360;
			}
			else if (d < -180)
			{	
				d += 360;
			}

			angles[i] += d * f;
		}
		//Con_DPrintf("%.3f \n", f );
	}
	else if ( m_pCurrentEntity->curstate.movetype != MOVETYPE_NONE ) 
	{
		VectorCopy( m_pCurrentEntity->angles, angles );
	}

	//Con_DPrintf("%.0f %0.f %0.f\n", modelpos[0], modelpos[1], modelpos[2] );
	//Con_DPrintf("%.0f %0.f %0.f\n", angles[0], angles[1], angles[2] );

	angles[PITCH] = -angles[PITCH];
	AngleMatrix (angles, (*m_protationmatrix));

	if ( !IEngineStudio.IsHardware() )
	{
		static float viewmatrix[3][4];

		VectorCopy (m_vRight, viewmatrix[0]);
		VectorCopy (m_vUp, viewmatrix[1]);
		VectorInverse (viewmatrix[1]);
		VectorCopy (m_vNormal, viewmatrix[2]);

		(*m_protationmatrix)[0][3] = modelpos[0] - m_vRenderOrigin[0];
		(*m_protationmatrix)[1][3] = modelpos[1] - m_vRenderOrigin[1];
		(*m_protationmatrix)[2][3] = modelpos[2] - m_vRenderOrigin[2];

		ConcatTransforms (viewmatrix, (*m_protationmatrix), (*m_paliastransform));

		// do the scaling up of x and y to screen coordinates as part of the transform
		// for the unclipped case (it would mess up clipping in the clipped case).
		// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
		// correspondingly so the projected x and y come out right
		// FIXME: make this work for clipped case too?
		if (trivial_accept)
		{
			for (i=0 ; i<4 ; i++)
			{
				(*m_paliastransform)[0][i] *= m_fSoftwareXScale *
						(1.0 / (ZISCALE * 0x10000));
				(*m_paliastransform)[1][i] *= m_fSoftwareYScale *
						(1.0 / (ZISCALE * 0x10000));
				(*m_paliastransform)[2][i] *= 1.0 / (ZISCALE * 0x10000);

			}
		}
	}

	extern cvar_t *m_pCvarRighthand;
	if (m_pCurrentEntity == gEngfuncs.GetViewModel() || m_pCurrentEntity->curstate.iuser4 == 1)
	{
		if (!m_pCvarRighthand->value)
		{
			(*m_protationmatrix)[0][1] *= -1;
			(*m_protationmatrix)[1][1] *= -1;
			(*m_protationmatrix)[2][1] *= -1;
		}
	}

	(*m_protationmatrix)[0][3] = modelpos[0];
	(*m_protationmatrix)[1][3] = modelpos[1];
	(*m_protationmatrix)[2][3] = modelpos[2];
}


/*
====================
StudioEstimateInterpolant

====================
*/
float CStudioModelRenderer::StudioEstimateInterpolant( void )
{
	float dadt = 1.0;

	if ( m_fDoInterp && ( m_pCurrentEntity->curstate.animtime >= m_pCurrentEntity->latched.prevanimtime + 0.01 ) )
	{
		dadt = (m_clTime - m_pCurrentEntity->curstate.animtime) / 0.1;
		if (dadt > 2.0)
		{
			dadt = 2.0;
		}
	}
	return dadt;
}

/*
====================
StudioCalcRotations

====================
*/
void CStudioModelRenderer::StudioCalcRotations ( float pos[][3], vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f )
{
	int					i;
	int					frame;
	mstudiobone_t		*pbone;

	float				s;
	float				adj[MAXSTUDIOCONTROLLERS];
	float				dadt;

	if (f > pseqdesc->numframes - 1)
	{
		f = 0;	// bah, fix this bug with changing sequences too fast
	}
	// BUG ( somewhere else ) but this code should validate this data.
	// This could cause a crash if the frame # is negative, so we'll go ahead
	//  and clamp it here
	else if ( f < -0.01 )
	{
		f = -0.01;
	}

	frame = (int)f;

	// Con_DPrintf("%d %.4f %.4f %.4f %.4f %d\n", m_pCurrentEntity->curstate.sequence, m_clTime, m_pCurrentEntity->animtime, m_pCurrentEntity->frame, f, frame );

	// Con_DPrintf( "%f %f %f\n", m_pCurrentEntity->angles[ROLL], m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->angles[YAW] );

	// Con_DPrintf("frame %d %d\n", frame1, frame2 );


	dadt = StudioEstimateInterpolant( );
	s = (f - frame);

	// add in programtic controllers
	pbone		= (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	StudioCalcBoneAdj( dadt, adj, m_pCurrentEntity->curstate.controller, m_pCurrentEntity->latched.prevcontroller, m_pCurrentEntity->mouth.mouthopen );

	for (i = 0; i < m_pStudioHeader->numbones; i++, pbone++, panim++) 
	{
		StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );

		StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
		// if (0 && i == 0)
		//	Con_DPrintf("%d %d %d %d\n", m_pCurrentEntity->curstate.sequence, frame, j, k );
	}

	if (pseqdesc->motiontype & STUDIO_X)
	{
		pos[pseqdesc->motionbone][0] = 0.0;
	}
	if (pseqdesc->motiontype & STUDIO_Y)
	{
		pos[pseqdesc->motionbone][1] = 0.0;
	}
	if (pseqdesc->motiontype & STUDIO_Z)
	{
		pos[pseqdesc->motionbone][2] = 0.0;
	}

	s = 0 * ((1.0 - (f - (int)(f))) / (pseqdesc->numframes)) * m_pCurrentEntity->curstate.framerate;

	if (pseqdesc->motiontype & STUDIO_LX)
	{
		pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	}
	if (pseqdesc->motiontype & STUDIO_LY)
	{
		pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	}
	if (pseqdesc->motiontype & STUDIO_LZ)
	{
		pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
	}
}

/*
====================
Studio_FxTransform

====================
*/
void CStudioModelRenderer::StudioFxTransform( cl_entity_t *ent, float transform[3][4] )
{
	switch( ent->curstate.renderfx )
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if ( gEngfuncs.pfnRandomLong(0,49) == 0 )
		{
			int axis = gEngfuncs.pfnRandomLong(0,1);
			if ( axis == 1 ) // Choose between x & z
				axis = 2;
			VectorScale( transform[axis], gEngfuncs.pfnRandomFloat(1,1.484), transform[axis] );
		}
		else if ( gEngfuncs.pfnRandomLong(0,49) == 0 )
		{
			float offset;
			int axis = gEngfuncs.pfnRandomLong(0,1);
			if ( axis == 1 ) // Choose between x & z
				axis = 2;
			offset = gEngfuncs.pfnRandomFloat(-10,10);
			transform[gEngfuncs.pfnRandomLong(0,2)][3] += offset;
		}
	break;
	case kRenderFxExplode:
		{
			float scale;

			scale = 1.0 + ( m_clTime - ent->curstate.animtime) * 10.0;
			if ( scale > 2 )	// Don't blow up more than 200%
				scale = 2;
			transform[0][1] *= scale;
			transform[1][1] *= scale;
			transform[2][1] *= scale;
		}
	break;

	}
}

/*
====================
StudioEstimateFrame

====================
*/
float CStudioModelRenderer::StudioEstimateFrame( mstudioseqdesc_t *pseqdesc )
{
	double				dfdt, f;

	if ( m_fDoInterp )
	{
		if ( m_clTime < m_pCurrentEntity->curstate.animtime )
		{
			dfdt = 0;
		}
		else
		{
			float multipler = 1;
			bool model = (m_pCurrentEntity == gEngfuncs.GetViewModel() || (m_pCurrentEntity->curstate.effects & EF_VIEWMODEL) != 0);
			if (model)
				multipler = GetWeaponMultipler();

			float fps = model ? (pseqdesc->fps / multipler) : pseqdesc->fps;
			dfdt = (m_clTime - m_pCurrentEntity->curstate.animtime) * m_pCurrentEntity->curstate.framerate * (fps);

		}
	}
	else
	{
		dfdt = 0;
	}

	if (pseqdesc->numframes <= 1)
	{
		f = 0;
	}
	else
	{
		f = (m_pCurrentEntity->curstate.frame * (pseqdesc->numframes - 1)) / 256.0;
	}
 	
	f += dfdt;

	if (pseqdesc->flags & STUDIO_LOOPING) 
	{
		if (pseqdesc->numframes > 1)
		{
			f -= (int)(f / (pseqdesc->numframes - 1)) *  (pseqdesc->numframes - 1);
		}
		if (f < 0) 
		{
			f += (pseqdesc->numframes - 1);
		}
	}
	else 
	{
		if (f >= pseqdesc->numframes - 1.001) 
		{
			f = pseqdesc->numframes - 1.001;
		}
		if (f < 0.0) 
		{
			f = 0.0;
		}
	}
	return f;
}

/*
====================
StudioSetupBones

====================
*/
void CStudioModelRenderer::StudioSetupBones ( void )
{
	int					i;
	double				f;

	mstudiobone_t		*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t		*panim;

	static float		pos[MAXSTUDIOBONES][3];
	static vec4_t		q[MAXSTUDIOBONES];
	float				bonematrix[3][4];

	static float		pos2[MAXSTUDIOBONES][3];
	static vec4_t		q2[MAXSTUDIOBONES];
	static float		pos3[MAXSTUDIOBONES][3];
	static vec4_t		q3[MAXSTUDIOBONES];
	static float		pos4[MAXSTUDIOBONES][3];
	static vec4_t		q4[MAXSTUDIOBONES];

	if (m_pCurrentEntity->curstate.sequence >=  m_pStudioHeader->numseq) 
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	// always want new gait sequences to start on frame zero
/*	if ( m_pPlayerInfo )
	{
		int playerNum = m_pCurrentEntity->index - 1;

		// new jump gaitsequence?  start from frame zero
		if ( m_nPlayerGaitSequences[ playerNum ] != m_pPlayerInfo->gaitsequence )
		{
	//		m_pPlayerInfo->gaitframe = 0.0;
			gEngfuncs.Con_Printf( "Setting gaitframe to 0\n" );
		}

		m_nPlayerGaitSequences[ playerNum ] = m_pPlayerInfo->gaitsequence;
//		gEngfuncs.Con_Printf( "index: %d     gaitsequence: %d\n",playerNum, m_pPlayerInfo->gaitsequence);
	}
*/
	f = StudioEstimateFrame( pseqdesc );

	if (m_pCurrentEntity->latched.prevframe > f)
	{
		//Con_DPrintf("%f %f\n", m_pCurrentEntity->prevframe, f );
	}

	panim = StudioGetAnim( m_pRenderModel, pseqdesc );
	StudioCalcRotations( pos, q, pseqdesc, panim, f );

	if (pseqdesc->numblends > 1)
	{
		float				s;
		float				dadt;

		panim += m_pStudioHeader->numbones;
		StudioCalcRotations( pos2, q2, pseqdesc, panim, f );

		dadt = StudioEstimateInterpolant();
		s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;

		StudioSlerpBones( q, pos, q2, pos2, s );

		if (pseqdesc->numblends == 4)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations( pos3, q3, pseqdesc, panim, f );

			panim += m_pStudioHeader->numbones;
			StudioCalcRotations( pos4, q4, pseqdesc, panim, f );

			s = (m_pCurrentEntity->curstate.blending[0] * dadt + m_pCurrentEntity->latched.prevblending[0] * (1.0 - dadt)) / 255.0;
			StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = (m_pCurrentEntity->curstate.blending[1] * dadt + m_pCurrentEntity->latched.prevblending[1] * (1.0 - dadt)) / 255.0;
			StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}
	
	if (m_fDoInterp &&
		m_pCurrentEntity->latched.sequencetime &&
		( m_pCurrentEntity->latched.sequencetime + 0.2 > m_clTime ) && 
		( m_pCurrentEntity->latched.prevsequence < m_pStudioHeader->numseq ))
	{
		// blend from last sequence
		static float		pos1b[MAXSTUDIOBONES][3];
		static vec4_t		q1b[MAXSTUDIOBONES];
		float				s;

		if (m_pCurrentEntity->latched.prevsequence >=  m_pStudioHeader->numseq) 
		{
			m_pCurrentEntity->latched.prevsequence = 0;
		}

		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->latched.prevsequence;
		panim = StudioGetAnim( m_pRenderModel, pseqdesc );
		// clip prevframe
		StudioCalcRotations( pos1b, q1b, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

		if (pseqdesc->numblends > 1)
		{
			panim += m_pStudioHeader->numbones;
			StudioCalcRotations( pos2, q2, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

			s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0;
			StudioSlerpBones( q1b, pos1b, q2, pos2, s );

			if (pseqdesc->numblends == 4)
			{
				panim += m_pStudioHeader->numbones;
				StudioCalcRotations( pos3, q3, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

				panim += m_pStudioHeader->numbones;
				StudioCalcRotations( pos4, q4, pseqdesc, panim, m_pCurrentEntity->latched.prevframe );

				s = (m_pCurrentEntity->latched.prevseqblending[0]) / 255.0;
				StudioSlerpBones( q3, pos3, q4, pos4, s );

				s = (m_pCurrentEntity->latched.prevseqblending[1]) / 255.0;
				StudioSlerpBones( q1b, pos1b, q3, pos3, s );
			}
		}

		s = 1.0 - (m_clTime - m_pCurrentEntity->latched.sequencetime) / 0.2;
		StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		//Con_DPrintf("prevframe = %4.2f\n", f);
		m_pCurrentEntity->latched.prevframe = f;
	}

	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	// bounds checking
	if ( m_pPlayerInfo )
	{
		if ( m_pPlayerInfo->gaitsequence >= m_pStudioHeader->numseq )
		{
			m_pPlayerInfo->gaitsequence = 0;
		}
	}

	// calc gait animation
	if ( m_pPlayerInfo && m_pPlayerInfo->gaitsequence != 0 )
	{ 
		if (m_pPlayerInfo->gaitsequence >=  m_pStudioHeader->numseq) 
		{
			m_pPlayerInfo->gaitsequence = 0;
		}

		int copy = 1;

		pseqdesc = (mstudioseqdesc_t *)( (byte *)m_pStudioHeader + m_pStudioHeader->seqindex ) + m_pPlayerInfo->gaitsequence;

		panim = StudioGetAnim( m_pRenderModel, pseqdesc );
		StudioCalcRotations( pos2, q2, pseqdesc, panim, m_pPlayerInfo->gaitframe );

		for ( i = 0; i < m_pStudioHeader->numbones; i++ )
		{
			if ( !strcmp( pbones[i].name, "Bip01 Spine" ) )
			{
				copy = 0;
			}
			else if ( !strcmp( pbones[ pbones[i].parent ].name, "Bip01 Pelvis" ) )
			{
				copy = 1;
			}
				
			if ( copy )
			{
				memcpy( pos[i], pos2[i], sizeof( pos[i] ) );
				memcpy( q[i], q2[i], sizeof( q[i] ) );
			}
		}
	}

	for (i = 0; i < m_pStudioHeader->numbones; i++) 
	{
		QuaternionMatrix( q[i], bonematrix );

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if (pbones[i].parent == -1) 
		{
			if ( IEngineStudio.IsHardware() )
			{
				ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_pbonetransform)[i]);

				// MatrixCopy should be faster...
				//ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);
				MatrixCopy( (*m_pbonetransform)[i], (*m_plighttransform)[i] );
			}
			else
			{
				ConcatTransforms ((*m_paliastransform), bonematrix, (*m_pbonetransform)[i]);
				ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);
			}

			// Apply client-side effects to the transformation matrix
			StudioFxTransform( m_pCurrentEntity, (*m_pbonetransform)[i] );

			if (MutatorEnabled(MUTATOR_PAPER))
			{
				if (m_pCurrentEntity != gEngfuncs.GetViewModel())
				{
					for (int j = 0; j < 3; ++j)
					{
						(*m_pbonetransform)[i][j][1] *= 0.1f;
					}
				}
			}

			if (MutatorEnabled(MUTATOR_MINIME))
			{
				if (m_pCurrentEntity != gEngfuncs.GetViewModel())
				{
					for (int j = 0; j < 3; ++j)
					{
						(*m_pbonetransform)[i][j][2] *= 0.5f;
					}
					(*m_pbonetransform)[i][2][3] -= 16.0f;
				}
			}
		} 
		else 
		{
			ConcatTransforms ((*m_pbonetransform)[pbones[i].parent], bonematrix, (*m_pbonetransform)[i]);
			ConcatTransforms ((*m_plighttransform)[pbones[i].parent], bonematrix, (*m_plighttransform)[i]);
		}

		if (MutatorEnabled(MUTATOR_BIGHEAD))
		{
			if (m_pCurrentEntity != gEngfuncs.GetViewModel())
			{
				//m_pPlayerInfo->topcolor;
				float multiplier = 1;
				if (!strcmp( pbones[i].name, "Bip01 Head" )) {
					multiplier = 3;
					for (int x = 0; x <= 2; x++)
						for (int y = 0; y <= 2; y++)
							(*m_pbonetransform)[i][x][y] *= multiplier;
				}

				if (!strcmp( pbones[i].name, "Bip01 R Arm" ) ||
					!strcmp( pbones[i].name, "Bip01 L Arm" )) {
					multiplier = 2;
					for (int x = 0; x <= 2; x++)
						for (int y = 0; y <= 2; y++)
							(*m_pbonetransform)[i][x][y] *= multiplier;
				}
			}
		}
	}
}


/*
====================
StudioSaveBones

====================
*/
void CStudioModelRenderer::StudioSaveBones( void )
{
	int		i;

	mstudiobone_t		*pbones;
	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	m_nCachedBones = m_pStudioHeader->numbones;

	for (i = 0; i < m_pStudioHeader->numbones; i++) 
	{
		strcpy( m_nCachedBoneNames[i], pbones[i].name );
		MatrixCopy( (*m_pbonetransform)[i], m_rgCachedBoneTransform[i] );
		MatrixCopy( (*m_plighttransform)[i], m_rgCachedLightTransform[i] );
	}
}


/*
====================
StudioMergeBones

====================
*/
void CStudioModelRenderer::StudioMergeBones ( model_t *m_pSubModel )
{
	int					i, j;
	double				f;
	int					do_hunt = true;

	mstudiobone_t		*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t		*panim;

	static float		pos[MAXSTUDIOBONES][3];
	float				bonematrix[3][4];
	static vec4_t		q[MAXSTUDIOBONES];

	if (m_pCurrentEntity->curstate.sequence >=  m_pStudioHeader->numseq) 
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	f = StudioEstimateFrame( pseqdesc );

	if (m_pCurrentEntity->latched.prevframe > f)
	{
		//Con_DPrintf("%f %f\n", m_pCurrentEntity->prevframe, f );
	}

	panim = StudioGetAnim( m_pSubModel, pseqdesc );
	StudioCalcRotations( pos, q, pseqdesc, panim, f );

	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);


	for (i = 0; i < m_pStudioHeader->numbones; i++) 
	{
		for (j = 0; j < m_nCachedBones; j++)
		{
			if (stricmp(pbones[i].name, m_nCachedBoneNames[j]) == 0)
			{
				MatrixCopy( m_rgCachedBoneTransform[j], (*m_pbonetransform)[i] );
				MatrixCopy( m_rgCachedLightTransform[j], (*m_plighttransform)[i] );
				break;
			}
		}
		if (j >= m_nCachedBones)
		{
			QuaternionMatrix( q[i], bonematrix );

			bonematrix[0][3] = pos[i][0];
			bonematrix[1][3] = pos[i][1];
			bonematrix[2][3] = pos[i][2];

			if (pbones[i].parent == -1) 
			{
				if ( IEngineStudio.IsHardware() )
				{
					ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_pbonetransform)[i]);

					// MatrixCopy should be faster...
					//ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);
					MatrixCopy( (*m_pbonetransform)[i], (*m_plighttransform)[i] );
				}
				else
				{
					ConcatTransforms ((*m_paliastransform), bonematrix, (*m_pbonetransform)[i]);
					ConcatTransforms ((*m_protationmatrix), bonematrix, (*m_plighttransform)[i]);
				}

				// Apply client-side effects to the transformation matrix
				StudioFxTransform( m_pCurrentEntity, (*m_pbonetransform)[i] );
			} 
			else 
			{
				ConcatTransforms ((*m_pbonetransform)[pbones[i].parent], bonematrix, (*m_pbonetransform)[i]);
				ConcatTransforms ((*m_plighttransform)[pbones[i].parent], bonematrix, (*m_plighttransform)[i]);
			}
		}
	}
}

#if defined( _TFC )
#include "pm_shared.h"
const Vector& GetTeamColor( int team_no );
#define IS_FIRSTPERSON_SPEC ( g_iUser1 == OBS_IN_EYE || (g_iUser1 && (gHUD.m_Spectator.m_pip->value == INSET_IN_EYE)) )

int GetRemapColor( int iTeam, bool bTopColor )
{
	int retVal = 0;

	switch( iTeam )
	{
	default:
	case 1: 
		if ( bTopColor )
			retVal = TEAM1_COLOR;
		else
			retVal = TEAM1_COLOR - 10;

		break;
	case 2: 
		if ( bTopColor )
			retVal = TEAM2_COLOR;
		else
			retVal = TEAM2_COLOR - 10;

		break;
	case 3: 
		if ( bTopColor )
			retVal = TEAM3_COLOR;
		else
			retVal = TEAM3_COLOR - 10;

		break;
	case 4: 
		if ( bTopColor )
			retVal = TEAM4_COLOR;
		else
			retVal = TEAM4_COLOR - 10;

		break;
	}

	return retVal;
}
#endif 

/*
====================
StudioDrawModel

====================
*/

Vector cs_vo;
Vector cs_va;
float(*BoneOrigin)[MAXSTUDIOBONES][3][4];

int CStudioModelRenderer::StudioDrawModel( int flags )
{
	alight_t lighting;
	vec3_t dir;

	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();
	IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );
	IEngineStudio.GetViewInfo( m_vRenderOrigin, m_vUp, m_vRight, m_vNormal );
	IEngineStudio.GetAliasScale( &m_fSoftwareXScale, &m_fSoftwareYScale );

	if (cl_icemodels && gHUD.m_IceModelsIndex != SKIN_MUTATOR && gHUD.m_IceModelsIndex != cl_icemodels->value)
	{
		gHUD.m_IceModelsIndex = cl_icemodels->value;
	}

	if (m_pCurrentEntity->curstate.renderfx == kRenderFxDeadPlayer)
	{
		entity_state_t deadplayer;

		int result;
		int save_interp;

		if (m_pCurrentEntity->curstate.renderamt <= 0 || m_pCurrentEntity->curstate.renderamt > gEngfuncs.GetMaxClients() )
			return 0;

		// get copy of player
		deadplayer = *(IEngineStudio.GetPlayerState( m_pCurrentEntity->curstate.renderamt - 1 )); //cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[m_pCurrentEntity->curstate.renderamt-1];

		// clear weapon, movement state
		deadplayer.number = m_pCurrentEntity->curstate.renderamt;
		deadplayer.weaponmodel = 0;
		deadplayer.gaitsequence = 0;

		deadplayer.movetype = MOVETYPE_NONE;
		VectorCopy( m_pCurrentEntity->curstate.angles, deadplayer.angles );
		VectorCopy( m_pCurrentEntity->curstate.origin, deadplayer.origin );

		save_interp = m_fDoInterp;
		m_fDoInterp = 0;
		
		// draw as though it were a player
		if (MutatorEnabled(MUTATOR_SANIC))
			result = 1;
		else
			result = StudioDrawPlayer( flags, &deadplayer );
		
		m_fDoInterp = save_interp;
		return result;
	}

	m_pRenderModel = m_pCurrentEntity->model;
	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata (m_pRenderModel);
	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( m_pRenderModel );

	StudioSetUpTransform( 0 );

	if (flags & STUDIO_RENDER)
	{
		// see if the bounding box lets us trivially reject, also sets
		if (!IEngineStudio.StudioCheckBBox ())
			return 0;

		/*
		// bboxs aren't set in cold ice
		vec3_t mins, maxs;
		StudioGetMinsMaxs(mins, maxs);
		if (gFog.CullFogBBox(mins, maxs))
			return 0;
		*/

		(*m_pModelsDrawn)++;
		(*m_pStudioModelCount)++; // render data cache cookie

		if (m_pStudioHeader->numbodyparts == 0)
			return 1;
	}

	if (m_pCurrentEntity->curstate.movetype == MOVETYPE_FOLLOW)
	{
		StudioMergeBones( m_pRenderModel );
	}
	else
	{
		StudioSetupBones( );
	}
	StudioSaveBones( );

	if (flags & STUDIO_EVENTS)
	{
		StudioCalcAttachments( );
		IEngineStudio.StudioClientEvents( );
		// copy attachments into global entity array
		if ( m_pCurrentEntity->index > 0 )
		{
			cl_entity_t *ent = gEngfuncs.GetEntityByIndex( m_pCurrentEntity->index );

			memcpy( ent->attachment, m_pCurrentEntity->attachment, sizeof( vec3_t ) * 4 );
		}
	}

	if (flags & STUDIO_RENDER)
	{
		lighting.plightvec = dir;
		IEngineStudio.StudioDynamicLight(m_pCurrentEntity, &lighting );

		if (!strcmp(m_pCurrentEntity->model->name, "models/w_items.mdl") && m_pCurrentEntity->curstate.body == 10)
		{
			lighting.color = Vector( 1.0f, 1.0f, 1.0f );
			lighting.ambientlight = 255;
			lighting.shadelight = 0;
		}

		if (!strcmp(m_pCurrentEntity->model->name, "models/coldspot.mdl"))
		{
			lighting.color = Vector( 1.0f, 1.0f, 1.0f );
			lighting.ambientlight = 192;
			lighting.shadelight = 0;
		}

		IEngineStudio.StudioEntityLight( &lighting );

		// model and frame independant
		IEngineStudio.StudioSetupLighting (&lighting);

		// get remap colors
#if defined( _TFC )

		m_nTopColor    = m_pCurrentEntity->curstate.colormap & 0xFF;
		m_nBottomColor = (m_pCurrentEntity->curstate.colormap & 0xFF00) >> 8;

		// use the old tfc colors for the models (view models)
		// team 1
		if ( ( m_nTopColor < 155 ) && ( m_nTopColor > 135 ) )
		{
			m_nTopColor    = TEAM1_COLOR;
			m_nBottomColor = TEAM1_COLOR - 10;
		}
		// team 2
		else if ( ( m_nTopColor < 260 ) && ( ( m_nTopColor > 240 ) || ( m_nTopColor == 5 ) ) )
		{
			m_nTopColor    = TEAM2_COLOR;
			m_nBottomColor = TEAM2_COLOR - 10;
		}
		// team 3
		else if ( ( m_nTopColor < 50 ) && ( m_nTopColor > 40 ) )
		{
			m_nTopColor    = TEAM3_COLOR;
			m_nBottomColor = TEAM3_COLOR - 10;
		}
		// team 4
		else if ( ( m_nTopColor < 110 )  && ( m_nTopColor > 75 ) )
		{
			m_nTopColor    = TEAM4_COLOR;
			m_nBottomColor = TEAM4_COLOR - 10;
		}

		// is this our view model and should it be glowing? we also fix a remap colors
		// problem if we're spectating in first-person mode
		if ( m_pCurrentEntity == gEngfuncs.GetViewModel() )
		{
			cl_entity_t *pTarget = NULL;

			// we're spectating someone via first-person mode
			if ( IS_FIRSTPERSON_SPEC )
			{
				pTarget = gEngfuncs.GetEntityByIndex( g_iUser2 );

				if ( pTarget )
				{
					// we also need to correct the m_nTopColor and m_nBottomColor for the 
					// view model here. this is separate from the glowshell stuff, but
					// the same conditions need to be met (this is the view model and we're
					// in first-person spectator mode)
					m_nTopColor = GetRemapColor( g_PlayerExtraInfo[pTarget->index].teamnumber, true );
					m_nBottomColor = GetRemapColor( g_PlayerExtraInfo[pTarget->index].teamnumber, false );
				}
			}
			// we're not spectating, this is OUR view model
			else 
			{
				pTarget = gEngfuncs.GetLocalPlayer();
			}

			if ( pTarget && pTarget->curstate.renderfx == kRenderFxGlowShell )
			{
				m_pCurrentEntity->curstate.renderfx = kRenderFxGlowShell;
				m_pCurrentEntity->curstate.rendercolor.r = pTarget->curstate.rendercolor.r;
				m_pCurrentEntity->curstate.rendercolor.g = pTarget->curstate.rendercolor.g;
				m_pCurrentEntity->curstate.rendercolor.b = pTarget->curstate.rendercolor.b;
			}
			else
			{
				m_pCurrentEntity->curstate.renderfx = kRenderFxNone;
				m_pCurrentEntity->curstate.rendercolor.r = 0;
				m_pCurrentEntity->curstate.rendercolor.g = 0;
				m_pCurrentEntity->curstate.rendercolor.b = 0;
			}
		}

#else
		m_nTopColor    = m_pCurrentEntity->curstate.colormap & 0xFF;
		m_nBottomColor = (m_pCurrentEntity->curstate.colormap & 0xFF00) >> 8;

		/*if ((m_pCurrentEntity->model && (strstr(m_pCurrentEntity->model->name, "w_"))) ||
			m_pCurrentEntity == gEngfuncs.GetViewModel())*/
		m_pCurrentEntity->curstate.skin = gHUD.m_IceModelsIndex;

		if ( m_pCurrentEntity == gEngfuncs.GetViewModel() )
		{
			cl_entity_t *pTarget = gEngfuncs.GetLocalPlayer();

			if (MutatorEnabled(MUTATOR_GOLDENGUNS))
				m_pCurrentEntity->curstate.skin = SKIN_GOLD;

			if ( pTarget && pTarget->curstate.renderfx == kRenderFxGlowShell )
			{
				m_pCurrentEntity->curstate.renderfx = kRenderFxGlowShell;
				m_pCurrentEntity->curstate.rendercolor.r = pTarget->curstate.rendercolor.r;
				m_pCurrentEntity->curstate.rendercolor.g = pTarget->curstate.rendercolor.g;
				m_pCurrentEntity->curstate.rendercolor.b = pTarget->curstate.rendercolor.b;
				m_pCurrentEntity->curstate.renderamt = fmin(fmax(pTarget->curstate.renderamt, 0), 10);
			}
			else if ( pTarget && pTarget->curstate.rendermode == kRenderTransAlpha )
			{
				m_pCurrentEntity->curstate.renderfx = kRenderFxNone;
				m_pCurrentEntity->curstate.rendermode = kRenderTransAdd;
				m_pCurrentEntity->curstate.rendercolor.r = 0;
				m_pCurrentEntity->curstate.rendercolor.g = 0;
				m_pCurrentEntity->curstate.rendercolor.b = 0;
				m_pCurrentEntity->curstate.renderamt = pTarget->curstate.renderamt;
			}
			else
			{
				m_pCurrentEntity->curstate.renderfx = kRenderFxNone;
				m_pCurrentEntity->curstate.rendermode = kRenderNormal;
				m_pCurrentEntity->curstate.rendercolor.r = 0;
				m_pCurrentEntity->curstate.rendercolor.g = 0;
				m_pCurrentEntity->curstate.rendercolor.b = 0;
				m_pCurrentEntity->curstate.renderamt = 0;
			}
		}
#endif 

		IEngineStudio.StudioSetRemapColors( m_nTopColor, m_nBottomColor );

#ifndef __APPLE__
		if (cl_portalmirror && cl_portalmirror->value)
		{
			if (!strcmp(m_pCurrentEntity->model->name, "models/w_portal.mdl"))
			{
				studiohdr_t* pHdr = (studiohdr_t*)m_pStudioHeader;
				mstudiotexture_t* pTexture = (mstudiotexture_t*)((byte*)m_pRenderModel->cache.data + pHdr->textureindex);

				std::vector<mstudiotexture_t> savedtexture;

				if (pHdr->textureindex > 0)
				{
					for (int i = 0; i < pHdr->numtextures; i++)
					{
						savedtexture.push_back(pTexture[i]);
						// memcpy(&pTexture[i], &pTexture[pHdr->numtextures + 1], sizeof(mstudiotexture_t));
						if (!strcmp(pTexture[i].name, "lul2.bmp"))
						{
							pTexture[i].index = gPortalRenderer.blankshit;
							//pTexture[i].width = gPortalRenderer.portalSize[0].x;
							//pTexture[i].height = -gPortalRenderer.portalSize[0].y;
						}
						else if (!strcmp(pTexture[i].name, "lul.bmp"))
						{
							pTexture[i].index = gPortalRenderer.blankshit;
							//pTexture[i].width = gPortalRenderer.portalSize[1].x;
							//pTexture[i].height = -gPortalRenderer.portalSize[1].y;
						}
					}
				}

				alight_t lighting;
				Vector dir;
				lighting.plightvec = dir;

				IEngineStudio.StudioDynamicLight(m_pCurrentEntity, &lighting);
				IEngineStudio.StudioEntityLight(&lighting);
				// model and frame independant
				IEngineStudio.StudioSetupLighting(&lighting);

				StudioRenderModel();

				for (int i = 0; i < pHdr->numtextures; i++)
				{
					memcpy(&pTexture[i], &savedtexture[i], sizeof(mstudiotexture_t));
				}

				return true;
			}
		}
#endif

		StudioRenderModel( );

		if (m_pCvarGlowModels && m_pCvarGlowModels->value)
		{
			// clientside batterylight lightning effect
			if (!strcmp(m_pCurrentEntity->model->name, "models/w_battery.mdl") && (m_pCurrentEntity->curstate.body == 0 || m_pCurrentEntity->curstate.body == 3))
			{
				dlight_t* dl = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
				VectorCopy(m_pCurrentEntity->curstate.origin, dl->origin);
				dl->radius = 20;
				dl->color.r = 0;
				dl->color.g = 91;
				dl->color.b = 91;
				dl->die = gHUD.m_flTimeDelta + 0.1f + gHUD.m_flTime;
			}

#ifdef _WIN32
			extern qboolean g_fXashEngine;
			if (!g_fXashEngine)
				AppendGlowModel();
#endif
		}
	}

	FlameSystem.Process(m_pCurrentEntity, IEngineStudio);

	return 1;
}

// ============================FULLBRIGHTS============================
// Fullbright attachments support written for HL:E
// by Bacontsu, much credits to : Admer456, FranticDreamer, and helpful people on TWHL!
// HOW TO USE : this function will combines 2 models and make one of them with "_light.mdl"
//				Fullbright (example : modelname_light.mdl)
void CStudioModelRenderer::AppendGlowModel()
{
	vec3_t dir;
	alight_t lighting;
	char* modelName = m_pCurrentEntity->model->name;
	int parentBody = m_pCurrentEntity->curstate.body;
	int parentSkin = m_pCurrentEntity->curstate.skin;
	const char* modNameReal = gEngfuncs.pfnGetGameDirectory();
	char modelNameLight[MAX_MODEL_NAME];
	char modName[MAX_MODEL_NAME];
	strcpy(modName, modNameReal);
	strcat(modName, "/");
	strcpy(modelNameLight, modelName);
	int length = strlen(modelNameLight);
	if (length > 4)
	{
		modelNameLight[length - 4] = '\0';
		strcat(modelNameLight, "_light.mdl");
		strcat(modName, modelNameLight);
	}

	FILE *bfp = fopen(modName, "rb");

	// Look for glow models, and attach it
	if (bfp != NULL)
	{
		if (CVAR_GET_FLOAT("developer") > 5)
			gEngfuncs.Con_Printf("model is %s, %s\n", modName, modelNameLight);

		cl_entity_t saveent = *m_pCurrentEntity;

		model_t* lightmodel = IEngineStudio.Mod_ForName(modName, 1); //load model for fullbrightpart
		m_pCurrentEntity->curstate.body = parentBody;
		m_pCurrentEntity->curstate.skin = parentSkin;

		m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(lightmodel);

		IEngineStudio.StudioSetHeader(m_pStudioHeader);

		StudioMergeBones(lightmodel); //merge both model

		m_pCurrentEntity->curstate.rendermode = kRenderTransAdd;

		lighting.plightvec = dir;
		IEngineStudio.StudioDynamicLight(m_pCurrentEntity, &lighting);

		// Fullbright
		lighting.color = Vector(1.0f, 1.0f, 1.0f);
		lighting.ambientlight = 255;
		lighting.shadelight = 0;

		IEngineStudio.StudioSetupLighting(&lighting);

		StudioRenderModel();

		StudioCalcAttachments();

		*m_pCurrentEntity = saveent;

		fclose(bfp);
	}
}

/*
====================
StudioEstimateGait

====================
*/
void CStudioModelRenderer::StudioEstimateGait( entity_state_t *pplayer )
{
	float dt;
	vec3_t est_velocity;

	dt = (m_clTime - m_clOldTime);
	if (dt < 0)
		dt = 0;
	else if (dt > 1.0)
		dt = 1;

	if (dt == 0 || m_pPlayerInfo->renderframe == m_nFrameCount)
	{
		m_flGaitMovement = 0;
		return;
	}

	// VectorAdd( pplayer->velocity, pplayer->prediction_error, est_velocity );
	if ( m_fGaitEstimation )
	{
		VectorSubtract( m_pCurrentEntity->origin, m_pPlayerInfo->prevgaitorigin, est_velocity );
		VectorCopy( m_pCurrentEntity->origin, m_pPlayerInfo->prevgaitorigin );
		m_flGaitMovement = Length( est_velocity );
		if (dt <= 0 || m_flGaitMovement / dt < 5)
		{
			m_flGaitMovement = 0;
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}
	}
	else
	{
		VectorCopy( pplayer->velocity, est_velocity );
		m_flGaitMovement = Length( est_velocity ) * dt;
	}

	if (est_velocity[1] == 0 && est_velocity[0] == 0)
	{
		float flYawDiff = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if (flYawDiff > 180)
			flYawDiff -= 360;
		if (flYawDiff < -180)
			flYawDiff += 360;

		if (dt < 0.25)
			flYawDiff *= dt * 4;
		else
			flYawDiff *= dt;

		m_pPlayerInfo->gaityaw += flYawDiff;
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - (int)(m_pPlayerInfo->gaityaw / 360) * 360;

		m_flGaitMovement = 0;
	}
	else
	{
		m_pPlayerInfo->gaityaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);
		if (m_pPlayerInfo->gaityaw > 180)
			m_pPlayerInfo->gaityaw = 180;
		if (m_pPlayerInfo->gaityaw < -180)
			m_pPlayerInfo->gaityaw = -180;
	}

}

/*
====================
StudioProcessGait

====================
*/
void CStudioModelRenderer::StudioProcessGait( entity_state_t *pplayer )
{
	mstudioseqdesc_t	*pseqdesc;
	float dt;
	int iBlend;
	float flYaw;	 // view direction relative to movement

	if (m_pCurrentEntity->curstate.sequence >=  m_pStudioHeader->numseq) 
	{
		m_pCurrentEntity->curstate.sequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	StudioPlayerBlend( pseqdesc, &iBlend, &m_pCurrentEntity->angles[PITCH] );

	m_pCurrentEntity->latched.prevangles[PITCH] = m_pCurrentEntity->angles[PITCH];
	m_pCurrentEntity->curstate.blending[0] = iBlend;
	m_pCurrentEntity->latched.prevblending[0] = m_pCurrentEntity->curstate.blending[0];
	m_pCurrentEntity->latched.prevseqblending[0] = m_pCurrentEntity->curstate.blending[0];

	// Con_DPrintf("%f %d\n", m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->blending[0] );

	dt = (m_clTime - m_clOldTime);
	if (dt < 0)
		dt = 0;
	else if (dt > 1.0)
		dt = 1;

	StudioEstimateGait( pplayer );

	// Con_DPrintf("%f %f\n", m_pCurrentEntity->angles[YAW], m_pPlayerInfo->gaityaw );

	// calc side to side turning
	flYaw = m_pCurrentEntity->angles[YAW] - m_pPlayerInfo->gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if (flYaw < -180)
		flYaw = flYaw + 360;
	if (flYaw > 180)
		flYaw = flYaw - 360;

	if (flYaw > 120)
	{
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw - 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw - 180;
	}
	else if (flYaw < -120)
	{
		m_pPlayerInfo->gaityaw = m_pPlayerInfo->gaityaw + 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw + 180;
	}

	// adjust torso
	m_pCurrentEntity->curstate.controller[0] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[1] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[2] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->curstate.controller[3] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
	m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
	m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
	m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];

	m_pCurrentEntity->angles[YAW] = m_pPlayerInfo->gaityaw;
	if (m_pCurrentEntity->angles[YAW] < -0)
		m_pCurrentEntity->angles[YAW] += 360;
	m_pCurrentEntity->latched.prevangles[YAW] = m_pCurrentEntity->angles[YAW];

	if (pplayer->gaitsequence >=  m_pStudioHeader->numseq) 
	{
		pplayer->gaitsequence = 0;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + pplayer->gaitsequence;

	// calc gait frame
	if (pseqdesc->linearmovement[0] > 0)
	{
		m_pPlayerInfo->gaitframe += (m_flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	}
	else
	{
		m_pPlayerInfo->gaitframe += pseqdesc->fps * dt;
	}

	// do modulo
	m_pPlayerInfo->gaitframe = m_pPlayerInfo->gaitframe - (int)(m_pPlayerInfo->gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if (m_pPlayerInfo->gaitframe < 0)
		m_pPlayerInfo->gaitframe += pseqdesc->numframes;
}

#if defined _TFC

#define PC_UNDEFINED	0 

#define PC_SCOUT		1 
#define PC_SNIPER		2 
#define PC_SOLDIER		3 
#define PC_DEMOMAN		4 
#define PC_MEDIC		5 
#define PC_HVYWEAP		6 
#define PC_PYRO			7
#define PC_SPY			8
#define PC_ENGINEER		9
#define PC_RANDOM		10 	
#define PC_CIVILIAN		11

#define PC_LASTCLASS	12

#define TFC_MODELS_OLD	0

extern cvar_t *tfc_newmodels;

char *sNewClassModelFiles[] =
{
	NULL,
	"models/player/scout/scout.mdl",
	"models/player/sniper/sniper.mdl",
	"models/player/soldier/soldier.mdl",
	"models/player/demo/demo.mdl",
	"models/player/medic/medic.mdl",
	"models/player/hvyweapon/hvyweapon.mdl",
	"models/player/pyro/pyro.mdl",
	"models/player/spy/spy.mdl",
	"models/player/engineer/engineer.mdl",
	"models/player/scout/scout.mdl",	// PC_RANDOM
	"models/player/civilian/civilian.mdl",
};

char *sOldClassModelFiles[] =
{
	NULL,
	"models/player/scout/scout2.mdl",
	"models/player/sniper/sniper2.mdl",
	"models/player/soldier/soldier2.mdl",
	"models/player/demo/demo2.mdl",
	"models/player/medic/medic2.mdl",
	"models/player/hvyweapon/hvyweapon2.mdl",
	"models/player/pyro/pyro2.mdl",
	"models/player/spy/spy2.mdl",
	"models/player/engineer/engineer2.mdl",
	"models/player/scout/scout2.mdl",	// PC_RANDOM
	"models/player/civilian/civilian.mdl",
};

#define NUM_WEAPON_PMODELS 18

char *sNewWeaponPModels[] =
{
	"models/p_9mmhandgun.mdl",
	"models/p_crowbar.mdl",
	"models/p_egon.mdl",
	"models/p_glauncher.mdl",
	"models/p_grenade.mdl",
	"models/p_knife.mdl",
	"models/p_medkit.mdl",
	"models/p_mini.mdl",
	"models/p_nailgun.mdl",
	"models/p_srpg.mdl",
	"models/p_shotgun.mdl",
	"models/p_snailgun.mdl",
	"models/p_sniper.mdl",
	"models/p_spanner.mdl",
	"models/p_umbrella.mdl",
	"models/p_rpg.mdl",
	"models/p_spygun.mdl",
	"models/p_smallshotgun.mdl"
};

char *sOldWeaponPModels[] =
{
	"models/p_9mmhandgun2.mdl",
	"models/p_crowbar2.mdl",
	"models/p_egon2.mdl",
	"models/p_glauncher2.mdl",
	"models/p_grenade2.mdl",
	"models/p_knife2.mdl",
	"models/p_medkit2.mdl",
	"models/p_mini2.mdl",
	"models/p_nailgun2.mdl",
	"models/p_rpg2.mdl",
	"models/p_shotgun2.mdl",
	"models/p_snailgun2.mdl",
	"models/p_sniper2.mdl",
	"models/p_spanner2.mdl",
	"models/p_umbrella.mdl",
	"models/p_rpg2.mdl",
	"models/p_9mmhandgun2.mdl",
	"models/p_shotgun2.mdl"
};


int CStudioModelRenderer :: ReturnDiguisedClass ( int iPlayerIndex )
{
	m_pRenderModel = IEngineStudio.SetupPlayerModel( iPlayerIndex );

	if ( !m_pRenderModel )
		return PC_SCOUT;
	
	for ( int i = PC_SCOUT ; i < PC_LASTCLASS ;  i++ )
	{
		if ( !strcmp ( m_pRenderModel->name, sNewClassModelFiles[ i ] ) )
			return i;
	}

	return PC_SCOUT;
}

char * ReturnCorrectedModelString ( int iSwitchClass )
{
	if ( tfc_newmodels->value == TFC_MODELS_OLD )
	{
		if ( sOldClassModelFiles[ iSwitchClass ] )
			return sOldClassModelFiles[ iSwitchClass ];
		else
			return sOldClassModelFiles[ PC_SCOUT ];
	}
	else
	{
		if ( sNewClassModelFiles[ iSwitchClass ] )
			return sNewClassModelFiles[ iSwitchClass ];
		else
			return sNewClassModelFiles[ PC_SCOUT ];
	}
}

#endif

#ifdef _TFC
float g_flSpinUpTime[ 33 ];
float g_flSpinDownTime[ 33 ];
#endif


/*
====================
StudioDrawPlayer

====================
*/
int CStudioModelRenderer::StudioDrawPlayer( int flags, entity_state_t *pplayer )
{
	m_pCurrentEntity = IEngineStudio.GetCurrentEntity();

	if (MutatorEnabled(MUTATOR_SANIC))
	{
		static TEMPENTITY *t[32];
		int c = pplayer->number - 1;
		int half_frames = 3; // 6 / 2 in sprite - (int)t[c]->frameMax / 2

		if (t[c] == NULL) {
			int model = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/sanic.spr");
			t[c] = gEngfuncs.pEfxAPI->R_DefaultSprite(pplayer->origin, model, 0);
			if (t[c]) {
				t[c]->entity.curstate.rendermode = kRenderNormal;
				t[c]->entity.curstate.scale = 0.70;
				t[c]->entity.curstate.framerate = 0;
				t[c]->entity.curstate.frame = c % half_frames;
				t[c]->clientIndex = c;
				t[c]->flags |= FTENT_PERSIST;
				t[c]->die = gEngfuncs.GetClientTime() + 15;
				t[c]->entity.curstate.renderamt = 255;
			}
		} else {
			t[c]->entity.origin = pplayer->origin;
			if (m_pCurrentEntity->curstate.health <= 0)
			{
				t[c]->entity.curstate.frame = (c % half_frames) + half_frames;
				t[c]->entity.curstate.rendermode = kRenderTransAlpha;
				if (t[c]->entity.curstate.renderamt > 0)
					t[c]->entity.curstate.renderamt -= 2;
				else
					t[c]->die = gEngfuncs.GetClientTime();
			} else
				t[c]->entity.curstate.frame = c % half_frames;
			if (t[c]->die < gEngfuncs.GetClientTime())
				t[c] = NULL;
		}

		return 1;
	}

	alight_t lighting;
	vec3_t dir;

	IEngineStudio.GetTimes( &m_nFrameCount, &m_clTime, &m_clOldTime );
	IEngineStudio.GetViewInfo( m_vRenderOrigin, m_vUp, m_vRight, m_vNormal );
	IEngineStudio.GetAliasScale( &m_fSoftwareXScale, &m_fSoftwareYScale );

	m_nPlayerIndex = pplayer->number - 1;

	if (m_nPlayerIndex < 0 || m_nPlayerIndex >= gEngfuncs.GetMaxClients())
		return 0;

	bool crate = MutatorEnabled(MUTATOR_CRATE);
	bool prophunt = m_pCurrentEntity->curstate.fuser4 > 0 && gHUD.m_GameMode == GAME_PROPHUNT;

	if (crate)
	{
		int modelindex;
		m_pRenderModel = gEngfuncs.CL_LoadModel("models/box.mdl", &modelindex );
	}

	if (prophunt)
	{
		crate = FALSE;
		int modelindex;
		m_pRenderModel = gEngfuncs.CL_LoadModel("models/w_weapons.mdl", &modelindex );
		if (m_pCurrentEntity->curstate.fuser4 > 49)
		{
			m_pRenderModel = gEngfuncs.CL_LoadModel("models/w_ammo.mdl", &modelindex );
		}
	}

#if defined( _TFC )

	int modelindex; 
	int iSwitchClass = pplayer->playerclass;

	if ( iSwitchClass == PC_SPY )
		iSwitchClass = ReturnDiguisedClass( m_nPlayerIndex );

	// do we have a "replacement_model" for this player?
	if ( pplayer->fuser1 )
	{
		m_pRenderModel = IEngineStudio.SetupPlayerModel( m_nPlayerIndex );
	}
	else
	{
		// get the model pointer using a "corrected" model string based on tfc_newmodels
		m_pRenderModel = gEngfuncs.CL_LoadModel( ReturnCorrectedModelString( iSwitchClass ), &modelindex );
	}

#else

	if (!crate && !prophunt)
		m_pRenderModel = IEngineStudio.SetupPlayerModel( m_nPlayerIndex );

#endif

	if (m_pRenderModel == NULL)
		return 0;

	m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata (m_pRenderModel);
	IEngineStudio.StudioSetHeader( m_pStudioHeader );
	IEngineStudio.SetRenderModel( m_pRenderModel );

	if (crate)
		m_pCurrentEntity->curstate.skin = m_nPlayerIndex % 8;
	
	if (prophunt)
	{
		m_pCurrentEntity->curstate.body = m_pCurrentEntity->curstate.fuser4 >= 50 ? m_pCurrentEntity->curstate.fuser4 - 49 : m_pCurrentEntity->curstate.fuser4;
		m_pCurrentEntity->curstate.skin = gHUD.m_IceModelsIndex;
		m_pCurrentEntity->origin[2] = m_pCurrentEntity->origin[2] - 36;
		m_pCurrentEntity->angles = Vector(0,0,0);
	}

	if (pplayer->gaitsequence)
	{
		vec3_t orig_angles;
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );

		//m_pCurrentEntity->curstate.angles = Vector(0,0,0);
		//m_pCurrentEntity->baseline.angles = Vector(0,0,0);
		//m_pCurrentEntity->angles = Vector(0,0,0);
		VectorCopy( m_pCurrentEntity->angles, orig_angles );
	
		StudioProcessGait( pplayer );

		m_pPlayerInfo->gaitsequence = pplayer->gaitsequence;
		m_pPlayerInfo = NULL;

		StudioSetUpTransform( 0 );
		VectorCopy( orig_angles, m_pCurrentEntity->angles );
	}
	else
	{
		m_pCurrentEntity->curstate.controller[0] = 127;
		m_pCurrentEntity->curstate.controller[1] = 127;
		m_pCurrentEntity->curstate.controller[2] = 127;
		m_pCurrentEntity->curstate.controller[3] = 127;
		m_pCurrentEntity->latched.prevcontroller[0] = m_pCurrentEntity->curstate.controller[0];
		m_pCurrentEntity->latched.prevcontroller[1] = m_pCurrentEntity->curstate.controller[1];
		m_pCurrentEntity->latched.prevcontroller[2] = m_pCurrentEntity->curstate.controller[2];
		m_pCurrentEntity->latched.prevcontroller[3] = m_pCurrentEntity->curstate.controller[3];
		
		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
		m_pPlayerInfo->gaitsequence = 0;

		StudioSetUpTransform( 0 );
	}

	if (flags & STUDIO_RENDER)
	{
		// see if the bounding box lets us trivially reject, also sets
		if (!IEngineStudio.StudioCheckBBox ())
			return 0;

		/*
		// bboxs aren't set in cold ice
		vec3_t mins, maxs;
		StudioGetMinsMaxs(mins, maxs);
		if (gFog.CullFogBBox(mins, maxs))
			return 0;
		*/

		(*m_pModelsDrawn)++;
		(*m_pStudioModelCount)++; // render data cache cookie

		if (m_pStudioHeader->numbodyparts == 0)
			return 1;
	}

	m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );
	StudioSetupBones( );
	StudioSaveBones( );
	m_pPlayerInfo->renderframe = m_nFrameCount;

	m_pPlayerInfo = NULL;

	if (flags & STUDIO_EVENTS)
	{
		StudioCalcAttachments( );
		IEngineStudio.StudioClientEvents( );
		// copy attachments into global entity array
		if ( m_pCurrentEntity->index > 0 )
		{
			cl_entity_t *ent = gEngfuncs.GetEntityByIndex( m_pCurrentEntity->index );

			memcpy( ent->attachment, m_pCurrentEntity->attachment, sizeof( vec3_t ) * 4 );
		}
	}

	if (flags & STUDIO_RENDER)
	{
		lighting.plightvec = dir;
		IEngineStudio.StudioDynamicLight(m_pCurrentEntity, &lighting );

		IEngineStudio.StudioEntityLight( &lighting );

		// model and frame independant
		IEngineStudio.StudioSetupLighting (&lighting);

		m_pPlayerInfo = IEngineStudio.PlayerInfo( m_nPlayerIndex );

#if defined _TFC

		m_nTopColor    = m_pPlayerInfo->topcolor;
		m_nBottomColor = m_pPlayerInfo->bottomcolor;

		// get old remap colors
		if ( tfc_newmodels->value == TFC_MODELS_OLD )
		{
			// team 1
			if ( ( m_nTopColor < 155 ) && ( m_nTopColor > 135 ) )
			{
				m_nTopColor    = TEAM1_COLOR;
				m_nBottomColor = TEAM1_COLOR - 10;
			}
			// team 2
			else if ( ( m_nTopColor < 260 ) && ( ( m_nTopColor > 240 ) || ( m_nTopColor == 5 ) ) )
			{
				m_nTopColor    = TEAM2_COLOR;
				m_nBottomColor = TEAM2_COLOR - 10;
			}
			// team 3
			else if ( ( m_nTopColor < 50 ) && ( m_nTopColor > 40 ) )
			{
				m_nTopColor    = TEAM3_COLOR;
				m_nBottomColor = TEAM3_COLOR - 10;
			}
			// team 4
			else if ( ( m_nTopColor < 110 )  && ( m_nTopColor > 75 ) )
			{
				m_nTopColor    = TEAM4_COLOR;
				m_nBottomColor = TEAM4_COLOR - 10;
			}
		}

#else
		// get remap colors
		m_nTopColor    = m_pPlayerInfo->topcolor;
		m_nBottomColor = m_pPlayerInfo->bottomcolor;

#endif

		// bounds check
		if (m_nTopColor < 0)
			m_nTopColor = 0;
		if (m_nTopColor > 360)
			m_nTopColor = 360;
		if (m_nBottomColor < 0)
			m_nBottomColor = 0;
		if (m_nBottomColor > 360)
			m_nBottomColor = 360;

		IEngineStudio.StudioSetRemapColors( m_nTopColor, m_nBottomColor );

		StudioRenderModel( );

		FlameSystem.Process(m_pCurrentEntity, IEngineStudio);

		m_pPlayerInfo = NULL;

		if (!crate && !prophunt && pplayer->weaponmodel)
		{
			cl_entity_t saveent = *m_pCurrentEntity;

			model_t *pweaponmodel = IEngineStudio.GetModelByIndex( pplayer->weaponmodel );

#if defined _TFC
			if ( pweaponmodel )
			{
				// if we want to see the old p_models
				if ( tfc_newmodels->value == TFC_MODELS_OLD )
				{
					for ( int i = 0 ; i < NUM_WEAPON_PMODELS ; ++i )
					{
						if ( !stricmp( pweaponmodel->name, sNewWeaponPModels[i] ) )
						{
							gEngfuncs.CL_LoadModel(  sOldWeaponPModels[i] , &modelindex );
							pweaponmodel = IEngineStudio.GetModelByIndex( modelindex );
							break;
						}
					}
				}
			}
#endif
			m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata (pweaponmodel);
			IEngineStudio.StudioSetHeader( m_pStudioHeader );

#ifdef _TFC		
			//Do spinning stuff for the HWGuy minigun
			if ( strstr ( m_pStudioHeader->name, "p_mini.mdl" ) )
			{
				if ( g_flSpinUpTime[ m_nPlayerIndex ] && g_flSpinUpTime[ m_nPlayerIndex ] > gEngfuncs.GetClientTime() )
				{
					float flmod = ( g_flSpinUpTime[ m_nPlayerIndex ] - ( gEngfuncs.GetClientTime() + 3.5 ) );
					flmod *= -30;
				
					m_pCurrentEntity->curstate.frame = flmod;
					m_pCurrentEntity->curstate.sequence = 2;
				}
				
				else if ( g_flSpinUpTime[ m_nPlayerIndex ] && g_flSpinUpTime[ m_nPlayerIndex ] <= gEngfuncs.GetClientTime() )
					g_flSpinUpTime[ m_nPlayerIndex ] = 0.0;
								
				else if ( g_flSpinDownTime[ m_nPlayerIndex ] && g_flSpinDownTime[ m_nPlayerIndex ] > gEngfuncs.GetClientTime() && !g_flSpinUpTime[ m_nPlayerIndex ] )
				{
					float flmod = ( g_flSpinDownTime[ m_nPlayerIndex ] - ( gEngfuncs.GetClientTime() + 3.5 ) );
					flmod *= -30;
				
					m_pCurrentEntity->curstate.frame = flmod;
					m_pCurrentEntity->curstate.sequence = 3;
				}
				
				else if ( g_flSpinDownTime[ m_nPlayerIndex ] && g_flSpinDownTime[ m_nPlayerIndex ] <= gEngfuncs.GetClientTime() && !g_flSpinUpTime[ m_nPlayerIndex ] )
					g_flSpinDownTime[ m_nPlayerIndex ] = 0.0;

				if ( m_pCurrentEntity->curstate.sequence == 70 || m_pCurrentEntity->curstate.sequence == 72 )
				{
					if ( g_flSpinUpTime[ m_nPlayerIndex ] )
						g_flSpinUpTime[ m_nPlayerIndex ] = 0.0;
				
					m_pCurrentEntity->curstate.sequence = 1;
				}

				StudioSetupBones( );
			}
			else
			{
				if ( g_flSpinUpTime[ m_nPlayerIndex ] || g_flSpinDownTime[ m_nPlayerIndex ] )
				{
					g_flSpinUpTime[ m_nPlayerIndex ] = 0.0;
					g_flSpinDownTime[ m_nPlayerIndex ] = 0.0;
				}
			}

#endif

			StudioMergeBones( pweaponmodel );

			IEngineStudio.StudioSetupLighting (&lighting);

			m_pCurrentEntity->curstate.skin = gHUD.m_IceModelsIndex;
			m_pCurrentEntity->curstate.body = pplayer->team;
			StudioRenderModel( );

			StudioCalcAttachments( );

			*m_pCurrentEntity = saveent;
		}

		int hatindex = 0;
		model_t *hatmodel;
		int body = 0;

		if (!crate && !prophunt)
		{
			if (MutatorEnabled(MUTATOR_JACK)) {
				gEngfuncs.CL_LoadModel("models/hats.mdl", &hatindex);
				body = 3;
			} else if (MutatorEnabled(MUTATOR_SANTAHAT)) {
				gEngfuncs.CL_LoadModel("models/hats.mdl", &hatindex);
				body = 0;
			} else if (MutatorEnabled(MUTATOR_PIRATEHAT)) {
				gEngfuncs.CL_LoadModel("models/hats.mdl", &hatindex);
				body = 1;
			} else if (MutatorEnabled(MUTATOR_MARSHMELLO)) {
				gEngfuncs.CL_LoadModel("models/hats.mdl", &hatindex);
				body = 2;
			} else if (MutatorEnabled(MUTATOR_PUMPKIN)) {
				gEngfuncs.CL_LoadModel("models/hats.mdl", &hatindex);
				body = 4;
			} else if (MutatorEnabled(MUTATOR_COWBOY)) {
				gEngfuncs.CL_LoadModel("models/hats.mdl", &hatindex);
				body = 5;
			}
		}

		if (hatindex)
		{
			cl_entity_t saveent = *m_pCurrentEntity;
			hatmodel = IEngineStudio.GetModelByIndex(hatindex);
			m_pStudioHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata(hatmodel);
			IEngineStudio.StudioSetHeader(m_pStudioHeader);
			m_pCurrentEntity->curstate.body = body;
			StudioMergeBones(hatmodel);
			IEngineStudio.StudioSetupLighting (&lighting);
			StudioRenderModel( );
			StudioCalcAttachments( );

			*m_pCurrentEntity = saveent;
		}
	}

	return 1;
}

/*
====================
StudioCalcAttachments

====================
*/
void CStudioModelRenderer::StudioCalcAttachments( void )
{
	int i;
	mstudioattachment_t *pattachment;

	if ( m_pStudioHeader->numattachments > 4 )
	{
		gEngfuncs.Con_DPrintf( "Too many attachments on %s\n", m_pCurrentEntity->model->name );
		exit( -1 );
	}

	vec3_t oldAim = m_pCurrentEntity->model->aim_punch;
	vec3_t oldAngles = m_pCurrentEntity->model->aim_angles;

	// calculate attachment points
	pattachment = (mstudioattachment_t *)((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);
	for (i = 0; i < m_pStudioHeader->numattachments; i++)
	{
		VectorTransform( pattachment[i].org, (*m_plighttransform)[pattachment[i].bone],  m_pCurrentEntity->attachment[i] );

		if (m_pCurrentEntity == gEngfuncs.GetViewModel())
		{
			if (i == 1 && m_pCurrentEntity->model->aim_punch != pattachment[1].org) {
				m_pCurrentEntity->model->aim_punch = pattachment[i].org;
#ifdef _DEBUG
				char str[256];
				sprintf(str, "Loaded aim values! %d: %.1f, %.1f, %.1f\n",
				i, m_pCurrentEntity->model->aim_punch.x, m_pCurrentEntity->model->aim_punch.y, m_pCurrentEntity->model->aim_punch.z);
				gEngfuncs.pfnConsolePrint(str);
#endif
			}

			if (i == 2 && m_pCurrentEntity->model->aim_angles != pattachment[2].org) {
				m_pCurrentEntity->model->aim_angles = pattachment[2].org;
#ifdef _DEBUG
				char str[256];
				sprintf(str, "Loaded angles values! %d: %.1f, %.1f, %.1f\n",
				i, m_pCurrentEntity->model->aim_angles.x, m_pCurrentEntity->model->aim_angles.y, m_pCurrentEntity->model->aim_angles.z);
				gEngfuncs.pfnConsolePrint(str);
#endif
			}
		}
	}

	if (m_pCurrentEntity == gEngfuncs.GetViewModel()) {
		if (i < 2 && Length(m_pCurrentEntity->model->aim_punch) > 1 && m_pCurrentEntity->model->aim_punch == oldAim) {
			m_pCurrentEntity->model->aim_punch = Vector(0,0,0);
#ifdef _DEBUG
			char str[256];
			sprintf(str, "Reset aim! model is: %s\n", m_pCurrentEntity->model->name);
			ConsolePrint(str);
#endif
		}

		if (i < 3 && Length(m_pCurrentEntity->model->aim_angles) > 1 && m_pCurrentEntity->model->aim_angles == oldAngles) {
			m_pCurrentEntity->model->aim_angles = Vector(0,0,0);
#ifdef _DEBUG
			char str[256];
			sprintf(str, "Reset angles! model is: %s\n", m_pCurrentEntity->model->name);
			ConsolePrint(str);
#endif
		}
	}

#ifndef __APPLE__
	if (cl_portalmirror && cl_portalmirror->value)
	{
		if (!strcmp(m_pCurrentEntity->model->name, "models/w_portal.mdl"))
		{
			for (int i = 0; i < 4; i++)
			{
				gPortalRenderer.m_PortalVertex[m_pCurrentEntity->curstate.skin][i] = m_pCurrentEntity->attachment[i];
			}
		}
	}
#endif
}

/*
====================
StudioRenderModel

====================
*/
void CStudioModelRenderer::StudioRenderModel( void )
{
	IEngineStudio.SetChromeOrigin();
	IEngineStudio.SetForceFaceFlags( 0 );

	if ( m_pCurrentEntity->curstate.renderfx == kRenderFxGlowShell )
	{
		m_pCurrentEntity->curstate.renderfx = kRenderFxNone;
		StudioRenderFinal( );
		
		if ( !IEngineStudio.IsHardware() )
		{
			gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
		}

		IEngineStudio.SetForceFaceFlags( STUDIO_NF_CHROME );

		gEngfuncs.pTriAPI->SpriteTexture( m_pChromeSprite, 0 );
		m_pCurrentEntity->curstate.renderfx = kRenderFxGlowShell;

		StudioRenderFinal( );
		if ( !IEngineStudio.IsHardware() )
		{
			gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
		}
	}
	else
	{
		StudioRenderFinal( );
	}
}

/*
====================
StudioRenderFinal_Software

====================
*/
void CStudioModelRenderer::StudioRenderFinal_Software( void )
{
	int i;

	// Note, rendermode set here has effect in SW
	IEngineStudio.SetupRenderer( 0 ); 

	if (m_pCvarDrawEntities->value == 2)
	{
		IEngineStudio.StudioDrawBones( );
	}
	else if (m_pCvarDrawEntities->value == 3)
	{
		IEngineStudio.StudioDrawHulls( );
	}
	else
	{
		for (i=0 ; i < m_pStudioHeader->numbodyparts ; i++)
		{
			IEngineStudio.StudioSetupModel( i, (void **)&m_pBodyPart, (void **)&m_pSubModel );
			IEngineStudio.StudioDrawPoints( );
		}
	}

	if (m_pCvarDrawEntities->value == 4)
	{
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
		IEngineStudio.StudioDrawHulls( );
		gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	}

	if (m_pCvarDrawEntities->value == 5)
	{
		IEngineStudio.StudioDrawAbsBBox( );
	}
	
	IEngineStudio.RestoreRenderer();
}

/*
====================
StudioRenderFinal_Hardware

====================
*/
void CStudioModelRenderer::StudioRenderFinal_Hardware( void )
{
	int i;
	int iShadows = CVAR_GET_FLOAT("cl_shadows");
	int rendermode;

	rendermode = IEngineStudio.GetForceFaceFlags() ? kRenderTransAdd : m_pCurrentEntity->curstate.rendermode;
	IEngineStudio.SetupRenderer( rendermode );
	
	if (m_pCvarDrawEntities->value == 2)
	{
		IEngineStudio.StudioDrawBones();
	}
	else if (m_pCvarDrawEntities->value == 3)
	{
		IEngineStudio.StudioDrawHulls();
	}
	else
	{
		for (i=0 ; i < m_pStudioHeader->numbodyparts ; i++)
		{
			IEngineStudio.StudioSetupModel( i, (void **)&m_pBodyPart, (void **)&m_pSubModel );

			if (m_fDoInterp)
			{
				// interpolation messes up bounding boxes.
				m_pCurrentEntity->trivial_accept = 0; 
			}

			IEngineStudio.GL_SetRenderMode( rendermode );

			extern cvar_t *m_pCvarRighthand;
			extern qboolean g_fXashEngine;
			if (m_pCurrentEntity == gEngfuncs.GetViewModel() || m_pCurrentEntity->curstate.iuser4 == 1) {
				if (!m_pCvarRighthand->value || g_fXashEngine)
					gEngfuncs.pTriAPI->CullFace( TRI_NONE );
			}

			if ((m_pCurrentEntity->curstate.effects & EF_VIEWMODEL) != 0)
			{
				// tune this if needed
				// the real viewmodel will always be always visible
				// and draw over the fake one
#ifndef __APPLE__
				glDepthRange(0.0f, 0.4f);
#endif
			}
			IEngineStudio.StudioDrawPoints();
			if ((m_pCurrentEntity->curstate.effects & EF_VIEWMODEL) != 0)
			{
#ifndef __APPLE__
				glDepthRange(0.0f, 1.0f);
#endif
			}

			IEngineStudio.GL_StudioDrawShadow();

#ifdef _WIN32
			if (iShadows == 1 && !g_fXashEngine)
			{
				GL_StudioDrawShadow = (void(*)(void))(((unsigned int)IEngineStudio.GL_StudioDrawShadow) + 35);

				if (IEngineStudio.GetCurrentEntity() != gEngfuncs.GetViewModel())
					ShadowHack();
			}
#endif

			if (m_pCurrentEntity == gEngfuncs.GetViewModel() || m_pCurrentEntity->curstate.iuser4 == 1) {
				if (!m_pCvarRighthand->value || g_fXashEngine)
					gEngfuncs.pTriAPI->CullFace( TRI_FRONT );
			}
		}
	}

	if ( m_pCvarDrawEntities->value == 4 )
	{
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
		IEngineStudio.StudioDrawHulls( );
		gEngfuncs.pTriAPI->RenderMode( kRenderNormal );
	}

	IEngineStudio.RestoreRenderer();
}

/*
====================
StudioRenderFinal

====================
*/
void CStudioModelRenderer::StudioRenderFinal(void)
{
	if ( IEngineStudio.IsHardware() )
	{
		StudioRenderFinal_Hardware();
	}
	else
	{
		StudioRenderFinal_Software();
	}
}

/*
====================
StudioGetMinsMaxs

====================
*/
void CStudioModelRenderer::StudioGetMinsMaxs( Vector& outMins, Vector& outMaxs )
{
	if (m_pCurrentEntity->curstate.sequence >= m_pStudioHeader->numseq)
		m_pCurrentEntity->curstate.sequence = 0;

	// Build full bounding box
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->curstate.sequence;

	Vector vTemp;
	static Vector vBounds[8];

	for (int i = 0; i < 8; i++)
	{
		if ( i & 1 ) vTemp[0] = pseqdesc->bbmin[0];
		else vTemp[0] = pseqdesc->bbmax[0];
		if ( i & 2 ) vTemp[1] = pseqdesc->bbmin[1];
		else vTemp[1] = pseqdesc->bbmax[1];
		if ( i & 4 ) vTemp[2] = pseqdesc->bbmin[2];
		else vTemp[2] = pseqdesc->bbmax[2];
		VectorCopy( vTemp, vBounds[i] );
	}

	float rotationMatrix[3][4];
	m_pCurrentEntity->angles[PITCH] = -m_pCurrentEntity->angles[PITCH];
	AngleMatrix(m_pCurrentEntity->angles, rotationMatrix);
	m_pCurrentEntity->angles[PITCH] = -m_pCurrentEntity->angles[PITCH];

	for (int i = 0; i < 8; i++ )
	{
		VectorCopy(vBounds[i], vTemp);
		VectorRotate(vTemp, rotationMatrix, vBounds[i]);
	}

	// Set the bounding box
	outMins = Vector(9999, 9999, 9999);
	outMaxs = Vector(-9999, -9999, -9999);
	for(int i = 0; i < 8; i++)
	{
		// Mins
		if(vBounds[i][0] < outMins[0]) outMins[0] = vBounds[i][0];
		if(vBounds[i][1] < outMins[1]) outMins[1] = vBounds[i][1];
		if(vBounds[i][2] < outMins[2]) outMins[2] = vBounds[i][2];

		// Maxs
		if(vBounds[i][0] > outMaxs[0]) outMaxs[0] = vBounds[i][0];
		if(vBounds[i][1] > outMaxs[1]) outMaxs[1] = vBounds[i][1];
		if(vBounds[i][2] > outMaxs[2]) outMaxs[2] = vBounds[i][2];
	}

	VectorAdd(outMins, m_pCurrentEntity->origin, outMins);
	VectorAdd(outMaxs, m_pCurrentEntity->origin, outMaxs);
}
