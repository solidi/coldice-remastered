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
//=========================================================
// GameRules.cpp
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"
#include	"teamplay_gamerules.h"
#include	"skill.h"
#include	"game.h"
#include	"items.h"
#include	"gungame_gamerules.h"
#include	"ctc_gamerules.h"
#include	"jvs_gamerules.h"
#include	"chilldemic_gamerules.h"
#include	"lms_gamerules.h"
#include	"coldskull_gamerules.h"
#include	"arena_gamerules.h"
#include	"ctf_gamerules.h"
#include	"shidden_gamerules.h"
#include	"horde_gamerules.h"
#include	"instagib_gamerules.h"
#include	"prophunt_gamerules.h"
#include	"busters_gamerules.h"
#include	"coldspot_gamerules.h"
#include	"shake.h"

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );

DLL_GLOBAL CGameRules*	g_pGameRules = NULL;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgMOTD;
extern int gmsgShowGameTitle;
extern int gmsgAddMutator;
extern int gmsgFog;
extern int gmsgChaos;
extern int gmsgHideWeapon;

int g_teamplay = 0;
int g_ExplosiveAI = 0;
int g_ItemsExplode = 0;

extern DLL_GLOBAL int g_GameMode;

DLL_GLOBAL const char *g_szMutators[] = {
	"chaos",
	"999",
	"astronaut",
	"autoaim",
	"barrels",
	"berserker",
	"bigfoot",
	"bighead",
	"busters",
	"chumxplode",
	"closeup",
	"coolflesh",
	"cowboy",
	"crate",
	"credits",
	"dealter",
	"dontshoot",
	"explosiveai",
	"fastweapons",
	"firebullets",
	"firestarter",
	"fog",
	"godmode",
	"goldenguns",
	"grenades",
	"halflife",
	"ice",
	"infiniteammo",
	"instagib",
	"inverse",
	"invisible",
	"itemsexplode",
	"jack",
	"jeepathon",
	"jope",
	"lightsout",
	"longjump",
	"loopback",
	"marshmellow",
	"maxpack",
	"megarun",
	"minime",
	"mirror",
	"napkinstory",
	"noclip",
	"noreload",
	"notify",
	"notthebees",
	"oldtime",
	"paintball",
	"paper",
	"piratehat",
	"plumber",
	"portal",
	"pumpkin",
	"pushy",
	"railguns",
	"randomweapon",
	"ricochet",
	"rocketbees",
	"rocketcrowbar",
	"rockets",
	"sanic",
	"santahat",
	"sildenafil",
	"slowbullets",
	"slowmo",
	"slowweapons",
	"snowballs",
	"speedup",
	"stahp",
	"superjump",
	"thirdperson",
	"three",
	"toilet",
	"topsyturvy",
	"turrets",
	"vested",
	"volatile",
};

//=========================================================
//=========================================================
BOOL CGameRules::CanHaveAmmo( CBasePlayer *pPlayer, const char *pszAmmoName, int iMaxCarry )
{
	int iAmmoIndex;

	if ( pszAmmoName )
	{
		iAmmoIndex = pPlayer->GetAmmoIndex( pszAmmoName );

		if ( iAmmoIndex > -1 )
		{
			if ( pPlayer->AmmoInventory( iAmmoIndex ) < iMaxCarry )
			{
				// player has room for more of this type of ammo
				return TRUE;
			}
		}
	}

	return FALSE;
}

//=========================================================
//=========================================================
edict_t *CGameRules :: GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	edict_t *pentSpawnSpot = EntSelectSpawnPoint( pPlayer );

	pPlayer->pev->origin = VARS(pentSpawnSpot)->origin + Vector(0,0,1);
	pPlayer->pev->v_angle  = g_vecZero;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->angles = VARS(pentSpawnSpot)->angles;
	pPlayer->pev->punchangle = g_vecZero;
	pPlayer->pev->fixangle = TRUE;
	
	return pentSpawnSpot;
}

//=========================================================
//=========================================================
BOOL CGameRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	// only living players can have items
	if ( pPlayer->pev->deadflag != DEAD_NO )
		return FALSE;

	if ( pWeapon->pszAmmo1() )
	{
		if ( !CanHaveAmmo( pPlayer, pWeapon->pszAmmo1(), pWeapon->iMaxAmmo1() ) )
		{
			// we can't carry anymore ammo for this gun. We can only 
			// have the gun if we aren't already carrying one of this type
			if ( pPlayer->HasPlayerItem( pWeapon ) )
			{
				return FALSE;
			}
		}
	}
	else
	{
		// weapon doesn't use ammo, don't take another if you already have it.
		if ( pPlayer->HasPlayerItem( pWeapon ) )
		{
			return FALSE;
		}
	}

	// note: will fall through to here if GetItemInfo doesn't fill the struct!
	return TRUE;
}

BOOL CGameRules::CanHavePlayerAmmo( CBasePlayer *pPlayer, CBasePlayerAmmo *pAmmo )
{
	return TRUE;
}

