#include "CommandQueue_DirectX.h"

#include "RenderDevice_DirectX.h"
#include "Fence_DirectX.h"
#include "Common_RHI.h"

void CommandQueue_DirectX::Register(IRenderDevice* inDevice, const CommandQueueDesc& inDesc)
{
	if (RenderDevice_DirectX* device = static_cast<RenderDevice_DirectX*>(inDevice))
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
		if (FAILED(device->GetDevice()->CreateCommandQueue(
			&queueDesc, IID_PPV_ARGS(&queue)))
		)
		{
			throw std::runtime_error("Failed to create Command Queue");
		}

		return;
	}

	throw std::runtime_error("Invalid device type for CommandQueue_DirectX");
}

void CommandQueue_DirectX::ExecuteCommandLists(const std::vector<ICommandList*>& commandLists)
{
}

void CommandQueue_DirectX::Signal(IFence* fence, uint64_t value)
{
	ID3D12Fence* fenceHandle = static_cast<Fence_DirectX*>(fence)->GetHandle();
	queue->Signal(fenceHandle, value);
}

void CommandQueue_DirectX::Wait(IFence * fence, uint64_t value)
{
	ID3D12Fence* fenceHandle = static_cast<Fence_DirectX*>(fence)->GetHandle();
	queue->Wait(fenceHandle, value);
}
