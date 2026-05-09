#pragma once

#include "RHI/IFence.h"
#include "Common_DirectX.h"

class Fence_DirectX : public IFence
{
public:
	~Fence_DirectX() override;

	void		Register(class IRenderDevice* inDevice) override;
	void		Wait(uint64_t value) override;
	uint64_t	GetCompletedValue() const override;

	ID3D12Fence* GetHandle() const { return fence.Get(); }

private:
	ComPtr<ID3D12Fence> fence;
	HANDLE              fenceEvent = nullptr;
};