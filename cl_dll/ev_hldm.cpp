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
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "entity_types.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_materials.h"

#include "eventscripts.h"
#include "ev_hldm.h"

#include "r_efx.h"
#include "event_api.h"
#include "event_args.h"
#include "in_defs.h"

#include <string.h>

#include "r_studioint.h"
#include "com_model.h"

extern engine_studio_api_t IEngineStudio;

static int tracerCount[ 32 ];

extern "C"
{
#include "pm_shared.h"
}

void V_PunchAxis( int axis, float punch );
void VectorAngles( const float *forward, float *angles );

extern cvar_t *cl_lw;
extern cvar_t *m_pCvarRighthand;
extern cvar_t *cl_bulletsmoke;
extern cvar_t *cl_gunsmoke;
extern cvar_t *cl_icemodels;

extern "C"
{

// HLDM
void EV_FireGlock1( struct event_args_s *args  );
void EV_FireGlock2( struct event_args_s *args  );
void EV_FireShotGunSingle( struct event_args_s *args  );
void EV_FireShotGunDouble( struct event_args_s *args  );
void EV_FireMP5( struct event_args_s *args  );
void EV_FireMP52( struct event_args_s *args  );
void EV_FirePython( struct event_args_s *args  );
void EV_FireGauss( struct event_args_s *args  );
void EV_SpinGauss( struct event_args_s *args  );
void EV_Crowbar( struct event_args_s *args  );
void EV_FireCrossbow( struct event_args_s *args  );
void EV_FireCrossbow2( struct event_args_s *args  );
void EV_FireRpg( struct event_args_s *args  );
void EV_FireRpgExtreme( struct event_args_s *args  );
void EV_EgonFire( struct event_args_s *args  );
void EV_EgonStop( struct event_args_s *args  );
void EV_HornetGunFire( struct event_args_s *args  );
void EV_TripmineFire( struct event_args_s *args  );
void EV_SnarkFire( struct event_args_s *args  );

// Ice
void EV_Knife( struct event_args_s *args );
void EV_ChumtoadFire( struct event_args_s *args );
void EV_ChumtoadRelease( struct event_args_s *args );
void EV_FireSniperRifle( struct event_args_s *args );
void EV_FireCannon( struct event_args_s *args );
void EV_FireCannonFlak( struct event_args_s *args );
void EV_FireMag60( struct event_args_s *args  );
void EV_FireChaingun( struct event_args_s *args  );
void EV_FireGrenadeLauncher( struct event_args_s *args  );
void EV_FireSmg( struct event_args_s *args  );
void EV_FireUsas( struct event_args_s *args  );
void EV_Fists( struct event_args_s *args  );
void EV_Wrench( struct event_args_s *args  );
void EV_Chainsaw( struct event_args_s *args  );
void EV_Fire12GaugeSingle( struct event_args_s *args  );
void EV_FireNuke( struct event_args_s *args  );
void EV_SnarkRelease ( struct event_args_s *args );
void EV_FireDeagle( struct event_args_s *args  );
void EV_FireDualDeagle( struct event_args_s *args  );
void EV_FireDualDeagleBoth( struct event_args_s *args  );
void EV_FireDualRpgBoth( struct event_args_s *args  );
void EV_FireDualMag60( struct event_args_s *args  );
void EV_FireDualSmg( struct event_args_s *args  );
void EV_FireDualWrench( struct event_args_s *args  );
void EV_FireDualUsas( struct event_args_s *args  );
void EV_FireDualUsasBoth( struct event_args_s *args  );
void EV_FireFreezeGun( struct event_args_s *args  );
void EV_RocketCrowbar( struct event_args_s *args  );
void EV_GravityGun( struct event_args_s *args  );
void EV_FireFlameStream( struct event_args_s *args  );
void EV_FireFlameThrower( struct event_args_s *args  );
void EV_EndFlameThrower( struct event_args_s *args  );
void EV_FireDualFlameStream( struct event_args_s *args  );
void EV_FireDualFlameThrower( struct event_args_s *args  );
void EV_EndDualFlameThrower( struct event_args_s *args  );
void EV_FireSawedOff( struct event_args_s *args  );
void EV_FireSawedOffDouble( struct event_args_s *args  );

void EV_TrainPitchAdjust( struct event_args_s *args );
}

#define VECTOR_CONE_1DEGREES Vector( 0.00873, 0.00873, 0.00873 )
#define VECTOR_CONE_2DEGREES Vector( 0.01745, 0.01745, 0.01745 )
#define VECTOR_CONE_3DEGREES Vector( 0.02618, 0.02618, 0.02618 )
#define VECTOR_CONE_4DEGREES Vector( 0.03490, 0.03490, 0.03490 )
#define VECTOR_CONE_5DEGREES Vector( 0.04362, 0.04362, 0.04362 )
#define VECTOR_CONE_6DEGREES Vector( 0.05234, 0.05234, 0.05234 )
#define VECTOR_CONE_7DEGREES Vector( 0.06105, 0.06105, 0.06105 )	
#define VECTOR_CONE_8DEGREES Vector( 0.06976, 0.06976, 0.06976 )
#define VECTOR_CONE_9DEGREES Vector( 0.07846, 0.07846, 0.07846 )
#define VECTOR_CONE_10DEGREES Vector( 0.08716, 0.08716, 0.08716 )
#define VECTOR_CONE_15DEGREES Vector( 0.13053, 0.13053, 0.13053 )
#define VECTOR_CONE_20DEGREES Vector( 0.17365, 0.17365, 0.17365 )

#define VectorAverage(a, b, o) {((o)[0] = ((a)[0] + (b)[0]) * 0.5, (o)[1] = ((a)[1] + (b)[1]) * 0.5, (o)[2] = ((a)[2] + (b)[2]) * 0.5);}

// play a strike sound based on the texture that was hit by the attack traceline.  VecSrc/VecEnd are the
// original traceline endpoints used by the attacker, iBulletType is the type of bullet that hit the texture.
// returns volume of strike instrument (crowbar) to play
float EV_HLDM_PlayTextureSound( int idx, pmtrace_t *ptr, float *vecSrc, float *vecEnd, int iBulletType )
{
	// hit the world, try to play sound based on texture material type
	char chTextureType = CHAR_TEX_CONCRETE;
	float fvol;
	float fvolbar;
	char *rgsz[4];
	int cnt;
	float fattn = ATTN_NORM;
	int entity;
	char *pTextureName;
	char texname[ 64 ];
	char szbuffer[ 64 ];

	entity = gEngfuncs.pEventAPI->EV_IndexFromTrace( ptr );

	// FIXME check if playtexture sounds movevar is set
	//

	chTextureType = 0;

	// Player
	if ( entity >= 1 && entity <= gEngfuncs.GetMaxClients() )
	{
		// hit body
		chTextureType = CHAR_TEX_FLESH;
	}
	else if ( entity == 0 )
	{
		// get texture from entity or world (world is ent(0))
		pTextureName = (char *)gEngfuncs.pEventAPI->EV_TraceTexture( ptr->ent, vecSrc, vecEnd );
		
		if ( pTextureName )
		{
			strcpy( texname, pTextureName );
			pTextureName = texname;

			// strip leading '-0' or '+0~' or '{' or '!'
			if (*pTextureName == '-' || *pTextureName == '+')
			{
				pTextureName += 2;
			}

			if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
			{
				pTextureName++;
			}
			
			// '}}'
			strcpy( szbuffer, pTextureName );
			szbuffer[ CBTEXTURENAMEMAX - 1 ] = 0;
				
			// get texture type
			chTextureType = PM_FindTextureType( szbuffer );	
		}
	}
	
	switch (chTextureType)
	{
	default:
	case CHAR_TEX_CONCRETE: fvol = 0.9;	fvolbar = 0.6;
		rgsz[0] = "player/pl_step1.wav";
		rgsz[1] = "player/pl_step2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_METAL: fvol = 0.9; fvolbar = 0.3;
		rgsz[0] = "player/pl_metal1.wav";
		rgsz[1] = "player/pl_metal2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_DIRT:	fvol = 0.9; fvolbar = 0.1;
		rgsz[0] = "player/pl_dirt1.wav";
		rgsz[1] = "player/pl_dirt2.wav";
		rgsz[2] = "player/pl_dirt3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_VENT:	fvol = 0.5; fvolbar = 0.3;
		rgsz[0] = "player/pl_duct1.wav";
		rgsz[1] = "player/pl_duct1.wav";
		cnt = 2;
		break;
	case CHAR_TEX_GRATE: fvol = 0.9; fvolbar = 0.5;
		rgsz[0] = "player/pl_grate1.wav";
		rgsz[1] = "player/pl_grate4.wav";
		cnt = 2;
		break;
	case CHAR_TEX_TILE:	fvol = 0.8; fvolbar = 0.2;
		rgsz[0] = "player/pl_tile1.wav";
		rgsz[1] = "player/pl_tile3.wav";
		rgsz[2] = "player/pl_tile2.wav";
		rgsz[3] = "player/pl_tile4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_SLOSH: fvol = 0.9; fvolbar = 0.0;
		rgsz[0] = "player/pl_slosh1.wav";
		rgsz[1] = "player/pl_slosh3.wav";
		rgsz[2] = "player/pl_slosh2.wav";
		rgsz[3] = "player/pl_slosh4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_WOOD: fvol = 0.9; fvolbar = 0.2;
		rgsz[0] = "debris/wood1.wav";
		rgsz[1] = "debris/wood2.wav";
		rgsz[2] = "debris/wood3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_GLASS:
	case CHAR_TEX_COMPUTER:
		fvol = 0.8; fvolbar = 0.2;
		rgsz[0] = "debris/glass1.wav";
		rgsz[1] = "debris/glass2.wav";
		rgsz[2] = "debris/glass3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_FLESH:
		if (iBulletType == BULLET_PLAYER_CROWBAR)
			return 0.0; // crowbar already makes this sound
		fvol = 1.0;	fvolbar = 0.2;
		rgsz[0] = "weapons/bullet_hit1.wav";
		rgsz[1] = "weapons/bullet_hit2.wav";
		fattn = 1.0;
		cnt = 2;
		break;
	case CHAR_TEX_SNOW:	fvol = 0.9; fvolbar = 0.1;
		rgsz[0] = "player/pl_snow1.wav";
		rgsz[1] = "player/pl_snow2.wav";
		rgsz[2] = "player/pl_snow3.wav";
		rgsz[3] = "player/pl_snow4.wav";
		cnt = 4;
		break;
	}

	// play material hit sound
	gEngfuncs.pEventAPI->EV_PlaySound( 0, ptr->endpos, CHAN_STATIC, rgsz[gEngfuncs.pfnRandomLong(0,cnt-1)], fvol, fattn, 0, 96 + gEngfuncs.pfnRandomLong(0,0xf) );
	return fvolbar;
}

char *EV_HLDM_DamageDecal( physent_t *pe )
{
	static char decalname[ 32 ];
	int idx;

	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
	if (player && (player->curstate.eflags & EFLAG_PAINTBALL) != 0) {
		idx = gEngfuncs.pfnRandomLong( 0, 7 );
		sprintf( decalname, "{paint%i", idx + 1 );
	} else {
		if ( pe->classnumber == 1 )
		{
			idx = gEngfuncs.pfnRandomLong( 0, 2 );
			sprintf( decalname, "{break%i", idx + 1 );
		}
		else if ( pe->rendermode != kRenderNormal )
		{
			sprintf( decalname, "{bproof1" );
		}
		else
		{
			idx = gEngfuncs.pfnRandomLong( 0, 4 );
			sprintf( decalname, "{bshot%i", idx + 1 );
		}
	}
	return decalname;
}

