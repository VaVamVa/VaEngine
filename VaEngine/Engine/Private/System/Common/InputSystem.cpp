#include "System/InputSystem.h"
#include "System/IKeyInput.h"
#include "System/IPointerInput.h"

#include <algorithm>
#include <cmath>

InputSystem::InputSystem(IKeyInput* keys, IPointerInput* pointer)
	: keys(keys), pointer(pointer) {}

void InputSystem::Update()
{
	actionValues.clear();

	const PointerState pState = pointer->GetState();

	for (const auto& ctx : contexts)
	{
		for (const auto& [action, bindings] : ctx.GetBindings())
		{
			float& value = actionValues[action];

			for (const auto& kb : bindings.keyBindings)
			{
				if (keys->IsDown(kb.key) || keys->IsPress(kb.key))
				{
					value += kb.scale;
					if (kb.axisCallback) kb.axisCallback(kb.scale);
				}
			}

			for (const auto& bb : bindings.boolBindings)
			{
				bool fire = false;
				switch (bb.trigger)
				{
				case EInputTrigger::Down:  fire = keys->IsDown (bb.key); break;
				case EInputTrigger::Press: fire = keys->IsPress(bb.key); break;
				case EInputTrigger::Up:    fire = keys->IsUp   (bb.key); break;
				}
				if (fire && bb.callback) bb.callback();
			}

			for (const auto& pb : bindings.pointerButtonBindings)
			{
				bool fire = false;
				switch (pb.trigger)
				{
				case EInputTrigger::Down:  fire = pointer->IsDown (pb.button); break;
				case EInputTrigger::Press: fire = pointer->IsPress(pb.button); break;
				case EInputTrigger::Up:    fire = pointer->IsUp   (pb.button); break;
				}
				if (fire && pb.callback) pb.callback();
			}

			for (const auto& pb : bindings.pointerBindings)
			{
				float raw = 0.0f;
				switch (pb.source)
				{
				case EPointerSource::DeltaX:      raw = pState.deltaX;      break;
				case EPointerSource::DeltaY:      raw = pState.deltaY;      break;
				case EPointerSource::ScrollDelta: raw = pState.scrollDelta; break;
				}
				const float result = raw * pb.scale;
				value += result;
				if (pb.axisCallback && result != 0.0f) pb.axisCallback(result);
			}
		}
	}
}

void InputSystem::PushContext(const InputContext& ctx)
{
	contexts.push_back(ctx);
}

void InputSystem::RemoveContext(const char* name)
{
	contexts.erase(
		std::remove_if(contexts.begin(), contexts.end(),
			[name](const InputContext& c) {
				return std::string(c.GetName()) == name;
			}
		),
		contexts.end());
}

bool InputSystem::IsTriggered(const char* action) const
{
	auto it = actionValues.find(action);
	return it != actionValues.end() && std::abs(it->second) > 0.0f;
}

float InputSystem::GetAxis(const char* action) const
{
	auto it = actionValues.find(action);
	return it != actionValues.end() ? it->second : 0.0f;
}
