#pragma once

#include "RHI/Texture/ITextureUAV.h"
#include "RHI/DirectX/Common_DirectX.h"

class RenderDevice_DirectX;

// RWTexture2D / RWTexture2DArray 리소스 + SRV/UAV 두 view 보관.
// Compute 출력 → Graphics 입력 (예: bone palette) 패턴 지원.
class TextureUAV_DirectX : public ITextureUAV
{
public:
	void Create(IRenderDevice* device,
	            EPixelFormat   format,
	            uint32_t       width,
	            uint32_t       height,
	            uint32_t       arraySize = 1) override;

	void BindUAV(ICommandList* cmdList, uint32_t slot, bool isCompute) override;
	void BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute) override;

	void* GetNativeResource() const override { return textureResource.Get(); }

private:
	ComPtr<ID3D12Resource>      textureResource;
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle  = {};
	D3D12_GPU_DESCRIPTOR_HANDLE uavGpuHandle  = {};
	ID3D12DescriptorHeap*       globalSrvHeap = nullptr;
};
