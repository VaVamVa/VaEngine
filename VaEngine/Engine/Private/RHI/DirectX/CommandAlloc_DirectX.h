#pragma once

#include "RHI/ICommandAlloc.h"
#include "Common_DirectX.h"

class CommandAlloc_DirectX : public ICommandAlloc
{
public:
	void Register(class IRenderDevice* device, const struct CommandAllocDesc& desc) override;

	void Reset() override;

	ID3D12CommandAllocator* GetHandle() { return commandAllocator.Get(); }

private:
	ComPtr<ID3D12CommandAllocator> commandAllocator;
};