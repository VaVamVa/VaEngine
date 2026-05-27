#pragma once

#include "RHI/Buffer/IColorBuffer.h"
#include "RHI/DirectX/Common_DirectX.h"

#include <memory>

class ColorBuffer_DirectX : public IColorBuffer
{
public:
	void Create(IRenderDevice* device,
	            EPixelFormat   format,
	            uint32_t       width,
	            uint32_t       height) override;

	IResourceView* GetRTV() const override { return rtvView.get(); }
	void BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute) override;

	void* GetNativeResource() const override { return textureResource.Get(); }

private:
	ComPtr<ID3D12Resource>         textureResource;
	ComPtr<ID3D12DescriptorHeap>   rtvHeap;
	std::unique_ptr<IResourceView> rtvView;

	D3D12_GPU_DESCRIPTOR_HANDLE    srvGpuHandle  = {};
	ID3D12DescriptorHeap*          globalSrvHeap = nullptr;
};
