#pragma once

#include "Render/IRenderPass.h"
#include "RHI/Common_RHI.h"
#include "RHI/IDepthBuffer.h"

#include <vector>
#include <memory>
#include <unordered_map>

class ICommandList;
class IRenderDevice;
class RenderScene;
class IRHIResource;

// 트랜지언트 깊이 버퍼 선언 — 그래프가 생성/캐시 관리
struct TransientDepthDesc
{
	uint32_t     width;
	uint32_t     height;
	EPixelFormat format;
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

	// 그래프 외부에서 관리되는 리소스의 초기 상태 등록 (스왑체인 백버퍼 등)
	void ImportResource(IRHIResource* resource, EResourceState initialState);

	// 트랜지언트 깊이 버퍼 선언 — 동일 desc가 이미 있으면 기존 핸들 반환 (프레임 간 재사용)
	uint32_t      DeclareTransientDepth(const TransientDepthDesc& desc);
	IDepthBuffer* GetTransientDepth(uint32_t handle) const;

	// 패스/상태 초기화 (트랜지언트 리소스는 유지)
	void Reset();

	// 트랜지언트 리소스 생성 → OnCompile 호출 → 배리어 사전 계산
	void Compile(IRenderDevice* device);

	// 패스 순서대로: 배리어 삽입 → Execute
	void Execute(ICommandList* cmdList, const RenderScene& scene);

	// Compile/Execute 후 리소스의 현재 추적 상태 조회
	EResourceState GetCurrentState(IRHIResource* resource) const;

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

	std::vector<PassEntry>                            entries;
	std::unordered_map<IRHIResource*, EResourceState> resourceStates;
	std::vector<TransientDepthEntry>                  transientDepths;
};
