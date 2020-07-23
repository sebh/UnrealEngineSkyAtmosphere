// Copyright Epic Games, Inc. All Rights Reserved.


#include "Game.h"

// basic file operations
#include <iostream>
#include <fstream>

void Game::SaveState()
{
	std::ofstream myfile;
	myfile.open("state.txt");
	myfile.write(reinterpret_cast<const char*>(&AtmosphereInfos), sizeof(AtmosphereInfo));
	myfile.write(reinterpret_cast<const char*>(&AtmosphereInfosSaved), sizeof(AtmosphereInfo));
	myfile.write(reinterpret_cast<const char*>(&mCamPos), sizeof(float3));
	myfile.write(reinterpret_cast<const char*>(&mCamPosFinal), sizeof(float3));
	myfile.write(reinterpret_cast<const char*>(&mViewDir), sizeof(float3));
	myfile.write(reinterpret_cast<const char*>(&mSunDir), sizeof(float3));
	myfile.write(reinterpret_cast<const char*>(&mSunIlluminanceScale), sizeof(float));
	myfile.write(reinterpret_cast<const char*>(&viewPitch), sizeof(float));
	myfile.write(reinterpret_cast<const char*>(&viewYaw), sizeof(float));
	myfile.write(reinterpret_cast<const char*>(&uiCamHeight), sizeof(float));
	myfile.write(reinterpret_cast<const char*>(&uiCamForward), sizeof(float));
	myfile.write(reinterpret_cast<const char*>(&uiSunPitch), sizeof(float));
	myfile.write(reinterpret_cast<const char*>(&uiSunYaw), sizeof(float));
	myfile.write(reinterpret_cast<const char*>(&NumScatteringOrder), sizeof(int));
	myfile.close();
}

void Game::LoadState()
{
	std::ifstream myfile("state.txt");
	if (myfile.is_open())
	{
		myfile.read(reinterpret_cast<char*>(&AtmosphereInfos), sizeof(AtmosphereInfo));
		myfile.read(reinterpret_cast<char*>(&AtmosphereInfosSaved), sizeof(AtmosphereInfo));
		myfile.read(reinterpret_cast<char*>(&mCamPos), sizeof(float3));
		myfile.read(reinterpret_cast<char*>(&mCamPosFinal), sizeof(float3));
		myfile.read(reinterpret_cast<char*>(&mViewDir), sizeof(float3));
		myfile.read(reinterpret_cast<char*>(&mSunDir), sizeof(float3));
		myfile.read(reinterpret_cast<char*>(&mSunIlluminanceScale), sizeof(float));
		myfile.read(reinterpret_cast<char*>(&viewPitch), sizeof(float));
		myfile.read(reinterpret_cast<char*>(&viewYaw), sizeof(float));
		myfile.read(reinterpret_cast<char*>(&uiCamHeight), sizeof(float));
		myfile.read(reinterpret_cast<char*>(&uiCamForward), sizeof(float));
		myfile.read(reinterpret_cast<char*>(&uiSunPitch), sizeof(float));
		myfile.read(reinterpret_cast<char*>(&uiSunYaw), sizeof(float));
		myfile.read(reinterpret_cast<char*>(&NumScatteringOrder), sizeof(int));
		myfile.close();

		ShouldClearPathTracedBuffer = true;
		forceGenLut = true;
		uiDataInitialised = false;
	}
}



