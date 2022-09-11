// Copyright Epic Games, Inc. All Rights Reserved.


#include "Game.h"
#include "GpuDebugRenderer.h"

#include "windows.h"

#include <imgui.h>

#undef max
#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>


Game::Game()
{
	memset(&AtmosphereInfosSaved, 0, sizeof(AtmosphereInfos));
	SetupEarthAtmosphere(AtmosphereInfos);

	// Offset along Z, looking towards Y
	mCamPos = {0.0, 0.0f, 0.0f};
	mViewDir = { 0.0, 1.0f, 0.0f };
}

Game::~Game()
{
}


void Game::loadShaders(bool firstTimeLoadShaders)
{
	auto GetStringNumber = [](int i)
	{
		switch (i)
		{
		case 0: return "0";
		case 1: return "1";
		case 2: return "2";
		case 3: return "3";
		default:
			ATLASSERT(false);
		}
		return "-1";
	};

	bool success = true;

	auto reloadShader = [&](auto** previousShader, const TCHAR* filename, const char* entryFunction, bool firstTimeLoadShaders, const Macros* macros = NULL, bool lazyCompilation = false)
	{
		if (firstTimeLoadShaders)
		{
			// The first time we want to compile the shader and make sure we fail if not succesful
			return reload(previousShader, filename, entryFunction, true, macros, lazyCompilation);
		}
		else
		{
			// other time we only want to make the shader dirty to schedule compilation when used
			(*previousShader)->markDirty();
			return true;
		}
	};

	const bool lazyCompilation = true;
	success &= reloadShader(&mVertexShader, L"Resources\\Common.hlsl", "DefaultVertexShader", firstTimeLoadShaders, nullptr, false);				// No lazy compilation because it is used to create a layout
	success &= reloadShader(&mScreenVertexShader, L"Resources\\Common.hlsl", "ScreenTriangleVertexShader", firstTimeLoadShaders, nullptr, false);	// No lazy compilation because it is used to create a layout
	success &= reload(&mPostProcessShader, L"Resources\\PostProcess.hlsl", "PostProcessPS", firstTimeLoadShaders, nullptr, lazyCompilation);
	success &= reload(&mApplySkyAtmosphereShader, L"Resources\\PostProcess.hlsl", "ApplySkyAtmospherePS", firstTimeLoadShaders, nullptr, lazyCompilation);

	success &= reload(&GeometryGS, L"Resources\\Common.hlsl", "LutGS", firstTimeLoadShaders, nullptr, lazyCompilation);

	success &= reload(&TransmittanceLutPS, L"Resources\\SkyAtmosphereTransmittanceLut.hlsl", "TransmittanceLutPS", firstTimeLoadShaders, nullptr, lazyCompilation);
	success &= reload(&DirectIrradianceLutPS, L"Resources\\SkyAtmosphereDirectIrradianceLut.hlsl", "DirectIrradianceLutPS", firstTimeLoadShaders, nullptr, lazyCompilation);
	success &= reload(&SingleScatteringLutPS, L"Resources\\SkyAtmosphereSingleScatteringLut.hlsl", "SingleScatteringLutPS", firstTimeLoadShaders, nullptr, lazyCompilation);
	success &= reload(&ScatteringDensityLutPS, L"Resources\\SkyAtmosphereScatteringDensity.hlsl", "ScatteringDensityLutPS", firstTimeLoadShaders, nullptr, lazyCompilation);
	success &= reload(&IndirectIrradianceLutPS, L"Resources\\SkyAtmosphereIndirectIrradiance.hlsl", "IndirectIrradianceLutPS", firstTimeLoadShaders, nullptr, lazyCompilation);
	success &= reload(&MultipleScatteringLutPS, L"Resources\\SkyAtmosphereMultipleScattering.hlsl", "MultipleScatteringLutPS", firstTimeLoadShaders, nullptr, lazyCompilation);

	{
		Macros macros;
		ShaderMacro illuMode = { "ILLUMINANCE_IS_ONE", GetStringNumber(1) };
		macros.push_back(illuMode);
		success &= reload(&NewMuliScattLutCS, L"Resources\\RenderSkyRayMarching.hlsl", "NewMultiScattCS", firstTimeLoadShaders, &macros, lazyCompilation); 
	}

	success &= reload(&CameraVolumesPS, L"Resources\\RenderWithLuts.hlsl", "RenderCameraVolumesPS", firstTimeLoadShaders, nullptr, lazyCompilation);
	
	success &= reload(&RenderWithLutPS, L"Resources\\RenderWithLuts.hlsl", "RenderWithLutsPS", firstTimeLoadShaders, nullptr, lazyCompilation);	
	success &= reload(&RenderTransmittanceLutPS, L"Resources\\RenderSkyRayMarching.hlsl", "RenderTransmittanceLutPS", firstTimeLoadShaders, nullptr, lazyCompilation);

	for (int ds = MultiScatApproxDisabled; ds < MultiScatApproxCount; ++ds)
	{
		Macros macros;
		ShaderMacro macroDs = { "MULTISCATAPPROX_ENABLED", GetStringNumber(ds) };
		macros.push_back(macroDs);
		success &= reload(&SkyViewLutPS[ds], L"Resources\\RenderSkyRayMarching.hlsl", "SkyViewLutPS", firstTimeLoadShaders, &macros, lazyCompilation);
	}

	success &= reload(&mTerrainVertexShader, L"Resources\\Terrain.hlsl", "TerrainVertexShader", firstTimeLoadShaders, nullptr, lazyCompilation);
	success &= reload(&mTerrainPixelShader, L"Resources\\Terrain.hlsl", "TerrainPixelShader", firstTimeLoadShaders, nullptr, lazyCompilation);

	for (int trans = TransmittanceMethodDeltaTracking; trans < TransmittanceMethodCount; ++trans)
	{
		for (int ggi = GroundGlobalIlluminationDisabled; ggi < GroundGlobalIlluminationCount; ++ggi)
		{
			for (int sm = ShadowmapDisabled; sm < ShadowmapCount; ++sm)
			{
				for (int ds = MultiScatApproxDisabled; ds < MultiScatApproxCount; ++ds)
				{
					Macros macros;
					ShaderMacro macroTrans = { "TRANSMITANCE_METHOD", GetStringNumber(trans) };
					ShaderMacro macroGgi = { "GROUND_GI_ENABLED", GetStringNumber(ggi) };
					ShaderMacro macroSm = { "SHADOWMAP_ENABLED", GetStringNumber(sm) };
					ShaderMacro macroDs = { "MULTISCATAPPROX_ENABLED", GetStringNumber(ds) };
					macros.push_back(macroTrans);
					macros.push_back(macroGgi);
					macros.push_back(macroSm);
					macros.push_back(macroDs);
					success &= reload(&RenderPathTracingPS[trans][ggi][sm][ds], L"Resources\\RenderSkyPathTracing.hlsl", "RenderPathTracingPS", firstTimeLoadShaders, &macros, lazyCompilation);
				}
			}
		}
	}

	for (int ds = MultiScatApproxDisabled; ds < MultiScatApproxCount; ++ds)
	{
		for (int fs = FastSkyDisabled; fs < FastSkyCount; ++fs)
		{
			for (int ct = ColoredTransmittanceDisabled; ct < ColoredTransmittanceCount; ++ct)
			{
				for (int fap = FastAerialPerspectiveDisabled; fap < FastAerialPerspectiveCount; ++fap)
				{
					for (int sm = ShadowmapDisabled; sm < ShadowmapCount; ++sm)
					{
						if (fap == FastAerialPerspectiveEnabled && ct == ColoredTransmittanceEnabled)
							continue;

						Macros macros;
						ShaderMacro macroDs = { "MULTISCATAPPROX_ENABLED", GetStringNumber(ds) };
						ShaderMacro macroFs = { "FASTSKY_ENABLED", GetStringNumber(fs) };
						ShaderMacro macroCt = { "COLORED_TRANSMITTANCE_ENABLED", GetStringNumber(ct) };
						ShaderMacro macroFap = { "FASTAERIALPERSPECTIVE_ENABLED", GetStringNumber(fap) };
						ShaderMacro macroSm = { "SHADOWMAP_ENABLED", GetStringNumber(sm) };
						macros.push_back(macroDs);
						macros.push_back(macroFs);
						macros.push_back(macroCt);
						macros.push_back(macroFap);
						macros.push_back(macroSm);
						success &= reload(&RenderRayMarchingPS[ds][fs][ct][fap][sm], L"Resources\\RenderSkyRayMarching.hlsl", "RenderRayMarchingPS", firstTimeLoadShaders, &macros, lazyCompilation);
					}
				}
			}
		}
	}

	for (int ds = MultiScatApproxDisabled; ds < MultiScatApproxCount; ++ds)
	{
		Macros macros;
		ShaderMacro macroDs = { "MULTISCATAPPROX_ENABLED", GetStringNumber(ds) };
		macros.push_back(macroDs);
		success &= reload(&CameraVolumesRayMarchPS[ds], L"Resources\\RenderSkyRayMarching.hlsl", "RenderCameraVolumePS", firstTimeLoadShaders, &macros, lazyCompilation);
	}

	InputLayoutDesc inputLayout;
	appendSimpleVertexDataToInputLayout(inputLayout, "POSITION", DXGI_FORMAT_R32G32B32_FLOAT);
	resetComPtr(&mLayout);
	mVertexShader->createInputLayout(inputLayout, &mLayout);	// Have a layout object with vertex stride in it

	ShouldClearPathTracedBuffer = true;
}

