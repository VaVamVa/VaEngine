#pragma once

#include "RHI/ISwapChain.h"
#include "Common_DirectX.h"

#include "RHI/BaseRHIResource.h"
#include "ResourceView_DirectX.h"

class SwapChain_DirectX : public ISwapChain
{
public:
	void Register(class IRenderDevice* inDevice, const struct SwapChainDesc& desc) override;
	void Present(bool bVsync) override;
	void Resize(uint32_t width, uint32_t height) override;

	BaseRHIResource* GetCurrentBackBuffer() const override;
	IResourceView* GetCurrentBackBufferView() const override;

protected:
	uint32_t GetCurrentBackBufferIndex() const override;

private:
	void CreateRTV(class RenderDevice_DirectX* device);

private:
	ComPtr<IDXGISwapChain3> swapChain;
	ComPtr<ID3D12DescriptorHeap> rtvHeap;  // RTV들이 담길 Heap

	static constexpr uint32_t MAX_BUFFER_COUNT = 3;  // 최대 3중 버퍼링까지 고려

	// SwapChain_DirectX가 BackBufferResource를 합성(composition)하는 구조라, SetTrackedState()(protected)를
	// SwapChain_DirectX::CreateRTV()에서 직접 호출할 수 없다. BackBufferResource 자신의 public 메서드로
	// 감싸 스왑체인 버퍼를 얻어온 직후 1회만 호출한다.
	struct BackBufferResource : BaseRHIResource
	{
		ComPtr<ID3D12Resource> resource;
		void* GetNativeResource() const override { return resource.Get(); }
		void MarkCreated() { SetTrackedState(EResourceState::Common); }
	};

	BackBufferResource backBuffers[MAX_BUFFER_COUNT];
	std::unique_ptr<IResourceView> backBufferViews[MAX_BUFFER_COUNT];

	uint32_t bufferCount = 0;
	uint32_t rtvDescriptorSize = 0; // 힙 내부 간격
};