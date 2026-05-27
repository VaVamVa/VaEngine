#include "CommandList_DirectX.h"
#include "CommandAlloc_DirectX.h"
#include "RenderDevice_DirectX.h"
#include "RHI/IRHIResource.h"

#include "SwapChain_DirectX.h"

#include <vector>

D3D12_RESOURCE_STATES GetResourceState(EResourceState state)
{
	if (state == EResourceState::Common || state == EResourceState::Present)
		return D3D12_RESOURCE_STATE_COMMON;

	const uint32_t s = static_cast<uint32_t>(state);
	D3D12_RESOURCE_STATES result = D3D12_RESOURCE_STATE_COMMON;

	if (s & uint32_t(EResourceState::VertexBuffer))            result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	if (s & uint32_t(EResourceState::IndexBuffer))             result |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
	if (s & uint32_t(EResourceState::RenderTarget))            result |= D3D12_RESOURCE_STATE_RENDER_TARGET;
	if (s & uint32_t(EResourceState::UnorderedAccess))         result |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	if (s & uint32_t(EResourceState::DepthWrite))              result |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
	if (s & uint32_t(EResourceState::DepthRead))               result |= D3D12_RESOURCE_STATE_DEPTH_READ;
	if (s & uint32_t(EResourceState::NonPixelShaderResource))  result |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	if (s & uint32_t(EResourceState::PixelShaderResource))     result |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	if (s & uint32_t(EResourceState::IndirectArgument))        result |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	if (s & uint32_t(EResourceState::CopyDest))                result |= D3D12_RESOURCE_STATE_COPY_DEST;
	if (s & uint32_t(EResourceState::CopySource))              result |= D3D12_RESOURCE_STATE_COPY_SOURCE;
	if (s & uint32_t(EResourceState::ResolveDest))             result |= D3D12_RESOURCE_STATE_RESOLVE_DEST;
	if (s & uint32_t(EResourceState::ResolveSource))           result |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
	if (s & uint32_t(EResourceState::RaytracingAcceleration))  result |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

	return result;
}

CommandList_DirectX::CommandList_DirectX(ID3D12GraphicsCommandList* handle) noexcept
{
    handle->AddRef();
    commandList.Attach(handle);
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

void CommandList_DirectX::SetVertexBuffer(IBuffer* vb, uint32_t stride, uint32_t totalSize)
{
	D3D12_VERTEX_BUFFER_VIEW view = {};
	view.BufferLocation = static_cast<ID3D12Resource*>(vb->GetNativeResource())->GetGPUVirtualAddress();
	view.StrideInBytes  = stride;
	view.SizeInBytes    = totalSize;
	commandList->IASetVertexBuffers(0, 1, &view);
}

void CommandList_DirectX::SetVertexBufferAt(IBuffer* vb, uint32_t slot, uint32_t stride, uint32_t totalSize, uint32_t byteOffset)
{
	D3D12_VERTEX_BUFFER_VIEW view = {};
	view.BufferLocation = static_cast<ID3D12Resource*>(vb->GetNativeResource())->GetGPUVirtualAddress() + byteOffset;
	view.StrideInBytes  = stride;
	view.SizeInBytes    = totalSize;
	commandList->IASetVertexBuffers(slot, 1, &view);
}

void CommandList_DirectX::SetIndexBuffer(IBuffer* ib, EIndexFormat format, uint32_t totalSize)
{
	D3D12_INDEX_BUFFER_VIEW view = {};
	view.BufferLocation = static_cast<ID3D12Resource*>(ib->GetNativeResource())->GetGPUVirtualAddress();
	view.Format         = (format == EIndexFormat::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	view.SizeInBytes    = totalSize;
	commandList->IASetIndexBuffer(&view);
}

void CommandList_DirectX::SetConstantBuffer(IBuffer* cb, uint32_t slot)
{
	D3D12_GPU_VIRTUAL_ADDRESS address =
		static_cast<ID3D12Resource*>(cb->GetNativeResource())->GetGPUVirtualAddress();
	commandList->SetGraphicsRootConstantBufferView(slot, address);
}

void CommandList_DirectX::SetGraphicsSRV(IResourceView* view, uint32_t slot)
{
	auto* dxView = static_cast<ResourceView_DirectX*>(view);
	auto* res    = static_cast<ID3D12Resource*>(dxView->GetResource()->GetNativeResource());
	commandList->SetGraphicsRootShaderResourceView(slot, res->GetGPUVirtualAddress());
}

void CommandList_DirectX::DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex)
{
	commandList->DrawIndexedInstanced(indexCount, 1, startIndex, baseVertex, 0);
}

void CommandList_DirectX::DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex, int32_t baseVertex, uint32_t startInstance)
{
	commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void CommandList_DirectX::DrawInstanced(uint32_t vertexCount, uint32_t instanceCount)
{
	commandList->DrawInstanced(vertexCount, instanceCount, 0, 0);
}

void CommandList_DirectX::SetPrimitiveTopology(EPrimitiveTopology topology)
{
	D3D12_PRIMITIVE_TOPOLOGY dxTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	switch (topology)
	{
		case EPrimitiveTopology::TriangleList:  dxTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;  break;
		case EPrimitiveTopology::TriangleStrip: dxTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
		case EPrimitiveTopology::LineList:      dxTopology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;      break;
		case EPrimitiveTopology::PointList:     dxTopology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;     break;
	}
	commandList->IASetPrimitiveTopology(dxTopology);
}

void CommandList_DirectX::BeginRenderPass(const RenderPassDesc& desc)
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[8] = {};
	for (uint32_t i = 0; i < desc.renderTargetCount; ++i)
		rtvHandles[i] = static_cast<ResourceView_DirectX*>(desc.renderTargets[i].view)->GetRTVHandle();

	const bool hasDepth = (desc.depthStencil.view != nullptr);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
	if (hasDepth)
		dsvHandle = static_cast<ResourceView_DirectX*>(desc.depthStencil.view)->GetRTVHandle();

	commandList->OMSetRenderTargets(
		desc.renderTargetCount, rtvHandles, FALSE,
		hasDepth ? &dsvHandle : nullptr
	);

	for (uint32_t i = 0; i < desc.renderTargetCount; ++i)
	{
		if (desc.renderTargets[i].loadAction == ELoadAction::Clear)
			commandList->ClearRenderTargetView(rtvHandles[i], desc.renderTargets[i].clearColor, 0, nullptr);
	}

	if (hasDepth && desc.depthStencil.loadAction == ELoadAction::Clear)
	{
		commandList->ClearDepthStencilView(
			dsvHandle,
			D3D12_CLEAR_FLAG_DEPTH,
			desc.depthStencil.clearColor[0],  // clearDepth (1.0f)
			0,                                 // clearStencil
			0, nullptr
		);
	}
}

