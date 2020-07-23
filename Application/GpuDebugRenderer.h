// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Math.h"
#include <vector>

#include "DirectXMath.h"
using namespace DirectX;

struct GpuDebugState
{
	RenderBuffer* gpuDebugLineBuffer;
	D3dShaderResourceView* gpuDebugLineBufferSRV;
	D3dUnorderedAccessView* gpuDebugLineBufferUAV;
	RenderBuffer* gpuDebugLineDispatchInd;
	D3dUnorderedAccessView* gpuDebugLineDispatchIndUAV;
};


void gpuDebugSystemCreate();
void gpuDebugSystemRelease();

void gpuDebugStateCreate(GpuDebugState& gds);
void gpuDebugStateDestroy(GpuDebugState& gds);

void gpuDebugStateFrameInit(GpuDebugState& gds, bool clearDebugData = true);
void gpuDebugStateDraw(GpuDebugState& gds, XMMATRIX& viewProjMatrix);


