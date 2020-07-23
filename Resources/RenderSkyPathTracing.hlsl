// Copyright Epic Games, Inc. All Rights Reserved.

#include "./Resources/RenderSkyCommon.hlsl"



////////////////////////////////////////////////////////////
// Path tracing context used by the integrators
////////////////////////////////////////////////////////////



struct PathTracingContext
{
	AtmosphereParameters Atmosphere;

	Ray ray;
	float3 P;
	float3 V;	// not always the view: sometimes it is the opposite of ray.d when one bounce has happened.

	float scatteringMajorant;
	float extinctionMajorant;

	float3 wavelengthMask;	// light source mask

	uint2 screenPixelPos;
	float randomState;

	int lastSurfaceIntersection;

	bool debugEnabled;
	bool singleScatteringRay;
	bool opaqueHit;

	bool hasScattered;
	float transmittance;
};

float random01(inout PathTracingContext ptc)
{
	// Trying to do the best noise here with simple function.
	// See https://www.shadertoy.com/view/ldjczd.
	float rnd = whangHashNoise(ptc.randomState, ptc.screenPixelPos.x, ptc.screenPixelPos.y);

	// This is some bad noise because not low discrepancy. I use such a noise for the EGSR comparisons but then it seems I forgot to checkin the code.

	//ptc.randomState++; return rnd;

#if 1
	//ptc.randomState += gTimeSec + gFrameId;	// hard to converge
	//ptc.randomState += gFrameId;
	ptc.randomState += gFrameId*1280;			// less pattern a the beginning be goes super wrong after a while...
#else
	uint animation = uint(gTime*123456.0);
	ptc.randomState += float((animation * 12345u) % 256u);
#endif

	return rnd;
}



////////////////////////////////////////////////////////////
// Misc functions
////////////////////////////////////////////////////////////



#define D_INTERSECTION_MEDIUM	0		// Participating media is going to be considered.
#define D_INTERSECTION_NULL		1		// Intersection with a null material surface surch as the top of the atmosphere.
#define D_INTERSECTION_GROUND	2		// Could be SOLID or SURFACE but let's call it ground in this particular case.

bool getNearestIntersection(inout PathTracingContext ptc,  in Ray ray, inout float3 P)
{
	float3 earthO = float3(0.0f, 0.0f, 0.0f);
	float t = raySphereIntersectNearest(ray.o, ray.d, earthO, ptc.Atmosphere.BottomRadius);
	float tTop = raySphereIntersectNearest(ray.o, ray.d, earthO, ptc.Atmosphere.TopRadius);

	ptc.lastSurfaceIntersection = D_INTERSECTION_MEDIUM;
	if (t < 0.0f)
	{
		// No intersection with earth: use a super large distance

		if (tTop < 0.0f)
		{
			t = 0.0f; // No intersection with earth nor atmosphere: stop right away 
			return false;
		}
		else
		{
			ptc.lastSurfaceIntersection = D_INTERSECTION_NULL;
			t = tTop;
		}
	}
	else
	{
		if (tTop > 0.0f)
		{
			t = min(tTop, t);
		}
		ptc.lastSurfaceIntersection = t == tTop ? D_INTERSECTION_NULL : D_INTERSECTION_GROUND;
	}

	P = ray.o + t * ray.d;
	return true; // mark as always valid
}



////////////////////////////////////////////////////////////
// Volume functions
////////////////////////////////////////////////////////////



bool insideAnyVolume(in PathTracingContext ptc, in Ray ray)
{
	const float h = length(ray.o);
	if ((h - ptc.Atmosphere.BottomRadius) < PLANET_RADIUS_OFFSET)
		return false;
	if ((ptc.Atmosphere.TopRadius - h) < -PLANET_RADIUS_OFFSET)
		return false;
	return true;
}

struct MediumSample
{
	float scattering;
	float absorption;
	float extinction;

	float scatteringMie;
	float absorptionMie;
	float extinctionMie;

	float scatteringRay;
	float absorptionRay;
	float extinctionRay;

	float scatteringOzo;
	float absorptionOzo;
	float extinctionOzo;

	float albedo;
};