void Game::releaseShaders()
{
	resetComPtr(&mLayout);

	resetPtr(&mVertexShader);
	resetPtr(&mScreenVertexShader);

	resetPtr(&mPostProcessShader);
	resetPtr(&mApplySkyAtmosphereShader);

	resetPtr(&TransmittanceLutPS);
	resetPtr(&DirectIrradianceLutPS);
	resetPtr(&SingleScatteringLutPS);
	resetPtr(&ScatteringDensityLutPS);
	resetPtr(&IndirectIrradianceLutPS);
	resetPtr(&MultipleScatteringLutPS);

	resetPtr(&NewMuliScattLutCS);

	resetPtr(&CameraVolumesPS);

	resetPtr(&RenderWithLutPS);
	resetPtr(&RenderTransmittanceLutPS);

	for (int ds = MultiScatApproxDisabled; ds < MultiScatApproxCount; ++ds)
	{
		resetPtr(&SkyViewLutPS[ds]);
	}

	resetPtr(&mTerrainVertexShader);
	resetPtr(&mTerrainPixelShader);

	for (int trans = TransmittanceMethodDeltaTracking; trans < TransmittanceMethodCount; ++trans)
	{
		for (int ggi = GroundGlobalIlluminationDisabled; ggi < GroundGlobalIlluminationCount; ++ggi)
		{
			for (int sm = ShadowmapDisabled; sm < ShadowmapCount; ++sm)
			{
				for (int ds = MultiScatApproxDisabled; ds < MultiScatApproxCount; ++ds)
				{
					resetPtr(&RenderPathTracingPS[trans][ggi][sm][ds]);
				}
			}
		}
	}

	for (int ds = MultiScatApproxDisabled; ds < MultiScatApproxCount; ++ds)
	{
		for (int fs = FastSkyDisabled; fs < FastSkyCount; ++fs)
		{
			for (int ct = ColoredTransmittanceDisabled; ct < ColoredTransmittanceCount; ++ct)
			{
				for (int fap = FastAerialPerspectiveDisabled; fap < FastAerialPerspectiveCount; ++fap)
				{
					for (int sm = ShadowmapDisabled; sm < ShadowmapCount; ++sm)
					{
						resetPtr(&RenderRayMarchingPS[ds][fs][ct][fap][sm]);
					}
				}
			}
		}
	}

	for (int ds = MultiScatApproxDisabled; ds < MultiScatApproxCount; ++ds)
	{
		resetPtr(&CameraVolumesRayMarchPS[ds]);
	}

	resetPtr(&GeometryGS);
}


void Game::initialise()
{
	gpuDebugSystemCreate();
	gpuDebugStateCreate(mDebugState);
	gpuDebugStateCreate(mDummyDebugState);

	////////// Load and compile shaders

	loadShaders(true);

	////////// Create other resources

	D3dDevice* device = g_dx11Device->getDevice();
	const D3dViewport& viewport = g_dx11Device->getBackBufferViewport();
	allocateResolutionIndependentResources();
	allocateResolutionDependentResources(uint32(viewport.Width), uint32(viewport.Height));
}



//////////////////////////////////////////////////////////////////////////

auto createTexture2dFromExr = [&](const char *filename)
{
	const char* exrErrorStr = nullptr;

	float* rgba = nullptr;
	int width = -1;
	int height = -1;
	const char *err = nullptr;
	int exrError = LoadEXR(&rgba, &width, &height, filename, &err);
	ATLASSERT(exrError == TINYEXR_SUCCESS);
	D3D11_TEXTURE2D_DESC texDesc = Texture2D::initDefault(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, false, false);

	D3D11_SUBRESOURCE_DATA initData;
	initData.pSysMem = rgba;
	initData.SysMemPitch = width * sizeof(float) * 4;
	initData.SysMemSlicePitch = 0;

	Texture2D* tex = new Texture2D(texDesc, &initData);
	free(rgba);
	return tex;
};

