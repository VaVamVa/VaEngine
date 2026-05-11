#pragma once

#include "System/IPointerInput.h"
#include <windows.h>

class MouseInput_Windows : public IPointerInput
{
public:
	explicit MouseInput_Windows(NativeDisplayInfo displayInfo);

	void Update() override;
	void ProcessNativeEvent(uintptr_t message, uintptr_t wParam, uintptr_t lParam) override;

	bool IsDown (EPointerButton btn) const override;
	bool IsUp   (EPointerButton btn) const override;
	bool IsPress(EPointerButton btn) const override;

	PointerState GetState() const override { return state; }

	void        SetCursorMode(ECursorMode mode) override;
	ECursorMode GetCursorMode()           const override { return cursorMode; }

private:
	enum EButtonStatus : uint8_t { None = 0, Down, Up, Press };

	static constexpr int BTN_COUNT = static_cast<int>(EPointerButton::Max);

	uint8_t prevBtn[BTN_COUNT] = {};
	uint8_t currBtn[BTN_COUNT] = {};
	uint8_t btnMap [BTN_COUNT] = {};

	PointerState state;
	bool         firstUpdate = true;

	HWND        hwnd       = nullptr;
	ECursorMode cursorMode = ECursorMode::Free;
};