void CommandList_DirectX::EndRenderPass()
{
}

void CommandList_DirectX::CopyBuffer(IBuffer* dst, IBuffer* src, uint64_t bytes)
{
	auto* dstRes = static_cast<ID3D12Resource*>(dst->GetNativeResource());
	auto* srcRes = static_cast<ID3D12Resource*>(src->GetNativeResource());
	commandList->CopyBufferRegion(dstRes, 0, srcRes, 0, bytes);
}

void CommandList_DirectX::CopyBufferToTexture(
	IRHIResource* dstTexture, uint32_t dstSubresource,
	IRHIResource* srcBuffer,  uint64_t srcOffset,
	uint32_t width, uint32_t height, uint32_t rowPitch)
{
	auto* dxDst = static_cast<ID3D12Resource*>(dstTexture->GetNativeResource());
	auto* dxSrc = static_cast<ID3D12Resource*>(srcBuffer->GetNativeResource());

	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource        = dxDst;
	dst.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = dstSubresource;

	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource                          = dxSrc;
	src.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.PlacedFootprint.Offset             = srcOffset;
	src.PlacedFootprint.Footprint.Format   = dxDst->GetDesc().Format;
	src.PlacedFootprint.Footprint.Width    = width;
	src.PlacedFootprint.Footprint.Height   = height;
	src.PlacedFootprint.Footprint.Depth    = 1;
	src.PlacedFootprint.Footprint.RowPitch = rowPitch;

	commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
}

void CommandList_DirectX::SetComputeConstantBuffer(IBuffer* cb, uint32_t slot)
{
	D3D12_GPU_VIRTUAL_ADDRESS address =
		static_cast<ID3D12Resource*>(cb->GetNativeResource())->GetGPUVirtualAddress();
	commandList->SetComputeRootConstantBufferView(slot, address);
}

void CommandList_DirectX::SetComputeSRV(IResourceView* view, uint32_t slot)
{
	auto* dxView = static_cast<ResourceView_DirectX*>(view);
	auto* res    = static_cast<ID3D12Resource*>(dxView->GetResource()->GetNativeResource());
	// Buffer SRV는 root SRV로 바인딩 (raw/structured 모두 GPU virtual address 사용)
	commandList->SetComputeRootShaderResourceView(slot, res->GetGPUVirtualAddress());
}

void CommandList_DirectX::SetComputeUAV(IResourceView* view, uint32_t slot)
{
	auto* dxView = static_cast<ResourceView_DirectX*>(view);
	auto* res    = static_cast<ID3D12Resource*>(dxView->GetResource()->GetNativeResource());
	// Buffer UAV는 root UAV로 바인딩
	commandList->SetComputeRootUnorderedAccessView(slot, res->GetGPUVirtualAddress());
}

void CommandList_DirectX::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

void CommandList_DirectX::UAVBarrier(IRHIResource* resource)
{
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type          = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.Flags         = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.UAV.pResource = static_cast<ID3D12Resource*>(resource->GetNativeResource());
	commandList->ResourceBarrier(1, &barrier);
}

