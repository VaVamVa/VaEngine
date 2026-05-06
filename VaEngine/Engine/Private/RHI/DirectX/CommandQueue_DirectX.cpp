#include "CommandQueue_DirectX.h"

#include "RenderDevice_DirectX.h"
#include "Fence_DirectX.h"
#include "CommandList_DirectX.h"

void CommandQueue_DirectX::Register(IRenderDevice* inDevice, const CommandQueueDesc& inDesc)
{
	if (RenderDevice_DirectX* dxDevice = static_cast<RenderDevice_DirectX*>(inDevice))
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};

		switch (static_cast<ECommandQueueType>(inDesc.type))
		{
			case ECommandQueueType::Compute:
			{
				queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
				break;
			}
			case ECommandQueueType::Copy:
			{
				queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
				break;
			}
			case ECommandQueueType::Graphics:
			default:
			{
				queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
				break;
			}
		}

		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 0;

		if (FAILED(dxDevice->GetDevice()->CreateCommandQueue(
			&queueDesc, IID_PPV_ARGS(&queue)))
		)
		{
			throw std::runtime_error("Failed to create Command Queue");
		}

		return;
	}

	throw std::runtime_error("Invalid device type for CommandQueue_DirectX");
}

void CommandQueue_DirectX::ExecuteCommandLists(const uint32_t& inCount, const std::span<ICommandList*>& commandLists)
{
	std::vector<ID3D12CommandList*> rawLists(inCount);
	for (uint32_t i = 0; i < inCount; ++i)
	{
		rawLists[i] = static_cast<CommandList_DirectX*>(commandLists[i])->GetHandle();
	}
	queue->ExecuteCommandLists(inCount, rawLists.data());
}

void CommandQueue_DirectX::Signal(IFence* fence, const uint64_t& value)
{
	ID3D12Fence* fenceHandle = static_cast<Fence_DirectX*>(fence)->GetHandle();
	queue->Signal(fenceHandle, value);
}

void CommandQueue_DirectX::Wait(IFence * fence, const uint64_t& value)
{
	ID3D12Fence* fenceHandle = static_cast<Fence_DirectX*>(fence)->GetHandle();
	queue->Wait(fenceHandle, value);
}
