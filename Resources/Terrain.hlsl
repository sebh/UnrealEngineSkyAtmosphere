// Copyright Epic Games, Inc. All Rights Reserved.


#include "./Resources/SkyAtmosphereCommon.hlsl"


// Terrain shader code is a shame but it does what it needs to in the end.


Texture2D<float4>  TerrainHeightmapTex				: register(t0);
Texture2D<float4>  ShadowmapTexture					: register(t1);
Texture2D<float4>  TransmittanceLutTexture			: register(t2);

struct TerrainVertexOutput
{
	float4 position		: SV_POSITION;
	float4 Uvs			: TEXCOORD0;
	float4 WorldPos		: TEXCOORD1;
	float3 color		: COLOR;
	float3 normal		: TEXCOORD2;
};

float4 SampleTerrain(in float quadx, in float quady, in float3 qp)
{
	const float terrainWidth = 100.0f;	// 100 km edge
	const float maxTerrainHeight = 100.0f;
	const float quadWidth = terrainWidth / gTerrainResolution;

	float2 Uvs = (float2(quadx, quady) + qp.xy) / gTerrainResolution;
#if 0
	const float height = TerrainHeightmapTex.SampleLevel(samplerLinearClamp, Uvs, 0).r;
#else
	const float offset = 0.0008;
	float HeightAccum = TerrainHeightmapTex.SampleLevel(samplerLinearClamp, Uvs + float2(0.0f, 0.0f), 0).r;
	HeightAccum+= TerrainHeightmapTex.SampleLevel(samplerLinearClamp, Uvs + float2( offset, 0.0f), 0).r;
	HeightAccum+= TerrainHeightmapTex.SampleLevel(samplerLinearClamp, Uvs + float2(-offset, 0.0f), 0).r;
	HeightAccum+= TerrainHeightmapTex.SampleLevel(samplerLinearClamp, Uvs + float2( 0.0f, offset), 0).r;
	HeightAccum+= TerrainHeightmapTex.SampleLevel(samplerLinearClamp, Uvs + float2( 0.0f,-offset), 0).r;
	const float height = HeightAccum / 5;
#endif
	float4 WorldPos = float4((float3(quadx, quady, maxTerrainHeight * height) + qp) * quadWidth - 0.5f * float3(terrainWidth, terrainWidth, 0.0), 1.0f);
	WorldPos.xyz += float3(-terrainWidth * 0.45, 0.4*terrainWidth, -0.0f);	// offset to position view
	return WorldPos;
}

TerrainVertexOutput TerrainVertexShader(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
	TerrainVertexOutput output = (TerrainVertexOutput)0;

	uint quadId = instanceId;
	uint vertId = vertexId;

	// For a range on screen in [-0.5,0.5]
	float3 qp = 0.0f;
	qp = vertId == 1 || vertId == 4 ? float3(1.0f, 0.0f, 0.0f) : qp;
	qp = vertId == 2 || vertId == 3 ? float3(0.0f, 1.0f, 0.0f) : qp;
	qp = vertId == 5 ? float3(1.0f, 1.0f, 0.0f) : qp;

	const float TerrainResolutionInv = 1.0 / float(gTerrainResolution);
	const float quadx = quadId / gTerrainResolution;
	const float quady = quadId % gTerrainResolution;

	float2 Uvs = (float2(quadx, quady) + qp.xy) / gTerrainResolution;	
	float4 WorldPos = SampleTerrain(quadx, quady, qp);


	output.WorldPos = WorldPos;
	output.Uvs.xy = Uvs;
	output.position = mul(gViewProjMat, WorldPos);

	output.color = 0.05 * (1.0 - gScreenshotCaptureActive);

	{
		const float offset = 5.0;
		float4 WorldPos0_ = SampleTerrain(quadx + qp.x - offset, quady + qp.y, 0.0f);
		float4 WorldPos1_ = SampleTerrain(quadx + qp.x + offset, quady + qp.y, 0.0f);
		float4 WorldPos_0 = SampleTerrain(quadx + qp.x,     quady + qp.y - offset, 0.0f);
		float4 WorldPos_1 = SampleTerrain(quadx + qp.x,     quady + qp.y + offset, 0.0f);
		output.normal = cross(normalize(WorldPos1_.xyz - WorldPos0_.xyz), normalize(WorldPos_1.xyz - WorldPos_0.xyz));
		output.normal = normalize(output.normal);
	}

	return output;
}

float4 TerrainPixelShader(TerrainVertexOutput input) : SV_TARGET
{
	float4 shadowUv = mul(gShadowmapViewProjMat, input.WorldPos / input.WorldPos.w);
	//shadowUv /= shadowUv.w;	// not needed as it is an ortho projection
	shadowUv.x = shadowUv.x*0.5 + 0.5;
	shadowUv.y =-shadowUv.y*0.5 + 0.5;

	float NoL = max(0.0, dot(sun_direction, normalize(input.normal)));
	float sunShadow = ShadowmapTexture.SampleCmpLevelZero(samplerShadow, shadowUv.xy, shadowUv.z);
#if 1
	// Bad hard coded shadow PCF filter
	float cnt = 1.0f;
	for (float u = -3.0; u <= 3.0f; u += 1.0f)
	{
		for (float v = -3.0; v <= 3.0f; v += 1.0f)
		{
			float offsetx = u*0.0001;
			float offsety = v*0.0001;
			sunShadow += ShadowmapTexture.SampleCmpLevelZero(samplerShadow, shadowUv.xy + float2(-offsetx, -offsety), shadowUv.z);
			cnt++;
		}
	}
	sunShadow /= cnt;
#endif

	// Second evaluate transmittance due to participating media
	AtmosphereParameters Atmosphere = GetAtmosphereParameters();
	float3 P0 = input.WorldPos.xyz / input.WorldPos.w + float3(0, 0, Atmosphere.BottomRadius);
	float viewHeight = length(P0);
	const float3 UpVector = P0 / viewHeight;
	float viewZenithCosAngle = dot(sun_direction, UpVector);
	float2 uv;
	LutTransmittanceParamsToUv(Atmosphere, viewHeight, viewZenithCosAngle, uv);
	const float3 trans = TransmittanceLutTexture.SampleLevel(samplerLinearClamp, uv, 0).rgb;


	return float4(input.color * sunShadow * NoL * trans, 1);
}