MediumSample sampleMedium(in PathTracingContext ptc)
{
	const float3 WorldPos = ptc.P;
	MediumSampleRGB medium = sampleMediumRGB(WorldPos, ptc.Atmosphere);

	MediumSample s;
	s.scatteringMie = dot(ptc.wavelengthMask, medium.scatteringMie);
	s.absorptionMie = dot(ptc.wavelengthMask, medium.absorptionMie);
	s.extinctionMie = dot(ptc.wavelengthMask, medium.extinctionMie);

	s.scatteringRay = dot(ptc.wavelengthMask, medium.scatteringRay);
	s.absorptionRay = dot(ptc.wavelengthMask, medium.absorptionRay);
	s.extinctionRay = dot(ptc.wavelengthMask, medium.extinctionRay);

	s.scatteringOzo = dot(ptc.wavelengthMask, medium.scatteringOzo);
	s.absorptionOzo = dot(ptc.wavelengthMask, medium.absorptionOzo);
	s.extinctionOzo = dot(ptc.wavelengthMask, medium.extinctionOzo);

	s.scattering = dot(ptc.wavelengthMask, medium.scattering);
	s.absorption = dot(ptc.wavelengthMask, medium.absorption);
	s.extinction = dot(ptc.wavelengthMask, medium.extinction);
	s.albedo     = dot(ptc.wavelengthMask, medium.albedo);

	return s;
}



////////////////////////////////////////////////////////////
// Transmittance integrator
////////////////////////////////////////////////////////////



// Forward declaration
float Transmittance(
	inout PathTracingContext ptc,
	in float3 P0,
	in float3 P1,
	in float3 direction);

// Estimate a transmittance from ptc.P towards a direction.
float TransmittanceEstimation(in PathTracingContext ptc, in float3 direction)
{
	float beamTransmittance = 1.0f;
	float3 P0 = ptc.P + direction * RAYDPOS;
	float3 P1;

	float3 earthO = float3(0.0, 0.0, 0.0);
	float t = raySphereIntersectNearest(P0, direction, earthO, ptc.Atmosphere.BottomRadius);
	if (t > 0.0f)
	{
		beamTransmittance = 0.0f; // earth is intersecting
	}
	else
	{
		t = raySphereIntersectNearest(P0, direction, earthO, ptc.Atmosphere.TopRadius);
		P1 = P0 + t * direction;
		beamTransmittance = Transmittance(ptc, P0, P1, direction);
	}

	return beamTransmittance;
}



////////////////////////////////////////////////////////////
// Sampling functions
////////////////////////////////////////////////////////////



#define D_SCATT_TYPE_NONE 0
#define D_SCATT_TYPE_MIE  1
#define D_SCATT_TYPE_RAY  2
#define D_SCATT_TYPE_UNI  3
#define D_SCATT_TYPE_ABS  4	// Not really a scattering event: the trace has ended because absorbed

float calcHgPhasepdf(in PathTracingContext ptc, float cosTheta)
{
	return hgPhase(ptc.Atmosphere.MiePhaseG, cosTheta);
}

float calcHgPhaseInvertcdf(in PathTracingContext ptc, float zeta)
{
	const float G = ptc.Atmosphere.MiePhaseG;
	float one_plus_g2 = 1.0f + G * G;
	float one_minus_g2 = 1.0f - G * G;
	float one_over_2g = 0.5f / G;
	float t = (one_minus_g2) / (1.0f - G + 2.0f * G * zeta);
	return one_over_2g * (one_plus_g2 - t * t);	// Careful: one_over_2g undefined for g~=0
}

void phaseEvaluateSample(in PathTracingContext ptc, in float3 sampleDirection, in int ScatteringType, out float value, out float pdf)
{
	const float3 wi = sampleDirection;
	const float3 wo = ptc.V;
	float cosTheta = dot(wi, wo);

	if (ScatteringType == D_SCATT_TYPE_RAY)
	{
		value = RayleighPhase(cosTheta);
		pdf = 0.25 / PI;	// 1/4PI since no importance is used in this case.
	}
	else //if (ScatteringType == D_SCATT_TYPE_MIE)
	{
		value = hgPhase(ptc.Atmosphere.MiePhaseG, cosTheta);
#if MIE_PHASE_IMPORTANCE_SAMPLING
		pdf = value;
#else
		pdf = 0.25 / PI;	// 1/4PI since phaseGenerateSample is not importance sampling a direction.
#endif
	}
	//else // D_SCATT_TYPE_UNI
	//{
	//	pdf = uniformPhase();
	//	value = pdf;
	//}
}

