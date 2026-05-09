#include "DepthBuffer_DirectX.h"
#include "ResourceView_DirectX.h"
#include "RHI/Common_RHI.h"

#include <stdexcept>

void DepthBuffer_DirectX::Create(ID3D12Device* device, uint32_t width, uint32_t height, DXGI_FORMAT format)
{
    D3D12_CLEAR_VALUE clearVal = {};
    clearVal.Format                  = format;
    clearVal.DepthStencil.Depth      = 1.0f;
    clearVal.DepthStencil.Stencil    = 0;

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resDesc   = CD3DX12_RESOURCE_DESC::Tex2D(
        format, width, height,
        1, 0, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );

    if (FAILED(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearVal,
        IID_PPV_ARGS(&depthResource.resource))))
    {
        throw std::runtime_error("Failed to create depth buffer resource");
    }

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&dsvHeap))))
    {
        throw std::runtime_error("Failed to create DSV descriptor heap");
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format        = format;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags         = D3D12_DSV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    device->CreateDepthStencilView(depthResource.resource.Get(), &dsvDesc, dsvHandle);

    ResourceViewDesc viewDesc = { EResourceViewType::DepthStencilView };
    dsvView = std::make_unique<ResourceView_DirectX>(viewDesc, dsvHandle, &depthResource);
}
