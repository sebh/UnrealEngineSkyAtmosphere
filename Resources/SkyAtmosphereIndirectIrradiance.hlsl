// Copyright Epic Games, Inc. All Rights Reserved.


#include "./Resources/SkyAtmosphereBruneton.hlsl"



Texture3D<float4>  SingleRayleighScatteringTexture			: register(t0);
Texture3D<float4>  SingleMieScatteringTexture				: register(t1);
Texture3D<float4>  MultipleScatteringTexture				: register(t2);



struct PixelOutputStructure
{
	float3 DeltaIrradiance									: SV_TARGET0;
	float3 Irradiance										: SV_TARGET1;
};



PixelOutputStructure IndirectIrradianceLutPS(VertexOutput Input)
{
	float2 PixPos = Input.position.xy;
	PixelOutputStructure PixelOutput;

	AtmosphereParameters Parameters = GetAtmosphereParameters();

	PixelOutput.DeltaIrradiance = ComputeIndirectIrradianceTexture(
		Parameters, SingleRayleighScatteringTexture,
		SingleMieScatteringTexture, MultipleScatteringTexture,
		PixPos, ScatteringOrder);
	PixelOutput.Irradiance = mul(getLuminanceFromRadiance(), PixelOutput.DeltaIrradiance);

	return PixelOutput;
}


