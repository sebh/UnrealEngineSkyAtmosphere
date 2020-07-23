// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

// Only for artefact evaluation, Should not be used when measuring performance
#define SKYHIGHQUALITY

class Texture2D;
class Texture3D;

struct GlslVec3
{
	float x, y, z;
};
#define vec3 GlslVec3
#include "./Resources/Bruneton17/definitions.glsl"

typedef unsigned int uint32;
typedef AtmosphereParameters AtmosphereInfo;

void SetupEarthAtmosphere(AtmosphereInfo& info);

struct LookUpTablesInfo
{
#if 1
	uint32 TRANSMITTANCE_TEXTURE_WIDTH = 256;
	uint32 TRANSMITTANCE_TEXTURE_HEIGHT = 64;

	uint32 SCATTERING_TEXTURE_R_SIZE = 32;
	uint32 SCATTERING_TEXTURE_MU_SIZE = 128;
	uint32 SCATTERING_TEXTURE_MU_S_SIZE = 32;
	uint32 SCATTERING_TEXTURE_NU_SIZE = 8;

	uint32 IRRADIANCE_TEXTURE_WIDTH = 64;
	uint32 IRRADIANCE_TEXTURE_HEIGHT = 16;
#else
	uint32 TRANSMITTANCE_TEXTURE_WIDTH = 64;
	uint32 TRANSMITTANCE_TEXTURE_HEIGHT = 16;

	uint32 SCATTERING_TEXTURE_R_SIZE = 16;
	uint32 SCATTERING_TEXTURE_MU_SIZE = 16;
	uint32 SCATTERING_TEXTURE_MU_S_SIZE = 16;
	uint32 SCATTERING_TEXTURE_NU_SIZE = 4;

	uint32 IRRADIANCE_TEXTURE_WIDTH = 32;
	uint32 IRRADIANCE_TEXTURE_HEIGHT = 8;
#endif

	// Derived from above
	uint32 SCATTERING_TEXTURE_WIDTH = 0xDEADBEEF;
	uint32 SCATTERING_TEXTURE_HEIGHT = 0xDEADBEEF;
	uint32 SCATTERING_TEXTURE_DEPTH = 0xDEADBEEF;

	void updateDerivedData()
	{
		SCATTERING_TEXTURE_WIDTH = SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_MU_S_SIZE;
		SCATTERING_TEXTURE_HEIGHT= SCATTERING_TEXTURE_MU_SIZE;
		SCATTERING_TEXTURE_DEPTH = SCATTERING_TEXTURE_R_SIZE;
	}

	LookUpTablesInfo() { updateDerivedData(); }
};



struct LookUpTables
{
	Texture2D* TransmittanceTex;
	Texture3D* ScatteringTex;
	Texture2D* IrradianceTex;

	void Allocate(LookUpTablesInfo& LutInfo);
	void Release();
};



struct TempLookUpTables
{
	Texture2D* DeltaIrradianceTex;
	Texture3D* DeltaMieScatteringTex;
	Texture3D* DeltaRaleighScatteringTex;
	Texture3D* DeltaScatteringDensityTex;

	void Allocate(LookUpTablesInfo& LutInfo);
	void Release();
};


