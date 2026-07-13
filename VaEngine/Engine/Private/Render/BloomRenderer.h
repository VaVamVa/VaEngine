#pragma once

#include "RHI/IRenderDevice.h"
#include "RHI/Buffer/IBuffer.h"
#include "RHI/Buffer/IColorBuffer.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"

#include <memory>
#include <cstdint>

class RenderGraph;
class ICommandList;
class ITextureUAV;
struct FrameOutput;

// hdrOut(풀 해상도) → BrightPass → BlurH → BlurV(half-res 핑퐁) → Composite(hdrOut에 가산 블렌딩)
// PostProcess 체인이 Bloom 하나뿐이므로 IPostProcessPass 같은 별도 추상화를 두지 않음
// (CLAUDE.md: 사용되지 않는 추상화는 추가하지 않는다 — 두 번째 효과가 생기면 그때 일반화)
class BloomRenderer
{
public:
    void Initialize(IRenderDevice* device,
                    const ShaderDesc& brightDesc, const ShaderDesc& blurHDesc,
                    const ShaderDesc& blurVDesc,  const ShaderDesc& compositeDesc,
                    uint32_t fullWidth, uint32_t fullHeight);

    // hdrOut에 결과를 가산 블렌딩 — Transparent 이후, Blit 이전에 호출
    void AddPasses(RenderGraph& graph, ITextureUAV* hdrOut, const FrameOutput& output);

    void RenderBrightPass(ICommandList* cmdList, ITextureUAV* hdrOut);
    void RenderBlurH(ICommandList* cmdList);
    void RenderBlurV(ICommandList* cmdList);
    void RenderComposite(ICommandList* cmdList);

    IColorBuffer* GetBloomA()    const { return bloomA.get(); }
    uint32_t      GetHalfWidth() const { return halfWidth; }
    uint32_t      GetHalfHeight() const { return halfHeight; }

private:
    std::unique_ptr<IBindingLayout> bindingLayout;  // 4개 PSO 공용 — t0 + b0 레이아웃 동일

    std::unique_ptr<IShader>        brightShader;
    std::unique_ptr<IPipelineState> brightPSO;
    std::unique_ptr<IBuffer>        thresholdBuffer;  // b0 — CB_BloomThreshold

    std::unique_ptr<IShader>        blurHShader;
    std::unique_ptr<IPipelineState> blurHPSO;
    std::unique_ptr<IShader>        blurVShader;
    std::unique_ptr<IPipelineState> blurVPSO;
    std::unique_ptr<IBuffer>        texelSizeBuffer;  // b0 — CB_BlurTexel (H/V 공용)

    std::unique_ptr<IShader>        compositeShader;
    std::unique_ptr<IPipelineState> compositePSO;
    std::unique_ptr<IBuffer>        intensityBuffer;  // b0 — CB_BloomIntensity

    std::unique_ptr<IColorBuffer> bloomA;  // half-res RTV+SRV (bright→A, blurV→A 최종 결과)
    std::unique_ptr<IColorBuffer> bloomB;  // half-res RTV+SRV (blurH 중간 결과)

    uint32_t halfWidth  = 0;
    uint32_t halfHeight = 0;
};
