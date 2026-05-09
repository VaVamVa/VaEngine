#include "Texture2DArray_DX.h"
#include "RHI/DirectX/RenderDevice_DirectX.h"
#include "RHI/DirectX/CommandList_DirectX.h"

#include <stdexcept>

void Texture2DArray_DX::Upload(IRenderDevice* iDevice,
                                const void*    data,
                                uint32_t       width,
                                uint32_t       height,
                                uint32_t       arraySize)
{
    auto* rdDevice = static_cast<RenderDevice_DirectX*>(iDevice);
    ID3D12Device* device = rdDevice->GetDevice();

    // 1. Texture2DArray 리소스 생성 (RGBA32F, GPU Default heap)
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width              = width;
    texDesc.Height             = height;
    texDesc.DepthOrArraySize   = static_cast<UINT16>(arraySize);
    texDesc.MipLevels          = 1;
    texDesc.Format             = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texDesc.SampleDesc.Count   = 1;
    texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&textureResource))))
        throw std::runtime_error("Texture2DArray: resource creation failed");

    // 2. 업로드 버퍼 크기 계산 (모든 서브리소스)
    const UINT subresourceCount = arraySize;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(subresourceCount);
    std::vector<UINT>   numRowsArr(subresourceCount);
    std::vector<UINT64> rowSizesArr(subresourceCount);
    UINT64 totalUploadSize = 0;
    device->GetCopyableFootprints(&texDesc, 0, subresourceCount, 0,
        footprints.data(), numRowsArr.data(), rowSizesArr.data(), &totalUploadSize);

    // 3. 업로드 버퍼 생성
    ComPtr<ID3D12Resource> uploadBuffer;
    auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufDesc    = CD3DX12_RESOURCE_DESC::Buffer(totalUploadSize);
    if (FAILED(device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&uploadBuffer))))
        throw std::runtime_error("Texture2DArray: upload buffer creation failed");

    // 4. 픽셀 데이터 복사 (슬라이스별)
    //    각 슬라이스는 width × height float4 데이터 (R32G32B32A32_FLOAT = 16 bytes/px)
    constexpr uint32_t bytesPerPixel = 16;
    const uint32_t sliceBytes = width * height * bytesPerPixel;

    BYTE* mapped = nullptr;
    uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
    for (UINT s = 0; s < subresourceCount; ++s)
    {
        const BYTE* src = static_cast<const BYTE*>(data) + s * sliceBytes;
        for (UINT row = 0; row < numRowsArr[s]; ++row)
        {
            memcpy(
                mapped + footprints[s].Offset + row * footprints[s].Footprint.RowPitch,
                src    + row * width * bytesPerPixel,
                rowSizesArr[s]);
        }
    }
    uploadBuffer->Unmap(0, nullptr);

    // 5. 일회성 커맨드로 복사
    D3D12_COMMAND_QUEUE_DESC qDesc = { D3D12_COMMAND_LIST_TYPE_DIRECT };
    ComPtr<ID3D12CommandQueue>        uploadQueue;
    ComPtr<ID3D12CommandAllocator>    uploadAlloc;
    ComPtr<ID3D12GraphicsCommandList> uploadCmd;
    device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&uploadQueue));
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAlloc));
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                              uploadAlloc.Get(), nullptr, IID_PPV_ARGS(&uploadCmd));

    for (UINT s = 0; s < subresourceCount; ++s)
    {
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource        = textureResource.Get();
        dst.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = s;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource       = uploadBuffer.Get();
        src.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprints[s];

        uploadCmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        textureResource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
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

    // 6. 전역 SRV heap에서 슬롯 할당 후 SRV 생성
    auto srv = rdDevice->AllocateSRVDescriptor();
    srvGpuHandle  = srv.gpu;
    globalSrvHeap = rdDevice->GetGlobalSRVHeap();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                          = DXGI_FORMAT_R32G32B32A32_FLOAT;
    srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2DArray.MipLevels        = 1;
    srvDesc.Texture2DArray.ArraySize        = arraySize;
    device->CreateShaderResourceView(textureResource.Get(), &srvDesc, srv.cpu);
}

void Texture2DArray_DX::Bind(ICommandList* cmdList, uint32_t slot)
{
    auto* d3dCmd = static_cast<CommandList_DirectX*>(cmdList)->GetHandle();
    ID3D12DescriptorHeap* heaps[] = { globalSrvHeap };
    d3dCmd->SetDescriptorHeaps(1, heaps);
    d3dCmd->SetGraphicsRootDescriptorTable(slot, srvGpuHandle);
}
