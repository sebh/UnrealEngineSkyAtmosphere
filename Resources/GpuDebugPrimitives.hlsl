// Copyright Epic Games, Inc. All Rights Reserved.


struct GpuDebugLine
{
	float4 worldPos0;
	float4 worldPos1;
	float4 colorAlpha0;
	float4 colorAlpha1;
};



#ifdef GPUDEBUG_CLIENT


// append line
RWStructuredBuffer<GpuDebugLine>	gpuDebugLineBuffer					: register(GPU_DEBUG_LINEBUFFER_UAV);
RWBuffer<uint>						gpuDebugLineDispatchIndBuffer		: register(GPU_DEBUG_LINEDISPATCHIND_UAV);

void addGpuDebugLine(GpuDebugLine debugLine)
{
	const uint maxLineCount = 128 * 1024; // See GpuDebugRenderer.cpp

	uint newNodeSlot;
	InterlockedAdd(gpuDebugLineDispatchIndBuffer[1], 1, newNodeSlot);
	if(newNodeSlot<maxLineCount)
		gpuDebugLineBuffer[newNodeSlot] = debugLine;
	else
		InterlockedAdd(gpuDebugLineDispatchIndBuffer[1], -1, newNodeSlot);	// go back to a safe line count
}


void addGpuDebugLine(float3 p0, float3 p1, float3 c)
{
	GpuDebugLine gdl;
	gdl.colorAlpha0 = gdl.colorAlpha1 = float4(c, 1.0);
	gdl.worldPos0 = float4( p0 ,1.0);
	gdl.worldPos1 = float4( p1 ,1.0);
	addGpuDebugLine(gdl);
}

void addGpuDebugCross(float3 p, float3 c, float r)
{
	GpuDebugLine gdl;
	gdl.colorAlpha0 = gdl.colorAlpha1 = float4(c, 1.0);
	gdl.worldPos0 = float4( p + float3(-r,0,0) ,1.0);
	gdl.worldPos1 = float4( p + float3( r,0,0) ,1.0);
	addGpuDebugLine(gdl);
	gdl.worldPos0 = float4( p + float3(0,-r,0) ,1.0);
	gdl.worldPos1 = float4( p + float3(0, r,0) ,1.0);
	addGpuDebugLine(gdl);
	gdl.worldPos0 = float4( p + float3(0,0,-r) ,1.0);
	gdl.worldPos1 = float4( p + float3(0,0, r) ,1.0);
	addGpuDebugLine(gdl);
}

#else


RWBuffer<uint>						gpuDebugLineDispatchIndBufferUav		: register(u0);

StructuredBuffer<GpuDebugLine>		gpuDebugLineBuffer						: register(t0);


// clear indirect dispatch buffer
[numthreads(1, 1, 1)]
void csGpuDebugClearIndBuffer(uint3 position : SV_DispatchThreadID)
{
	gpuDebugLineDispatchIndBufferUav[0] = 2;
	gpuDebugLineDispatchIndBufferUav[1] = 0;
	gpuDebugLineDispatchIndBufferUav[2] = 0;
	gpuDebugLineDispatchIndBufferUav[3] = 0;
}


cbuffer Constants : register(b0)
{
	float4x4 g_viewProjMatrix;
}

struct VertexOutput
{
	float4 position		: SV_POSITION;
	float4 colorAlpha	: TEXCOORD0;
};

// Render debug lines
VertexOutput vsRenderGpuDebugLine(
	uint vertexID: SV_VertexID,
	uint instanceID : SV_InstanceID
)
{
	VertexOutput output;

	GpuDebugLine debugLine = gpuDebugLineBuffer[instanceID];

	float4 vertexPosition = vertexID == 0 ? debugLine.worldPos0   : debugLine.worldPos1;
	float4 vertexColor    = vertexID == 0 ? debugLine.colorAlpha0 : debugLine.colorAlpha1;

	output.position  = mul(g_viewProjMatrix, vertexPosition);
	output.colorAlpha= vertexColor;

	return output;
}

float4 psRenderGpuDebugLine(VertexOutput input) : SV_TARGET
{
	return input.colorAlpha;
}

#endif



