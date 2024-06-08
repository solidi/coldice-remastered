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

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );

DLL_GLOBAL CGameRules*	g_pGameRules = NULL;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgMOTD;
extern int gmsgShowGameTitle;
extern int gmsgAddMutator;
extern int gmsgFog;
extern int gmsgChaos;

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
	"chumxplode",
	"coolflesh",
	"crate",
	"credits",
	"dealter",
	"dontshoot",
	"explosiveai",
	"fastweapons",
	"fog",
	"goldenguns",
	"grenades",
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
	"mirror",
	"noclip",
	"noreload",
	"notify",
	"notthebees",
	"oldtime",
	"paintball",
	"piratehat",
	"plumber",
	"portal",
	"pumpkin",
	"pushy",
	"randomweapon",
	"ricochet",
	"rockets",
	"rocketcrowbar",
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
			case GAME_LMS:
				return new CHalfLifeLastManStanding;
			case GAME_CHILLDEMIC:
				return new CHalfLifeChilldemic;
			case GAME_COLDSKULL:
				return new CHalfLifeColdSkull;
			case GAME_CTC:
				return new CHalfLifeCaptureTheChumtoad;
			case GAME_CTF:
				return new CHalfLifeCaptureTheFlag;
			case GAME_GUNGAME:
				return new CHalfLifeGunGame;
			case GAME_ICEMAN:
				return new CHalfLifeJesusVsSanta;
			case GAME_SHIDDEN:
				return new CHalfLifeShidden;
			case GAME_HORDE:
				return new CHalfLifeHorde;
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

	if (strcmp(szSkyColor[0], "0") != 0 && strlen(szSkyColor[0]))
	{
		CVAR_SET_STRING("sv_skycolor_r", szSkyColor[0]);
		CVAR_SET_STRING("sv_skycolor_g", szSkyColor[1]);
		CVAR_SET_STRING("sv_skycolor_b", szSkyColor[2]);
	}
	else
	{
		strcpy(szSkyColor[0], CVAR_GET_STRING("sv_skycolor_r"));
		strcpy(szSkyColor[1], CVAR_GET_STRING("sv_skycolor_g"));
		strcpy(szSkyColor[2], CVAR_GET_STRING("sv_skycolor_b"));
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
		CVAR_SET_STRING("sv_skycolor_r", "0");
		CVAR_SET_STRING("sv_skycolor_g", "0");
		CVAR_SET_STRING("sv_skycolor_b", "0");
	}
	else
	{
		LIGHT_STYLE(0, "m");
		if (flashlight.value == 2)
		{
			CVAR_SET_STRING("mp_flashlight", "0");
			toggleFlashlight = 1;
		}
		CVAR_SET_STRING("sv_skycolor_r", szSkyColor[0]);
		CVAR_SET_STRING("sv_skycolor_g", szSkyColor[1]);
		CVAR_SET_STRING("sv_skycolor_b", szSkyColor[2]);
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

		if (pPlayer && pPlayer->IsPlayer())
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
	if (pWeapon && pWeapon->m_pPlayer)
	{
		if (m_iDontShoot)
		{
			if (pWeapon->pszAmmo1() != NULL && pWeapon->m_pPlayer->pev->solid != SOLID_NOT)
			{
				if (!FBitSet(pWeapon->m_pPlayer->pev->flags, FL_GODMODE))
				{
					ClearMultiDamage();
					pWeapon->m_pPlayer->pev->health = 0; // without this, player can walk as a ghost.
					pWeapon->m_pPlayer->Killed(pWeapon->m_pPlayer->pev, pWeapon->m_pPlayer->pev, GIB_ALWAYS);
				}
				CGrenade::Vest( pWeapon->m_pPlayer->pev, pWeapon->m_pPlayer->pev->origin );
				ClientPrint(pWeapon->m_pPlayer->pev, HUD_PRINTCENTER, "Don't Shoot!!!\n(fists / kicks / slides only!)");
				pWeapon->m_flNextPrimaryAttack = pWeapon->m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
				return FALSE; // nothing else.
			}
		}

		if (MutatorEnabled(MUTATOR_ROCKETS))
		{
			if (!pWeapon->m_bFired && RANDOM_LONG(0,10) == 2) {
				pWeapon->ThrowRocket(FALSE);
			}
		}
		
		if (MutatorEnabled(MUTATOR_GRENADES))
		{
			if (!pWeapon->m_bFired && RANDOM_LONG(0,10) == 2) {
				pWeapon->ThrowGrenade(FALSE);
			}
		}
		
		if (MutatorEnabled(MUTATOR_SNOWBALL))
		{
			if (!pWeapon->m_bFired && RANDOM_LONG(0,10) == 2) {
				pWeapon->ThrowSnowball(FALSE);
			}
		}

		if (MutatorEnabled(MUTATOR_PUSHY))
		{
			UTIL_MakeVectors( pWeapon->m_pPlayer->pev->v_angle );
			pWeapon->m_pPlayer->pev->velocity = pWeapon->m_pPlayer->pev->velocity - gpGlobals->v_forward * RANDOM_FLOAT(50,100) * 5;
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

	GiveMutators(pPlayer);

	if (MutatorEnabled(MUTATOR_INVISIBLE)) {
		pPlayer->MakeInvisible();
	}
	else
	{
		pPlayer->MakeVisible();
	}

	if (randomweapon.value)
		pPlayer->GiveRandomWeapon(NULL);

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
			//pPlayer->m_EFlags &= ~EFLAG_CANCEL;
			//pPlayer->m_EFlags |= EFLAG_JEEP;
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
}

void CGameRules::GiveMutators(CBasePlayer *pPlayer)
{
	if (!pPlayer->IsAlive())
		return;

	if (MutatorEnabled(MUTATOR_ROCKETCROWBAR)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_rocketcrowbar"))
			pPlayer->GiveNamedItem("weapon_rocketcrowbar");
	}

	if (MutatorEnabled(MUTATOR_INSTAGIB)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_railgun"))
			pPlayer->GiveNamedItem("weapon_dual_railgun");
		pPlayer->GiveAmmo(URANIUM_MAX_CARRY, "uranium", URANIUM_MAX_CARRY);
	}

	if (MutatorEnabled(MUTATOR_PLUMBER)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_wrench"))
			pPlayer->GiveNamedItem("weapon_dual_wrench");
	}

	if (MutatorEnabled(MUTATOR_BARRELS)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_gravitygun"))
			pPlayer->GiveNamedItem("weapon_gravitygun");
	}

	if (MutatorEnabled(MUTATOR_PORTAL)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_ashpod"))
			pPlayer->GiveNamedItem("weapon_ashpod");
	}

	if (MutatorEnabled(MUTATOR_LONGJUMP)) {
		if (!pPlayer->m_fLongJump && (pPlayer->pev->weapons & (1<<WEAPON_SUIT)))
			pPlayer->GiveNamedItem("item_longjump");
	}

	if (MutatorEnabled(MUTATOR_BERSERKER)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_chainsaw"))
			pPlayer->GiveNamedItem("weapon_chainsaw");
	}

	if (MutatorEnabled(MUTATOR_VESTED)) {
		if (!pPlayer->HasNamedPlayerItem("weapon_vest"))
			pPlayer->GiveNamedItem("weapon_vest");
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
		if (t->mutatorId == mutatorId && t->timeToLive > gpGlobals->time)
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

BOOL CGameRules::MutatorAllowed(const char *mutator)
{
	// No invisible in shidden
	if (strstr(mutator, g_szMutators[MUTATOR_INVISIBLE - 1]) || atoi(mutator) == MUTATOR_INVISIBLE)
		return !(g_GameMode == GAME_SHIDDEN);

	if (strstr(mutator, g_szMutators[MUTATOR_DEALTER - 1]) || atoi(mutator) == MUTATOR_DEALTER)
		return !(g_GameMode == GAME_SHIDDEN);

	if (strstr(mutator, g_szMutators[MUTATOR_MAXPACK - 1]) || atoi(mutator) == MUTATOR_MAXPACK)
		return !(g_GameMode == GAME_SNOWBALL);

	if (strstr(mutator, g_szMutators[MUTATOR_CHUMXPLODE - 1]) || atoi(mutator) == MUTATOR_CHUMXPLODE)
		return !(g_GameMode == GAME_CTC);
	
	return TRUE;
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
		float choasIncrement = RANDOM_LONG(10,30);

		if (strlen(addmutator.string))
		{
			if (MutatorAllowed(addmutator.string))
			{
				if (!strcmp(addmutator.string, "chaos") || !strcmp(addmutator.string, "1"))
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
				else
				{
					for (int i = 0; i < MAX_MUTATORS; i++)
					{
						if (strstr(addmutator.string, g_szMutators[i]) || addmutator.value == (i + 1))
						{
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
								int time = fmin(fmax(mutatortime.value, 10), 120);
								MESSAGE_BEGIN(MSG_ALL, gmsgAddMutator);
									WRITE_BYTE(i + 1);
									WRITE_BYTE(time);
								MESSAGE_END();

								mutators_t *mutator = new mutators_t();
								mutator->mutatorId = i + 1;
								mutator->timeToLive = gpGlobals->time + time;
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
		while (m != NULL)
		{
			// ALERT(at_aiconsole, ">>> [%.2f] found m->mutatorId=%d [%.2f]\n", gpGlobals->time, m->mutatorId, m->timeToLive);

			if (m->timeToLive <= gpGlobals->time)
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
				count++;
			}

			prev = m;
			m = m->next;
		}

		// chaos mode
		int adjcount = fmin(fmax(mutatorcount.value, 0), 7);
		if (m_flChaosMutatorTime && m_flChaosMutatorTime < gpGlobals->time && count < adjcount)
		{
			m_flChaosMutatorTime = gpGlobals->time + choasIncrement;

			int attempts = 0;
			while (attempts < adjcount)
			{
				int index = RANDOM_LONG(1,(int)ARRAYSIZE(g_szMutators) - 1);
				const char *tryIt = g_szMutators[index];

				// Skip mutators that break multiplayer
				if (g_pGameRules->IsMultiplayer())
				{
					if (strstr(tryIt, "slowmo") ||
						strstr(tryIt, "speedup") ||
						strstr(tryIt, "topsyturvy") ||
						strstr(tryIt, "explosiveai"))
					{
						attempts++;
						continue;
					}
				}
				else
				{
					if (strstr(tryIt, "maxpack"))
					{
						attempts++;
						continue;
					}
				}

				if (strlen(chaosfilter.string) > 2 && strstr(chaosfilter.string, tryIt))
				{
					// ALERT(at_console, "Ignoring \"%s\", found in sv_chaosfilter\n", tryIt);
					attempts++;
					continue;
				}

				CVAR_SET_STRING("sv_addmutator", g_szMutators[index]);

				MESSAGE_BEGIN(MSG_ALL, gmsgChaos);
					WRITE_BYTE(choasIncrement);
				MESSAGE_END();

				break;
			}
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
					char *key = g_engfuncs.pfnGetInfoKeyBuffer(pl->edict());
					char *name = g_engfuncs.pfnInfoKeyValue(key, "oname");
					if (name && strlen(name))
						g_engfuncs.pfnSetClientKeyValue(pl->entindex(), key, "name", name);
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
					char name[64], *key = g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict());
					strcpy(name, STRING(pl->pev->netname));
					g_engfuncs.pfnSetClientKeyValue(pl->entindex(), key, "oname", name);
					g_engfuncs.pfnSetClientKeyValue(pl->entindex(), key, "name", "Jope");
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
					pl->pev->movetype = MOVETYPE_WALK;
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
