// Copyright Epic Games, Inc. All Rights Reserved.


#include "./Resources/SkyAtmosphereBruneton.hlsl"



Texture2D<float4>  TransmittanceLUT		: register(t0);



struct PixelOutputStructure
{
	float3 DeltaRaleigh					: SV_TARGET0;
	float3 DeltaMie						: SV_TARGET1;
	float4 Scattering					: SV_TARGET2;
//	float3 SingleMieScattering			: SV_TARGET3;	// Build a permutation if necessary
};



PixelOutputStructure SingleScatteringLutPS(GeometryOutput Input)
{
	float2 PixPos = Input.position.xy;
	PixelOutputStructure PixelOutput;

	AtmosphereParameters Parameters = GetAtmosphereParameters();

	ComputeSingleScatteringTexture(Parameters, TransmittanceLUT, float3(PixPos, float(Input.sliceId)+0.5f), PixelOutput.DeltaRaleigh, PixelOutput.DeltaMie);
	float3 DeltaRaleigh = mul(getLuminanceFromRadiance(), PixelOutput.DeltaRaleigh);
	float DeltaMieR = mul(getLuminanceFromRadiance(), PixelOutput.DeltaMie);
	PixelOutput.Scattering = float4(DeltaRaleigh, DeltaMieR);
//	PixelOutput.SingleMieScattering = mul(getLuminanceFromRadiance(), PixelOutput.DeltaMie);

	return PixelOutput;
}


