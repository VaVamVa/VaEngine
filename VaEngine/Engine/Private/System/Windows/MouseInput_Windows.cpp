#include "MouseInput_Windows.h"

#include <cstring>
#include <windows.h>

MouseInput_Windows::MouseInput_Windows()
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

	// 커서 위치 및 delta
	POINT pt;
	GetCursorPos(&pt);
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

	// scrollDelta는 ProcessNativeEvent에서 누적, 매 프레임 소비 후 초기화
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

std::unique_ptr<IPointerInput> IPointerInput::Create()
{
	return std::make_unique<MouseInput_Windows>();
}
