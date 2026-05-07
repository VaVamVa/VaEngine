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
	void SetVertexBuffer(IResourceBuffer* vb, uint32_t stride, uint32_t totalSize) override;
	void SetIndexBuffer(IResourceBuffer* ib, EIndexFormat format, uint32_t totalSize) override;
	void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, int32_t baseVertex = 0) override;
#pragma endregion Commands


private:
	ComPtr<ID3D12GraphicsCommandList> commandList;
};