void EV_HLDM_GunshotDecalTrace( pmtrace_t *pTrace, char *decalName )
{
	int iRand;
	physent_t *pe;

	gEngfuncs.pEfxAPI->R_BulletImpactParticles( pTrace->endpos );

	iRand = gEngfuncs.pfnRandomLong(0,0x7FFF);
	if ( iRand < (0x7fff/2) )// not every bullet makes a sound.
	{
		switch( iRand % 5)
		{
		case 0:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); break;
		case 1:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric2.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); break;
		case 2:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric3.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); break;
		case 3:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric4.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); break;
		case 4:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric5.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); break;
		}
	}

	pe = gEngfuncs.pEventAPI->EV_GetPhysent( pTrace->ent );

	// Only decal brush models such as the world etc.
	if (  decalName && decalName[0] && pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) )
	{
		if ( CVAR_GET_FLOAT( "r_decals" ) )
		{
			gEngfuncs.pEfxAPI->R_DecalShoot( 
				gEngfuncs.pEfxAPI->Draw_DecalIndex( gEngfuncs.pEfxAPI->Draw_DecalIndexFromName( decalName ) ), 
				gEngfuncs.pEventAPI->EV_IndexFromTrace( pTrace ), 0, pTrace->endpos, 0 );
		}
	}
}

void EV_HLDM_DecalGunshot( pmtrace_t *pTrace, int iBulletType )
{
	physent_t *pe;

	pe = gEngfuncs.pEventAPI->EV_GetPhysent( pTrace->ent );

	if ( pe && pe->solid == SOLID_BSP )
	{
		switch( iBulletType )
		{
		case BULLET_PLAYER_9MM:
		case BULLET_MONSTER_9MM:
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_PLAYER_BUCKSHOT:
		case BULLET_PLAYER_357:
		default:
			// smoke and decal
			EV_HLDM_GunshotDecalTrace( pTrace, EV_HLDM_DamageDecal( pe ) );
			break;
		}
	}
}

int EV_HLDM_CheckTracer( int idx, float *vecSrc, float *end, float *forward, float *right, int iBulletType, int iTracerFreq, int *tracerCount )
{
	int tracer = 0;
	int i;
	qboolean player = idx >= 1 && idx <= gEngfuncs.GetMaxClients() ? true : false;

	if ( iTracerFreq != 0 && ( (*tracerCount)++ % iTracerFreq) == 0 )
	{
		vec3_t vecTracerSrc;

		if ( player )
		{
			vec3_t offset( 0, 0, -4 );

			// adjust tracer position for player
			for ( i = 0; i < 3; i++ )
			{
				vecTracerSrc[ i ] = vecSrc[ i ] + offset[ i ] + right[ i ] * 2 + forward[ i ] * 16;
			}
		}
		else
		{
			VectorCopy( vecSrc, vecTracerSrc );
		}
		
		if ( iTracerFreq != 1 )		// guns that always trace also always decal
			tracer = 1;

		switch( iBulletType )
		{
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_MONSTER_9MM:
		case BULLET_MONSTER_12MM:
		default:
			EV_CreateTracer( vecTracerSrc, end );
			break;
		}
	}

	return tracer;
}


/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.
================
*/
void EV_HLDM_FireBullets( int idx, float *forward, float *right, float *up, int cShots, float *vecSrc, float *vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int *tracerCount, float flSpreadX, float flSpreadY )
{
	int i;
	pmtrace_t tr;
	int iShot;
	int tracer;
	
	for ( iShot = 1; iShot <= cShots; iShot++ )	
	{
		vec3_t vecDir, vecEnd, vecGunPosition;
			
		float x, y, z;
		//We randomize for the Shotgun.
		if ( iBulletType == BULLET_PLAYER_BUCKSHOT || iBulletType == BULLET_PLAYER_BUCKSHOT_SPECIAL )
		{
			do {
				x = gEngfuncs.pfnRandomFloat(-0.5,0.5) + gEngfuncs.pfnRandomFloat(-0.5,0.5);
				y = gEngfuncs.pfnRandomFloat(-0.5,0.5) + gEngfuncs.pfnRandomFloat(-0.5,0.5);
				z = x*x+y*y;
			} while (z > 1);

			for ( i = 0 ; i < 3; i++ )
			{
				vecDir[i] = vecDirShooting[i] + x * flSpreadX * right[ i ] + y * flSpreadY * up [ i ];
				vecEnd[i] = vecSrc[ i ] + flDistance * vecDir[ i ];
				vecGunPosition[i] = vecSrc[i] + forward[ i ] + up [ i ] + right[i];
			}

			// sparks
			if (iBulletType == BULLET_PLAYER_BUCKSHOT_SPECIAL)
			{
				if (cl_bulletsmoke && cl_bulletsmoke->value && gEngfuncs.pfnRandomLong(0, 1))
				{
					int model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/hotglow.spr");
					if (cl_icemodels && cl_icemodels->value)
						model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/ice_hotglow.spr");
					gEngfuncs.pEfxAPI->R_TempSprite(vecGunPosition, vecDir * gEngfuncs.pfnRandomLong(400, 600),
													0.018, model, kRenderTransAdd, kRenderFxNoDissipation,
													gEngfuncs.pfnRandomLong(250, 255), 0.25, FTENT_SLOWGRAVITY | FTENT_FADEOUT);
				}
			}
		}//But other guns already have their spread randomized in the synched spread.
		else
		{

			for ( i = 0 ; i < 3; i++ )
			{
				vecDir[i] = vecDirShooting[i] + flSpreadX * right[ i ] + flSpreadY * up [ i ];
				vecEnd[i] = vecSrc[ i ] + flDistance * vecDir[ i ];
			}
		}

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
	
		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();
	
		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr );

		tracer = EV_HLDM_CheckTracer( idx, vecSrc, tr.endpos, forward, right, iBulletType, iTracerFreq, tracerCount );

		// do damage, paint decals
		if ( tr.fraction != 1.0 )
		{
			switch(iBulletType)
			{
			default:
			case BULLET_PLAYER_9MM:		
				
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
			
					break;
			case BULLET_PLAYER_MP5:		
				
				if ( !tracer )
				{
					EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
					EV_HLDM_DecalGunshot( &tr, iBulletType );
				}
				break;
			case BULLET_PLAYER_BUCKSHOT:
			case BULLET_PLAYER_BUCKSHOT_SPECIAL:
				
				EV_HLDM_DecalGunshot( &tr, iBulletType );
			
				break;
			case BULLET_PLAYER_357:
				
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				
				break;

			}
		}

		if (cl_bulletsmoke && cl_bulletsmoke->value) {
			physent_t *pe = gEngfuncs.pEventAPI->EV_GetPhysent( tr.ent );
			if (pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP )) {
				if ( gEngfuncs.pfnRandomLong(0, 3) > 2 ) {
					int model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/gunsmoke.spr" );
					TEMPENTITY *t = gEngfuncs.pEfxAPI->R_DefaultSprite(tr.endpos - Vector(forward[0], forward[1], forward[2]) * 20, model, gEngfuncs.pfnRandomLong(12, 18));
					if (t) {
						t->entity.curstate.rendermode = kRenderTransAdd;
						t->entity.curstate.renderamt = gEngfuncs.pfnRandomLong(120, 140);
						t->entity.curstate.scale = gEngfuncs.pfnRandomFloat(0.2, 0.3);
					}
				}

				if ( gEngfuncs.pfnRandomLong(0, 3) > 2 ) {
					int model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/sparks.spr" );
					if (cl_icemodels && cl_icemodels->value)
						model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/ice_sparks.spr" );
					TEMPENTITY *t = gEngfuncs.pEfxAPI->R_DefaultSprite(tr.endpos - Vector(forward[0], forward[1], forward[2]) * 40, model, gEngfuncs.pfnRandomLong(12, 18));
					if (t) {
						t->entity.curstate.rendermode = kRenderTransAdd;
						t->entity.curstate.renderamt = gEngfuncs.pfnRandomLong(220, 240);
						t->entity.curstate.scale = gEngfuncs.pfnRandomFloat(0.06, 0.08);
					}
				}
			}
		}

		gEngfuncs.pEventAPI->EV_PopPMStates();
	}
}

void EV_GunSmoke(vec3_t smokeOrigin, float scale, int idx, int ducking, float *forward, float *up, float *right, float fScale, float uScale, float rScale) {
	if (cl_gunsmoke && !cl_gunsmoke->value) {
		return;
	}

	if ( gEngfuncs.pfnRandomLong(0, 3) > 2 ) {
		if ( EV_IsLocal(idx) ) {
			int model = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/smokeball2.spr" );
			TEMPENTITY *t = gEngfuncs.pEfxAPI->R_DefaultSprite(smokeOrigin, model, gEngfuncs.pfnRandomLong(32, 48));
			if (t) {
				t->entity.curstate.rendermode = kRenderTransAdd;
				t->entity.curstate.renderamt = gEngfuncs.pfnRandomLong(40, 60);
				t->entity.curstate.scale = scale;
			}
		} else {
			// Don't draw for others.
		}
	}
}

//======================
//	    GLOCK START
//======================
void EV_FireGlock1( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	int empty;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	
	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	empty = args->bparam1;
	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( empty ? GLOCK_SHOOT_EMPTY : GLOCK_SHOOT, 2 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(2.0, 4.0));
	}

	float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 100, 150 );
	float fU = gEngfuncs.pfnRandomFloat( 50, 100 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, -2, -28, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * -4, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
	}

	if ( args->bparam2 )
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "handgun_silenced.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );
	} else {
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "handgun.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );
	}

	EV_GetGunPosition( args, vecSrc, origin );
	
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.3, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, 0, args->fparam1, args->fparam2 );
}

void EV_FireGlock2( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	int empty;
	
	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	empty = args->bparam1;
	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( empty ? GLOCK_SHOOT_EMPTY : GLOCK_SHOOT, 2 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-2.0, -4.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(2.0, 4.0));
	}

	float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 100, 150 );
	float fU = gEngfuncs.pfnRandomFloat( 50, 100 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, 2, -28, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * -4, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( gEngfuncs.GetViewModel()->attachment[1], ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
	}

	if ( args->bparam2 )
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "handgun_silenced.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );
	} else {
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "handgun.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );
	}

	EV_GetGunPosition( args, vecSrc, origin );
	
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.3, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );
	
}
//======================
//	   GLOCK END
//======================

//======================
//	  SHOTGUN START
//======================
void EV_FireShotGunDouble( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	int j;
	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shotgunshell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SHOTGUN_FIRE2, 2 );
		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-10.0, -12.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-10.0, -12.0)); //yaw, - = right
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(10.0, 12.0)); //roll, - = left
	}

	for ( j = 0; j < 2; j++ )
	{
		float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 50, 70 );
		float fU = gEngfuncs.pfnRandomFloat( 100, 150 );
		EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, -10, -28, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * 6, fU, fR );

		if ( EV_IsLocal( idx ) )
		{
			EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHOTSHELL );
		}
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/dbarrel1.wav", gEngfuncs.pfnRandomFloat(0.98, 1.0), ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.8, idx, args->ducking, forward, right, up, 0, 0, 0);

	if ( gEngfuncs.GetMaxClients() > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 8, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 1, &tracerCount[idx-1], 0.17365, 0.04362 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 12, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 1, &tracerCount[idx-1], 0.08716, 0.08716 );
	}
}

