#pragma once

#include "RHI/IRenderDevice.h"
#include "RHI/Buffer/IBuffer.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"

#include <memory>
#include <cstdint>

class RenderGraph;
class RenderScene;
class GBufferRenderer;
class ICommandList;

class DeferredLightingRenderer
{
public:
    void Initialize(IRenderDevice* device, const ShaderDesc& csDesc,
                    uint32_t width, uint32_t height);

    void AddPasses(RenderGraph& graph, GBufferRenderer& gbuffer);

    // DeferredLightingPass::Execute 에서 호출
    void Dispatch(ICommandList* cmdList, GBufferRenderer& gbuffer, const RenderScene& scene);

private:
    std::unique_ptr<IBindingLayout> computeBindingLayout;
    std::unique_ptr<IShader>        computeShader;
    std::unique_ptr<IPipelineState> computePipelineState;

    std::unique_ptr<IBuffer> cameraBuffer;  // CB_DeferredCamera (b0)
    std::unique_ptr<IBuffer> lightsBuffer;  // CB_Lights         (b2)

    uint32_t width  = 0;
    uint32_t height = 0;
};
