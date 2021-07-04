// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#define DX_DEBUG_EVENT 1
#define DX_DEBUG_RESOURCE_NAME 1

// Windows and Dx11 includes
#include <map>
#include <string>
#include <vector>
#include <windows.h>
#include <windowsx.h>
#include <atlbase.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3d11_2.h>

#include "DxMath.h"

// include the Direct3D Library file
#pragma comment (lib, "d3d11.lib")
#if DX_DEBUG_EVENT || DX_DEBUG_RESOURCE_NAME
	#pragma comment( lib, "dxguid.lib")	// For debug name guid
#endif


typedef ID3D11Device						D3dDevice;
typedef ID3D11DeviceContext					D3dRenderContext;

typedef ID3D11InputLayout					D3dInputLayout;
typedef D3D11_INPUT_ELEMENT_DESC			D3dInputElementDesc;
typedef D3D11_VIEWPORT						D3dViewport;

typedef ID3D11ShaderResourceView			D3dShaderResourceView;
typedef D3D11_UNORDERED_ACCESS_VIEW_DESC	D3dUnorderedAccessViewDesc;
typedef ID3D11UnorderedAccessView			D3dUnorderedAccessView;
typedef ID3D11RenderTargetView				D3dRenderTargetView;
typedef ID3D11DepthStencilView				D3dDepthStencilView;

typedef D3D11_BUFFER_DESC					D3dBufferDesc;
typedef D3D11_TEXTURE2D_DESC				D3dTexture2dDesc;
typedef D3D11_TEXTURE3D_DESC				D3dTexture3dDesc;
typedef D3D11_SUBRESOURCE_DATA				D3dSubResourceData;
typedef D3D11_SAMPLER_DESC					D3dSamplerDesc;

typedef D3D11_DEPTH_STENCIL_DESC			D3dDepthStencilDesc;
typedef D3D11_RASTERIZER_DESC				D3dRasterizerDesc;
typedef D3D11_BLEND_DESC					D3dBlendDesc;



class Dx11Device
{
public:

	static void initialise(const HWND& hWnd);
	static void shutdown();

	D3dDevice*								getDevice()			{ return mDev; }
	D3dRenderContext*						getDeviceContext()	{ return mDevcon; }
	IDXGISwapChain*							getSwapChain()		{ return mSwapchain; }
	D3dRenderTargetView*					getBackBufferRT()	{ return mBackBufferRT; }

#if DX_DEBUG_EVENT
	CComPtr<ID3DUserDefinedAnnotation>		mUserDefinedAnnotation;
#endif

	void swap(bool vsyncEnabled);

	static void setNullRenderTarget(D3dRenderContext* devcon)
	{
		D3dRenderTargetView*    nullRTV = nullptr;
		D3dUnorderedAccessView* nullUAV = nullptr;
		//devcon->OMSetRenderTargets(1, &nullRTV, nullptr);
		devcon->OMSetRenderTargetsAndUnorderedAccessViews(1, &nullRTV, nullptr, 1, 0, &nullUAV, nullptr);
	}
	static void setNullPsResources(D3dRenderContext* devcon)
	{
		static D3dShaderResourceView* null[16] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };	// not good, only 8, would need something smarter maybe...
		devcon->PSSetShaderResources(0, 16, null);
	}
	static void setNullVsResources(D3dRenderContext* devcon)
	{
		static D3dShaderResourceView* null[16] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		devcon->VSSetShaderResources(0, 16, null);
	}
	static void setNullCsResources(D3dRenderContext* devcon)
	{
		static D3dShaderResourceView* null[16] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		devcon->CSSetShaderResources(0, 16, null);
	}
	static void setNullCsUnorderedAccessViews(D3dRenderContext* devcon)
	{
		static D3dUnorderedAccessView* null[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		devcon->CSSetUnorderedAccessViews(0, 8, null, nullptr);
	}

	const D3dViewport& getBackBufferViewport() const { return mBackBufferViewport; }
	void updateSwapChain(uint32 newWidth, uint32 newHeight);

private:
	Dx11Device();
	Dx11Device(Dx11Device&);
	//Dx11Device(const Dx11Device&);
	~Dx11Device();

	void internalInitialise(const HWND& hWnd);
	void internalShutdown();

	IDXGISwapChain*							mSwapchain;				// the pointer to the swap chain interface
	D3dDevice*								mDev;					// the pointer to our Direct3D device interface
	D3dRenderContext*						mDevcon;				// the pointer to our Direct3D device context

	D3dRenderTargetView*					mBackBufferRT;			// back buffer render target

	D3dViewport								mBackBufferViewport;
};

