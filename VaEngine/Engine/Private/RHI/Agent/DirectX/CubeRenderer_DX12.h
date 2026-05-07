#pragma once

#include "RHI/Agent/CubeRenderer.h"
#include "RHI/Buffer/IConstantBuffer.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"
#include "Mesh/IMesh.h"
#include <chrono>

class CubeRenderer_DX12 : public CubeRenderer
{
public:
	CubeRenderer_DX12() = default;
	virtual ~CubeRenderer_DX12() = default;

	void Initialize(IRenderDevice* device) override;
	void Draw(ICommandList* cmdList) override;

private:
	std::unique_ptr<IBindingLayout>  bindingLayout;
	std::unique_ptr<IPipelineState>  pipelineState;
	std::unique_ptr<IConstantBuffer> constantBuffer;
	std::unique_ptr<ITexture>        texture;
	std::unique_ptr<IMesh>           mesh;

	std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};