//=========================================================
// load the SkillData struct with the proper values based on the skill level.
//=========================================================
void CGameRules::RefreshSkillData ( void )
{
	int	iSkill;

	iSkill = (int)CVAR_GET_FLOAT("skill");
	g_iSkillLevel = iSkill;

	if ( iSkill < 1 )
	{
		iSkill = 1;
	}
	else if ( iSkill > 3 )
	{
		iSkill = 3; 
	}

	gSkillData.iSkillLevel = iSkill;

	ALERT ( at_console, "\nGAME SKILL LEVEL:%d\n",iSkill );

	//Agrunt		
	gSkillData.agruntHealth = GetSkillCvar( "sk_agrunt_health" );
	gSkillData.agruntDmgPunch = GetSkillCvar( "sk_agrunt_dmg_punch");

	// Apache 
	gSkillData.apacheHealth = GetSkillCvar( "sk_apache_health");

	// Barney
	gSkillData.barneyHealth = GetSkillCvar( "sk_barney_health");

	// Big Momma
	gSkillData.bigmommaHealthFactor = GetSkillCvar( "sk_bigmomma_health_factor" );
	gSkillData.bigmommaDmgSlash = GetSkillCvar( "sk_bigmomma_dmg_slash" );
	gSkillData.bigmommaDmgBlast = GetSkillCvar( "sk_bigmomma_dmg_blast" );
	gSkillData.bigmommaRadiusBlast = GetSkillCvar( "sk_bigmomma_radius_blast" );

	// Bullsquid
	gSkillData.bullsquidHealth = GetSkillCvar( "sk_bullsquid_health");
	gSkillData.bullsquidDmgBite = GetSkillCvar( "sk_bullsquid_dmg_bite");
	gSkillData.bullsquidDmgWhip = GetSkillCvar( "sk_bullsquid_dmg_whip");
	gSkillData.bullsquidDmgSpit = GetSkillCvar( "sk_bullsquid_dmg_spit");

	// Gargantua
	gSkillData.gargantuaHealth = GetSkillCvar( "sk_gargantua_health");
	gSkillData.gargantuaDmgSlash = GetSkillCvar( "sk_gargantua_dmg_slash");
	gSkillData.gargantuaDmgFire = GetSkillCvar( "sk_gargantua_dmg_fire");
	gSkillData.gargantuaDmgStomp = GetSkillCvar( "sk_gargantua_dmg_stomp");

	// Hassassin
	gSkillData.hassassinHealth = GetSkillCvar( "sk_hassassin_health");

	// Headcrab
	gSkillData.headcrabHealth = GetSkillCvar( "sk_headcrab_health");
	gSkillData.headcrabDmgBite = GetSkillCvar( "sk_headcrab_dmg_bite");

	// Hgrunt 
	gSkillData.hgruntHealth = GetSkillCvar( "sk_hgrunt_health");
	gSkillData.hgruntDmgKick = GetSkillCvar( "sk_hgrunt_kick");
	gSkillData.hgruntShotgunPellets = GetSkillCvar( "sk_hgrunt_pellets");
	gSkillData.hgruntGrenadeSpeed = GetSkillCvar( "sk_hgrunt_gspeed");

	// Houndeye
	gSkillData.houndeyeHealth = GetSkillCvar( "sk_houndeye_health");
	gSkillData.houndeyeDmgBlast = GetSkillCvar( "sk_houndeye_dmg_blast");

	// ISlave
	gSkillData.slaveHealth = GetSkillCvar( "sk_islave_health");
	gSkillData.slaveDmgClaw = GetSkillCvar( "sk_islave_dmg_claw");
	gSkillData.slaveDmgClawrake = GetSkillCvar( "sk_islave_dmg_clawrake");
	gSkillData.slaveDmgZap = GetSkillCvar( "sk_islave_dmg_zap");

	// Icthyosaur
	gSkillData.ichthyosaurHealth = GetSkillCvar( "sk_ichthyosaur_health");
	gSkillData.ichthyosaurDmgShake = GetSkillCvar( "sk_ichthyosaur_shake");

	// Leech
	gSkillData.leechHealth = GetSkillCvar( "sk_leech_health");

	gSkillData.leechDmgBite = GetSkillCvar( "sk_leech_dmg_bite");

	// Controller
	gSkillData.controllerHealth = GetSkillCvar( "sk_controller_health");
	gSkillData.controllerDmgZap = GetSkillCvar( "sk_controller_dmgzap");
	gSkillData.controllerSpeedBall = GetSkillCvar( "sk_controller_speedball");
	gSkillData.controllerDmgBall = GetSkillCvar( "sk_controller_dmgball");

	// Nihilanth
	gSkillData.nihilanthHealth = GetSkillCvar( "sk_nihilanth_health");
	gSkillData.nihilanthZap = GetSkillCvar( "sk_nihilanth_zap");

	// Scientist
	gSkillData.scientistHealth = GetSkillCvar( "sk_scientist_health");

	// Snark
	gSkillData.snarkHealth = GetSkillCvar( "sk_snark_health");
	gSkillData.snarkDmgBite = GetSkillCvar( "sk_snark_dmg_bite");
	gSkillData.snarkDmgPop = GetSkillCvar( "sk_snark_dmg_pop");

	// Zombie
	gSkillData.zombieHealth = GetSkillCvar( "sk_zombie_health");
	gSkillData.zombieDmgOneSlash = GetSkillCvar( "sk_zombie_dmg_one_slash");
	gSkillData.zombieDmgBothSlash = GetSkillCvar( "sk_zombie_dmg_both_slash");

	//Turret
	gSkillData.turretHealth = GetSkillCvar( "sk_turret_health");

	// MiniTurret
	gSkillData.miniturretHealth = GetSkillCvar( "sk_miniturret_health");
	
	// Sentry Turret
	gSkillData.sentryHealth = GetSkillCvar( "sk_sentry_health");

// PLAYER WEAPONS

	// Crowbar whack
	gSkillData.plrDmgCrowbar = GetSkillCvar( "sk_plr_crowbar");

	// Glock Round
	gSkillData.plrDmg9MM = GetSkillCvar( "sk_plr_9mm_bullet");

	// 357 Round
	gSkillData.plrDmg357 = GetSkillCvar( "sk_plr_357_bullet");

	// Rifle Round
	gSkillData.plrDmgSniperRifle = GetSkillCvar( "sk_plr_sniper_rifle_bullet");

	// MP5 Round
	gSkillData.plrDmgMP5 = GetSkillCvar( "sk_plr_9mmAR_bullet");

	// M203 grenade
	gSkillData.plrDmgM203Grenade = GetSkillCvar( "sk_plr_9mmAR_grenade");

	// Shotgun buckshot
	gSkillData.plrDmgBuckshot = GetSkillCvar( "sk_plr_buckshot");

	gSkillData.plrDmgExpBuckshot = GetSkillCvar( "sk_plr_expbuckshot");

	// Crossbow
	gSkillData.plrDmgCrossbowClient = GetSkillCvar( "sk_plr_xbow_bolt_client");
	gSkillData.plrDmgCrossbowMonster = GetSkillCvar( "sk_plr_xbow_bolt_monster");

	// RPG
	gSkillData.plrDmgRPG = GetSkillCvar( "sk_plr_rpg");

	// Gauss gun
	gSkillData.plrDmgGauss = GetSkillCvar( "sk_plr_gauss");

	// Egon Gun
	gSkillData.plrDmgEgonNarrow = GetSkillCvar( "sk_plr_egon_narrow");
	gSkillData.plrDmgEgonWide = GetSkillCvar( "sk_plr_egon_wide");

	// Hand Grendade
	gSkillData.plrDmgHandGrenade = GetSkillCvar( "sk_plr_hand_grenade");

	// Satchel Charge
	gSkillData.plrDmgSatchel = GetSkillCvar( "sk_plr_satchel");

	// Tripmine
	gSkillData.plrDmgTripmine = GetSkillCvar( "sk_plr_tripmine");

	// Vest
	gSkillData.plrDmgVest = GetSkillCvar( "sk_plr_vest");

	// Cluster Grenades
	gSkillData.plrDmgClusterGrenade = GetSkillCvar( "sk_plr_cgrenade");

	// Knife
	gSkillData.plrDmgKnife = GetSkillCvar( "sk_plr_knife");

	// Flying Knife
	gSkillData.plrDmgFlyingKnife = GetSkillCvar( "sk_plr_flyingknife");

	// Flying Crowbar
	gSkillData.plrDmgFlyingCrowbar = GetSkillCvar( "sk_plr_flyingcrowbar");

	// Chumtoad
	gSkillData.chumtoadHealth = GetSkillCvar( "sk_chumtoad_health");
	gSkillData.chumtoadDmgBite = GetSkillCvar( "sk_chumtoad_dmg_bite");
	gSkillData.chumtoadDmgPop = GetSkillCvar( "sk_chumtoad_dmg_pop");

	// Railgun
	gSkillData.plrDmgRailgun = GetSkillCvar( "sk_plr_railgun");

	// Flak
	gSkillData.plrDmgFlak = GetSkillCvar( "sk_plr_flak");
	gSkillData.plrDmgFlakBomb = GetSkillCvar( "sk_plr_flakbomb");

	// Fists
	gSkillData.plrDmgFists = GetSkillCvar( "sk_plr_fists");
	gSkillData.plrDmgShoryuken = GetSkillCvar( "sk_plr_shoryuken");

	// Wrench
	gSkillData.plrDmgWrench = GetSkillCvar( "sk_plr_wrench");

	// Flying Wrench
	gSkillData.plrDmgFlyingWrench = GetSkillCvar( "sk_plr_flyingwrench");

	// Snowball
	gSkillData.plrDmgSnowball = GetSkillCvar( "sk_plr_snowball");

	// Chainsaw
	gSkillData.plrDmgChainsaw = GetSkillCvar( "sk_plr_chainsaw");

	// Nuke
	gSkillData.plrDmgNuke = GetSkillCvar( "sk_plr_nuke");

	// Kick
	gSkillData.plrDmgKick = GetSkillCvar( "sk_plr_kick" );

	// Plasma
	gSkillData.plrDmgPlasma = GetSkillCvar( "sk_plr_plasma" );

	// Gravity Gun
	gSkillData.plrDmgGravityGun = GetSkillCvar( "sk_plr_gravitygun" );

	// Flame Thrower
	gSkillData.plrDmgFlameThrower = GetSkillCvar( "sk_plr_flamethrower" );

	// Knife Snipe
	gSkillData.plrDmgKnifeSnipe = GetSkillCvar( "sk_plr_knifesnipe" );

	// Panther
	gSkillData.pantherHealth = GetSkillCvar( "sk_panther_health");
	gSkillData.pantherSlash = GetSkillCvar( "sk_panther_slash");

	// Grapple Hook
	gSkillData.plrDmgHook = GetSkillCvar( "sk_plr_hook" );
	gSkillData.plrSpeedHook = GetSkillCvar( "sk_plr_hookspeed" );

	// MONSTER WEAPONS
	gSkillData.monDmg12MM = GetSkillCvar( "sk_12mm_bullet");
	gSkillData.monDmgMP5 = GetSkillCvar ("sk_9mmAR_bullet" );
	gSkillData.monDmg9MM = GetSkillCvar( "sk_9mm_bullet");

	// MONSTER HORNET
	gSkillData.monDmgHornet = GetSkillCvar( "sk_hornet_dmg");

	// PLAYER HORNET
// Up to this point, player hornet damage and monster hornet damage were both using
// monDmgHornet to determine how much damage to do. In tuning the hivehand, we now need
// to separate player damage and monster hivehand damage. Since it's so late in the project, we've
// added plrDmgHornet to the SKILLDATA struct, but not to the engine CVar list, so it's inaccesible
// via SKILLS.CFG. Any player hivehand tuning must take place in the code. (sjb)
	gSkillData.plrDmgHornet = 7;


	// HEALTH/CHARGE
	gSkillData.suitchargerCapacity = GetSkillCvar( "sk_suitcharger" );
	gSkillData.batteryCapacity = GetSkillCvar( "sk_battery" );
	gSkillData.healthchargerCapacity = GetSkillCvar ( "sk_healthcharger" );
	gSkillData.healthkitCapacity = GetSkillCvar ( "sk_healthkit" );
	gSkillData.scientistHeal = GetSkillCvar ( "sk_scientist_heal" );

	// monster damage adj
	gSkillData.monHead = GetSkillCvar( "sk_monster_head" );
	gSkillData.monChest = GetSkillCvar( "sk_monster_chest" );
	gSkillData.monStomach = GetSkillCvar( "sk_monster_stomach" );
	gSkillData.monLeg = GetSkillCvar( "sk_monster_leg" );
	gSkillData.monArm = GetSkillCvar( "sk_monster_arm" );

	// player damage adj
	gSkillData.plrHead = GetSkillCvar( "sk_player_head" );
	gSkillData.plrChest = GetSkillCvar( "sk_player_chest" );
	gSkillData.plrStomach = GetSkillCvar( "sk_player_stomach" );
	gSkillData.plrLeg = GetSkillCvar( "sk_player_leg" );
	gSkillData.plrArm = GetSkillCvar( "sk_player_arm" );
}

