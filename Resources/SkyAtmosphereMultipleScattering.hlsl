// Copyright Epic Games, Inc. All Rights Reserved.


#include "./Resources/SkyAtmosphereBruneton.hlsl"



Texture2D<float4>  TransmittanceLUT							: register(t0);
Texture3D<float4>  ScatteringDensityLUT						: register(t1);



struct PixelOutputStructure
{
	float3 DeltaMultipleScattering							: SV_TARGET0;
	float4 Scattering										: SV_TARGET1;
};



PixelOutputStructure MultipleScatteringLutPS(GeometryOutput Input)
{
	float2 PixPos = Input.position.xy;
	PixelOutputStructure PixelOutput;

	AtmosphereParameters Parameters = GetAtmosphereParameters();

	float nu = 0.0;
	PixelOutput.DeltaMultipleScattering = ComputeMultipleScatteringTexture(
		Parameters, TransmittanceLUT, ScatteringDensityLUT,
		float3(PixPos, float(Input.sliceId) + 0.5f), nu);

	PixelOutput.Scattering = float4(mul(getLuminanceFromRadiance(), PixelOutput.DeltaMultipleScattering.rgb / RayleighPhaseFunction(nu)), 0.0);

	return PixelOutput;
}


