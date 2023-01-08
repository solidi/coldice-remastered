// 02/08/02 November235: Particle System
#pragma once

#include "particlesys.h"
#include <list>

class ParticleSystemManager
{
public:
	ParticleSystemManager( void );
	~ParticleSystemManager( void );
	void AddSystem( ParticleSystem *pNewSystem, const char *tag );
	ParticleSystem *FindSystem( cl_entity_t* pEntity );
	void UpdateSystems( float frametime, int sky );
	void ClearSystems( void );
	void SortSystems( void );
//	void DeleteSystem( ParticleSystem* );
	void DeleteSystemWithEntity( int entindex );

	void Setup( int mode );
	void RefreshFlameSystem( int mode );

	bool reset;

private:
	ParticleSystem* m_pFirstSystem;
	std::list <particle *> SortedList;
};

//extern ParticleSystemManager* g_pParticleSystems; 
extern ParticleSystemManager g_pParticleSystems;
