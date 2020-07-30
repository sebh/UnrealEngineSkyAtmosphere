// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Dx11Base/WindowInput.h"
#include "Dx11Base/Dx11Device.h"


#include "SkyAtmosphereCommon.h"
#include "GpuDebugRenderer.h"
#include <functional>

struct CaptureEvent
{
	bool setupdone = false;
	bool skipScreenshot = false;
	char filename[256];
	std::function<void(void)> stateSetup;
	uint32 waitFrameCount = 3;
};

struct CaptureState
{
	bool active = false;
	uint32 eventId = 0;
	uint32 frame = 0;
	std::vector<CaptureEvent> events;
};

class Game
{
public:
	Game();
	~Game();

	void initialise();
	void reallocateResolutionDependent(uint32 newWidth, uint32 newHeight);
	void shutdown();

	void update(const WindowInputData& inputData);
	void render();

private:

	/// Load/reload all shaders if compilation is succesful.
	/// @firstTimeLoadShaders: calls exit(0) if any of the reload/compilation failed.
	void loadShaders(bool firstTimeLoadShaders);
	/// release all shaders
	void releaseShaders();

	void allocateResolutionIndependentResources();
	void releaseResolutionIndependentResources();
	void allocateResolutionDependentResources(uint32 newWidth, uint32 newHeight);
	void releaseResolutionDependentResources();

	void saveBackBufferHdr(const char* filepath);

	// Test vertex buffer
	struct VertexType
	{
		float position[3];
	};
	RenderBuffer* vertexBuffer;
	RenderBuffer* indexBuffer;

	//
	// Testing some GPU buffers and shaders 
	//

	struct CommonConstantBufferStructure
	{
		float4x4 gViewProjMat;

		float4 gColor;

		float3 gSunIlluminance;
		int gScatteringMaxPathDepth;

		unsigned int gResolution[2];
		float gFrameTimeSec;
		float gTimeSec;

		unsigned int gMouseLastDownPos[2];
		unsigned int gFrameId;
		unsigned int gTerrainResolution;
		float gScreenshotCaptureActive;

		float RayMarchMinMaxSPP[2];
		float pad[2];
	};
	typedef ConstantBuffer<CommonConstantBufferStructure> CommonConstantBuffer;
	CommonConstantBuffer* mConstantBuffer;
	CommonConstantBufferStructure mConstantBufferCPU;

	RenderBuffer* mSomeBuffer;
	D3dUnorderedAccessView* mSomeBufferUavView;

	DepthStencilState* mDefaultDepthStencilState;
	DepthStencilState* mDisabledDepthStencilState;
	BlendState* mDefaultBlendState;
	RasterizerState* mDefaultRasterizerState;
	RasterizerState* mShadowRasterizerState;

	VertexShader* mVertexShader;
	VertexShader* mScreenVertexShader;

	VertexShader* mTerrainVertexShader;
	PixelShader*  mTerrainPixelShader;

	PixelShader*  mApplySkyAtmosphereShader;
	PixelShader*  mPostProcessShader;

	D3dInputLayout* mLayout;

	Texture2D* mPathTracingLuminanceBuffer;
	Texture2D* mPathTracingTransmittanceBuffer;

	Texture2D* mFrameAtmosphereBuffer;

	Texture2D* mBackBufferHdr;
	Texture2D* mBackBufferDepth;
	Texture2D* mBackBufferHdrStagingTexture;

	Texture2D* mShadowMap;

	Texture2D* mTerrainHeightmapTex;
	Texture2D* mBlueNoise2dTex;

	uint32 mFrameId = 0;
	bool takeScreenShot = false;

	const uint32 MultiScatteringLUTRes = 32;

	////////////////////////////////////////////////////////////////////////////////
	// Sky and Atmosphere parameters

	struct SkyAtmosphereConstantBufferStructure
	{
		//
		// From AtmosphereParameters
		//

		IrradianceSpectrum solar_irradiance;
		Angle sun_angular_radius;

		ScatteringSpectrum absorption_extinction;
		Number mu_s_min;

		ScatteringSpectrum rayleigh_scattering;
		Number mie_phase_function_g;

		ScatteringSpectrum mie_scattering;
		Length bottom_radius;

		ScatteringSpectrum mie_extinction;
		Length top_radius;

		ScatteringSpectrum mie_absorption;
		Length pad00;

		DimensionlessSpectrum ground_albedo;
		float pad0;

		float rayleigh_density[12];
		float mie_density[12];
		float absorption_density[12];

		//
		// Add generated static header constant
		//

		int TRANSMITTANCE_TEXTURE_WIDTH;
		int TRANSMITTANCE_TEXTURE_HEIGHT;
		int IRRADIANCE_TEXTURE_WIDTH;
		int IRRADIANCE_TEXTURE_HEIGHT;

		int SCATTERING_TEXTURE_R_SIZE;
		int SCATTERING_TEXTURE_MU_SIZE;
		int SCATTERING_TEXTURE_MU_S_SIZE;
		int SCATTERING_TEXTURE_NU_SIZE;

		float3 SKY_SPECTRAL_RADIANCE_TO_LUMINANCE;
		float  pad3;
		float3 SUN_SPECTRAL_RADIANCE_TO_LUMINANCE;
		float  pad4;

		//
		// Other globals
		//
		float4x4 gSkyViewProjMat;
		float4x4 gSkyInvViewProjMat;
		float4x4 gShadowmapViewProjMat;

		float3 camera;
		float  pad5;
		float3 sun_direction;
		float  pad6;
		float3 view_ray;
		float  pad7;

		float MultipleScatteringFactor;
		float MultiScatteringLUTRes;
		float pad9;
		float pad10;
	};
	typedef ConstantBuffer<SkyAtmosphereConstantBufferStructure> SkyAtmosphereConstantBuffer;
	SkyAtmosphereConstantBuffer* SkyAtmosphereBuffer;