//=========================================================
// instantiate the proper game rules object
//=========================================================

CGameRules *InstallGameRules( void )
{
	SERVER_COMMAND( "exec game.cfg\n" );
	SERVER_EXECUTE( );

	if ( !gpGlobals->deathmatch )
	{
		// generic half-life
		g_teamplay = 0;
		return new CHalfLifeRules;
	}
	else
	{
		if ( teamplay.value > 0 )
		{
			// teamplay

			g_teamplay = 1;
			return new CHalfLifeTeamplay;
		}

		g_teamplay = 0;

		switch (g_GameMode)
		{
			case GAME_ARENA:
				return new CHalfLifeArena;
			case GAME_BUSTERS:
				return new CMultiplayBusters;
			case GAME_LMS:
				return new CHalfLifeLastManStanding;
			case GAME_CHILLDEMIC:
				return new CHalfLifeChilldemic;
			case GAME_COLDSKULL:
				return new CHalfLifeColdSkull;
			case GAME_COLDSPOT:
				return new CHalfLifeColdSpot;
			case GAME_CTC:
				return new CHalfLifeCaptureTheChumtoad;
			case GAME_CTF:
				return new CHalfLifeCaptureTheFlag;
			case GAME_GUNGAME:
				return new CHalfLifeGunGame;
			case GAME_HORDE:
				return new CHalfLifeHorde;
			case GAME_INSTAGIB:
				return new CHalfLifeInstagib;
			case GAME_ICEMAN:
				return new CHalfLifeJesusVsSanta;
			case GAME_PROPHUNT:
				return new CHalfLifePropHunt;
			case GAME_SHIDDEN:
				return new CHalfLifeShidden;
		}

		if ((int)gpGlobals->deathmatch == 1)
		{
			// vanilla deathmatch
			return new CHalfLifeMultiplay;
		}
		else
		{
			// vanilla deathmatch??
			return new CHalfLifeMultiplay;
		}
	}
}

void CGameRules::EnvMutators( void )
{
	if (MutatorEnabled(MUTATOR_SLOWMO) && CVAR_GET_FLOAT("sys_timescale") != 0.49f)
		CVAR_SET_FLOAT("sys_timescale", 0.49);
	else
	{
		if (!MutatorEnabled(MUTATOR_SLOWMO) &&
			CVAR_GET_FLOAT("sys_timescale") > 0.48f && CVAR_GET_FLOAT("sys_timescale") < 0.50f)
			CVAR_SET_FLOAT("sys_timescale", 1.0);
	}

	// Lights out
	int toggleFlashlight = 0;
	if (MutatorEnabled(MUTATOR_LIGHTSOUT))
	{
		LIGHT_STYLE(0, "b");
		if (flashlight.value != 1)
		{
			CVAR_SET_STRING("mp_flashlight", "2");
			toggleFlashlight = 2;
		}
		CVAR_SET_STRING("sv_skycolor_r", "1");
		CVAR_SET_STRING("sv_skycolor_g", "1");
		CVAR_SET_STRING("sv_skycolor_b", "1");
	}
	else
	{
		LIGHT_STYLE(0, "m");
		if (flashlight.value == 2)
		{
			CVAR_SET_STRING("mp_flashlight", "0");
			toggleFlashlight = 1;
		}
		CVAR_SET_STRING("sv_skycolor_r", szSkyColorRed);
		CVAR_SET_STRING("sv_skycolor_g", szSkyColorGreen);
		CVAR_SET_STRING("sv_skycolor_b", szSkyColorBlue);
	}

	// Jump height
	if (MutatorEnabled(MUTATOR_SUPERJUMP) && CVAR_GET_FLOAT("sv_jumpheight") != 299)
		CVAR_SET_FLOAT("sv_jumpheight", 299);
	else
	{
		if (!MutatorEnabled(MUTATOR_SUPERJUMP) && CVAR_GET_FLOAT("sv_jumpheight") == 299)
			CVAR_SET_FLOAT("sv_jumpheight", 45);
	}

	// Gravity
	if (MutatorEnabled(MUTATOR_ASTRONAUT) && CVAR_GET_FLOAT("sv_gravity") != 199)
		CVAR_SET_FLOAT("sv_gravity", 199);
	else
	{
		if (!MutatorEnabled(MUTATOR_ASTRONAUT) && CVAR_GET_FLOAT("sv_gravity") == 199)
			CVAR_SET_FLOAT("sv_gravity", 800);
	}

	if (MutatorEnabled(MUTATOR_BIGFOOT) && CVAR_GET_FLOAT("sv_stepsize") != 192)
		CVAR_SET_FLOAT("sv_stepsize", 192);
	else
	{
		if (!MutatorEnabled(MUTATOR_BIGFOOT) && CVAR_GET_FLOAT("sv_stepsize") == 192)
			CVAR_SET_FLOAT("sv_stepsize", 18);
	}

	// Player loop
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		CBasePlayer *pl = (CBasePlayer *)pPlayer;

		if (pPlayer && pPlayer->IsPlayer() && !pl->HasDisconnected)
		{
			if (MutatorEnabled(MUTATOR_FOG))
			{
				MESSAGE_BEGIN(MSG_ONE, gmsgFog, NULL, pPlayer->pev);
					WRITE_COORD(50);
					WRITE_COORD(200);
					WRITE_BYTE(125);
					WRITE_BYTE(125);
					WRITE_BYTE(125);
					WRITE_COORD(0);
				MESSAGE_END();
			}
			else
			{
				CBaseEntity *pEntity = UTIL_FindEntityByClassname(NULL, "env_fog");
				if (pEntity)
					pEntity->Use( pPlayer, pPlayer, USE_SET, 0 );
				else
				{
					MESSAGE_BEGIN(MSG_ONE, gmsgFog, NULL, pPlayer->pev);
						WRITE_COORD(0);
						WRITE_COORD(0);
						WRITE_BYTE(0);
						WRITE_BYTE(0);
						WRITE_BYTE(0);
						WRITE_COORD(0);
					MESSAGE_END();
				}
			}

			// Non observer
			if (!pl->IsObserver())
			{
				if (toggleFlashlight)
				{
					if ( toggleFlashlight == 2 && !pl->FlashlightIsOn() )
						pl->FlashlightTurnOn();
					else if (toggleFlashlight == 1 && pl->FlashlightIsOn())
						pl->FlashlightTurnOff();
				}
			}
		}
	}
}

