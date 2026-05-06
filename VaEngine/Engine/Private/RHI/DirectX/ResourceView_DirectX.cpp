#include "ResourceView_DirectX.h"

#include "RHI/IRHIResource.h"

ResourceView_DirectX::ResourceView_DirectX(const ResourceViewDesc& inDesc, D3D12_CPU_DESCRIPTOR_HANDLE inRTVHandle, IRHIResource* inResource)
	: desc(inDesc), resource(inResource), rtvHandle(inRTVHandle)
{
}

IRHIResource* ResourceView_DirectX::GetResource() const
{
	return resource;
}