void phaseGenerateSample(inout PathTracingContext ptc, out float3 newDirection, in int ScatteringType, out float value, out float pdf)
{
	if (ScatteringType == D_SCATT_TYPE_RAY)
	{
		// Evaluate a random direction
		newDirection = getUniformSphereSample(random01(ptc), random01(ptc));
	}
	else //if (ScatteringType == D_SCATT_TYPE_MIE)
	{
#if MIE_PHASE_IMPORTANCE_SAMPLING
		// Evaluate a random direction with importance sampling
		float phi = 2.0f * PI * random01(ptc);
		float cosTheta = calcHgPhaseInvertcdf(ptc, random01(ptc));
		float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));	// max to make sqrt safe. Can make the GPU hang otherwise...

		const float3 wo = ptc.V;
		float3 mainDir = -wo;		// -wo, we use the same invertcdf convention as in http://www.pbr-book.org/3ed-2018/Light_Transport_II_Volume_Rendering/Sampling_Volume_Scattering.html#fragment-ComputedirectionmonowiforHenyey--Greensteinsample-0
		float3 t0, t1;

		const float3 up = float3(0.0, 1.0, 0.0);
		const float3 right = float3(1.0, 0.0, 0.0);
		if (abs(dot(mainDir, up)) > 0.01)
		{
			t0 = normalize(cross(mainDir, up));
			t1 = normalize(cross(mainDir, t0));
		}
		else
		{
			t0 = normalize(cross(mainDir, right));
			t1 = normalize(cross(mainDir, t0));
		}

		newDirection = sinTheta * sin(phi) * t0 + sinTheta * cos(phi) * t1 + cosTheta * mainDir;
		//newDirection = normalize(newDirection);
#else
		newDirection = getUniformSphereSample(random01(ptc), random01(ptc));
#endif
	}
	//else // D_SCATT_TYPE_UNI
	//{
	//	// Evaluate a random direction
	//	newDirection = getUniformSphereSample(random01(ptc), random01(ptc));
	//}

	// From direction, evaluate the phase value and pdf
	phaseEvaluateSample(ptc, newDirection, ScatteringType, value, pdf);
}


void lightGenerateSample(inout PathTracingContext ptc, out float3 direction, out float value, out float pdf, out float beamTransmittance, out bool isDeltaLight)
{
	direction = sun_direction;
	pdf = 1;
	isDeltaLight = true;

	value = dot(gSunIlluminance, ptc.wavelengthMask);			// Value not taking into account trasmittance

	beamTransmittance = TransmittanceEstimation(ptc, direction);// Compute transmitance through atmosphere
}


float getShadow(in AtmosphereParameters Atmosphere, float3 P)
{
	// First evaluate opaque shadow
	float4 shadowUv = mul(gShadowmapViewProjMat, float4(P + float3(0.0, 0.0, -Atmosphere.BottomRadius), 1.0));
	//shadowUv /= shadowUv.w;	// not be needed as it is an ortho projection
	shadowUv.x = shadowUv.x*0.5 + 0.5;
	shadowUv.y = -shadowUv.y*0.5 + 0.5;
	if (all(shadowUv.xyz >= 0.0) && all(shadowUv.xyz < 1.0))
	{
		return ShadowmapTexture.SampleCmpLevelZero(samplerShadow, shadowUv.xy, shadowUv.z);
	}
	return 1.0f;
}



////////////////////////////////////////////////////////////
// Integrators
////////////////////////////////////////////////////////////



#if TRANSMITANCE_METHOD == 0

//// Delta tracking
float Transmittance(
	inout PathTracingContext ptc,
	in float3 P0,
	in float3 P1,
	in float3 direction)
{
#if SHADOWMAP_ENABLED
	// First evaluate opaque shadow
	if (getShadow(ptc.Atmosphere, P0) <= 0.0)
		return 0.0;
#endif
	float distance = length(P0 - P1);

	bool terminated = false;
	float t = 0;
	do
	{
		float zeta = random01(ptc);
		t = t + infiniteTransmittanceIS(ptc.extinctionMajorant, zeta);

		if (t > distance)
			break; // Did not terminate in the volume

		// Update the shading context
		float3 P = P0 + t * direction;
		ptc.P = P;

		// Evaluate the local absorption after updating the shading context
		float extinction = sampleMedium(ptc).extinction;
		float xi = random01(ptc);
		if (xi < (extinction / ptc.extinctionMajorant))
		{
			terminated = true;
		}

#if DEBUGENABLED // Transmittance estimation point
		if (ptc.debugEnabled)
		{
			float3 color = terminated ? float3(0.5, 0.0, 0.0) : float3(0.0, 0.5, 0.0);
			addGpuDebugCross(ToDebugWorld + ptc.P, color, 1.0);
			addGpuDebugLine(ToDebugWorld + P0, ToDebugWorld + ptc.P, float3(0.5, 0.5, 0.5));
		}
#endif
	} while (!terminated);

	if (terminated)
		return 0.0;
	else
		return 1.0;
}

