#pragma once

#include "RHI/Common_RHI.h"

#include <vector>

class ICommandList;
class RenderScene;
class IRHIResource;

struct PassResourceDecl
{
	IRHIResource*  resource;
	EResourceState requiredState;
};

class IRenderPass
{
public:
	virtual ~IRenderPass() = default;

	// Compile() 시점에 트랜지언트 리소스 생성 직후 호출 — 패스가 필요한 트랜지언트 포인터를 확보
	virtual void OnCompile(class RenderGraph& graph) {}

	// Compile() 시점에 호출 — 이 패스가 읽고 쓰는 리소스와 필요 상태 선언
	virtual void DeclareResources(std::vector<PassResourceDecl>& reads,
	                              std::vector<PassResourceDecl>& writes) const {}

	virtual void Execute(ICommandList* cmdList, const RenderScene& scene) = 0;
};
