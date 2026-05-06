#include "CommandAlloc_DirectX.h"

#include "RenderDevice_DirectX.h"

void CommandAlloc_DirectX::Register(IRenderDevice* device, const CommandAllocDesc& desc)
{
	if (RenderDevice_DirectX* dxDevice = static_cast<RenderDevice_DirectX*>(device))
	{
		D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		switch (desc.type)
		{
			case ECommandQueueType::Graphics:
			{
				type = D3D12_COMMAND_LIST_TYPE_DIRECT; 
				break;
			}
			case ECommandQueueType::Compute:	
			{
				type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
				break;
			}
			case ECommandQueueType::Copy:		
			{
				type = D3D12_COMMAND_LIST_TYPE_COPY;
				break;
			}
			default:
			{
				throw std::runtime_error("Invalid command allocator type specified in CommandAllocatorDesc");
			}
		}
		if (FAILED(dxDevice->GetDevice()->CreateCommandAllocator(
			type, IID_PPV_ARGS(&commandAllocator)))
		)
		{
			throw std::runtime_error("Failed to create command allocator.");
		}
		return;
	}

	throw std::runtime_error("Invalid device type for CommandAlloc_DirectX");
}

void CommandAlloc_DirectX::Reset()
{
	if (FAILED(commandAllocator->Reset()))
	{
		throw std::runtime_error("Failed to reset command allocator.");
	}
}
