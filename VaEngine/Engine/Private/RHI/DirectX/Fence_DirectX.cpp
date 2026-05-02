#include "Fence_DirectX.h"

#include "RenderDevice_DirectX.h"

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
		return;
	}
	throw std::runtime_error("Invalid device type for Fence_DirectX");
	
}

void Fence_DirectX::SetEventOnComplete(uint64_t value, void* inEventHandle)
{
	fence->SetEventOnCompletion(value, static_cast<HANDLE>(inEventHandle));
}

uint64_t Fence_DirectX::GetCompletedValue() const
{
	return fence->GetCompletedValue();
}
