#include "DepthBuffer_DirectX.h"
#include "ResourceView_DirectX.h"
#include "RenderDevice_DirectX.h"
#include "CommandList_DirectX.h"
#include "RHI/Common_RHI.h"

#include <stdexcept>

namespace
{
    struct DepthFormatTriplet
    {
        DXGI_FORMAT resourceFormat;
        DXGI_FORMAT dsvFormat;
        DXGI_FORMAT srvFormat;
    };

    // 리소스는 typeless로 생성해야 DSV와 SRV를 동시에 생성할 수 있다.
    // D3D12는 D24_UNORM_S8_UINT / D32_FLOAT 포맷 리소스에 SRV 생성을 허용하지 않는다.
    DepthFormatTriplet GetDepthFormatTriplet(DXGI_FORMAT requested)
    {
        switch (requested)
        {
        case DXGI_FORMAT_D32_FLOAT:
            return { DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_FLOAT };
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        default:
            return { DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS };
        }
    }
}

void DepthBuffer_DirectX::Create(IRenderDevice* device, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t arraySize)
{
    auto* rdDevice  = static_cast<RenderDevice_DirectX*>(device);
    auto* d3dDevice = rdDevice->GetDevice();

    const DepthFormatTriplet fmt     = GetDepthFormatTriplet(format);
    const DXGI_FORMAT resourceFormat = fmt.resourceFormat;
    const DXGI_FORMAT dsvFormat      = fmt.dsvFormat;
    const DXGI_FORMAT srvFormat      = fmt.srvFormat;

    D3D12_CLEAR_VALUE clearVal            = {};
    clearVal.Format                       = dsvFormat;
    clearVal.DepthStencil.Depth           = 1.0f;
    clearVal.DepthStencil.Stencil         = 0;

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resDesc   = CD3DX12_RESOURCE_DESC::Tex2D(
        resourceFormat, width, height,
        static_cast<UINT16>(arraySize), 0, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );

    if (FAILED(d3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearVal,
        IID_PPV_ARGS(&depthResource))))
    {
        throw std::runtime_error("Failed to create depth buffer resource");
    }
    SetTrackedState(EResourceState::DepthWrite);  // CreateCommittedResource가 실제로 이 상태로 생성

    // 1. DSV — arraySize==1이면 기존 1-slot TEXTURE2D 경로 그대로, arraySize>1(CSM 등)이면
    //    슬라이스별 DSV(N-slot heap, TEXTURE2DARRAY + FirstArraySlice)를 생성한다.
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors             = arraySize;
    dsvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap))))
    {
        throw std::runtime_error("Failed to create DSV descriptor heap");
    }

    D3D12_DESCRIPTOR_HEAP_DESC readOnlyDsvHeapDesc = dsvHeapDesc;
    if (FAILED(d3dDevice->CreateDescriptorHeap(&readOnlyDsvHeapDesc, IID_PPV_ARGS(&readOnlyDsvHeap))))
    {
        throw std::runtime_error("Failed to create read-only DSV descriptor heap");
    }

    const UINT dsvIncrement = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    dsvViews.resize(arraySize);
    readOnlyDsvViews.resize(arraySize);

    for (uint32_t slice = 0; slice < arraySize; ++slice)
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = dsvFormat;
        if (arraySize > 1)
        {
            dsvDesc.ViewDimension                  = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice        = 0;
            dsvDesc.Texture2DArray.FirstArraySlice = slice;
            dsvDesc.Texture2DArray.ArraySize       = 1;
        }
        else
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        }
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        dsvHandle.ptr += static_cast<SIZE_T>(slice) * dsvIncrement;
        d3dDevice->CreateDepthStencilView(depthResource.Get(), &dsvDesc, dsvHandle);

        ResourceViewDesc dsvViewDesc = { EResourceViewType::DepthStencilView };
        dsvViews[slice] = std::make_unique<ResourceView_DirectX>(dsvViewDesc, dsvHandle, this);

        // Read-only DSV — D3D12_RESOURCE_STATE_DEPTH_READ 와 함께 사용 (TransparentPass, DebugLinePass)
        D3D12_DEPTH_STENCIL_VIEW_DESC readOnlyDsvDesc = dsvDesc;
        readOnlyDsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;

        D3D12_CPU_DESCRIPTOR_HANDLE readOnlyDsvHandle = readOnlyDsvHeap->GetCPUDescriptorHandleForHeapStart();
        readOnlyDsvHandle.ptr += static_cast<SIZE_T>(slice) * dsvIncrement;
        d3dDevice->CreateDepthStencilView(depthResource.Get(), &readOnlyDsvDesc, readOnlyDsvHandle);

        ResourceViewDesc readOnlyDsvViewDesc = { EResourceViewType::DepthStencilView };
        readOnlyDsvViews[slice] = std::make_unique<ResourceView_DirectX>(readOnlyDsvViewDesc, readOnlyDsvHandle, this);
    }

    // 2. SRV — 전역 힙, 배열 전체를 커버하는 1개 (Deferred Lighting Compute에서 Depth 읽기)
    globalSrvHeap = rdDevice->GetGlobalSRVHeap();

    auto srvSlot = rdDevice->AllocateSRVDescriptor();
    srvGpuHandle = srvSlot.gpu;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                  = srvFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if (arraySize > 1)
    {
        srvDesc.ViewDimension                  = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MipLevels       = 1;
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize       = arraySize;
    }
    else
    {
        srvDesc.ViewDimension       = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
    }

    d3dDevice->CreateShaderResourceView(depthResource.Get(), &srvDesc, srvSlot.cpu);
}

void DepthBuffer_DirectX::BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute)
{
    auto* d3dCmd = static_cast<CommandList_DirectX*>(cmdList)->GetHandle();
    ID3D12DescriptorHeap* heaps[] = { globalSrvHeap };
    d3dCmd->SetDescriptorHeaps(1, heaps);
    if (isCompute)
        d3dCmd->SetComputeRootDescriptorTable(slot, srvGpuHandle);
    else
        d3dCmd->SetGraphicsRootDescriptorTable(slot, srvGpuHandle);
}
