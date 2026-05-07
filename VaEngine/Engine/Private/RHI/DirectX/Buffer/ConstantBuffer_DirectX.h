#pragma once

#include "RHI/Buffer/IConstantBuffer.h"
#include "RHI/IRHIResource.h"
#include "RHI/DirectX/Common_DirectX.h"

class ConstantBuffer_DirectX : public IConstantBuffer
{
public:
	void Create(ID3D12Device* device, size_t size);

	void SetData(const void* data, size_t size) override;
	void Bind(ICommandList* cmdList, uint32_t slot) override;

private:
	std::unique_ptr<TRHIResource<ID3D12Resource>> bufferResource;
};
