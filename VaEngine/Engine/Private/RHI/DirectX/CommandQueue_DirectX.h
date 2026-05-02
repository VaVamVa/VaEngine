#pragma once

#include "Interfaces/ICommandQueue.h"
#include "Common_DirectX.h"

class CommandQueue_DirectX : public ICommandQueue
{
public:
	void Register(class IRenderDevice* inDevice, const struct CommandQueueDesc& inDesc) override;
	void ExecuteCommandLists(const std::vector<ICommandList*>& commandLists) override;
	void Signal(IFence* fence, uint64_t value) override;
	void Wait(IFence* fence, uint64_t value) override;

	ID3D12CommandQueue* GetHandle() const { return queue.Get(); }

private:
	ComPtr<ID3D12CommandQueue> queue;
};