// Copyright Epic Games, Inc. All Rights Reserved.


#include "DirectXMath.h"
using namespace DirectX;

typedef unsigned char		uint8;
typedef unsigned short		uint16;
typedef unsigned int		uint32;
typedef char				int8;
typedef short				int16;
typedef long				int32;
//typedef float				float;

typedef XMMATRIX float4x4;
typedef XMFLOAT3X3 float3x3;
typedef XMVECTOR Vector4;
typedef XMFLOAT4 float4;
typedef XMFLOAT3 float3;
typedef XMFLOAT2 float2;

#define CLAMP(x, x0, x1) (x < x0 ? x0 : (x > x1 ? x1 : x))


