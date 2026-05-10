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
        throw std::runtime_error("Failed to load HDR texture file");

    Upload(static_cast<RenderDevice_DirectX*>(device),
           hdr, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    stbi_image_free(hdr);
}

void TextureFloat_DirectX::LoadFromMemory(IRenderDevice* device, const void* pixels, uint32_t width, uint32_t height)
{
    Upload(static_cast<RenderDevice_DirectX*>(device),
           static_cast<const float*>(pixels), width, height);
}

void TextureFloat_DirectX::Upload(RenderDevice_DirectX* rdDevice, const float* pixels, uint32_t width, uint32_t height)
{
    ID3D12Device* device = rdDevice->GetDevice();

    auto texDesc     = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height);
    auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&textureResource))))
        throw std::runtime_error("Failed to create float texture resource");

    UINT64 uploadSize;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT   numRows;
    UINT64 rowSizeInBytes;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &uploadSize);

    ComPtr<ID3D12Resource> uploadBuffer;
    auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    if (FAILED(device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer))))
        throw std::runtime_error("Failed to create float texture upload buffer");

    BYTE* mapped = nullptr;
    uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
    const uint32_t srcRowStride = width * sizeof(float) * 4;
    for (UINT row = 0; row < numRows; ++row)
        memcpy(
            mapped + footprint.Offset + row * footprint.Footprint.RowPitch,
            reinterpret_cast<const BYTE*>(pixels) + row * srcRowStride,
            rowSizeInBytes);
    uploadBuffer->Unmap(0, nullptr);

    D3D12_COMMAND_QUEUE_DESC qDesc = { D3D12_COMMAND_LIST_TYPE_DIRECT };
    ComPtr<ID3D12CommandQueue>        uploadQueue;
    ComPtr<ID3D12CommandAllocator>    uploadAlloc;
    ComPtr<ID3D12GraphicsCommandList> uploadCmd;
    device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&uploadQueue));
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAlloc));
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAlloc.Get(), nullptr, IID_PPV_ARGS(&uploadCmd));

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource        = textureResource.Get();
    dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource       = uploadBuffer.Get();
    srcLoc.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint = footprint;

    uploadCmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        textureResource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    uploadCmd->ResourceBarrier(1, &barrier);
    uploadCmd->Close();

    ID3D12CommandList* lists[] = { uploadCmd.Get() };
    uploadQueue->ExecuteCommandLists(1, lists);

    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    uploadQueue->Signal(fence.Get(), 1);
    fence->SetEventOnCompletion(1, event);
    WaitForSingleObject(event, INFINITE);
    CloseHandle(event);

    auto srv      = rdDevice->AllocateSRVDescriptor();
    srvGpuHandle  = srv.gpu;
    globalSrvHeap = rdDevice->GetGlobalSRVHeap();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                  = DXGI_FORMAT_R32G32B32A32_FLOAT;
    srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels     = 1;
    device->CreateShaderResourceView(textureResource.Get(), &srvDesc, srv.cpu);
}

void TextureFloat_DirectX::Bind(ICommandList* cmdList, uint32_t slot, bool isCompute)
{
    auto* d3dCmd = static_cast<CommandList_DirectX*>(cmdList)->GetHandle();
    ID3D12DescriptorHeap* heaps[] = { globalSrvHeap };
    d3dCmd->SetDescriptorHeaps(1, heaps);
    if (isCompute)
        d3dCmd->SetComputeRootDescriptorTable(slot, srvGpuHandle);
    else
        d3dCmd->SetGraphicsRootDescriptorTable(slot, srvGpuHandle);
}
