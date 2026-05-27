#pragma once

#include "Render/IRenderer.h"
#include "Render/IMaterial.h"
#include "Render/DebugTextRenderer.h"
#include "Render/DebugLineRenderer.h"

#include "RHI/Buffer/IBuffer.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"

#include <memory>
class RenderScene;

class ForwardRenderer : public IRenderer
{
public:
	void Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc) override;
	void InitializeSky(IRenderDevice* device, const ShaderDesc& skyShaderDesc);
	void InitializeDebugText(IRenderDevice* device, const ShaderDesc& glyphShaderDesc, const char* ttfPath);
	void InitializeDebugLines(IRenderDevice* device, const ShaderDesc& lineShaderDesc);
	void AddPasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& scene) override;
	void AddOpaquePasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& scene);
	void AddTransparentPasses(RenderGraph& graph, const FrameOutput& output);
	void AddDebugLinePasses(RenderGraph& graph, const FrameOutput& output);
	void AddDebugTextPasses(RenderGraph& graph, const FrameOutput& output);
	void Render(ICommandList* cmdList, const RenderScene& scene, bool isTransparentPass);
	void RenderSky(ICommandList* cmdList, const RenderScene& scene);
	void RenderDebugLines(ICommandList* cmdList, const RenderScene& scene);
	void RenderDebugText(ICommandList* cmdList, const RenderScene& scene, uint32_t screenW, uint32_t screenH);

	IMaterial* GetMaterial() const { return material.get(); }

private:
	// Forward pass GPU resources
	std::unique_ptr<IBindingLayout> bindingLayout;
	std::unique_ptr<IShader>        shader;
	std::unique_ptr<IPipelineState> pipelineState;
	std::unique_ptr<IBuffer>        viewProjBuffer;  // b0: view * proj (per-frame)
	std::unique_ptr<IBuffer>        lightsBuffer;    // b2: lights + material + eyePos
	std::unique_ptr<IBuffer>        instanceBuffer;  // slot 1: per-instance world matrices
	std::unique_ptr<ITexture>       texture;

	std::unique_ptr<IMaterial>      material;

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
