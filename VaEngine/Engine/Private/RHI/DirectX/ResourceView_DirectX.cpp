#include "ResourceView_DirectX.h"

#include "RHI/BaseRHIResource.h"

ResourceView_DirectX::ResourceView_DirectX(const ResourceViewDesc& inDesc, D3D12_CPU_DESCRIPTOR_HANDLE inCpuHandle, BaseRHIResource* inResource)
	: desc(inDesc), resource(inResource), cpuHandle(inCpuHandle)
{
}

ResourceView_DirectX::ResourceView_DirectX(const ResourceViewDesc& inDesc,
                                           D3D12_CPU_DESCRIPTOR_HANDLE inCpuHandle,
                                           D3D12_GPU_DESCRIPTOR_HANDLE inGpuHandle,
                                           BaseRHIResource* inResource)
	: desc(inDesc), resource(inResource), cpuHandle(inCpuHandle), gpuHandle(inGpuHandle)
{
}

BaseRHIResource* ResourceView_DirectX::GetResource() const
{
	return resource;
}