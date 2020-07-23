// Copyright Epic Games, Inc. All Rights Reserved.


#include "Dx11Device.h"
#include "D3Dcompiler.h"
#include "Strsafe.h"
#include <comdef.h> // _com_error
#include <iostream>

// Good dx tutorial: http://www.directxtutorial.com/Lesson.aspx?lessonid=11-4-2
// Dx debug API http://seanmiddleditch.com/direct3d-11-debug-api-tricks/, also https://msdn.microsoft.com/en-us/library/windows/desktop/ff476881(v=vs.85).aspx#Debug


Dx11Device* g_dx11Device = nullptr;

Dx11Device::Dx11Device()
{
}

Dx11Device::~Dx11Device()
{
	internalShutdown();
}

void Dx11Device::initialise(const HWND& hWnd)
{
	Dx11Device::shutdown();

	g_dx11Device = new Dx11Device();
	g_dx11Device->internalInitialise(hWnd);
}

void Dx11Device::shutdown()
{
	delete g_dx11Device;
	g_dx11Device = nullptr;
}

void Dx11Device::internalInitialise(const HWND& hWnd)
{
	HRESULT hr;

	// create a struct to hold information about the swap chain
	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

	// fill the swap chain description struct
	scd.BufferCount = 1;                                    // one back buffer
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // use 32-bit color
	scd.BufferDesc.RefreshRate.Numerator = 1;				// 60fps target
	scd.BufferDesc.RefreshRate.Denominator = 60;			// 60fps target
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
	scd.OutputWindow = hWnd;                                // the window to be used
	scd.SampleDesc.Count = 1;                               // multisample
	scd.Windowed = TRUE;                                    // windowed/full-screen mode

	const uint32 requestedFeatureLevelsCount = 3;
	const D3D_FEATURE_LEVEL requestedFeatureLevels[requestedFeatureLevelsCount] =
	{
		D3D_FEATURE_LEVEL_12_1,         // Always ask for 12.1 feature level if available
										// other fallbacks
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,       // Always ask for 11.1 (or higher) if available
	};

	// create a device, device context and swap chain using the information in the scd struct
	hr = D3D11CreateDeviceAndSwapChain(NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		DX_DEBUG_EVENT|DX_DEBUG_RESOURCE_NAME ? D3D11_CREATE_DEVICE_DEBUG : NULL,
		requestedFeatureLevels,
		requestedFeatureLevelsCount,
		D3D11_SDK_VERSION,
		&scd,
		&mSwapchain,
		&mDev,
		NULL,
		&mDevcon);
	ATLASSERT(hr == S_OK);

#if DX_DEBUG_EVENT
	// Could do more https://blogs.msdn.microsoft.com/chuckw/2012/11/30/direct3d-sdk-debug-layer-tricks/
	// If this fails, make sure you have the graphic tools installed. (Apps/Manage optional features windows settings)
	hr = mDevcon->QueryInterface(__uuidof(mUserDefinedAnnotation), reinterpret_cast<void**>(&mUserDefinedAnnotation));
	ATLASSERT( hr == S_OK );
#endif // DX_DEBUG_EVENT

	updateSwapChain(0, 0);

	// By default, set the back buffer as current render target and viewport (no state tracking for now...)
	mDevcon->OMSetRenderTargets(1, &mBackBufferRT, NULL);
	mDevcon->RSSetViewports(1, &mBackBufferViewport);
	
	D3D_FEATURE_LEVEL deviceFeatureLevel = mDev->GetFeatureLevel();
	OutputDebugStringA("\n\nSelected D3D feature level: ");
	switch (deviceFeatureLevel)
	{
	case D3D_FEATURE_LEVEL_12_1:
		OutputDebugStringA("D3D_FEATURE_LEVEL_12_1\n\n");
		break;
	case D3D_FEATURE_LEVEL_12_0:
		OutputDebugStringA("D3D_FEATURE_LEVEL_12_0\n\n");
		break;
	case D3D_FEATURE_LEVEL_11_1:
		OutputDebugStringA("D3D_FEATURE_LEVEL_11_1\n\n");
		break;
	default:
		ATLASSERT(false); // unhandled level string
	}

	/*D3D11_FEATURE_DATA_D3D11_OPTIONS2 featuretest;
	hr = mDev->CheckFeatureSupport(
		D3D11_FEATURE_D3D11_OPTIONS2,
		&featuretest,
		sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS2));
	ATLASSERT(hr == S_OK);*/

	/*D3D11_FEATURE_DATA_DOUBLES dataDouble;
	hr = mDev->CheckFeatureSupport(D3D11_FEATURE_DOUBLES, &dataDouble, sizeof(dataDouble));
	ATLASSERT(hr == S_OK);
	if (!dataDouble.DoublePrecisionFloatShaderOps)
	{
		printf("No hardware support for double-precision.\n");
	}*/
}

void Dx11Device::internalShutdown()
{
	// close and release all existing COM objects
	resetComPtr(&mBackBufferRT);
	resetComPtr(&mSwapchain);
	resetComPtr(&mDev);
	resetComPtr(&mDevcon);
}

