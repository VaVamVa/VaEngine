#pragma once

#include "Render/IRenderPass.h"
#include "RHI/BaseRHIResource.h"

// 스왑체인 백버퍼를 Present 상태로 전환하는 것 자체를 그래프의 마지막 Pass로 등록한다.
// 실제 배리어는 RenderGraph::Compile()이 다른 Pass와 완전히 동일한 경로로 계산·삽입하므로,
// 호출부(Execute.cpp)가 "이 리소스가 그래프 관리 대상인지" 따로 판단할 필요가 없다.
class PresentTransitionPass : public IRenderPass
{
public:
	explicit PresentTransitionPass(BaseRHIResource* backBuffer) : backBuffer(backBuffer) {}

	void DeclareResources(std::vector<PassResourceDecl>& reads,
	                       std::vector<PassResourceDecl>& writes) const override
	{
		writes.push_back({ backBuffer, EResourceState::Present });
	}

	void Execute(ICommandList*, const RenderScene&) override {}  // 배리어는 이미 preBarriers로 처리됨

private:
	BaseRHIResource* backBuffer;
};
