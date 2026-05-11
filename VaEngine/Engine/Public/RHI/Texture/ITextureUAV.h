#pragma once

#include "RHI/IRHIResource.h"
#include "RHI/Common_RHI.h"

class IRenderDevice;
class ICommandList;

// Compute 셰이더가 RWTexture로 쓰고, graphics 셰이더가 SRV로 읽을 수 있는 텍스처.
// 같은 리소스에 SRV/UAV 두 개의 view가 동시에 생성된다.
class ITextureUAV : public IRHIResource
{
public:
	virtual ~ITextureUAV() = default;

	virtual void Create(IRenderDevice* device,
	                    EPixelFormat   format,
	                    uint32_t       width,
	                    uint32_t       height,
	                    uint32_t       arraySize = 1) = 0;

	// UAV (descriptor table) 바인딩 — compute pass 또는 graphics pass에서 RWTexture로 사용 시
	virtual void BindUAV(ICommandList* cmdList, uint32_t slot, bool isCompute) = 0;

	// SRV (descriptor table) 바인딩 — compute 결과를 후속 graphics pass에서 읽을 때
	virtual void BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute) = 0;
};
