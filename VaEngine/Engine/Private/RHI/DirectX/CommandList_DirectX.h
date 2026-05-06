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
#pragma endregion Commands


private:
	ComPtr<ID3D12GraphicsCommandList> commandList;
};



