// Copyright Epic Games, Inc. All Rights Reserved.


#include "./Resources/Common.hlsl"



[numthreads(8, 8, 1)]
void ToyShaderCS(uint3 threadId : SV_DispatchThreadID)
{
	uint2 texCoord = threadId.xy;

	rwTexture2d[texCoord] = float4(texCoord / float2(gResolution), 0.0, 1.0);;
}


