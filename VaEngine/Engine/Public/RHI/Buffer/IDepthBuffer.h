#pragma once

class IRHIResource;
class IResourceView;
class ICommandList;

class IDepthBuffer
{
public:
    virtual ~IDepthBuffer() = default;

    virtual IRHIResource*  GetResource() const = 0;
    virtual IResourceView* GetView()     const = 0;

    // Deferred Lighting Compute에서 Depth를 SRV로 읽어 WorldPos를 역투영할 때 사용.
    // 리소스는 typeless 포맷으로 생성되고, SRV view는 R24_UNORM_X8_TYPELESS로 해석한다.
    virtual void BindSRV(ICommandList* cmdList, uint32_t slot, bool isCompute) = 0;
};
