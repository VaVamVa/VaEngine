// STB_IMAGE_IMPLEMENTATION은 Texture_DirectX.cpp에 정의됨 — 중복 정의 금지
#include <stb_image.h>

#include "TextureFloat_DirectX.h"
#include "RHI/DirectX/RenderDevice_DirectX.h"
#include "RHI/DirectX/CommandList_DirectX.h"

#include <stdexcept>
#include <vector>

void TextureFloat_DirectX::LoadFromFile(IRenderDevice* device, const char* path)
{
    int width, height, channels;
    float* hdr = stbi_loadf(path, &width, &height, &channels, STBI_rgb_alpha);
    if (!hdr)
    {
        throw std::runtime_error("Failed to load HDR texture file");
    }

    LoadFromMemory(device, hdr, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    stbi_image_free(hdr);
}

void TextureFloat_DirectX::LoadFromMemory(IRenderDevice* device, const void* pixels, uint32_t width, uint32_t height)
{
    if (RenderDevice_DirectX* dxLayer = static_cast<RenderDevice_DirectX*>(device))
    {
        ID3D12Device* dxDevice = dxLayer->GetDevice();

        auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height);
        auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        if (FAILED(dxDevice->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&textureResource)))
            )
        {
            throw std::runtime_error("Failed to create float texture resource");
        }

        UINT64 uploadSize;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        UINT   numRows;
        UINT64 rowSizeInBytes;
        dxDevice->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &uploadSize);

        ComPtr<ID3D12Resource> uploadBuffer;
        auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
        if (FAILED(dxDevice->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer)))
            )
        {
            throw std::runtime_error("Failed to create float texture upload buffer");
        }

        BYTE* mapped = nullptr;
        uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
        const uint32_t srcRowStride = width * sizeof(float) * 4;
        for (UINT row = 0; row < numRows; ++row)
        {
            memcpy(
                mapped + footprint.Offset + row * footprint.Footprint.RowPitch,
                reinterpret_cast<const BYTE*>(pixels) + row * srcRowStride,
                rowSizeInBytes
            );
        }
        uploadBuffer->Unmap(0, nullptr);

        RawResource texRes{ textureResource.Get() };
        RawResource uplRes{ uploadBuffer.Get() };
        device->ImmediateSubmit([&](ICommandList* cmdList)
        {
            cmdList->CopyBufferToTexture(
                &texRes, 0,
                &uplRes, footprint.Offset,
                width, height,
                footprint.Footprint.RowPitch);

            ResourceBarrier barrier{ &texRes,
                EResourceState::CopyDest,
                EResourceState::PixelShaderResource };
            cmdList->SetResourceBarrier(1, &barrier);
        });

        auto srv = dxLayer->AllocateSRVDescriptor();
        srvGpuHandle = srv.gpu;
        globalSrvHeap = dxLayer->GetGlobalSRVHeap();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        dxDevice->CreateShaderResourceView(textureResource.Get(), &srvDesc, srv.cpu);
    }
}


void TextureFloat_DirectX::Bind(ICommandList* cmdList, uint32_t slot, bool isCompute)
{
    auto* d3dCmd = static_cast<CommandList_DirectX*>(cmdList)->GetHandle();
    ID3D12DescriptorHeap* heaps[] = { globalSrvHeap };
    d3dCmd->SetDescriptorHeaps(1, heaps);
    if (isCompute)
    {
        d3dCmd->SetComputeRootDescriptorTable(slot, srvGpuHandle);
    }
    else
    {
        d3dCmd->SetGraphicsRootDescriptorTable(slot, srvGpuHandle);
    }
}
