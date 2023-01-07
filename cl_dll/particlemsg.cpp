#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "particlemgr.h"
#include "particlesys.h"
#include "FlameSystem.h"

extern cvar_t *cl_icemodels;

int CHudParticle::Init( void )
{
	gHUD.AddHudElem(this);
	return 1;
}

int CHudParticle::VidInit( void )
{
	m_iFlags |= HUD_ACTIVE;
	gEngfuncs.Con_DPrintf("::: VidInit -> SetParticles\n");
	gHUD.m_iIceModels = -1; // always clear on vidinit
	SetParticles();
	return 1;
}

void CHudParticle::Reset( void )
{
	m_iFlags |= HUD_ACTIVE;
}

int CHudParticle::Draw(float flTime)
{
	// Monitor runtime user preference changes
	SetParticles();
	return 1;
}

void CHudParticle::SetParticles()
{
	if (cl_icemodels)
	{
		if (gHUD.m_iIceModels != cl_icemodels->value)
		{
			gEngfuncs.Con_DPrintf("::: SetParticles -> Do Refresh\n");
			g_pParticleSystems.Setup(cl_icemodels->value);
			FlameSystem.Init();
			//g_pParticleSystems.RefreshFlameSystem(cl_icemodels->value);
			gHUD.m_iIceModels = cl_icemodels->value;
		}
	}
}