void EV_FireShotGunSingle( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	
	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shotgunshell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SHOTGUN_FIRE, 2 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-5.0, -7.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-5.0, -7.0)); //yaw, - = right
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(5.0, 7.0)); //roll, - = left
	}

	float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 50, 70 );
	float fU = gEngfuncs.pfnRandomFloat( 100, 150 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, -10, -28, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * 6, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHOTSHELL );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/sbarrel1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.5, idx, args->ducking, forward, right, up, 0, 0, 0);

	if ( gEngfuncs.GetMaxClients() > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 4, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 1, &tracerCount[idx-1], 0.08716, 0.04362 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 6, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 1, &tracerCount[idx-1], 0.08716, 0.08716 );
	}
}
//======================
//	   SHOTGUN END
//======================

//======================
//	    MP5 START
//======================
void EV_FireMP5( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shell.mdl");// brass shell
	
	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( MP5_FIRE1 + gEngfuncs.pfnRandomLong(0,2), 2 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat( -2, 2 ) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-2.0, -2.0)); //yaw, - = right
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(2.0, 4.0)); //roll, - = left
	}

	float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 50, 70 );
	float fU = gEngfuncs.pfnRandomFloat( 100, 150 );
	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * -4, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
	}

	switch( gEngfuncs.pfnRandomLong( 0, 2 ) )
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "m16_hks1.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "m16_hks2.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	case 2:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "m16_hks3.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	}

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);

	if ( gEngfuncs.GetMaxClients() > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx-1], args->fparam1, args->fparam2 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx-1], args->fparam1, args->fparam2 );
	}
}

// We only predict the animation and sound
// The grenade is still launched from the server.
void EV_FireMP52( event_args_t *args )
{
	int idx;
	vec3_t origin;
	
	idx = args->entindex;
	VectorCopy( args->origin, origin );

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( MP5_LAUNCH, 2 );
		V_PunchAxis(PITCH, -10 );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(8.0, 10.0)); //yaw, - = right
	}
	
	switch( gEngfuncs.pfnRandomLong( 0, 1 ) )
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "m16_glauncher.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "m16_glauncher2.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	}
}
//======================
//		 MP5 END
//======================

//======================
//	   PHYTON START 
//	     ( .357 )
//======================
void EV_FirePython( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Python uses different body in multiplayer versus single player
		int multiplayer = gEngfuncs.GetMaxClients() == 1 ? 0 : 1;

		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( PYTHON_FIRE1, multiplayer ? 1 : 0 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-10.0, -15.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-3.0, -5.0)); //yaw, - = right
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(3.0, 5.0)); //roll, - = left
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "revolver_fire.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM );

	EV_GetGunPosition( args, vecSrc, origin );
	
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_357, 0, 0, args->fparam1, args->fparam2 );
}
//======================
//	    PHYTON END 
//	     ( .357 )
//======================

//======================
//	   GAUSS START 
//======================
#define SND_CHANGE_PITCH	(1<<7)		// duplicated in protocol.h change sound pitch

void EV_SpinGauss( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	int iSoundState = 0;

	int pitch;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	pitch = args->iparam1;

	iSoundState = args->bparam1 ? SND_CHANGE_PITCH : 0;

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "ambience/pulsemachine.wav", 1.0, ATTN_NORM, iSoundState, pitch );
}

/*
==============================
EV_StopPreviousGauss

==============================
*/
void EV_StopPreviousGauss( int idx )
{
	// Make sure we don't have a gauss spin event in the queue for this guy
	gEngfuncs.pEventAPI->EV_KillEvents( idx, "events/gaussspin.sc" );
	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_WEAPON, "ambience/pulsemachine.wav" );
}

extern float g_flApplyVel;

void EV_FireGauss( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	float flDamage = args->fparam1;
	int primaryfire = args->bparam1;

	int m_fPrimaryFire = args->bparam1;
	int m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
	vec3_t vecSrc;
	vec3_t vecDest;
	edict_t		*pentIgnore;
	pmtrace_t tr, beam_tr;
	float flMaxFrac = 1.0;
	int	nTotal = 0;
	int fHasPunched = 0;
	int fFirstBeam = 1;
	int	nMaxHits = 10;
	physent_t *pEntity;
	int m_iBeam, m_iGlow, m_iBalls;
	vec3_t up, right, forward;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	if ( args->bparam2 )
	{
		EV_StopPreviousGauss( idx );
		return;
	}

//	Con_Printf( "Firing gauss with %f\n", flDamage );
	EV_GetGunPosition( args, vecSrc, origin );

	m_iBeam = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/smoke.spr" );
	if (cl_icemodels && cl_icemodels->value)
		m_iBalls = m_iGlow = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/ice_hotglow.spr" );
	else
		m_iBalls = m_iGlow = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/hotglow.spr" );
	
	AngleVectors( angles, forward, right, up );

	VectorMA( vecSrc, 8192, forward, vecDest );

	if ( EV_IsLocal( idx ) )
	{
		V_PunchAxis(PITCH, -2.0 );
		gEngfuncs.pEventAPI->EV_WeaponAnimation( GAUSS_FIRE2, 2 );

		if ( m_fPrimaryFire == false )
			 g_flApplyVel = flDamage;	
			 
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/gauss2.wav", 0.5 + flDamage * (1.0 / 400.0), ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	while (flDamage > 10 && nMaxHits > 0)
	{
		nMaxHits--;

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
		
		// Store off the old count
		gEngfuncs.pEventAPI->EV_PushPMStates();
	
		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	

		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecDest, PM_STUDIO_BOX, -1, &tr );

		gEngfuncs.pEventAPI->EV_PopPMStates();

		if ( tr.allsolid )
			break;

		int r = 255, g = 128, b = 0;
		if (cl_icemodels && cl_icemodels->value) {
			r = 0;
			g = 113;
			b = 230;
		}

		if (fFirstBeam)
		{
			if ( EV_IsLocal( idx ) )
			{
				// Add muzzle flash to current weapon model
				EV_MuzzleFlash();
			}
			fFirstBeam = 0;

			gEngfuncs.pEfxAPI->R_BeamEntPoint( 
				idx | 0x1000,
				tr.endpos,
				m_iBeam,
				0.1,
				m_fPrimaryFire ? 1.0 : 2.5,
				0.0,
				m_fPrimaryFire ? 128.0 : flDamage,
				0,
				0,
				0,
				m_fPrimaryFire ? r : 255,
				m_fPrimaryFire ? g : 255,
				m_fPrimaryFire ? b : 255
			);
		}
		else
		{
			gEngfuncs.pEfxAPI->R_BeamPoints( vecSrc,
				tr.endpos,
				m_iBeam,
				0.1,
				m_fPrimaryFire ? 1.0 : 2.5,
				0.0,
				m_fPrimaryFire ? 128.0 : flDamage,
				0,
				0,
				0,
				m_fPrimaryFire ? r : 255,
				m_fPrimaryFire ? g : 255,
				m_fPrimaryFire ? b : 255
			);
		}

		pEntity = gEngfuncs.pEventAPI->EV_GetPhysent( tr.ent );
		if ( pEntity == NULL )
			break;

		if ( pEntity->solid == SOLID_BSP )
		{
			float n;

			pentIgnore = NULL;

			n = -DotProduct( tr.plane.normal, forward );

			if (n < 0.5) // 60 degrees	
			{
				// ALERT( at_console, "reflect %f\n", n );
				// reflect
				vec3_t r;
			
				VectorMA( forward, 2.0 * n, tr.plane.normal, r );

				flMaxFrac = flMaxFrac - tr.fraction;
				
				VectorCopy( r, forward );

				VectorMA( tr.endpos, 8.0, forward, vecSrc );
				VectorMA( vecSrc, 8192.0, forward, vecDest );

				gEngfuncs.pEfxAPI->R_TempSprite( tr.endpos, vec3_origin, 0.2, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage * n / 255.0, flDamage * n * 0.5 * 0.1, FTENT_FADEOUT );

				vec3_t fwd;
				VectorAdd( tr.endpos, tr.plane.normal, fwd );

				gEngfuncs.pEfxAPI->R_Sprite_Trail( TE_SPRITETRAIL, tr.endpos, fwd, m_iBalls, 3, 0.1, gEngfuncs.pfnRandomFloat( 10, 20 ) / 100.0, 100,
									255, 100 );

				// lose energy
				if ( n == 0 )
				{
					n = 0.1;
				}
				
				flDamage = flDamage * (1 - n);

			}
			else
			{
				// tunnel
				EV_HLDM_DecalGunshot( &tr, BULLET_MONSTER_12MM );

				gEngfuncs.pEfxAPI->R_TempSprite( tr.endpos, vec3_origin, 1.0, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage / 255.0, 6.0, FTENT_FADEOUT );

				// limit it to one hole punch
				if (fHasPunched)
				{
					break;
				}
				fHasPunched = 1;
				
				// try punching through wall if secondary attack (primary is incapable of breaking through)
				if ( !m_fPrimaryFire )
				{
					vec3_t start;

					VectorMA( tr.endpos, 8.0, forward, start );

					// Store off the old count
					gEngfuncs.pEventAPI->EV_PushPMStates();
						
					// Now add in all of the players.
					gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );

					gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
					gEngfuncs.pEventAPI->EV_PlayerTrace( start, vecDest, PM_STUDIO_BOX, -1, &beam_tr );

					if ( !beam_tr.allsolid )
					{
						vec3_t delta;
						float n;

						// trace backwards to find exit point

						gEngfuncs.pEventAPI->EV_PlayerTrace( beam_tr.endpos, tr.endpos, PM_STUDIO_BOX, -1, &beam_tr );

						VectorSubtract( beam_tr.endpos, tr.endpos, delta );
						
						n = Length( delta );

						if (n < flDamage)
						{
							if (n == 0)
								n = 1;
							flDamage -= n;

							// absorption balls
							{
								vec3_t fwd;
								VectorSubtract( tr.endpos, forward, fwd );
								gEngfuncs.pEfxAPI->R_Sprite_Trail( TE_SPRITETRAIL, tr.endpos, fwd, m_iBalls, 3, 0.1, gEngfuncs.pfnRandomFloat( 10, 20 ) / 100.0, 100,
									255, 100 );
							}

	//////////////////////////////////// WHAT TO DO HERE
							// CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, NORMAL_EXPLOSION_VOLUME, 3.0 );

							EV_HLDM_DecalGunshot( &beam_tr, BULLET_MONSTER_12MM );
							
							gEngfuncs.pEfxAPI->R_TempSprite( beam_tr.endpos, vec3_origin, 0.1, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage / 255.0, 6.0, FTENT_FADEOUT );
			
							// balls
							{
								vec3_t fwd;
								VectorSubtract( beam_tr.endpos, forward, fwd );
								gEngfuncs.pEfxAPI->R_Sprite_Trail( TE_SPRITETRAIL, beam_tr.endpos, fwd, m_iBalls, (int)(flDamage * 0.3), 0.1, gEngfuncs.pfnRandomFloat( 10, 20 ) / 100.0, 200,
									255, 40 );
							}
							
							VectorAdd( beam_tr.endpos, forward, vecSrc );
						}
					}
					else
					{
						flDamage = 0;
					}

					gEngfuncs.pEventAPI->EV_PopPMStates();
				}
				else
				{
					if ( m_fPrimaryFire )
					{
						// slug doesn't punch through ever with primary 
						// fire, so leave a little glowy bit and make some balls
						gEngfuncs.pEfxAPI->R_TempSprite( tr.endpos, vec3_origin, 0.2, m_iGlow, kRenderGlow, kRenderFxNoDissipation, 200.0 / 255.0, 0.3, FTENT_FADEOUT );
			
						{
							vec3_t fwd;
							VectorAdd( tr.endpos, tr.plane.normal, fwd );
							gEngfuncs.pEfxAPI->R_Sprite_Trail( TE_SPRITETRAIL, tr.endpos, fwd, m_iBalls, 8, 0.6, gEngfuncs.pfnRandomFloat( 10, 20 ) / 100.0, 100,
								255, 200 );
						}
					}

					flDamage = 0;
				}
			}
		}
		else
		{
			VectorAdd( tr.endpos, forward, vecSrc );
		}
	}
}
//======================
//	   GAUSS END 
//======================

