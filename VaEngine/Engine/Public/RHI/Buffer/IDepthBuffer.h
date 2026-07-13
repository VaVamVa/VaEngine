#pragma once

#include "RHI/BaseRHIResource.h"

class IResourceView;
class ICommandList;

// DepthBuffer 자신이 곧 하나의 트랙 가능한 리소스이므로(ColorBuffer/TextureUAV와 동일 패턴)
// BaseRHIResource를 직접 상속한다 — 별도 GetResource() 접근자 불필요, DepthBuffer 포인터
// 자체가 BaseRHIResource*로 암시적 업캐스트되어 PassResourceDecl에 바로 사용 가능하다.
class IDepthBuffer : public BaseRHIResource
{
public:
    virtual ~IDepthBuffer() override = default;

    // slice: arraySize>1(CSM 등)일 때 캐스케이드/레이어 인덱스. 단일 depth(arraySize==1)는 기본값 0만 유효.
    virtual IResourceView* GetView(uint32_t slice = 0)         const = 0;  // DSV_FLAG_NONE — DepthWrite 상태에서 사용
    virtual IResourceView* GetReadOnlyView(uint32_t slice = 0) const = 0;  // DSV_FLAG_READ_ONLY_DEPTH — DepthRead 상태에서 사용

    // Deferred Lighting Compute에서 Depth를 SRV로 읽어 WorldPos를 역투영할 때 사용.
    // 리소스는 typeless 포맷으로 생성되고, SRV view는 R24_UNORM_X8_TYPELESS로 해석한다.
    virtual void BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute) = 0;
};
