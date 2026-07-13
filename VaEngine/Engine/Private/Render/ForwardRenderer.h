#pragma once

#include "Render/IMaterial.h"
#include "Render/DebugTextRenderer.h"
#include "Render/DebugLineRenderer.h"

#include "RHI/Buffer/IBuffer.h"
#include "RHI/Buffer/IColorBuffer.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"

#include <memory>
class IRenderDevice;
class RenderGraph;
struct FrameOutput;
class RenderScene;
class IDepthBuffer;
class ITextureUAV;

class ForwardRenderer
{
public:
	void Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc);
	// width/height: Weighted Blended OIT accum/revealage 렌더타겟 크기(풀해상도)
	void InitializeTransparent(IRenderDevice* device, const ShaderDesc& shaderDesc,
	                           const ShaderDesc& compositeShaderDesc, uint32_t width, uint32_t height);
	void InitializeSky(IRenderDevice* device, const ShaderDesc& skyShaderDesc);
	void InitializeDebugText(IRenderDevice* device, const ShaderDesc& glyphShaderDesc, const char* ttfPath);
	void InitializeDebugLines(IRenderDevice* device, const ShaderDesc& lineShaderDesc);
	void AddOpaquePasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& scene);
	// hdrOut: Deferred 경로에서 Transparent가 hdrOut(RTV)에 그려지도록 재배치됨 (Phase 1-1, Tonemap 순서 보장)
	// irradianceMap/prefilteredMap/brdfLUT/iblEnabled: IBL Stage A/B — DeferredLightingRenderer와 동일 산출물을 공유
	void AddTransparentPasses(RenderGraph& graph, const FrameOutput& output, IDepthBuffer* sharedDepth, ITextureUAV* hdrOut,
	                          ITextureUAV* irradianceMap, ITextureUAV* prefilteredMap, ITextureUAV* brdfLUT, bool iblEnabled);
	void AddDebugLinePasses(RenderGraph& graph, const FrameOutput& output, IDepthBuffer* sharedDepth);
	void AddDebugTextPasses(RenderGraph& graph, const FrameOutput& output);
	void Render(ICommandList* cmdList, const RenderScene& scene);
	// OIT accum 패스 — accum/revealage MRT에 기록(1-Pass, CullMode::None)
	void RenderTransparent(ICommandList* cmdList, const RenderScene& scene,
	                       ITextureUAV* irradianceMap, ITextureUAV* prefilteredMap, ITextureUAV* brdfLUT, bool iblEnabled);
	// OIT 합성 패스 — accum/revealage를 풀어 hdrOut에 AlphaBlend
	void RenderOITComposite(ICommandList* cmdList);
	void RenderSky(ICommandList* cmdList, const RenderScene& scene);
	void RenderDebugLines(ICommandList* cmdList, const RenderScene& scene);
	void RenderDebugText(ICommandList* cmdList, const RenderScene& scene, uint32_t screenW, uint32_t screenH);

	IMaterial* GetMaterial() const { return material.get(); }

private:
	// Opaque pass GPU resources
	std::unique_ptr<IBindingLayout> bindingLayout;
	std::unique_ptr<IShader>        shader;
	std::unique_ptr<IPipelineState> pipelineState;
	std::unique_ptr<IBuffer>        viewProjBuffer;  // b0: view * proj (per-frame)
	std::unique_ptr<IBuffer>        lightsBuffer;    // b2: lights + material + eyePos
	std::unique_ptr<IBuffer>        instanceBuffer;  // slot 1: per-instance world matrices
	std::unique_ptr<ITexture>       texture;
	std::unique_ptr<ITexture>       defaultNormalTexture;  // normal map 폴백 (1×1 tangent-up)
	std::unique_ptr<IMaterial>      material;

	// Transparent — Weighted Blended OIT (1-Pass, CullMode::None)
	std::unique_ptr<IShader>        transparentShader;
	std::unique_ptr<IPipelineState> transparentPSO;   // MRT(accum, revealage), EBlendMode::OITAccumulate
	std::unique_ptr<IColorBuffer>   oitAccum;          // RGBA16F, 풀해상도
	std::unique_ptr<IColorBuffer>   oitRevealage;       // R32_FLOAT, 풀해상도

	// OIT 합성 — accum/revealage → hdrOut (AlphaBlend)
	std::unique_ptr<IBindingLayout> compositeBindingLayout;
	std::unique_ptr<IShader>        compositeShader;
	std::unique_ptr<IPipelineState> compositePSO;

	// Sky pass GPU resources
	std::unique_ptr<IBindingLayout> skyBindingLayout;
	std::unique_ptr<IShader>        skyShader;
	std::unique_ptr<IPipelineState> skyPipelineState;
	std::unique_ptr<IBuffer>        skyDataBuffer;   // b0: CB_SkyData (InvProj + InvViewRot)

	// Debug line pass
	std::unique_ptr<DebugLineRenderer> debugLineRenderer;

	// Debug text pass
	std::unique_ptr<DebugTextRenderer> debugTextRenderer;
	IRenderDevice*                     renderDevice = nullptr;
};