//======================
//	   CROWBAR START
//======================

enum crowbar_e {
	CROWBAR_IDLE = 0,
	CROWBAR_DRAW_LOWKEY,
	CROWBAR_DRAW,
	CROWBAR_HOLSTER,
	CROWBAR_ATTACK1HIT,
	CROWBAR_ATTACK1MISS,
	CROWBAR_ATTACK2MISS,
	CROWBAR_ATTACK2HIT,
	CROWBAR_ATTACK3MISS,
	CROWBAR_ATTACK3HIT,
	CROWBAR_IDLE2,
	CROWBAR_IDLE3
};

int g_iSwing;

//Only predict the miss sounds, hit sounds are still played 
//server side, so players don't get the wrong idea.
void EV_Crowbar( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	
	//Play Swing sound
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM); 

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( CROWBAR_ATTACK1MISS, 1 );

		switch( (g_iSwing++) % 3 )
		{
			case 0:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( CROWBAR_ATTACK1MISS, 1 ); 
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-3.0, -5.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(3.0, 5.0));
				break;
			case 1:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( CROWBAR_ATTACK2MISS, 1 ); 
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(3.0, 5.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(-3.0, -5.0));
				break;
			case 2:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( CROWBAR_ATTACK3MISS, 1 ); 
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-3.0, -5.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(3.0, 5.0));
				break;
		}
	}
}
//======================
//	   CROWBAR END 
//======================

//======================
//	  CROSSBOW START
//======================
enum crossbow_e {
	CROSSBOW_IDLE1 = 0,	// full
	CROSSBOW_IDLE2,		// empty
	CROSSBOW_FIDGET1,	// full
	CROSSBOW_FIDGET2,	// empty
	CROSSBOW_FIRE1,		// full
	CROSSBOW_FIRE2,		// reload
	CROSSBOW_FIRE3,		// empty
	CROSSBOW_RELOAD,	// from empty
	CROSSBOW_DRAW_LOWKEY,
	CROSSBOW_DRAW1,		// full
	CROSSBOW_DRAW2,		// empty
	CROSSBOW_HOLSTER1,	// full
	CROSSBOW_HOLSTER2,	// empty
};

//=====================
// EV_BoltCallback
// This function is used to correct the origin and angles 
// of the bolt, so it looks like it's stuck on the wall.
//=====================
void EV_BoltCallback ( struct tempent_s *ent, float frametime, float currenttime )
{
	ent->entity.origin = ent->entity.baseline.vuser1;
	ent->entity.angles = ent->entity.baseline.vuser2;
}

void EV_FireCrossbow2( event_args_t *args )
{
	vec3_t vecSrc, vecEnd;
	vec3_t up, right, forward;
	pmtrace_t tr;

	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );

	VectorCopy( args->velocity, velocity );
	
	AngleVectors( angles, forward, right, up );

	EV_GetGunPosition( args, vecSrc, origin );

	VectorMA( vecSrc, 8192, forward, vecEnd );

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "boltgun_fire.wav", 1, ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0,0xF) );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "weapons/xbow_reload1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0,0xF) );

	if ( EV_IsLocal( idx ) )
	{
		if ( args->iparam1 )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( CROSSBOW_FIRE1, 1 );
		else
			gEngfuncs.pEventAPI->EV_WeaponAnimation( CROSSBOW_FIRE3, 1 );
	}

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr );
	
	//We hit something
	if ( tr.fraction < 1.0 )
	{
		physent_t *pe = gEngfuncs.pEventAPI->EV_GetPhysent( tr.ent ); 

		//Not the world, let's assume we hit something organic ( dog, cat, uncle joe, etc ).
		if ( pe->solid != SOLID_BSP )
		{
			switch( gEngfuncs.pfnRandomLong(0,1) )
			{
			case 0:
				gEngfuncs.pEventAPI->EV_PlaySound( idx, tr.endpos, CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
			case 1:
				gEngfuncs.pEventAPI->EV_PlaySound( idx, tr.endpos, CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM, 0, PITCH_NORM ); break;
			}
		}
		//Stick to world but don't stick to glass, it might break and leave the bolt floating. It can still stick to other non-transparent breakables though.
		else if ( pe->rendermode == kRenderNormal ) 
		{
			gEngfuncs.pEventAPI->EV_PlaySound( 0, tr.endpos, CHAN_BODY, "weapons/xbow_hit1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, PITCH_NORM );
		
			//Not underwater, do some sparks...
			if ( gEngfuncs.PM_PointContents( tr.endpos, NULL ) != CONTENTS_WATER)
				 gEngfuncs.pEfxAPI->R_SparkShower( tr.endpos );

			vec3_t vBoltAngles;
			int iModelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( "models/w_bolt.mdl" );

			VectorAngles( forward, vBoltAngles );

			TEMPENTITY *bolt = gEngfuncs.pEfxAPI->R_TempModel( tr.endpos - forward * 10, Vector( 0, 0, 0), vBoltAngles , 5, iModelIndex, TE_BOUNCE_NULL );
			
			if ( bolt )
			{
				bolt->flags |= ( FTENT_CLIENTCUSTOM ); //So it calls the callback function.
				bolt->entity.baseline.vuser1 = tr.endpos - forward * 10; // Pull out a little bit
				bolt->entity.baseline.vuser2 = vBoltAngles; //Look forward!
				bolt->callback = EV_BoltCallback; //So we can set the angles and origin back. (Stick the bolt to the wall)
			}
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

//TODO: Fully predict the fliying bolt.
void EV_FireCrossbow( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "boltgun_fire.wav", 1, ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0,0xF) );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "weapons/xbow_reload1.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong(0,0xF) );

	//Only play the weapon anims if I shot it. 
	if ( EV_IsLocal( idx ) )
	{
		if ( args->iparam1 )
			gEngfuncs.pEventAPI->EV_WeaponAnimation( CROSSBOW_FIRE1, 1 );
		else
			gEngfuncs.pEventAPI->EV_WeaponAnimation( CROSSBOW_FIRE3, 1 );

		V_PunchAxis( 0, -2.0 );
	}
}
//======================
//	   CROSSBOW END 
//======================

//======================
//	    RPG START 
//======================
enum rpg_e {
	RPG_IDLE = 0,
	RPG_FIDGET,
	RPG_RELOAD,		// to reload
	RPG_FIRE2,		// to empty
	RPG_FIRE3,		// to empty, with sound!
	RPG_HOLSTER1,	// loaded
	RPG_DRAW_LOWKEY,
	RPG_DRAW1,		// loaded
	RPG_HOLSTER2,	// unloaded
	RPG_DRAW_UL,	// unloaded
	RPG_IDLE_UL,	// unloaded idle
	RPG_FIDGET_UL,	// unloaded fidget
};

void EV_FireRpg( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM, 0, PITCH_NORM );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM, 0, PITCH_NORM );

	//Only play the weapon anims if I shot it. 
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( RPG_FIRE2, 1 );
	
		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-5.0, -7.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(2.0, 4.0));
	}
}

void EV_FireRpgExtreme( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy( args->origin, origin );

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM, 0, PITCH_NORM );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM, 0, PITCH_NORM );

	//Only play the weapon anims if I shot it.
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( RPG_FIRE3, 1 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-7.0, -10.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(2.0, 4.0));
	}
}
//======================
//	     RPG END 
//======================

//======================
//	    EGON END 
//======================
enum egon_e {
	EGON_IDLE1 = 0,
	EGON_FIDGET1,
	EGON_ALTFIREON,
	EGON_ALTFIRECYCLE,
	EGON_ALTFIREOFF,
	EGON_FIRE1,
	EGON_FIRE2,
	EGON_FIRE3,
	EGON_FIRE4,
	EGON_DRAW_LOWKEY,
	EGON_DRAW,
	EGON_HOLSTER
};

int g_fireAnims1[] = { EGON_FIRE1, EGON_FIRE2, EGON_FIRE3, EGON_FIRE4 };
int g_fireAnims2[] = { EGON_ALTFIRECYCLE };

enum EGON_FIRESTATE { FIRE_OFF, FIRE_CHARGE };
enum EGON_FIREMODE { FIRE_NARROW, FIRE_WIDE};

#define	EGON_PRIMARY_VOLUME		450
#define EGON_BEAM_SPRITE		"sprites/xbeam1.spr"
#define EGON_FLARE_SPRITE		"sprites/XSpark1.spr"
#define EGON_SOUND_OFF			"weapons/egon_off1.wav"
#define EGON_SOUND_RUN			"weapons/egon_run3.wav"
#define EGON_SOUND_STARTUP		"weapons/egon_windup2.wav"

#define ARRAYSIZE(p)		(sizeof(p)/sizeof(p[0]))

BEAM *pBeam;
BEAM *pBeam2;

void EV_EgonFire( event_args_t *args )
{
	int idx, iFireState, iFireMode;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	iFireState = args->iparam1;
	iFireMode = args->iparam2;
	int iStartup = args->bparam1;


	if ( iStartup )
	{
		if ( iFireMode == FIRE_WIDE )
			gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, EGON_SOUND_STARTUP, 0.98, ATTN_NORM, 0, 125 );
		else
			gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, EGON_SOUND_STARTUP, 0.9, ATTN_NORM, 0, 100 );
	}
	else
	{
		if ( iFireMode == FIRE_WIDE )
			gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, EGON_SOUND_RUN, 0.98, ATTN_NORM, 0, 125 );
		else
			gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, EGON_SOUND_RUN, 0.9, ATTN_NORM, 0, 100 );
	}

	//Only play the weapon anims if I shot it.
	if ( EV_IsLocal( idx ) ) {
		gEngfuncs.pEventAPI->EV_WeaponAnimation ( g_fireAnims1[ gEngfuncs.pfnRandomLong( 0, 3 ) ], 1 );
		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(4.0, -4.0));
	}

	if ( iStartup == 1 && EV_IsLocal( idx ) && !pBeam && !pBeam2 && cl_lw->value ) //Adrian: Added the cl_lw check for those lital people that hate weapon prediction.
	{
		vec3_t vecSrc, vecEnd, origin, angles, forward, right, up;
		pmtrace_t tr;

		cl_entity_t *pl = gEngfuncs.GetEntityByIndex( idx );

		if ( pl )
		{
			VectorCopy( gHUD.m_vecAngles, angles );
			
			AngleVectors( angles, forward, right, up );

			EV_GetGunPosition( args, vecSrc, pl->origin );

			VectorMA( vecSrc, 2048, forward, vecEnd );

			gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );	
				
			// Store off the old count
			gEngfuncs.pEventAPI->EV_PushPMStates();
			
			// Now add in all of the players.
			gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	

			gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
			gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr );

			gEngfuncs.pEventAPI->EV_PopPMStates();

			int iBeamModelIndex = gEngfuncs.pEventAPI->EV_FindModelIndex( EGON_BEAM_SPRITE );

			float r = 50.0f, r2 = 50.0f;
			float g = 50.0f, g2 = 50.0f;
			float b = 125.0f, b2 = 125.0f;
			float amp = 0.2f;

			if ( iFireMode == FIRE_NARROW ) {
				r2 = 60.0f;
				g2 = 120.0f;
				b2 = 64.0f;
				amp = 0.01f;
			}

			if ( IEngineStudio.IsHardware() )
			{
				r /= 100.0f;
				g /= 100.0f;
				r2 /= 100.0f;
				g2 /= 100.0f;
			}

			V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-4.0, 4.0));

			pBeam = gEngfuncs.pEfxAPI->R_BeamEntPoint ( idx | 0x1000, tr.endpos, iBeamModelIndex, 99999, 3.5, amp, 0.7, 55, 0, 0, r, g, b );

			if ( pBeam )
				 pBeam->flags |= ( FBEAM_SINENOISE );
 
			pBeam2 = gEngfuncs.pEfxAPI->R_BeamEntPoint ( idx | 0x1000, tr.endpos, iBeamModelIndex, 99999, 5.0, 0.08, 0.7, 25, 0, 0, r2, g2, b2 );
		}
	}
}

