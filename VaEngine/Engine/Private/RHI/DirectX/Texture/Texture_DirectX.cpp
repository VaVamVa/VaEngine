#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture_DirectX.h"
#include "RHI/DirectX/RenderDevice_DirectX.h"
#include "RHI/DirectX/CommandList_DirectX.h"


#include <stdexcept>

void Texture_DirectX::LoadFromFile(IRenderDevice* device, const char* path)
{
	int width, height, channels;
	stbi_uc* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
	if (!pixels)
		throw std::runtime_error("Failed to load texture file");

	hasAlpha = (channels == 4);
	Upload(static_cast<RenderDevice_DirectX*>(device),
		pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
	stbi_image_free(pixels);
}

void Texture_DirectX::LoadFromMemory(IRenderDevice* device, const void* pixels, uint32_t width, uint32_t height)
{
	Upload(static_cast<RenderDevice_DirectX*>(device), pixels, width, height);
}

void Texture_DirectX::Upload(RenderDevice_DirectX* rdDevice, const void* pixels, uint32_t width, uint32_t height)
{
	ID3D12Device* device = rdDevice->GetDevice();

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

	// 5. 복사 명령 기록 및 GPU 완료 대기
	RawResource texRes{ textureResource.Get() };
	RawResource uplRes{ uploadBuffer.Get() };
	rdDevice->ImmediateSubmit([&](ICommandList* cmdList)
	{
		cmdList->CopyBufferToTexture(
			&texRes, 0,
			&uplRes, footprint.Offset,
			static_cast<uint32_t>(width), height,
			footprint.Footprint.RowPitch);

		ResourceBarrier barrier{ &texRes,
			EResourceState::CopyDest,
			EResourceState::PixelShaderResource };
		cmdList->SetResourceBarrier(1, &barrier);
	});

	// 7. 전역 SRV heap에서 슬롯 할당 후 SRV 생성
	auto srv = rdDevice->AllocateSRVDescriptor();
	srvGpuHandle  = srv.gpu;
	globalSrvHeap = rdDevice->GetGlobalSRVHeap();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels     = 1;
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, srv.cpu);
}

void Texture_DirectX::Bind(ICommandList* cmdList, uint32_t slot, bool isCompute)
{
	auto* d3dCmd = static_cast<CommandList_DirectX*>(cmdList)->GetHandle();
	ID3D12DescriptorHeap* heaps[] = { globalSrvHeap };
	d3dCmd->SetDescriptorHeaps(1, heaps);
	if (isCompute)
		d3dCmd->SetComputeRootDescriptorTable(slot, srvGpuHandle);
	else
		d3dCmd->SetGraphicsRootDescriptorTable(slot, srvGpuHandle);
}
