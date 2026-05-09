#pragma once

#include "RHI/IDepthBuffer.h"
#include "RHI/IRHIResource.h"
#include "RHI/IResourceView.h"
#include "Common_DirectX.h"

#include <memory>

class DepthBuffer_DirectX : public IDepthBuffer
{
public:
    void Create(ID3D12Device* device, uint32_t width, uint32_t height, DXGI_FORMAT format);

    IRHIResource*  GetResource() const override { return const_cast<DepthResource*>(&depthResource); }
    IResourceView* GetView()     const override { return dsvView.get(); }

private:
    // ComPtr로 수명 관리, GetNativeResource()로 외부 노출
    struct DepthResource : IRHIResource
    {
        ComPtr<ID3D12Resource> resource;
        void* GetNativeResource() const override { return resource.Get(); }
    };

    DepthResource                  depthResource;
    ComPtr<ID3D12DescriptorHeap>   dsvHeap;
    std::unique_ptr<IResourceView> dsvView;
};
