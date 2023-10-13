#ifndef FLAMESYS_HEADER
#define FLAMESYS_HEADER

#include <map>
#include "r_studioint.h"

typedef struct FlameSys
{
	int MaxFlames;
	int Enable;
	float NextTick;
	float NextDlight;

} FlameSys;

class CFlameSystem
{
public:
	void SetState(int EntIndex, int State);
	void Extinguish(int EntIndex);
	void Process(cl_entity_s *Entity, const engine_studio_api_t &IEngineStudio);
	void Init();
private:
	std::map <int, FlameSys> Data;
};

extern CFlameSystem FlameSystem;

#else
#endif;
