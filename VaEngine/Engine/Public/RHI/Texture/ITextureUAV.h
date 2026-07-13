#pragma once

#include "RHI/BaseRHIResource.h"
#include "RHI/Common_RHI.h"

class IRenderDevice;
class ICommandList;

// Compute 셰이더가 RWTexture로 쓰고, graphics 셰이더가 SRV로 읽을 수 있는 텍스처.
// 같은 리소스에 SRV/UAV 두 개의 view가 동시에 생성된다.
class ITextureUAV : public BaseRHIResource
{
public:
	virtual ~ITextureUAV() = default;

	// mipLevels>1: 밉당 별도 UAV 디스크립터를 생성(D3D12 제약 — UAV는 단일 서브리소스만 참조 가능),
	// SRV는 전체 체인을 커버해 그래픽스/컴퓨트 양쪽에서 SampleLevel로 밉 보간 읽기가 가능하다.
	virtual void Create(IRenderDevice* device,
	                    EPixelFormat   format,
	                    uint32_t       width,
	                    uint32_t       height,
	                    uint32_t       arraySize = 1,
	                    uint32_t       mipLevels = 1) = 0;

	// UAV (descriptor table) 바인딩 — compute pass 또는 graphics pass에서 RWTexture로 사용 시.
	// mipSlice: 컴퓨트가 기록할 대상 밉 레벨(밉당 디스크립터가 분리되어 있어야 함).
	virtual void BindUAV(ICommandList* cmdList, uint32_t slot, bool isCompute, uint32_t mipSlice = 0) = 0;

	// SRV (descriptor table) 바인딩 — compute 결과를 후속 graphics pass에서 읽을 때
	virtual void BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute) = 0;

	// RTV 핸들 — BeginRenderPass의 RenderPassAttachment::view에 전달 (SkyPass / TransparentPass)
	virtual IResourceView* GetRTV() const = 0;
};