BOOL CGameRules::WeaponMutators( CBasePlayerWeapon *pWeapon )
{
	if (pWeapon && pWeapon->m_pPlayer && pWeapon->m_pPlayer->pev->solid != SOLID_NOT)
	{
		if (m_iDontShoot)
		{
			if (pWeapon->pszAmmo1() != NULL || pWeapon->m_iId == WEAPON_ZAPGUN || pWeapon->m_iId == WEAPON_ROCKETCROWBAR)
			{
				if (!FBitSet(pWeapon->m_pPlayer->pev->flags, FL_GODMODE))
				{
					float damageGiven = pWeapon->pev->dmg;

					if (!damageGiven)
						damageGiven = RANDOM_FLOAT(20, 60);
					UTIL_ScreenFade(pWeapon->m_pPlayer, Vector(200, 0, 0), 0.5, 0.5, 32, FFADE_IN);
					pWeapon->m_pPlayer->TakeDamage( pWeapon->m_pPlayer->pev, pWeapon->m_pPlayer->pev, damageGiven, DMG_BLAST);

					ClientPrint(pWeapon->m_pPlayer->pev, HUD_PRINTCENTER, "Don't Shoot!!!\n(fists / kicks / slides only!)");
					pWeapon->m_flNextPrimaryAttack = pWeapon->m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
					// return FALSE; // nothing else.
				}
			}
		}

		if (pWeapon->m_flStartThrow == 0 && pWeapon->m_fireState == 0)
		{
			if (MutatorEnabled(MUTATOR_ROCKETS))
			{
				if (pWeapon->m_pPlayer->pev->fov == 0 && !pWeapon->m_bFired && RANDOM_LONG(0,10) == 2) {
					pWeapon->ThrowRocket(FALSE);
				}
			}
			
			if (MutatorEnabled(MUTATOR_GRENADES))
			{
				if (pWeapon->m_pPlayer->pev->fov == 0 && !pWeapon->m_bFired && RANDOM_LONG(0,10) == 2) {
					pWeapon->ThrowGrenade(FALSE);
				}
			}
			
			if (MutatorEnabled(MUTATOR_SNOWBALL))
			{
				if (pWeapon->m_pPlayer->pev->fov == 0 && !pWeapon->m_bFired && RANDOM_LONG(0,10) == 2) {
					pWeapon->ThrowSnowball(FALSE);
				}
			}

			if (MutatorEnabled(MUTATOR_PUSHY))
			{
				pWeapon->m_pPlayer->pev->flags &= ~FL_ONGROUND;
				UTIL_MakeVectors( pWeapon->m_pPlayer->pev->v_angle );
				pWeapon->m_pPlayer->pev->velocity = pWeapon->m_pPlayer->pev->velocity - gpGlobals->v_forward * RANDOM_FLOAT(50,100) * 3;
			}
		}
	}

	return TRUE;
}

void CGameRules::SpawnMutators(CBasePlayer *pPlayer)
{
	if (MutatorEnabled(MUTATOR_TOPSYTURVY))
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "topsy", "1");
	else
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "topsy", "0");

	if (MutatorEnabled(MUTATOR_MEGASPEED))
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "1");

	if (MutatorEnabled(MUTATOR_ICE))
		pPlayer->pev->friction = 0.3;

	if (MutatorEnabled(MUTATOR_LIGHTSOUT))
		pPlayer->FlashlightTurnOn();

	if (MutatorEnabled(MUTATOR_SANTAHAT))
		pPlayer->m_flNextSantaSound = gpGlobals->time + RANDOM_FLOAT(10,15);
	else
		pPlayer->m_flNextSantaSound = 0;

	if (randomweapon.value && g_pGameRules->MutatorAllowed("randomweapon"))
		pPlayer->GiveRandomWeapon(NULL);

	GiveMutators(pPlayer);

	if (MutatorEnabled(MUTATOR_INVISIBLE)) {
		pPlayer->MakeInvisible();
	}
	else
	{
		pPlayer->MakeVisible();
	}

	if (MutatorEnabled(MUTATOR_999)) {
		pPlayer->pev->max_health = 999;
		pPlayer->pev->health = 999;
		pPlayer->pev->armorvalue = 999;
	}

	if (MutatorEnabled(MUTATOR_JEEPATHON)) {
		if (!pPlayer->IsObserver())
		{
			pPlayer->pev->body = 0;
			pPlayer->SetBodygroup( 2, 1 );
		}
	}

	if (MutatorEnabled(MUTATOR_TOILET)) {
		if (!pPlayer->IsObserver())
		{
			pPlayer->pev->body = 0;
			if (RANDOM_LONG(0,1))
			{ // toilet
				pPlayer->SetBodygroup( 0, 2 );
				pPlayer->SetBodygroup( 1, 1 );
				pPlayer->SetBodygroup( 2, 2 );
			}
			else
			{ // camera
				pPlayer->SetBodygroup( 0, 1 );
				pPlayer->SetBodygroup( 1, 2 );
			}
		}
	}

	// For decap reset
	if (MutatorEnabled(MUTATOR_RICOCHET))
		pPlayer->pev->body = 0;

	if (MutatorEnabled(MUTATOR_NOCLIP))
		pPlayer->pev->movetype = MOVETYPE_NOCLIP;

	if (MutatorEnabled(MUTATOR_GODMODE))
		pPlayer->pev->flags |= FL_GODMODE;
}

void CGameRules::GiveMutators(CBasePlayer *pPlayer)
{
	if (!pPlayer->IsAlive())
		return;

	if (MutatorEnabled(MUTATOR_ROCKETCROWBAR)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_rocketcrowbar"))
		{
			pPlayer->GiveNamedItem("weapon_rocketcrowbar");
			pPlayer->SelectItem("weapon_rocketcrowbar");
		}
	}

	if (MutatorEnabled(MUTATOR_INSTAGIB)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_zapgun"))
		{
			pPlayer->GiveNamedItem("weapon_zapgun");
			pPlayer->SelectItem("weapon_zapgun");
		}
	}

	if (MutatorEnabled(MUTATOR_RAILGUNS)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_railgun"))
		{
			pPlayer->GiveNamedItem("weapon_dual_railgun");
			pPlayer->SelectItem("weapon_dual_railgun");
		}
		pPlayer->GiveAmmo(URANIUM_MAX_CARRY, "uranium", URANIUM_MAX_CARRY);
	}

	if (MutatorEnabled(MUTATOR_PLUMBER)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_wrench"))
		{
			pPlayer->GiveNamedItem("weapon_dual_wrench");
			pPlayer->SelectItem("weapon_dual_wrench");
		}
	}

	if (MutatorEnabled(MUTATOR_COWBOY)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_sawedoff"))
		{
			pPlayer->GiveNamedItem("weapon_dual_sawedoff");
			pPlayer->SelectItem("weapon_dual_sawedoff");
		}
	}

	if (MutatorEnabled(MUTATOR_BUSTERS)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_egon"))
		{
			pPlayer->GiveNamedItem("weapon_egon");
			pPlayer->SelectItem("weapon_egon");
		}
	}

	if (MutatorEnabled(MUTATOR_BARRELS)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_gravitygun"))
		{
			pPlayer->GiveNamedItem("weapon_gravitygun");
			pPlayer->SelectItem("weapon_gravitygun");
		}
	}

	if (MutatorEnabled(MUTATOR_PORTAL)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_ashpod"))
		{
			pPlayer->GiveNamedItem("weapon_ashpod");
			pPlayer->SelectItem("weapon_ashpod");
		}
	}

	if (MutatorEnabled(MUTATOR_BERSERKER)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_chainsaw"))
		{
			pPlayer->GiveNamedItem("weapon_chainsaw");
			pPlayer->SelectItem("weapon_chainsaw");
		}
	}

	if (MutatorEnabled(MUTATOR_VESTED)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_vest"))
		{
			pPlayer->GiveNamedItem("weapon_vest");
			pPlayer->SelectItem("weapon_vest");
		}
	}

	if (MutatorEnabled(MUTATOR_FIRESTARTER)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_flamethrower"))
		{
			pPlayer->GiveNamedItem("weapon_flamethrower");
			pPlayer->SelectItem("weapon_flamethrower");
		}
	}

	if (MutatorEnabled(MUTATOR_ROCKETBEES)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_hornetgun"))
		{
			pPlayer->GiveNamedItem("weapon_hornetgun");
			pPlayer->SelectItem("weapon_hornetgun");
		}
	}

	if (MutatorEnabled(MUTATOR_LONGJUMP)) {
		if (!pPlayer->m_fLongJump && (pPlayer->pev->weapons & (1<<WEAPON_SUIT)))
			pPlayer->GiveNamedItem("item_longjump");
	}

	if (MutatorEnabled(MUTATOR_GODMODE)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_vice"))
		{
			pPlayer->GiveNamedItem("weapon_vice");
			pPlayer->SelectItem("weapon_vice");
		}
	}

	if (MutatorEnabled(MUTATOR_DONTSHOOT)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_fingergun"))
		{
			pPlayer->GiveNamedItem("weapon_fingergun");
			pPlayer->SelectItem("weapon_fingergun");
		}
	}
}