void EV_EgonStop( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy ( args->origin, origin );

	gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, EGON_SOUND_RUN );
	
	if ( args->iparam1 )
		 gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, EGON_SOUND_OFF, 0.98, ATTN_NORM, 0, 100 );

	if ( EV_IsLocal( idx ) ) 
	{
		if ( pBeam )
		{
			pBeam->die = 0.0;
			pBeam = NULL;
		}
			
		
		if ( pBeam2 )
		{
			pBeam2->die = 0.0;
			pBeam2 = NULL;
		}
	}
}
//======================
//	    EGON END 
//======================

//======================
//	   HORNET START
//======================
enum hgun_e {
	HGUN_IDLE1 = 0,
	HGUN_FIDGETSWAY,
	HGUN_FIDGETSHAKE,
	HGUN_DOWN,
	HGUN_DRAW_LOWKEY,
	HGUN_UP,
	HGUN_SHOOT
};

void EV_HornetGunFire( event_args_t *args )
{
	int idx, iFireMode;
	vec3_t origin, angles, vecSrc, forward, right, up;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	iFireMode = args->iparam1;

	//Only play the weapon anims if I shot it.
	if ( EV_IsLocal( idx ) )
	{
		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-2, 2));
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-2.0, -2.0)); //yaw, - = right
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(2.0, 4.0)); //roll, - = left
		gEngfuncs.pEventAPI->EV_WeaponAnimation ( HGUN_SHOOT, 1 );
	}

	switch ( gEngfuncs.pfnRandomLong ( 0 , 2 ) )
	{
		case 0:	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "agrunt/ag_fire1.wav", 1, ATTN_NORM, 0, 100 );	break;
		case 1:	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "agrunt/ag_fire2.wav", 1, ATTN_NORM, 0, 100 );	break;
		case 2:	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "agrunt/ag_fire3.wav", 1, ATTN_NORM, 0, 100 );	break;
	}
}
//======================
//	   HORNET END
//======================

//======================
//	   TRIPMINE START
//======================
enum tripmine_e {
	TRIPMINE_IDLE1 = 0,
	TRIPMINE_SPIN,
	TRIPMINE_IDLE2,
	TRIPMINE_ARM1,
	TRIPMINE_ARM2,
	TRIPMINE_FIDGET,
	TRIPMINE_HOLSTER,
	TRIPMINE_DRAW_LOWKEY,
	TRIPMINE_DRAW,
	TRIPMINE_WORLD,
	TRIPMINE_GROUND,
};

//We only check if it's possible to put a trip mine
//and if it is, then we play the animation. Server still places it.
void EV_TripmineFire( event_args_t *args )
{
	int idx;
	vec3_t vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy( args->origin, vecSrc );
	VectorCopy( args->angles, angles );

	AngleVectors ( angles, forward, NULL, NULL );
		
	if ( !EV_IsLocal ( idx ) )
		return;

	// Grab predicted result for local player
	gEngfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );

	vecSrc = vecSrc + view_ofs;

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecSrc + forward * 128, PM_NORMAL, -1, &tr );

	//Hit something solid
	if ( tr.fraction < 1.0 ) {
		 gEngfuncs.pEventAPI->EV_WeaponAnimation ( TRIPMINE_ARM2, 0 );
		 V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
	}
	
	gEngfuncs.pEventAPI->EV_PopPMStates();
}
//======================
//	   TRIPMINE END
//======================

//======================
//	   SQUEAK START
//======================
enum squeak_e {
	SQUEAK_IDLE1 = 0,
	SQUEAK_FIDGETFIT,
	SQUEAK_FIDGETNIP,
	SQUEAK_DOWN,
	SQUEAK_DRAW_LOWKEY,
	SQUEAK_UP,
	SQUEAK_THROW,
	SNARK_RELEASE
};

#define VEC_HULL_MIN		Vector(-16, -16, -36)
#define VEC_DUCK_HULL_MIN	Vector(-16, -16, -18 )

void EV_SnarkFire( event_args_t *args )
{
	int idx;
	vec3_t vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy( args->origin, vecSrc );
	VectorCopy( args->angles, angles );

	AngleVectors ( angles, forward, NULL, NULL );
		
	if ( !EV_IsLocal ( idx ) )
		return;
	
	if ( args->ducking )
		vecSrc = vecSrc - ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
	
	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc + forward * 20, vecSrc + forward * 64, PM_NORMAL, -1, &tr );

	//Find space to drop the thing.
	if ( tr.allsolid == 0 && tr.startsolid == 0 && tr.fraction > 0.25 ) {
		 gEngfuncs.pEventAPI->EV_WeaponAnimation ( SQUEAK_THROW, 0 );
		 V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
	}
	
	gEngfuncs.pEventAPI->EV_PopPMStates();
}

void EV_SnarkRelease( event_args_t *args )
{
	int idx;
	vec3_t vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy( args->origin, vecSrc );
	VectorCopy( args->angles, angles );

	AngleVectors ( angles, forward, NULL, NULL );

	if ( !EV_IsLocal ( idx ) )
		return;

	if ( args->ducking )
		vecSrc = vecSrc - ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc + forward * 20, vecSrc + forward * 64, PM_NORMAL, -1, &tr );

	//Find space to drop the thing.
	if ( tr.allsolid == 0 && tr.startsolid == 0 && tr.fraction > 0.25 ) {
		 gEngfuncs.pEventAPI->EV_WeaponAnimation ( SNARK_RELEASE, 0 );
		 V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

//======================
//	   SQUEAK END
//======================

enum knife_e {
	KNIFE_IDLE = 0,
	KNIFE_DRAW_LOWKEY,
	KNIFE_DRAW,
	KNIFE_HOLSTER,
	KNIFE_THROW,
	KNIFE_ATTACK1HIT,
	KNIFE_ATTACK1MISS,
	KNIFE_ATTACK2MISS,
	KNIFE_ATTACK2HIT,
	KNIFE_ATTACK3MISS,
	KNIFE_ATTACK3HIT
};

void EV_Knife( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;
	VectorCopy( args->origin, origin );

	//Play Swing sound
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "knife_miss2.wav", 1, ATTN_NORM, 0, PITCH_NORM);

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( KNIFE_ATTACK1MISS, 1 );

		switch( (g_iSwing++) % 3 )
		{
			case 0:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( KNIFE_ATTACK1MISS, 1 ); 
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-3.0, -5.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(3.0, 5.0));
				break;
			case 1:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( KNIFE_ATTACK2MISS, 1 );
				V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-3.0, -5.0));
				break;
			case 2:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( KNIFE_ATTACK3MISS, 1 );
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-3.0, -5.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(3.0, 5.0));
				break;
		}
	}
}

enum chumtoad_e {
	CHUMTOAD_IDLE1 = 0,
	CHUMTOAD_FIDGETFIT,
	CHUMTOAD_FIDGETNIP,
	CHUMTOAD_DOWN,
	CHUMTOAD_UP,
	CHUMTOAD_THROW,
	CHUMTOAD_RELEASE,
	CHUMTOAD_DRAW_LOWKEY,
	CHUMTOAD_DRAW
};

void EV_ChumtoadFire( event_args_t *args )
{
	int idx;
	vec3_t vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy( args->origin, vecSrc );
	VectorCopy( args->angles, angles );

	AngleVectors ( angles, forward, NULL, NULL );

	if ( !EV_IsLocal ( idx ) )
		return;

	if ( args->ducking )
		vecSrc = vecSrc - ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc + forward * 20, vecSrc + forward * 64, PM_NORMAL, -1, &tr );

	//Find space to drop the thing.
	if ( tr.allsolid == 0 && tr.startsolid == 0 && tr.fraction > 0.25 ) {
		 gEngfuncs.pEventAPI->EV_WeaponAnimation ( CHUMTOAD_THROW, 0 );
		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

void EV_ChumtoadRelease( event_args_t *args )
{
	int idx;
	vec3_t vecSrc, angles, view_ofs, forward;
	pmtrace_t tr;

	idx = args->entindex;
	VectorCopy( args->origin, vecSrc );
	VectorCopy( args->angles, angles );

	AngleVectors ( angles, forward, NULL, NULL );

	if ( !EV_IsLocal ( idx ) )
		return;

	if ( args->ducking )
		vecSrc = vecSrc - ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );

	// Store off the old count
	gEngfuncs.pEventAPI->EV_PushPMStates();

	// Now add in all of the players.
	gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc + forward * 20, vecSrc + forward * 64, PM_NORMAL, -1, &tr );

	//Find space to drop the thing.
	if ( tr.allsolid == 0 && tr.startsolid == 0 && tr.fraction > 0.25 ) {
		 gEngfuncs.pEventAPI->EV_WeaponAnimation ( CHUMTOAD_RELEASE, 0 );
		 V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}

enum rifle_e
{
	RIFLE_DRAW_LOWKEY = 0,
	RIFLE_DRAW,
	RIFLE_IDLE,
	RIFLE_SHOOT,
	RIFLE_SHOOT_EMPTY,
	RIFLE_RELOAD,
	RIFLE_ZOOM_IN,
	RIFLE_ZOOM_OUT
};

void EV_FireSniperRifle( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( RIFLE_SHOOT, 2 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat( -7, -10 ) );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "rifle1.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.5, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );
}

enum cannon_e {
	CANNON_IDLE = 0,
	CANNON_IDLE2,
	CANNON_FIDGET,
	CANNON_SPINUP,
	CANNON_SPIN,
	CANNON_FIRE_BOMB,
	CANNON_FIRE_FLAK,
	CANNON_HOLSTER1,
	CANNON_DRAW_LOWKEY,
	CANNON_DRAW1
};

void EV_FireCannon( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy( args->origin, origin );

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "cannon_fire.wav", 0.9, ATTN_NORM, 0, PITCH_NORM );

	//Only play the weapon anims if I shot it.
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( CANNON_FIRE_BOMB, 1 );
		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-5.0, -7.0));
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(2.0, 4.0));
	}
}

void EV_FireCannonFlak( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy( args->origin, origin );

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM, 0, PITCH_NORM );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM, 0, PITCH_NORM );

	//Only play the weapon anims if I shot it.
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( CANNON_FIRE_FLAK, 1 );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(2.0, 4.0));
	}
}

