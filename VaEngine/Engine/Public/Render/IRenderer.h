#pragma once

#include "RHI/Shader/IShader.h"
#include "RHI/Common_RHI.h"

class RenderGraph;
class IRenderDevice;
class IRHIResource;
class IResourceView;
class RenderScene;

// 한 프레임의 출력 대상 정보 — 렌더러가 패스를 구성할 때 사용
struct FrameOutput
{
	IRHIResource*  backBuffer     = nullptr;
	IResourceView* backBufferView = nullptr;
	float          clearColor[4]  = {};
	uint32_t       width          = 1280;
	uint32_t       height         = 720;
};

class IRenderer
{
public:
	virtual ~IRenderer() = default;

	virtual void Initialize(IRenderDevice* device, const ShaderDesc& shaderDesc) = 0;

	// 렌더러가 사용할 패스를 graph에 등록 (실행은 graph.Execute()에서).
	// scene: 패스 등록 시점에 scene 정보(예: SkinnedRenderCommand 목록)가 필요한 렌더러용 — 안 쓰면 무시.
	virtual void AddPasses(RenderGraph& graph, const FrameOutput& output, const RenderScene& scene) = 0;
};
