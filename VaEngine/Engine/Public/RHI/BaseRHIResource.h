#pragma once

#include "RHI/Common_RHI.h"

class RenderGraph;  // friend 선언용 전방 선언 — RHI가 Render 계층을 #include하지 않음(계층 방향 유지)

struct BaseRHIResource
{
public:
	virtual ~BaseRHIResource() = default;

	virtual void* GetNativeResource() const = 0;

	// 현재 추적 상태 조회 — 제약 없음(디버그 패널 등에서 자유롭게 조회 가능)
	EResourceState GetTrackedState() const { return trackedState; }

protected:
	// RenderGraph::Compile() 안에서만 호출할 것(barrier 계획 단계). 각 구현체의 Create()에서
	// 실제 초기 상태를 설정할 때도 파생 클래스 자격으로 사용(protected이므로 상속 계층 전체에서 접근 가능).
	void SetTrackedState(EResourceState state) { trackedState = state; }

private:
	friend class RenderGraph;  // Compile()의 배리어 계산에서 SetTrackedState() 호출 허용
	EResourceState trackedState = EResourceState::Uninitialized;  // 각 구현체 Create()가 실제 상태로 갱신 필수
};
