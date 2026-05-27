#pragma once

#include "RHI/Buffer/IDepthBuffer.h"
#include "RHI/IRHIResource.h"
#include "RHI/IResourceView.h"
#include "Common_DirectX.h"

#include <memory>

class IRenderDevice;
class ICommandList;

class DepthBuffer_DirectX : public IDepthBuffer
{
public:
    void Create(IRenderDevice* device, uint32_t width, uint32_t height, DXGI_FORMAT format);

    IRHIResource*  GetResource() const override { return const_cast<DepthResource*>(&depthResource); }
    IResourceView* GetView()     const override { return dsvView.get(); }
    void           BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute) override;

private:
    struct DepthResource : IRHIResource
    {
        ComPtr<ID3D12Resource> resource;
        void* GetNativeResource() const override { return resource.Get(); }
    };

    DepthResource                  depthResource;
    ComPtr<ID3D12DescriptorHeap>   dsvHeap;
    std::unique_ptr<IResourceView> dsvView;

    D3D12_GPU_DESCRIPTOR_HANDLE    srvGpuHandle  = {};
    std::unique_ptr<IResourceView> srvView;
    ID3D12DescriptorHeap*          globalSrvHeap = nullptr;
};