void Dx11Device::updateSwapChain(uint32 newWidth, uint32 newHeight)
{
	resetComPtr(&mBackBufferRT);

	if (newWidth > 0 && newHeight > 0)
	{
		mSwapchain->ResizeBuffers(0, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, 0);

		// Update the back buffer RT
		ID3D11Texture2D* backBufferTexture;// back buffer texture
		mSwapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBufferTexture);
		mDev->CreateRenderTargetView(backBufferTexture, NULL, &mBackBufferRT);
		resetComPtr(&backBufferTexture); // This does not delete the object. Just release the ref we just got.

		mBackBufferViewport.Width = (float)newWidth;
		mBackBufferViewport.Height = (float)newHeight;
	}
	else
	{
		// We get here when creating a Dx11Device
		ID3D11Texture2D* backBufferTexture;// back buffer texture
		mSwapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBufferTexture);
		mDev->CreateRenderTargetView(backBufferTexture, NULL, &mBackBufferRT);

		D3dTexture2dDesc texDesc;
		backBufferTexture->GetDesc(&texDesc);
		mBackBufferViewport.TopLeftX = 0.0f;
		mBackBufferViewport.TopLeftY = 0.0f;
		mBackBufferViewport.Width = (float)texDesc.Width;
		mBackBufferViewport.Height = (float)texDesc.Height;
		mBackBufferViewport.MinDepth = 0.0f;
		mBackBufferViewport.MaxDepth = 1.0f;

		resetComPtr(&backBufferTexture); // See description above.
	}
}

void Dx11Device::swap(bool vsyncEnabled)
{
	mSwapchain->Present(vsyncEnabled ? 1 : 0, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


RenderBuffer::RenderBuffer(D3dBufferDesc& bufferDesc, void* initialData) :
	mDesc(bufferDesc)
{
	ID3D11Device* device = g_dx11Device->getDevice();

	D3dSubResourceData data;
	data.pSysMem = initialData;
	data.SysMemPitch = 0;
	data.SysMemSlicePitch = 0;

	HRESULT hr = device->CreateBuffer( &mDesc, initialData ? &data : nullptr, &mBuffer);
	ATLASSERT(hr == S_OK);
}

RenderBuffer::~RenderBuffer()
{
	resetComPtr(&mBuffer);
}

void RenderBuffer::map(D3D11_MAP map, ScopedMappedRenderbuffer& mappedBuffer)
{
	// Check some state
	ATLASSERT( mDesc.Usage == D3D11_USAGE_DYNAMIC && (mDesc.CPUAccessFlags&D3D11_CPU_ACCESS_WRITE)!=0 );

	// Reset to 0
	ZeroMemory(&mappedBuffer.mMappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));

	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	context->Map(mBuffer, 0, map, 0, &mappedBuffer.mMappedResource);
	mappedBuffer.mMappedBuffer = mBuffer;
}

void RenderBuffer::unmap(ScopedMappedRenderbuffer& mappedBuffer)
{
	if (mappedBuffer.mMappedBuffer)
	{
		D3dRenderContext* context = g_dx11Device->getDeviceContext();
		context->Unmap(mappedBuffer.mMappedBuffer, 0);
		mappedBuffer.mMappedBuffer = nullptr;
	}
}

D3dBufferDesc RenderBuffer::initConstantBufferDesc_dynamic(uint32 byteSize)
{
	D3dBufferDesc desc = { byteSize , D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
	return desc;
}
D3dBufferDesc RenderBuffer::initVertexBufferDesc_default(uint32 byteSize)
{
	D3dBufferDesc desc = { byteSize , D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
	return desc;
}
D3dBufferDesc RenderBuffer::initIndexBufferDesc_default(uint32 byteSize)
{
	D3dBufferDesc desc = { byteSize , D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER, 0, 0, 0 };
	return desc;
}
D3dBufferDesc RenderBuffer::initBufferDesc_default(uint32 byteSize)
{
	D3dBufferDesc desc = { byteSize , D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, 0 };
	return desc;
}
D3dBufferDesc RenderBuffer::initBufferDesc_uav(uint32 byteSize)
{
	D3dBufferDesc desc = { byteSize , D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, 0, 0, 0 };
	return desc;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DXGI_FORMAT getDepthStencilViewFormatFromTypeless(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R16_TYPELESS:
		return DXGI_FORMAT_D16_UNORM;
		break;
	case DXGI_FORMAT_R24G8_TYPELESS:
		return  DXGI_FORMAT_D24_UNORM_S8_UINT;
		break;
	case DXGI_FORMAT_R32_TYPELESS:
		return  DXGI_FORMAT_D32_FLOAT;
		break;
	}
	ATLASSERT(false); // unknown format
	return DXGI_FORMAT_UNKNOWN;
}
DXGI_FORMAT getDepthShaderResourceFormatFromTypeless(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R16_TYPELESS:
		return DXGI_FORMAT_R16_UNORM;
		break;
	case DXGI_FORMAT_R24G8_TYPELESS:
		return  DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		break;
	case DXGI_FORMAT_R32_TYPELESS:
		return  DXGI_FORMAT_D32_FLOAT;
		break;
	}
	ATLASSERT(false); // unknown format
	return DXGI_FORMAT_UNKNOWN;
}

bool isFormatTypeless(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R24G8_TYPELESS:
	//case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	//case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC7_TYPELESS:
		return true;
	}
	return false;
}

Texture2D::Texture2D(D3dTexture2dDesc& desc, D3dSubResourceData* initialData) :
	mDesc(desc)
{
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreateTexture2D(&mDesc, initialData, &mTexture);
	ATLASSERT(hr == S_OK);

	DXGI_FORMAT format = desc.Format;
	if (mDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
		depthStencilViewDesc.Format = getDepthStencilViewFormatFromTypeless(format);
		HRESULT hr = device->CreateDepthStencilView(mTexture, &depthStencilViewDesc, &mDepthStencilView);
		ATLASSERT(hr == S_OK);

		format = getDepthShaderResourceFormatFromTypeless(format);
	}

	ATLASSERT(!isFormatTypeless(format));

	if (mDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		CD3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc(D3D11_SRV_DIMENSION_TEXTURE2D);
		shaderResourceViewDesc.Format = format;
		HRESULT hr = device->CreateShaderResourceView(mTexture, &shaderResourceViewDesc, &mShaderResourceView);
		ATLASSERT(hr == S_OK);
	}
	if (mDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		CD3D11_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc(D3D11_UAV_DIMENSION_TEXTURE2D);
		unorderedAccessViewDesc.Format = format;
		HRESULT hr = device->CreateUnorderedAccessView(mTexture, &unorderedAccessViewDesc, &mUnorderedAccessView);
		ATLASSERT(hr == S_OK);
	}
	if (mDesc.BindFlags & D3D11_BIND_RENDER_TARGET)
	{
		CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2D);
		renderTargetViewDesc.Format = format;
		HRESULT hr = device->CreateRenderTargetView(mTexture, &renderTargetViewDesc, &mRenderTargetView);
		ATLASSERT(hr == S_OK);
	}
}
Texture2D::~Texture2D()
{
	if (mDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		resetComPtr(&mDepthStencilView);
	}
	if (mDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		resetComPtr(&mShaderResourceView);
	}
	if (mDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		resetComPtr(&mUnorderedAccessView);
	}
	if (mDesc.BindFlags & D3D11_BIND_RENDER_TARGET)
	{
		resetComPtr(&mRenderTargetView);
	}
	resetComPtr(&mTexture);
}
D3dTexture2dDesc Texture2D::initDepthStencilBuffer(uint32 width, uint32 height, bool uav)
{
	D3dTexture2dDesc desc = { 0 };
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R24G8_TYPELESS; // DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL | (uav ? D3D11_BIND_UNORDERED_ACCESS : 0);	// cannot be D3D11_BIND_SHADER_RESOURCE
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	return desc;
}

