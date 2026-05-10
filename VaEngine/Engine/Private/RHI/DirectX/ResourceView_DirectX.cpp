#include "ResourceView_DirectX.h"

#include "RHI/IRHIResource.h"

ResourceView_DirectX::ResourceView_DirectX(const ResourceViewDesc& inDesc, D3D12_CPU_DESCRIPTOR_HANDLE inCpuHandle, IRHIResource* inResource)
	: desc(inDesc), resource(inResource), cpuHandle(inCpuHandle)
{
}

ResourceView_DirectX::ResourceView_DirectX(const ResourceViewDesc& inDesc,
                                           D3D12_CPU_DESCRIPTOR_HANDLE inCpuHandle,
                                           D3D12_GPU_DESCRIPTOR_HANDLE inGpuHandle,
                                           IRHIResource* inResource)
	: desc(inDesc), resource(inResource), cpuHandle(inCpuHandle), gpuHandle(inGpuHandle)
{
}

IRHIResource* ResourceView_DirectX::GetResource() const
{
	return resource;
}