void Game::saveBackBufferHdr(const char* filepath)
{
	const D3D11_TEXTURE2D_DESC& desc = mBackBufferHdrStagingTexture->mDesc;
	ATLASSERT(desc.Format == DXGI_FORMAT_R32G32B32A32_FLOAT);

	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT res = context->Map(mBackBufferHdrStagingTexture->mTexture, 0, D3D11_MAP_READ, 0, &mappedResource); // this will stall since we do not have proper staging textures to hide frame latency
	ATLASSERT(res == S_OK);
	if (res == S_OK)
	{
		const float* data = (float*)mappedResource.pData;
		const int pixelCount = mBackBufferHdr->mDesc.Width * mBackBufferHdr->mDesc.Height;
		float* dataResolved = new float[pixelCount * 4];
		for (int p = 0; p < pixelCount; p++)
		{
			int i = p * 4;
			float sampleCount = data[i + 3];
			sampleCount = sampleCount > 0.0f ? sampleCount : 1.0f;
			dataResolved[i] = data[i] / sampleCount;
			dataResolved[i + 1] = data[i + 1] / sampleCount;
			dataResolved[i + 2] = data[i + 2] / sampleCount;
			dataResolved[i + 3] = 1.0f;
		}

		const char *err = nullptr;
		SaveEXR(dataResolved, mBackBufferHdr->mDesc.Width, mBackBufferHdr->mDesc.Height, 4, 0, filepath, &err);
		if (err != nullptr)
		{
			OutputDebugStringA(err);
		}
		context->Unmap(mBackBufferHdrStagingTexture->mTexture, 0);
		delete[] dataResolved;
	}
}

//////////////////////////////////////////////////////////////////////////



void Game::reallocateResolutionDependent(uint32 newWidth, uint32 newHeight)
{
	releaseResolutionDependentResources();
	allocateResolutionDependentResources(newWidth, newHeight);
}


void Game::allocateResolutionIndependentResources()
{
	D3dDevice* device = g_dx11Device->getDevice();

	// Simple triangle geometry
	VertexType vertices[3];
	vertices[0] = { { 0.0f, 0.0f, 0.0f } };
	vertices[1] = { { 0.0f, 0.5f, 0.0f } };
	vertices[2] = { { 0.5f, 0.0f, 0.0f } };
	uint32 indices[3];
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	vertexBuffer = new RenderBuffer(RenderBuffer::initVertexBufferDesc_default(sizeof(vertices)), vertices);
	indexBuffer = new RenderBuffer(RenderBuffer::initIndexBufferDesc_default(sizeof(indices)), indices);

	mConstantBuffer = new CommonConstantBuffer();

	uint32 bufferElementSize = (sizeof(float) * 4);
	uint32 bufferElementCount = 1024;
	D3dBufferDesc someBufferDesc = { bufferElementCount * bufferElementSize , D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 0, 0, 0 };
	mSomeBuffer = new RenderBuffer(someBufferDesc);

	D3dUnorderedAccessViewDesc someBufferUavViewDesc;
	someBufferUavViewDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	someBufferUavViewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	someBufferUavViewDesc.Buffer.FirstElement = 0;
	someBufferUavViewDesc.Buffer.NumElements = bufferElementCount;
	someBufferUavViewDesc.Buffer.Flags = 0; // D3D11_BUFFER_UAV_FLAG_RAW
	HRESULT hr = device->CreateUnorderedAccessView(mSomeBuffer->mBuffer, &someBufferUavViewDesc, &mSomeBufferUavView);
	ATLASSERT(hr == S_OK);

	mDefaultDepthStencilState = new DepthStencilState(DepthStencilState::initDefaultDepthOnStencilOff());
	D3dDepthStencilDesc DisabledDepthTest = DepthStencilState::initDepthNoWriteStencilOff();
	DisabledDepthTest.DepthEnable = false;
	mDisabledDepthStencilState = new DepthStencilState(DisabledDepthTest);
	mDefaultRasterizerState = new RasterizerState(RasterizerState::initDefaultState());
	mDefaultBlendState = new BlendState(BlendState::initDisabledState());

	D3dRasterizerDesc RSDesc = RasterizerState::initDefaultState();
	RSDesc.CullMode = D3D11_CULL_NONE;
	RSDesc.DepthBias = 5.0;
	RSDesc.SlopeScaledDepthBias = 5.0;
	mShadowRasterizerState = new RasterizerState(RSDesc);

	SamplerLinear = new SamplerState(SamplerState::initLinearClamp());
	SamplerShadow = new SamplerState(SamplerState::initShadowCmpClamp());

	SkyAtmosphereBuffer = new SkyAtmosphereConstantBuffer();
	SkyAtmosphereSideBuffer = new SkyAtmosphereSideConstantBuffer();

	D3dBlendDesc Blend0Nop1AddDesc = BlendState::initDisabledState();
	Blend0Nop1AddDesc.IndependentBlendEnable = TRUE;
	Blend0Nop1AddDesc.RenderTarget[1].BlendEnable = TRUE;
	Blend0Nop1AddDesc.RenderTarget[1].SrcBlend = D3D11_BLEND_ONE;
	Blend0Nop1AddDesc.RenderTarget[1].DestBlend = D3D11_BLEND_ONE;
	Blend0Nop1AddDesc.RenderTarget[1].BlendOp = D3D11_BLEND_OP_ADD;
	Blend0Nop1AddDesc.RenderTarget[1].SrcBlendAlpha = D3D11_BLEND_ONE;
	Blend0Nop1AddDesc.RenderTarget[1].DestBlendAlpha = D3D11_BLEND_ONE;
	Blend0Nop1AddDesc.RenderTarget[1].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	Blend0Nop1AddDesc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	Blend0Nop1Add = new BlendState(Blend0Nop1AddDesc);

	D3dBlendDesc BlendAddRGBADesc = BlendState::initAdditiveState();
	BlendAddRGBA = new BlendState(BlendAddRGBADesc);

	D3dBlendDesc BlendPremult = BlendState::initPreMultBlendState();
	BlendPremult.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	BlendPremult.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	BlendPremult.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BlendPreMutlAlpha = new BlendState(BlendPremult);

	D3dBlendDesc BlendLMDesc = BlendState::initPreMultDualBlendState();
	BlendLuminanceTransmittance = new BlendState(BlendLMDesc);

	LUTs.Allocate(LutsInfo);
	TempLUTs.Allocate(LutsInfo);

	{
		D3dTexture3dDesc desc = Texture3D::initDefault(DXGI_FORMAT_R16G16B16A16_FLOAT, 32, 32, 32, true, true);
		AtmosphereCameraScatteringVolume = new Texture3D(desc);
		desc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
		AtmosphereCameraTransmittanceVolume = new Texture3D(desc);
	}

	mBlueNoise2dTex = createTexture2dFromExr("./Resources/bluenoise.exr");		// I do not remember where this noise texture comes from.
	mTerrainHeightmapTex = createTexture2dFromExr("./Resources/heightmap1.exr");

	D3dTexture2dDesc desc = Texture2D::initDefault(DXGI_FORMAT_R16G16B16A16_FLOAT, LutsInfo.TRANSMITTANCE_TEXTURE_WIDTH, LutsInfo.TRANSMITTANCE_TEXTURE_HEIGHT, true, true);
	mTransmittanceTex = new Texture2D(desc);

	{
		D3dTexture2dDesc descIllum = desc;
		descIllum.Width = MultiScatteringLUTRes;
		descIllum.Height = MultiScatteringLUTRes;
	//	descIllum.Format = DXGI_FORMAT_R11G11B10_FLOAT;
	//	descIllum.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		descIllum.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		MultiScattTex = new Texture2D(descIllum);
		MultiScattStep0Tex = new Texture2D(descIllum);
	}

	{
		D3dTexture2dDesc descSkyView = desc;
		descSkyView.Width = 192;
		descSkyView.Height = 108;
		descSkyView.Format = DXGI_FORMAT_R11G11B10_FLOAT;
		mSkyViewLutTex = new Texture2D(descSkyView);
	}
}

