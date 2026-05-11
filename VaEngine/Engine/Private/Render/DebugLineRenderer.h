#pragma once

#include "RHI/IBuffer.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"
#include "Utilities/DebuggingHelper.h"
#include "Math/Container.h"

#include <memory>
#include <vector>

class IRenderDevice;
class ICommandList;

class DebugLineRenderer
{
public:
    void Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc);
    void Render(ICommandList* cmdList,
                const std::vector<DebugLineEntry>& lines,
                const Matrix4x4& viewProj);

private:
    struct LineVertex { float x, y, z, r, g, b, a; };  // 28 bytes

    static constexpr uint32_t MAX_LINE_VERTS = 8192;    // 4096 lines

    std::unique_ptr<IBindingLayout> bindingLayout;
    std::unique_ptr<IShader>        shader;
    std::unique_ptr<IPipelineState> pipelineState;       // depthEnable=false (기즈모 등)
    std::unique_ptr<IPipelineState> pipelineStateDepth;  // depthEnable=true  (픽 레이 등)
    std::unique_ptr<IBuffer>        vertexBuffer;
    std::unique_ptr<IBuffer>        vertexBufferDepth;
    std::unique_ptr<IBuffer>        viewProjBuffer;
};
