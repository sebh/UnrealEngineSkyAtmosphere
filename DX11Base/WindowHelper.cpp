// Copyright Epic Games, Inc. All Rights Reserved.


#include "WindowHelper.h"


LRESULT CALLBACK WindowProcess(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WindowHelper *window = (WindowHelper*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
		break;
	}

	switch (message)
	{
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_NCMOUSELEAVE:
	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
		window->processMouseMessage(message, wParam, lParam);
		break;

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_CHAR:
	case WM_SYSCHAR:
		window->processKeyMessage(message, wParam, lParam);
		break;

	case WM_SIZE:
	//case WM_SIZING:	// When receiving that message, the backbuffer we get is null for some reason. Would be good to still see image on screen.
	// Also it seems that this is not even enough to handle a windo going full screen. Using Atl+Enter make things crash of lock up.
		if (wParam != SIZE_MINIMIZED)
		{
			window->processWindowSizeMessage(message, wParam, lParam);
		}
		break;
	}

	// Handle any messages the switch statement didn't
	return DefWindowProc(hWnd, message, wParam, lParam);
}


WindowHelper::WindowHelper(HINSTANCE hInstance, const RECT& clientRect, int nCmdShow, LPCWSTR windowName)
	: mHInstance(hInstance)
	, mNCmdShow(nCmdShow)
	, mClientRect(clientRect)
{
	mInput.init();

	// And create the rectangle that will allow it
	RECT rect = { 0, 0, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top }; // set the size, but not the position otherwise does not seem to work
	DWORD style = (WS_OVERLAPPEDWINDOW | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX); // WS_OVERLAPPED without edge resize, WS_OVERLAPPEDWINDOW with
	BOOL menu = false;
	AdjustWindowRect(&rect, style, menu);
	//Get the required window dimensions
	int WindowWidth = rect.right - rect.left; //Required width
	//WindowWidth += (2 * GetSystemMetrics(SM_CXFIXEDFRAME)); //Add frame widths
	int WindowHeight = rect.bottom - rect.top; //Required height
	//WindowHeight += GetSystemMetrics(SM_CYCAPTION); //Titlebar height
	//WindowHeight += GetSystemMetrics(SM_CYMENU); //Uncomment for menu bar height
	//WindowHeight += (2 * GetSystemMetrics(SM_CYFIXEDFRAME)); //Frame heights

	// clear out the window class for use
	ZeroMemory(&mWc, sizeof(WNDCLASSEX));
	// fill in the struct with the needed information
	mWc.cbSize = sizeof(WNDCLASSEX);
	mWc.style = CS_HREDRAW | CS_VREDRAW;
	mWc.lpfnWndProc = WindowProcess;
	mWc.hInstance = mHInstance;
	mWc.hCursor = LoadCursor(NULL, IDC_ARROW);
	mWc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	mWc.lpszClassName = L"WindowClass1";

	// register the window class
	RegisterClassEx(&mWc);

	// create the window and use the result as the handle
	mHWnd = CreateWindowEx(NULL,
		L"WindowClass1",					// name of the window class
		windowName,							// title of the window
		style,								// not resizable
		clientRect.top,						// x-position of the window
		clientRect.left,					// y-position of the window
		WindowWidth,						// width of the window
		WindowHeight,						// height of the window
		NULL,								// we have no parent window, NULL
		NULL,								// we aren't using menus, NULL
		hInstance,							// application handle
		NULL);								// used with multiple windows, NULL

	SetWindowLongPtr(mHWnd, GWLP_USERDATA, (LONG_PTR)(this));
	SetWindowPos(mHWnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER); // Make sure the pointer is cached  https://docs.microsoft.com/en-us/windows/desktop/api/winuser/nf-winuser-setwindowlongptra
}

WindowHelper::~WindowHelper()
{
}


void WindowHelper::showWindow()
{
	ShowWindow(mHWnd, mNCmdShow);
}


void WindowHelper::processMouseMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	// TODO WM_NCMOUSELEAVE 

	mInput.mInputStatus.mouseX = LOWORD(lParam);
	mInput.mInputStatus.mouseY = HIWORD(lParam);

	InputEvent event;
	event.mouseX = mInput.mInputStatus.mouseX;
	event.mouseY = mInput.mInputStatus.mouseY;
	switch (message)
	{
	case WM_MOUSEMOVE:
		event.type = etMouseMoved;
		break;
	case WM_LBUTTONDOWN:
		event.type = etMouseButtonDown;
		event.mouseButton = mbLeft;
		mInput.mInputStatus.mouseButtons[event.mouseButton] = true;
		break;
	case WM_MBUTTONDOWN:
		event.type = etMouseButtonDown;
		event.mouseButton = mbMiddle;
		mInput.mInputStatus.mouseButtons[event.mouseButton] = true;
		break;
	case WM_RBUTTONDOWN:
		event.type = etMouseButtonDown;
		event.mouseButton = mbRight;
		mInput.mInputStatus.mouseButtons[event.mouseButton] = true;
		break;
	case WM_LBUTTONUP:
		event.type = etMouseButtonUp;
		event.mouseButton = mbLeft;
		mInput.mInputStatus.mouseButtons[event.mouseButton] = false;
		break;
	case WM_MBUTTONUP:
		event.type = etMouseButtonUp;
		event.mouseButton = mbMiddle;
		mInput.mInputStatus.mouseButtons[event.mouseButton] = false;
		break;
	case WM_RBUTTONUP:
		event.type = etMouseButtonUp;
		event.mouseButton = mbRight;
		mInput.mInputStatus.mouseButtons[event.mouseButton] = false;
		break;
	case WM_LBUTTONDBLCLK:
		event.type = etMouseButtonDoubleClick;
		event.mouseButton = mbLeft;
		break;
	case WM_MBUTTONDBLCLK:
		event.type = etMouseButtonDoubleClick;
		event.mouseButton = mbMiddle;
		break;
	case WM_RBUTTONDBLCLK:
		event.type = etMouseButtonDoubleClick;
		event.mouseButton = mbRight;
		break;

	// TODO
	//case WM_MOUSEWHEEL:
	//case WM_MOUSEHWHEEL:
	}
	mInput.mInputEvents.push_back(event);
}

