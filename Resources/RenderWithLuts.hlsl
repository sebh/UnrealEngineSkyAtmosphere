// Copyright Epic Games, Inc. All Rights Reserved.


#include "./Resources/SkyAtmosphereBruneton.hlsl"

Texture2D<float4>  TransmittanceLUT							: register(t0);
Texture2D<float4>  IrradianceLUT							: register(t1);
Texture3D<float4>  ScatteringLUT							: register(t2);
Texture3D<float4>  ScatteringCamVol							: register(t3);
Texture3D<float4>  TransmittanceCamVol						: register(t4);

Texture2D<float4>  ViewDepthTexture							: register(t5);



#define SKYATMOSPHERE_HEADER	float2 pixPos = Input.position.xy; \
								AtmosphereParameters Parameters = GetAtmosphereParameters(); \
								float3 ClipSpace = float3((pixPos / float2(gResolution))*float2(2.0, -2.0) - float2(1.0, -1.0), 0.5); \
								float4 HPos = mul(gSkyInvViewProjMat, float4(ClipSpace, 1.0)); \
								float3 WorldDir = normalize(HPos.xyz / HPos.w - camera); \
								float earthR = bottom_radius; \
								float3 earthO = float3(0.0, 0.0, -earthR); \
								float3 camPos = camera + float3(0, 0, earthR); \
								float3 sunDir = sun_direction; \
								float3 SunLuminance = 0.0; \
								if (dot(WorldDir, sunDir) > cos(0.5*0.505*3.14159 / 180.0)) \
								{ \
									SunLuminance = 100.0; /*Using any value here because everything is relative anyway in this simulation*/ \
								} \
								float shadow_length = 0.0f; \



struct PixelOutputStruct
{
	float3 ScatTransfert : SV_TARGET0;
	float3 Transmittance : SV_TARGET1;
};
PixelOutputStruct RenderCameraVolumesPS(GeometryOutput Input)
{
	PixelOutputStruct PixelOutput;

	SKYATMOSPHERE_HEADER

	const float t = float(Input.sliceId) * 2;
	float3 WorldPos = camPos + t * WorldDir;

	float3 Transmittance = 0.0f;
	float3 IlluminanceToLuminanceTransfer = GetSkyRadianceToPoint(Parameters, TransmittanceLUT, ScatteringLUT, ScatteringLUT, camPos, WorldPos, shadow_length, sunDir, Transmittance);
	
	PixelOutput.ScatTransfert = IlluminanceToLuminanceTransfer;
	PixelOutput.Transmittance = Transmittance;
	return PixelOutput;
}



float4 RenderWithLutsPS(VertexOutput Input) : SV_TARGET
{
	SKYATMOSPHERE_HEADER

	// Compute earth intersection and luminance

	float3 SunTransmittance = 0.0f;
	float3 transmittance = 0.0f;
	float3 WorldPos = camPos + 100000.0f * WorldDir;

	float t = raySphereIntersect(camera, WorldDir, earthO, earthR);
	WorldPos = t > 0.0 ? camPos + t * WorldDir : WorldPos;

	ClipSpace.z = ViewDepthTexture[pixPos].r;
	if (ClipSpace.z < 1.0f)
	{
		float4 DepthBufferWorldPos = mul(gSkyInvViewProjMat, float4(ClipSpace,1.0));
		DepthBufferWorldPos /= DepthBufferWorldPos.w;

		float tDepth = length(DepthBufferWorldPos.xyz - camera); // not camPos, no earth offset
		if (t < 0.0f || tDepth < t)
		{
			t = tDepth;
		}
	}


	float3 SunIlluminanceToGroundLuminanceTransfer = 0;
	float3 SunIlluminanceToSkyLuminanceTransfer = 0;
	if (t > 0.0)
	{
		WorldPos = camPos + t * WorldDir;
		SunIlluminanceToGroundLuminanceTransfer = GetSkyRadianceToPoint(Parameters, TransmittanceLUT, ScatteringLUT, ScatteringLUT, camPos, WorldPos, shadow_length, sunDir, transmittance);
	}
	else
	{
		SunIlluminanceToSkyLuminanceTransfer = GetSkyRadiance(Parameters, TransmittanceLUT, ScatteringLUT, ScatteringLUT, camPos, WorldDir, shadow_length, sunDir, transmittance);
		SunTransmittance = transmittance;
	}

	// Compute in scattering and apply transmittance on background
	float3 Luminance = gSunIlluminance * (SunIlluminanceToSkyLuminanceTransfer + SunIlluminanceToGroundLuminanceTransfer) +
		(1.0 - gScreenshotCaptureActive) * SunLuminance * SunTransmittance;

	return float4(Luminance, 1.0 - dot(transmittance, float3(0.33, 0.33, 0.34))); // we should use dual blending here for correctness
}


