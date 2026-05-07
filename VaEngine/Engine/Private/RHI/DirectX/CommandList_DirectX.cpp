#include "CommandList_DirectX.h"
#include "CommandAlloc_DirectX.h"
#include "RenderDevice_DirectX.h"
#include "RHI/IRHIResource.h"

#include "SwapChain_DirectX.h"

#include <vector>

D3D12_RESOURCE_STATES GetResourceState(EResourceState state)
{
	switch (state)
	{
	case EResourceState::Present:
	case EResourceState::Common:					return D3D12_RESOURCE_STATE_COMMON;
	case EResourceState::VertexBuffer:				return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	case EResourceState::IndexBuffer:				return D3D12_RESOURCE_STATE_INDEX_BUFFER;
	case EResourceState::RenderTarget:				return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case EResourceState::UnorderedAccess:			return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	case EResourceState::DepthWrite:				return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	case EResourceState::DepthRead:					return D3D12_RESOURCE_STATE_DEPTH_READ;
	case EResourceState::NonPixelShaderResource:	return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	case EResourceState::PixelShaderResource:		return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	case EResourceState::IndirectArgument:			return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	case EResourceState::CopyDest:					return D3D12_RESOURCE_STATE_COPY_DEST;
	case EResourceState::CopySource:				return D3D12_RESOURCE_STATE_COPY_SOURCE;
	case EResourceState::ResolveDest:				return D3D12_RESOURCE_STATE_RESOLVE_DEST;
	case EResourceState::ResolveSource:				return D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
	case EResourceState::RaytracingAcceleration:	return D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

	default:										return D3D12_RESOURCE_STATE_COMMON;
	}

}

void CommandList_DirectX::Register(IRenderDevice* device, const CommandListDesc& desc)
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
				throw std::runtime_error("Invalid command list type specified in CommandListDesc");
			}
		}

		// Command list 생성용 임시 Allocator 생성, 함수 스택 종료 시 ComPtr에 의해 자동으로 해제됩니다.
		ComPtr<ID3D12CommandAllocator> tempAllocator;
		if (FAILED(dxDevice->GetDevice()->CreateCommandAllocator(type, IID_PPV_ARGS(&tempAllocator))))
		{
			throw std::runtime_error("Failed to create command allocator for Command List");
		}
		if (FAILED(dxDevice->GetDevice()->CreateCommandList(0, type, tempAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)))
		)
		{
			throw std::runtime_error("Failed to create Command List");
		}

		// Command list는 생성 직후에 open 상태이므로, 명령 기록 전에 반드시 닫아야 합니다.
		commandList->Close();
		return;
	}
	throw std::runtime_error("Invalid device type for CommandList_DirectX");
}

void CommandList_DirectX::Begin(ICommandAlloc* inAllocator)
{
	ID3D12CommandAllocator* allocatorHandle = static_cast<CommandAlloc_DirectX*>(inAllocator)->GetHandle();
	if (FAILED(commandList->Reset(allocatorHandle, nullptr)))
	{
		throw std::runtime_error("Failed to Begin command list.");
	}
}

void CommandList_DirectX::Close()
{
	if (FAILED(commandList->Close()))
	{
		throw std::runtime_error("Failed to close command list.");
	}
}

void CommandList_DirectX::SetResourceBarrier(uint32_t numBarriers, const ResourceBarrier* inBarriers)
{
	std::vector<D3D12_RESOURCE_BARRIER> dxBarriers(numBarriers);

	for (uint32_t i = 0; i < numBarriers; ++i)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		barrier.Transition.pResource = static_cast<ID3D12Resource*>(inBarriers[i].resource->GetNativeResource());
		barrier.Transition.StateBefore = GetResourceState(inBarriers[i].beforeState);
		barrier.Transition.StateAfter = GetResourceState(inBarriers[i].afterState);
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		dxBarriers[i] = barrier;
	}
	commandList->ResourceBarrier((UINT)dxBarriers.size(), dxBarriers.data());
}

void CommandList_DirectX::ClearRenderTargetView(IResourceView* inRTVHandle, const float inColor[4])
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = static_cast<ResourceView_DirectX*>(inRTVHandle)->GetRTVHandle();

	commandList->ClearRenderTargetView(rtvHandle, inColor, 0, nullptr);
}

void CommandList_DirectX::SetRenderTarget(IResourceView* rtv)
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = static_cast<ResourceView_DirectX*>(rtv)->GetRTVHandle();
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
}

void CommandList_DirectX::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
	D3D12_VIEWPORT viewport = { x, y, width, height, minDepth, maxDepth };
	commandList->RSSetViewports(1, &viewport);
}

void CommandList_DirectX::SetScissorRect(int32_t left, int32_t top, int32_t right, int32_t bottom)
{
	D3D12_RECT rect = { left, top, right, bottom };
	commandList->RSSetScissorRects(1, &rect);
}

void CommandList_DirectX::SetVertexBuffer(IResourceBuffer* vb, uint32_t stride, uint32_t totalSize)
{
	D3D12_VERTEX_BUFFER_VIEW view = {};
	view.BufferLocation = static_cast<ID3D12Resource*>(vb->GetResource()->GetNativeResource())->GetGPUVirtualAddress();
	view.StrideInBytes  = stride;
	view.SizeInBytes    = totalSize;
	commandList->IASetVertexBuffers(0, 1, &view);
}

void CommandList_DirectX::SetIndexBuffer(IResourceBuffer* ib, EIndexFormat format, uint32_t totalSize)
{
	D3D12_INDEX_BUFFER_VIEW view = {};
	view.BufferLocation = static_cast<ID3D12Resource*>(ib->GetResource()->GetNativeResource())->GetGPUVirtualAddress();
	view.Format         = (format == EIndexFormat::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	view.SizeInBytes    = totalSize;
	commandList->IASetIndexBuffer(&view);
}

void CommandList_DirectX::DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex)
{
	commandList->DrawIndexedInstanced(indexCount, 1, startIndex, baseVertex, 0);
}