const char *entityList[] =
{
	"weapon_crowbar",
	"weapon_knife"
	"weapon_wrench",
	"weapon_chainsaw",
	"weapon_rocketcrowbar",
	"weapon_gravitygun",
	"weapon_ashpod",
	"weapon_9mmhandgun",
	"weapon_357",
	"weapon_deagle",
	"weapon_mag60",
	"weapon_smg",
	"weapon_sawedoff",
	"weapon_dual_sawedoff",
	"weapon_shotgun",
	"weapon_9mmAR",
	"weapon_12gauage",
	"weapon_shotgun",
	"weapon_usas",
	"weapon_crossbow",
	"weapon_sniperrifle",
	"weapon_chaingun",
	"weapon_freezegun",
	"weapon_rpg",
	"weapon_gauss",
	"weapon_egon",
	"weapon_hornetgun",
	"weapon_railgun",
	"weapon_glauncher",
	"weapon_cannon",
	"weapon_nuke",
	"weapon_snowball",
	"weapon_grenade",
	"weapon_tripmine",
	"weapon_satchel",
	"weapon_snark",
	"weapon_chumtoad",
	"weapon_vest",
	"weapon_flamethrower",
	"weapon_dual_wrench",
	"weapon_dual_glock",
	"weapon_dual_deagle",
	"weapon_dual_mag60",
	"weapon_dual_smg",
	"weapon_dual_usas",
	"weapon_dual_chaingun",
	"weapon_dual_hornetgun",
	"weapon_dual_railgun",
	"weapon_dual_rpg",
	"weapon_dual_flamethrower",
	"ammo_rpgclip",
	"ammo_9mmAR",
	"ammo_ARgrenades",
	"ammo_357",
	"ammo_buckshot",
	"ammo_9mmclip",
	"ammo_gaussclip",
	"ammo_crossbow",
	"item_healthkit",
	"item_battery",
	"item_longjump",
	"weaponbox",
};

BOOL CGameRules::MutatorEnabled(int mutatorId)
{
	// check queue or something?
	mutators_t *t = m_Mutators;
	while (t != NULL) {
		if (t->mutatorId == mutatorId && (t->timeToLive > gpGlobals->time || t->timeToLive == -1))
		{
			// ALERT(at_aiconsole, ">>> YES mutatorId=%d , time=%.2f > global=%.2f \n", mutatorId, t->timeToLive, gpGlobals->time);
			return TRUE;
		}
		t = t->next;
	}

	return FALSE;
}

mutators_t *CGameRules::GetMutators()
{
	return m_Mutators;
}

void CGameRules::AddRandomMutator(const char *cvarName, BOOL withBar, BOOL three)
{
	int attempts = 0;
	int adjcount = fmin(fmax(mutatorcount.value, 0), 7);
	int mutatorTime = fmin(fmax(mutatortime.value, 10), 120);
	float choasIncrement = mutatorTime;

	while (attempts < adjcount)
	{
		int index = RANDOM_LONG(1,(int)ARRAYSIZE(g_szMutators) - 1);
		const char *tryIt = g_szMutators[index];

		// If gamerules disallows it
		if (!g_pGameRules->MutatorAllowed(tryIt))
		{
			attempts++;
			continue;
		}

		// If already loaded
		if (strlen(addmutator.string) > 2 && strstr(addmutator.string, tryIt))
		{
			attempts++;
			continue;
		}

		// If chaosfilter disallows it
		if (strlen(chaosfilter.string) > 2 && strstr(chaosfilter.string, tryIt))
		{
			// ALERT(at_console, "Ignoring \"%s\", found in sv_chaosfilter\n", tryIt);
			attempts++;
			continue;
		}

		cvar_t *cvarp = CVAR_GET_POINTER(cvarName);
		char mutatorToAdd[512];
		if (withBar || three)
			sprintf(mutatorToAdd, "%s;", g_szMutators[index]);
		else
			sprintf(mutatorToAdd, "%s253;", g_szMutators[index]);
		if (strlen(cvarp->string))
		{
			strcat(mutatorToAdd, cvarp->string);
		}
		CVAR_SET_STRING(cvarp->name, mutatorToAdd);

		if (withBar)
		{
			MESSAGE_BEGIN(MSG_ALL, gmsgChaos);
				WRITE_BYTE(choasIncrement);
			MESSAGE_END();
		}

		break;
	}
}

extern int gmsgPlayClientSound;

void CGameRules::AddInstantMutator(void)
{
	int max_instant_mutators = 9;
	int random = RANDOM_LONG(0, max_instant_mutators);
	switch (random)
	{
		case 0:
			UTIL_ClientPrintAll(HUD_PRINTTALK, "[Mutators] Nothing selected!\n");
			break;
		case 1:
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				CBasePlayer *pl = (CBasePlayer *)pPlayer;
				if (pPlayer && pPlayer->IsPlayer() && !pl->IsSpectator() && pl->IsAlive() && !pl->HasDisconnected)
					pPlayer->pev->health += 1;
			}
			UTIL_ClientPrintAll(HUD_PRINTTALK, "[Mutators] +1 health!\n");
			break;
		case 2:
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				CBasePlayer *pl = (CBasePlayer *)pPlayer;
				if (pPlayer && pPlayer->IsPlayer() && !pl->IsSpectator() && pl->IsAlive() && !pl->HasDisconnected)
				{
					if (pPlayer->pev->armorvalue <= 0)
					{
						pPlayer->pev->health = 0;
						pl->Killed(VARS(eoNullEntity), VARS(eoNullEntity), GIB_ALWAYS);
					}
					else
					{
						int temp = pPlayer->pev->health;
						pPlayer->pev->health = pPlayer->pev->armorvalue;
						pPlayer->pev->armorvalue = temp;
					}
				}
			}
			UTIL_ClientPrintAll(HUD_PRINTTALK, "[Mutators] Swap health and armor!\n");
			break;
		case 3:
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				CBasePlayer *pl = (CBasePlayer *)pPlayer;
				if (pPlayer && pPlayer->IsPlayer() && !pl->IsSpectator() && pl->IsAlive() && !pl->HasDisconnected)
					pPlayer->pev->health += 100;
			}
			UTIL_ClientPrintAll(HUD_PRINTTALK, "[Mutators] +100 health!\n");
			break;
		case 4:
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				CBasePlayer *pl = (CBasePlayer *)pPlayer;
				if (pPlayer && pPlayer->IsPlayer() && !pl->IsSpectator() && pl->IsAlive() && !pl->HasDisconnected)
					pPlayer->pev->armorvalue += 100;
			}
			UTIL_ClientPrintAll(HUD_PRINTTALK, "[Mutators] +100 armor!\n");
			break;
		case 5:
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				CBasePlayer *pl = (CBasePlayer *)pPlayer;
				if (pPlayer && pPlayer->IsPlayer() && !pl->IsSpectator() && pl->IsAlive() && !pl->HasDisconnected)
					pPlayer->pev->health = pPlayer->pev->health = 1;
			}
			UTIL_ClientPrintAll(HUD_PRINTTALK, "[Mutators] 1 health!\n");
			break;
		case 6:
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				CBasePlayer *pl = (CBasePlayer *)pPlayer;
				if (pPlayer && pPlayer->IsPlayer() && !pl->IsSpectator() && pl->IsAlive() && !pl->HasDisconnected)
				{
					pPlayer->pev->health = 69;
					pPlayer->pev->armorvalue = 69;
				}
			}
			UTIL_ClientPrintAll(HUD_PRINTTALK, "[Mutators] Nice!\n");
			break;
		case 7:
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				CBasePlayer *pl = (CBasePlayer *)pPlayer;
				if (pPlayer && pPlayer->IsPlayer() && !pl->IsSpectator() && pl->IsAlive() && !pl->HasDisconnected)
				{
					pPlayer->pev->health = 67;
					pPlayer->pev->armorvalue = 67;
				}
			}
			UTIL_ClientPrintAll(HUD_PRINTTALK, "[Mutators] Six, seeeeven!\n");
			break;
		case 8:
			for (int i = 1; i <= gpGlobals->maxClients; ++i)
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				CBasePlayer *pl = (CBasePlayer *)pPlayer;
				if (pPlayer && pPlayer->IsPlayer() && !pl->IsSpectator() && pl->IsAlive() && !pl->HasDisconnected)
				{
					// Spawn slightly above feet to avoid clipping
					UTIL_MakeAimVectors(pPlayer->pev->angles);
					Vector org = pPlayer->pev->origin + Vector(0, 0, 16) + gpGlobals->v_forward * 64;

					// Tiny toss so it doesn’t roll far; a little jitter avoids stacking
					Vector vel(
						RANDOM_FLOAT(-20.0f, 20.0f),
						RANDOM_FLOAT(-20.0f, 20.0f),
						RANDOM_FLOAT(5.0f, 25.0f)
					);
					CGrenade::ShootTimed(pPlayer->pev, org, vel, RANDOM_FLOAT(2.0f, 4.0f));
				}
			}
			UTIL_ClientPrintAll(HUD_PRINTTALK, "[Mutators] Hot potato!\n");
			break;
		case 9:
			for (int i = 1; i <= gpGlobals->maxClients; ++i)
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				CBasePlayer *pl = (CBasePlayer *)pPlayer;
				if (pPlayer && pPlayer->IsPlayer() && !pl->IsSpectator() && pl->IsAlive() && !pl->HasDisconnected)
					pPlayer->TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), RANDOM_FLOAT(10.0f, 20.0f), DMG_SLASH);
			}
			UTIL_ClientPrintAll(HUD_PRINTTALK, "[Mutators] Random damage!\n");
			break;
	}

	// Instant mutators do not have client side integration.
	MESSAGE_BEGIN( MSG_BROADCAST, gmsgPlayClientSound );
		WRITE_BYTE(CLIENT_SOUND_CHICKEN);
	MESSAGE_END();

	// Continue chaos timer bar
	int mutatorTime = fmin(fmax(mutatortime.value, 10), 120);
	float choasIncrement = mutatorTime;
	MESSAGE_BEGIN(MSG_ALL, gmsgChaos);
		WRITE_BYTE(choasIncrement);
	MESSAGE_END();
}

