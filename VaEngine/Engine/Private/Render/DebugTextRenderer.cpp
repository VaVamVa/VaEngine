#include "DebugTextRenderer.h"
#include "Utilities/GlyphAtlas.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Common_RHI.h"

#include <algorithm>
#include <cstring>

DebugTextRenderer::DebugTextRenderer()  = default;
DebugTextRenderer::~DebugTextRenderer() = default;

void DebugTextRenderer::Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc,
                                    const char* ttfPath, float fontSize)
{
    // GlyphAtlas 초기화
    glyphAtlas = std::make_unique<GlyphAtlas>();
    glyphAtlas->Initialize(device, ttfPath, fontSize);

    // 바인딩 레이아웃: b0(CB, Vertex) + t0(Texture, Pixel)
    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Vertex },
        { EBindingType::Texture,        0, EShaderStage::Pixel  },
    };
    bindingLayout = device->CreateBindingLayout(bindings, 2);

    shader = device->CreateShader(shaderDesc);

    // 정점 입력: POSITION(float2), TEXCOORD(float2), COLOR(float4) — 32 bytes/vertex
    VertexInputDesc inputs[] = {
        { "POSITION", 0, EPixelFormat::R32G32_FLOAT,       0,  0, false },
        { "TEXCOORD", 0, EPixelFormat::R32G32_FLOAT,       8,  0, false },
        { "COLOR",    0, EPixelFormat::R32G32B32A32_FLOAT, 16, 0, false },
    };

    PipelineStateDesc psoDesc = {
        .shader           = shader.get(),
        .vertexInputs     = inputs,
        .vertexInputCount = 3,
        .rtvFormat        = EPixelFormat::R8G8B8A8_UNORM,
        .dsvFormat        = EPixelFormat::Unknown,
        .cullMode         = ECullMode::None,
        .blendMode        = EBlendMode::AlphaBlend,
        .depthEnable      = false,
        .bindingLayout    = bindingLayout.get()
    };
    pipelineState = device->CreatePipelineState(psoDesc);

    // 동적 정점 버퍼
    vertexBuffer = device->CreateBuffer({
        .size   = MAX_GLYPH_VERTS * sizeof(GlyphVertex),
        .usage  = EBufferUsage::VertexBuffer,
        .access = EMemoryAccess::Upload,
        .stride = sizeof(GlyphVertex)
    });

    // 화면 크기 상수 버퍼
    screenSizeBuffer = device->CreateBuffer({
        .size   = 16,   // float2 + padding = 16 bytes
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });
}

static constexpr int PANEL_LEFT    = 10;
static constexpr int PANEL_TOP     = 15;
static constexpr int PANEL_PADDING = 4;

void DebugTextRenderer::Render(ICommandList* cmdList, IRenderDevice* device,
                                const std::vector<DebugTextEntry>& entries,
                                const std::map<uint32_t, DebugPanelEntry>& panelEntries,
                                uint32_t screenW, uint32_t screenH)
{
    if (entries.empty() && panelEntries.empty()) return;

    // 새 글리프가 추가된 경우 Atlas 텍스처 재업로드
    glyphAtlas->FlushIfDirty(device);

    // 쿼드 정점 빌드 (패널 먼저 → 자유 위치 나중, 겹칠 때 자유 위치가 위)
    std::vector<GlyphVertex> verts;
    verts.reserve((panelEntries.size() + entries.size()) * 16 * 6);

    auto buildGlyphs = [&](const std::string& text, float startX, float startY, Vector4 color)
    {
        auto codepoints = GlyphAtlas::ParseUTF8(text);
        float penX = startX;
        float penY = startY;
        const float r = color.x, g = color.y, b = color.z, a = color.w;

        for (uint32_t cp : codepoints)
        {
            if (cp == '\n')
            {
                penX  = startX;
                penY += static_cast<float>(glyphAtlas->GetLineHeight());
                continue;
            }

            const GlyphInfo* gl = glyphAtlas->GetGlyph(cp);
            if (!gl) continue;

            if (gl->width > 0 && gl->height > 0)
            {
                float x0 = penX + gl->bearingX;
                float y0 = penY + gl->bearingY;
                float x1 = x0  + gl->width;
                float y1 = y0  + gl->height;

                verts.push_back({ x0, y0, gl->u0, gl->v0, r, g, b, a });
                verts.push_back({ x1, y0, gl->u1, gl->v0, r, g, b, a });
                verts.push_back({ x0, y1, gl->u0, gl->v1, r, g, b, a });
                verts.push_back({ x1, y0, gl->u1, gl->v0, r, g, b, a });
                verts.push_back({ x1, y1, gl->u1, gl->v1, r, g, b, a });
                verts.push_back({ x0, y1, gl->u0, gl->v1, r, g, b, a });

                if (verts.size() >= MAX_GLYPH_VERTS) return;
            }

            penX += gl->advance;
        }
    };

    // 1. 패널 (key 오름차순, 좌상단 고정 레이아웃)
    {
        const int lineStep = glyphAtlas->GetLineHeight() + PANEL_PADDING;
        int rowIndex = 0;
        for (const auto& [key, entry] : panelEntries)
        {
            float y = static_cast<float>(PANEL_TOP + rowIndex * lineStep);
            buildGlyphs(entry.text, static_cast<float>(PANEL_LEFT), y, entry.color);
            ++rowIndex;
        }
    }

    // 2. 자유 위치 (패널 위에 렌더링)
    for (const auto& entry : entries)
        buildGlyphs(entry.text, entry.position.x, entry.position.y, entry.color);

    if (verts.empty()) return;

    // 화면 크기 CB 업로드
    struct ScreenData { float w, h, pad[2]; };
    ScreenData sd = { static_cast<float>(screenW), static_cast<float>(screenH), {0, 0} };
    screenSizeBuffer->Upload(&sd, sizeof(sd));

    // 정점 버퍼 업로드
    uint32_t vertCount = static_cast<uint32_t>(std::min(verts.size(), static_cast<size_t>(MAX_GLYPH_VERTS)));
    vertexBuffer->Upload(verts.data(), vertCount * sizeof(GlyphVertex));

    // 렌더링
    pipelineState->Bind(cmdList);
    cmdList->SetConstantBuffer(screenSizeBuffer.get(), 0);         // root param 0 → b0
    glyphAtlas->GetTexture()->Bind(cmdList, 1);                    // root param 1 → t0
    cmdList->SetPrimitiveTopology(EPrimitiveTopology::TriangleList);
    cmdList->SetVertexBuffer(vertexBuffer.get(), sizeof(GlyphVertex), vertCount * sizeof(GlyphVertex));
    cmdList->DrawInstanced(vertCount, 1);
}
