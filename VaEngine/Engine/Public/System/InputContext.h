#pragma once

#include "System/IKeyInput.h"
#include "System/IPointerInput.h"

#include <vector>
#include <unordered_map>
#include <string>

struct KeyAxisBinding
{
	EKeyCode key;
	float    scale = 1.0f;
};

struct PointerAxisBinding
{
	EPointerSource source;
	float          scale = 1.0f;
};

struct ActionBindings
{
	std::vector<KeyAxisBinding>     keyBindings;
	std::vector<PointerAxisBinding> pointerBindings;
};

/*
InputContext: 물리 입력 → 논리 액션 매핑 정의 (vtable 없음)

사용 예:
    InputContext ctx("PlayerMovement");
    ctx.MapKey("MoveForward", EKeyCode::W,  +1.0f);
    ctx.MapKey("MoveForward", EKeyCode::S,  -1.0f);
    ctx.MapPointer("LookX",   EPointerSource::DeltaX, 0.1f);
*/
class InputContext
{
public:
	explicit InputContext(const char* name) : contextName(name) {}

	// 키 → 1D 축 (scale: +1 정방향, -1 역방향)
	void MapKey    (const char* action, EKeyCode key, float scale = 1.0f);
	// 키 → bool 액션 (누르는 동안 1.0 반환)
	void MapKeyBool(const char* action, EKeyCode key);
	// 포인터 delta/scroll → 1D 축
	void MapPointer(const char* action, EPointerSource src, float scale = 1.0f);

	const char* GetName() const { return contextName; }
	const std::unordered_map<std::string, ActionBindings>& GetBindings() const { return bindings; }

private:
	const char* contextName;
	std::unordered_map<std::string, ActionBindings> bindings;
};

inline void InputContext::MapKey(const char* action, EKeyCode key, float scale)
{
	bindings[action].keyBindings.push_back({ key, scale });
}

inline void InputContext::MapKeyBool(const char* action, EKeyCode key)
{
	bindings[action].keyBindings.push_back({ key, 1.0f });
}

inline void InputContext::MapPointer(const char* action, EPointerSource src, float scale)
{
	bindings[action].pointerBindings.push_back({ src, scale });
}
