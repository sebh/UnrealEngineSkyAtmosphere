// Copyright Epic Games, Inc. All Rights Reserved.


#include "./Resources/SkyAtmosphereBruneton.hlsl"



float4 TransmittanceLutPS(VertexOutput Input) : SV_TARGET
{
	float2 pixPos = Input.position.xy;

	AtmosphereParameters Parameters = GetAtmosphereParameters();
	float3 transmittance = ComputeTransmittanceToTopAtmosphereBoundaryTexture(Parameters, pixPos);

	return float4(transmittance, 1.0);
}