D3dTexture2dDesc Texture2D::initDefault(DXGI_FORMAT format, uint32 width, uint32 height, bool renderTarget, bool uav)
{
	D3dTexture2dDesc desc = { 0 };
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (renderTarget ? D3D11_BIND_RENDER_TARGET : 0) | (uav ? D3D11_BIND_UNORDERED_ACCESS : 0);
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	return desc;
}





Texture3D::Texture3D(D3dTexture3dDesc& desc, D3dSubResourceData* initialData) :
	mDesc(desc)
{
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreateTexture3D(&mDesc, initialData, &mTexture);
	ATLASSERT(hr == S_OK);
	
	if (mDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		CD3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc(D3D11_SRV_DIMENSION_TEXTURE3D);
		shaderResourceViewDesc.Format = desc.Format;
		HRESULT hr = device->CreateShaderResourceView(mTexture, &shaderResourceViewDesc, &mShaderResourceView);
		ATLASSERT(hr == S_OK);
		if (desc.MipLevels > 1)
		{
			for (uint32 l = 0; l < desc.MipLevels; ++l)
			{
				shaderResourceViewDesc.Texture3D.MostDetailedMip = l;
				shaderResourceViewDesc.Texture3D.MipLevels = 1;

				ID3D11ShaderResourceView* mipShaderResourceView;
				hr = device->CreateShaderResourceView(mTexture, &shaderResourceViewDesc, &mipShaderResourceView);
				ATLASSERT(hr == S_OK);
				mShaderResourceViewMips.push_back(mipShaderResourceView);
			}
		}
	}
	if (mDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		CD3D11_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc(D3D11_UAV_DIMENSION_TEXTURE3D);
		unorderedAccessViewDesc.Format = desc.Format;
		HRESULT hr = device->CreateUnorderedAccessView(mTexture, &unorderedAccessViewDesc, &mUnorderedAccessView);
		ATLASSERT(hr == S_OK);
		if (desc.MipLevels > 1)
		{
			for (uint32 l = 0; l < desc.MipLevels; ++l)
			{
				unorderedAccessViewDesc.Texture3D.MipSlice = l;

				ID3D11UnorderedAccessView* mipUnorderedAccessView;
				hr = device->CreateUnorderedAccessView(mTexture, &unorderedAccessViewDesc, &mipUnorderedAccessView);
				ATLASSERT(hr == S_OK);
				mUnorderedAccessViewMips.push_back(mipUnorderedAccessView);
			}
		}
	}
	if (mDesc.BindFlags & D3D11_BIND_RENDER_TARGET)
	{
		CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE3D);
		renderTargetViewDesc.Format = desc.Format;
		renderTargetViewDesc.Texture3D.MipSlice = 0;
		renderTargetViewDesc.Texture3D.FirstWSlice = 0;
		renderTargetViewDesc.Texture3D.WSize = desc.Depth;
		HRESULT hr = device->CreateRenderTargetView(mTexture, &renderTargetViewDesc, &mRenderTargetView);
		ATLASSERT(hr == S_OK);
	}
}
Texture3D::~Texture3D()
{
	if (mDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		resetComPtr(&mShaderResourceView);
		for (auto view : mShaderResourceViewMips)
		{
			resetComPtr(&view);
		}
		mShaderResourceViewMips.clear();
	}
	if (mDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		resetComPtr(&mUnorderedAccessView);
		for (auto view : mUnorderedAccessViewMips)
		{
			resetComPtr(&view);
		}
		mUnorderedAccessViewMips.clear();
	}
	resetComPtr(&mTexture);
	if (mDesc.BindFlags & D3D11_BIND_RENDER_TARGET)
	{
		resetComPtr(&mRenderTargetView);
	}
}
D3dTexture3dDesc Texture3D::initDefault(DXGI_FORMAT format, uint32 width, uint32 height, uint32 depth, bool renderTarget, bool uav)
{
	D3dTexture3dDesc desc = { 0 };
	desc.Width = width;
	desc.Height = height;
	desc.Depth = depth;
	desc.MipLevels = 1;
	desc.Format = format;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (renderTarget ? D3D11_BIND_RENDER_TARGET : 0) | (uav ? D3D11_BIND_UNORDERED_ACCESS : 0);
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	return desc;
}



