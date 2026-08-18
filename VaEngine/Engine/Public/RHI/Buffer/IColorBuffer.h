#pragma once

#include "RHI/BaseRHIResource.h"
#include "RHI/Common_RHI.h"

class IRenderDevice;
class ICommandList;
class IResourceView;

class IColorBuffer : public BaseRHIResource
{
public:
	virtual ~IColorBuffer() = default;

	// optimizedClearColor: 이 리소스가 실제로 클리어될 값(D3D12 "optimized clear value")과
	// 다르면 매 클리어 호출이 느린 경로를 탐(경고만 뜨고 동작은 정상 — CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE).
	// nullptr이면 기존과 동일하게 (0,0,0,0) 사용.
	virtual void Create(IRenderDevice* device,
	                    EPixelFormat   format,
	                    uint32_t       width,
	                    uint32_t       height,
	                    const float*   optimizedClearColor = nullptr) = 0;

	virtual IResourceView* GetRTV() const = 0;
	virtual void BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute) = 0;
};