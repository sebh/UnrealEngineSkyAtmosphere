// Copyright Epic Games, Inc. All Rights Reserved.


#include "Game.h"

#include "windows.h"

#include <imgui.h>



void Game::generateSkyAtmosphereLUTs()
{
	D3dRenderContext* context = g_dx11Device->getDeviceContext();

	D3dViewport LutViewPort = { 0,0,1,1,0.0f,1.0f };

	// 
	{
		GPU_SCOPED_TIMEREVENT(TransmittanceLUT, 177, 34, 76);

		LutViewPort.Width = float(LutsInfo.TRANSMITTANCE_TEXTURE_WIDTH);
		LutViewPort.Height = float(LutsInfo.TRANSMITTANCE_TEXTURE_HEIGHT);
		context->RSSetViewports(1, &LutViewPort);

		const uint32* initialCount = 0;
		context->OMSetRenderTargetsAndUnorderedAccessViews(1, &LUTs.TransmittanceTex->mRenderTargetView, nullptr, 0, 0, nullptr, initialCount);

		// Set null input assembly and layout
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);

		// Final view
		mScreenVertexShader->setShader(*context);
		TransmittanceLutPS->setShader(*context);

		context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);

		context->Draw(3, 0);
		g_dx11Device->setNullPsResources(context);
		g_dx11Device->setNullRenderTarget(context);
	}

	// 
	{
		GPU_SCOPED_TIMEREVENT(DirectIrradianceLUT, 177, 34, 76);

		LutViewPort.Width = float(LutsInfo.IRRADIANCE_TEXTURE_WIDTH);
		LutViewPort.Height = float(LutsInfo.IRRADIANCE_TEXTURE_HEIGHT);
		context->RSSetViewports(1, &LutViewPort);

		const uint32* initialCount = 0;
		D3dRenderTargetView* RtViews[2] = { TempLUTs.DeltaIrradianceTex->mRenderTargetView, LUTs.IrradianceTex->mRenderTargetView };
		context->OMSetRenderTargetsAndUnorderedAccessViews(2, RtViews, nullptr, 0, 0, nullptr, initialCount);

		// Set null input assembly and layout
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);

		// Final view
		mScreenVertexShader->setShader(*context);
		DirectIrradianceLutPS->setShader(*context);

		context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);

		context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);
		context->PSSetShaderResources(0, 1, &LUTs.TransmittanceTex->mShaderResourceView);

		context->Draw(3, 0);
		g_dx11Device->setNullPsResources(context);
		g_dx11Device->setNullRenderTarget(context);
	}

	SkyAtmosphereSideConstantBufferStructure SideConstants;
	memset(&SideConstants, 0xBA, sizeof(SkyAtmosphereSideConstantBufferStructure));
	{
		SideConstants.ScatteringOrder = 1;
		SideConstants.LuminanceFromRadiance = float4x4(1.0f,0.0f,0.0f,0.0f, 0.0f,1.0f,0.0f,0.0f, 0.0f,0.0f,1.0f,0.0f, 0.0f,0.0f,0.0f,1.0f);
		SideConstants.UseSingleMieScatteringTexture = 0;	// Going without separated single mie scattering for now.
		SkyAtmosphereSideBuffer->update(SideConstants);
	}

	// 
	{
		GPU_SCOPED_TIMEREVENT(DirectScatteringLUT, 177, 34, 76);

		LutViewPort.Width = float(LutsInfo.SCATTERING_TEXTURE_WIDTH);
		LutViewPort.Height = float(LutsInfo.SCATTERING_TEXTURE_HEIGHT);
		context->RSSetViewports(1, &LutViewPort);
	
		const uint32* initialCount = 0;
		D3dRenderTargetView* RtViews[4] = { TempLUTs.DeltaRaleighScatteringTex->mRenderTargetView, TempLUTs.DeltaMieScatteringTex->mRenderTargetView, LUTs.ScatteringTex->mRenderTargetView, nullptr};
		const uint32 RtCount = 3;
		context->OMSetRenderTargetsAndUnorderedAccessViews(RtCount, RtViews, nullptr, 0, 0, nullptr, initialCount);

		// Set null input assembly and layout
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);

		// Final view
		mScreenVertexShader->setShader(*context);
		GeometryGS->setShader(*context);
		SingleScatteringLutPS->setShader(*context);

		context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);
		context->PSSetConstantBuffers(2, 1, &SkyAtmosphereSideBuffer->mBuffer);

		context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);
		context->PSSetShaderResources(0, 1, &LUTs.TransmittanceTex->mShaderResourceView);

		context->DrawInstanced(3, LUTs.ScatteringTex->mDesc.Depth, 0, 0);
		context->GSSetShader(nullptr, nullptr, 0);
		g_dx11Device->setNullPsResources(context);
		g_dx11Device->setNullRenderTarget(context);
	}



	// Compute the 2nd, 3rd and 4th order of scattering, in sequence.
	{
		GPU_SCOPED_TIMEREVENT(MultiScatLutLoop, 177, 34, 76);

		for (int ScatteringOrder = 2; ScatteringOrder <= NumScatteringOrder; ++ScatteringOrder)
		{
			GPU_SCOPED_EVENT(MultiScatIteration);

			{
				// Update the side constants buffer
				SideConstants.ScatteringOrder = ScatteringOrder;
				SkyAtmosphereSideBuffer->update(SideConstants);
			}

			// Compute the scattering density, and store it in delta_scattering_density_texture.
			{
				//GPU_SCOPED_TIMEREVENT(ScatteringDensityLUT, 177, 34, 76);

				LutViewPort.Width = float(LutsInfo.SCATTERING_TEXTURE_WIDTH);
				LutViewPort.Height = float(LutsInfo.SCATTERING_TEXTURE_HEIGHT);
				context->RSSetViewports(1, &LutViewPort);

				const uint32* initialCount = 0;
				context->OMSetRenderTargetsAndUnorderedAccessViews(1, &TempLUTs.DeltaScatteringDensityTex->mRenderTargetView, nullptr, 0, 0, nullptr, initialCount);

				// Set null input assembly and layout
				context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				context->IASetInputLayout(nullptr);

				// Final view
				mScreenVertexShader->setShader(*context);
				GeometryGS->setShader(*context);
				ScatteringDensityLutPS->setShader(*context);

				context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
				context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
				context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);
				context->PSSetConstantBuffers(2, 1, &SkyAtmosphereSideBuffer->mBuffer);

				context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);
				context->PSSetShaderResources(0, 1, &LUTs.TransmittanceTex->mShaderResourceView);
				context->PSSetShaderResources(1, 1, &TempLUTs.DeltaIrradianceTex->mShaderResourceView);
				context->PSSetShaderResources(2, 1, &TempLUTs.DeltaRaleighScatteringTex->mShaderResourceView);
				context->PSSetShaderResources(3, 1, &TempLUTs.DeltaMieScatteringTex->mShaderResourceView);
				context->PSSetShaderResources(4, 1, &TempLUTs.DeltaRaleighScatteringTex->mShaderResourceView); // Because GLuint delta_multiple_scattering_texture = delta_rayleigh_scattering_texture;

				context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);
				context->DrawInstanced(3, TempLUTs.DeltaScatteringDensityTex->mDesc.Depth, 0, 0);
				context->GSSetShader(nullptr, nullptr, 0);
				g_dx11Device->setNullPsResources(context);
				g_dx11Device->setNullRenderTarget(context);
			}

			{
				// Update the side constants buffer with ScatteringOrder - 1. See Bruneton2017.
				SideConstants.ScatteringOrder = ScatteringOrder - 1;
				SkyAtmosphereSideBuffer->update(SideConstants);
			}

			// Compute the indirect irradiance, store it in delta_irradiance_texture and accumulate it in irradiance_texture_.
			{
				//GPU_SCOPED_TIMEREVENT(IndirectIrradianceLUT, ScatteringOrder, 177, 34, 76);

				LutViewPort.Width = float(LutsInfo.IRRADIANCE_TEXTURE_WIDTH);
				LutViewPort.Height = float(LutsInfo.IRRADIANCE_TEXTURE_WIDTH);
				context->RSSetViewports(1, &LutViewPort);

				const uint32* initialCount = 0;
				D3dRenderTargetView* RtViews[2] = { TempLUTs.DeltaIrradianceTex->mRenderTargetView, LUTs.IrradianceTex->mRenderTargetView };
				context->OMSetRenderTargetsAndUnorderedAccessViews(2, RtViews, nullptr, 0, 0, nullptr, initialCount);

				// Set null input assembly and layout
				context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				context->IASetInputLayout(nullptr);

				// Final view
				mScreenVertexShader->setShader(*context);
				IndirectIrradianceLutPS->setShader(*context);

				context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
				context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
				context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);
				context->PSSetConstantBuffers(2, 1, &SkyAtmosphereSideBuffer->mBuffer);

				context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);
				context->PSSetShaderResources(0, 1, &TempLUTs.DeltaRaleighScatteringTex->mShaderResourceView);
				context->PSSetShaderResources(1, 1, &TempLUTs.DeltaMieScatteringTex->mShaderResourceView);
				context->PSSetShaderResources(2, 1, &TempLUTs.DeltaRaleighScatteringTex->mShaderResourceView); // Because GLuint delta_multiple_scattering_texture = delta_rayleigh_scattering_texture;

				context->OMSetBlendState(Blend0Nop1Add->mState, nullptr, 0xffffffff);
				context->Draw(3, 0);
				context->GSSetShader(nullptr, nullptr, 0);
				g_dx11Device->setNullPsResources(context);
				g_dx11Device->setNullRenderTarget(context);
			}

			{
				// Update the side constants buffer
				SideConstants.ScatteringOrder = ScatteringOrder;
				SkyAtmosphereSideBuffer->update(SideConstants);
			}

			// Compute the multiple scattering, store it in delta_multiple_scattering_texture, and accumulate it in scattering_texture_.
			{
				//GPU_SCOPED_TIMEREVENT(MultipleScatteringLUT, ScatteringOrder, 177, 34, 76);

				LutViewPort.Width = float(LutsInfo.SCATTERING_TEXTURE_WIDTH);
				LutViewPort.Height = float(LutsInfo.SCATTERING_TEXTURE_HEIGHT);
				context->RSSetViewports(1, &LutViewPort);

				const uint32* initialCount = 0;
				D3dRenderTargetView* RtViews[2] = { TempLUTs.DeltaRaleighScatteringTex->mRenderTargetView, LUTs.ScatteringTex->mRenderTargetView }; // Because GLuint delta_multiple_scattering_texture = delta_rayleigh_scattering_texture;
				context->OMSetRenderTargetsAndUnorderedAccessViews(2, RtViews, nullptr, 0, 0, nullptr, initialCount);

				// Set null input assembly and layout
				context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				context->IASetInputLayout(nullptr);

				// Final view
				mScreenVertexShader->setShader(*context);
				GeometryGS->setShader(*context);
				MultipleScatteringLutPS->setShader(*context);

				context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
				context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
				context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);
				context->PSSetConstantBuffers(2, 1, &SkyAtmosphereSideBuffer->mBuffer);

				context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);
				context->PSSetShaderResources(0, 1, &LUTs.TransmittanceTex->mShaderResourceView);
				context->PSSetShaderResources(1, 1, &TempLUTs.DeltaScatteringDensityTex->mShaderResourceView);

				context->OMSetBlendState(Blend0Nop1Add->mState, nullptr, 0xffffffff);
				context->DrawInstanced(3, LUTs.ScatteringTex->mDesc.Depth, 0, 0);
				context->GSSetShader(nullptr, nullptr, 0);
				g_dx11Device->setNullPsResources(context);
				g_dx11Device->setNullRenderTarget(context);
			}
		}
	}

	context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);
}


