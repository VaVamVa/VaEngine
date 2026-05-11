#include "MouseInput_Windows.h"

#include <cstring>

MouseInput_Windows::MouseInput_Windows(NativeDisplayInfo displayInfo)
	: hwnd(static_cast<HWND>(displayInfo.Handle))
{
	memset(prevBtn, 0, sizeof(prevBtn));
	memset(currBtn, 0, sizeof(currBtn));
	memset(btnMap,  0, sizeof(btnMap));

	POINT pt;
	GetCursorPos(&pt);
	state.x = static_cast<float>(pt.x);
	state.y = static_cast<float>(pt.y);
}

void MouseInput_Windows::Update()
{
	// 버튼 상태
	memcpy(prevBtn, currBtn, BTN_COUNT);
	currBtn[0] = GetAsyncKeyState(VK_LBUTTON) & 0x8000 ? 1u : 0u;
	currBtn[1] = GetAsyncKeyState(VK_RBUTTON) & 0x8000 ? 1u : 0u;
	currBtn[2] = GetAsyncKeyState(VK_MBUTTON) & 0x8000 ? 1u : 0u;

	for (int i = 0; i < BTN_COUNT; ++i)
	{
		if      (!prevBtn[i] && currBtn[i])  btnMap[i] = Down;
		else if ( prevBtn[i] && !currBtn[i]) btnMap[i] = Up;
		else if ( prevBtn[i] && currBtn[i])  btnMap[i] = Press;
		else                                  btnMap[i] = None;
	}

	if (cursorMode == ECursorMode::Centered && hwnd)
	{
		RECT rect;
		GetClientRect(hwnd, &rect);
		POINT center = { (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2 };
		ClientToScreen(hwnd, &center);

		POINT pt;
		GetCursorPos(&pt);

		if (firstUpdate)
		{
			firstUpdate = false;
		}
		else
		{
			state.deltaX = static_cast<float>(pt.x - center.x);
			state.deltaY = static_cast<float>(pt.y - center.y);
		}

		SetCursorPos(center.x, center.y);
		state.x = static_cast<float>(center.x);
		state.y = static_cast<float>(center.y);
	}
	else
	{
		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(hwnd, &pt);
		float newX = static_cast<float>(pt.x);
		float newY = static_cast<float>(pt.y);

		if (firstUpdate)
		{
			firstUpdate = false;
		}
		else
		{
			state.deltaX = newX - state.x;
			state.deltaY = newY - state.y;
		}

		state.x = newX;
		state.y = newY;
	}

	state.scroll      += state.scrollDelta;
	state.scrollDelta  = 0.0f;
}

void MouseInput_Windows::ProcessNativeEvent(uintptr_t message, uintptr_t wParam, uintptr_t lParam)
{
	if (message == WM_MOUSEWHEEL)
	{
		short delta = static_cast<short>(HIWORD(static_cast<DWORD>(wParam)));
		state.scrollDelta += static_cast<float>(delta);
	}
}

bool MouseInput_Windows::IsDown (EPointerButton btn) const { return btnMap[static_cast<uint32_t>(btn)] == Down;  }
bool MouseInput_Windows::IsUp   (EPointerButton btn) const { return btnMap[static_cast<uint32_t>(btn)] == Up;    }
bool MouseInput_Windows::IsPress(EPointerButton btn) const { return btnMap[static_cast<uint32_t>(btn)] == Press; }

void MouseInput_Windows::SetCursorMode(ECursorMode mode)
{
	if (cursorMode == mode) return;
	cursorMode = mode;

	if (mode == ECursorMode::Centered)
	{
		ShowCursor(FALSE);
		firstUpdate = true;
	}
	else
	{
		ShowCursor(TRUE);
	}
}

std::unique_ptr<IPointerInput> IPointerInput::Create(NativeDisplayInfo displayInfo)
{
	return std::make_unique<MouseInput_Windows>(displayInfo);
}