SamplerState::SamplerState(D3dSamplerDesc& desc)
{
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreateSamplerState(&desc, &mSampler);
	ATLASSERT(hr == S_OK);
}
SamplerState::~SamplerState()
{
	resetComPtr(&mSampler);
}
D3dSamplerDesc SamplerState::initLinearClamp()
{
	D3dSamplerDesc desc = { 0 };
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.MipLODBias = 0.0f;
	desc.MaxAnisotropy = 1;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;
	desc.MinLOD = -FLT_MAX;
	desc.MaxLOD = FLT_MAX;
	return desc;
}
D3dSamplerDesc SamplerState::initPointClamp()
{
	D3dSamplerDesc desc = { 0 };
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.MipLODBias = 0.0f;
	desc.MaxAnisotropy = 1;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;
	desc.MinLOD = -FLT_MAX;
	desc.MaxLOD = FLT_MAX;
	return desc;
}
D3dSamplerDesc SamplerState::initShadowCmpClamp()
{
	D3dSamplerDesc desc = { 0 };
	desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.MipLODBias = 0.0f;
	desc.MaxAnisotropy = 1;
	desc.ComparisonFunc = D3D11_COMPARISON_LESS;
	desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;
	desc.MinLOD = -FLT_MAX;
	desc.MaxLOD = FLT_MAX;
	return desc;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


DepthStencilState::DepthStencilState(D3dDepthStencilDesc& desc)
{
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreateDepthStencilState(&desc, &mState);
	ATLASSERT(hr == S_OK);
}
DepthStencilState::~DepthStencilState()
{
	resetComPtr(&mState);
}
D3dDepthStencilDesc DepthStencilState::initDefaultDepthOnStencilOff()
{
	D3dDepthStencilDesc desc = { 0 };
	// Depth test parameters
	desc.DepthEnable = true;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = D3D11_COMPARISON_LESS;
	// Stencil test parameters
	desc.StencilEnable = false;
	desc.StencilReadMask = 0xFF;
	desc.StencilWriteMask = 0xFF;
	// Stencil operations if pixel is front-facing
	desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	// Stencil operations if pixel is back-facing
	desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	return desc;
}
D3dDepthStencilDesc DepthStencilState::initDepthNoWriteStencilOff()
{
	D3dDepthStencilDesc desc = { 0 };
	// Depth test parameters
	desc.DepthEnable = true;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D11_COMPARISON_LESS;
	// Stencil test parameters
	desc.StencilEnable = false;
	desc.StencilReadMask = 0xFF;
	desc.StencilWriteMask = 0xFF;
	// Stencil operations if pixel is front-facing
	desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	// Stencil operations if pixel is back-facing
	desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	return desc;
}

RasterizerState::RasterizerState(D3dRasterizerDesc& desc)
{
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreateRasterizerState(&desc, &mState);
	ATLASSERT(hr == S_OK);
}
RasterizerState::~RasterizerState()
{
	resetComPtr(&mState);
}
D3dRasterizerDesc RasterizerState::initDefaultState()
{
	D3dRasterizerDesc desc = { 0 };
	ZeroMemory(&desc, sizeof(D3dRasterizerDesc));
	desc.AntialiasedLineEnable = FALSE;
	desc.CullMode = D3D11_CULL_BACK;
	desc.DepthBias = 0;
	desc.DepthBiasClamp = 0.0f;
	desc.DepthClipEnable = TRUE;
	desc.FillMode = D3D11_FILL_SOLID;
	desc.FrontCounterClockwise = FALSE;
	desc.MultisampleEnable = FALSE;
	desc.ScissorEnable = FALSE;
	desc.SlopeScaledDepthBias = 0.0f;
	return desc;
}


BlendState::BlendState(D3dBlendDesc & desc)
{
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreateBlendState(&desc, &mState);
	ATLASSERT(hr == S_OK);
}
BlendState::~BlendState()
{
	resetComPtr(&mState);
}
D3dBlendDesc BlendState::initDisabledState()
{
	D3dBlendDesc desc = { 0 };
	desc.AlphaToCoverageEnable = false;
	desc.IndependentBlendEnable = false;
	desc.RenderTarget[0].BlendEnable = false;
	desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	return desc;
}
D3dBlendDesc BlendState::initPreMultBlendState()
{
	D3dBlendDesc desc = { 0 };
	desc.AlphaToCoverageEnable = false;
	desc.IndependentBlendEnable = false;
	desc.RenderTarget[0].BlendEnable = true;
	desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;				// src*1 + dst*(1.0 - srcA)
	desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;			// src*0 + dst * (1.0 - srcA)
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	return desc;
}
D3dBlendDesc BlendState::initPreMultDualBlendState()
{
	D3dBlendDesc desc = { 0 };
	desc.AlphaToCoverageEnable = false;
	desc.IndependentBlendEnable = false;
	desc.RenderTarget[0].BlendEnable = true;
	desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	desc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC1_COLOR;
	desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;				// src0*1 + dst*src1	, colored transmittance + color
	desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;			// src*0  + dst*1		, keep alpha intact
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	return desc;
}
D3dBlendDesc BlendState::initAdditiveState()
{
	D3dBlendDesc desc = { 0 };
	desc.AlphaToCoverageEnable = false;
	desc.IndependentBlendEnable = false;
	desc.RenderTarget[0].BlendEnable = true;
	desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	return desc;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


void appendSimpleVertexDataToInputLayout(InputLayoutDesc& inputLayout, const char* semanticName, DXGI_FORMAT format)
{
	D3dInputElementDesc desc;

	desc.SemanticName = semanticName;
	desc.SemanticIndex = 0;
	desc.Format = format;
	desc.InputSlot = 0;
	desc.AlignedByteOffset = inputLayout.empty() ? 0 : D3D11_APPEND_ALIGNED_ELEMENT;
	desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	desc.InstanceDataStepRate = 0;

	inputLayout.push_back(desc);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

static ID3D10Blob* compileShader(const TCHAR* filename, const char* entryFunction, const char* profile, const Macros* macros = NULL)
{
	ID3D10Blob* shaderBuffer = NULL;
	ID3DBlob * errorbuffer = NULL;
	const uint32 defaultFlags = 0;

#define MAX_SHADER_MACRO 64
	D3D_SHADER_MACRO shaderMacros[MAX_SHADER_MACRO];
	shaderMacros[0] = { NULL, NULL };	// in case there is no macros
	if (macros != NULL)
	{
		size_t macrosCount = macros->size();
		bool validMacroCount = macros->size() <= MAX_SHADER_MACRO-1;	// -1 to handle the null end of list macro
		ATLASSERT(validMacroCount);
		if (!validMacroCount)
		{
			OutputDebugStringA("\nNumber of macro is too high for shader ");
			OutputDebugStringW(filename);
			OutputDebugStringA("\n");
			return NULL;
		}
		for (int32 m = 0; m < macrosCount; ++m)
		{
			const ShaderMacro& sm = macros->at(m);
			shaderMacros[m] = { sm.Name.c_str() , sm.Definition.c_str() };
		}
		shaderMacros[macrosCount] = { NULL, NULL };
	}

	HRESULT hr = D3DCompileFromFile(
		filename,							// filename
		shaderMacros,						// defines
		D3D_COMPILE_STANDARD_FILE_INCLUDE,	// default include handler (includes relative to the compiled file)
		entryFunction,						// function name
		profile,							// target profile
		defaultFlags,//D3DCOMPILE_DEBUG,	// flag1
		defaultFlags,						// flag2
		&shaderBuffer,						// ouput
		&errorbuffer);						// errors

	if (FAILED(hr))
	{
		OutputDebugStringA("\n===> Failed to compile shader: function=");
		OutputDebugStringA(entryFunction);
		OutputDebugStringA(", profile=");
		OutputDebugStringA(profile);
		OutputDebugStringA(", file=");
		OutputDebugStringW(filename);
		OutputDebugStringA(" :\n");
		OutputDebugStringA(" HResult = "); 
		_com_error err(hr);
		OutputDebugStringW(err.ErrorMessage());
		OutputDebugStringA("\n");
		if (errorbuffer)
		{
			OutputDebugStringA((char*)errorbuffer->GetBufferPointer());
			resetComPtr(&errorbuffer);
		}
		resetComPtr(&shaderBuffer);
		OutputDebugStringA("\n\n");
		return NULL;
	}
	return shaderBuffer;
}

ShaderBase::ShaderBase(const TCHAR* filename, const char* entryFunction, const char* profile, const Macros* macros, bool lazyCompilation)
	: mShaderBuffer(nullptr)
	, mFilename(filename)
	, mEntryFunction(entryFunction)
	, mProfile(profile)
{
	if(macros)
		mMacros = *macros;
	if(!lazyCompilation)
		mShaderBuffer = compileShader(mFilename, mEntryFunction, mProfile, macros);
}

ShaderBase::~ShaderBase()
{
	resetComPtr(&mShaderBuffer);
}

bool ShaderBase::recompileShaderIfNeeded()
{
	ID3D10Blob* compiledShaderBuffer;
	bool newShaderBinaryAvailable = false;
	if (mDirty && (compiledShaderBuffer = compileShader(mFilename, mEntryFunction, mProfile, &mMacros)))
	{
		resetComPtr(&mShaderBuffer);
		mShaderBuffer = compiledShaderBuffer;
		newShaderBinaryAvailable = true;
	}
	mDirty = false;	// we always remove dirtiness to avoid try to recompile each frame.
	return newShaderBinaryAvailable;
}

VertexShader::VertexShader(const TCHAR* filename, const char* entryFunction, const Macros* macros, bool lazyCompilation)
	: ShaderBase(filename, entryFunction, "vs_5_0", macros, lazyCompilation)
{
	if (!compilationSuccessful()) return; // failed compilation
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreateVertexShader(mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), NULL, &mVertexShader);
	ATLASSERT(hr == S_OK);
	mDirty = false;
}
VertexShader::~VertexShader()
{
	resetComPtr(&mVertexShader);
}

void VertexShader::createInputLayout(InputLayoutDesc inputLayout, D3dInputLayout** layout)
{
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreateInputLayout(inputLayout.data(), uint32(inputLayout.size()), mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), layout);
	ATLASSERT(hr == S_OK);
}

void VertexShader::setShader(D3dRenderContext& context)
{
	if (recompileShaderIfNeeded())
	{
		resetComPtr(&mVertexShader);
		ID3D11Device* device = g_dx11Device->getDevice();
		HRESULT hr = device->CreateVertexShader(mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), NULL, &mVertexShader);
		ATLASSERT(hr == S_OK);
	}
	context.VSSetShader(mVertexShader, nullptr, 0);
}