void Game::releaseResolutionIndependentResources()
{
	LUTs.Release();
	TempLUTs.Release();

	resetPtr(&mConstantBuffer);
	resetPtr(&indexBuffer);
	resetPtr(&vertexBuffer);

	resetComPtr(&mSomeBufferUavView);
	resetPtr(&mSomeBuffer);

	resetPtr(&mDefaultDepthStencilState);
	resetPtr(&mDisabledDepthStencilState);
	resetPtr(&mDefaultBlendState);
	resetPtr(&mDefaultRasterizerState);
	resetPtr(&mShadowRasterizerState);

	resetPtr(&SamplerLinear);
	resetPtr(&SamplerShadow);

	resetPtr(&SkyAtmosphereBuffer);
	resetPtr(&SkyAtmosphereSideBuffer);
	resetPtr(&Blend0Nop1Add);
	resetPtr(&BlendAddRGBA);
	resetPtr(&BlendPreMutlAlpha);
	resetPtr(&BlendLuminanceTransmittance);

	resetPtr(&AtmosphereCameraScatteringVolume);
	resetPtr(&AtmosphereCameraTransmittanceVolume);

	resetPtr(&mBlueNoise2dTex);
	resetPtr(&mTerrainHeightmapTex);

	resetPtr(&mTransmittanceTex);
	resetPtr(&MultiScattTex);
	resetPtr(&MultiScattStep0Tex);
	resetPtr(&mSkyViewLutTex);
}

void Game::allocateResolutionDependentResources(uint32 newWidth, uint32 newHeight)
{
	// DXGI_FORMAT_R32G32B32A32_FLOAT DXGI_FORMAT_R16G16B16A16_FLOAT

	mBackBufferDepth = new Texture2D(Texture2D::initDepthStencilBuffer(newWidth, newHeight, false));
#ifdef SKYHIGHQUALITY
	D3D11_TEXTURE2D_DESC backBufferHdrDesc = Texture2D::initDefault(DXGI_FORMAT_R32G32B32A32_FLOAT, newWidth, newHeight, true, true);
#else
	D3D11_TEXTURE2D_DESC backBufferHdrDesc = Texture2D::initDefault(DXGI_FORMAT_R16G16B16A16_FLOAT, newWidth, newHeight, true, true);
#endif
	mBackBufferHdr = new Texture2D(backBufferHdrDesc);
	{
		D3D11_TEXTURE2D_DESC desc = backBufferHdrDesc;
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		desc.BindFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		mBackBufferHdrStagingTexture = new Texture2D(desc);
	}
	{
		D3D11_TEXTURE2D_DESC pathTracingBufferDesc = backBufferHdrDesc;
		pathTracingBufferDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;	// Could use 111110 if there is no count when used only for blur and accumulate
		mPathTracingLuminanceBuffer = new Texture2D(pathTracingBufferDesc);
		mPathTracingTransmittanceBuffer = new Texture2D(pathTracingBufferDesc);
	}
	{
		D3D11_TEXTURE2D_DESC FrameAtmosphereDesc = backBufferHdrDesc;
		FrameAtmosphereDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		//FrameAtmosphereDesc.Width /= 2; FrameAtmosphereDesc.Height /= 2;
		mFrameAtmosphereBuffer = new Texture2D(FrameAtmosphereDesc);
	}
	mShadowMap = new Texture2D(Texture2D::initDepthStencilBuffer(ShadowmapSize, ShadowmapSize, false));
}

void Game::releaseResolutionDependentResources()
{
	resetPtr(&mBackBufferHdr);
	resetPtr(&mBackBufferDepth);
	resetPtr(&mBackBufferHdrStagingTexture);
	resetPtr(&mPathTracingLuminanceBuffer);
	resetPtr(&mFrameAtmosphereBuffer);
	resetPtr(&mPathTracingTransmittanceBuffer);
	resetPtr(&mShadowMap);
}

void Game::shutdown()
{
	gpuDebugSystemRelease();
	gpuDebugStateDestroy(mDebugState);
	gpuDebugStateDestroy(mDummyDebugState);

	////////// Release resources

	releaseResolutionIndependentResources();
	releaseResolutionDependentResources();

	////////// Release shaders

	releaseShaders();
}

