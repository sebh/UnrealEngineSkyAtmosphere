// Copyright Epic Games, Inc. All Rights Reserved.


#include "./Resources/Common.hlsl"


Texture2D<float4> PathtracingLuminanceTexture				: register(t2);
Texture2D<float4> PathtracingTransmittanceTexture			: register(t3);

float sRGB(float x)
{
	if (x <= 0.00031308)
		return 12.92 * x;
	else
		return 1.055*pow(x, (1.0 / 2.4)) - 0.055;
}

float4 sRGB(float4 vec)
{
	return float4(sRGB(vec.x), sRGB(vec.y), sRGB(vec.z), vec.w);
}





struct PixelOutputStruct
{
	float4 HdrBuffer		: SV_TARGET0;
	float4 Transmittance	: SV_TARGET1;
};

PixelOutputStruct ApplySkyAtmospherePS(VertexOutput input)
{
	uint2 texCoord = input.position.xy;

	float4 PathtracingLuminance		= PathtracingLuminanceTexture.Load(uint3(texCoord, 0));
	float4 PathtracingTransmittance	= PathtracingTransmittanceTexture.Load(uint3(texCoord, 0));

	PathtracingLuminance = PathtracingLuminance.w > 0.0 ? PathtracingLuminance / PathtracingLuminance.w : float4(0.0, 0.0, 0.0, 1.0);
	PathtracingTransmittance = PathtracingTransmittance.w > 0.0 ? PathtracingTransmittance / PathtracingTransmittance.w : float4(0.0, 0.0, 0.0, 1.0);

	PixelOutputStruct output;
	output.HdrBuffer = PathtracingLuminance;
	output.Transmittance = PathtracingTransmittance;
	return output;
}


float4 PostProcessPS(VertexOutput input) : SV_TARGET
{
	uint2 texCoord = input.position.xy;

	float4 rgbA = texture2d.Load(uint3(texCoord,0));
	rgbA /= rgbA.aaaa;	// Normalise according to sample count when path tracing

	// Similar setup to the Bruneton demo
	float3 white_point = float3(1.08241, 0.96756, 0.95003);
	float exposure = 10.0;
	return float4( pow((float3) 1.0 - exp(-rgbA.rgb / white_point * exposure), (float3)(1.0 / 2.2)), 1.0 );
}



