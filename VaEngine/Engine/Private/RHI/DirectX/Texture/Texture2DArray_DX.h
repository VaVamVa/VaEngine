#pragma once

#include "RHI/Texture/ITexture2DArray.h"
#include "RHI/DirectX/Common_DirectX.h"

class Texture2DArray_DX : public ITexture2DArray
{
public:
    void Upload(IRenderDevice* device,
                const void*    data,
                uint32_t       width,
                uint32_t       height,
                uint32_t       arraySize) override;

    void Bind(ICommandList* cmdList, uint32_t slot) override;

private:
    ComPtr<ID3D12Resource>      textureResource;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle  = {};
    ID3D12DescriptorHeap*       globalSrvHeap = nullptr;
};