void Game::generateSkyAtmosphereCameraVolumes()
{
	mConstantBufferCPU.gResolution[0] = AtmosphereCameraScatteringVolume->mDesc.Width;
	mConstantBufferCPU.gResolution[1] = AtmosphereCameraScatteringVolume->mDesc.Height;
	mConstantBuffer->update(mConstantBufferCPU);

	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	GPU_SCOPED_TIMEREVENT(CameraVolumes, 177, 34, 76);

	const uint32* initialCount = 0;
	D3dRenderTargetView* RtViews[2] = { AtmosphereCameraScatteringVolume->mRenderTargetView, AtmosphereCameraTransmittanceVolume->mRenderTargetView };
	context->OMSetRenderTargetsAndUnorderedAccessViews(2, RtViews, nullptr, 0, 0, nullptr, initialCount);

	D3dViewport CameraVolumeViewPort = { 0,0,32,32,0.0f,1.0f };
	context->RSSetViewports(1, &CameraVolumeViewPort);

	// Set null input assembly and layout
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(nullptr);

	// Final view
	mScreenVertexShader->setShader(*context);
	GeometryGS->setShader(*context);
	CameraVolumesPS->setShader(*context);

	context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
	context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
	context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);

	context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);
	context->PSSetShaderResources(0, 1, &LUTs.TransmittanceTex->mShaderResourceView);
	context->PSSetShaderResources(1, 1, &LUTs.IrradianceTex->mShaderResourceView);
	context->PSSetShaderResources(2, 1, &LUTs.ScatteringTex->mShaderResourceView);

	context->DrawInstanced(3, AtmosphereCameraScatteringVolume->mDesc.Depth, 0, 0);
	context->GSSetShader(nullptr, nullptr, 0);
	g_dx11Device->setNullPsResources(context);
	g_dx11Device->setNullRenderTarget(context);
}

