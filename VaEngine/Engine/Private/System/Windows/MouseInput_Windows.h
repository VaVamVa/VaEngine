#pragma once

#include "System/IPointerInput.h"

class MouseInput_Windows : public IPointerInput
{
public:
	MouseInput_Windows();

	void Update() override;
	void ProcessNativeEvent(uintptr_t message, uintptr_t wParam, uintptr_t lParam) override;

	bool IsDown (EPointerButton btn) const override;
	bool IsUp   (EPointerButton btn) const override;
	bool IsPress(EPointerButton btn) const override;

	PointerState GetState() const override { return state; }

private:
	enum EButtonStatus : uint8_t { None = 0, Down, Up, Press };

	static constexpr int BTN_COUNT = static_cast<int>(EPointerButton::Max);

	uint8_t prevBtn[BTN_COUNT];
	uint8_t currBtn[BTN_COUNT];
	uint8_t btnMap [BTN_COUNT];

	PointerState state;
	bool         firstUpdate = true;
};
