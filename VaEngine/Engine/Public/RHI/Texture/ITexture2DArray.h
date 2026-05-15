#pragma once

#include <cstdint>

class IRenderDevice;
class ICommandList;

// 본 변환 행렬 배열 전용 GPU 텍스처
// Format: R32G32B32A32_FLOAT
// Width = boneCount * 4  (행렬 1개 = float4 행 × 4)
// Height = frameCount
// ArraySize = clipCount
class ITexture2DArray
{
public:
    virtual ~ITexture2DArray() = default;

    virtual void Upload(IRenderDevice* device,
                        const void*    data,
                        uint32_t       width,
                        uint32_t       height,
                        uint32_t       arraySize) = 0;

    // isCompute=true 시 SetComputeRootDescriptorTable, 기본은 graphics
    virtual void Bind(ICommandList* cmdList, uint32_t slot, bool isCompute = false) = 0;

    virtual bool HasAlpha() const = 0;
};
