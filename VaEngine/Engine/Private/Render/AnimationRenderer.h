#pragma once

#include "Render/IRenderer.h"
#include "Render/IMaterial.h"
#include "Animation/AnimController.h"

#include "RHI/IBuffer.h"
#include "RHI/IResourceView.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"

#include <memory>

class RenderScene;

class AnimationRenderer : public IRenderer
{
public:
    void Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc) override;
    void AddPasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& scene) override;
    void Render(ICommandList* cmdList, const RenderScene& scene);

    // Step 3 — bone palette compute pass (graphics pass 직전에 실행)
    void RenderCompute(ICommandList* cmdList, const RenderScene& scene);

    IMaterial*    GetMaterial()        const { return material.get(); }

    // Pass 들이 사용 — compute root sig + PSO 등을 외부에서 접근
    IPipelineState* GetComputePipelineState() const { return computePipelineState.get(); }
    IPipelineState* GetGraphicsPipelineState() const { return pipelineState.get(); }
    IBuffer*        GetViewProjBuffer()       const { return viewProjBuffer.get(); }
    IBuffer*        GetLightsBuffer()         const { return lightsBuffer.get(); }
    IBuffer*        GetInstanceBuffer()       const { return instanceBuffer.get(); }

private:
    // Graphics pass
    std::unique_ptr<IBindingLayout> bindingLayout;
    std::unique_ptr<IShader>        shader;
    std::unique_ptr<IPipelineState> pipelineState;
    std::unique_ptr<IBuffer>        viewProjBuffer;   // b0
    std::unique_ptr<IBuffer>        lightsBuffer;     // b2
    std::unique_ptr<IBuffer>        instanceBuffer;   // slot 1: per-instance world

    std::unique_ptr<IMaterial>      material;

    // Compute pass — bone palette pre-computation
    std::unique_ptr<IBindingLayout> computeBindingLayout;
    std::unique_ptr<IShader>        computeShader;
    std::unique_ptr<IPipelineState> computePipelineState;
    // ※ BonePalette buffer/UAV/SRV는 SkinnedMesh가 소유 (mesh별 한 buffer 공유)

    static constexpr uint32_t MAX_INSTANCES = 500;
};