void CGameRules::MutatorsThink(void)
{
	if (m_flAddMutatorTime < gpGlobals->time)
	{
		// storage from votes
		if (strlen(mutatorlist.string))
		{
			char *mutator;
			char list[512];
			char second[512] = {""};
			strcpy(list, mutatorlist.string);
			mutator = list;
			mutator = strtok( mutator, ";" );
			BOOL first = FALSE;
			while ( mutator != NULL && *mutator )
			{
				if (!first)
				{
					CVAR_SET_STRING("sv_addmutator", mutator);
					first = TRUE;
				}
				else
				{
					sprintf(second, "%s;%s;", second, mutator);
				}
				mutator = strtok( NULL, ";" );
			}
			CVAR_SET_STRING("sv_mutatorlist", second);
		}

		// Check new
		int mutatorTime = fmin(fmax(mutatortime.value, 10), 120);
		float choasIncrement = mutatorTime;

		if (strlen(addmutator.string))
		{
			if (g_pGameRules->MutatorAllowed(addmutator.string))
			{
				if (strstr(addmutator.string, "chaos") || !strcmp(addmutator.string, "1"))
				{
					m_flChaosMutatorTime = gpGlobals->time + choasIncrement;
					MESSAGE_BEGIN(MSG_ALL, gmsgChaos);
						WRITE_BYTE(choasIncrement);
					MESSAGE_END();
					ALERT(at_console, "Mutator chaos enabled.\n");
				}
				else if (!strcmp(addmutator.string, "unchaos"))
				{
					m_flChaosMutatorTime = 0;
					ALERT(at_console, "Mutator chaos disabled.\n");
				}
				else if (!strcmp(addmutator.string, "clear"))
				{
					MESSAGE_BEGIN(MSG_ALL, gmsgAddMutator);
						WRITE_BYTE(254);
						WRITE_BYTE(mutatorTime);
					MESSAGE_END();

					m_flChaosMutatorTime = 0;

					mutators_t *t = m_Mutators;
					while (t != NULL)
					{
						t->timeToLive = gpGlobals->time;
						t = t->next;
					}
				}
				else
				{
					for (int i = 0; i < MAX_MUTATORS; i++)
					{
						if (strstr(addmutator.string, g_szMutators[i]) || addmutator.value == (i + 1))
						{
							// Special pass
							if (strstr(addmutator.string, "three"))
							{
								BOOL bar=FALSE, three=TRUE;
								AddRandomMutator("sv_mutatorlist", bar, three);
								AddRandomMutator("sv_mutatorlist", bar, three);
								AddRandomMutator("sv_mutatorlist", bar, three);
								continue;
							}

							// make sure not already in list.
							mutators_t *t = m_Mutators;
							BOOL add = TRUE;
							while (t != NULL)
							{
								if (t->mutatorId == i)
								{
									add = FALSE;
								}
								t = t->next;
							}

							if (add)
							{
								// Forever
								if (strstr(addmutator.string, "253"))
									mutatorTime = 253;

								MESSAGE_BEGIN(MSG_ALL, gmsgAddMutator);
									WRITE_BYTE(i + 1);
									WRITE_BYTE(mutatorTime);
								MESSAGE_END();

								mutators_t *mutator = new mutators_t();
								mutator->mutatorId = i + 1;
								mutator->timeToLive = mutatorTime == 253 ? -1 : gpGlobals->time + mutatorTime;
								mutator->next = m_Mutators ? m_Mutators : NULL;
								m_Mutators = mutator;

								ALERT(at_console, "Mutator \"%s\" enabled at %.2f until %.2f\n", g_szMutators[i], gpGlobals->time, mutator->timeToLive);

								m_flDetectedMutatorChange = gpGlobals->time + 1.0;
							}

							break;
						}
					}
				}
			}
			else
			{
				ALERT(at_console, "Mutator \"%s\" has been filtered in this mode.\n", addmutator.string);
			}

			CVAR_SET_STRING("sv_addmutator", "");
		}

		// remove old
		int count = 0;
		mutators_t *m = m_Mutators;
		mutators_t *prev = NULL;
		char szMutators[128] = "";
		while (m != NULL)
		{
			// ALERT(at_aiconsole, ">>> [%.2f] found m->mutatorId=%d [%.2f]\n", gpGlobals->time, m->mutatorId, m->timeToLive);

			if (m->timeToLive <= gpGlobals->time && m->timeToLive != -1)
			{
				ALERT(at_console, "Mutator \"%s\" disabled at %.2f\n", g_szMutators[m->mutatorId-1], gpGlobals->time);
				// ALERT(at_aiconsole, ">>> [%.2f] delete m->mutatorId=%d\n", gpGlobals->time, m->mutatorId);

				m_flDetectedMutatorChange = gpGlobals->time + 1.0;

				if (prev)
				{
					mutators_t *temp;
					temp = prev->next;
					prev->next = temp->next;
					//delete temp;
				}
				else
				{
					m_Mutators = NULL;
					break;
				}
			}
			else
			{
				char buffer[16];
				sprintf(buffer, "%d;", m->mutatorId);
				strcat(szMutators, buffer);
				count++;
			}

			prev = m;
			m = m->next;
		}

		// ALERT(at_aiconsole, "szMutators is \"%s\"\n", szMutators );
		gpGlobals->startspot = ALLOC_STRING(szMutators);
		// ALERT(at_aiconsole, "STRING(gpGlobals->startspot) is \"%s\"\n", STRING(gpGlobals->startspot) );

		// chaos mode
		int adjcount = fmin(fmax(mutatorcount.value, 0), 7);
		if (m_flChaosMutatorTime && m_flChaosMutatorTime < gpGlobals->time && count < adjcount)
		{
			m_flChaosMutatorTime = gpGlobals->time + choasIncrement;

			if (!RANDOM_LONG(0, instantmutators.value))
				AddRandomMutator("sv_addmutator", TRUE);
			else
				AddInstantMutator();
		}

		m_flAddMutatorTime = gpGlobals->time + 1.0;
	}

	// Apply mutators
	if (m_flDetectedMutatorChange && m_flDetectedMutatorChange < gpGlobals->time)
	{
		RefreshSkillData();

		EnvMutators();

		if (m_JopeCheck) {
			UTIL_ClientPrintAll(HUD_PRINTCENTER, "The JOPE is over with!\n");
			m_JopeCheck = FALSE;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				CBasePlayer *pl = (CBasePlayer *)pPlayer;
				if (pPlayer && pPlayer->IsPlayer())
				{
					char *name = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer( pl->edict() ), "j");
					if (name && strlen(name))
					{
						g_engfuncs.pfnSetClientKeyValue(pl->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pl->edict() ), "name", name);
						g_engfuncs.pfnSetClientKeyValue(pl->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pl->edict() ), "j", "");
					}
				}
			}
		}
		else
		{
			if (MutatorEnabled(MUTATOR_JOPE)) {
				m_JopeCheck = TRUE;
				UTIL_ClientPrintAll(HUD_PRINTCENTER, "You've been JOPED!\n");
			}
		}

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
			CBasePlayer *pl = (CBasePlayer *)pPlayer;

			if (pPlayer && pPlayer->IsPlayer())
			{
				if (m_JopeCheck)
				{
					char name[64];
					strcpy(name, STRING(pl->pev->netname));
					g_engfuncs.pfnSetClientKeyValue(pl->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pl->edict() ), "j", name);
					g_engfuncs.pfnSetClientKeyValue(pl->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( pl->edict() ), "name", "Jope");
				}

				if (MutatorEnabled(MUTATOR_CREDITS))
				{
					pl->m_iCreditMode = 1;
					pl->m_fCreditsTime = gpGlobals->time;
				}
				else
				{
					pl->m_fCreditsTime = 0;
				}
			}

			if (pPlayer && pPlayer->IsPlayer() && !pl->IsObserver())
			{
				pl->m_iShowMutatorMessage = gpGlobals->time + 2.0;

				if (MutatorEnabled(MUTATOR_TOPSYTURVY)) {
					g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "topsy", "1");
				} else {
					g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "topsy", "0");
				}

				if (MutatorEnabled(MUTATOR_ICE))
					pPlayer->pev->friction = 0.3;
				else if (pPlayer->pev->friction > 0.2 && pPlayer->pev->friction < 0.4)
					pPlayer->pev->friction = 1.0;

				if (MutatorEnabled(MUTATOR_MEGASPEED))
					g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "1");
				else if (((CBasePlayer *)pPlayer)->m_fHasRune != RUNE_HASTE &&
						 !((CBasePlayer *)pPlayer)->IsArmoredMan &&
						 ((CBasePlayer *)pPlayer)->pev->fuser4 != 1)
					g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "0");

				if (!MutatorEnabled(MUTATOR_AUTOAIM)) {
					pl->ResetAutoaim();
				}

				if (MutatorEnabled(MUTATOR_SANTAHAT))
					pl->m_flNextSantaSound = gpGlobals->time + RANDOM_FLOAT(10,15);
				else
					pl->m_flNextSantaSound = 0;

				GiveMutators(pl);

				if (MutatorEnabled(MUTATOR_INVISIBLE))
				{
					pl->MakeInvisible();
				}
				else
				{
					pl->MakeVisible();
				}

				if (MutatorEnabled(MUTATOR_999)) {
					pl->pev->max_health = 999;
					pl->pev->health = 999;
					pl->pev->armorvalue = 999;
				}
				else
				{
					pl->pev->max_health = 100;
					if (pl->pev->health > 100)
						pl->pev->health = 100;
					if (pl->pev->armorvalue > 100)
						pl->pev->armorvalue = 100;
				}

				if (MutatorEnabled(MUTATOR_JEEPATHON)) {
					if (!pl->IsObserver())
					{
						pl->pev->body = 0;
						pl->SetBodygroup( 2, 1 );
					}
				}

				if (MutatorEnabled(MUTATOR_TOILET)) {
					if (!pl->IsObserver())
					{
						pl->pev->body = 0;
						if (RANDOM_LONG(0,1))
						{ // toilet
							pl->SetBodygroup( 0, 2 );
							pl->SetBodygroup( 1, 1 );
							pl->SetBodygroup( 2, 2 );
						}
						else
						{ // camera
							pl->SetBodygroup( 0, 1 );
							pl->SetBodygroup( 1, 2 );
						}
					}
				}

				if (!MutatorEnabled(MUTATOR_JEEPATHON) && !MutatorEnabled(MUTATOR_TOILET))
				{
					pl->pev->body = 0;
				}

				if (MutatorEnabled(MUTATOR_NOCLIP))
				{
					pl->pev->movetype = MOVETYPE_NOCLIP;
				}
				else
				{
					if (pl->pev->movetype == MOVETYPE_NOCLIP)
					{
						edict_t* pentSpawnSpot = EntSelectSpawnPoint( pl );
						if (pentSpawnSpot)
						{
							UTIL_SetOrigin(pl->pev, VARS(pentSpawnSpot)->origin + Vector(0,0,1));
							pl->pev->angles = VARS(pentSpawnSpot)->angles;
							CBaseEntity *ent = NULL;
							while ( (ent = UTIL_FindEntityInSphere( ent, pl->pev->origin, 64 )) != NULL )
							{
								// if ent is a client, kill em (unless they are ourselves)
								if ( ent->IsPlayer() && ent->IsAlive() && ent->edict() != pl->edict() && !ent->pev->iuser1 )
								{
									ClearMultiDamage();
									ent->pev->health = 0; // without this, player can walk as a ghost.
									((CBasePlayer *)ent)->Killed(pl->pev, VARS(INDEXENT(0)), GIB_ALWAYS);
								}
							}
						}
						else
						{
							TraceResult trace;
							UTIL_TraceHull(pl->pev->origin, pl->pev->origin, dont_ignore_monsters, head_hull, pl->edict(), &trace);
							if (trace.fStartSolid)
							{
								if (pl->IsPlayer() && pl->IsAlive()) {
									ClearMultiDamage();
									pl->pev->health = 0;
									pl->Killed( pl->pev, GIB_NEVER );
								}
							}
						}
						pl->pev->movetype = MOVETYPE_WALK;
					}
				}

				if (MutatorEnabled(MUTATOR_GODMODE))
				{
					pl->pev->flags |= FL_GODMODE;
				}
				else
				{
					// If a player is snowman, they have FL_NOTARGET set, so keep them in godmode.
					if ( FBitSet(pl->pev->flags, FL_GODMODE) && !FBitSet(pl->pev->flags, FL_NOTARGET) )
						pl->pev->flags &= ~FL_GODMODE;
				}

				if (!FBitSet(pl->pev->flags, FL_FAKECLIENT))
				{
					if (MutatorEnabled(MUTATOR_HALFLIFE))
					{
						if (!FBitSet(pl->m_iHideHUD, HIDEHUD_ICE))
						{
							pl->m_iHideHUD |= HIDEHUD_ICE;
							MESSAGE_BEGIN( MSG_ONE, gmsgHideWeapon, NULL, pl->pev );
								WRITE_BYTE( pl->m_iHideHUD );
							MESSAGE_END();
						}
					}
					else
					{
						if (FBitSet(pl->m_iHideHUD, HIDEHUD_ICE))
						{
							pl->m_iHideHUD &= ~HIDEHUD_ICE;
							MESSAGE_BEGIN( MSG_ONE, gmsgHideWeapon, NULL, pl->pev );
								WRITE_BYTE( pl->m_iHideHUD );
							MESSAGE_END();
						}
					}
				}
			}

			if (MutatorEnabled(MUTATOR_INFINITEAMMO) && infiniteammo.value != 1)
				CVAR_SET_FLOAT("sv_infiniteammo", 2);
			else
			{
				if (!MutatorEnabled(MUTATOR_INFINITEAMMO) && infiniteammo.value == 2)
					CVAR_SET_FLOAT("sv_infiniteammo", 0);
			}

			if (MutatorEnabled(MUTATOR_RANDOMWEAPON) && randomweapon.value != 1)
				CVAR_SET_FLOAT("mp_randomweapon", 2);
			else
			{
				if (!MutatorEnabled(MUTATOR_RANDOMWEAPON) && randomweapon.value == 2)
					CVAR_SET_FLOAT("mp_randomweapon", 0);
			}

			if (MutatorEnabled(MUTATOR_SPEEDUP) && CVAR_GET_FLOAT("sys_timescale") != 1.49f)
				CVAR_SET_FLOAT("sys_timescale", 1.49);
			else
			{
				if (!MutatorEnabled(MUTATOR_SPEEDUP) &&
					CVAR_GET_FLOAT("sys_timescale") > 1.48f && CVAR_GET_FLOAT("sys_timescale") < 1.50f)
					CVAR_SET_FLOAT("sys_timescale", 1.0);
			}

			if (MutatorEnabled(MUTATOR_SLOWBULLETS) && CVAR_GET_FLOAT("sv_slowbullets") != 2)
			{
				CVAR_SET_FLOAT("sv_slowbullets", 2);
			}
			else
			{
				if (!MutatorEnabled(MUTATOR_SLOWBULLETS) && CVAR_GET_FLOAT("sv_slowbullets") == 2)
					CVAR_SET_FLOAT("sv_slowbullets", 0);
			}

			if (MutatorEnabled(MUTATOR_EXPLOSIVEAI) && g_ExplosiveAI != 1)
			{
				g_ExplosiveAI = 1;
			}
			else
			{
				if (!MutatorEnabled(MUTATOR_EXPLOSIVEAI) && g_ExplosiveAI == 1)
					g_ExplosiveAI = 0;
			}

			if (MutatorEnabled(MUTATOR_ITEMSEXPLODE) && g_ItemsExplode != 1)
			{
				g_ItemsExplode = 1;

				CBaseEntity *pEntity = NULL;
				for (int itemIndex = 0; itemIndex < ARRAYSIZE(entityList); itemIndex++)
				{
					while ((pEntity = UTIL_FindEntityByClassname(pEntity, entityList[itemIndex])) != NULL)
					{
						// ALERT(at_aiconsole, "setting damage for %s at [x=%.2f,y=%.2f,z=%.2f]\n", entityList[itemIndex], pEntity->pev->origin.x, pEntity->pev->origin.y, pEntity->pev->origin.z);
						pEntity->pev->takedamage = DAMAGE_YES;
						pEntity->pev->health = 1;
					}
				}
			}
			else
			{
				if (!MutatorEnabled(MUTATOR_ITEMSEXPLODE) && g_ItemsExplode == 1)
				{
					g_ItemsExplode = 0;

					CBaseEntity *pEntity = NULL;
					for (int itemIndex = 0; itemIndex < ARRAYSIZE(entityList); itemIndex++)
					{
						while ((pEntity = UTIL_FindEntityByClassname(pEntity, entityList[itemIndex])) != NULL)
						{
							// ALERT(at_aiconsole, "setting damage for %s at [x=%.2f,y=%.2f,z=%.2f]\n", entityList[itemIndex], pEntity->pev->origin.x, pEntity->pev->origin.y, pEntity->pev->origin.z);
							pEntity->pev->takedamage = DAMAGE_NO;
							pEntity->pev->health = 0;
						}
					}
				}
			}

			if (MutatorEnabled(MUTATOR_NOTTHEBEES) && m_iNotTheBees != 1)
			{
				m_iNotTheBees = 1;
			}
			else
			{
				if (!MutatorEnabled(MUTATOR_NOTTHEBEES) && m_iNotTheBees == 1)
					m_iNotTheBees = 0;
			}

			if (MutatorEnabled(MUTATOR_DONTSHOOT) && !m_iDontShoot)
			{
				m_iDontShoot = TRUE;
			}
			else
			{
				if (!MutatorEnabled(MUTATOR_DONTSHOOT) && m_iDontShoot)
					m_iDontShoot = FALSE;
			}

			if (MutatorEnabled(MUTATOR_VOLATILE) && !m_iVolatile)
			{
				m_iVolatile = TRUE;
			}
			else
			{
				if (!MutatorEnabled(MUTATOR_VOLATILE) && m_iVolatile)
					m_iVolatile = FALSE;
			}
		}

		m_flDetectedMutatorChange = 0;
	}
}