#elif TRANSMITANCE_METHOD == 1

//// Ratio tracking from http://drz.disneyresearch.com/~jnovak/publications/RRTracking/index.html
float Transmittance(
	inout PathTracingContext ptc,
	in float3 P0,
	in float3 P1,
	in float3 direction)
{
#if SHADOWMAP_ENABLED
	// First evaluate opaque shadow
	if (getShadow(ptc.Atmosphere, P0) <= 0.0)
		return 0.0;
#endif
	float distance = length(P0 - P1);
	float3 dir = float3(P1 - P0) / distance;

	float t = 0;
	float transmittance = 1.0f;
	do
	{
		float zeta = random01(ptc);
		t = t + infiniteTransmittanceIS(ptc.extinctionMajorant, zeta);

		// Update the shading context
		float3 P = P0 + t * dir;
		ptc.P = P;

		if (t > distance)
			break; // Did not terminate in the volume

		float extinction = sampleMedium(ptc).extinction;
		transmittance *= 1.0f - max(0.0f, extinction / ptc.extinctionMajorant);

#if DEBUGENABLED // Transmittance estimation point
		if (ptc.debugEnabled)
		{
			float3 color = lerp(float3(0.0, 0.5, 0.0), float3(0.5, 0.0, 0.0), float3(transmittance, transmittance, transmittance));
			addGpuDebugCross(ToDebugWorld + ptc.P, color, 1.0);
			addGpuDebugLine(ToDebugWorld + P0, ToDebugWorld + ptc.P, float3(0.5, 0.5, 0.5));
		}
#endif
	} while (true);
	return saturate(transmittance);
}

#elif TRANSMITANCE_METHOD == 2

float Transmittance(
	inout PathTracingContext ptc,
	in float3 P0,
	in float3 P1,
	in float3 direction)
{
#if SHADOWMAP_ENABLED
	// First evaluate opaque shadow
	if (getShadow(ptc.Atmosphere, P0) <= 0.0)
		return 0.0;
#endif

	// Second evaluate transmittance due to participating media
	float viewHeight = length(P0);
	const float3 UpVector = P0 / viewHeight;
	float viewZenithCosAngle = dot(direction, UpVector);
	float2 uv;
	LutTransmittanceParamsToUv(ptc.Atmosphere, viewHeight, viewZenithCosAngle, uv);
	const float3 trans = TransmittanceLutTexture.SampleLevel(samplerLinearClamp, uv, 0).rgb;

#if DEBUGENABLED // Transmittance value
	if (ptc.debugEnabled)
	{
		addGpuDebugLine(ToDebugWorld + P0, ToDebugWorld + P0 + direction*100.0f, trans);
	}
#endif

	return dot(trans, ptc.wavelengthMask);
}

#else 

#error Transmittance needs to be implemented.

#endif



