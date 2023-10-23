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

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );

DLL_GLOBAL CGameRules*	g_pGameRules = NULL;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgMOTD;
extern int gmsgMutators;

int g_teamplay = 0;
int g_ExplosiveAI = 0;
int g_ItemsExplode = 0;

extern DLL_GLOBAL const char *g_MutatorChaos;
extern DLL_GLOBAL const char *g_MutatorRocketCrowbar;
extern DLL_GLOBAL const char *g_MutatorInstaGib;
extern DLL_GLOBAL const char *g_MutatorVolatile;
extern DLL_GLOBAL const char *g_MutatorPlumber;
extern DLL_GLOBAL const char *g_MutatorPaintball;
extern DLL_GLOBAL const char *g_MutatorSuperJump;
extern DLL_GLOBAL const char *g_MutatorMegaSpeed;
extern DLL_GLOBAL const char *g_MutatorLightsOut;
extern DLL_GLOBAL const char *g_MutatorSlowmo;
extern DLL_GLOBAL const char *g_MutatorIce;
extern DLL_GLOBAL const char *g_MutatorTopsyTurvy;
extern DLL_GLOBAL const char *g_MutatorBarrels;
extern DLL_GLOBAL const char *g_MutatorLoopback;
extern DLL_GLOBAL const char *g_MutatorMaxPack;
extern DLL_GLOBAL const char *g_MutatorInfiniteAmmo;
extern DLL_GLOBAL const char *g_MutatorRandomWeapon;
extern DLL_GLOBAL const char *g_MutatorSpeedUp;
extern DLL_GLOBAL const char *g_MutatorRockets;
extern DLL_GLOBAL const char *g_MutatorGrenades;
extern DLL_GLOBAL const char *g_MutatorSnowball;
extern DLL_GLOBAL const char *g_MutatorPushy;
extern DLL_GLOBAL const char *g_MutatorGravity;
extern DLL_GLOBAL const char *g_MutatorInvisible;
extern DLL_GLOBAL const char *g_MutatorPortal;
extern DLL_GLOBAL const char *g_MutatorJope;
extern DLL_GLOBAL const char *g_MutatorLongJump;
extern DLL_GLOBAL const char *g_MutatorSlowBullets;
extern DLL_GLOBAL const char *g_MutatorExplosiveAI;
extern DLL_GLOBAL const char *g_MutatorItemsExplode;
extern DLL_GLOBAL const char *g_MutatorNotTheBees;
extern DLL_GLOBAL const char *g_MutatorDontShoot;
extern DLL_GLOBAL const char *g_Mutator999;
extern DLL_GLOBAL const char *g_MutatorBerserker;
extern DLL_GLOBAL const char *g_MutatorJeepAThon;

extern DLL_GLOBAL int g_GameMode;

DLL_GLOBAL const char *g_szMutators[] = {
	"chaos",
	"rocketcrowbar",
	"instagib",
	"volatile",
	"plumber",
	"paintball",
	"dkmode",
	"superjump",
	"megarun",
	"lightsout",
	"slowmo",
	"ice",
	"topsyturvy",
	"turrets",
	"barrels",
	"chumxplode",
	"santahat",
	"coolflesh",
	"sanic",
	"coolflesh",
	"loopback",
	"maxpack",
	"infiniteammo",
	"randomweapon",
	"speedup",
	"rockets",
	"invisible",
	"grenades",
	"astronaut",
	"snowballs",
	"pushy",
	"portal",
	"jope",
	"inverse",
	"oldtime",
	"sildenafil",
	"longjump",
	"slowbullets",
	"explosiveai",
	"itemsexplode",
	"notthebees",
	"dontshoot",
	"999",
	"berserker",
	"autoaim",
	"slowweapons",
	"fastweapons",
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

		if (g_GameMode == GAME_GUNGAME)
		{
			g_teamplay = 0;
			return new CHalfLifeGunGame;
		}
		else if (g_GameMode == GAME_CTC)
		{
			g_teamplay = 0;
			return new CHalfLifeCaptureTheChumtoad;
		}
		else if (g_GameMode == GAME_ICEMAN)
		{
			g_teamplay = 0;
			return new CHalfLifeJesusVsSanta;
		}
		else if (g_GameMode == GAME_CHILLDEMIC)
		{
			g_teamplay = 0;
			return new CHalfLifeChilldemic;
		}

		if ((int)gpGlobals->deathmatch == 1)
		{
			// vanilla deathmatch
			g_teamplay = 0;
			return new CHalfLifeMultiplay;
		}
		else
		{
			// vanilla deathmatch??
			g_teamplay = 0;
			return new CHalfLifeMultiplay;
		}
	}
}

