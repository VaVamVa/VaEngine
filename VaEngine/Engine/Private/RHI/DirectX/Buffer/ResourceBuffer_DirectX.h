#pragma once

#include "RHI/Buffer/IResourceBuffer.h"
#include "RHI/IRHIResource.h"
#include "RHI/DirectX/Common_DirectX.h"

class ResourceBuffer_DX12 : public IResourceBuffer
{
public:
	void Create(ID3D12Device* device, const ResourceBufferDesc& desc);

	void Upload(const void* data, size_t size) override;
	IRHIResource* GetResource() const override;

private:
	std::unique_ptr<TRHIResource<ID3D12Resource>> bufferResource;
};