void Game::update(const WindowInputData& inputData)
{
	static int previousMouseX = inputData.mInputStatus.mouseX;
	static int previousMouseY = inputData.mInputStatus.mouseY;
	for (auto& event : inputData.mInputEvents)
	{
		if (event.type == etMouseMoved && inputData.mInputStatus.keys[kShift])
		{
			int dx = inputData.mInputStatus.mouseX - previousMouseX;
			int dy = inputData.mInputStatus.mouseY - previousMouseY;
			viewPitch += -dy;
			viewYaw += -dx;
			ShouldClearPathTracedBuffer = dy != 0 || dx != 0;
			mFrameId = 0;
		}
		if (event.type == etMouseMoved && inputData.mInputStatus.keys[kControl])
		{
			int dx = inputData.mInputStatus.mouseX - previousMouseX;
			int dy = inputData.mInputStatus.mouseY - previousMouseY;
			uiSunPitch += 0.2f * float(-dy)*3.14f / 180.0f;
			uiSunYaw   += 0.2f * float(-dx)*3.14f / 180.0f;
			ShouldClearPathTracedBuffer = dy != 0 || dx != 0;
			mFrameId = 0;
		}
		if (event.type == etMouseButtonDown)
		{
			mConstantBufferCPU.gMouseLastDownPos[0] = inputData.mInputStatus.mouseX;
			mConstantBufferCPU.gMouseLastDownPos[1] = inputData.mInputStatus.mouseY;
		}
		if (event.type == etKeyDown && event.key == kC)
			takeScreenShot = true;
		if (event.type == etKeyDown && event.key == kT)
		{
			if (MethodSwitchDebug)
			{
				uiRenderingMethod = MethodPathTracing;
				currentMultipleScatteringFactor = 0.0f;
			}
			else
			{
				uiRenderingMethod = MethodRaymarching;
				currentMultipleScatteringFactor = 1.0f;
			}
			MethodSwitchDebug = !MethodSwitchDebug;
			mFrameId = 0;
		}

		if (event.type == etKeyDown && event.key == kF5)
			SaveState();
		else if (event.type == etKeyDown && event.key == kF9)
			LoadState();
	}
	previousMouseX = inputData.mInputStatus.mouseX;
	previousMouseY = inputData.mInputStatus.mouseY;


	// Listen to CTRL+S for shader live update in a very simple fashion (from http://www.lofibucket.com/articles/64k_intro.html)
	static ULONGLONG lastLoadTime = GetTickCount64();
	if (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState('S'))
	{
		const ULONGLONG tickCount = GetTickCount64();
		if (tickCount - lastLoadTime > 200)
		{
			Sleep(100);					// Wait for a while to let the file system finish the file write.
			loadShaders(false);			// Reload (all) the shaders
		}
		lastLoadTime = tickCount;
	}

	const D3dViewport& backBufferViewport = g_dx11Device->getBackBufferViewport();
	float aspectRatioXOverY = backBufferViewport.Width / backBufferViewport.Height;
	mCamPosFinal = { 0.0, 0.0f, 0.0f };

	// Camera orientation
	if (viewPitch > 80.0f) viewPitch = 80.0f;
	if (viewPitch < -80.0f) viewPitch = -80.0f;
	XMMATRIX BBB = XMMatrixRotationRollPitchYaw(-viewPitch * 3.14159f / 180.0f, viewYaw*3.14159f / 180.0f, 0.0f);
	XMStoreFloat3(&mViewDir, BBB.r[2]);
	float3 viewDirCopy = mViewDir;
	mViewDir.y = viewDirCopy.z;
	mViewDir.z = viewDirCopy.y;
	mCamPosFinal.x += mViewDir.x * uiCamForward;
	mCamPosFinal.y += mViewDir.y * uiCamForward;
	mCamPosFinal.z += mViewDir.z * uiCamForward;
	mCamPosFinal.z += uiCamHeight;

	{
		float3 focusPosition2 = float3(mCamPosFinal.x + mViewDir.x, mCamPosFinal.y + mViewDir.y, mCamPosFinal.z + mViewDir.z);
		FXMVECTOR eyePosition = XMLoadFloat3(&mCamPosFinal);
		FXMVECTOR focusPosition = XMLoadFloat3(&focusPosition2);
		FXMVECTOR upDirection = { 0.0f, 0.0f, 1.0f, 0.0f };	// Unreal z-up

		mViewMat = XMMatrixLookAtLH(eyePosition, focusPosition, upDirection);
		mProjMat = XMMatrixPerspectiveFovLH(66.6f*3.14159f / 180.0f, aspectRatioXOverY, 0.1f, 20000.0f);
		mViewProjMat = XMMatrixMultiply(mViewMat, mProjMat);
	}

	XMMATRIX sunMatrix = XMMatrixRotationRollPitchYaw(-uiSunPitch, uiSunYaw, 0.0f);
	XMStoreFloat3(&mSunDir, sunMatrix.r[2]);
	float3 tmp = mSunDir;
	mSunDir.y = tmp.z;
	mSunDir.z = tmp.y;

	{
	//	const float xOffset = -0.45 * 100.0f;
	//	const float yOffset = 0.4 * 100.0f;
		FXMVECTOR eyePosition = { 0.0f,  0.0f, 0.0f, 1.0 };
		FXMVECTOR focusPosition = { 0.0f - mSunDir.x, 0.0f - mSunDir.y, 0.0f - mSunDir.z, 0.0f};
		FXMVECTOR upDirection = { 0.0f, 0.0f, 1.0f, 0.0f };	// Unreal z-up
		XMMATRIX viewMatrix = XMMatrixLookAtLH(eyePosition, focusPosition, upDirection);

		XMMATRIX projMatrix = XMMatrixOrthographicOffCenterLH
		(
			-100.0f,	/*float ViewLeft,  */
			100.0f,		/*float ViewRight, */
			-100.0f,	/*float ViewBottom,*/
			100.0f,		/*float ViewTop,   */
			-100.0f,	/*float NearZ,	   */
			100.0f		/*float FarZ	   */
		);

		mShadowmapViewProjMat = XMMatrixMultiply(viewMatrix, projMatrix);
	}
}

auto EqualFloat3 = [](const GlslVec3& a, const GlslVec3& b) {return a.x == b.x && a.y == b.y && a.z == b.z; };
auto CreateGlslVec3 = [](float x, float y, float z) {GlslVec3 vec = { x, y, z }; return vec; };
auto length3 = [](GlslVec3& v) {return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); };
auto normalize3 = [](GlslVec3& v, float l) {GlslVec3 r; r.x = v.x / l, r.y = v.y / l, r.z = v.z / l; return r; };
auto scale3 = [](GlslVec3& v, float l) {GlslVec3 r; r.x = v.x * l, r.y = v.y * l, r.z = v.z * l; return r; };
auto sub3 = [](GlslVec3& a, GlslVec3& b) {GlslVec3 r; r.x = a.x - b.x; r.y = a.y - b.y; r.z = a.z - b.z; return r; };
auto add3 = [](GlslVec3& a, GlslVec3& b) {GlslVec3 r; r.x = a.x + b.x; r.y = a.y + b.y; r.z = a.z + b.z; return r; };
auto MaxZero3 = [](GlslVec3& a) {GlslVec3 r; r.x=a.x>0.0f?a.x:0.0f; r.y=a.y>0.0f?a.y:0.0f; r.z=a.z>0.0f?a.z:0.0f; return r; };


