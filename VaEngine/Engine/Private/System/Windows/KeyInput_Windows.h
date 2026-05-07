#pragma once

#include "System/IKeyInput.h"

class KeyInput_Windows : public IKeyInput
{
public:
	KeyInput_Windows();

	void Update() override;

	bool IsDown (EKeyCode key) const override;
	bool IsUp   (EKeyCode key) const override;
	bool IsPress(EKeyCode key) const override;

private:
	enum EKeyStatus : uint8_t { None = 0, Down, Up, Press };

	static constexpr int KEY_COUNT = static_cast<int>(EKeyCode::Max);

	uint8_t prevState[KEY_COUNT];
	uint8_t currState[KEY_COUNT];
	uint8_t keyMap   [KEY_COUNT];
};
