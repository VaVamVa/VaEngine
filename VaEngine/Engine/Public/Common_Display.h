#pragma once

#include <cstdint>

struct NativeDisplayInfo
{
	enum class EPlatform { None, Windows, Android, iOS, Max };

	EPlatform platform;
	union
	{
		// HWND(Windows) || ANativeWindow(Android)
		void*		Handle;
		uintptr_t	RawData;
	};

	// HINSTANCE(Windows) || nullptr(Android)
	void* Display;
};