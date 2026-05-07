#pragma once

#include "RHI/IResourceView.h"
#include "Common_DirectX.h"

class IRHIResource;

//composition
class ResourceView_DirectX : public IResourceView
{
public:
	ResourceView_DirectX(const ResourceViewDesc& inDesc, D3D12_CPU_DESCRIPTOR_HANDLE inRTVHandle, IRHIResource* inResource);

	IRHIResource*			GetResource()	const override;
	EResourceViewType		GetType()		const override { return desc.type; }
	const ResourceViewDesc& GetDesc()		const override { return desc; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return rtvHandle; }

private:
	ResourceViewDesc				desc;
	IRHIResource*					resource;
	D3D12_CPU_DESCRIPTOR_HANDLE		rtvHandle; // RTV, DSV, SRV 등 공통으로 사용할 수 있는 핸들
};