BOOL CGameRules::WeaponMutators( CBasePlayerWeapon *pWeapon )
{
	if (pWeapon && pWeapon->m_pPlayer)
	{
		if (m_iDontShoot)
		{
			if (pWeapon->iItemSlot() != 1)
			{
				CGrenade::Vest( pWeapon->m_pPlayer->pev, pWeapon->m_pPlayer->pev->origin );
				pWeapon->m_pPlayer->pev->solid = SOLID_NOT;
				pWeapon->m_pPlayer->GibMonster();
				pWeapon->m_pPlayer->pev->effects |= EF_NODRAW;
				ClientPrint(pWeapon->m_pPlayer->pev, HUD_PRINTCENTER, "Don't Shoot!!!\n(fists / kicks / slides only!)");
				return FALSE; // nothing else.
			}
		}

		if (strstr(mutators.string, g_MutatorRockets) ||
			atoi(mutators.string) == MUTATOR_ROCKETS)
		{
			if (!pWeapon->m_bFired && RANDOM_LONG(0,10) == 2) {
				pWeapon->ThrowRocket(FALSE);
			}
		}
		
		if (strstr(mutators.string, g_MutatorGrenades) ||
			atoi(mutators.string) == MUTATOR_GRENADES)
		{
			if (!pWeapon->m_bFired && RANDOM_LONG(0,10) == 2) {
				pWeapon->ThrowGrenade(FALSE);
			}
		}
		
		if (strstr(mutators.string, g_MutatorSnowball) ||
			atoi(mutators.string) == MUTATOR_SNOWBALL)
		{
			if (!pWeapon->m_bFired && RANDOM_LONG(0,10) == 2) {
				pWeapon->ThrowSnowball(FALSE);
			}
		}

		if (strstr(mutators.string, g_MutatorPushy) ||
			atoi(mutators.string) == MUTATOR_PUSHY)
		{
			UTIL_MakeVectors( pWeapon->m_pPlayer->pev->v_angle );
			pWeapon->m_pPlayer->pev->velocity = pWeapon->m_pPlayer->pev->velocity - gpGlobals->v_forward * RANDOM_FLOAT(50,100) * 5;
		}
	}

	return TRUE;
}

void CGameRules::SpawnMutators(CBasePlayer *pPlayer)
{
	if (strstr(mutators.string, g_MutatorTopsyTurvy) ||
		atoi(mutators.string) == MUTATOR_TOPSYTURVY)
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "topsy", "1");
	else
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "topsy", "0");

	if ((strstr(mutators.string, g_MutatorMegaSpeed) ||
		atoi(mutators.string) == MUTATOR_MEGASPEED))
		g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "1");

	if ((strstr(mutators.string, g_MutatorIce) ||
		atoi(mutators.string) == MUTATOR_ICE))
		pPlayer->pev->friction = 0.3;

	GiveMutators(pPlayer);

	if (strstr(mutators.string, g_MutatorInvisible) ||
		atoi(mutators.string) == MUTATOR_INVISIBLE) {
		if (pPlayer->pev->renderfx == kRenderFxGlowShell)
			pPlayer->pev->renderfx = kRenderFxNone;
		if (pPlayer->pev->rendermode != kRenderTransAlpha)
			pPlayer->pev->rendermode = kRenderTransAlpha;
	}
	else
	{
		if (pPlayer->pev->rendermode == kRenderTransAlpha)
			pPlayer->pev->rendermode = kRenderNormal;
	}

	if (randomweapon.value)
		pPlayer->GiveRandomWeapon(NULL);

	if (strstr(mutators.string, g_Mutator999) ||
		atoi(mutators.string) == MUTATOR_999) {
		pPlayer->pev->max_health = 999;
		pPlayer->pev->health = 999;
		pPlayer->pev->armorvalue = 999;
	}
}

