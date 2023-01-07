#include "hud.h"
#include "event_api.h"
#include "com_model.h"
#include "studio.h"
#include "studio_util.h"
#include "StudioModelRenderer.h"
#include "cl_util.h"
#include "triangleapi.h"
#include "gl_dlight.h"
#include "FlameSystem.h"
#include "particlesys.h"

CFlameSystem FlameSystem;

void CFlameSystem::SetState(int EntIndex, int State)
{
	Data[EntIndex].Enable = State;
	Data[EntIndex].NextTick = 0;
	Data[EntIndex].NextDlight = 0;

	// gEngfuncs.Con_Printf("CFlameSystem::SetState EntIndex=%d,State=%d\n", EntIndex, State);

	if (!State)
		Extinguish(EntIndex);
}

void CFlameSystem::Extinguish(int EntIndex)
{
	// gEngfuncs.Con_Printf("CFlameSystem::Extinguish EntIndex=%d\n", EntIndex);
	Data[EntIndex].Enable = 0;
	Data[EntIndex].MaxFlames = 0;
	gEngfuncs.pEventAPI->EV_StopSound(EntIndex, CHAN_STATIC, "fire_loop.wav");
}

float(*m_pbonetransform)[MAXSTUDIOBONES][3][4];
extern vec3_t v_origin;

void CFlameSystem::Process(cl_entity_s *Entity, const engine_studio_api_t &IEngineStudio)
{
	int i = Entity->index;

	studiohdr_t *ModelHeader;
	mstudiobodyparts_t *m_pBodyPart;
	mstudiomodel_t *m_pSubModel;
	vec3_t *pstudioverts;
	byte *pvertbone;
	int Vert;
	int BodyPart;
	bool ItIsViewModel = 0;

	Vector Origin;

	if (Data[i].Enable == 1)
	{
		if (Data[i].NextTick < gEngfuncs.GetClientTime())
		{
			Data[i].NextTick = gEngfuncs.GetClientTime() + 0.05;

			ItIsViewModel = (Entity == gEngfuncs.GetViewModel()) ? TRUE : FALSE;

			ModelHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata(Entity->model);

			if (!Data[i].MaxFlames)
			{
				if (ItIsViewModel)
				{
					Data[i].MaxFlames = 3;
				}
				else
				{
					Data[i].MaxFlames = (-Entity->curstate.mins[2] + Entity->curstate.maxs[2]) / 10;

					if (Data[i].MaxFlames < 5)
						Data[i].MaxFlames = 5;
				}
			}

			gEngfuncs.pEventAPI->EV_PlaySound(i, Entity->curstate.origin, CHAN_STATIC,
				"fire_loop.wav", 1, 0.7, (1 << 6) | (1 << 7), 100);

			if (!ItIsViewModel)
				BoneOrigin = (float(*)[MAXSTUDIOBONES][3][4])IEngineStudio.StudioGetBoneTransform();

			for (int z = 0; z < Data[i].MaxFlames; z++)
			{
				if (ItIsViewModel)
				{
					Origin.x = gEngfuncs.pfnRandomFloat(-16, 16);
					Origin.y = gEngfuncs.pfnRandomFloat(-16, 16);
					Origin.z = gEngfuncs.pfnRandomFloat(0, 32);
					Origin = Origin + v_origin;
				}
				else
				{
					if (!strcmp(Entity->model->name, "models/hgrunt.mdl"))
						BodyPart = 0;
					else
						BodyPart = gEngfuncs.pfnRandomLong(0, ModelHeader->numbodyparts - 1);

					m_pBodyPart = (mstudiobodyparts_t *)((byte *)ModelHeader + ModelHeader->bodypartindex) + BodyPart;
					m_pSubModel = (mstudiomodel_t *)((byte *)ModelHeader + m_pBodyPart->modelindex);
					pstudioverts = (vec3_t *)((byte *)ModelHeader + m_pSubModel->vertindex);
					pvertbone = ((byte *)ModelHeader + m_pSubModel->vertinfoindex);
					Vert = gEngfuncs.pfnRandomLong(1, m_pSubModel->numverts);
					VectorTransform(pstudioverts[Vert], (*BoneOrigin)[pvertbone[Vert]], Origin);
				}
				
				if (Data[i].NextDlight < gEngfuncs.GetClientTime())
				{
					Data[i].NextDlight = gEngfuncs.GetClientTime() + 0.15;

					/*
					DynamicLight *dl = MY_AllocDlight(gEngfuncs.pfnRandomLong(-800, -700));
					dl->origin = Origin;
					dl->radius = Data[i].MaxFlames * 16 * gEngfuncs.pfnRandomFloat(0.9, 1);
					dl->color[0] = 0.8745 * gEngfuncs.pfnRandomFloat(0.9, 1);
					dl->color[1] = 0.5411 * gEngfuncs.pfnRandomFloat(0.9, 1);
					dl->color[2] = 0.313 * gEngfuncs.pfnRandomFloat(0.9, 1);
					dl->die = gEngfuncs.GetClientTime() + 0.15;
					dl->decay = dl->radius / 2;
					*/
				}
				
				particle *Particle = NULL;

				switch (gEngfuncs.pfnRandomLong(0, 3))
				{
				case 0:
					Particle = Burn1->ManualSpray(1, Origin);
					break;
				case 1:
					Particle = Burn2->ManualSpray(1, Origin);
					break;
				case 2:
					Particle = Burn3->ManualSpray(1, Origin);
					break;
				}

				if (Particle)
				{
					if (ItIsViewModel || (i <= MAX_CLIENTS && gEngfuncs.pfnRandomLong(0, 1)))
					{
						Particle->LocalCoordsEnt = i;
						Particle->LocalCoordsOffset = (Origin - Entity->origin);
					}
				}

				BurnSmoke->ManualSpray(1, Origin + Vector(gEngfuncs.pfnRandomFloat(-3, 3),
					gEngfuncs.pfnRandomFloat(-3, 3), gEngfuncs.pfnRandomFloat(-3, 3)));
			}
		}
	}
}

void CFlameSystem::Init()
{
	Data.clear();
}
