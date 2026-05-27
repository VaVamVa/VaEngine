#pragma once

#include "RHI/Buffer/IBuffer.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"
#include "Utilities/DebuggingHelper.h"

#include <memory>
#include <vector>
#include <map>

class IRenderDevice;
class ICommandList;
class GlyphAtlas;

class DebugTextRenderer
{
public:
    DebugTextRenderer();
    ~DebugTextRenderer();

    void Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc, const char* ttfPath, float fontSize = 18.0f);
    void Render(ICommandList* cmdList, IRenderDevice* device,
                const std::vector<DebugTextEntry>& entries,
                const std::map<uint32_t, DebugPanelEntry>& panelEntries,
                uint32_t screenW, uint32_t screenH);

private:
    struct GlyphVertex
    {
        float x, y;        // 화면 픽셀 좌표
        float u, v;        // Atlas UV
        float r, g, b, a;  // 색상
    };

    static constexpr uint32_t MAX_GLYPH_VERTS = 8192 * 6;  // 최대 8192 글자

    std::unique_ptr<GlyphAtlas>     glyphAtlas;
    std::unique_ptr<IBindingLayout> bindingLayout;
    std::unique_ptr<IShader>        shader;
    std::unique_ptr<IPipelineState> pipelineState;
    std::unique_ptr<IBuffer>        vertexBuffer;
    std::unique_ptr<IBuffer>        screenSizeBuffer;
};
