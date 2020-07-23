// Copyright Epic Games, Inc. All Rights Reserved.


#include "Game.h"

#include "windows.h"

#include <imgui.h>



void Game::renderTransmittanceLutPS()
{
	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	GPU_SCOPED_TIMEREVENT(TransLUT, 230, 230, 76);

	D3dViewport LutViewPort = { 0.0f, 0.0f, float(LutsInfo.TRANSMITTANCE_TEXTURE_WIDTH), float(LutsInfo.TRANSMITTANCE_TEXTURE_HEIGHT), 0.0f, 1.0f };
	context->RSSetViewports(1, &LutViewPort);

	const uint32* initialCount = 0;
	context->OMSetRenderTargetsAndUnorderedAccessViews(1, &mTransmittanceTex->mRenderTargetView, nullptr, 0, 0, nullptr, initialCount);

	// Set null input assembly and layout
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(nullptr);

	// Final view
	mScreenVertexShader->setShader(*context);
	RenderTransmittanceLutPS->setShader(*context);

	context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
	context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
	context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);

	context->Draw(3, 0);
	g_dx11Device->setNullPsResources(context);
	g_dx11Device->setNullRenderTarget(context);
}



void Game::renderNewMultiScattTexPS()
{
	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	GPU_SCOPED_TIMEREVENT(NewMultiScatCS, 230, 230, 76);

	auto SetCommon = [&]()
	{
		context->CSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->CSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);

		context->CSSetSamplers(0, 1, &SamplerLinear->mSampler);
		context->CSSetSamplers(1, 1, &SamplerShadow->mSampler);

		context->CSSetShaderResources(1, 1, &mBlueNoise2dTex->mShaderResourceView);
		context->CSSetShaderResources(2, 1, &mTransmittanceTex->mShaderResourceView);

		context->CSSetShaderResources(4, 1, &mBackBufferDepth->mShaderResourceView);	// Clear out depth
		context->CSSetShaderResources(5, 1, &mShadowMap->mShaderResourceView);			// Clear out shadow map
	};

	auto DispatchCS = [&](UINT w, UINT h)
	{
		uint32 DispatchSizeX = w;// divRoundUp(w, 8);
		uint32 DispatchSizeY = h;// divRoundUp(h, 8);
		uint32 DispatchSizeZ = 1;
		context->Dispatch(DispatchSizeX, DispatchSizeY, DispatchSizeZ);
	};

	g_dx11Device->setNullVsResources(context);
	g_dx11Device->setNullPsResources(context);
	g_dx11Device->setNullRenderTarget(context);
	g_dx11Device->setNullCsResources(context);
	g_dx11Device->setNullCsUnorderedAccessViews(context);

	NewMuliScattLutCS->setShader(*context);
	SetCommon();
	context->CSSetUnorderedAccessViews(0, 1, &MultiScattTex->mUnorderedAccessView, nullptr);
	DispatchCS(MultiScattStep0Tex->mDesc.Width, MultiScattStep0Tex->mDesc.Height);

	g_dx11Device->setNullCsResources(context);
	g_dx11Device->setNullCsUnorderedAccessViews(context);
}


void Game::renderSkyViewLut()
{
	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	GPU_SCOPED_TIMEREVENT(SkyViewLut, 230, 230, 76);

	D3dViewport LutViewPort = { 0.0f, 0.0f, float(mSkyViewLutTex->mDesc.Width), float(mSkyViewLutTex->mDesc.Height), 0.0f, 1.0f };
	context->RSSetViewports(1, &LutViewPort);

	const uint32* initialCount = 0;
	context->OMSetRenderTargetsAndUnorderedAccessViews(1, &mSkyViewLutTex->mRenderTargetView, nullptr, 0, 0, nullptr, initialCount);

	// Set null input assembly and layout
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(nullptr);

	// Final view
	mScreenVertexShader->setShader(*context);
	SkyViewLutPS[currentMultipleScatteringFactor>0.0f ? 1 : 0]->setShader(*context);

	context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
	context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
	context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);

	context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);
	context->PSSetSamplers(1, 1, &SamplerShadow->mSampler);

	context->PSSetShaderResources(1, 1, &mBlueNoise2dTex->mShaderResourceView);
	context->PSSetShaderResources(2, 1, &mTransmittanceTex->mShaderResourceView);

	context->PSSetShaderResources(4, 1, &mBackBufferDepth->mShaderResourceView);	// Clear out depth
	context->PSSetShaderResources(5, 1, &mShadowMap->mShaderResourceView);			// Clear out shadow map

	context->PSSetShaderResources(6, 1, &MultiScattTex->mShaderResourceView);

	context->Draw(3, 0);
	g_dx11Device->setNullPsResources(context);
	g_dx11Device->setNullRenderTarget(context);
}