static InputKey translateKey(WPARAM wParam)
{
	switch (wParam)
	{
	case VK_RIGHT: 					return kRight;
	case VK_LEFT: 					return kLeft;
	case VK_DOWN: 					return kDown;
	case VK_UP:						return kUp;
	case VK_SPACE:					return kSpace;
	case VK_NUMPAD0:				return kNumpad0;
	case VK_NUMPAD1:				return kNumpad1;
	case VK_NUMPAD2:				return kNumpad2;
	case VK_NUMPAD3:				return kNumpad3;
	case VK_NUMPAD4:				return kNumpad4;
	case VK_NUMPAD5:				return kNumpad5;
	case VK_NUMPAD6:				return kNumpad6;
	case VK_NUMPAD7:				return kNumpad7;
	case VK_NUMPAD8:				return kNumpad8;
	case VK_NUMPAD9:				return kNumpad9;
	case VK_MULTIPLY:				return kMultiply;
	case VK_ADD:					return kAdd;
	case VK_SEPARATOR:				return kSeparator;
	case VK_SUBTRACT:				return kSubtract;
	case VK_DECIMAL:				return kDecimal;
	case VK_DIVIDE:					return kDivide;
	case VK_F1:						return kF1;
	case VK_F2:						return kF2;
	case VK_F3:						return kF3;
	case VK_F4:						return kF4;
	case VK_F5:						return kF5;
	case VK_F6:						return kF6;
	case VK_F7:						return kF7;
	case VK_F8:						return kF8;
	case VK_F9:						return kF9;
	case VK_F10:					return kF10;
	case VK_F11:					return kF11;
	case VK_F12:					return kF12;
	case VK_SHIFT:					return kShift;
	case VK_CONTROL:				return kControl;

	case '0': 						return k0;
	case '1': 						return k1;
	case '2': 						return k2;
	case '3': 						return k3;
	case '4': 						return k4;
	case '5': 						return k5;
	case '6': 						return k6;
	case '7': 						return k7;
	case '8': 						return k8;
	case '9': 						return k9;
	case 'A': 						return kA;
	case 'B': 						return kB;
	case 'C': 						return kC;
	case 'D': 						return kD;
	case 'E': 						return kE;
	case 'F': 						return kF;
	case 'G': 						return kG;
	case 'H': 						return kH;
	case 'I': 						return kI;
	case 'J': 						return kJ;
	case 'K': 						return kK;
	case 'L': 						return kL;
	case 'M': 						return kM;
	case 'N': 						return kN;
	case 'O': 						return kO;
	case 'P': 						return kP;
	case 'Q': 						return kQ;
	case 'R': 						return kR;
	case 'S': 						return kS;
	case 'T': 						return kT;
	case 'U': 						return kU;
	case 'V': 						return kV;
	case 'W': 						return kW;
	case 'X': 						return kX;
	case 'Y': 						return kY;
	case 'Z':						return kZ;

// TODO VK_GAMEPAD

	default:
		return kUnknown;
	}
}

void WindowHelper::processKeyMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	InputEvent event;
	event.mouseX = mInput.mInputStatus.mouseX;
	event.mouseY = mInput.mInputStatus.mouseY;

	event.key = translateKey(wParam);
	if (event.key == kUnknown)
		return;	// unkown key so do not register the event.

	switch (message)
	{
	case WM_KEYDOWN:
		event.type = etKeyDown;
		mInput.mInputStatus.keys[event.key] = true;
		break;
	case WM_KEYUP:
		event.type = etKeyUp;
		mInput.mInputStatus.keys[event.key] = false;
		break;

	case WM_SYSCHAR:
	case WM_CHAR:
		event.type = etKeyChar;
		break;
	}

	if (event.key == kControl)
	{
		mInput.mInputStatus.keys[kLcontrol] = GetKeyState(VK_LCONTROL)!=0;
		mInput.mInputStatus.keys[kRcontrol] = GetKeyState(VK_RCONTROL)!=0;
	}
	else if (event.key == kShift)
	{
		mInput.mInputStatus.keys[kLshift] = GetKeyState(VK_LSHIFT)!=0;
		mInput.mInputStatus.keys[kRshift] = GetKeyState(VK_RSHIFT)!=0;
	}

	mInput.mInputEvents.push_back(event);
}

void WindowHelper::processWindowSizeMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	mWindowResizedCallback(lParam);
}

bool WindowHelper::translateSingleMessage(MSG& msg)
{
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		// translate keystroke messages into the right format
		TranslateMessage(&msg);

		// Message translated
		return true;
	}

	// No message translated
	return false;
}
