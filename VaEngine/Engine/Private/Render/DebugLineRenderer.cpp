#include "DebugLineRenderer.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Common_RHI.h"

#include <algorithm>
#include <cstring>

void DebugLineRenderer::Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc)
{
    BindingEntry bindings[] = {
        { EBindingType::ConstantBuffer, 0, EShaderStage::Vertex },
    };
    bindingLayout = device->CreateBindingLayout(bindings, 1);

    shader = device->CreateShader(shaderDesc);

    VertexInputDesc inputs[] = {
        { "POSITION", 0, EPixelFormat::R32G32B32_FLOAT,    0,  0, false },
        { "COLOR",    0, EPixelFormat::R32G32B32A32_FLOAT, 12, 0, false },
    };

    PipelineStateDesc psoDesc = {
        .shader           = shader.get(),
        .vertexInputs     = inputs,
        .vertexInputCount = 2,
        .rtvFormats       = { EPixelFormat::R8G8B8A8_UNORM },
        .dsvFormat        = EPixelFormat::Unknown,
        .cullMode         = ECullMode::None,
        .blendMode        = EBlendMode::AlphaBlend,
        .depthEnable      = false,
        .topologyType     = EPrimitiveTopologyType::Line,
        .bindingLayout    = bindingLayout.get()
    };
    pipelineState = device->CreatePipelineState(psoDesc);

    PipelineStateDesc psoDepthDesc = {
        .shader           = shader.get(),
        .vertexInputs     = inputs,
        .vertexInputCount = 2,
        .rtvFormats       = { EPixelFormat::R8G8B8A8_UNORM },
        .dsvFormat        = EPixelFormat::D24_UNORM_S8_UINT,
        .cullMode         = ECullMode::None,
        .blendMode        = EBlendMode::AlphaBlend,
        .depthEnable      = true,
        .topologyType     = EPrimitiveTopologyType::Line,
        .bindingLayout    = bindingLayout.get()
    };
    pipelineStateDepth = device->CreatePipelineState(psoDepthDesc);

    vertexBuffer = device->CreateBuffer({
        .size   = MAX_LINE_VERTS * sizeof(LineVertex),
        .usage  = EBufferUsage::VertexBuffer,
        .access = EMemoryAccess::Upload,
        .stride = sizeof(LineVertex)
    });

    vertexBufferDepth = device->CreateBuffer({
        .size   = MAX_LINE_VERTS * sizeof(LineVertex),
        .usage  = EBufferUsage::VertexBuffer,
        .access = EMemoryAccess::Upload,
        .stride = sizeof(LineVertex)
    });

    viewProjBuffer = device->CreateBuffer({
        .size   = sizeof(float) * 16,
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });
}

void DebugLineRenderer::Render(ICommandList* cmdList,
                                const std::vector<DebugLineEntry>& lines,
                                const Matrix4x4& viewProj)
{
    if (lines.empty()) return;

    std::vector<LineVertex> depthVerts, noDepthVerts;
    depthVerts.reserve(lines.size() * 2);
    noDepthVerts.reserve(lines.size() * 2);

    for (const auto& line : lines)
    {
        LineVertex a = { line.start.x, line.start.y, line.start.z,
                         line.color.x, line.color.y, line.color.z, line.color.w };
        LineVertex b = { line.end.x,   line.end.y,   line.end.z,
                         line.color.x, line.color.y, line.color.z, line.color.w };
        auto& target = line.depthTest ? depthVerts : noDepthVerts;
        if (target.size() < MAX_LINE_VERTS) { target.push_back(a); target.push_back(b); }
    }

    viewProjBuffer->Upload(viewProj.m, sizeof(float) * 16);

    if (!depthVerts.empty())
    {
        const uint32_t count = static_cast<uint32_t>(depthVerts.size());
        vertexBufferDepth->Upload(depthVerts.data(), count * sizeof(LineVertex));
        pipelineStateDepth->Bind(cmdList);
        cmdList->SetConstantBuffer(viewProjBuffer.get(), 0);
        cmdList->SetPrimitiveTopology(EPrimitiveTopology::LineList);
        cmdList->SetVertexBuffer(vertexBufferDepth.get(), sizeof(LineVertex), count * sizeof(LineVertex));
        cmdList->DrawInstanced(count, 1);
    }

    if (!noDepthVerts.empty())
    {
        const uint32_t count = static_cast<uint32_t>(noDepthVerts.size());
        vertexBuffer->Upload(noDepthVerts.data(), count * sizeof(LineVertex));
        pipelineState->Bind(cmdList);
        cmdList->SetConstantBuffer(viewProjBuffer.get(), 0);
        cmdList->SetPrimitiveTopology(EPrimitiveTopology::LineList);
        cmdList->SetVertexBuffer(vertexBuffer.get(), sizeof(LineVertex), count * sizeof(LineVertex));
        cmdList->DrawInstanced(count, 1);
    }
}
