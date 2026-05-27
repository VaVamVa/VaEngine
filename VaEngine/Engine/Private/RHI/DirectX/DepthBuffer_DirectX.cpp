#include "DepthBuffer_DirectX.h"
#include "ResourceView_DirectX.h"
#include "RenderDevice_DirectX.h"
#include "CommandList_DirectX.h"
#include "RHI/Common_RHI.h"

#include <stdexcept>

void DepthBuffer_DirectX::Create(IRenderDevice* device, uint32_t width, uint32_t height, DXGI_FORMAT format)
{
    auto* rdDevice  = static_cast<RenderDevice_DirectX*>(device);
    auto* d3dDevice = rdDevice->GetDevice();

    // 리소스는 typeless로 생성해야 DSV와 SRV를 동시에 생성할 수 있다.
    // D3D12는 D24_UNORM_S8_UINT 포맷 리소스에 SRV 생성을 허용하지 않는다.
    const DXGI_FORMAT resourceFormat = DXGI_FORMAT_R24G8_TYPELESS;
    const DXGI_FORMAT dsvFormat      = DXGI_FORMAT_D24_UNORM_S8_UINT;
    const DXGI_FORMAT srvFormat      = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

    D3D12_CLEAR_VALUE clearVal            = {};
    clearVal.Format                       = dsvFormat;
    clearVal.DepthStencil.Depth           = 1.0f;
    clearVal.DepthStencil.Stencil         = 0;

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resDesc   = CD3DX12_RESOURCE_DESC::Tex2D(
        resourceFormat, width, height,
        1, 0, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );

    if (FAILED(d3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearVal,
        IID_PPV_ARGS(&depthResource.resource))))
    {
        throw std::runtime_error("Failed to create depth buffer resource");
    }

    // 1. DSV — 1-slot private heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors             = 1;
    dsvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap))))
    {
        throw std::runtime_error("Failed to create DSV descriptor heap");
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format        = dsvFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags         = D3D12_DSV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    d3dDevice->CreateDepthStencilView(depthResource.resource.Get(), &dsvDesc, dsvHandle);

    ResourceViewDesc dsvViewDesc = { EResourceViewType::DepthStencilView };
    dsvView = std::make_unique<ResourceView_DirectX>(dsvViewDesc, dsvHandle, &depthResource);

    // 2. SRV — 전역 힙 (Deferred Lighting Compute에서 Depth 읽기)
    globalSrvHeap = rdDevice->GetGlobalSRVHeap();

    auto srvSlot = rdDevice->AllocateSRVDescriptor();
    srvGpuHandle = srvSlot.gpu;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                  = srvFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels     = 1;

    d3dDevice->CreateShaderResourceView(depthResource.resource.Get(), &srvDesc, srvSlot.cpu);
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
