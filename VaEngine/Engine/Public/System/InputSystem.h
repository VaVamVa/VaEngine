#pragma once

#include "System/InputContext.h"

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

class IKeyInput;
class IPointerInput;

/*
IKeyInput*, IPointerInput*의 수명은 호출자 책임 (소유하지 않음).
플랫폼 분기 없는 단일 구현.
*/
class InputSystem
{
public:
	InputSystem(IKeyInput* keys, IPointerInput* pointer);

	void Update();

	void PushContext  (const InputContext& ctx);
	void RemoveContext(const char* name);

	// Update() 이후 유효
	bool  IsTriggered(const char* action) const;  // Down or Press
	float GetAxis    (const char* action) const;  // 키: scale / 포인터: raw delta * scale

private:
	IKeyInput*     keys;
	IPointerInput* pointer;

	std::vector<InputContext>                                                                    contexts;
	std::unordered_map<std::string, float, TransparentStringHash, std::equal_to<>> actionValues;
};