void Game::renderPathTracing()
{
	const bool enableGroundGI = AtmosphereInfos.ground_albedo.x != 0 || AtmosphereInfos.ground_albedo.y != 0 || AtmosphereInfos.ground_albedo.z != 0;
	int GroundGiPermutation = enableGroundGI ? GroundGlobalIlluminationEnabled : GroundGlobalIlluminationDisabled;
	const bool GameMode = false;
	const D3dViewport& backBufferViewport = g_dx11Device->getBackBufferViewport();
	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	D3dRenderTargetView* backBuffer = g_dx11Device->getBackBufferRT();

	mConstantBufferCPU.gResolution[0] = GameMode ? uint32(mFrameAtmosphereBuffer->mDesc.Width) : uint32(mPathTracingLuminanceBuffer->mDesc.Width);
	mConstantBufferCPU.gResolution[1] = GameMode ? uint32(mFrameAtmosphereBuffer->mDesc.Height) : uint32(mPathTracingLuminanceBuffer->mDesc.Height);
	mConstantBuffer->update(mConstantBufferCPU);

	D3dViewport LutViewPort = { 0.0f, 0.0f, float(mConstantBufferCPU.gResolution[0]), float(mConstantBufferCPU.gResolution[1]), 0.0f, 1.0f };

	//////////
	////////// Render using path tracing
	//////////
	context->RSSetViewports(1, &backBufferViewport);
	{
		GPU_SCOPED_TIMEREVENT(RenderPathTracing, 255, 255, 200);

		GpuDebugState& gds = mUpdateDebugState ? mDebugState : mDummyDebugState;
		D3dUnorderedAccessView* const uavs[2] = { gds.gpuDebugLineBufferUAV, gds.gpuDebugLineDispatchIndUAV };
		UINT const uavInitCounts[2] = { -1, -1 };
		if (GameMode)
		{
			context->OMSetRenderTargetsAndUnorderedAccessViews(1, &mFrameAtmosphereBuffer->mRenderTargetView, nullptr, 2, 2, uavs, uavInitCounts);
			context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);
		}
		else
		{
			D3dRenderTargetView* const rtvs[2] = { mPathTracingLuminanceBuffer->mRenderTargetView, mPathTracingTransmittanceBuffer->mRenderTargetView };
			context->OMSetRenderTargetsAndUnorderedAccessViews(2, rtvs, nullptr, 2, 2, uavs, uavInitCounts);
			context->OMSetBlendState(BlendAddRGBA->mState, nullptr, 0xffffffff);
		}
		context->OMSetDepthStencilState(mDisabledDepthStencilState->mState, 0);

		// Set null input assembly and layout
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);

		// Final view
		mScreenVertexShader->setShader(*context);
		RenderPathTracingPS[currentTransPermutation][GroundGiPermutation][currentShadowPermutation ? 1 : 0][currentMultipleScatteringFactor>0.0f ? 1 : 0]->setShader(*context);

		context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);

		context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);
		context->PSSetSamplers(1, 1, &SamplerShadow->mSampler);

		context->PSSetShaderResources(1, 1, &mBlueNoise2dTex->mShaderResourceView);
		context->PSSetShaderResources(2, 1, &mTransmittanceTex->mShaderResourceView);

		context->PSSetShaderResources(4, 1, &mBackBufferDepth->mShaderResourceView);
		context->PSSetShaderResources(5, 1, &mShadowMap->mShaderResourceView);

		context->PSSetShaderResources(6, 1, &MultiScattTex->mShaderResourceView);

		context->Draw(3, 0);
		g_dx11Device->setNullPsResources(context);
		g_dx11Device->setNullRenderTarget(context);
		context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);
	}
}



