#pragma once

#include "RHI/ISwapChain.h"
#include "Common_DirectX.h"

#include "RHI/IRHIResource.h"
#include "ResourceView_DirectX.h"

class SwapChain_DirectX : public ISwapChain
{
public:
	void Register(class IRenderDevice* inDevice, const struct SwapChainDesc& desc) override;
	void Present(bool bVsync) override;
	void Resize(uint32_t width, uint32_t height) override;

	IRHIResource* GetCurrentBackBuffer() const override;
	IResourceView* GetCurrentBackBufferView() const override;

protected:
	uint32_t GetCurrentBackBufferIndex() const override;

private:
	void CreateRTV(class RenderDevice_DirectX* device);

private:
	ComPtr<IDXGISwapChain3> swapChain;
	ComPtr<ID3D12DescriptorHeap> rtvHeap;  // RTV들이 담길 Heap

	static constexpr uint32_t MAX_BUFFER_COUNT = 3;  // 최대 3중 버퍼링까지 고려
	TRHIResource<ID3D12Resource> backBuffers[MAX_BUFFER_COUNT];
	std::unique_ptr<IResourceView> backBufferViews[MAX_BUFFER_COUNT];

	uint32_t bufferCount = 0;
	uint32_t rtvDescriptorSize = 0; // 힙 내부 간격
};