void Game::updateSkyAtmosphereConstant()
{
	D3dRenderContext* context = g_dx11Device->getDeviceContext();

	// Constant buffer update
	{
		SkyAtmosphereConstantBufferStructure cb;
		memset(&cb, 0xBA, sizeof(SkyAtmosphereConstantBufferStructure));

		cb.solar_irradiance = AtmosphereInfos.solar_irradiance;
		cb.sun_angular_radius = AtmosphereInfos.sun_angular_radius;
		cb.absorption_extinction = AtmosphereInfos.absorption_extinction;
		cb.mu_s_min = AtmosphereInfos.mu_s_min;

		memcpy(cb.rayleigh_density, &AtmosphereInfos.rayleigh_density, sizeof(AtmosphereInfos.rayleigh_density));
		memcpy(cb.mie_density, &AtmosphereInfos.mie_density, sizeof(AtmosphereInfos.mie_density));
		memcpy(cb.absorption_density, &AtmosphereInfos.absorption_density, sizeof(AtmosphereInfos.absorption_density));

		cb.mie_phase_function_g = AtmosphereInfos.mie_phase_function_g;
		cb.rayleigh_scattering = AtmosphereInfos.rayleigh_scattering;
		const float RayleighScatScale = 1.0f;
		cb.rayleigh_scattering.x *= RayleighScatScale;
		cb.rayleigh_scattering.y *= RayleighScatScale;
		cb.rayleigh_scattering.z *= RayleighScatScale;
		cb.mie_scattering = AtmosphereInfos.mie_scattering;
		cb.mie_absorption = MaxZero3(sub3(AtmosphereInfos.mie_extinction, AtmosphereInfos.mie_scattering));
		cb.mie_extinction = AtmosphereInfos.mie_extinction;
		cb.ground_albedo = AtmosphereInfos.ground_albedo;
		cb.bottom_radius = AtmosphereInfos.bottom_radius;
		cb.top_radius = AtmosphereInfos.top_radius;
		cb.MultipleScatteringFactor = currentMultipleScatteringFactor;
		cb.MultiScatteringLUTRes = MultiScatteringLUTRes;

		//
		cb.TRANSMITTANCE_TEXTURE_WIDTH = LutsInfo.TRANSMITTANCE_TEXTURE_WIDTH;
		cb.TRANSMITTANCE_TEXTURE_HEIGHT = LutsInfo.TRANSMITTANCE_TEXTURE_HEIGHT;
		cb.IRRADIANCE_TEXTURE_WIDTH = LutsInfo.IRRADIANCE_TEXTURE_WIDTH;
		cb.IRRADIANCE_TEXTURE_HEIGHT = LutsInfo.IRRADIANCE_TEXTURE_HEIGHT;
		cb.SCATTERING_TEXTURE_R_SIZE = LutsInfo.SCATTERING_TEXTURE_R_SIZE;
		cb.SCATTERING_TEXTURE_MU_SIZE = LutsInfo.SCATTERING_TEXTURE_MU_SIZE;
		cb.SCATTERING_TEXTURE_MU_S_SIZE = LutsInfo.SCATTERING_TEXTURE_MU_S_SIZE;
		cb.SCATTERING_TEXTURE_NU_SIZE = LutsInfo.SCATTERING_TEXTURE_NU_SIZE;
		cb.SKY_SPECTRAL_RADIANCE_TO_LUMINANCE = float3(114974.916437f, 71305.954816f, 65310.548555f); // Not used if using LUTs as transfert
		cb.SUN_SPECTRAL_RADIANCE_TO_LUMINANCE = float3(98242.786222f, 69954.398112f, 66475.012354f);  // idem

		//
		cb.gSkyViewProjMat = mViewProjMat;
		{
			XMVECTOR mViewProjMatDet = XMMatrixDeterminant(mViewProjMat);
			cb.gSkyInvViewProjMat = XMMatrixInverse(&mViewProjMatDet, mViewProjMat);
		}
		{
			XMVECTOR mProjMatDet = XMMatrixDeterminant(mProjMat);
			cb.gSkyInvProjMat = XMMatrixInverse(&mProjMatDet, mProjMat);
		}
		{
			XMVECTOR mViewMatDet = XMMatrixDeterminant(mViewMat);
			cb.gSkyInvViewMat = XMMatrixInverse(&mViewMatDet, mViewMat);
		}


		cb.gShadowmapViewProjMat = mShadowmapViewProjMat;

		cb.camera = mCamPosFinal;
		cb.view_ray = mViewDir;
		cb.sun_direction = mSunDir;

		SkyAtmosphereBuffer->update(cb);
	}
}

static float MieScatteringLength;
static GlslVec3 MieScatteringColor;
static float MieAbsLength;
static GlslVec3 MieAbsColor;
static float MieScaleHeight;

static float RayleighScatteringLength;
static GlslVec3 RayleighScatteringColor;
static float RayleighScaleHeight;

static float AbsorptionLength;
static GlslVec3 AbsorptionColor;

static float AtmosphereHeight;

static GlslVec3 uiGroundAbledo = {0.0f, 0.0f, 0.0f};
static GlslVec3 uiGroundAbledoPrev;

static float uiCamHeightPrev;
static float uiCamForwardPrev;
static float uiSunYawPrev;
static float uiSunPitchPrev;

static int uiRenderingMethodPrev = -1;
static int NumScatteringOrderPrev = 0;

static int transPermutationPrev = 0;
static bool shadowPermutationPrev = 0;
static bool RenderTerrainPrev = 0;
static float multipleScatteringFactorPrev = 0;

