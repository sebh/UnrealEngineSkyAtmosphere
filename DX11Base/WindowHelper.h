// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include <windows.h>
#include <windowsx.h>
#include "WindowInput.h"

#include <functional>

class WindowHelper
{
public:
	WindowHelper(
		HINSTANCE hInstance, 
		const RECT& clientRect, 
		int nCmdShow,
		LPCWSTR windowName);
	~WindowHelper();

	void showWindow();

	bool translateSingleMessage(MSG& msg);

	const WindowInputData& getInputData()
	{
		return mInput;
	}
	void clearInputEvents()
	{
		mInput.mInputEvents.clear();
	}

	const HWND getHwnd() { return mHWnd; }

	void processMouseMessage(UINT message, WPARAM wParam, LPARAM lParam);
	void processKeyMessage(UINT message, WPARAM wParam, LPARAM lParam);
	void processWindowSizeMessage(UINT message, WPARAM wParam, LPARAM lParam);

	void setWindowResizedCallback(std::function<void(LPARAM lParam)> windowResizedCallback) { mWindowResizedCallback = windowResizedCallback; }

private:
	WindowHelper();


	HINSTANCE		mHInstance;			/// The application instance
	HWND			mHWnd;				/// The handle for the window, filled by a function
	WNDCLASSEX		mWc;				/// This struct holds information for the window class
	RECT			mClientRect;		/// The client rectangle where we render into
	int				mNCmdShow;			/// Window show cmd

	WindowInputData mInput;				/// input event and status (mouse, keyboard, etc.)

	std::function<void(LPARAM lParam)> mWindowResizedCallback = [&](LPARAM lParam){};
};


