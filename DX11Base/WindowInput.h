// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include <vector>

enum InputKey
{
	kLeft,
	kRight,
	kUp,
	kDown,
	kSpace,
	kNumpad0,
	kNumpad1,
	kNumpad2,
	kNumpad3,
	kNumpad4,
	kNumpad5,
	kNumpad6,
	kNumpad7,
	kNumpad8,
	kNumpad9,
	kMultiply,
	kAdd,
	kSeparator,
	kSubtract,
	kDecimal,
	kDivide,
	kF1,
	kF2,
	kF3,
	kF4,
	kF5,
	kF6,
	kF7,
	kF8,
	kF9,
	kF10,
	kF11,
	kF12,

	kShift,			// event and input status
	kControl,		// event and input status
	kLshift,		// never an event, only set on the input status
	kRshift,		// never an event, only set on the input status
	kLcontrol,		// never an event, only set on the input status
	kRcontrol,		// never an event, only set on the input status

	k0,
	k1,
	k2,
	k3,
	k4,
	k5,
	k6,
	k7,
	k8,
	k9,
	kA,
	kB,
	kC,
	kD,
	kE,
	kF,
	kG,
	kH,
	kI,
	kJ,
	kK,
	kL,
	kM,
	kN,
	kO,
	kP,
	kQ,
	kR,
	kS,
	kT,
	kU,
	kV,
	kW,
	kX,
	kY,
	kZ,

	kCount,
	kUnknown
};
enum InputMouseButton
{
	mbLeft,
	mbMiddle,
	mbRight,
	mbCount
};
enum InputEventType
{
	etKeyUp,
	etKeyDown,
	etKeyChar,
	etMouseMoved,
	etMouseButtonUp,
	etMouseButtonDown,
	etMouseButtonDoubleClick,
	etCount
};

struct InputEvent
{
	InputEventType type;
	union 
	{
		InputKey key;
		InputMouseButton mouseButton;
	};
	int  mouseX;
	int  mouseY;
};

struct WindowInputStatus
{

	bool keys[kCount];
	bool mouseButtons[mbCount];
	int  mouseX;
	int  mouseY;

	void init()
	{
		memset(this, 0, sizeof(WindowInputStatus));
	}

};

typedef std::vector<InputEvent> WindowInputEventList;

struct WindowInputData
{
	WindowInputStatus mInputStatus;		/// status after all events in mInputEvents
	WindowInputEventList mInputEvents;	/// every events that occured ince last update

	void init()
	{
		mInputStatus.init();
		mInputEvents.clear();
		mInputEvents.reserve(16);
	}
};

