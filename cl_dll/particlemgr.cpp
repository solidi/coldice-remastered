// 02/08/02 November235: Particle System
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "event_api.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "particlemgr.h"
#include "particlesys.h"

//ParticleSystemManager*	g_pParticleSystems = NULL;
ParticleSystemManager g_pParticleSystems; // buz - static single object

ParticleSystemManager::ParticleSystemManager( void )
{
	m_pFirstSystem = NULL;
}

ParticleSystemManager::~ParticleSystemManager( void )
{
	ClearSystems();
}

void ParticleSystemManager::Setup( int mode )
{
	// gEngfuncs.Con_DPrintf("::: ParticleSystemManager::Setup( mode=%d )\n", mode);
	g_pParticleSystems.ClearSystems();

	ExpSmoke = new ParticleSystem(0, "aurora/SmokeExp.aur", 0, 0, Vector(1, 1, 1));
	g_pParticleSystems.AddSystem(ExpSmoke, "ExpSmoke");

	if (mode == 0)
	{
		Burn1 = new ParticleSystem(0, "aurora/Burn1.aur", 0, 0, Vector(1, 1, 1));
		Burn2 = new ParticleSystem(0, "aurora/Burn2.aur", 0, 0, Vector(1, 1, 1));
		Burn3 = new ParticleSystem(0, "aurora/Burn3.aur", 0, 0, Vector(1, 1, 1));
	}
	else
	{
		Burn1 = new ParticleSystem(0, "aurora/ice_Burn1.aur", 0, 0, Vector(1, 1, 1));
		Burn2 = new ParticleSystem(0, "aurora/ice_Burn2.aur", 0, 0, Vector(1, 1, 1));
		Burn3 = new ParticleSystem(0, "aurora/ice_Burn3.aur", 0, 0, Vector(1, 1, 1));
	}
	g_pParticleSystems.AddSystem(Burn1, "Burn1");
	g_pParticleSystems.AddSystem(Burn2, "Burn2");
	g_pParticleSystems.AddSystem(Burn3, "Burn3");

	BurnSmoke = new ParticleSystem(0, "aurora/BurnSmoke.aur", 0, 0, Vector(1, 1, 1));
	g_pParticleSystems.AddSystem(BurnSmoke, "BurnSmoke");

	gHUD.SetPLFlames = 0;
}

void ParticleSystemManager::RefreshFlameSystem( int mode )
{
	for (int i = 1; i <= gEngfuncs.GetMaxClients(); i++)
	{
		char temp[32];
		if (mode)
		{
			sprintf(temp, "ice_fburst1 - %d", i);
			g_pParticleSystems.AddSystem(new ParticleSystem(i, "aurora/ice_fburst1.aur"), temp);
			sprintf(temp, "ice_fburst2 - %d", i);
			g_pParticleSystems.AddSystem(new ParticleSystem(i, "aurora/ice_fburst2.aur"), temp);
			sprintf(temp, "ice_fburst3 - %d", i);
			g_pParticleSystems.AddSystem(new ParticleSystem(i, "aurora/ice_fburst3.aur"), temp);
		}
		else
		{
			sprintf(temp, "fburst1 - %d", i); 
			g_pParticleSystems.AddSystem(new ParticleSystem(i, "aurora/fburst1.aur"), temp);
			sprintf(temp, "fburst2 - %d", i); 
			g_pParticleSystems.AddSystem(new ParticleSystem(i, "aurora/fburst2.aur"), temp);
			sprintf(temp, "fburst3 - %d", i);
			g_pParticleSystems.AddSystem(new ParticleSystem(i, "aurora/fburst3.aur"), temp);
		}

		// gEngfuncs.Con_DPrintf("::: loaded client %d to flamethrower\n", i);
	}
}

void ParticleSystemManager::AddSystem( ParticleSystem* pNewSystem, const char *tag )
{
	// gEngfuncs.Con_Printf("::: add pNewSystem->m_iEntIndex=%d tag=%s\n", pNewSystem->m_iEntIndex, tag);
	strcpy(pNewSystem->tag, tag);
	pNewSystem->m_pNextSystem = m_pFirstSystem;
	m_pFirstSystem = pNewSystem;
}

ParticleSystem *ParticleSystemManager::FindSystem( cl_entity_t* pEntity )
{
	for (ParticleSystem *pSys = m_pFirstSystem; pSys; pSys = pSys->m_pNextSystem)
	{
		if (pEntity->index == pSys->m_iEntIndex)
//		if (pEntity == pSys->GetEntity())
		{
			return pSys;
		}
	}
	return NULL;
}

