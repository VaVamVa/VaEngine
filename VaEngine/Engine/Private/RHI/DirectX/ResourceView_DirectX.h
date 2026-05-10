#pragma once

#include "RHI/IResourceView.h"
#include "Common_DirectX.h"

class IRHIResource;

//composition
class ResourceView_DirectX : public IResourceView
{
public:
	// RTV/DSV용 — CPU 핸들만 보유 (descriptor table 바인딩 없음)
	ResourceView_DirectX(const ResourceViewDesc& inDesc, D3D12_CPU_DESCRIPTOR_HANDLE inCpuHandle, IRHIResource* inResource);

	// SRV/UAV용 — descriptor table 바인딩을 위해 GPU 핸들도 보유 (전역 SRV 힙에 할당된 슬롯)
	ResourceView_DirectX(const ResourceViewDesc& inDesc,
	                     D3D12_CPU_DESCRIPTOR_HANDLE inCpuHandle,
	                     D3D12_GPU_DESCRIPTOR_HANDLE inGpuHandle,
	                     IRHIResource* inResource);

	IRHIResource*			GetResource()	const override;
	EResourceViewType		GetType()		const override { return desc.type; }
	const ResourceViewDesc& GetDesc()		const override { return desc; }

	// RTV, DSV, SRV CPU 핸들 — 기존 호출부 호환을 위해 이름 유지
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return cpuHandle; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() const { return cpuHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const { return gpuHandle; }

private:
	ResourceViewDesc				desc;
	IRHIResource*					resource;
	D3D12_CPU_DESCRIPTOR_HANDLE		cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE		gpuHandle = {};  // RTV/DSV view는 사용 안 함 (zero-init)
};