void CGameRules::GiveMutators(CBasePlayer *pPlayer)
{
	if (strstr(mutators.string, g_MutatorRocketCrowbar) ||
		atoi(mutators.string) == MUTATOR_ROCKETCROWBAR) {
		if (!pPlayer->HasNamedPlayerItem("weapon_rocketcrowbar"))
			pPlayer->GiveNamedItem("weapon_rocketcrowbar");
	}

	if (strstr(mutators.string, g_MutatorInstaGib) ||
		atoi(mutators.string) == MUTATOR_INSTAGIB) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_railgun"))
			pPlayer->GiveNamedItem("weapon_dual_railgun");
		pPlayer->GiveAmmo(URANIUM_MAX_CARRY, "uranium", URANIUM_MAX_CARRY);
	}

	if (strstr(mutators.string, g_MutatorPlumber) ||
		atoi(mutators.string) == MUTATOR_PLUMBER) {
		if (!pPlayer->HasNamedPlayerItem("weapon_dual_wrench"))
			pPlayer->GiveNamedItem("weapon_dual_wrench");
	}

	if (strstr(mutators.string, g_MutatorBarrels) ||
		atoi(mutators.string) == MUTATOR_BARRELS) {
		if (!pPlayer->HasNamedPlayerItem("weapon_gravitygun"))
			pPlayer->GiveNamedItem("weapon_gravitygun");
	}

	if (strstr(mutators.string, g_MutatorPortal) ||
		atoi(mutators.string) == MUTATOR_PORTAL) {
		if (!pPlayer->HasNamedPlayerItem("weapon_ashpod"))
			pPlayer->GiveNamedItem("weapon_ashpod");
	}

	if (strstr(mutators.string, g_MutatorLongJump) ||
		atoi(mutators.string) == MUTATOR_LONGJUMP) {
		if (!pPlayer->m_fLongJump && (pPlayer->pev->weapons & (1<<WEAPON_SUIT)))
			pPlayer->GiveNamedItem("item_longjump");
	}

	if (strstr(mutators.string, g_MutatorBerserker) ||
		atoi(mutators.string) == MUTATOR_BERSERKER) {
		pPlayer->RemoveAllItems(FALSE);
		pPlayer->GiveNamedItem("weapon_fists");
		pPlayer->GiveNamedItem("weapon_chainsaw");
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

void CGameRules::CheckMutators(void)
{
	if ((strstr(mutators.string, g_MutatorChaos) ||
		atoi(mutators.string) == MUTATOR_CHAOS))
	{
		// Reset if going long
		if (m_flChaosCheck > (gpGlobals->time + chaostime.value))
		{
			m_flChaosCheck = 0;
		}

		if (m_flChaosCheck < gpGlobals->time)
		{
			CBaseEntity *pWorld = CBaseEntity::Instance(NULL);
			if (pWorld)
				((CWorld *)pWorld)->RandomizeMutators();

			m_flChaosCheck = gpGlobals->time + chaostime.value;
		}
	}

	// Let the game breathe between changes, also allows some changes like sys_timeframe to work
	if (strcmp(m_flCheckMutators, mutators.string) != 0)
	{
		m_flDetectedMutatorChange = gpGlobals->time + 2.0;
		strcpy(m_flCheckMutators, mutators.string);
	}

	if (strlen(m_flCheckMutators) > 0 &&
		m_flDetectedMutatorChange && m_flDetectedMutatorChange < gpGlobals->time)
	{
		RefreshSkillData();

		if ((strstr(mutators.string, g_MutatorSlowmo) ||
			atoi(mutators.string) == MUTATOR_SLOWMO) && CVAR_GET_FLOAT("sys_timescale") != 0.49f)
			CVAR_SET_FLOAT("sys_timescale", 0.49);
		else
		{
			if ((!strstr(mutators.string, g_MutatorSlowmo) &&
				atoi(mutators.string) != MUTATOR_SLOWMO) &&
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

		if ((strstr(mutators.string, g_MutatorLightsOut) ||
			atoi(mutators.string) == MUTATOR_LIGHTSOUT))
		{
			LIGHT_STYLE(0, "b");
			CVAR_SET_STRING("mp_flashlight", "1");
			CVAR_SET_STRING("sv_skycolor_r", "0");
			CVAR_SET_STRING("sv_skycolor_g", "0");
			CVAR_SET_STRING("sv_skycolor_b", "0");
		}
		else
		{
			LIGHT_STYLE(0, "m");
			CVAR_SET_STRING("sv_skycolor_r", szSkyColor[0]);
			CVAR_SET_STRING("sv_skycolor_g", szSkyColor[1]);
			CVAR_SET_STRING("sv_skycolor_b", szSkyColor[2]);
		}

		if (m_JopeCheck) {
			UTIL_ClientPrintAll(HUD_PRINTCENTER, "KING JOPE HAS BEEN DETHRONED!\n");
			m_JopeCheck = FALSE;
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
				CBasePlayer *pl = (CBasePlayer *)pPlayer;
				if (pPlayer && pPlayer->IsPlayer() && !pl->IsObserver())
				{
					if (m_szJopeName[i - 1] && m_szJopeName[i - 1][0] != 0 && strlen(m_szJopeName[i - 1])) {
						g_engfuncs.pfnSetClientKeyValue(pl->entindex(), g_engfuncs.pfnGetInfoKeyBuffer(pl->edict()),
							"name", m_szJopeName[i - 1]);
					}
				}
			}
		}

		if (strstr(mutators.string, g_MutatorJope) ||
			atoi(mutators.string) == MUTATOR_JOPE) {
			m_JopeCheck = TRUE;
			UTIL_ClientPrintAll(HUD_PRINTCENTER, "ALL HAIL KING JOPE!\n");
		}

		for (int i = 1; i <= gpGlobals->maxClients; i++)
		{
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
			CBasePlayer *pl = (CBasePlayer *)pPlayer;
			if (pPlayer && pPlayer->IsPlayer() && !pl->IsObserver())
			{
				pl->m_iShowMutatorMessage = gpGlobals->time + 2.0;

				if (strstr(mutators.string, g_MutatorTopsyTurvy) ||
					atoi(mutators.string) == MUTATOR_TOPSYTURVY) {
					g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "topsy", "1");
				} else {
					g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "topsy", "0");
				}

				if ((strstr(mutators.string, g_MutatorIce) ||
					atoi(mutators.string) == MUTATOR_ICE))
					pPlayer->pev->friction = 0.3;
				else if (pPlayer->pev->friction > 0.2 && pPlayer->pev->friction < 0.4)
					pPlayer->pev->friction = 1.0;

				if ((strstr(mutators.string, g_MutatorMegaSpeed) ||
					atoi(mutators.string) == MUTATOR_MEGASPEED))
					g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "1");
				else if (((CBasePlayer *)pPlayer)->m_fHasRune != RUNE_HASTE &&
						 !((CBasePlayer *)pPlayer)->IsArmoredMan)
					g_engfuncs.pfnSetPhysicsKeyValue(pPlayer->edict(), "haste", "0");

				GiveMutators(pl);

				if (strstr(mutators.string, g_MutatorInvisible) ||
					atoi(mutators.string) == MUTATOR_INVISIBLE) {
					if (pPlayer->pev->renderfx == kRenderFxGlowShell)
						pPlayer->pev->renderfx = kRenderFxNone;
					if (pl->pev->rendermode != kRenderTransAlpha)
						pl->pev->rendermode = kRenderTransAlpha;
				}
				else
				{
					if (pl->m_fHasRune != RUNE_CLOAK)
					{
						if (pl->pev->rendermode == kRenderTransAlpha)
							pl->pev->rendermode = kRenderNormal;
					}
				}

				if (m_JopeCheck) {
					strcpy(m_szJopeName[pl->entindex() - 1], STRING(pl->pev->netname));
					g_engfuncs.pfnSetClientKeyValue(pl->entindex(), g_engfuncs.pfnGetInfoKeyBuffer(pl->edict()),
						"name", "Jope");
				}
				
				if (strstr(mutators.string, g_Mutator999) ||
					atoi(mutators.string) == MUTATOR_999) {
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
			}

			if ((strstr(mutators.string, g_MutatorSuperJump) ||
				atoi(mutators.string) == MUTATOR_SUPERJUMP) && CVAR_GET_FLOAT("sv_jumpheight") != 299)
			{
				CVAR_SET_FLOAT("sv_jumpheight", 299);
			}
			else
			{
				if ((!strstr(mutators.string, g_MutatorSuperJump) &&
					atoi(mutators.string) != MUTATOR_SUPERJUMP) && CVAR_GET_FLOAT("sv_jumpheight") == 299)
					CVAR_SET_FLOAT("sv_jumpheight", 45);
			}

			if ((strstr(mutators.string, g_MutatorGravity) ||
				atoi(mutators.string) == MUTATOR_GRAVITY) && CVAR_GET_FLOAT("sv_gravity") != 199)
			{
				CVAR_SET_FLOAT("sv_gravity", 199);
			}
			else
			{
				if ((!strstr(mutators.string, g_MutatorGravity) &&
					atoi(mutators.string) != MUTATOR_GRAVITY) && CVAR_GET_FLOAT("sv_gravity") == 199)
					CVAR_SET_FLOAT("sv_gravity", 800);
			}

			if ((strstr(mutators.string, g_MutatorInfiniteAmmo) ||
				atoi(mutators.string) == MUTATOR_INFINITEAMMO) && infiniteammo.value != 1)
				CVAR_SET_FLOAT("sv_infiniteammo", 1);
			else
			{
				if ((!strstr(mutators.string, g_MutatorInfiniteAmmo) &&
					atoi(mutators.string) != MUTATOR_INFINITEAMMO) && infiniteammo.value)
					CVAR_SET_FLOAT("sv_infiniteammo", 0);
			}

			if ((strstr(mutators.string, g_MutatorRandomWeapon) ||
				atoi(mutators.string) == MUTATOR_RANDOMWEAPON) && randomweapon.value != 1)
				CVAR_SET_FLOAT("mp_randomweapon", 1);
			else
			{
				if ((!strstr(mutators.string, g_MutatorRandomWeapon) &&
					atoi(mutators.string) != MUTATOR_RANDOMWEAPON) && randomweapon.value)
					CVAR_SET_FLOAT("mp_randomweapon", 0);
			}

			if ((strstr(mutators.string, g_MutatorSpeedUp) ||
				atoi(mutators.string) == MUTATOR_SPEEDUP) && CVAR_GET_FLOAT("sys_timescale") != 1.49f)
				CVAR_SET_FLOAT("sys_timescale", 1.49);
			else
			{
				if ((!strstr(mutators.string, g_MutatorSpeedUp) &&
					atoi(mutators.string) != MUTATOR_SPEEDUP) &&
					CVAR_GET_FLOAT("sys_timescale") > 1.48f && CVAR_GET_FLOAT("sys_timescale") < 1.50f)
					CVAR_SET_FLOAT("sys_timescale", 1.0);
			}

			if ((strstr(mutators.string, g_MutatorSlowBullets) ||
				atoi(mutators.string) == MUTATOR_SLOWBULLETS) && CVAR_GET_FLOAT("sv_slowbullets") != 2)
			{
				CVAR_SET_FLOAT("sv_slowbullets", 2);
			}
			else
			{
				if ((!strstr(mutators.string, g_MutatorSlowBullets) &&
					atoi(mutators.string) != MUTATOR_SLOWBULLETS) && CVAR_GET_FLOAT("sv_slowbullets") == 2)
					CVAR_SET_FLOAT("sv_slowbullets", 0);
			}

			if ((strstr(mutators.string, g_MutatorExplosiveAI) ||
				atoi(mutators.string) == MUTATOR_EXPLOSIVEAI) && g_ExplosiveAI != 1)
			{
				g_ExplosiveAI = 1;
			}
			else
			{
				if ((!strstr(mutators.string, g_MutatorExplosiveAI) &&
					atoi(mutators.string) != MUTATOR_EXPLOSIVEAI) && g_ExplosiveAI == 1)
					g_ExplosiveAI = 0;
			}

			if ((strstr(mutators.string, g_MutatorItemsExplode) ||
				atoi(mutators.string) == MUTATOR_ITEMSEXPLODE) && g_ItemsExplode != 1)
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
				if ((!strstr(mutators.string, g_MutatorItemsExplode) &&
					atoi(mutators.string) != MUTATOR_ITEMSEXPLODE) && g_ItemsExplode == 1)
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

			if ((strstr(mutators.string, g_MutatorNotTheBees) ||
				atoi(mutators.string) == MUTATOR_NOTTHEBEES) && m_iNotTheBees != 1)
			{
				m_iNotTheBees = 1;
			}
			else
			{
				if ((!strstr(mutators.string, g_MutatorNotTheBees) &&
					atoi(mutators.string) != MUTATOR_NOTTHEBEES) && m_iNotTheBees == 1)
					m_iNotTheBees = 0;
			}

			if ((strstr(mutators.string, g_MutatorDontShoot) ||
				atoi(mutators.string) == MUTATOR_DONTSHOOT) && !m_iDontShoot)
			{
				m_iDontShoot = TRUE;
			}
			else
			{
				if ((!strstr(mutators.string, g_MutatorDontShoot) &&
					atoi(mutators.string) != MUTATOR_DONTSHOOT) && m_iDontShoot)
					m_iDontShoot = FALSE;
			}

			if ((strstr(mutators.string, g_MutatorVolatile) ||
				atoi(mutators.string) == MUTATOR_VOLATILE) && !m_iVolatile)
			{
				m_iVolatile = TRUE;
			}
			else
			{
				if ((!strstr(mutators.string, g_MutatorVolatile) &&
					atoi(mutators.string) != MUTATOR_VOLATILE) && m_iVolatile)
					m_iVolatile = FALSE;
			}
		}

		char szMutators[64];
		strncpy(szMutators, mutators.string, sizeof(szMutators));
		MESSAGE_BEGIN( MSG_ALL, gmsgMutators );
			WRITE_STRING(szMutators);
		MESSAGE_END();

		m_flDetectedMutatorChange = 0;
	}
}

