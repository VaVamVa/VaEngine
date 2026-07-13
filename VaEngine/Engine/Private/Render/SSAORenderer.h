#pragma once

#include "RHI/IRenderDevice.h"
#include "RHI/Buffer/IColorBuffer.h"
#include "RHI/Buffer/IBuffer.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"
#include "RHI/Texture/ITexture.h"

#include <memory>
#include <cstdint>

class RenderGraph;
class ICommandList;
class IDepthBuffer;
struct CameraData;

// GBuffer(Normal+Depth) → SSAORawPass(PS) → SSAOBlurPass(PS) → DeferredLightingPass가 결과를
// ambient(IBL) 항에 곱한다. Bloom과 동일한 풀스크린 삼각형 PS 체인 패턴(컴퓨트 아님) —
// 1차 구현은 Deferred(불투명) 경로 전용. Forward/Transparent는 IBL Stage B 때와 동일하게
// 범위 밖(점진적 확장 패턴).
class SSAORenderer
{
public:
    void Initialize(IRenderDevice* device, const ShaderDesc& ssaoDesc, const ShaderDesc& blurDesc,
                    uint32_t fullWidth, uint32_t fullHeight);

    // gNormalRough/gDepth: GBufferRenderer가 소유한 RT1/Depth에 대한 비소유 포인터.
    void AddPasses(RenderGraph& graph, IColorBuffer* gNormalRough, IDepthBuffer* gDepth,
                  const CameraData& cam);

    IColorBuffer* GetBlurredSSAO() const { return ssaoBlurred.get(); }

    // *Pass::Execute 에서 호출
    void RenderSSAO(ICommandList* cmdList, IColorBuffer* gNormalRough, IDepthBuffer* gDepth,
                    const CameraData& cam);
    void RenderBlur(ICommandList* cmdList);

private:
    void GenerateKernelAndNoise(IRenderDevice* device);

    std::unique_ptr<IBindingLayout> ssaoBindingLayout;
    std::unique_ptr<IShader>        ssaoShader;
    std::unique_ptr<IPipelineState> ssaoPipelineState;
    std::unique_ptr<IBuffer>        cameraBuffer;   // b0 — CB_SSAOCamera, 매 프레임 업로드
    std::unique_ptr<IBuffer>        paramsBuffer;   // b1 — CB_SSAOParams(커널+radius 등), Init 1회
    std::unique_ptr<ITexture>       noiseTexture;   // 4x4 랜덤 회전 벡터 타일

    std::unique_ptr<IBindingLayout> blurBindingLayout;
    std::unique_ptr<IShader>        blurShader;
    std::unique_ptr<IPipelineState> blurPipelineState;
    std::unique_ptr<IBuffer>        blurTexelBuffer;  // b0 — CB_SSAOBlur, Init 1회

    std::unique_ptr<IColorBuffer> ssaoRaw;       // R32_FLOAT, 풀해상도
    std::unique_ptr<IColorBuffer> ssaoBlurred;   // R32_FLOAT, 풀해상도

    uint32_t width  = 0;
    uint32_t height = 0;

    static constexpr uint32_t kKernelSize = 32;
    static constexpr float    kDefaultRadius = 0.5f;  // meter (1 unit = 1m 확정 — Scene Scale 전환 참조)
    static constexpr float    kDefaultBias   = 0.025f;
    static constexpr float    kDefaultPower  = 1.0f;
};
