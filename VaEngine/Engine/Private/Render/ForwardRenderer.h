#pragma once

#include "Render/IRenderer.h"
#include "Render/IMaterial.h"

#include "RHI/IBuffer.h"
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
	void AddPasses(RenderGraph& graph, const FrameOutput& output) override;
	void Render(ICommandList* cmdList, const RenderScene& scene);

	IMaterial* GetMaterial() const { return material.get(); }

private:
	// GPU resources
	std::unique_ptr<IBindingLayout> bindingLayout;
	std::unique_ptr<IShader>        shader;
	std::unique_ptr<IPipelineState> pipelineState;
	std::unique_ptr<IBuffer>        viewProjBuffer;  // b0: view * proj (per-frame)
	std::unique_ptr<IBuffer>        lightsBuffer;    // b2: lights + material + eyePos
	std::unique_ptr<IBuffer>        instanceBuffer;  // slot 1: per-instance world matrices
	std::unique_ptr<ITexture>       texture;

	std::unique_ptr<IMaterial>      material;
};