extern Dx11Device* g_dx11Device;


#if DX_DEBUG_RESOURCE_NAME
#define DX_SET_DEBUG_NAME(obj, debugName) obj->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof(debugName), debugName)
#else
#define DX_SET_DEBUG_NAME(obj, debugName) 
#endif

#if DX_DEBUG_EVENT
#define GPU_BEGIN_EVENT(eventName) g_dx11Device->mUserDefinedAnnotation->BeginEvent(L""#eventName)
#define GPU_END_EVENT() g_dx11Device->mUserDefinedAnnotation->EndEvent()
#else
#define GPU_BEGIN_EVENT(eventName) 
#define GPU_END_EVENT() 
#endif

struct ScopedGpuEvent
{
	ScopedGpuEvent(LPCWSTR name)
		: mName(name)
	{
#if DX_DEBUG_EVENT
		g_dx11Device->mUserDefinedAnnotation->BeginEvent(mName);
#endif
	}
	~ScopedGpuEvent()
	{
		release();
	}
	void release()
	{
		if (!released)
		{
			released = true;
#if DX_DEBUG_EVENT
			g_dx11Device->mUserDefinedAnnotation->EndEvent();
#endif
		}
	}
private:
	ScopedGpuEvent() = delete;
	ScopedGpuEvent(ScopedGpuEvent&) = delete;
	LPCWSTR mName;
	bool released = false;
};
#define GPU_SCOPED_EVENT(timerName) ScopedGpuEvent gpuEvent##timerName##(L""#timerName)


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


class RenderBuffer
{
public:

	RenderBuffer(D3dBufferDesc& mBufferDesc, void* initialData=nullptr);
	virtual ~RenderBuffer();


	// Usage of dynamic resources and mapping

	struct ScopedMappedRenderbuffer
	{
		ScopedMappedRenderbuffer()
			: mMappedBuffer(nullptr)
		{}
		~ScopedMappedRenderbuffer()
		{ RenderBuffer::unmap(*this); }

		void* getDataPtr() { return mMappedResource.pData; }
	private:
		friend class RenderBuffer;
		D3D11_MAPPED_SUBRESOURCE mMappedResource;
		ID3D11Buffer* mMappedBuffer;
	};
	void map(D3D11_MAP map, ScopedMappedRenderbuffer& mappedBuffer);
	static void unmap(ScopedMappedRenderbuffer& mappedBuffer);


	// Some basic descriptor initialisation methods

	static D3dBufferDesc initConstantBufferDesc_dynamic(uint32 byteSize);
	static D3dBufferDesc initVertexBufferDesc_default(uint32 byteSize);
	static D3dBufferDesc initIndexBufferDesc_default(uint32 byteSize);
	static D3dBufferDesc initBufferDesc_default(uint32 byteSize);
	static D3dBufferDesc initBufferDesc_uav(uint32 byteSize);

public:///////////////////////////////////protected:
	D3dBufferDesc mDesc;
	ID3D11Buffer* mBuffer;

private:
	RenderBuffer();
	RenderBuffer(RenderBuffer&);
};


template<typename T>
class ConstantBuffer : public RenderBuffer
{
public:
	ConstantBuffer() : RenderBuffer(getDesc(), nullptr) {}

	void update(const T& content)
	{
		ATLASSERT(mBuffer != nullptr);
		RenderBuffer::ScopedMappedRenderbuffer bufferMap;
		map(D3D11_MAP_WRITE_DISCARD, bufferMap);
		T* cb = (T*)bufferMap.getDataPtr();
		memcpy(cb, &content, sizeof(T));
	}
private:

	static D3dBufferDesc getDesc()
	{
		ATLASSERT(sizeof(T) % 16 == 0);
		return initConstantBufferDesc_dynamic(sizeof(T));
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


class Texture2D
{
public:
	Texture2D(D3dTexture2dDesc& desc, D3dSubResourceData* initialData = nullptr);
	virtual ~Texture2D();
	static D3dTexture2dDesc initDepthStencilBuffer(uint32 width, uint32 height, bool uav);
	static D3dTexture2dDesc initDefault(DXGI_FORMAT format, uint32 width, uint32 height, bool renderTarget, bool uav);
	D3dTexture2dDesc mDesc;
	ID3D11Texture2D* mTexture = nullptr;
	D3dDepthStencilView* mDepthStencilView = nullptr;
	D3dRenderTargetView* mRenderTargetView = nullptr;
	D3dShaderResourceView* mShaderResourceView = nullptr;
	D3dUnorderedAccessView* mUnorderedAccessView = nullptr;
private:
	Texture2D();
	Texture2D(Texture2D&);
};

class Texture3D
{
public:
	Texture3D(D3dTexture3dDesc& desc, D3dSubResourceData* initialData = nullptr);
	virtual ~Texture3D();
	static D3dTexture3dDesc initDefault(DXGI_FORMAT format, uint32 width, uint32 height, uint32 depth, bool renderTarget, bool uav);
	D3dTexture3dDesc mDesc;
	ID3D11Texture3D* mTexture = nullptr;
	D3dRenderTargetView* mRenderTargetView = nullptr;				// level 0
	D3dShaderResourceView* mShaderResourceView = nullptr;			// level 0
	D3dUnorderedAccessView* mUnorderedAccessView = nullptr;			// level 0
	std::vector<D3dShaderResourceView*> mShaderResourceViewMips;		// all levels
	std::vector<D3dUnorderedAccessView*> mUnorderedAccessViewMips;	// all levels
private:
	Texture3D();
	Texture3D(Texture3D&);
};

class SamplerState
{
public:
	SamplerState(D3dSamplerDesc& desc);
	virtual ~SamplerState();
	static D3dSamplerDesc initLinearClamp();
	static D3dSamplerDesc initPointClamp();
	static D3dSamplerDesc initShadowCmpClamp();
	ID3D11SamplerState* mSampler = nullptr;
private:
	SamplerState();
	SamplerState(SamplerState&);
};


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


class DepthStencilState
{
public:
	DepthStencilState(D3dDepthStencilDesc& desc);
	virtual ~DepthStencilState();
	static D3dDepthStencilDesc initDefaultDepthOnStencilOff();
	static D3dDepthStencilDesc initDepthNoWriteStencilOff();
	ID3D11DepthStencilState* mState;
private:
	DepthStencilState();
	DepthStencilState(DepthStencilState&);
};

class RasterizerState
{
public:
	RasterizerState(D3dRasterizerDesc& desc);
	virtual ~RasterizerState();
	static D3dRasterizerDesc initDefaultState();
	ID3D11RasterizerState* mState;
private:
	RasterizerState();
	RasterizerState(RasterizerState&);
};

class BlendState
{
public:
	BlendState(D3dBlendDesc & desc);
	virtual ~BlendState();
	static D3dBlendDesc initDisabledState();
	static D3dBlendDesc initPreMultBlendState();
	static D3dBlendDesc initPreMultDualBlendState();
	static D3dBlendDesc initAdditiveState();
	ID3D11BlendState* mState;
private:
	BlendState();
	BlendState(BlendState&);
};


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


typedef std::vector<D3dInputElementDesc> InputLayoutDesc;

// Append a simple per vertex data layout input 
void appendSimpleVertexDataToInputLayout(InputLayoutDesc& inputLayout, const char* semanticName, DXGI_FORMAT format);

// Semantic names: https://msdn.microsoft.com/en-us/library/windows/desktop/bb509647(v=vs.85).aspx


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

struct ShaderMacro
{
	// We use string here to own the memory.
	// This is a requirement for delayed loading with non static shader parameter (created on stack or heap with unkown lifetime)
	std::string Name;
	std::string Definition;
};
typedef std::vector<ShaderMacro> Macros; // D3D_SHADER_MACRO contains pointers to string so those string must be static as of today.
class ShaderBase
{
public:
	ShaderBase(const TCHAR* filename, const char* entryFunction, const char* profile, const Macros* macros = nullptr, bool lazyCompilation = false);
	virtual ~ShaderBase();
	bool compilationSuccessful() { return mShaderBuffer != nullptr; }
	void markDirty() { mDirty = true; }

protected:
	ID3D10Blob* mShaderBuffer;
	const TCHAR* mFilename;
	const char* mEntryFunction;
	const char* mProfile;
	Macros mMacros;
	bool mDirty = true;		// If dirty, needs to be recompiled

	inline bool recompileShaderIfNeeded();

private:
	ShaderBase();
	ShaderBase(ShaderBase&);
};

class VertexShader : public ShaderBase
{
public:
	VertexShader(const TCHAR* filename, const char* entryFunction, const Macros* macros = nullptr, bool lazyCompilation = false);
	virtual ~VertexShader();
	void createInputLayout(InputLayoutDesc inputLayout, D3dInputLayout** layout);	// abstract that better
	void setShader(D3dRenderContext& context);
private:
	ID3D11VertexShader* mVertexShader;
};

class PixelShader : public ShaderBase
{
public:
	PixelShader(const TCHAR* filename, const char* entryFunction, const Macros* macros = nullptr, bool lazyCompilation = false);
	virtual ~PixelShader();
	void setShader(D3dRenderContext& context);
private:
	ID3D11PixelShader* mPixelShader;
};

class HullShader : public ShaderBase
{
public:
	HullShader(const TCHAR* filename, const char* entryFunction, const Macros* macros = nullptr, bool lazyCompilation = false);
	virtual ~HullShader();
	void setShader(D3dRenderContext& context);
private:
	ID3D11HullShader* mHullShader;
};

class DomainShader : public ShaderBase
{
public:
	DomainShader(const TCHAR* filename, const char* entryFunction, const Macros* macros = nullptr, bool lazyCompilation = false);
	virtual ~DomainShader();
	void setShader(D3dRenderContext& context);
private:
	ID3D11DomainShader* mDomainShader;
};

class GeometryShader : public ShaderBase
{
public:
	GeometryShader(const TCHAR* filename, const char* entryFunction, const Macros* macros = nullptr, bool lazyCompilation = false);
	virtual ~GeometryShader();
	void setShader(D3dRenderContext& context);
private:
	ID3D11GeometryShader* mGeometryShader;
};

class ComputeShader : public ShaderBase
{
public:
	ComputeShader(const TCHAR* filename, const char* entryFunction, const Macros* macros = nullptr, bool lazyCompilation = false);
	virtual ~ComputeShader();
	void setShader(D3dRenderContext& context);
private:
	ID3D11ComputeShader* mComputeShader;
};



////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

/// Example with double buffering on why we should use at least 3 timer query in this case (2 still works)
///  2: current frame added to context commend buffer
///  1: frame currently in flight
///  0: frame previous to current one in flight, done, data should be available
#define V_GPU_TIMER_FRAMECOUNT 3

/// Maximum number of timer in a frame and timer graph
#define V_TIMER_MAX_COUNT 512

class DxGpuPerformance
{
private:
	struct DxGpuTimer;
public:
	struct TimerGraphNode;

	static void initialise();
	static void shutdown();

	/// Limitation as of today: each timer in a single frame must have different names
	static void startGpuTimer(const char* name, unsigned char r, unsigned char g, unsigned char  b);
	static void endGpuTimer(const char* name);

	/// To call when starting to build render commands
	static void startFrame();
	/// To call after the back buffer swap
	static void endFrame();

	// TODO: add a reset function for when resources needs to all be re-allocated

	/// Some public structure that can be used to print out a frame timer graph
	struct TimerGraphNode
	{
		std::string name;
		float r, g, b;
		DxGpuTimer* timer = nullptr;
		std::vector<TimerGraphNode*> subGraph;	// will result in lots of allocations but that will do for now...
		TimerGraphNode* parent = nullptr;

		// Converted and extracted data
		float mBeginMs;
		float mEndMs;
		float mLastDurationMs;

	private:
		friend class DxGpuPerformance;
		// A bit too much to store all that raw data but convenient for now
		UINT64 mBeginTick;
		UINT64 mEndTick;
		UINT64 mLastDurationTick;
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
	};
	typedef std::vector<TimerGraphNode*> GpuTimerGraph;

	/// Returns the root node of the performance timer graph. This first root node contains nothing valid appart from childrens sub graphs.
	static const TimerGraphNode* getLastUpdatedTimerGraphRootNode();

private:
	DxGpuPerformance() = delete;
	DxGpuPerformance(DxGpuPerformance&) = delete;

	/// The actual timer data storing the dx11 queries
	struct DxGpuTimer
	{
		DxGpuTimer();
		~DxGpuTimer();

		void initialize();
		void release();

		ID3D11Query* mDisjointQueries[V_GPU_TIMER_FRAMECOUNT];
		ID3D11Query* mBeginQueries[V_GPU_TIMER_FRAMECOUNT];
		ID3D11Query* mEndQueries[V_GPU_TIMER_FRAMECOUNT];

		/// The graph node associated to this timer
		TimerGraphNode* mNode[V_GPU_TIMER_FRAMECOUNT];

		bool mUsedThisFrame = false;	///! should be checked before querying data in case it is not used in some frames (also we only support one timer per name)
		bool mEnded = false;			///! sanity check to make sure a stared element is ended
	};

	typedef std::map<std::string, DxGpuTimer*> GpuTimerMap;
	static GpuTimerMap mTimers;			///! All the timers mapped using their name
	static int32 mMeasureTimerFrameId;	///! Last measured frame (appended timer to command buffer)
	static int32 mReadTimerFrameId;		///! Last frame we read the timer values from the api
	static int32 mLastReadTimerFrameId;	///! Last frame we read the timers and they are still valid for debug print on screen (data from previous finished frame)

	// Double buffer so that we can display the previous frame timers while the current frame is being processed
	static GpuTimerGraph mTimerGraphs[V_GPU_TIMER_FRAMECOUNT];	///! Timer graphs of the last frames
	static TimerGraphNode* mCurrentTimeGraph;					///! The current graph being filled up this frame

	// Basically, node object are not in container as this can result in invalid/stale pointer when reallocated. Instead we allocate in static arrays
	// and container point to the static array. With such an approach, all pointers will remain valid over the desired lifetime (several frames).
	static DxGpuTimer mTimerArray[V_TIMER_MAX_COUNT];
	static int32 mAllocatedTimers;
	static TimerGraphNode mTimerGraphNodeArray[V_GPU_TIMER_FRAMECOUNT][V_TIMER_MAX_COUNT];
	static int32 mAllocatedTimerGraphNodes[V_GPU_TIMER_FRAMECOUNT];
};

struct ScopedGpuTimer
{
	ScopedGpuTimer(const char* name, unsigned char r, unsigned char g, unsigned char b)
		: mName(name)
	{
		DxGpuPerformance::startGpuTimer(mName, r, g, b);
	}
	~ScopedGpuTimer()
	{
		release();
	}
	void release()
	{
		if (!released)
		{
			released = true;
			DxGpuPerformance::endGpuTimer(mName);
		}
	}
private:
	ScopedGpuTimer() = delete;
	ScopedGpuTimer(ScopedGpuTimer&) = delete;
	const char* mName;
	bool released = false;
};

#define GPU_SCOPED_TIMER(timerName, r, g, b) ScopedGpuTimer gpuTimer##timerName##(#timerName, r, g, b)
#define GPU_SCOPED_TIMEREVENT(teName, r, g, b) GPU_SCOPED_EVENT(teName);GPU_SCOPED_TIMER(teName, r, g, b);


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////


template <class type>
void resetComPtr(type** ptr)
{
	if (*ptr)
	{
		(*ptr)->Release();
		(*ptr) = nullptr;
	}
}

template <class type>
void resetPtr(type** ptr)
{
	if (*ptr)
	{
		delete *ptr;
		(*ptr) = nullptr;
	}
}

// Can be used to load and reload (live update) a shader
template <class type>
bool reload(type** previousShader, const TCHAR* filename, const char* entryFunction, bool exitIfFail, const Macros* macros = NULL, bool lazyCompilation = false)
{
	type* newShader = new type(filename, entryFunction, macros, lazyCompilation);
	if (newShader->compilationSuccessful() || lazyCompilation)
	{
		// if lazyCompilation, we need to assign the newly create object. Later compilation that do not want to recreate object should just mark the shader object as dirty.
		// If compilation succesful also.
		resetPtr(previousShader);
		*previousShader = newShader;
		return true;
	}
	else
	{
		delete newShader;
		if (exitIfFail)
			exit(0);
		return false;
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////



int32 divRoundUp(int32 numer, int32 denum);