bool Integrate(
	inout PathTracingContext ptc,
	in Ray wi,
	inout float3 P, // closestHit
	out float L,
	out float transmittance,
	out float weight,
	inout Ray wo,
	inout int OutScatteringType)
{
	OutScatteringType = D_SCATT_TYPE_NONE;

	float3 P0 = ptc.P;
	if (!getNearestIntersection(ptc, createRay(P0, wi.d), P))
		return false;
	float tMax = length(P - P0);

	if (ptc.singleScatteringRay)
	{
		float2 pixPos = ptc.screenPixelPos;
		float3 ClipSpace = float3((pixPos / float2(gResolution))*float2(2.0, -2.0) - float2(1.0, -1.0), 0.5);
		ClipSpace.z = ViewDepthTexture[pixPos].r;
		if (ClipSpace.z < 1.0f)
		{
			ptc.opaqueHit = true;
			float4 DepthBufferWorldPos = mul(gSkyInvViewProjMat, float4(ClipSpace, 1.0));
			DepthBufferWorldPos /= DepthBufferWorldPos.w;

			float tDepth = length(DepthBufferWorldPos.xyz - (ptc.P+float3(0.0, 0.0, -ptc.Atmosphere.BottomRadius))); // apply earth offset to go back to origin as top of earth mode.
			if (tDepth < tMax)
			{
				// P and ptc.P will br written in so no need to update them
				tMax = tDepth;
			}
		}
	}

	bool eventScatter = false;
	bool eventAbsorb = false;
	float extinction = 0;
	float scattering = 0;
	float albedo = 0;
	transmittance = 1.0f;

	float t = 0;
	do
	{
		if (ptc.extinctionMajorant == 0.0) break; // cannot importance sample, so stop right away


		float zeta = random01(ptc);
		t = t + infiniteTransmittanceIS(ptc.extinctionMajorant, zeta); // unbounded domain proportional with PDF to the transmittance

		if (t >= tMax)
		{
			break; // Did not terminate in the volume
		}

		// Update the shading context
		float3 P1 = P0 + t * wi.d;
		ptc.P = P1;

#if DEBUGENABLED // Sample point
		if (ptc.debugEnabled) { addGpuDebugCross(ToDebugWorld + ptc.P, float3(0.5, 1.0, 1.0), 0.5); }
#endif

		// Recompute the local extinction after updating the shading context
		MediumSample medium = sampleMedium(ptc);
		extinction = medium.extinction;
		scattering = medium.scattering;
		albedo = medium.albedo;
		float xi = random01(ptc);
		if (xi <= (medium.scattering / ptc.extinctionMajorant))
		{
			eventScatter = true;

			float zeta = random01(ptc);
			if (zeta < medium.scatteringMie / medium.scattering)
			{
				OutScatteringType = D_SCATT_TYPE_MIE;
			}
			else
			{
				OutScatteringType = D_SCATT_TYPE_RAY;
			}
		}
		else if (xi < (medium.extinction / ptc.extinctionMajorant))	// on top of scattering, as extinction = scattering + absorption
			eventAbsorb = true;
		// else null event
	} while (!(eventScatter || eventAbsorb));

	if (eventScatter && all(extinction > 0.0))
	{
#if DEBUGENABLED // Path
		if (ptc.debugEnabled) { addGpuDebugLine(ToDebugWorld + P0, ToDebugWorld + ptc.P, float3(0, 1, 0)); }
#endif
		ptc.lastSurfaceIntersection = D_INTERSECTION_MEDIUM;
		P = ptc.P;

		const float Tr = 1.0;										// transmittance to previous event (view since in this case we only consider single scattering)
		const float pdf = infiniteTransmittancePDF(scattering, Tr); // This must use scattering since this is the scattering pdf case after scatter/absorb separation
		transmittance = Tr;

		// Note that pdf already has extinction in it, so we should avoid the multiply and divide; it is shown here for clarity
		weight = albedo * extinction / pdf;
	}
	else if (eventAbsorb)
	{
#if DEBUGENABLED // Path
		if (ptc.debugEnabled) { addGpuDebugLine(ToDebugWorld + P0, ToDebugWorld + ptc.P, float3(0, 1, 0)); }
#endif
		OutScatteringType = D_SCATT_TYPE_ABS;
		ptc.lastSurfaceIntersection = D_INTERSECTION_MEDIUM;
		P = ptc.P;

		transmittance = 0.0;	// will set throughput to 0 and stop processing loop
		weight = 0.0;			// will remove lighting
	}
	else
	{
		// Max distance reached without absorption or scattering event. Keep lastSurfaceIntersection computed in getNearestIntersection above.
		P = P0 + tMax * wi.d; // out of the volume range

#if DEBUGENABLED // Path
		if (ptc.debugEnabled) { addGpuDebugLine(ToDebugWorld + P0, ToDebugWorld + P, float3(0, 1, 0)); }
#endif

		transmittance = 1.0f;
		float pdf = transmittance;
		weight = 1.0 / pdf;
	}

	L = 0.0f;
	wo = createRay(P + wi.d*RAYDPOS, wi.d);

	return true;
}



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////