enum mag60_e {
	MAG60_AIM = 0,
	MAG60_IDLE1,
	MAG60_IDLE2,
	MAG60_IDLE3,
	MAG60_SHOOT,
	MAG60_SHOOT_SIDEWAYS,
	MAG60_SHOOT_EMPTY,
	MAG60_RELOAD,
	MAG60_RELOAD_SIDEWAYS,
	MAG60_RELOAD_NOT_EMPTY,
	MAG60_DRAW_LOWKEY,
	MAG60_DRAW,
	MAG60_HOLSTER,
	MAG60_BUTTON,
	MAG60_ROTATE_DOWN,
	MAG60_ROTATE_UP
};

void EV_FireMag60( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	int empty;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	empty = args->bparam1;
	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( args->bparam2 ? MAG60_SHOOT_SIDEWAYS : MAG60_SHOOT, 2 );

		V_PunchAxis(PITCH, -2.0 );
	}

	float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 50, 70 );
	float fU = gEngfuncs.pfnRandomFloat( 100, 150 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, 0, -28, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * 0, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
	}

	if ( args->bparam2 )
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "mag60_fire.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );
	} else {
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "mag60_fire.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );
	}

	EV_GetGunPosition( args, vecSrc, origin );

	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.4, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, 0, args->fparam1, args->fparam2 );
}

enum chaingun_e
{
    CHAINGUN_IDLE = 0,
	CHAINGUN_IDLE1,
	CHAINGUN_SPINUP,
	CHAINGUN_SPINDOWN,
	CHAINGUN_FIRE,
	CHAINGUN_DRAW_LOWKEY,
	CHAINGUN_DRAW,
	CHAINGUN_HOLSTER,
	CHAINGUN_RELOAD,
};

void EV_FireChaingun( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	int empty;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	empty = args->bparam1;
	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
		if ( args->bparam2 ) {
			gEngfuncs.pEventAPI->EV_WeaponAnimation( CHAINGUN_FIRE, 2 );
		}
		V_PunchAxis(PITCH, -2.0 );
	}

	float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 50, 70 );
	float fU = gEngfuncs.pfnRandomFloat( 100, 150 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, 0, -32, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * 0, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "chaingun_fire.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );

	EV_GetGunPosition( args, vecSrc, origin );

	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_357, 2, &tracerCount[idx-1], args->fparam1, args->fparam2 );
}

enum glauncher_e
{
	GLAUNCHER_IDLE1 = 0,
	GLAUNCHER_IDLE2,
	GLAUNCHER_DRAW_LOWKEY,
	GLAUNCHER_DRAW,
	GLAUNCHER_HOLSTER,
	GLAUNCHER_RELOAD,
	GLAUNCHER_SHOOT,
};

void EV_FireGrenadeLauncher( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t up, right, forward;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( GLAUNCHER_SHOOT, 2 );
		V_PunchAxis(PITCH, -10 );
	}

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.7, idx, args->ducking, forward, right, up, 0, 0, 0);

	switch( gEngfuncs.pfnRandomLong( 0, 1 ) )
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/glauncher.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/glauncher2.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	}
}

enum smg_e
{
	SMG_AIM = 0,
	SMG_IDLE1,
	SMG_IDLE2,
	SMG_IDLE3,
	SMG_RELOAD,
	SMG_DEPLOY,
	SMG_FIRE1,
	SMG_SELECT,
	SMG_HOLSTER,
};

void EV_FireSmg( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SMG_FIRE1, 0 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-5.0, -7.0));
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(2.0, 4.0));
	}

	float fR = (m_pCvarRighthand->value != 0 ? 1 : -1) * gEngfuncs.pfnRandomFloat( 50, 70 );
	float fU = gEngfuncs.pfnRandomFloat( 100, 150 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, 0, -30, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * 0, fU, fR);

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "smg_fire.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.3, idx, args->ducking, forward, right, up, 0, 0, 0);

	if ( gEngfuncs.GetMaxClients() > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx-1], args->fparam1, args->fparam2 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx-1], args->fparam1, args->fparam2 );
	}
}

enum usas_e {
	USAS_AIM = 0,
	USAS_LONGIDLE,
	USAS_IDLE1,
	USAS_LAUNCH,
	USAS_RELOAD,
	USAS_DRAW_LOWKEY,
	USAS_DEPLOY,
	USAS_FIRE1,
	USAS_FIRE2,
	USAS_FIRE3,
	USAS_HOLSTER,
};

void EV_FireUsas( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shotgunshell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( USAS_FIRE1, 2 );

		V_PunchAxis(PITCH, -5.0 );
	}

	float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 50, 70 );
	float fU = gEngfuncs.pfnRandomFloat( 100, 150 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, -10, -38, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * -8, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHOTSHELL ); 
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "usas_fire.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);

	if ( gEngfuncs.GetMaxClients() > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 4, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.04362 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 6, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.08716 );
	}
}

enum fists_e {
	FISTS_IDLE = 0,
	FISTS_DRAW_LOWKEY,
	FISTS_DRAW,
	FISTS_HOLSTER,
	FISTS_ATTACK1HIT,
	FISTS_ATTACK1MISS,
	FISTS_ATTACK2MISS,
	FISTS_ATTACK2HIT,
	FISTS_ATTACK3MISS,
	FISTS_ATTACK3HIT,
	FISTS_IDLE2,
	FISTS_IDLE3,
	FISTS_PULLUP
};

void EV_Fists( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;
	VectorCopy( args->origin, origin );

	//Play Swing sound
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "fists_miss.wav", 1, ATTN_NORM, 0, PITCH_NORM);

	if (args->bparam1)
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_VOICE, "fists_shoryuken.wav", 1, ATTN_NORM, 0, PITCH_NORM);

	if ( EV_IsLocal( idx ) )
	{
		if (args->bparam1) {
			gEngfuncs.pEventAPI->EV_WeaponAnimation ( FISTS_ATTACK3MISS, 1 );
			V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-7.0, -10.0));
			V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-5.0, -8.0));
			V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(5.0, 8.0));
		} else {
			switch( (g_iSwing++) % 2 )
			{
				case 0:
					gEngfuncs.pEventAPI->EV_WeaponAnimation ( FISTS_ATTACK1MISS, 1 );
					V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-3.0, -5.0));
					break;
				case 1:
					gEngfuncs.pEventAPI->EV_WeaponAnimation ( FISTS_ATTACK2MISS, 1 );
					V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(3.0, 5.0));
					V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(-3.0, -5.0));
					break;
			}
		}
	}
}

enum wrench_e {
	WRENCH_IDLE = 0,
	WRENCH_DRAW_LOWKEY,
	WRENCH_DRAW,
	WRENCH_HOLSTER,
	WRENCH_ATTACK1HIT,
	WRENCH_ATTACK1MISS,
	WRENCH_ATTACK2HIT,
	WRENCH_ATTACK2MISS,
	WRENCH_ATTACK3HIT,
	WRENCH_ATTACK3MISS,
	WRENCH_IDLE2,
	WRENCH_IDLE3,
	WRENCH_PULL_BACK,
	WRENCH_THROW2,
};

void EV_Wrench( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;
	VectorCopy( args->origin, origin );

	//Play Swing sound
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "wrench_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM);

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( WRENCH_ATTACK1MISS, 1 );

		switch( (g_iSwing++) % 3 )
		{
			case 0:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( WRENCH_ATTACK1MISS, 1 );
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-5.0, -7.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(5.0, 7.0));
				break;
			case 1:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( WRENCH_ATTACK2MISS, 1 );
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(5.0, 7.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(-5.0, -7.0));
				break;
			case 2:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( WRENCH_ATTACK3MISS, 1 );
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-5.0, -7.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(5.0, 7.0));
				break;
		}
	}
}

enum chainsaw_e {
	CHAINSAW_IDLE = 0,
	CHAINSAW_DRAW_LOWKEY,
	CHAINSAW_DRAW,
	CHAINSAW_DRAW_EMPTY,
	CHAINSAW_ATTACK_START,
	CHAINSAW_ATTACK_LOOP,
	CHAINSAW_ATTACK_END,
	CHAINSAW_RELOAD,
	CHAINSAW_SLASH1,
	CHAINSAW_SLASH2,
	CHAINSAW_SLASH3,
	CHAINSAW_SLASH4,
	CHAINSAW_EMPTY,
	CHAINSAW_HOLSTER,
};

void EV_Chainsaw( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;
	VectorCopy( args->origin, origin );

	if (args->bparam1)
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "chainsaw_slash.wav", 1, ATTN_NORM, 0, PITCH_NORM);

	if ( EV_IsLocal( idx ) )
	{
		// slashing
		if (args->bparam1) {
			gEngfuncs.pEventAPI->EV_WeaponAnimation( CHAINSAW_SLASH1, 0 );

			switch( (g_iSwing++) % 3 )
			{
				case 0:
					gEngfuncs.pEventAPI->EV_WeaponAnimation ( CHAINSAW_SLASH1, 0 );
					V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-5.0, -7.0)); //yaw, - = right
					V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(5.0, 7.0)); //roll, - = left
					break;
				case 1:
					gEngfuncs.pEventAPI->EV_WeaponAnimation ( CHAINSAW_SLASH2, 0 );
					V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(8.0, 10.0)); //yaw, - = right
					V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(-8.0, -10.0)); //roll, - = left
					break;
				case 2:
					gEngfuncs.pEventAPI->EV_WeaponAnimation ( CHAINSAW_SLASH3, 0 );
					V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-5.0, -7.0)); //yaw, - = right
					V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(5.0, 7.0)); //roll, - = left
					break;
			}
		} else {
			// out straight
			V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-3.0, 3.0));
		}
	}
}

enum gauge_shotgun_e {
	GAUGE_SHOTGUN_IDLE = 0,
	GAUGE_SHOTGUN_IDLE4,
	GUAGE_SHOTGUN_DRAW_LOWKEY,
	GAUGE_SHOTGUN_DRAW,
	GAUGE_SHOTGUN_FIRE,
	GAUGE_SHOTGUN_START_RELOAD,
	GAUGE_SHOTGUN_RELOAD,
	GAUGE_SHOTGUN_PUMP,
	GAUGE_SHOTGUN_HOLSTER,
	GAUGE_SHOTGUN_FIRE2,
	GAUGE_SHOTGUN_IDLE_DEEP
};

void EV_Fire12GaugeSingle( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shotgunshell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( GAUGE_SHOTGUN_FIRE, 0 );
		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-3.0, -5.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-3.0, -5.0)); //yaw, - = right
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(3.0, 5.0)); //roll, - = left
	}

	float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 50, 70 );
	float fU = gEngfuncs.pfnRandomFloat( 100, 150 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, 0, -28, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * 6, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHOTSHELL );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "12gauge_fire.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.5, idx, args->ducking, forward, right, up, 0, 0, 0);

	if ( gEngfuncs.GetMaxClients() > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 4, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.04362 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 6, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.08716 );
	}
}

enum nuke_e {
	NUKE_IDLE = 0,
	NUKE_FIDGET,
	NUKE_RELOAD,		// to reload
	NUKE_FIRE2,		// to empty
	NUKE_FIRE3,		// to empty, with sound!
	NUKE_HOLSTER1,	// loaded
	NUKE_DRAW_LOWKEY,
	NUKE_DRAW1,		// loaded
	NUKE_HOLSTER2,	// unloaded
	NUKE_DRAW_UL,	// unloaded
	NUKE_IDLE_UL,	// unloaded idle
	NUKE_FIDGET_UL,	// unloaded fidget
};

void EV_FireNuke( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy( args->origin, origin );

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM, 0, PITCH_NORM );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM, 0, PITCH_NORM );

	//Only play the weapon anims if I shot it.
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( NUKE_FIRE2, 1 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-10.0, -15.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-7.0, -10.0)); //yaw, - = right
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(7.0, 10.0)); //roll, - = left
	}
}

