#pragma once

#include "RHI/Buffer/IResourceBuffer.h"
#include "RHI/IRHIResource.h"
#include "Common_DirectX.h"

class GPUBuffer_DirectX : public IResourceBuffer
{
public:
	GPUBuffer_DirectX() = default;
	virtual ~GPUBuffer_DirectX() = default;

	void Register(class IRenderDevice* device, const GPUBufferDesc& desc) override;
	void Upload(const void* data, size_t size) override;

	const GPUBufferDesc& GetDesc() const override { return desc; }
	class IRHIResource* GetResource() const override { return const_cast<TRHIResource<ID3D12Resource>*>(&resource); }

private:
	GPUBufferDesc desc;
	TRHIResource<ID3D12Resource> resource;
};
