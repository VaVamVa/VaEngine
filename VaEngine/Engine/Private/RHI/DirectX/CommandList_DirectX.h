#pragma once

#include "RHI/ICommandList.h"
#include "Common_DirectX.h"

class IResourceView;

D3D12_RESOURCE_STATES GetResourceState(EResourceState state);

class CommandList_DirectX : public ICommandList
{
public:
	void Register(class IRenderDevice* device, const struct CommandListDesc& desc) override;
	void Begin(class ICommandAlloc* inAllocator) override;
	void Close() override;

	ID3D12GraphicsCommandList* GetHandle() { return commandList.Get(); }

#pragma region Commands
	void SetResourceBarrier(uint32_t numBarriers, const struct ResourceBarrier* inBarriers) override;
	void ClearRenderTargetView(IResourceView* inViewHandle, const float inColor[4]) override;
	void SetRenderTarget(IResourceView* rtv) override;
	void SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) override;
	void SetScissorRect(int32_t left, int32_t top, int32_t right, int32_t bottom) override;
	void SetVertexBuffer(IBuffer* vb, uint32_t stride, uint32_t totalSize) override;
	void SetVertexBufferAt(IBuffer* vb, uint32_t slot, uint32_t stride, uint32_t totalSize, uint32_t byteOffset = 0) override;
	void SetIndexBuffer(IBuffer* ib, EIndexFormat format, uint32_t totalSize) override;
	void SetConstantBuffer(IBuffer* cb, uint32_t slot) override;
	void SetGraphicsSRV(IResourceView* view, uint32_t slot) override;
	void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, int32_t baseVertex = 0) override;
	void DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex = 0, int32_t baseVertex = 0, uint32_t startInstance = 0) override;
	void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount) override;
	void SetPrimitiveTopology(EPrimitiveTopology topology) override;
	void BeginRenderPass(const RenderPassDesc& desc) override;
	void EndRenderPass() override;
	void CopyBuffer(IBuffer* dst, IBuffer* src, uint64_t bytes) override;

	void SetComputeConstantBuffer(IBuffer* cb, uint32_t slot) override;
	void SetComputeSRV(IResourceView* view, uint32_t slot) override;
	void SetComputeUAV(IResourceView* view, uint32_t slot) override;
	void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
	void UAVBarrier(IRHIResource* resource) override;
#pragma endregion Commands


private:
	ComPtr<ID3D12GraphicsCommandList> commandList;
};



