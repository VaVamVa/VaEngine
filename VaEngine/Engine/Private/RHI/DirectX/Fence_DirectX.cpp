#include "Fence_DirectX.h"

#include "RenderDevice_DirectX.h"

Fence_DirectX::~Fence_DirectX()
{
	if (fenceEvent)
		CloseHandle(fenceEvent);
}

void Fence_DirectX::Register(IRenderDevice* inDevice)
{
	if (RenderDevice_DirectX* device = static_cast<RenderDevice_DirectX*>(inDevice))
	{
		if (FAILED(device->GetDevice()->CreateFence(
			0
			, D3D12_FENCE_FLAG_NONE
			, IID_PPV_ARGS(&fence)
		)))
		{
			throw std::runtime_error("Failed to create Fence");
		}

		fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!fenceEvent)
			throw std::runtime_error("Failed to create fence event");

		return;
	}
	throw std::runtime_error("Invalid device type for Fence_DirectX");
}

void Fence_DirectX::Wait(uint64_t value)
{
	if (fence->GetCompletedValue() < value)
	{
		fence->SetEventOnCompletion(value, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}
}

uint64_t Fence_DirectX::GetCompletedValue() const
{
	return fence->GetCompletedValue();
}
