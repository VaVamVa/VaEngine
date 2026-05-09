#include "System/InputContext.h"

void InputContext::MapKey(const char* action, EKeyCode key, float scale)
{
	bindings[action].keyBindings.push_back({ key, scale });
}

void InputContext::MapKey(const char* action, EKeyCode key, AxisInputCallback callback, float scale)
{
	bindings[action].keyBindings.push_back({ key, scale, std::move(callback) });
}

void InputContext::MapKeyBool(const char* action, EKeyCode key, EInputTrigger trigger, ActionCallback callback)
{
	bindings[action].boolBindings.push_back({ key, trigger, std::move(callback) });
}

void InputContext::MapPointer(const char* action, EPointerSource src, float scale)
{
	bindings[action].pointerBindings.push_back({ src, scale });
}

void InputContext::MapPointer(const char* action, EPointerSource src, AxisInputCallback callback, float scale)
{
	bindings[action].pointerBindings.push_back({ src, scale, std::move(callback) });
}

void InputContext::MapPointerButton(const char* action, EPointerButton btn, EInputTrigger trigger, ActionCallback callback)
{
	bindings[action].pointerButtonBindings.push_back({ btn, trigger, std::move(callback) });
}
