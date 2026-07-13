#pragma once

#include "RHI/Buffer/IDepthBuffer.h"
#include "RHI/IResourceView.h"
#include "Common_DirectX.h"

#include <memory>
#include <vector>

class IRenderDevice;
class ICommandList;

class DepthBuffer_DirectX : public IDepthBuffer
{
public:
    void Create(IRenderDevice* device, uint32_t width, uint32_t height, DXGI_FORMAT format, uint32_t arraySize = 1);

    void*          GetNativeResource()   const override { return depthResource.Get(); }
    IResourceView* GetView(uint32_t slice = 0)         const override { return dsvViews[slice].get(); }
    IResourceView* GetReadOnlyView(uint32_t slice = 0) const override { return readOnlyDsvViews[slice].get(); }
    void           BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute) override;

private:
    ComPtr<ID3D12Resource>                       depthResource;
    ComPtr<ID3D12DescriptorHeap>                 dsvHeap;
    std::vector<std::unique_ptr<IResourceView>>  dsvViews;
    ComPtr<ID3D12DescriptorHeap>                 readOnlyDsvHeap;
    std::vector<std::unique_ptr<IResourceView>>  readOnlyDsvViews;

    D3D12_GPU_DESCRIPTOR_HANDLE    srvGpuHandle  = {};
    std::unique_ptr<IResourceView> srvView;
    ID3D12DescriptorHeap*          globalSrvHeap = nullptr;
};
