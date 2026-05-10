#include "RenderDevice_DirectX.h"

#include "CommandQueue_DirectX.h"
#include "SwapChain_DirectX.h"
#include "Fence_DirectX.h"
#include "CommandList_DirectX.h"
#include "CommandAlloc_DirectX.h"
#include "RHI/DirectX/Buffer/Buffer_DirectX.h"
#include "RHI/DirectX/Pipeline/BindingLayout_DirectX.h"
#include "RHI/DirectX/Pipeline/PipelineState_DirectX.h"
#include "RHI/DirectX/Texture/Texture_DirectX.h"
#include "RHI/DirectX/Texture/TextureFloat_DirectX.h"
#include "RHI/DirectX/Texture/Texture2DArray_DX.h"
#include "RHI/DirectX/Texture/TextureUAV_DirectX.h"
#include "RHI/DirectX/Shader/Shader_DirectX.h"
#include "RHI/DirectX/DepthBuffer_DirectX.h"
#include "RHI/DirectX/ResourceView_DirectX.h"
#include "RHI/IRHIResource.h"
#include "Utilities/DebuggingHelper.h"
#include <format>

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
	VA_LOG("RHI", "CreateSwapChain");
	std::unique_ptr<ISwapChain> swapChain = std::make_unique<SwapChain_DirectX>();
	swapChain->Register(this, desc);
	return swapChain;
}

std::unique_ptr<ICommandQueue> RenderDevice_DirectX::CreateCommandQueue(const CommandQueueDesc& desc)
{
	VA_LOG("RHI", std::format("CreateCommandQueue: Type={}", (uint32_t)desc.type));
	std::unique_ptr<ICommandQueue> queue = std::make_unique<CommandQueue_DirectX>();
	queue->Register(this, desc);
	return queue;
}

std::unique_ptr<IFence> RenderDevice_DirectX::CreateFence()
{
	VA_LOG("RHI", "CreateFence");
    std::unique_ptr<IFence> fence = std::make_unique<Fence_DirectX>();
	fence->Register(this);
	return fence;
}

std::unique_ptr<ICommandList> RenderDevice_DirectX::CreateCommandList(const CommandListDesc& desc)
{
	VA_LOG("RHI", std::format("CreateCommandList: Type={}", (uint32_t)desc.type));
	std::unique_ptr<ICommandList> commandList = std::make_unique<CommandList_DirectX>();
	commandList->Register(this, desc);
	return commandList;
}

std::unique_ptr<ICommandAlloc> RenderDevice_DirectX::CreateCommandAllocator(const CommandAllocDesc& desc)
{
	VA_LOG("RHI", std::format("CreateCommandAllocator: Type={}", (uint32_t)desc.type));
	std::unique_ptr<ICommandAlloc> commandAllocator = std::make_unique<CommandAlloc_DirectX>();
	commandAllocator->Register(this, desc);
    return commandAllocator;
}

std::unique_ptr<IBuffer> RenderDevice_DirectX::CreateBuffer(const BufferDesc& desc)
{
	VA_LOG("RHI", std::format("CreateBuffer: Size={}, Usage={}", desc.size, (uint32_t)desc.usage));
	auto buffer = std::make_unique<Buffer_DirectX>();
	buffer->Create(device.Get(), desc);
	return buffer;
}

std::unique_ptr<IBindingLayout> RenderDevice_DirectX::CreateBindingLayout(const BindingEntry* entries, uint32_t count, bool isCompute)
{
	VA_LOG("RHI", std::format("CreateBindingLayout: Count={}, Compute={}", count, isCompute));
	auto layout = std::make_unique<BindingLayout_DirectX>();
	layout->Create(device.Get(), entries, count, isCompute);
	return layout;
}

std::unique_ptr<IPipelineState> RenderDevice_DirectX::CreatePipelineState(const PipelineStateDesc& desc)
{
	VA_LOG("RHI", "CreatePipelineState");
	auto state = std::make_unique<PipelineState_DirectX>();
	state->Create(device.Get(), desc);
	return state;
}

std::unique_ptr<IPipelineState> RenderDevice_DirectX::CreateComputePipelineState(const ComputePipelineStateDesc& desc)
{
	VA_LOG("RHI", "CreateComputePipelineState");
	auto state = std::make_unique<PipelineState_DirectX>();
	state->Create(device.Get(), desc);
	return state;
}

std::unique_ptr<ITexture> RenderDevice_DirectX::CreateTexture()
{
	VA_LOG("RHI", "CreateTexture");
	return std::make_unique<Texture_DirectX>();
}

std::unique_ptr<ITexture> RenderDevice_DirectX::CreateTextureFloat()
{
	VA_LOG("RHI", "CreateTextureFloat");
	return std::make_unique<TextureFloat_DirectX>();
}

std::unique_ptr<ITexture2DArray> RenderDevice_DirectX::CreateTexture2DArray()
{
	VA_LOG("RHI", "CreateTexture2DArray");
	return std::make_unique<Texture2DArray_DX>();
}