	struct SkyAtmosphereSideConstantBufferStructure
	{
		float4x4 LuminanceFromRadiance;
		int      ScatteringOrder;
		int      UseSingleMieScatteringTexture;
		float2   pad01;
	};
	typedef ConstantBuffer<SkyAtmosphereSideConstantBufferStructure> SkyAtmosphereSideConstantBuffer;
	SkyAtmosphereSideConstantBuffer* SkyAtmosphereSideBuffer;

	SamplerState* SamplerLinear;
	SamplerState* SamplerShadow;

	BlendState* Blend0Nop1Add;
	BlendState* BlendAddRGBA;
	BlendState* BlendPreMutlAlpha;
	BlendState* BlendLuminanceTransmittance;

	PixelShader*  TransmittanceLutPS;
	PixelShader*  DirectIrradianceLutPS;
	PixelShader*  SingleScatteringLutPS;
	PixelShader*  ScatteringDensityLutPS;
	PixelShader*  IndirectIrradianceLutPS;
	PixelShader*  MultipleScatteringLutPS;

	PixelShader*  CameraVolumesPS;

	GeometryShader*  GeometryGS;

	LookUpTablesInfo LutsInfo;
	AtmosphereInfo AtmosphereInfos;
	AtmosphereInfo AtmosphereInfosSaved;
	LookUpTables LUTs;
	TempLookUpTables TempLUTs;
	Texture3D* AtmosphereCameraScatteringVolume;
	Texture3D* AtmosphereCameraTransmittanceVolume;

	const uint32 ShadowmapSize = 4096;
	float4x4 mShadowmapViewProjMat;

	float4x4 mViewProjMat;
	float3   mCamPos;
	float3   mCamPosFinal;
	float3	 mViewDir;
	float3   mSunDir;
	float mSunIlluminanceScale = 1.0f;
	float viewPitch = 0.0f;
	float viewYaw = 0.0f;

	PixelShader*  RenderWithLutPS;



	enum {
		TransmittanceMethodDeltaTracking = 0,
		TransmittanceMethodRatioTracking,
		TransmittanceMethodLUT,
		TransmittanceMethodCount
	};
	enum {
		GroundGlobalIlluminationDisabled = 0,
		GroundGlobalIlluminationEnabled,
		GroundGlobalIlluminationCount
	};
	enum {
		ShadowmapDisabled = 0,
		ShadowmapEnabled,
		ShadowmapCount
	};
	enum {
		MultiScatApproxDisabled = 0,
		MultiScatApproxEnabled,
		MultiScatApproxCount
	};
	enum {
		ColoredTransmittanceDisabled = 0,
		ColoredTransmittanceEnabled,
		ColoredTransmittanceCount
	};
	enum {
		FastSkyDisabled = 0,
		FastSkyEnabled,
		FastSkyCount
	};
	enum {
		FastAerialPerspectiveDisabled = 0,
		FastAerialPerspectiveEnabled,
		FastAerialPerspectiveCount
	};
	PixelShader* RenderPathTracingPS[TransmittanceMethodCount][GroundGlobalIlluminationCount][ShadowmapCount][MultiScatApproxCount];
	PixelShader* RenderRayMarchingPS[MultiScatApproxCount][FastSkyCount][ColoredTransmittanceCount][FastAerialPerspectiveCount][ShadowmapCount];
	int currentTransPermutation = TransmittanceMethodLUT;
	bool currentShadowPermutation = false;
	float currentMultipleScatteringFactor = 1.0f;
	bool currentFastSky = true;
	bool currentAerialPerspective = true;
	bool currentColoredTransmittance = false;
	float currentAtmosphereHeight = -1.0f;

	Texture2D* mTransmittanceTex;
	Texture2D* MultiScattTex;
	Texture2D* MultiScattStep0Tex;
	Texture2D* mSkyViewLutTex;
	PixelShader* RenderTransmittanceLutPS;
	PixelShader* SkyViewLutPS[MultiScatApproxCount];
	PixelShader* CameraVolumesRayMarchPS[MultiScatApproxCount];
	ComputeShader*  NewMuliScattLutCS;

	// UI
	float uiCamHeight = 0.5f;
	float uiCamForward = -1.0f;
	float uiSunPitch = 0.45f;
	float uiSunYaw = 0.0f;
	int NumScatteringOrder = 4;

	bool  ShouldClearPathTracedBuffer = true;
	bool forceGenLut = false;
	bool uiDataInitialised = false;

	enum {
		MethodBruneton2017 = 0,
		MethodPathTracing,
		MethodRaymarching,
		MethodCount
	};
	int  uiRenderingMethod = MethodRaymarching;
	bool MethodSwitchDebug = true;
	int uiViewRayMarchMinSPP = 4;
	int uiViewRayMarchMaxSPP = 14;

	bool RenderTerrain = true;

	// Render functions
	void updateSkyAtmosphereConstant();
	void generateSkyAtmosphereLUTs();
	void generateSkyAtmosphereCameraVolumes();
	void renderSkyAtmosphereUsingLUTs();

	void renderTransmittanceLutPS();
	void renderNewMultiScattTexPS();
	void renderSkyViewLut();
	void renderPathTracing();
	void RenderSkyAtmosphereOverOpaque();
	void renderRayMarching();
	void generateSkyAtmosphereCameraVolumeWithRayMarch();
	void renderTerrain();
	void renderShadowmap();

	bool mPrintDebug = false;
	bool mClearDebugState = true;
	bool mUpdateDebugState = true;
	GpuDebugState mDebugState;
	GpuDebugState mDummyDebugState;

	void SaveState();
	void LoadState();
};


