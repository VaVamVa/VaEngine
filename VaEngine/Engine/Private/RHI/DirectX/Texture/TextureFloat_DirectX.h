#pragma once

#include "RHI/Texture/ITexture.h"
#include "RHI/DirectX/Common_DirectX.h"

class RenderDevice_DirectX;

class TextureFloat_DirectX : public ITexture
{
public:
    void LoadFromFile(IRenderDevice* device, const char* path) override;
    void LoadFromMemory(IRenderDevice* device, const void* pixels, uint32_t width, uint32_t height) override;
    void Bind(ICommandList* cmdList, uint32_t slot, bool isCompute = false) override;
    bool HasAlpha() const override { return false; }

private:
    void Upload(RenderDevice_DirectX* device, const float* pixels, uint32_t width, uint32_t height);

    ComPtr<ID3D12Resource>      textureResource;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle  = {};
    ID3D12DescriptorHeap*       globalSrvHeap = nullptr;
};