void Game::renderSkyAtmosphereUsingLUTs()
{
	const D3dViewport& backBufferViewport = g_dx11Device->getBackBufferViewport();
	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	D3dRenderTargetView* backBuffer = g_dx11Device->getBackBufferRT();

	mConstantBufferCPU.gResolution[0] = uint32(backBufferViewport.Width);
	mConstantBufferCPU.gResolution[1] = uint32(backBufferViewport.Height);
	mConstantBuffer->update(mConstantBufferCPU);

	//////////
	////////// Render using the LUTs
	//////////
	context->RSSetViewports(1, &backBufferViewport);
	{
		GPU_SCOPED_TIMEREVENT(RenderWithLUTs, 177, 34, 76);

		const uint32* initialCount = 0;
		context->OMSetRenderTargetsAndUnorderedAccessViews(1, &mBackBufferHdr->mRenderTargetView, nullptr, 0, 0, nullptr, nullptr);
		context->OMSetDepthStencilState(mDisabledDepthStencilState->mState, 0);
		context->OMSetBlendState(BlendPreMutlAlpha->mState, nullptr, 0xffffffff);

		// Set null input assembly and layout
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);

		// Final view
		mScreenVertexShader->setShader(*context);
		RenderWithLutPS->setShader(*context);

		context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(1, 1, &SkyAtmosphereBuffer->mBuffer);

		context->PSSetSamplers(0, 1, &SamplerLinear->mSampler);
		context->PSSetShaderResources(0, 1, &LUTs.TransmittanceTex->mShaderResourceView);
		context->PSSetShaderResources(1, 1, &LUTs.IrradianceTex->mShaderResourceView);
		context->PSSetShaderResources(2, 1, &LUTs.ScatteringTex->mShaderResourceView);
		context->PSSetShaderResources(3, 1, &AtmosphereCameraScatteringVolume->mShaderResourceView);
		context->PSSetShaderResources(4, 1, &AtmosphereCameraTransmittanceVolume->mShaderResourceView);

		context->PSSetShaderResources(5, 1, &mBackBufferDepth->mShaderResourceView);

		context->Draw(3, 0);
		g_dx11Device->setNullPsResources(context);
		g_dx11Device->setNullRenderTarget(context);
		context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);
	}
}