PixelShader::PixelShader(const TCHAR* filename, const char* entryFunction, const Macros* macros, bool lazyCompilation)
	: ShaderBase(filename, entryFunction, "ps_5_0", macros, lazyCompilation)
{
	if (!compilationSuccessful()) return; // failed compilation
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreatePixelShader(mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), NULL, &mPixelShader);
	ATLASSERT(hr == S_OK);
	mDirty = false;
}
PixelShader::~PixelShader()
{
	resetComPtr(&mPixelShader);
}
void PixelShader::setShader(D3dRenderContext& context)
{
	if (recompileShaderIfNeeded())
	{
		resetComPtr(&mPixelShader);
		ID3D11Device* device = g_dx11Device->getDevice();
		HRESULT hr = device->CreatePixelShader(mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), NULL, &mPixelShader);
		ATLASSERT(hr == S_OK);
	}
	context.PSSetShader(mPixelShader, nullptr, 0);
}

HullShader::HullShader(const TCHAR* filename, const char* entryFunction, const Macros* macros, bool lazyCompilation)
	: ShaderBase(filename, entryFunction, "hs_5_0", macros, lazyCompilation)
{
	if (!compilationSuccessful()) return; // failed compilation
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreateHullShader(mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), NULL, &mHullShader);
	ATLASSERT(hr == S_OK);
	mDirty = false;
}
HullShader::~HullShader()
{
	resetComPtr(&mHullShader);
}
void HullShader::setShader(D3dRenderContext& context)
{
	if (recompileShaderIfNeeded())
	{
		resetComPtr(&mHullShader);
		ID3D11Device* device = g_dx11Device->getDevice();
		HRESULT hr = device->CreateHullShader(mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), NULL, &mHullShader);
		ATLASSERT(hr == S_OK);
	}
	context.HSSetShader(mHullShader, nullptr, 0);
}

