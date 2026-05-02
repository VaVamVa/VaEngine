#pragma once

#include "Interfaces/IFence.h"
#include "Common_DirectX.h"

class Fence_DirectX : public IFence
{
public:
	void		Register(class IRenderDevice* inDevice) override;
	void		SetEventOnComplete(uint64_t value, void* inEventHandle) override;
	uint64_t	GetCompletedValue() const override;

	ID3D12Fence* GetHandle() const { return fence.Get(); }

private:
	ComPtr<ID3D12Fence> fence;
};