void CGameRules::CheckGameMode( void )
{
	// Blank at map start
	if (!strlen(m_flCheckGameMode) || !strcmp(gamemode.string, "0"))
		strcpy(m_flCheckGameMode, gamemode.string);

	if (strcmp(m_flCheckGameMode, gamemode.string) != 0)
	{
		//m_flDetectedGameModeChange = gpGlobals->time + 5.0;
		strcpy(m_flCheckGameMode, gamemode.string);
		//UTIL_ClientPrintAll(HUD_PRINTTALK, "Game mode has changed to \"%s\". Ending current game in 5 seconds.\n", m_flCheckGameMode);
		SERVER_COMMAND( "restart\n" ); // Force a restart, so we can load the correct gamerules
		return;
	}

	// Hack for direct server start from GUI
	if (gamemode.value > 0 && g_GameMode != gamemode.value)
	{
		g_GameMode = gamemode.value;
		SERVER_COMMAND( "restart\n" );
		return;
	}

	// UNDONE: Hotswap gamemode?
	//if (m_flDetectedGameModeChange && m_flDetectedGameModeChange < gpGlobals->time)
	{
		// changelevel.
		// g_fGameOver = TRUE;

		/*
		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
			CBasePlayer *pl = (CBasePlayer *)pPlayer;
			if (pPlayer && pPlayer->IsPlayer() && !pl->IsObserver())
			{
				pl->m_iShowGameModeMessage = gpGlobals->time + 1.0;
			}
		}

		g_pGameRules->ResetGameMode();
		CBaseEntity *pWorld = CBaseEntity::Instance(NULL);
		if (pWorld)
			((CWorld *)pWorld)->SetGameMode();
		if (g_pGameRules)
			delete g_pGameRules;
		g_pGameRules = InstallGameRules();
		*/

		//m_flDetectedGameModeChange = 0;
	}
}

