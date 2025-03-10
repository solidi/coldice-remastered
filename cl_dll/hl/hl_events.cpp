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
#include "../hud.h"
#include "../cl_util.h"
#include "event_api.h"

extern "C"
{
// HLDM
void EV_FireGlock1( struct event_args_s *args  );
void EV_FireShotGunSingle( struct event_args_s *args  );
void EV_FireShotGunDouble( struct event_args_s *args  );
void EV_FireMP5( struct event_args_s *args  );
void EV_FireMP52( struct event_args_s *args  );
void EV_FirePython( struct event_args_s *args  );
void EV_FireGauss( struct event_args_s *args  );
void EV_SpinGauss( struct event_args_s *args  );
void EV_Crowbar( struct event_args_s *args );
void EV_FireCrossbow( struct event_args_s *args );
void EV_FireCrossbow2( struct event_args_s *args );
void EV_FireRpg( struct event_args_s *args );
void EV_EgonFire( struct event_args_s *args );
void EV_EgonStop( struct event_args_s *args );
void EV_HornetGunFire( struct event_args_s *args );
void EV_TripmineFire( struct event_args_s *args );
void EV_SnarkFire( struct event_args_s *args );

// Ice
void EV_Knife( struct event_args_s *args );
void EV_FireRpgExtreme( struct event_args_s *args );
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
void EV_Chainsaw( struct event_args_s *args );
void EV_Fire12GaugeSingle( struct event_args_s *args  );
void EV_FireNuke( struct event_args_s *args  );
void EV_SnarkRelease( struct event_args_s *args );
void EV_FireDeagle( struct event_args_s *args );
void EV_FireDualDeagle( struct event_args_s *args );
void EV_FireDualDeagleBoth( struct event_args_s *args );
void EV_FireDualRpgBoth( struct event_args_s *args );
void EV_FireDualMag60( struct event_args_s *args );
void EV_FireDualSmg( struct event_args_s *args );
void EV_FireDualWrench( struct event_args_s *args );
void EV_FireDualUsas( struct event_args_s *args );
void EV_FireDualUsasBoth( struct event_args_s *args );
void EV_FireFreezeGun( struct event_args_s *args );
void EV_RocketCrowbar( struct event_args_s *args );
void EV_GravityGun( struct event_args_s *args  );
void EV_FireFlameStream( struct event_args_s *args  );
void EV_FireFlameThrower( struct event_args_s *args  );
void EV_EndFlameThrower( struct event_args_s *args  );
void EV_FireDualFlameStream( struct event_args_s *args  );
void EV_FireDualFlameThrower( struct event_args_s *args  );
void EV_EndDualFlameThrower( struct event_args_s *args  );
void EV_FireSawedOff( struct event_args_s *args  );
void EV_FireSawedOffDouble( struct event_args_s *args  );
void EV_FireDualSawedOff( struct event_args_s *args  );
void EV_FireDualSawedOffDouble( struct event_args_s *args  );
void EV_FireDualChaingun( struct event_args_s *args  );
void EV_FireDualHornetGun( struct event_args_s *args  );
void EV_FireFingergun( struct event_args_s *args  );
void EV_FireZapgun( struct event_args_s *args  );
void EV_FireDualGlock( struct event_args_s *args );
void EV_FireVice( struct event_args_s *args );

void EV_TrainPitchAdjust( struct event_args_s *args );
void EV_VehiclePitchAdjust( struct event_args_s *args );
}

