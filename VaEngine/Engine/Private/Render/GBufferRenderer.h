#pragma once

#include "RHI/IRenderDevice.h"
#include "RHI/Buffer/IColorBuffer.h"
#include "RHI/Buffer/IDepthBuffer.h"
#include "RHI/Buffer/IBuffer.h"
#include "RHI/Texture/ITextureUAV.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"

#include <memory>
#include <cstdint>
#include <vector>

class RenderGraph;
class RenderScene;
class ICommandList;
class SkinnedMesh;

class GBufferRenderer
{
public:
    // sharedDepth: SceneRenderer 소유, GBufferRenderer는 포인터만 수령 (§1-1.C)
    void Initialize(IRenderDevice* device, IDepthBuffer* sharedDepth,
                    const ShaderDesc& shaderDesc,
                    uint32_t width, uint32_t height);

    void InitializeSkinned(IRenderDevice* device, const ShaderDesc& shaderDesc);

    void AddPasses(RenderGraph& graph, const std::vector<SkinnedMesh*>& skinnedMeshes);

    // GBufferPass::Execute 에서 호출
    void RenderGBuffer(ICommandList* cmdList, const RenderScene& scene,
                       const std::vector<SkinnedMesh*>& skinnedMeshes);

    IColorBuffer* GetRT0()    const { return gbRT0.get(); }
    IColorBuffer* GetRT1()    const { return gbRT1.get(); }
    IColorBuffer* GetRT2()    const { return gbRT2.get(); }
    ITextureUAV*  GetHdrOut() const { return hdrOut.get(); }
    IDepthBuffer* GetDepth()  const { return depth; }
    uint32_t      GetWidth()  const { return width; }
    uint32_t      GetHeight() const { return height; }

private:
    // G-Buffer RT
    std::unique_ptr<IColorBuffer> gbRT0;   // Albedo(RGB) + AO(A)         — R8G8B8A8_UNORM
    std::unique_ptr<IColorBuffer> gbRT1;   // WorldNormal(XYZ) + Rough(W) — R16G16B16A16_FLOAT
    std::unique_ptr<IColorBuffer> gbRT2;   // Metallic(R) + 예약(GBA)    — R8G8B8A8_UNORM
    std::unique_ptr<ITextureUAV>  hdrOut;  // DeferredLighting 출력 + 후속 RT — R16G16B16A16_FLOAT

    IDepthBuffer* depth = nullptr;         // 소유권 없음 — SceneRenderer 소유

    // Static mesh PSO
    std::unique_ptr<IBindingLayout> bindingLayout;
    std::unique_ptr<IShader>        shader;
    std::unique_ptr<IPipelineState> pipelineState;

    // Skinned mesh PSO
    std::unique_ptr<IBindingLayout> skinnedBindingLayout;
    std::unique_ptr<IShader>        skinnedShader;
    std::unique_ptr<IPipelineState> skinnedPipelineState;

    // GPU 버퍼
    std::unique_ptr<IBuffer>   viewProjBuffer;        // b0: ViewProj
    std::unique_ptr<IBuffer>   materialBuffer;        // b1: GBufferMaterial (roughness/metallic)
    std::unique_ptr<IBuffer>   instanceBuffer;        // slot 1: per-instance world (static)
    std::unique_ptr<IBuffer>   skinnedInstanceBuffer; // slot 1: per-instance world (skinned)
    std::unique_ptr<ITexture>  defaultTexture;        // albedo 폴백 (1×1 white)

    uint32_t width  = 0;
    uint32_t height = 0;
};
