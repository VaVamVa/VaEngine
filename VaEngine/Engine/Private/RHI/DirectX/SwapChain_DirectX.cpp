#include "SwapChain_DirectX.h"

#include "RenderDevice_DirectX.h"
#include "CommandQueue_DirectX.h"

void SwapChain_DirectX::Register(IRenderDevice* inDevice, const SwapChainDesc& desc)
{
	RenderDevice_DirectX* device = static_cast<RenderDevice_DirectX*>(inDevice);
	CommandQueue_DirectX * queue = static_cast<CommandQueue_DirectX*>(desc.RegisteredQueue);
	HWND hWnd = static_cast<HWND>(desc.displayInfo.Handle);

	if (device && queue && hWnd)
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = desc.width;
		swapChainDesc.Height = desc.height;
		swapChainDesc.Format = static_cast<DXGI_FORMAT>(desc.pixelFormat);
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = desc.bufferCount;
		this->bufferCount = desc.bufferCount;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> tempSwapChain;
		if (FAILED(device->GetFactory()->CreateSwapChainForHwnd(
				queue->GetHandle()
				, hWnd
				, &swapChainDesc
				, nullptr
				, nullptr
				, &tempSwapChain
			)
		))
		{
			throw std::runtime_error("Failed to create Swap Chain");
		}
		if (FAILED(tempSwapChain.As(&swapChain)))
		{
			throw std::runtime_error("Failed to up-cast Swap Chain to IDXGISwapChain3");
		}

		CreateRTV(device);
		return;
	}

	throw std::runtime_error("Invalid Resources for Swap Chain registration");
}

void SwapChain_DirectX::Present(bool bVsync)
{
	// DXGI_SWAP_EFFECT_FLIP_DISCARD를 사용할 때, Present의 첫 번째 인자는 0 또는 1이 될 수 있습니다.
	// 첫번째 인자가 1이면, DXGI는 화면 재생 빈도에 맞춰 프레임을 표시(VSync)하도록 시도합니다.
	HRESULT hR = swapChain->Present(bVsync ? 1 : 0, 0);
	if (hR == DXGI_ERROR_DEVICE_REMOVED || hR == DXGI_ERROR_DEVICE_RESET)
	{
		throw std::runtime_error("GPU device was removed or reset. Swap Chain needs to be recreated.");
	}
	else if (FAILED(hR))
	{
		throw std::runtime_error("Failed to present Swap Chain");
	}
}

void SwapChain_DirectX::Resize(uint32_t width, uint32_t height)
{
	// TODO:
	// 1. Flush GPU (모든 명령 완료 대기)
	// 2. Release BackBuffers (mBackBuffers[i].Reset())
	// 3. mSwapChain->ResizeBuffers(...)
	// 4. CreateRTV() 재호출

}

uint32_t SwapChain_DirectX::GetCurrentBackBufferIndex() const
{
	return swapChain->GetCurrentBackBufferIndex();
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain_DirectX::GetCurrentBackBufferRTV() const
{
	// rtvHeap의 시작점 기준 잡고, 
	// 현재 백 버퍼 인덱스와 백 버퍼의 간격을 곱하여 현재 백 버퍼에 해당하는 포인터(RTV 핸들)로 이동
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
		rtvHeap->GetCPUDescriptorHandleForHeapStart()
		, GetCurrentBackBufferIndex()
		, rtvDescriptorSize);

	return rtvHandle;
}

void SwapChain_DirectX::CreateRTV(RenderDevice_DirectX* device)
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = bufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (FAILED(device->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap))))
	{
		throw std::runtime_error("Failed to create RTV Descriptor Heap");
	}
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());

	rtvDescriptorSize = 
		device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	assert(bufferCount <= MAX_BUFFER_COUNT);
	for (uint32_t i = 0; i < bufferCount; ++i)
	{

		if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]))))
		{
			throw std::runtime_error("Failed to get back buffer from Swap Chain");
		}

		// RTV 생성 & 힙의 i번째 슬롯에 RTV를 할당
		device->GetDevice()->CreateRenderTargetView(backBuffers[i].Get(), nullptr, rtvHandle);

		// 다음 RTV 슬롯으로 포인터 이동
		rtvHandle.Offset(1, rtvDescriptorSize);
	}
	
}