/*
======================
Game_HookEvents

Associate script file name with callback functions.  Callback's must be extern "C" so
 the engine doesn't get confused about name mangling stuff.  Note that the format is
 always the same.  Of course, a clever mod team could actually embed parameters, behavior
 into the actual .sc files and create a .sc file parser and hook their functionality through
 that.. i.e., a scripting system.

That was what we were going to do, but we ran out of time...oh well.
======================
*/
void Game_HookEvents( void )
{
	gEngfuncs.pfnHookEvent( "events/glock1.sc",					EV_FireGlock1 );
	gEngfuncs.pfnHookEvent( "events/shotgun1.sc",				EV_FireShotGunSingle );
	gEngfuncs.pfnHookEvent( "events/shotgun2.sc",				EV_FireShotGunDouble );
	gEngfuncs.pfnHookEvent( "events/mp5.sc",					EV_FireMP5 );
	gEngfuncs.pfnHookEvent( "events/mp52.sc",					EV_FireMP52 );
	gEngfuncs.pfnHookEvent( "events/python.sc",					EV_FirePython );
	gEngfuncs.pfnHookEvent( "events/gauss.sc",					EV_FireGauss );
	gEngfuncs.pfnHookEvent( "events/gaussspin.sc",				EV_SpinGauss );
	gEngfuncs.pfnHookEvent( "events/train.sc",					EV_TrainPitchAdjust );
	gEngfuncs.pfnHookEvent( "events/crowbar.sc",				EV_Crowbar );
	gEngfuncs.pfnHookEvent( "events/crossbow1.sc",				EV_FireCrossbow );
	gEngfuncs.pfnHookEvent( "events/crossbow2.sc",				EV_FireCrossbow2 );
	gEngfuncs.pfnHookEvent( "events/rpg.sc",					EV_FireRpg );
	gEngfuncs.pfnHookEvent( "events/egon_fire.sc",				EV_EgonFire );
	gEngfuncs.pfnHookEvent( "events/egon_stop.sc",				EV_EgonStop );
	gEngfuncs.pfnHookEvent( "events/firehornet.sc",				EV_HornetGunFire );
	gEngfuncs.pfnHookEvent( "events/tripfire.sc",				EV_TripmineFire );
	gEngfuncs.pfnHookEvent( "events/snarkfire.sc",				EV_SnarkFire );

	// Ice
	gEngfuncs.pfnHookEvent( "events/knife.sc",					EV_Knife );
	gEngfuncs.pfnHookEvent( "events/rpg_extreme.sc",			EV_FireRpgExtreme );
	gEngfuncs.pfnHookEvent( "events/chumtoadfire.sc",			EV_ChumtoadFire );
	gEngfuncs.pfnHookEvent( "events/chumtoadrelease.sc",		EV_ChumtoadRelease );
	gEngfuncs.pfnHookEvent( "events/sniper_rifle.sc",			EV_FireSniperRifle );
	gEngfuncs.pfnHookEvent( "events/cannon.sc",					EV_FireCannon );
	gEngfuncs.pfnHookEvent( "events/cannon_flak.sc",			EV_FireCannonFlak );
	gEngfuncs.pfnHookEvent( "events/mag60.sc",					EV_FireMag60 );
	gEngfuncs.pfnHookEvent( "events/chaingun.sc",				EV_FireChaingun );
	gEngfuncs.pfnHookEvent( "events/glauncher.sc",				EV_FireGrenadeLauncher );
	gEngfuncs.pfnHookEvent( "events/smg.sc",					EV_FireSmg );
	gEngfuncs.pfnHookEvent( "events/usas.sc",					EV_FireUsas );
	gEngfuncs.pfnHookEvent( "events/fists.sc",					EV_Fists );
	gEngfuncs.pfnHookEvent( "events/wrench.sc",					EV_Wrench );
	gEngfuncs.pfnHookEvent( "events/chainsaw.sc",				EV_Chainsaw );
	gEngfuncs.pfnHookEvent( "events/gauge_single.sc",			EV_Fire12GaugeSingle );
	gEngfuncs.pfnHookEvent( "events/nuke1.sc",					EV_FireNuke );
	gEngfuncs.pfnHookEvent( "events/snarkrelease.sc",			EV_SnarkRelease );
	gEngfuncs.pfnHookEvent( "events/deagle.sc",					EV_FireDeagle );
	gEngfuncs.pfnHookEvent( "events/dual_deagle.sc",			EV_FireDualDeagle );
	gEngfuncs.pfnHookEvent( "events/dual_deagle_both.sc",		EV_FireDualDeagleBoth );
	gEngfuncs.pfnHookEvent( "events/dual_rpg_both.sc",			EV_FireDualRpgBoth );
	gEngfuncs.pfnHookEvent( "events/dual_mag60.sc",				EV_FireDualMag60 );
	gEngfuncs.pfnHookEvent( "events/dual_smg.sc",				EV_FireDualSmg );
	gEngfuncs.pfnHookEvent( "events/dual_wrench.sc",			EV_FireDualWrench );
	gEngfuncs.pfnHookEvent( "events/dual_usas.sc",				EV_FireDualUsas );
	gEngfuncs.pfnHookEvent( "events/dual_usas_both.sc",			EV_FireDualUsasBoth );
	gEngfuncs.pfnHookEvent( "events/freezegun.sc",				EV_FireFreezeGun );
	gEngfuncs.pfnHookEvent( "events/rocketcrowbar.sc",			EV_RocketCrowbar );
	gEngfuncs.pfnHookEvent( "events/gravitygun.sc",				EV_GravityGun );
	gEngfuncs.pfnHookEvent( "events/flamestream.sc",			EV_FireFlameStream );
	gEngfuncs.pfnHookEvent( "events/flamethrower.sc",			EV_FireFlameThrower );
	gEngfuncs.pfnHookEvent( "events/flamethrowerend.sc",		EV_EndFlameThrower );
	gEngfuncs.pfnHookEvent( "events/dual_flamestream.sc",		EV_FireDualFlameStream );
	gEngfuncs.pfnHookEvent( "events/dual_flamethrower.sc",		EV_FireDualFlameThrower );
	gEngfuncs.pfnHookEvent( "events/dual_flamethrowerend.sc",	EV_EndDualFlameThrower );
	gEngfuncs.pfnHookEvent( "events/sawedoff1.sc",				EV_FireSawedOff );
	gEngfuncs.pfnHookEvent( "events/sawedoff2.sc",				EV_FireSawedOffDouble );
	gEngfuncs.pfnHookEvent( "events/dualsawedoff1.sc",			EV_FireDualSawedOff );
	gEngfuncs.pfnHookEvent( "events/dualsawedoff2.sc",			EV_FireDualSawedOffDouble );
	gEngfuncs.pfnHookEvent( "events/dual_chaingun.sc",			EV_FireDualChaingun );
	gEngfuncs.pfnHookEvent( "events/dual_hornetgun.sc",			EV_FireDualHornetGun );
	gEngfuncs.pfnHookEvent( "events/fingergun.sc",				EV_FireFingergun );
	gEngfuncs.pfnHookEvent( "events/zapgun.sc",					EV_FireZapgun );
	gEngfuncs.pfnHookEvent( "events/vehicle.sc",				EV_VehiclePitchAdjust );
	gEngfuncs.pfnHookEvent( "events/dual_glock.sc",				EV_FireDualGlock );
	gEngfuncs.pfnHookEvent( "events/vice.sc",					EV_FireVice );
}