enum deagle_e {
	DEAGLE_IDLE1,
	DEAGLE_FIDGET,
	DEAGLE_IDLE_EMPTY,
	DEAGLE_FIRE1,
	DEAGLE_FIRE_EMPTY,
	DEAGLE_RELOAD,
	DEAGLE_RELOAD_EMPTY,
	DEAGLE_DRAW_LOWKEY,
	DEAGLE_DRAW,
	DEAGLE_HOLSTER,
	DEAGLE_HOLSTER_EMPTY,
};

void EV_FireDeagle( event_args_t *args )
{
	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shell.mdl");// brass shell

	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	int empty = args->bparam1;

	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();

		gEngfuncs.pEventAPI->EV_WeaponAnimation( empty ? DEAGLE_FIRE_EMPTY : DEAGLE_FIRE1, 0 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-10.0, -15.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-5.0, -7.0)); //yaw, - = right
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(5.0, 7.0)); //roll, - = left
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "deagle_fire.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM );

	float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 100, 150 );
	float fU = gEngfuncs.pfnRandomFloat( 50, 100 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, -2, -28, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * -4, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
	}

	EV_GetGunPosition( args, vecSrc, origin );

	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_357, 0, 0, args->fparam1, args->fparam2 );
}

enum dual_deagle_e {
	DUAL_DEAGLE_IDLE,
	DUAL_DEAGLE_FIRE_LEFT,
	DUAL_DEAGLE_FIRE_RIGHT,
	DUAL_DEAGLE_FIRE_LAST_LEFT,
	DUAL_DEAGLE_FIRE_LAST_RIGHT,
	DUAL_DEAGLE_RELOAD,
	DUAL_DEAGLE_DEPLOY,
	DUAL_DEAGLE_HOLSTER,
	DUAL_DEAGLE_FIRE_BOTH,
};

void EV_FireDualDeagle( event_args_t *args )
{
	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shell.mdl");// brass shell

	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	int clip = args->bparam1;
	int end = args->bparam2;

	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();

		if (end)
			gEngfuncs.pEventAPI->EV_WeaponAnimation( clip % 2 == 0 ? DUAL_DEAGLE_FIRE_LAST_RIGHT : DUAL_DEAGLE_FIRE_LAST_LEFT, 0 );
		else
			gEngfuncs.pEventAPI->EV_WeaponAnimation( clip % 2 == 0 ? DUAL_DEAGLE_FIRE_RIGHT : DUAL_DEAGLE_FIRE_LEFT, 0 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-10.0, -15.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-5.0, -7.0)); //yaw, - = right
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(5.0, 7.0)); //roll, - = left
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "deagle_fire.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM );

	float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 100, 150 );
	if (clip % 2 != 0 ) fR *= -1;

	float fU = gEngfuncs.pfnRandomFloat( 50, 100 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, -2, -28, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * -4, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
	}

	EV_GetGunPosition( args, vecSrc, origin );

	VectorCopy( forward, vecAiming );

	if (clip % 2 == 0)
		EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);
	else
		EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[1], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_357, 0, 0, args->fparam1, args->fparam2 );
}

void EV_FireDualDeagleBoth( event_args_t *args )
{
	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shell.mdl");// brass shell

	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	int clip = args->bparam1;

	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();

		gEngfuncs.pEventAPI->EV_WeaponAnimation( DUAL_DEAGLE_FIRE_BOTH, 0 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-11.0, -16.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-6.0, -8.0)); //yaw, - = right
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(6.0, 8.0)); //roll, - = left
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "deagle_fire.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "deagle_fire.wav", gEngfuncs.pfnRandomFloat(0.9, 1.0), ATTN_NORM, 0, PITCH_NORM );

	float fR = gEngfuncs.pfnRandomFloat( 100, 150 );
	float fU = gEngfuncs.pfnRandomFloat( 50, 100 );
	//fR *= -1;
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, -2, -28, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * -4, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );

		fR *= -1;
		EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, -2, -28, (m_pCvarRighthand->value != 0.0f ? -1 : 1) * -4, fU, fR );
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
	}

	EV_GetGunPosition( args, vecSrc, origin );

	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);
	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[1], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 2, vecSrc, vecAiming, 8192, BULLET_PLAYER_357, 0, 0, args->fparam1, args->fparam2 );
}

enum dual_rpg_e {
	DUAL_RPG_DRAW_LEFT = 0,
	DUAL_RPG_DRAW_BOTH,
	DUAL_RPG_IDLE_BOTH,
	DUAL_RPG_FIRE_BOTH,
	DUAL_RPG_HOLSTER_BOTH,
	DUAL_RPG_RELOAD_BOTH,
};

void EV_FireDualRpgBoth( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/rocketfire1.wav", 0.9, ATTN_NORM, 0, PITCH_NORM );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "weapons/glauncher.wav", 0.7, ATTN_NORM, 0, PITCH_NORM );

	//Only play the weapon anims if I shot it. 
	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( DUAL_RPG_FIRE_BOTH, 1 );
	
		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-5.0, -7.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(2.0, 4.0));
	}
}

enum dual_mag60_e
{	
	DUAL_MAG60_IDLE = 0,
	DUAL_MAG60_FIRE_BOTH1,
	DUAL_MAG60_RELOAD,
	DUAL_MAG60_DEPLOY,
	DUAL_MAG60_HOLSTER,
};

void EV_FireDualMag60( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( DUAL_MAG60_FIRE_BOTH1, 0 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-5.0, -7.0));
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(2.0, 4.0));
	}

	float fR = gEngfuncs.pfnRandomFloat( 50, 70 );
	float fU = gEngfuncs.pfnRandomFloat( 100, 150 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, 0, -30, 0, fU, fR);

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );

		EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[1], velocity, ShellVelocity, ShellOrigin, forward, right, up, 0, -30, 0, fU, fR * -1);
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "mag60_fire.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "mag60_fire.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.3, idx, args->ducking, forward, right, up, 0, 0, 0);
	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[1], 0.3, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 2, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx-1], args->fparam1, args->fparam2 );
}

enum dual_smg_e
{	
	DUAL_SMG_IDLE = 0,
	DUAL_SMG_FIRE_BOTH1,
	DUAL_SMG_RELOAD,
	DUAL_SMG_DEPLOY,
	DUAL_SMG_HOLSTER,
};

void EV_FireDualSmg( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( DUAL_SMG_FIRE_BOTH1, 0 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-5.0, -7.0));
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-2.0, -4.0));
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(2.0, 4.0));
	}

	float fR = gEngfuncs.pfnRandomFloat( 50, 70 );
	float fU = gEngfuncs.pfnRandomFloat( 100, 150 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, 0, -30, 0, fU, fR);

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );

		EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[1], velocity, ShellVelocity, ShellOrigin, forward, right, up, 0, -30, 0, fU, fR * -1);
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "smg_fire.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "smg_fire.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.3, idx, args->ducking, forward, right, up, 0, 0, 0);
	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[1], 0.3, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 2, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx-1], args->fparam1, args->fparam2 );
}

enum dual_wrench_e {
	DUAL_WRENCH_IDLE= 0,
	DUAL_WRENCH_ATTACK1HIT,
	DUAL_WRENCH_ATTACK1MISS,
	DUAL_WRENCH_ATTACK2HIT,
	DUAL_WRENCH_ATTACK2MISS,
	DUAL_WRENCH_ATTACK3HIT,
	DUAL_WRENCH_ATTACK3MISS,
	DUAL_WRENCH_PULL_BACK,
	DUAL_WRENCH_THROW,
	DUAL_WRENCH_DRAW,
	DUAL_WRENCH_HOLSTER,
};

void EV_FireDualWrench( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;
	VectorCopy( args->origin, origin );

	//Play Swing sound
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "wrench_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM);

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( DUAL_WRENCH_ATTACK1MISS, 1 );

		switch( (g_iSwing++) % 3 )
		{
			case 0:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( DUAL_WRENCH_ATTACK1MISS, 1 );
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-5.0, -7.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(5.0, 7.0));
				break;
			case 1:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( DUAL_WRENCH_ATTACK2MISS, 1 );
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(5.0, 7.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(-5.0, -7.0));
				break;
			case 2:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( DUAL_WRENCH_ATTACK3MISS, 1 );
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-5.0, -7.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(5.0, 7.0));
				break;
		}
	}
}

enum dual_usas_e {
	DUAL_USAS_IDLE = 0,
	DUAL_USAS_FIRE_BOTH1,
	DUAL_USAS_RELOAD,
	DUAL_USAS_DEPLOY,
	DUAL_USAS_HOLSTER,
	DUAL_USAS_FIRE_LEFT,
	DUAL_USAS_FIRE_RIGHT
};

void EV_FireDualUsas( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;
	float flSpread = 0.01;
	int clip = args->bparam1;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shotgunshell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();

		gEngfuncs.pEventAPI->EV_WeaponAnimation( clip % 2 == 0 ? DUAL_USAS_FIRE_RIGHT : DUAL_USAS_FIRE_LEFT, 0 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-6.0, -8.0) );
	}

	float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 50, 70 );
	float fU = gEngfuncs.pfnRandomFloat( 100, 150 );
	if (clip % 2 != 0 ) fR *= -1;

	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, -10, -38, 8, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHOTSHELL );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "usas_fire.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	if (clip % 2 == 0)
		EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);
	else
		EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[1], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 4, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.04362 );
}

void EV_FireDualUsasBoth( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	int shell;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	shell = gEngfuncs.pEventAPI->EV_FindModelIndex ("models/w_shotgunshell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( DUAL_USAS_FIRE_BOTH1, 2 );

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-7.0, -9.0) );
	}

	float fR = (m_pCvarRighthand->value != 0 ? -1 : 1) * gEngfuncs.pfnRandomFloat( 50, 70 );
	float fU = gEngfuncs.pfnRandomFloat( 100, 150 );
	EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, -10, -38, 8, fU, fR );

	if ( EV_IsLocal( idx ) )
	{
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHOTSHELL );

		EV_GetDefaultShellInfo( args, gEngfuncs.GetViewModel()->attachment[0], velocity, ShellVelocity, ShellOrigin, forward, right, up, -10, -38, -8, fU, fR * -1);
		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "usas_fire.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_ITEM, "usas_fire.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);
	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[1], 0.6, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 8, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.04362 );
}

enum freezegun_e {
	FREEZEGUN_IDLE,
	FREEZEGUN_RELOAD,
	FREEZEGUN_DRAW_LOWKEY,
	FREEZEGUN_DRAW,
	FREEZEGUN_HOLSTER,
	FREEZEGUN_SHOOT1,
	FREEZEGUN_SHOOT2,
	FREEZEGUN_SHOOT3,
};

void EV_FireFreezeGun( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;
	vec3_t up, right, forward;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if (EV_IsLocal( idx ))
	{
		switch (gEngfuncs.pfnRandomLong(0, 2))
		{
		case 0:
			gEngfuncs.pEventAPI->EV_WeaponAnimation( FREEZEGUN_SHOOT1, 0 );
			break;
		case 1:
			gEngfuncs.pEventAPI->EV_WeaponAnimation( FREEZEGUN_SHOOT2, 0 );
			break;
		default:
			gEngfuncs.pEventAPI->EV_WeaponAnimation( FREEZEGUN_SHOOT3, 0 );
		}

		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-7.0, -9.0));
	}

	int fPrimaryFire = args->bparam2;

	if ( fPrimaryFire )
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "freezegun_fire.wav", 1, ATTN_NORM, 0, 100 );
	}
}

