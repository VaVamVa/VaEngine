#include "RenderDevice_DirectX.h"

#include "CommandQueue_DirectX.h"
#include "SwapChain_DirectX.h"
#include "Fence_DirectX.h"
#include "Interfaces/ICommandList.h"

void RenderDevice_DirectX::Initialize()
{
    // 1. 디버그 레이어 활성화 (Debug 빌드 시)
    EnableDebugLayer();
    // 2. DXGI Factory 생성
    CreateFactory();
    // 3. 적절한 GPU 어댑터 선택
    PickAdapter();
    // 4. D3D12 Device 생성
    CreateDevice();

}

void RenderDevice_DirectX::Shutdown()
{
	device.Reset();
	adapter.Reset();
	factory.Reset();
}

std::unique_ptr<ISwapChain> RenderDevice_DirectX::CreateSwapChain(const SwapChainDesc& desc)
{
	if (desc.RegisteredQueue == nullptr || desc.displayInfo.Handle == nullptr)
    {
        throw std::runtime_error("Handles must be registered before creating Swap Chain");
    }
	std::unique_ptr<ISwapChain> swapChain = std::make_unique<SwapChain_DirectX>();
	swapChain->Register(this, desc);
	return swapChain;
}

std::unique_ptr<ICommandQueue> RenderDevice_DirectX::CreateCommandQueue(const CommandQueueDesc& desc)
{
	std::unique_ptr<ICommandQueue> queue = std::make_unique<CommandQueue_DirectX>();
	queue->Register(this, desc);
	return queue;
}

std::unique_ptr<IFence> RenderDevice_DirectX::CreateFence()
{
    std::unique_ptr<IFence> fence = std::make_unique<Fence_DirectX>();
	fence->Register(this);
	return fence;
}

std::unique_ptr<ICommandList> RenderDevice_DirectX::CreateCommandList(const CommandListDesc& desc)
{
	return std::unique_ptr<ICommandList>();
}

void RenderDevice_DirectX::EnableDebugLayer()
{
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        throw std::runtime_error("Failed to get Debug Controller");
	}
    debugController->EnableDebugLayer();
#endif
}

void RenderDevice_DirectX::CreateFactory()
{
    UINT flags = 0;
#if defined(_DEBUG)
    flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    if (FAILED(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory))))
    {
		throw std::runtime_error("Failed to create DXGI Factory");
    }
}

void RenderDevice_DirectX::PickAdapter()
{
    for (UINT i = 0;
        factory->EnumAdapterByGpuPreference(
            i
            , DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
            , IID_PPV_ARGS(&adapter)
        ) != DXGI_ERROR_NOT_FOUND;
        ++i
    ) 
    {
        DXGI_ADAPTER_DESC3 desc;
		adapter->GetDesc3(&desc);
        
        // 내장 GPU(소프트웨어 렌더러)는 건너뛰기
        if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) { continue; }

        // D3D12 지원 여부 확인
        if (SUCCEEDED(D3D12CreateDevice(
                adapter.Get()
                , D3D_FEATURE_LEVEL_12_1
                , _uuidof(ID3D12Device)
                , nullptr
                )
            )
        ) { break; }
    }

	if (adapter == nullptr)
    {
        throw std::runtime_error("Failed to find a suitable GPU adapter");
    }
}

void RenderDevice_DirectX::CreateDevice()
{
    if (FAILED(D3D12CreateDevice(
            adapter.Get()
            , D3D_FEATURE_LEVEL_12_1
            , IID_PPV_ARGS(&device)
            )
        )
    )
    {
        throw std::runtime_error("Failed to create D3D12 Device");
    }

#if defined(_DEBUG)
    {
        // ID3D12InfoQueue 인터페이스를 사용하여 메시지 큐에 콜백을 등록하면 디버그 레이어에서 GPU 관련 오류를 더 자세히 볼 수 있음
        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(device.As(&infoQueue)))
        {
            // 심각한 오류 발생 시 실행 중단 요청
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            
            // 필요에 따라 경고도 가능
            // infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
            
            /* 너무 빈번하거나 무시해도 되는 특정 경고 메시지는 필터링 가능
            D3D12_MESSAGE_ID hideMessages[] = { 
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
                , ... 
            };
            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hideMessages);
            filter.DenyList.pIDList = hideMessages;
            infoQueue->AddStorageFilterEntries(&filter);
             */

        }
	}
#endif
}