void Game::render()
{
	//  Menu/imgui
	{
		uiCamHeightPrev = uiCamHeight;
		uiCamForwardPrev = uiCamForward;
		uiSunYawPrev = uiSunYaw;
		uiSunPitchPrev = uiSunPitch;
		NumScatteringOrderPrev = NumScatteringOrder;
		uiGroundAbledoPrev = uiGroundAbledo = AtmosphereInfos.ground_albedo;

		////////////////////////////////////////////////////////////////////////////////////////////////////
		ImGui::Begin("Scene");

		char tmp[512];
		sprintf_s(tmp, sizeof(tmp), "Frame %i", mFrameId);
		ImGui::Text(tmp);

		ImGui::Checkbox("ClearDebug", &mClearDebugState);
		ImGui::Checkbox("UpdateDebug", &mUpdateDebugState);
		ImGui::Checkbox("PrintDebug", &mPrintDebug);
		ImGui::Separator();

		ImGui::Text("View");
		ImGui::SliderFloat("Height", &uiCamHeight, 0.001f, 2.0f*(AtmosphereInfos.top_radius - AtmosphereInfos.bottom_radius), "%.3f", 3.0f);
		ImGui::SliderFloat("Forward", &uiCamForward, -3.0f*AtmosphereInfos.top_radius, -1.0f, "%.3f", 3.0f);

		ImGui::Text("Sun");
		ImGui::SliderFloat("IllumScale", &mSunIlluminanceScale, 0.1f, 100.0f, "%.3f", 3.0f);
		ImGui::SliderFloat("Yaw", &uiSunYaw, -3.14f, 3.14f);
		ImGui::SliderFloat("Pitch", &uiSunPitch, -3.14f, 3.14f);

		ImGui::End();
		////////////////////////////////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////////////////////////////
		ImGui::Begin("Atmosphere");

		ImGui::SliderFloat("Mie phase", &AtmosphereInfos.mie_phase_function_g, 0.0f, 0.999f, "%.3f", 1.0f / 3.0f);
		ImGui::SliderInt("Scatt Order", &NumScatteringOrder, 1, 50);

		const GlslVec3 vec3Zero = CreateGlslVec3(0.0f, 0.0f, 0.0f);

		// Convert physical data to UI
		if (!uiDataInitialised)
		{
			uiDataInitialised = true;
			GlslVec3 RayleighScattering = AtmosphereInfos.rayleigh_scattering;
			GlslVec3 MieScattering = AtmosphereInfos.mie_scattering;
			GlslVec3 MieAbsorption = MaxZero3(sub3(AtmosphereInfos.mie_extinction, AtmosphereInfos.mie_scattering));
			MieScatteringLength = length3(MieScattering);
			MieScatteringColor = MieScatteringLength == 0.0f ? vec3Zero : normalize3(MieScattering, MieScatteringLength);
			MieAbsLength = length3(MieAbsorption);
			MieAbsColor = MieAbsLength == 0.0f ? vec3Zero : normalize3(MieAbsorption, MieAbsLength);
			RayleighScatteringLength = length3(RayleighScattering);
			RayleighScatteringColor = RayleighScatteringLength == 0.0f ? vec3Zero : normalize3(RayleighScattering, RayleighScatteringLength);
			AtmosphereHeight = AtmosphereInfos.top_radius - AtmosphereInfos.bottom_radius;
			MieScaleHeight = -1.0f / AtmosphereInfos.mie_density.layers[1].exp_scale;
			RayleighScaleHeight = -1.0f / AtmosphereInfos.rayleigh_density.layers[1].exp_scale;

			AbsorptionLength = length3(AtmosphereInfos.absorption_extinction);
			AbsorptionColor = AbsorptionLength == 0.0f ? vec3Zero : normalize3(AtmosphereInfos.absorption_extinction, AbsorptionLength);
		}

		ImGui::ColorEdit3( "MieScattCoeff", &MieScatteringColor.x);
		ImGui::SliderFloat("MieScattScale", &MieScatteringLength, 0.00001f, 0.1f, "%.5f", 3.0f);
		ImGui::ColorEdit3( "MieAbsorCoeff", &MieAbsColor.x);
		ImGui::SliderFloat("MieAbsorScale", &MieAbsLength, 0.00001f, 10.0f, "%.5f", 3.0f);
		ImGui::ColorEdit3( "RayScattCoeff", &RayleighScatteringColor.x);
		ImGui::SliderFloat("RayScattScale", &RayleighScatteringLength, 0.00001f, 10.0f, "%.5f", 3.0f);
		ImGui::ColorEdit3( "AbsorptiCoeff", &AbsorptionColor.x);
		ImGui::SliderFloat("AbsorptiScale", &AbsorptionLength, 0.00001f, 10.0f, "%.5f", 3.0f);
		ImGui::SliderFloat("Planet radius", &AtmosphereInfos.bottom_radius, 100.0f, 8000.0f);
		ImGui::SliderFloat("Atmos height", &AtmosphereHeight, 10.0f, 150.0f);
		ImGui::SliderFloat("MieScaleHeight", &MieScaleHeight, 0.5f, 20.0f);
		ImGui::SliderFloat("RayScaleHeight", &RayleighScaleHeight, 0.5f, 20.0f);

		ImGui::ColorEdit3("Ground albedo", &uiGroundAbledo.x);

		ImGui::End();
		////////////////////////////////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////////////////////////////
		ImGui::Begin("Render method/Tech");

		uiRenderingMethodPrev = uiRenderingMethod;
		const char* listbox_renderingMethods[] = { "Bruneton 2017", "Path Tracing", "NEW Ray Marching"};
		ImGui::Combo("Render method", &uiRenderingMethod, listbox_renderingMethods, MethodCount, 3);

		transPermutationPrev = currentTransPermutation;
		shadowPermutationPrev = currentShadowPermutation;
		RenderTerrainPrev = RenderTerrain;
		if (uiRenderingMethod == MethodPathTracing || uiRenderingMethod == MethodRaymarching)
		{
			if (uiRenderingMethod == MethodPathTracing)
			{
				const char* listbox_transmittanceMethods[] = { "TransmittanceDeltaTracking", "TransmittanceRatioTracking", "TransmittanceLUT" };
				ImGui::Combo("Trans method", &currentTransPermutation, listbox_transmittanceMethods, TransmittanceMethodCount, 3);
			}

			ImGui::Checkbox("ShadowMap", &currentShadowPermutation);
			ImGui::Checkbox("Terrain", &RenderTerrain);
		}

		if (uiRenderingMethod == MethodRaymarching)
		{
			ImGui::SliderInt("Min SPP", &uiViewRayMarchMinSPP, 1, 30);
			ImGui::SliderInt("Max SPP", &uiViewRayMarchMaxSPP, 2, 31);
			ImGui::Checkbox("FastSky",  &currentFastSky);
			ImGui::Checkbox("FastAerialPersepctive",  &currentAerialPerspective);
			if(!currentAerialPerspective)
				ImGui::Checkbox("RGB Transmittance",  &currentColoredTransmittance);
		}

		multipleScatteringFactorPrev = currentMultipleScatteringFactor;
		if (uiRenderingMethod != MethodBruneton2017)
		{
			ImGui::SliderFloat("Multi-Scattering approx", &currentMultipleScatteringFactor, 0.0f, 1.0f);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("If DualScattering>0, the path tracer will use it and stop at the first path depth.");
		}

		ImGui::End();
		////////////////////////////////////////////////////////////////////////////////////////////////////

		// Convert UI to physical data
		AtmosphereInfos.mie_scattering = scale3(MieScatteringColor, MieScatteringLength);
		AtmosphereInfos.mie_extinction = add3(AtmosphereInfos.mie_scattering, scale3(MieAbsColor, MieAbsLength));
		AtmosphereInfos.rayleigh_scattering = scale3(RayleighScatteringColor, RayleighScatteringLength);
		AtmosphereInfos.absorption_extinction = scale3(AbsorptionColor, AbsorptionLength);
		AtmosphereInfos.top_radius = AtmosphereInfos.bottom_radius + AtmosphereHeight;
		AtmosphereInfos.mie_density.layers[1].exp_scale = -1.0f / MieScaleHeight;
		AtmosphereInfos.rayleigh_density.layers[1].exp_scale = -1.0f / RayleighScaleHeight;
		AtmosphereInfos.ground_albedo = uiGroundAbledo;
	}

	GPU_SCOPED_TIMEREVENT(GameRender, 75, 75, 75);

	const D3dViewport& backBufferViewport = g_dx11Device->getBackBufferViewport();
	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	D3dRenderTargetView* backBuffer = g_dx11Device->getBackBufferRT();

	// Constant buffer update
	{
		XMMATRIX viewMatrix = XMMatrixIdentity();
		XMMATRIX projMatrix = XMMatrixOrthographicLH(1.0, 1.0, -1.0, 1.0);
		XMMATRIX ViewProjMat = XMMatrixMultiply(viewMatrix, projMatrix);

		mConstantBufferCPU.gViewProjMat = ViewProjMat;
		mConstantBufferCPU.gColor = { 0.0, 1.0, 1.0, 1.0 };
		mConstantBufferCPU.gResolution[0] = uint32(backBufferViewport.Width);
		mConstantBufferCPU.gResolution[1] = uint32(backBufferViewport.Height);
		mConstantBufferCPU.gSunIlluminance = { 1.0f*mSunIlluminanceScale, 1.0f*mSunIlluminanceScale, 1.0f*mSunIlluminanceScale };
		mConstantBufferCPU.gScatteringMaxPathDepth = NumScatteringOrder;
		static ULONGLONG LastTime = GetTickCount64();
		static float ElapsedTimeSec = 0;
		const ULONGLONG CurTime = GetTickCount64();
		mConstantBufferCPU.gFrameTimeSec = float(CurTime - LastTime) / 1000.0f;
		mConstantBufferCPU.gTimeSec = ElapsedTimeSec;
		mConstantBufferCPU.gFrameId = mFrameId;
		uiViewRayMarchMaxSPP = uiViewRayMarchMinSPP >= uiViewRayMarchMaxSPP ? uiViewRayMarchMinSPP + 1 : uiViewRayMarchMaxSPP;
		mConstantBufferCPU.RayMarchMinMaxSPP[0] = float(uiViewRayMarchMinSPP);
		mConstantBufferCPU.RayMarchMinMaxSPP[1] = float(uiViewRayMarchMaxSPP);
		mConstantBufferCPU.gScreenshotCaptureActive = false; // Make sure the terrain or sundisk are not taken into account to focus on the most important part: atmosphere.
		ElapsedTimeSec += mConstantBufferCPU.gFrameTimeSec;
		mConstantBuffer->update(mConstantBufferCPU);
	}

	// Set default state
	{
		context->OMSetDepthStencilState(mDefaultDepthStencilState->mState, 0);
		context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);
		context->RSSetState(mDefaultRasterizerState->mState);
		context->RSSetViewports(1, &backBufferViewport);
	}

	// Clear the HDR back buffer
	{
		GPU_SCOPED_TIMER(Clear, 34, 177, 76);
		D3DCOLORVALUE clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		context->ClearRenderTargetView(mBackBufferHdr->mRenderTargetView, &clearColor.r);
		context->ClearDepthStencilView(mBackBufferDepth->mDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
		context->ClearDepthStencilView(mShadowMap->mDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
		if (uiRenderingMethod==MethodPathTracing && ShouldClearPathTracedBuffer)
		{
			context->ClearRenderTargetView(mPathTracingLuminanceBuffer->mRenderTargetView, &clearColor.r);
			context->ClearRenderTargetView(mPathTracingTransmittanceBuffer->mRenderTargetView, &clearColor.r);
		}
	}

	ShouldClearPathTracedBuffer = uiRenderingMethod != MethodPathTracing;
	{
		// This hsould happen here du to delay in imgui update (processed at the end of the frame)
		ShouldClearPathTracedBuffer |= uiCamHeightPrev != uiCamHeight || uiCamForwardPrev != uiCamForward
			|| uiSunYawPrev != uiSunYaw || uiSunPitchPrev != uiSunPitch;
	}

	{
		GPU_SCOPED_TIMEREVENT(GpuDebugDataClear, 0, 255, 0);
		gpuDebugStateFrameInit(mDebugState, mUpdateDebugState && mClearDebugState);
		gpuDebugStateFrameInit(mDummyDebugState);
	}

	//////////
	////////// Sky / Atmosphere
	//////////

	updateSkyAtmosphereConstant();

	bool AtmosphereHasChanged = forceGenLut;
	if (memcmp(&AtmosphereInfos, &AtmosphereInfosSaved, sizeof(AtmosphereInfo)) != 0 || uiRenderingMethodPrev != uiRenderingMethod
		|| NumScatteringOrderPrev != NumScatteringOrder || !EqualFloat3(uiGroundAbledoPrev, uiGroundAbledo) || currentTransPermutation != transPermutationPrev
		|| shadowPermutationPrev != currentShadowPermutation || multipleScatteringFactorPrev != currentMultipleScatteringFactor
		|| RenderTerrainPrev != RenderTerrain)
	{
		forceGenLut |= true;
		AtmosphereHasChanged = true;
		ShouldClearPathTracedBuffer = true;
		memcpy(&AtmosphereInfosSaved, &AtmosphereInfos, sizeof(AtmosphereInfo));
		mFrameId = 0;
	}

	renderShadowmap();
	renderTerrain();

	{
		GPU_SCOPED_TIMEREVENT(SkyRender, 255, 255, 255);
		if (uiRenderingMethod != MethodBruneton2017)
		{
			renderTransmittanceLutPS();
		}

		if(uiRenderingMethod == MethodRaymarching || (uiRenderingMethod == MethodPathTracing && currentMultipleScatteringFactor > 0.0f))
		{
			renderNewMultiScattTexPS();
		}

		if (uiRenderingMethod == MethodPathTracing)
		{
			renderPathTracing();
			RenderSkyAtmosphereOverOpaque();
		}
		else if (uiRenderingMethod == MethodRaymarching)
		{
			if (currentFastSky)
				renderSkyViewLut();
			generateSkyAtmosphereCameraVolumeWithRayMarch();
			renderRayMarching();
		}
		else
		{
			if (AtmosphereHasChanged)
			{
				generateSkyAtmosphereLUTs();
				forceGenLut = false;
			}
			generateSkyAtmosphereCameraVolumes();
			renderSkyAtmosphereUsingLUTs();
		}
	}

	//////////
	////////// Final post process
	//////////
	context->RSSetViewports(1, &backBufferViewport);

	// Post process into the back buffer using a pixel shader
	{
		GPU_SCOPED_TIMEREVENT(Post, 34, 177, 76);

		const uint32* initialCount = 0;
		context->OMSetRenderTargetsAndUnorderedAccessViews(1, &backBuffer, nullptr, 1, 1, &mSomeBufferUavView, initialCount);

		// Set null input assembly and layout
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);

		// Final view
		mScreenVertexShader->setShader(*context);
		mPostProcessShader->setShader(*context);

		context->VSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);
		context->PSSetConstantBuffers(0, 1, &mConstantBuffer->mBuffer);

		context->PSSetShaderResources(0, 1, &mBackBufferHdr->mShaderResourceView);

		context->Draw(3, 0);
		g_dx11Device->setNullPsResources(context);
	}

	if (mPrintDebug)
	{
		GPU_SCOPED_TIMEREVENT(GpuDebugDrawLine, 0, 255, 0);
		context->OMSetDepthStencilState(mDisabledDepthStencilState->mState, 0);
		gpuDebugStateDraw(mDebugState, mViewProjMat);
		context->OMSetBlendState(mDefaultBlendState->mState, nullptr, 0xffffffff);
		context->OMSetDepthStencilState(mDefaultDepthStencilState->mState, 0);
	}

	if (takeScreenShot)
	{
		const char* screenShotFilePath = "screenshot.exr";
		D3dRenderContext* context = g_dx11Device->getDeviceContext();
		context->CopyResource(mBackBufferHdrStagingTexture->mTexture, mBackBufferHdr->mTexture);
		saveBackBufferHdr(screenShotFilePath);
		takeScreenShot = false;
	}
	mFrameId++;

}