// This is the volume path tracer core loop. It was tested on other volumetric data and compared against PBRT and Mitsuba (e.g. https://twitter.com/SebHillaire/status/1076144032961757185 or https://twitter.com/SebHillaire/status/1073568200762245122)
// It could be improved by really following the Radiance transfert Equation path integral as a loop for each event.
float LightIntegratorInner(
	VertexOutput Input,
	inout PathTracingContext ptc)
{
	//////////
	float2 pixPos = Input.position.xy;
	float3 ClipSpace = float3((pixPos / float2(gResolution))*float2(2.0, -2.0) - float2(1.0, -1.0), 0.5);
	float4 HPos = mul(gSkyInvViewProjMat, float4(ClipSpace, 1.0));

	float earthR = ptc.Atmosphere.BottomRadius;
	float3 earthO = float3(0.0, 0.0, 0.0);

	float3 WorldDir = normalize(HPos.xyz / HPos.w - camera);
	float3 camPos = camera + float3(0, 0, earthR);
	float3 sunDir = sun_direction;

	//////////

	float L = 0.0f;
	float throughput = 1.0f;
	Ray ray = createRay(camPos, WorldDir); // ray from camera to pixel

	// Path tracer loop
	// Based on Production Volume Rendering https://graphics.pixar.com/library/ProductionVolumeRendering/

	//float t = raySphereIntersectNearest(camPos, WorldDir, float3(0.0f, 0.0f, 0.0f), ptc.Atmosphere.BottomRadius); if (t > 0.0f) {	return 0.0;	} // hit earth: stop tracing, in game can be a test against the depth buffer

	// Move to top atmospehre
	float3 prevRayO = ray.o;
	if (MoveToTopAtmosphere(ray.o, ray.d, ptc.Atmosphere.TopRadius))
	{
#if DEBUGENABLED // Atmosphere entrance
		if (ptc.debugEnabled) { addGpuDebugLine(ToDebugWorld + prevRayO, ToDebugWorld + ray.o, float3(0, 0, 1)); }
#endif
		camPos = ray.o;
	}
	else
	{
		// Ray is not intersecting the atmosphere
		return L;
	}

	float3 P = ray.o;			// Default start point when the camera is inside a volume
	float3 prevDebugPos = P;

	bool hasScattered = false;
	int step = 0; 
	while (step < gScatteringMaxPathDepth && throughput>0.0)
	{
#if GROUND_GI_ENABLED
		if (ptc.lastSurfaceIntersection == D_INTERSECTION_GROUND && !hasScattered) // ptc.hasScattered is checked to avoid colored ground
		{
			// If ground is directly visible as the first intersection (has not scattered before) then we should stop tracing for the ground to not show up.
			break;
		}

		// Handle collision with opaque
		if (ptc.lastSurfaceIntersection == D_INTERSECTION_GROUND && hasScattered) // ptc.hasScattered is checked to avoid colored ground
		{
			// Offset position to be always be the volume 
			float h = length(ray.o);
			float3 UpVector = ray.o / h;
			ray.o = ray.o + UpVector * 0.025f;
			P = ray.o;

			// Could also add emissive contribution to luminance here to simulate city lights.
			float SunTransmittance = TransmittanceEstimation(ptc, sunDir);

			// Generate a new up direction assuming a diffuse surface (incorrectly as it would need to follow a cosine distribution with matching pdf, but ok as it is not used for comparison images)
			float3 newDirection = getUniformSphereSample(random01(ptc), random01(ptc));
			const float dotVec = dot(newDirection, UpVector);
			if (dotVec < 0.0f)
			{
				newDirection = newDirection - 2.0f * dotVec * UpVector;
			}
			ray.d = newDirection;
			//const float NdotV = saturate(dotVec);
			const float NdotL = saturate(dot(UpVector, sunDir));
			const float albedo = saturate(dot(ptc.Atmosphere.GroundAlbedo, ptc.wavelengthMask));
			const float DiffuseEval = albedo * (1.0f / PI);
			const float DiffusePdf = 1;

			// Also update throughput based on albedo.
			L += throughput * SunTransmittance * (DiffuseEval * NdotL * dot(gSunIlluminance, ptc.wavelengthMask));
			throughput *= DiffuseEval / DiffusePdf;

#if 0 //DEBUGENABLED // Bounce on ground 
			if (ptc.debugEnabled)
			{
				float3 DebugP = ToDebugWorld + ray.o + UpVector * 0.1;
				addGpuDebugLine(DebugP, DebugP + newDirection * 10, float3(1, 1, 1));
				addGpuDebugCross(ToDebugWorld + ray.o, float3(0, 0, 1), 10.0);
			}
#endif 
		}
#endif

		// store current context: ray, intersection point P, etc.
		ptc.ray = ray;
		ptc.P = P + ray.d * RAYDPOS;
		ptc.V = -ray.d;

		// Compute next ray from last intersection.
		// From there, next ray is the reference ray for volumetric interactions.
		Ray nextRay = createRay(P + ray.d *RAYDPOS, ray.d);
		int ScatteringType = D_SCATT_TYPE_NONE;

		if (insideAnyVolume(ptc, nextRay))
		{
			float Lv = 0.0;
			float transmittance = 0.0;
			float weight = 0.0;

			bool hasCollision = Integrate(ptc, nextRay, P, Lv, transmittance, weight, nextRay, ScatteringType); // Run volume integrator on the considered range

#if  DEBUGENABLED // Path vertex type
			if (ptc.debugEnabled)
			{
				float3 DebugP = ptc.P;
				float3 color = float3(1.0, 0.0, 0.0);
				float size = 25.0f;
				bool doPrint = true;
				if (!hasCollision)
				{
					// Big trouble so big red cross
				}
				else
				{
					if (ptc.lastSurfaceIntersection == D_INTERSECTION_MEDIUM)
					{
						if (ScatteringType == D_SCATT_TYPE_MIE || ScatteringType == D_SCATT_TYPE_RAY)
						{
							color = float3(1.0, 1.0, 0.0);
							size = 2.0;
					//		doPrint = false;
						}
						else if (ScatteringType == D_SCATT_TYPE_ABS)
						{
							// This can happen when we get out of the volume during integration
							color = float3(0.25, 0.25, 0.25);
							size = 2.0;
					//		doPrint = false;
						}
					}
					else
					{
						if (ptc.lastSurfaceIntersection == D_INTERSECTION_GROUND)
						{
							DebugP = nextRay.o;
							color = float3(204, 102, 0) / 255.0f;
							size = 5.0;
						}
						else if (ptc.lastSurfaceIntersection == D_INTERSECTION_NULL)
						{
							color = float3(0.5, 0.5, 1.0);
							size = 2.0;
						}
					}
				}
				if(doPrint)
					addGpuDebugCross(ToDebugWorld + DebugP, color, 2.0);
			}
#endif

			if (hasCollision && ptc.lastSurfaceIntersection == D_INTERSECTION_MEDIUM)
			{
				float lightL;
				float bsdfL;
				float beamTransmittance;
				float lightPdf, bsdfPdf;
				float misWeight;
				float3 sampleDirection;
				bool isDeltaLight;

				// What we do here: (1) sample light, (2) apply phase, (3) update throughput, No MIS, matches PBRT perfectly
				lightGenerateSample(ptc, sampleDirection, lightL, lightPdf, beamTransmittance, isDeltaLight);
				phaseEvaluateSample(ptc, sampleDirection, ScatteringType, bsdfL, bsdfPdf);

#if DUALSCATTERING_ENABLED
				// Trying some approximation to multi scattering
				const float globalL = transmittance * weight * (lightL) / (lightPdf); // We do not apply beamTransmittance here because otherwise no multi scatter in shadowed region. But now strong forward mie scattering can leak through opaque.

				float multiScatteredLuminance = 0.0f;	// Absorption
				if (ScatteringType == D_SCATT_TYPE_MIE || ScatteringType == D_SCATT_TYPE_RAY)
				{
					MediumSample medium = sampleMedium(ptc);

					float viewHeight = length(ptc.P);
					const float3 UpVector = ptc.P / viewHeight;
					float SunZenithCosAngle = dot(sun_direction, UpVector);

					multiScatteredLuminance += dot(ptc.wavelengthMask, GetMultipleScattering(ptc.Atmosphere, medium.scattering, medium.extinction, ptc.P, SunZenithCosAngle));
				}

				// multiScatteredLuminance is the integral over the sphere of the incoming luminance assuming a uniform phase function at current point. 
				// The phase and scattering probability is part of the LUT alread. So we do not multiply with bsdfL as this is for the directional sun only.
				Lv += globalL * ((beamTransmittance*bsdfL) + multiScatteredLuminance);
				L += throughput * Lv;
				throughput *= transmittance;

				hasScattered = true;
				break;
#else
				// Default lighting code
				Lv += lightL * bsdfL * beamTransmittance / (lightPdf);
				L += weight * throughput * Lv * transmittance;
				throughput *= transmittance;
#endif

				if (insideAnyVolume(ptc, nextRay))
				{
					hasScattered = true;
					ptc.singleScatteringRay = false;
#if MIE_PHASE_IMPORTANCE_SAMPLING
					// This code is also valid for the uniform sampling, but we optimise it out if we do not use anisotropic phase importance sampling.
					float phaseValue, phasePdf;
					phaseGenerateSample(ptc, nextRay.d, ScatteringType, phaseValue, phasePdf);
					throughput *= phaseValue / phasePdf;
#else
					nextRay.d = getUniformSphereSample(random01(ptc), random01(ptc));	// Simple uniform distribution.
#endif
				}
			} 
			//else if (insideAnyVolume(ptc, nextRay))	// Not needed because no internal acceleration structure
			//{
			////	step--;	// to not have internal subdivision affect path depth
			//	break;
			//}
			else if(ptc.lastSurfaceIntersection == D_INTERSECTION_NULL || (ptc.lastSurfaceIntersection == D_INTERSECTION_GROUND && !GROUND_GI_ENABLED))
			{
				// No intersection within the range
				//return ptc.wavelengthMask.g*0.5;
				break;
			}
		}
		else
		{
#if DEBUGENABLED // Unhandled/wrong Path vertex
			if (ptc.debugEnabled)
			{
				float3 DebugP = ToDebugWorld + ptc.P;
				addGpuDebugLine(DebugP, DebugP + float3(0, 15, 0), float3(1.0, 0.0, 0.0));
			}
#endif
			// Exit as we have exited the atmosphere volume
			break;
		}

		ray = nextRay;
		step++;
	}

	if (!hasScattered && !ptc.opaqueHit && step==0)
	{
		L += throughput * dot(ptc.wavelengthMask, GetSunLuminance(camPos, WorldDir, ptc.Atmosphere.BottomRadius));
	}

	ptc.transmittance = !hasScattered ? throughput : 0.0f;
	return L;
}



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////



