// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "DX11Base/Dx11Device.h"
#include "GpuDebugRenderer.h"




static ComputeShader* g_gpuDebugClearCs = nullptr;
static VertexShader* g_gpuDebugRenderLineVs = nullptr;
static PixelShader*  g_gpuDebugRenderLinePS = nullptr;

struct GpuDebugRenderConstant
{
	XMMATRIX viewProjMatrix;
};
typedef ConstantBuffer<GpuDebugRenderConstant> GpuDebugRenderConstantBuffer;
static GpuDebugRenderConstantBuffer* g_gpuDebugConstantBuffer = nullptr;


void gpuDebugSystemCreate()
{
	resetPtr(&g_gpuDebugConstantBuffer);
	g_gpuDebugConstantBuffer = new GpuDebugRenderConstantBuffer();

	reload(&g_gpuDebugClearCs, L"Resources\\GpuDebugPrimitives.hlsl", "csGpuDebugClearIndBuffer", true, nullptr, true);
	reload(&g_gpuDebugRenderLineVs, L"Resources\\GpuDebugPrimitives.hlsl", "vsRenderGpuDebugLine", true, nullptr, true);
	reload(&g_gpuDebugRenderLinePS, L"Resources\\GpuDebugPrimitives.hlsl", "psRenderGpuDebugLine", true, nullptr, true);
}

void gpuDebugSystemRelease()
{
	resetPtr(&g_gpuDebugConstantBuffer);
	resetPtr(&g_gpuDebugClearCs);
	resetPtr(&g_gpuDebugRenderLineVs);
	resetPtr(&g_gpuDebugRenderLinePS);
}

void gpuDebugStateCreate(GpuDebugState& gds)
{
	{
		;
		const uint32 maxLineCount = 128 * 1024; // See GpuDebugPrimitives.hlsl
		const uint32 structureByteStride = sizeof(float)*4*4;
		const uint32 sizeInBytes = structureByteStride * maxLineCount;
		D3D11_BUFFER_DESC debugLineBufferDesc = RenderBuffer::initBufferDesc_uav(sizeInBytes);
		debugLineBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		debugLineBufferDesc.StructureByteStride = structureByteStride;
		gds.gpuDebugLineBuffer = new RenderBuffer(debugLineBufferDesc);

		CD3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc(D3D11_UAV_DIMENSION_BUFFER);
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = maxLineCount;
		HRESULT hr = g_dx11Device->getDevice()->CreateUnorderedAccessView(gds.gpuDebugLineBuffer->mBuffer, &uavDesc, &gds.gpuDebugLineBufferUAV);
		ATLASSERT(hr == S_OK);

		CD3D11_SHADER_RESOURCE_VIEW_DESC srvDesc(D3D11_SRV_DIMENSION_BUFFER);
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = maxLineCount;
		hr = g_dx11Device->getDevice()->CreateShaderResourceView(gds.gpuDebugLineBuffer->mBuffer, &srvDesc, &gds.gpuDebugLineBufferSRV);
		ATLASSERT(hr == S_OK);
	}
	{
		D3D11_BUFFER_DESC debugLineBufferIndDesc = RenderBuffer::initBufferDesc_uav(4 * sizeof(uint32));
		debugLineBufferIndDesc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		gds.gpuDebugLineDispatchInd = new RenderBuffer(debugLineBufferIndDesc);

		CD3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc(D3D11_UAV_DIMENSION_BUFFER);
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = 4;
		HRESULT hr = g_dx11Device->getDevice()->CreateUnorderedAccessView(gds.gpuDebugLineDispatchInd->mBuffer, &uavDesc, &gds.gpuDebugLineDispatchIndUAV);
		ATLASSERT(hr == S_OK);
	}
}

void gpuDebugStateDestroy(GpuDebugState& gds)
{
	resetComPtr(&gds.gpuDebugLineBufferSRV);
	resetComPtr(&gds.gpuDebugLineBufferUAV);
	resetComPtr(&gds.gpuDebugLineDispatchIndUAV);
	resetPtr(&gds.gpuDebugLineBuffer);
	resetPtr(&gds.gpuDebugLineDispatchInd);
}

void gpuDebugStateFrameInit(GpuDebugState& gds, bool clearDebugData)
{
	auto context = g_dx11Device->getDeviceContext();
	Dx11Device::setNullRenderTarget(context);
	Dx11Device::setNullPsResources(context);
	Dx11Device::setNullVsResources(context);

	if (clearDebugData)
	{
		g_gpuDebugClearCs->setShader(*context);
		context->CSSetUnorderedAccessViews(0, 1, &gds.gpuDebugLineDispatchIndUAV, nullptr);
		context->Dispatch(1, 1, 1);
	}

	Dx11Device::setNullCsResources(context);
	Dx11Device::setNullCsUnorderedAccessViews(context);
}

void gpuDebugStateDraw(GpuDebugState& gds, XMMATRIX& viewProjMatrix)
{
	auto context = g_dx11Device->getDeviceContext();
	GpuDebugRenderConstant debugRenderConstant;
	debugRenderConstant.viewProjMatrix = viewProjMatrix;
	g_gpuDebugConstantBuffer->update(debugRenderConstant);

	// Set vertex buffer stride and offset.
	context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	context->VSSetConstantBuffers(0, 1, &g_gpuDebugConstantBuffer->mBuffer);

	// Set the vertex and pixel shaders that will be used to render this triangle.
	g_gpuDebugRenderLineVs->setShader(*context);
	g_gpuDebugRenderLinePS->setShader(*context);
	context->IASetInputLayout(nullptr);

	context->VSSetShaderResources(0, 1, &gds.gpuDebugLineBufferSRV);

	// vertex tris per hair instance
	context->DrawInstancedIndirect(gds.gpuDebugLineDispatchInd->mBuffer, 0);

	g_dx11Device->setNullVsResources(context);
}



