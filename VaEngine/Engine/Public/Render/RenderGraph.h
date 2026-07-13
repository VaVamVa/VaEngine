#pragma once

#include "Render/IRenderPass.h"
#include "RHI/Common_RHI.h"
#include "RHI/Buffer/IDepthBuffer.h"
#include "Utilities/DebuggingHelper.h"  // VA_DEBUG — 아래 리소스 상태 추적 기능 전체를 이 값으로 컴파일 타임에 껐다 켰다 함

#include <vector>
#include <memory>
#if VA_DEBUG
#include <set>
#include <tuple>
#endif

class ICommandList;
class IRenderDevice;
class RenderScene;
class BaseRHIResource;

// 트랜지언트 깊이 버퍼 선언 — 그래프가 생성/캐시 관리
struct TransientDepthDesc
{
	uint32_t     width;
	uint32_t     height;
	EPixelFormat format;
	uint32_t     arraySize = 1;  // >1: CSM 등 슬라이스별 DSV가 필요한 depth 배열
};

// 프레임별 출력 대상 정보 (스왑체인 백버퍼 등) — Execute.cpp 에서 RenderScene과 함께 RenderGraph::Execute에 전달
struct FrameOutput
{
	BaseRHIResource* backBuffer = nullptr;
	IResourceView* backBufferView = nullptr;
	float          clearColor[4] = {};
	uint32_t       width = 1280;
	uint32_t       height = 720;
};


class RenderGraph
{
public:
	template<typename TPass, typename... Args>
	void AddPass(Args&&... args)
	{
		PassEntry entry;
		entry.pass = std::make_unique<TPass>(std::forward<Args>(args)...);
		entries.push_back(std::move(entry));
	}

	// 트랜지언트 깊이 버퍼 선언 — 동일 desc가 이미 있으면 기존 핸들 반환 (프레임 간 재사용)
	uint32_t      DeclareTransientDepth(const TransientDepthDesc& desc);
	IDepthBuffer* GetTransientDepth(uint32_t handle) const;

	// 패스 목록 초기화 (트랜지언트 리소스는 유지 — 리소스 자신의 trackedState도 그대로 보존됨)
	void Reset();

	// 트랜지언트 리소스 생성 → OnCompile 호출 → 배리어 사전 계산 (각 리소스의 trackedState를 직접 읽고 갱신)
	void Compile(IRenderDevice* device);

	// 패스 순서대로: 배리어 삽입 → Execute
	void Execute(ICommandList* cmdList, const RenderScene& scene);

private:
	struct PassEntry
	{
		std::unique_ptr<IRenderPass> pass;
		std::vector<ResourceBarrier> preBarriers;
	};

	struct TransientDepthEntry
	{
		TransientDepthDesc            desc;
		std::unique_ptr<IDepthBuffer> resource;
	};

	std::vector<PassEntry>           entries;
	std::vector<TransientDepthEntry> transientDepths;

#if VA_DEBUG
	// ─────────────────────────────────────────────────────────────────────
	// 리소스 상태 전환 추적(검증용) — VA_DEBUG=0이면 멤버·함수·호출부가 전부 컴파일에서 빠진다.
	// (VA_LOG 한 줄만 감싸는 걸로는 부족함 — 아래 부기 로직 자체가 매 배리어마다 도는 실비용이라
	// Compile()에서 이 함수를 부르는 지점까지 통째로 막아야 한다.)
	//
	// 처음 N프레임만 전수 기록하고 멈춘다 — BonePalette 첫 프레임 전환·Shadow Map/백버퍼의 프레임 간
	// 반복 패턴은 몇 프레임만 봐도 전부 드러나므로 무제한 기록으로 로그가 비대해지는 것을 막는다.
	// 단, 조용해진 뒤에도 (리소스, 이전상태, 이후상태) 조합을 한 번도 못 봤다면(신규 리소스·리사이즈·
	// 예상 밖 전환 등) 그 순간부터 다시 N프레임 재기록한다.
	// ─────────────────────────────────────────────────────────────────────
	static constexpr uint32_t kTraceResourceStateFrames = 5;
	uint32_t traceFrameCount = 0;
	std::set<std::tuple<BaseRHIResource*, EResourceState, EResourceState>> seenTransitions;

	// 실제 배리어가 삽입되는 순간(=상태가 바뀌는 순간)에만 Compile()에서 호출.
	void TraceResourceTransition(BaseRHIResource* resource, EResourceState before, EResourceState after);
#endif
};