void Game::renderRayMarching()
{
	const D3dViewport& backBufferViewport = g_dx11Device->getBackBufferViewport();
	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	D3dRenderTargetView* backBuffer = g_dx11Device->getBackBufferRT();

	mConstantBufferCPU.gResolution[0] = uint32(mBackBufferHdr->mDesc.Width);
	mConstantBufferCPU.gResolution[1] = uint32(mBackBufferHdr->mDesc.Height);
	mConstantBuffer->update(mConstantBufferCPU);

	D3dViewport LutViewPort = { 0.0f, 0.0f, float(mConstantBufferCPU.gResolution[0]), float(mConstantBufferCPU.gResolution[1]), 0.0f, 1.0f };

	//////////
	////////// Render using path tracing
	//////////
	context->RSSetViewports(1, &LutViewPort);
	{
		GPU_SCOPED_TIMEREVENT(RenderRayMarching, 255, 255, 200);

		int fastAerialPersp = currentAerialPerspective ? 1 : 0;
		int ColoredTransmittance = currentColoredTransmittance && !fastAerialPersp ? 1 : 0;

		GpuDebugState& gds = mUpdateDebugState ? mDebugState : mDummyDebugState;
		D3dUnorderedAccessView* const uavs[2] = { gds.gpuDebugLineBufferUAV, gds.gpuDebugLineDispatchIndUAV };
		UINT const uavInitCounts[2] = { -1, -1 };
		context->OMSetRenderTargetsAndUnorderedAccessViews(1, &mBackBufferHdr->mRenderTargetView, nullptr, 2, 2, uavs, uavInitCounts);
		context->OMSetDepthStencilState(mDisabledDepthStencilState->mState, 0);

		if (ColoredTransmittance)
		{
			context->OMSetBlendState(BlendLuminanceTransmittance->mState, nullptr, 0xffffffff);
		}
		else
		{
			context->OMSetBlendState(BlendPreMutlAlpha->mState, nullptr, 0xffffffff);
		}

		// Set null input assembly and layout
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);

		// Final view
		mScreenVertexShader->setShader(*context);
		RenderRayMarchingPS[currentMultipleScatteringFactor>0.0f ? 1 : 0][currentFastSky ? 1 : 0]
			[ColoredTransmittance][fastAerialPersp][currentShadowPermutation ? 1 : 0]->setShader(*context);

		context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);

		context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);
		context->PSSetSamplers(1, 1, &SamplerShadow->mSampler);

		context->PSSetShaderResources(1, 1, &mBlueNoise2dTex->mShaderResourceView);
		context->PSSetShaderResources(2, 1, &mTransmittanceTex->mShaderResourceView);
		context->PSSetShaderResources(3, 1, &mSkyViewLutTex->mShaderResourceView);

		context->PSSetShaderResources(4, 1, &mBackBufferDepth->mShaderResourceView);
		context->PSSetShaderResources(5, 1, &mShadowMap->mShaderResourceView);

		context->PSSetShaderResources(6, 1, &MultiScattTex->mShaderResourceView);
		context->PSSetShaderResources(7, 1, &AtmosphereCameraScatteringVolume->mShaderResourceView);
		

		context->Draw(3, 0);
		g_dx11Device->setNullPsResources(context);
		g_dx11Device->setNullRenderTarget(context);
		context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);
	}
}