//======================
//	   CROWBAR START
//======================

enum rocket_crowbar_e {
	ROCKET_CROWBAR_IDLE = 0,
	ROCKET_CROWBAR_DRAW_LOWKEY,
	ROCKET_CROWBAR_DRAW,
	ROCKET_CROWBAR_HOLSTER,
	ROCKET_CROWBAR_ATTACK1HIT,
	ROCKET_CROWBAR_ATTACK1MISS,
	ROCKET_CROWBAR_ATTACK2MISS,
	ROCKET_CROWBAR_ATTACK2HIT,
	ROCKET_CROWBAR_ATTACK3MISS,
	ROCKET_CROWBAR_ATTACK3HIT,
	ROCKET_CROWBAR_IDLE2,
	ROCKET_CROWBAR_IDLE3
};

void EV_RocketCrowbar( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	
	//Play Swing sound
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM);

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( ROCKET_CROWBAR_ATTACK1MISS, 1 );

		switch( (g_iSwing++) % 3 )
		{
			case 0:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( ROCKET_CROWBAR_ATTACK1MISS, 1 );
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-3.0, -5.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(3.0, 5.0));
				break;
			case 1:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( ROCKET_CROWBAR_ATTACK2MISS, 1 );
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(3.0, 5.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(-3.0, -5.0));
				break;
			case 2:
				gEngfuncs.pEventAPI->EV_WeaponAnimation ( ROCKET_CROWBAR_ATTACK3MISS, 1 );
				V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-3.0, -5.0));
				V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(3.0, 5.0));
				break;
		}
	}
}

enum gravitygun_e {
	GRAVITYGUN_IDLE1 = 0,
	GRAVITYGUN_HOLD_IDLE,
	GRAVITYGUN_PICKUP,
	GRAVITYGUN_FIRE,
	GRAVITYGUN_DRAW,
};

void EV_GravityGun(event_args_t* args)
{
	int idx;
	int targidx;
	int m_iBeam;
	bool isBspModel;
	Vector vecSrc, angles, forward;
	pmtrace_t tr;

	idx = args->entindex;
	targidx = args->iparam1;
	isBspModel = args->bparam1;
	VectorCopy(args->origin, vecSrc);
	VectorCopy(args->angles, angles);

	m_iBeam = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/smoke.spr");

	AngleVectors(angles, forward, NULL, NULL);

	if (EV_IsLocal(idx))
	{
		if (targidx > 0)
			gEngfuncs.pEventAPI->EV_WeaponAnimation(GRAVITYGUN_FIRE, 0);
		else
			gEngfuncs.pEventAPI->EV_WeaponAnimation(GRAVITYGUN_PICKUP, 0);
	}

	if (targidx > 0)
	{
		cl_entity_t* targent = gEngfuncs.GetEntityByIndex(targidx);

		Vector targpos = targent->origin;
		targpos[2] += targent->curstate.maxs[2] / 2;
		if (isBspModel)
			VectorAverage(targent->curstate.maxs + targent->origin, targent->curstate.mins + targent->origin, targpos);

		int r = 255, g = 128, b = 0;
		if (cl_icemodels && cl_icemodels->value) {
			r = 0;
			g = 113;
			b = 230;
		}
		gEngfuncs.pEfxAPI->R_BeamEntPoint(idx | 0x1000, targpos, m_iBeam, 0.1, 0.4, 0.4, 1, 0.4, 0, 1, r, g, b); 

		gEngfuncs.pEventAPI->EV_PlaySound(idx, args->origin, CHAN_WEAPON, "weapons/gauss2.wav", 0.5, ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong(0, 0x1f));
	}
}

enum flamethrower_e
{
	FLAMETHROWER_IDLE1,
	FLAMETHROWER_FIDGET,
	FLAMETHROWER_ON,
	FLAMETHROWER_CYCLE,
	FLAMETHROWER_OFF,
	FLAMETHROWER_FIRE1,
	FLAMETHROWER_FIRE2,
	FLAMETHROWER_FIRE3,
	FLAMETHROWER_FIRE4,
	FLAMETHROWER_DEPLOY_LOWKEY,
	FLAMETHROWER_DEPLOY,
	FLAMETHROWER_HOLSTER,
};

void EV_FireFlameStream( event_args_t *args )
{
	int idx;
	vec3_t origin,angles,forward,right,up;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	int iStartup = args->bparam1;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );

	AngleVectors( angles, forward, right, up );

	if ( iStartup )
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "flameburst.wav", 1, ATTN_NORM, 0, PITCH_NORM );
	else
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, "flamerun.wav", 1, ATTN_NORM, 0, PITCH_NORM );

	if ( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation ( FLAMETHROWER_FIRE3, 0 );
}

void EV_FireFlameThrower( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy( args->origin, origin );

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "flamethrower.wav", 0.9, ATTN_NORM, 0, PITCH_NORM );

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( FLAMETHROWER_FIRE4, 0 );
		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-2.0, -4.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-1.0, -2.0));
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(1.0, 2.0));
	}
}

void EV_EndFlameThrower( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy ( args->origin, origin );

	gEngfuncs.pEventAPI->EV_StopAllSounds(idx,CHAN_STATIC);

	if ( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation ( FLAMETHROWER_OFF, 0 );
	
	if ( args->iparam1 )
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "flamethrowerend.wav", 1, 0.6, 0, 100 );
}

enum dual_flamethrower_e
{
	DUAL_FLAMETHROWER_IDLE1,
	DUAL_FLAMETHROWER_FIDGET,
	DUAL_FLAMETHROWER_ON,
	DUAL_FLAMETHROWER_CYCLE,
	DUAL_FLAMETHROWER_OFF,
	DUAL_FLAMETHROWER_FIRE1,
	DUAL_FLAMETHROWER_FIRE2,
	DUAL_FLAMETHROWER_FIRE3,
	DUAL_FLAMETHROWER_FIRE4,
	DUAL_FLAMETHROWER_DEPLOY_LOWKEY,
	DUAL_FLAMETHROWER_DEPLOY,
	DUAL_FLAMETHROWER_HOLSTER,
};

void EV_FireDualFlameStream( event_args_t *args )
{
	int idx;
	vec3_t origin,angles,forward,right,up;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	int iStartup = args->bparam1;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );

	AngleVectors( angles, forward, right, up );

	if ( iStartup )
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "flameburst.wav", 1, ATTN_NORM, 0, PITCH_NORM );
	else
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, "flamerun.wav", 1, ATTN_NORM, 0, PITCH_NORM );

	if ( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation ( FLAMETHROWER_FIRE3, 0 );
}

void EV_FireDualFlameThrower( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy( args->origin, origin );

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "flamethrower.wav", 0.9, ATTN_NORM, 0, PITCH_NORM );

	if ( EV_IsLocal( idx ) )
	{
		gEngfuncs.pEventAPI->EV_WeaponAnimation( FLAMETHROWER_FIRE4, 0 );
		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-2.0, -4.0) );
		V_PunchAxis(YAW, gEngfuncs.pfnRandomFloat(-1.0, -2.0));
		V_PunchAxis(ROLL, gEngfuncs.pfnRandomFloat(1.0, 2.0));
	}
}

void EV_EndDualFlameThrower( event_args_t *args )
{
	int idx;
	vec3_t origin;

	idx = args->entindex;
	VectorCopy ( args->origin, origin );

	gEngfuncs.pEventAPI->EV_StopAllSounds(idx,CHAN_STATIC);

	if ( EV_IsLocal( idx ) )
		gEngfuncs.pEventAPI->EV_WeaponAnimation ( FLAMETHROWER_OFF, 0 );
	
	if ( args->iparam1 )
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "flamethrowerend.wav", 1, 0.6, 0, 100 );
}

enum sawedoff_e {
	SAWEDOFF_IDLE = 0,
	SAWEDOFF_IDLE2,
	SAWEDOFF_SHOOTLEFT,
	SAWEDOFF_SHOOTRIGHT,
	SAWEDOFF_SHOOTBOTH,
	SAWEDOFF_RELOAD,
	SAWEDOFF_DRAW,
	SAWEDOFF_HOLSTER,
};

void EV_FireSawedOff( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	int j;
	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	int clip = args->bparam1;

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		if (clip % 2 == 0)
			gEngfuncs.pEventAPI->EV_WeaponAnimation( SAWEDOFF_SHOOTRIGHT, 0 );
		else
			gEngfuncs.pEventAPI->EV_WeaponAnimation( SAWEDOFF_SHOOTLEFT, 0 );
		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-15.0, -17.0) );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "sawedoff.wav", gEngfuncs.pfnRandomFloat(0.98, 1.0), ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	if (clip % 2 == 0)
		EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.8, idx, args->ducking, forward, right, up, 0, 0, 0);
	else
		EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[1], 0.8, idx, args->ducking, forward, right, up, 0, 0, 0);


	EV_HLDM_FireBullets( idx, forward, right, up, 8, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT_SPECIAL, 1, &tracerCount[idx-1], 0.17365, 0.04362 );
}

void EV_FireSawedOffDouble( event_args_t *args )
{
	int idx;
	vec3_t origin;
	vec3_t angles;
	vec3_t velocity;

	int j;
	vec3_t ShellVelocity;
	vec3_t ShellOrigin;
	vec3_t vecSrc, vecAiming;
	vec3_t vecSpread;
	vec3_t up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		gEngfuncs.pEventAPI->EV_WeaponAnimation( SAWEDOFF_SHOOTBOTH, 0 );
		V_PunchAxis(PITCH, gEngfuncs.pfnRandomFloat(-20.0, -22.0) );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "sawedoff.wav", gEngfuncs.pfnRandomFloat(0.98, 1.0), ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );
	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "sawedoff.wav", gEngfuncs.pfnRandomFloat(0.98, 1.0), ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[0], 0.8, idx, args->ducking, forward, right, up, 0, 0, 0);
	EV_GunSmoke(gEngfuncs.GetViewModel()->attachment[1], 0.8, idx, args->ducking, forward, right, up, 0, 0, 0);

	EV_HLDM_FireBullets( idx, forward, right, up, 16, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT_SPECIAL, 1, &tracerCount[idx-1], 0.17365, 0.04362 );
}

void EV_TrainPitchAdjust( event_args_t *args )
{
	int idx;
	vec3_t origin;

	unsigned short us_params;
	int noise;
	float m_flVolume;
	int pitch;
	int stop;
	
	char sz[ 256 ];

	idx = args->entindex;
	
	VectorCopy( args->origin, origin );

	us_params = (unsigned short)args->iparam1;
	stop	  = args->bparam1;

	m_flVolume	= (float)(us_params & 0x003f)/40.0;
	noise		= (int)(((us_params) >> 12 ) & 0x0007);
	pitch		= (int)( 10.0 * (float)( ( us_params >> 6 ) & 0x003f ) );

	switch ( noise )
	{
	case 1: strcpy( sz, "plats/ttrain1.wav"); break;
	case 2: strcpy( sz, "plats/ttrain2.wav"); break;
	case 3: strcpy( sz, "plats/ttrain3.wav"); break; 
	case 4: strcpy( sz, "plats/ttrain4.wav"); break;
	case 5: strcpy( sz, "plats/ttrain6.wav"); break;
	case 6: strcpy( sz, "plats/ttrain7.wav"); break;
	default:
		// no sound
		strcpy( sz, "" );
		return;
	}

	if ( stop )
	{
		gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, sz );
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, sz, m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, pitch );
	}
}

int EV_TFC_IsAllyTeam( int iTeam1, int iTeam2 )
{
	return 0;
}