DomainShader::DomainShader(const TCHAR* filename, const char* entryFunction, const Macros* macros, bool lazyCompilation)
	: ShaderBase(filename, entryFunction, "ds_5_0", macros, lazyCompilation)
{
	if (!compilationSuccessful()) return; // failed compilation
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreateDomainShader(mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), NULL, &mDomainShader);
	ATLASSERT(hr == S_OK);
	mDirty = false;
}
DomainShader::~DomainShader()
{
	resetComPtr(&mDomainShader);
}
void DomainShader::setShader(D3dRenderContext& context)
{
	if (recompileShaderIfNeeded())
	{
		resetComPtr(&mDomainShader);
		ID3D11Device* device = g_dx11Device->getDevice();
		HRESULT hr = device->CreateDomainShader(mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), NULL, &mDomainShader);
		ATLASSERT(hr == S_OK);
	}
	context.DSSetShader(mDomainShader, nullptr, 0);
}

GeometryShader::GeometryShader(const TCHAR* filename, const char* entryFunction, const Macros* macros, bool lazyCompilation)
	: ShaderBase(filename, entryFunction, "gs_5_0", macros, lazyCompilation)
{
	if (!compilationSuccessful()) return; // failed compilation
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreateGeometryShader(mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), NULL, &mGeometryShader);
	ATLASSERT(hr == S_OK);
	mDirty = false;
}
GeometryShader::~GeometryShader()
{
	resetComPtr(&mGeometryShader);
}
void GeometryShader::setShader(D3dRenderContext& context)
{
	if (recompileShaderIfNeeded())
	{
		resetComPtr(&mGeometryShader);
		ID3D11Device* device = g_dx11Device->getDevice();
		HRESULT hr = device->CreateGeometryShader(mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), NULL, &mGeometryShader);
		ATLASSERT(hr == S_OK);
	}
	context.GSSetShader(mGeometryShader, nullptr, 0);
}