void CGameRules::CheckGameMode( void )
{
	// Blank at map start
	if (!strlen(m_flCheckGameMode))
		strcpy(m_flCheckGameMode, gamemode.string);

	if (strcmp(m_flCheckGameMode, gamemode.string) != 0)
	{
		m_flDetectedGameModeChange = gpGlobals->time + 5.0;
		strcpy(m_flCheckGameMode, gamemode.string);
		//UTIL_ClientPrintAll(HUD_PRINTTALK, "Game mode has changed to \"%s\". Ending current game in 5 seconds.\n", m_flCheckGameMode);
		UTIL_ClientPrintAll(HUD_PRINTCONSOLE, "Game mode has changed to \"%s\". Please change the map to begin.\n", m_flCheckGameMode);
	}

	if (m_flDetectedGameModeChange && m_flDetectedGameModeChange < gpGlobals->time)
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

		m_flDetectedGameModeChange = 0;
	}
}

void CGameRules::UpdateMutatorMessage( CBasePlayer *pPlayer )
{
	if (pPlayer->m_iShowMutatorMessage != -1 && pPlayer->m_iShowMutatorMessage < gpGlobals->time) {
		// Display Mutators
		if (strlen(mutators.string) > 2 && strlen(mutators.string) < 64)
			pPlayer->DisplayHudMessage(UTIL_VarArgs("Mutators Active: %s", mutators.string),
			TXT_CHANNEL_MUTATOR_TITLE, .02, .18, 210, 210, 210, 2, .015, 2, 5, .25);

		pPlayer->m_iShowMutatorMessage = -1;
	}
}

void CGameRules::UpdateGameModeMessage( CBasePlayer *pPlayer )
{
	if (pPlayer->m_iShowGameModeMessage != -1 && pPlayer->m_iShowGameModeMessage < gpGlobals->time) {
		if (strlen(gamemode.string) > 1 && strcmp(gamemode.string, "ffa"))
			pPlayer->DisplayHudMessage(UTIL_VarArgs("Game Mode is %s", gamemode.string), TXT_CHANNEL_GAME_TITLE, .02, .14, 210, 210, 210, 2, .015, 2, 5, .25);
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
