#pragma once

#include "RHI/Buffer/IBuffer.h"
#include "RHI/DirectX/Common_DirectX.h"

class Buffer_DirectX : public IBuffer
{
public:
	void Create(ID3D12Device* device, const BufferDesc& desc);

	void* Map()   override;
	void  Unmap() override;

	void* GetNativeResource() const override;

private:
	ComPtr<ID3D12Resource> bufferResource;
};