struct PixelOutputStruct
{
	float4 Luminance		: SV_TARGET0;
#if GAMEMODE_ENABLED==0
	float4 Transmittance	: SV_TARGET1;
#endif
};
PixelOutputStruct RenderPathTracingPS(VertexOutput Input)
{
	float2 pixPos = Input.position.xy;

	const float NumSample = 1.0f;
	float3 OutputLuminance = 0.0f;

	float3 ScatteringMajorant = rayleigh_scattering + mie_scattering;
	float3 ExtinctionMajorant = rayleigh_scattering + mie_extinction + absorption_extinction;

	PathTracingContext ptc = (PathTracingContext)0;
	ptc.Atmosphere = GetAtmosphereParameters();
	ptc.extinctionMajorant = mean(ExtinctionMajorant);
	ptc.scatteringMajorant = mean(ScatteringMajorant);
	ptc.lastSurfaceIntersection == D_INTERSECTION_MEDIUM;
	ptc.P = 0.0f;
	ptc.V = 0.0f;
	ptc.ray = createRay(0.0f, 0.0f);
	ptc.screenPixelPos = pixPos;
	ptc.randomState = (pixPos.x + pixPos.y*float(gResolution.x)) + uint(gFrameId * 123u) % 32768u;
	ptc.singleScatteringRay = true;
	ptc.opaqueHit = false;
	ptc.transmittance = 1.0f;	// initialise to 1 for above atmosphere transmittance to be 1..
	ptc.debugEnabled = all(ptc.screenPixelPos == gMouseLastDownPos);


	//ptc.debugEnabled = ptc.screenPixelPos.x % 512 ==0 && ptc.screenPixelPos.y % 512 == 0;

#if 0
	float zeta = random01(ptc);
#else
	// Using bluenoise as a first scramble for wavelength selection for delta tracking
	const float noise = BlueNoise2dTexture[(pixPos * 7) % 64].r;
	float zeta = noise + (gFrameId % 3) / 3.0;
	zeta = zeta > 1.0f ? zeta - 1.0f : zeta;
#endif



	float3 mask = 0.0f;
	if (zeta < 1.0 / 3.0)
	{
		ptc.extinctionMajorant = ExtinctionMajorant.r;
		ptc.scatteringMajorant = ScatteringMajorant.r;
		ptc.wavelengthMask = float3(1.0, 0.0, 0.0);
	}
	else if (zeta < 2.0 / 3.0)
	{
		ptc.extinctionMajorant = ExtinctionMajorant.g;
		ptc.scatteringMajorant = ScatteringMajorant.g;
		ptc.wavelengthMask = float3(0.0, 1.0, 0.0);
	}
	else
	{
		ptc.extinctionMajorant = ExtinctionMajorant.b;
		ptc.scatteringMajorant = ScatteringMajorant.b;
		ptc.wavelengthMask = float3(0.0, 0.0, 1.0);
	}

	float wavelengthPdf = 1.0 / 3.0;
	const float3 wavelengthWeight = ptc.wavelengthMask / wavelengthPdf;

	OutputLuminance = LightIntegratorInner(Input, ptc);

	PixelOutputStruct output;

	//if (pixPos.x < 512 && pixPos.y < 512)
	//{
	//	output.Luminance = float4(0.2*SkyViewLutTexture.SampleLevel(samplerLinearClamp, pixPos / float2(512, 512), 0).rgb, 1.0);
	//	output.Transmittance = float4(0,0,0,1);
	//	return output;
	//}

#if GAMEMODE_ENABLED
	output.Luminance = float4(OutputLuminance   * wavelengthWeight, dot(ptc.transmittance * wavelengthWeight, float3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f)));
#else
	output.Luminance = float4(OutputLuminance   * wavelengthWeight, 1.0f);
	output.Transmittance = float4(ptc.transmittance * wavelengthWeight, 1.0f);
#endif
	return output;
}


