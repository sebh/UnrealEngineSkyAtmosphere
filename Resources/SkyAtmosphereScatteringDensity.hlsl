// Copyright Epic Games, Inc. All Rights Reserved.


#include "./Resources/SkyAtmosphereBruneton.hlsl"



Texture2D<float4>  TransmittanceLUT							: register(t0);
Texture2D<float4>  IrradianceLUT							: register(t1);
Texture3D<float4>  SingleRayleighScatteringTexture			: register(t2);
Texture3D<float4>  SingleMieScatteringTexture				: register(t3);
Texture3D<float4>  MultipleScatteringTexture				: register(t4);



struct PixelOutputStructure
{
	float3 ScatteringDensity								: SV_TARGET0;
};



PixelOutputStructure ScatteringDensityLutPS(GeometryOutput Input)
{
	float2 PixPos = Input.position.xy;
	PixelOutputStructure PixelOutput;

	AtmosphereParameters Parameters = GetAtmosphereParameters();

	PixelOutput.ScatteringDensity = ComputeScatteringDensityTexture(
		Parameters, TransmittanceLUT, SingleRayleighScatteringTexture,
		SingleMieScatteringTexture, MultipleScatteringTexture,
		IrradianceLUT, float3(PixPos, float(Input.sliceId) + 0.5f),
		ScatteringOrder);

	return PixelOutput;
}


