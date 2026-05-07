#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture_DirectX.h"
#include "RHI/DirectX/RenderDevice_DirectX.h"
#include "RHI/DirectX/CommandList_DirectX.h"

#include <stdexcept>

void Texture_DirectX::LoadFromFile(IRenderDevice* device, const wchar_t* path)
{
	char narrowPath[512];
	WideCharToMultiByte(CP_UTF8, 0, path, -1, narrowPath, sizeof(narrowPath), nullptr, nullptr);

	int width, height, channels;
	stbi_uc* pixels = stbi_load(narrowPath, &width, &height, &channels, STBI_rgb_alpha);
	if (!pixels)
		throw std::runtime_error("Failed to load texture file");

	Upload(static_cast<RenderDevice_DirectX*>(device)->GetDevice(),
		pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
	stbi_image_free(pixels);
}

void Texture_DirectX::LoadFromMemory(IRenderDevice* device, const void* pixels, uint32_t width, uint32_t height)
{
	Upload(static_cast<RenderDevice_DirectX*>(device)->GetDevice(), pixels, width, height);
}

void Texture_DirectX::Upload(ID3D12Device* device, const void* pixels, uint32_t width, uint32_t height)
{
	// 1. Default heap 텍스처 리소스 생성
	auto texDesc    = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height);
	auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	if (FAILED(device->CreateCommittedResource(
		&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&textureResource))))
		throw std::runtime_error("Failed to create texture resource");

	// 2. 업로드 버퍼 크기 및 행 배치 계산
	UINT64 uploadSize;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
	UINT   numRows;
	UINT64 rowSizeInBytes;
	device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &uploadSize);

	// 3. Upload heap 버퍼 생성
	ComPtr<ID3D12Resource> uploadBuffer;
	auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
	if (FAILED(device->CreateCommittedResource(
		&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer))))
		throw std::runtime_error("Failed to create upload buffer");

	// 4. 픽셀 데이터 복사 (DX12 행 피치 정렬 고려)
	BYTE* mapped = nullptr;
	uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	for (UINT row = 0; row < numRows; ++row)
		memcpy(
			mapped + footprint.Offset + row * footprint.Footprint.RowPitch,
			static_cast<const BYTE*>(pixels) + row * width * 4,
			rowSizeInBytes);
	uploadBuffer->Unmap(0, nullptr);

	// 5. 일회성 커맨드 인프라 생성
	D3D12_COMMAND_QUEUE_DESC qDesc = { D3D12_COMMAND_LIST_TYPE_DIRECT };
	ComPtr<ID3D12CommandQueue>        uploadQueue;
	ComPtr<ID3D12CommandAllocator>    uploadAlloc;
	ComPtr<ID3D12GraphicsCommandList> uploadCmd;
	device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&uploadQueue));
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAlloc));
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAlloc.Get(), nullptr, IID_PPV_ARGS(&uploadCmd));

	// 6. 복사 명령 기록
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

	// 7. 실행 및 GPU 완료 대기
	ID3D12CommandList* lists[] = { uploadCmd.Get() };
	uploadQueue->ExecuteCommandLists(1, lists);

	ComPtr<ID3D12Fence> fence;
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	uploadQueue->Signal(fence.Get(), 1);
	fence->SetEventOnCompletion(1, event);
	WaitForSingleObject(event, INFINITE);
	CloseHandle(event);

	// 8. SRV 디스크립터 힙 + SRV 생성
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&srvHeap));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels     = 1;
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc,
		srvHeap->GetCPUDescriptorHandleForHeapStart());
}

void Texture_DirectX::Bind(ICommandList* cmdList, uint32_t slot)
{
	auto* d3dCmd = static_cast<CommandList_DirectX*>(cmdList)->GetHandle();
	ID3D12DescriptorHeap* heaps[] = { srvHeap.Get() };
	d3dCmd->SetDescriptorHeaps(1, heaps);
	d3dCmd->SetGraphicsRootDescriptorTable(slot, srvHeap->GetGPUDescriptorHandleForHeapStart());
}