void CGameRules::UpdateMutatorMessage( CBasePlayer *pPlayer )
{
	if (pPlayer->m_iShowMutatorMessage != -1 && pPlayer->m_iShowMutatorMessage < gpGlobals->time) {
		// Display Mutators
		/*
		if (strlen(mutators.string) > 2 && strlen(mutators.string) < 64)
			pPlayer->DisplayHudMessage(UTIL_VarArgs("Mutators Active: %s", mutators.string),
			TXT_CHANNEL_MUTATOR_TITLE, .02, .18, 210, 210, 210, 2, .015, 2, 5, .25);
		*/

		pPlayer->m_iShowMutatorMessage = -1;
	}
}

void CGameRules::UpdateGameModeMessage( CBasePlayer *pPlayer )
{
	if (pPlayer->m_iShowGameModeMessage != -1 && pPlayer->m_iShowGameModeMessage < gpGlobals->time) {
		//if (strlen(gamemode.string) > 1 && strcmp(gamemode.string, "ffa"))
		//	pPlayer->DisplayHudMessage(UTIL_VarArgs("Game Mode is %s", gamemode.string), TXT_CHANNEL_GAME_TITLE, .02, .14, 210, 210, 210, 2, .015, 2, 5, .25);
		pPlayer->m_iShowGameModeMessage = -1;
	}
}

BOOL CGameRules::IsCtC()
{
	return g_GameMode == GAME_CTC;
}

BOOL CGameRules::IsChilldemic()
{
	return g_GameMode == GAME_CHILLDEMIC;
}

BOOL CGameRules::IsJVS()
{
	return g_GameMode == GAME_ICEMAN;
}

BOOL CGameRules::IsShidden()
{
	return g_GameMode == GAME_SHIDDEN;
}

BOOL CGameRules::IsInstagib()
{
	return g_GameMode == GAME_INSTAGIB;
}

BOOL CGameRules::IsPropHunt()
{
	return g_GameMode == GAME_PROPHUNT;
}

BOOL CGameRules::IsBusters()
{
	return g_GameMode == GAME_BUSTERS;
}

BOOL CGameRules::IsGunGame()
{
	return g_GameMode == GAME_GUNGAME;
}

BOOL CGameRules::IsHorde()
{
	return g_GameMode == GAME_HORDE;
}