// blended particles don't use the z-buffer, so we need to sort them before drawing.
// for efficiency, only the systems are sorted - individual particles just get drawn in order of creation.
// (this should actually make things look better - no ugly popping when one particle passes through another.)
void ParticleSystemManager::SortSystems()
{
	ParticleSystem* pSystem;
	ParticleSystem* pLast;
	ParticleSystem* pBeforeCompare, *pCompare;
	
	if (!m_pFirstSystem) return;

	// calculate how far away each system is from the viewer
	for( pSystem = m_pFirstSystem; pSystem; pSystem = pSystem->m_pNextSystem )
		pSystem->CalculateDistance();

	// do an insertion sort on the systems
	pLast = m_pFirstSystem;
	pSystem = pLast->m_pNextSystem;
	while (pSystem)
	{
		if (pLast->m_fViewerDist < pSystem->m_fViewerDist)
		{
			// pSystem is in the wrong place! First, let's unlink it from the list
			pLast->m_pNextSystem = pSystem->m_pNextSystem;

			// then find somewhere to insert it
			if (m_pFirstSystem == pLast || m_pFirstSystem->m_fViewerDist < pSystem->m_fViewerDist)
			{
				// pSystem comes before the first system, insert it there
				pSystem->m_pNextSystem = m_pFirstSystem;
				m_pFirstSystem = pSystem;
			}
			else
			{
				// insert pSystem somewhere within the sorted part of the list
				pBeforeCompare = m_pFirstSystem;
				pCompare = pBeforeCompare->m_pNextSystem;
				while (pCompare != pLast)
				{
					if (pCompare->m_fViewerDist < pSystem->m_fViewerDist)
					{
						// pSystem comes before pCompare. We've found where it belongs.
						break;
					}

					pBeforeCompare = pCompare;
					pCompare = pBeforeCompare->m_pNextSystem;
				}

				// we've found where pSystem belongs. Insert it between pBeforeCompare and pCompare.
				pBeforeCompare->m_pNextSystem = pSystem;
				pSystem->m_pNextSystem = pCompare;
			}
		}
		else
		{
			//pSystem is in the right place, move on
			pLast = pSystem;
		}
		pSystem = pLast->m_pNextSystem;
	}
}

bool SortParticles(const particle *first, const particle *second)
{
	return first->distance > second->distance;
}

void ParticleSystemManager::UpdateSystems( float frametime, int sky ) //LRC - now with added time!
{
//	gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd);
//	gEngfuncs.pTriAPI->RenderMode(kRenderTransAlpha);
	ParticleSystem* pSystem;
	ParticleSystem* pLast = NULL;
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();

	vec3_t normal, forward, right, up, prevorigin;

	gEngfuncs.GetViewAngles((float*)normal);
	AngleVectors(normal, forward, right, up);
//	vec3_t normal, forward, right, up;

//	gEngfuncs.GetViewAngles((float*)normal);
//	AngleVectors(normal,forward,right,up);

	//SortSystems();

	SortedList.clear();

	pSystem = m_pFirstSystem;

	while( pSystem )
	{
		// buz
		cl_entity_t *ent = NULL;
		ent = gEngfuncs.GetEntityByIndex(pSystem->m_iEntIndex);

		if (!ent || (ent->curstate.renderfx == 70 && !sky) ||
			(ent->curstate.renderfx != 70 && sky))
		{
			pLast = pSystem;
			pSystem = pSystem->m_pNextSystem;
			continue;
		}

		if (pSystem->UpdateSystem(frametime, localPlayer->curstate.messagenum))
		{
			//if (pSystem->m_iEntIndex > 0)
			//	gEngfuncs.Con_Printf("ParticleSystemManager::UpdateSystem DONE - pSystem->m_iEntIndex=%d\n", pSystem->m_iEntIndex);

			pSystem->DrawSystem(SortedList);
			pLast = pSystem;
			pSystem = pSystem->m_pNextSystem;
		}
		else // delete this system
		{
			if (pLast)
			{
				pLast->m_pNextSystem = pSystem->m_pNextSystem;
				delete pSystem;
				pSystem = pLast->m_pNextSystem;
			}
			else // deleting the first system
			{
				m_pFirstSystem = pSystem->m_pNextSystem;
				delete pSystem;
				pSystem = m_pFirstSystem;
			}
		}
	}

	SortedList.sort(SortParticles);

	for (particle * Particle:SortedList)
	{
		//if (Particle->MySys->m_iEntIndex > 0)
		//	gEngfuncs.Con_Printf("particle * Particle:SortedList - Particle->MySys->m_iEntIndex=%d\n", Particle->MySys->m_iEntIndex);
		Particle->MySys->DrawParticle(Particle, right, up);
	}

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

	if (reset)
	{
		Setup(gHUD.m_iIceModels);
		FlameSystem.Init();
		reset = false;
	}
}

void ParticleSystemManager::ClearSystems( void )
{
	ParticleSystem *pSystem = m_pFirstSystem;
	ParticleSystem *pTemp;

	while ( pSystem )
	{
		pTemp = pSystem->m_pNextSystem;
		// gEngfuncs.Con_DPrintf("::: delete pSystem->m_iEntIndex=%d, tag=%s\n", pSystem->m_iEntIndex, pSystem->tag);
		delete pSystem;
		pSystem = pTemp;
	}

	m_pFirstSystem = NULL;
}

void ParticleSystemManager::DeleteSystemWithEntity( int entindex )
{
	ParticleSystem *pPrev = NULL;
	for (ParticleSystem *pSys = m_pFirstSystem; pSys; pSys = pSys->m_pNextSystem)
	{
		if (entindex == pSys->m_iEntIndex)
		{
			if (pPrev)
			{
				pPrev->m_pNextSystem = pSys->m_pNextSystem;
				delete pSys;
				return;
				/*pSys = pPrev;
				continue;*/
			}
			else // deleting first system
			{
				m_pFirstSystem = pSys->m_pNextSystem;
				delete pSys;
				return;
			}
		}
		pPrev = pSys;
	}
}