void Game::RenderSkyAtmosphereOverOpaque()
{
	const D3dViewport& backBufferViewport = g_dx11Device->getBackBufferViewport();
	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	D3dRenderTargetView* backBuffer = g_dx11Device->getBackBufferRT();

	mConstantBufferCPU.gResolution[0] = uint32(backBufferViewport.Width);
	mConstantBufferCPU.gResolution[1] = uint32(backBufferViewport.Height);
	mConstantBuffer->update(mConstantBufferCPU);

	const bool enableGroundGI = AtmosphereInfos.ground_albedo.x != 0 || AtmosphereInfos.ground_albedo.y != 0 || AtmosphereInfos.ground_albedo.z != 0;
	int GroundGiPermutation = enableGroundGI ? GroundGlobalIlluminationEnabled : GroundGlobalIlluminationDisabled;

	//////////
	////////// Render using path tracing
	//////////
	context->RSSetViewports(1, &backBufferViewport);
	{
		GPU_SCOPED_TIMEREVENT(RenderSkyAtmosphereOverOpaque, 230, 230, 76);

		GpuDebugState& gds = mUpdateDebugState ? mDebugState : mDummyDebugState;
		D3dUnorderedAccessView* const uavs[2] = { gds.gpuDebugLineBufferUAV, gds.gpuDebugLineDispatchIndUAV };
		UINT const uavInitCounts[2] = { -1, -1 };
		context->OMSetRenderTargetsAndUnorderedAccessViews(1, &mBackBufferHdr->mRenderTargetView, mBackBufferDepth->mDepthStencilView, 0, 0, nullptr, nullptr);
		context->OMSetDepthStencilState(mDisabledDepthStencilState->mState, 0);
		context->OMSetBlendState(BlendLuminanceTransmittance->mState, nullptr, 0xffffffff);

		// Set null input assembly and layout
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);

		// Final view
		mScreenVertexShader->setShader(*context);
		mApplySkyAtmosphereShader->setShader(*context);

		context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);

		context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);

		context->PSSetShaderResources(2, 1, &mPathTracingLuminanceBuffer->mShaderResourceView);
		context->PSSetShaderResources(3, 1, &mPathTracingTransmittanceBuffer->mShaderResourceView);

		context->Draw(3, 0);
		g_dx11Device->setNullPsResources(context);
		g_dx11Device->setNullRenderTarget(context);
		context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);
	}
}


void Game::generateSkyAtmosphereCameraVolumeWithRayMarch()
{
	mConstantBufferCPU.gResolution[0] = AtmosphereCameraScatteringVolume->mDesc.Width;
	mConstantBufferCPU.gResolution[1] = AtmosphereCameraScatteringVolume->mDesc.Height;
	mConstantBuffer->update(mConstantBufferCPU);

	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	GPU_SCOPED_TIMEREVENT(CameraVolumes, 177, 34, 76);

	const uint32* initialCount = 0;
	D3dRenderTargetView* RtViews[1] = { AtmosphereCameraScatteringVolume->mRenderTargetView };
	context->OMSetRenderTargetsAndUnorderedAccessViews(1, RtViews, nullptr, 0, 0, nullptr, initialCount);

	D3dViewport CameraVolumeViewPort = { 0,0,32,32,0.0f,1.0f };
	context->RSSetViewports(1, &CameraVolumeViewPort);

	// Set null input assembly and layout
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(nullptr);

	// Final view
	mScreenVertexShader->setShader(*context);
	GeometryGS->setShader(*context);
	CameraVolumesRayMarchPS[currentMultipleScatteringFactor>0.0f ? 1 : 0]->setShader(*context);

	context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
	context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
	context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);

	context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);
	context->PSSetSamplers(1, 1, &SamplerShadow->mSampler);

	context->PSSetShaderResources(1, 1, &mBlueNoise2dTex->mShaderResourceView);
	context->PSSetShaderResources(2, 1, &mTransmittanceTex->mShaderResourceView);
	context->PSSetShaderResources(3, 1, &mSkyViewLutTex->mShaderResourceView);

	context->PSSetShaderResources(4, 1, &mBackBufferDepth->mShaderResourceView);
	context->PSSetShaderResources(5, 1, &mShadowMap->mShaderResourceView);

	context->PSSetShaderResources(6, 1, &MultiScattTex->mShaderResourceView);

	context->DrawInstanced(3, AtmosphereCameraScatteringVolume->mDesc.Depth, 0, 0);
	context->GSSetShader(nullptr, nullptr, 0);
	g_dx11Device->setNullPsResources(context);
	g_dx11Device->setNullRenderTarget(context);
}

