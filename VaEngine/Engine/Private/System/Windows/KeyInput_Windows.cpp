#include "KeyInput_Windows.h"

#include <cstring>
#include <windows.h>

KeyInput_Windows::KeyInput_Windows()
{
	memset(prevState, 0, sizeof(prevState));
	memset(currState, 0, sizeof(currState));
	memset(keyMap,    0, sizeof(keyMap));
}

void KeyInput_Windows::Update()
{
	memcpy(prevState, currState, KEY_COUNT);

	uint8_t vkState[256];
	GetKeyboardState(vkState);

	for (int i = 0; i < 256; ++i)
		currState[i] = (vkState[i] & 0x80) ? 1u : 0u;

	// 게임패드 범위(256–511)는 이 플랫폼에서 미지원
	memset(currState + 256, 0, KEY_COUNT - 256);

	for (int i = 0; i < KEY_COUNT; ++i)
	{
		if      (!prevState[i] && currState[i])  keyMap[i] = Down;
		else if ( prevState[i] && !currState[i]) keyMap[i] = Up;
		else if ( prevState[i] && currState[i])  keyMap[i] = Press;
		else                                      keyMap[i] = None;
	}
}

bool KeyInput_Windows::IsDown (EKeyCode key) const { return keyMap[static_cast<uint32_t>(key)] == Down;  }
bool KeyInput_Windows::IsUp   (EKeyCode key) const { return keyMap[static_cast<uint32_t>(key)] == Up;    }
bool KeyInput_Windows::IsPress(EKeyCode key) const { return keyMap[static_cast<uint32_t>(key)] == Press; }

std::unique_ptr<IKeyInput> IKeyInput::Create()
{
	return std::make_unique<KeyInput_Windows>();
}
