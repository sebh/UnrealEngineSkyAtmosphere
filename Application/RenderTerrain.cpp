// Copyright Epic Games, Inc. All Rights Reserved.


#include "Game.h"

#include "windows.h"

#include <imgui.h>

// This is the resolution of the terrain. Render tile by tile using instancing... Bad but it works and this is not important for what I need to do.
const static uint32 TerrainResolution = 512;

void Game::renderTerrain()
{
	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	GPU_SCOPED_TIMEREVENT(Terrain, 76, 34, 177);

	if (!RenderTerrain)
		return;

	const D3dViewport& backBufferViewport = g_dx11Device->getBackBufferViewport();
	D3dRenderTargetView* backBuffer = g_dx11Device->getBackBufferRT();

	mConstantBufferCPU.gViewProjMat = mViewProjMat;
	mConstantBufferCPU.gTerrainResolution = TerrainResolution;
	mConstantBufferCPU.gResolution[0] = uint32(backBufferViewport.Width);
	mConstantBufferCPU.gResolution[1] = uint32(backBufferViewport.Height);
	mConstantBuffer->update(mConstantBufferCPU);

	context->RSSetViewports(1, &backBufferViewport);

	{
		context->OMSetRenderTargetsAndUnorderedAccessViews(1, &mBackBufferHdr->mRenderTargetView, mBackBufferDepth->mDepthStencilView, 0, 0, nullptr, nullptr);
		context->OMSetDepthStencilState(mDefaultDepthStencilState->mState, 0);
		context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);

		// Set null input assembly and layout
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);

		// Final view
		mTerrainVertexShader->setShader(*context);
		mTerrainPixelShader->setShader(*context);

		context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->VSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);
		context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);

		context->VSSetSamplers(0, 1, &SamplerLinear->mSampler);
		context->VSSetShaderResources(0, 1, &mTerrainHeightmapTex->mShaderResourceView);
		context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);
		context->PSSetSamplers(1, 1, &SamplerShadow->mSampler);
		context->PSSetShaderResources(0, 1, &mTerrainHeightmapTex->mShaderResourceView);
		context->PSSetShaderResources(1, 1, &mShadowMap->mShaderResourceView);
		context->PSSetShaderResources(2, 1, &mTransmittanceTex->mShaderResourceView);

		context->DrawInstanced(6, TerrainResolution*TerrainResolution, 0, 0);
		g_dx11Device->setNullPsResources(context);
		g_dx11Device->setNullRenderTarget(context);
		context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);
	}
}



void Game::renderShadowmap()
{
	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	GPU_SCOPED_TIMEREVENT(Shadowmap, 76, 34, 177);

	if (!RenderTerrain)
		return;

	D3dViewport Viewport;
	Viewport.TopLeftX = Viewport.TopLeftY = 0;
	Viewport.Width = Viewport.Height = float(ShadowmapSize);
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;

	mConstantBufferCPU.gViewProjMat = mShadowmapViewProjMat;
	mConstantBufferCPU.gTerrainResolution = TerrainResolution;
	mConstantBufferCPU.gResolution[0] = uint32(Viewport.Width);
	mConstantBufferCPU.gResolution[1] = uint32(Viewport.Height);
	mConstantBuffer->update(mConstantBufferCPU);

	context->RSSetViewports(1, &Viewport);

	{
		context->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, mShadowMap->mDepthStencilView, 0, 0, nullptr, nullptr);
		context->OMSetDepthStencilState(mDefaultDepthStencilState->mState, 0);
		context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);
		context->RSSetState(mShadowRasterizerState->mState);

		// Set null input assembly and layout
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);

		// Final view
		mTerrainVertexShader->setShader(*context);
		context->PSSetShader(nullptr, nullptr, 0);

		context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->VSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);
		context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);

		context->VSSetSamplers(0, 1, &SamplerLinear->mSampler);
		context->VSSetShaderResources(0, 1, &mTerrainHeightmapTex->mShaderResourceView);

		context->DrawInstanced(6, TerrainResolution*TerrainResolution, 0, 0);
		g_dx11Device->setNullPsResources(context);
		g_dx11Device->setNullRenderTarget(context);
		context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);
		context->RSSetState(mDefaultRasterizerState->mState);
	}
}

