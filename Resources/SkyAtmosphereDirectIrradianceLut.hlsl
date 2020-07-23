// Copyright Epic Games, Inc. All Rights Reserved.


#include "./Resources/SkyAtmosphereBruneton.hlsl"



Texture2D<float4>  TransmittanceLUT		: register(t0);



struct PixelOutputStructure
{
	float3 DeltaIrradiance				: SV_TARGET0;
	float3 Irradiance					: SV_TARGET1;
};



PixelOutputStructure DirectIrradianceLutPS(VertexOutput Input)
{
	float2 PixPos = Input.position.xy;
	PixelOutputStructure PixelOutput;

	AtmosphereParameters Parameters = GetAtmosphereParameters();

	PixelOutput.DeltaIrradiance = ComputeDirectIrradianceTexture(Parameters, TransmittanceLUT, PixPos);
	PixelOutput.Irradiance = 0.0f;

	return PixelOutput;
}