ComputeShader::ComputeShader(const TCHAR* filename, const char* entryFunction, const Macros* macros, bool lazyCompilation)
	: ShaderBase(filename, entryFunction, "cs_5_0", macros, lazyCompilation)
{
	if (!compilationSuccessful()) return; // failed compilation
	ID3D11Device* device = g_dx11Device->getDevice();
	HRESULT hr = device->CreateComputeShader(mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), NULL, &mComputeShader);
	ATLASSERT(hr == S_OK);
	mDirty = false;
}
ComputeShader::~ComputeShader()
{
	resetComPtr(&mComputeShader);
}
void ComputeShader::setShader(D3dRenderContext& context)
{
	if (recompileShaderIfNeeded())
	{
		resetComPtr(&mComputeShader);
		ID3D11Device* device = g_dx11Device->getDevice();
		HRESULT hr = device->CreateComputeShader(mShaderBuffer->GetBufferPointer(), mShaderBuffer->GetBufferSize(), NULL, &mComputeShader);
		ATLASSERT(hr == S_OK);
	}
	context.CSSetShader(mComputeShader, nullptr, 0);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// static members
DxGpuPerformance::GpuTimerMap DxGpuPerformance::mTimers;
int32 DxGpuPerformance::mMeasureTimerFrameId;
int32 DxGpuPerformance::mReadTimerFrameId;
int32 DxGpuPerformance::mLastReadTimerFrameId;

DxGpuPerformance::DxGpuTimer DxGpuPerformance::mTimerArray[V_TIMER_MAX_COUNT];
int32 DxGpuPerformance::mAllocatedTimers;

DxGpuPerformance::TimerGraphNode DxGpuPerformance::mTimerGraphNodeArray[V_GPU_TIMER_FRAMECOUNT][V_TIMER_MAX_COUNT];
int32 DxGpuPerformance::mAllocatedTimerGraphNodes[V_GPU_TIMER_FRAMECOUNT];

DxGpuPerformance::GpuTimerGraph DxGpuPerformance::mTimerGraphs[V_GPU_TIMER_FRAMECOUNT];
DxGpuPerformance::TimerGraphNode* DxGpuPerformance::mCurrentTimeGraph = nullptr;

void DxGpuPerformance::initialise()
{
	mTimers.clear();
	mAllocatedTimers = 0;

	mMeasureTimerFrameId = 0;							// first frame
	mReadTimerFrameId = -V_GPU_TIMER_FRAMECOUNT+1;		// invalid
	mLastReadTimerFrameId = -V_GPU_TIMER_FRAMECOUNT-1;	// invalid
}
void DxGpuPerformance::shutdown()
{
	for (int32 i = 0; i < mAllocatedTimers; ++i)
		mTimerArray[i].release();
	mTimers.clear();
}

void DxGpuPerformance::startFrame()
{
	// Clear the frame we are going to measure and append root timer node
	for (int32 i = 0, cnt = mAllocatedTimerGraphNodes[mMeasureTimerFrameId]; i < cnt; ++i)
		mTimerGraphNodeArray[mMeasureTimerFrameId][i].subGraph.clear();
	mAllocatedTimerGraphNodes[mMeasureTimerFrameId] = 0; // reset the counter
	TimerGraphNode* root = &mTimerGraphNodeArray [mMeasureTimerFrameId] [mAllocatedTimerGraphNodes[mMeasureTimerFrameId]++];
	root->name = "root";
	root->r = root->g = root->b = 127;
	mTimerGraphs[mMeasureTimerFrameId].clear();

	mTimerGraphs[mMeasureTimerFrameId].push_back(root);
	mCurrentTimeGraph = (*mTimerGraphs[mMeasureTimerFrameId].begin());
}

void DxGpuPerformance::startGpuTimer(const char* name, unsigned char r, unsigned char g, unsigned char  b)
{
	GpuTimerMap::iterator it = mTimers.find(name);
	DxGpuTimer* timer = nullptr;
	if (it == mTimers.end())
	{
		// Allocate a timer and insert into the map
		ATLASSERT(mAllocatedTimers<(V_TIMER_MAX_COUNT-1));
		timer = &mTimerArray[mAllocatedTimers++];
		mTimers[name] = timer;
		timer->initialize();
	}
	else
	{
		ATLASSERT(!(*it).second->mUsedThisFrame);	// a timer can only be used once a frame
		timer = (*it).second;
	}

	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	context->Begin(timer->mDisjointQueries[mMeasureTimerFrameId]);
	context->End(timer->mBeginQueries[mMeasureTimerFrameId]);

	// Make sure we do not add the timer twice per frame
	ATLASSERT(!timer->mUsedThisFrame);
	timer->mUsedThisFrame = true;

	// Push the timer node we just started
	ATLASSERT(mAllocatedTimerGraphNodes[mMeasureTimerFrameId]<(V_TIMER_MAX_COUNT - 1));
	TimerGraphNode* node = &mTimerGraphNodeArray[mMeasureTimerFrameId][mAllocatedTimerGraphNodes[mMeasureTimerFrameId]++];
	node->name = name;
	node->r = float(r) / 255.0f;
	node->g = float(g) / 255.0f;
	node->b = float(b) / 255.0f;
	node->timer = timer;
	node->parent = mCurrentTimeGraph;
	mCurrentTimeGraph->subGraph.push_back(node);
	mCurrentTimeGraph = (*mCurrentTimeGraph->subGraph.rbegin());
	timer->mNode[mMeasureTimerFrameId] = mCurrentTimeGraph;
								// TO SOLVE: have static array of node 
}

void DxGpuPerformance::endGpuTimer(const char* name)
{
	DxGpuTimer* timer = mTimers[name];

	D3dRenderContext* context = g_dx11Device->getDeviceContext();
	context->End(timer->mDisjointQueries[mMeasureTimerFrameId]);
	context->End(timer->mEndQueries[mMeasureTimerFrameId]);
	timer->mEnded = true;

	// Pop to the parent of the timer node that has just ended
	mCurrentTimeGraph = mCurrentTimeGraph->parent;
	ATLASSERT(mCurrentTimeGraph!=nullptr);
}

void DxGpuPerformance::endFrame()
{
	static ULONGLONG LastTime = GetTickCount64();
	const ULONGLONG CurTime = GetTickCount64();
	static float FrameTimeSec = 0;
	FrameTimeSec = float(CurTime - LastTime) / 1000.0f;
	LastTime = CurTime;

	// Fetch data from ready timer
	if (mReadTimerFrameId >= 0)
	{
		int32 localReadTimerFrameId = mReadTimerFrameId%V_GPU_TIMER_FRAMECOUNT;

		D3dRenderContext* context = g_dx11Device->getDeviceContext();
		DxGpuPerformance::GpuTimerMap::iterator it;

		// Get all the data first
		for (it = mTimers.begin(); it != mTimers.end(); it++)
		{
			DxGpuPerformance::DxGpuTimer* timer = (*it).second;
			if (!timer->mUsedThisFrame)	// we should test usePreviousFrame but that will be enough for now
				continue;
			ATLASSERT(timer->mEnded);	// the timer must have been ended this frame

			TimerGraphNode* node = timer->mNode[localReadTimerFrameId];
			if (!node)
				continue;				// This can happen when a timer is registered later when some features are enabled.
#if 1
			while (context->GetData(timer->mBeginQueries[localReadTimerFrameId], &node->mBeginTick, sizeof(UINT64), 0) != S_OK);
			while (context->GetData(timer->mEndQueries[localReadTimerFrameId], &node->mEndTick, sizeof(UINT64), 0) != S_OK);
			while (context->GetData(timer->mDisjointQueries[localReadTimerFrameId], &node->disjointData, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0) != S_OK);
#else
			context->GetData(timer->mBeginQueries[localReadTimerFrameId], &node->mBeginTick, sizeof(UINT64), 0);
			context->GetData(timer->mEndQueries[localReadTimerFrameId], &node->mEndTick, sizeof(UINT64), 0);
			context->GetData(timer->mDisjointQueries[localReadTimerFrameId], &node->disjointData, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0);
#endif
		}

		// Get the begining of the frame measurement
		UINT64 minBeginTime = 0xFFFFFFFFFFFFFFFF;
		for (it = mTimers.begin(); it != mTimers.end(); it++)
		{
			DxGpuPerformance::DxGpuTimer* timer = (*it).second;
			if (!timer->mUsedThisFrame)	// we should test usePreviousFrame but that will be enough for now
				continue;

			TimerGraphNode* node = timer->mNode[localReadTimerFrameId];
			if (!node)
				continue;				// This can happen when a timer is registered later when some features are enabled.

			minBeginTime = min(minBeginTime, node->mBeginTick);
		}


		for (it = mTimers.begin(); it != mTimers.end(); it++)
		{
			DxGpuPerformance::DxGpuTimer* timer = (*it).second;

			if (!timer->mUsedThisFrame)	// we should test usePreviousFrame but that will be enough for now
				continue;

			// Reset the safety checks
			timer->mUsedThisFrame = false;
			timer->mEnded = false;

			TimerGraphNode* node = timer->mNode[localReadTimerFrameId];
			if (!node)
				continue;				// This can happen when a timer is registered later when some features are enabled.

			float beginMs = 0.0f;
			float endMs = 0.0f;
			float lastDurationMs = 0.0f;
			if (node->disjointData.Disjoint == FALSE)
			{
				float factor = 1000.0f / float(node->disjointData.Frequency);
				beginMs = (node->mBeginTick - minBeginTime) * factor;
				endMs   = (node->mEndTick - minBeginTime) * factor;
				lastDurationMs = (node->mEndTick - node->mBeginTick) * factor;
			}
			node->mBeginMs = beginMs;
			node->mEndMs = endMs;

#if 1
			node->mLastDurationMs = lastDurationMs;
#else
			float lerpFactor = FrameTimeSec / 0.25f;
			lerpFactor = lerpFactor>1.0f ? 1.0f : lerpFactor<0.0f ? 0.0f : lerpFactor;
			node->mLastDurationMs += (lastDurationMs - node->mLastDurationMs) * lerpFactor;
#endif
		}
	}
	else
	{
		// At least reset timers safety checks
		DxGpuPerformance::GpuTimerMap::iterator it;
		for (it = mTimers.begin(); it != mTimers.end(); it++)
		{
			DxGpuPerformance::DxGpuTimer* timer = (*it).second;
			timer->mUsedThisFrame = false;
			timer->mEnded = false;
		}
	}

	// Move onto next frame
	mReadTimerFrameId++;
	mMeasureTimerFrameId = (mMeasureTimerFrameId + 1) % V_GPU_TIMER_FRAMECOUNT;
	mLastReadTimerFrameId++;
}

const DxGpuPerformance::TimerGraphNode* DxGpuPerformance::getLastUpdatedTimerGraphRootNode()
{
	if (mLastReadTimerFrameId >= 0)
	{
		return *mTimerGraphs[mLastReadTimerFrameId%V_GPU_TIMER_FRAMECOUNT].begin();
	}
	return nullptr;
}

DxGpuPerformance::DxGpuTimer::DxGpuTimer()
{
}

DxGpuPerformance::DxGpuTimer::~DxGpuTimer()
{
}

void DxGpuPerformance::DxGpuTimer::initialize()
{
	ID3D11Device* device = g_dx11Device->getDevice();
	D3D11_QUERY_DESC queryDesc;
	queryDesc.Query = D3D11_QUERY_TIMESTAMP;
	queryDesc.MiscFlags = 0;
	D3D11_QUERY_DESC disjointQueryDesc;
	disjointQueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	disjointQueryDesc.MiscFlags = 0;
	for (int32 i = 0; i < V_GPU_TIMER_FRAMECOUNT; ++i)
	{
		device->CreateQuery(&disjointQueryDesc, &mDisjointQueries[i]);
		device->CreateQuery(&queryDesc, &mBeginQueries[i]);
		device->CreateQuery(&queryDesc, &mEndQueries[i]);
	}
}
void DxGpuPerformance::DxGpuTimer::release()
{
	ID3D11Device* device = g_dx11Device->getDevice();
	for (int32 i = 0; i < V_GPU_TIMER_FRAMECOUNT; ++i)
	{
		resetComPtr(&mDisjointQueries[i]);
		resetComPtr(&mBeginQueries[i]);
		resetComPtr(&mEndQueries[i]);
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////



int32 divRoundUp(int32 numer, int32 denum)
{
	return (numer + denum - 1) / denum;
}



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


//buffer, constant, view, render target and shader creation
// http://www.rastertek.com/dx11tut04.html
// https://msdn.microsoft.com/en-us/library/windows/desktop/dn508285(v=vs.85).aspx