std::unique_ptr<ITextureUAV> RenderDevice_DirectX::CreateTextureUAV()
{
	VA_LOG("RHI", "CreateTextureUAV");
	return std::make_unique<TextureUAV_DirectX>();
}

std::unique_ptr<IResourceView> RenderDevice_DirectX::CreateBufferSRV(IBuffer* buffer, uint32_t numElements, uint32_t strideBytes)
{
	if (buffer == nullptr)
		throw std::runtime_error("CreateBufferSRV: buffer is null");

	auto* d3dResource = static_cast<ID3D12Resource*>(buffer->GetNativeResource());

	auto slot = AllocateSRVDescriptor();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement     = 0;
	srvDesc.Buffer.NumElements      = numElements;

	if (strideBytes == 0)
	{
		// Raw byte buffer (ByteAddressBuffer): R32_TYPELESS + RAW flag, NumElements는 4-byte 단위 개수
		srvDesc.Format                = DXGI_FORMAT_R32_TYPELESS;
		srvDesc.Buffer.Flags          = D3D12_BUFFER_SRV_FLAG_RAW;
		srvDesc.Buffer.StructureByteStride = 0;
	}
	else
	{
		// Structured buffer
		srvDesc.Format                     = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;
		srvDesc.Buffer.StructureByteStride = strideBytes;
	}

	device->CreateShaderResourceView(d3dResource, &srvDesc, slot.cpu);

	ResourceViewDesc viewDesc{ EResourceViewType::ShaderResourceView, 1, 1 };
	auto* bufferAsResource = static_cast<IRHIResource*>(buffer);
	return std::make_unique<ResourceView_DirectX>(viewDesc, slot.cpu, slot.gpu, bufferAsResource);
}

std::unique_ptr<IResourceView> RenderDevice_DirectX::CreateBufferUAV(IBuffer* buffer, uint32_t numElements, uint32_t strideBytes)
{
	if (buffer == nullptr)
		throw std::runtime_error("CreateBufferUAV: buffer is null");

	auto* d3dResource = static_cast<ID3D12Resource*>(buffer->GetNativeResource());

	auto slot = AllocateSRVDescriptor();

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension      = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements  = numElements;
	uavDesc.Buffer.CounterOffsetInBytes = 0;

	if (strideBytes == 0)
	{
		// RWByteAddressBuffer
		uavDesc.Format                      = DXGI_FORMAT_R32_TYPELESS;
		uavDesc.Buffer.Flags                = D3D12_BUFFER_UAV_FLAG_RAW;
		uavDesc.Buffer.StructureByteStride  = 0;
	}
	else
	{
		// RWStructuredBuffer
		uavDesc.Format                      = DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.Flags                = D3D12_BUFFER_UAV_FLAG_NONE;
		uavDesc.Buffer.StructureByteStride  = strideBytes;
	}

	device->CreateUnorderedAccessView(d3dResource, nullptr, &uavDesc, slot.cpu);

	ResourceViewDesc viewDesc{ EResourceViewType::UnorderedAccessView, 1, 1 };
	auto* bufferAsResource = static_cast<IRHIResource*>(buffer);
	return std::make_unique<ResourceView_DirectX>(viewDesc, slot.cpu, slot.gpu, bufferAsResource);
}

std::unique_ptr<IDepthBuffer> RenderDevice_DirectX::CreateDepthBuffer(uint32_t width, uint32_t height, EPixelFormat format)
{
	VA_LOG("RHI", std::format("CreateDepthBuffer: {}x{}", width, height));
	auto buffer = std::make_unique<DepthBuffer_DirectX>();
	buffer->Create(device.Get(), width, height, static_cast<DXGI_FORMAT>(format));
	return buffer;
}

std::unique_ptr<IShader> RenderDevice_DirectX::CreateShader(const ShaderDesc& desc)
{
	VA_LOG("RHI", "CreateShader");
	auto shader = std::make_unique<Shader_DirectX>();
	shader->Load(desc);
	return shader;
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

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = GLOBAL_SRV_HEAP_SIZE;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&globalSrvHeap))))
        throw std::runtime_error("Failed to create global SRV descriptor heap");

    srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

RenderDevice_DirectX::SRVDescriptor RenderDevice_DirectX::AllocateSRVDescriptor()
{
    if (srvAllocIndex >= GLOBAL_SRV_HEAP_SIZE)
        throw std::runtime_error("Global SRV heap exhausted");

    const uint32_t idx = srvAllocIndex++;
    SRVDescriptor desc;
    desc.cpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        globalSrvHeap->GetCPUDescriptorHandleForHeapStart(), idx, srvDescriptorSize);
    desc.gpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        globalSrvHeap->GetGPUDescriptorHandleForHeapStart(), idx, srvDescriptorSize);
    return desc;
}
