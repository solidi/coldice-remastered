#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "particlemgr.h"
#include "particlesys.h"
#include "FlameSystem.h"

int CHudParticle::Init( void )
{
	gHUD.AddHudElem(this);
	return 1;
}

int CHudParticle::VidInit( void )
{
	m_iFlags |= HUD_ACTIVE;
	// gEngfuncs.Con_DPrintf("::: VidInit -> SetParticles\n");
	iceModels = -1; // always clear on vidinit
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
	if (iceModels != gHUD.m_IceModelsIndex)
	{
		// gEngfuncs.Con_DPrintf("::: SetParticles -> Do Refresh\n");
		g_pParticleSystems.reset = true;
		iceModels = gHUD.m_IceModelsIndex;
	}
}
