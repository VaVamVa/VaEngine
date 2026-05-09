#pragma once

#include "System/IKeyInput.h"
#include "System/IPointerInput.h"

#include <functional>
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>

using ActionCallback    = std::function<void()>;
using AxisInputCallback = std::function<void(float)>;

struct TransparentStringHash
{
	using is_transparent = void;
	size_t operator()(std::string_view sv) const noexcept
	{
		return std::hash<std::string_view>{}(sv);
	}
};

struct KeyBoolBinding;
struct PointerButtonBinding;
struct KeyAxisBinding;
struct PointerAxisBinding;
struct ActionBindings;

/*
InputContext: 물리 입력 → 논리 액션 매핑 정의 (vtable 없음)

사용 예:
    InputContext ctx("Player");
    ctx.MapKey    ("MoveForward", EKeyCode::W, +1.0f);
    ctx.MapPointer("LookY",       EPointerSource::DeltaY, InvertY);
    ctx.MapKeyBool("Jump",        EKeyCode::Space, EInputTrigger::Down, OnJump);
    ctx.MapKeyBool("Crouch",      EKeyCode::LCtrl, EInputTrigger::Down, StartCrouch);
    ctx.MapKeyBool("Crouch",      EKeyCode::LCtrl, EInputTrigger::Up,   EndCrouch);
*/
class InputContext
{
public:
	explicit InputContext(const char* name) : contextName(name) {}

	// 키 → 1D 축 (scale: +1 정방향, -1 역방향)
	void MapKey    (const char* action, EKeyCode key, float scale = 1.0f);
	// 키 → 1D 축 + 콜백 (누르는 동안 매 프레임 호출, 인자: scale)
	void MapKey    (const char* action, EKeyCode key, AxisInputCallback callback, float scale = 1.0f);
	// 키 → 이벤트 콜백 (Down / Press / Up 중 지정한 트리거에만 호출)
	void MapKeyBool(const char* action, EKeyCode key, EInputTrigger trigger, ActionCallback callback);
	// 포인터 delta/scroll → 1D 축
	void MapPointer      (const char* action, EPointerSource src, float scale = 1.0f);
	// 포인터 → 1D 축 + 콜백 (입력 있는 프레임마다 호출, 인자: raw * scale)
	void MapPointer      (const char* action, EPointerSource src, AxisInputCallback callback, float scale = 1.0f);
	// 포인터 버튼 → 이벤트 콜백 (Down / Press / Up)
	void MapPointerButton(const char* action, EPointerButton btn, EInputTrigger trigger, ActionCallback callback);

	const char* GetName() const { return contextName; }
	const std::unordered_map<std::string, ActionBindings, TransparentStringHash, std::equal_to<>>& GetBindings() const { return bindings; }

private:
	const char* contextName;
	std::unordered_map<std::string, ActionBindings, TransparentStringHash, std::equal_to<>> bindings;
};


////////////////////////////////////////////////////////////////////////////////////////////

struct KeyBoolBinding
{
	EKeyCode       key;
	EInputTrigger  trigger;
	ActionCallback callback;
};

struct PointerButtonBinding
{
	EPointerButton button;
	EInputTrigger  trigger;
	ActionCallback callback;
};

struct KeyAxisBinding
{
	EKeyCode          key;
	float             scale = 1.0f;
	AxisInputCallback axisCallback = nullptr;
};

struct PointerAxisBinding
{
	EPointerSource    source;
	float             scale = 1.0f;
	AxisInputCallback axisCallback = nullptr;
};

struct ActionBindings
{
	std::vector<KeyAxisBinding>       keyBindings;
	std::vector<KeyBoolBinding>       boolBindings;
	std::vector<PointerAxisBinding>   pointerBindings;
	std::vector<PointerButtonBinding> pointerButtonBindings;
};