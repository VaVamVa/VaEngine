#include "Render/RenderGraph.h"
#include "Scene/RenderScene.h"
#include "RHI/ICommandList.h"
#include "RHI/IRenderDevice.h"
#include "RHI/BaseRHIResource.h"
#include "Utilities/DebuggingHelper.h"
#include <cassert>
#include <format>

#if VA_DEBUG
namespace
{
	const char* ResourceStateToString(EResourceState state)
	{
		switch (state)
		{
		case EResourceState::Common:                  return "Common";
		case EResourceState::VertexBuffer:            return "VertexBuffer";
		case EResourceState::IndexBuffer:             return "IndexBuffer";
		case EResourceState::RenderTarget:            return "RenderTarget";
		case EResourceState::UnorderedAccess:         return "UnorderedAccess";
		case EResourceState::DepthWrite:              return "DepthWrite";
		case EResourceState::DepthRead:               return "DepthRead";
		case EResourceState::NonPixelShaderResource:  return "NonPixelShaderResource";
		case EResourceState::PixelShaderResource:     return "PixelShaderResource";
		case EResourceState::StreamOut:               return "StreamOut";
		case EResourceState::IndirectArgument:        return "IndirectArgument";
		case EResourceState::CopyDest:                return "CopyDest";
		case EResourceState::CopySource:              return "CopySource";
		case EResourceState::ResolveDest:             return "ResolveDest";
		case EResourceState::ResolveSource:           return "ResolveSource";
		case EResourceState::RaytracingAcceleration:  return "RaytracingAcceleration";
		case EResourceState::Present:                 return "Present";
		case EResourceState::Uninitialized:           return "Uninitialized";
		default:                                      return "Unknown";
		}
	}
}

// BaseRHIResource의 모든 상태 변경이 Compile() 이 한 곳을 거친다는 점을 이용해 전수 기록한다.
// 처음 N프레임만 기록하고 멈추되, 조용해진 뒤 (리소스, 이전상태, 이후상태) 조합을 한 번도
// 못 봤다면(신규 리소스·리사이즈·예상 밖 전환) 그 순간부터 다시 N프레임 재개한다.
// 이 함수 자체가 VA_DEBUG로 감싸져 있으므로, 꺼두면 멤버(seenTransitions/traceFrameCount)와
// 호출부(Compile())까지 전부 컴파일에서 빠져 런타임 비용이 완전히 사라진다.
void RenderGraph::TraceResourceTransition(BaseRHIResource* resource, EResourceState before, EResourceState after)
{
	const bool firstTimeSeen = seenTransitions.insert({ resource, before, after }).second;
	if (firstTimeSeen && traceFrameCount > kTraceResourceStateFrames)
	{
		VA_LOG("ResourceState", std::format(
			"--- new transition pattern [{:p}] {} -> {}, resuming trace for {} frames ---",
			static_cast<void*>(resource), ResourceStateToString(before), ResourceStateToString(after),
			kTraceResourceStateFrames));
		traceFrameCount = 1;
	}

	if (traceFrameCount <= kTraceResourceStateFrames)
	{
		VA_LOG("ResourceState", std::format("[{:p}] {} -> {}",
			static_cast<void*>(resource), ResourceStateToString(before), ResourceStateToString(after)));
	}
}
#endif

uint32_t RenderGraph::DeclareTransientDepth(const TransientDepthDesc& desc)
{
	for (uint32_t i = 0; i < static_cast<uint32_t>(transientDepths.size()); ++i)
	{
		const auto& td = transientDepths[i];
		if (td.desc.width == desc.width && td.desc.height == desc.height && td.desc.format == desc.format
		    && td.desc.arraySize == desc.arraySize)
			return i;
	}
	transientDepths.push_back({ desc, nullptr });
	return static_cast<uint32_t>(transientDepths.size() - 1);
}

IDepthBuffer* RenderGraph::GetTransientDepth(uint32_t handle) const
{
	return handle < static_cast<uint32_t>(transientDepths.size())
		? transientDepths[handle].resource.get()
		: nullptr;
}

void RenderGraph::Reset()
{
	entries.clear();
	// transientDepths는 유지 — 다음 프레임에 동일 desc면 GPU 리소스 재사용.
	// trackedState도 리소스 자신(BaseRHIResource)이 들고 있으므로 별도로 시드/복원할 것이 없다.
}

void RenderGraph::Compile(IRenderDevice* device)
{
	uint32_t totalBarriers = 0;
#if VA_DEBUG
	++traceFrameCount;
#endif

	// 1. 미생성 트랜지언트 리소스 생성 (이미 있으면 재사용). 생성 시점에 DepthBuffer_DirectX::Create()가
	//    trackedState를 DepthWrite로 설정하므로(IDepthBuffer가 BaseRHIResource를 직접 상속), 여기서
	//    별도로 상태를 시드할 필요가 없다.
	for (auto& td : transientDepths)
	{
		if (!td.resource)
		{
			td.resource = device->CreateDepthBuffer(td.desc.width, td.desc.height, td.desc.format, td.desc.arraySize);
			VA_LOG("RenderGraph", std::format("Transient Depth Created: {}x{}", td.desc.width, td.desc.height));
		}
	}

	// 2. 패스에 트랜지언트 포인터 전달
	for (PassEntry& entry : entries)
		entry.pass->OnCompile(*this);

	// 3. 배리어 사전 계산 — 각 리소스가 스스로 들고 있는 trackedState를 직접 읽고 갱신한다.
	//    (등록/맵 조회 없음 — 리소스 포인터 자체가 곧 상태의 위치)
	for (PassEntry& entry : entries)
	{
		std::vector<PassResourceDecl> reads, writes;
		entry.pass->DeclareResources(reads, writes);

		auto transition = [&](const PassResourceDecl& decl)
		{
			const EResourceState current = decl.resource->GetTrackedState();
			// 백엔드 무관 안전장치: 어떤 구현체가 Create()에서 초기 상태 설정을 빠뜨리면
			// GPU에 커맨드가 올라가기 전, 여기서 즉시 실패해 원인을 드러낸다.
			assert(current != EResourceState::Uninitialized &&
			       "BaseRHIResource: Create()가 SetTrackedState()로 실제 초기 상태를 설정하지 않음");

			if (current != decl.requiredState)
			{
				entry.preBarriers.push_back({ decl.resource, current, decl.requiredState });
#if VA_DEBUG
				TraceResourceTransition(decl.resource, current, decl.requiredState);
#endif
				decl.resource->SetTrackedState(decl.requiredState);  // protected + friend RenderGraph
				totalBarriers++;
			}
		};

		for (const PassResourceDecl& d : reads)  transition(d);
		for (const PassResourceDecl& d : writes) transition(d);
	}

	VA_DRAW_PANEL(2, std::format("RenderGraph: {} Passes, {} Barriers", entries.size(), totalBarriers));
}

void RenderGraph::Execute(ICommandList* cmdList, const RenderScene& scene)
{
	for (PassEntry& entry : entries)
	{
		if (!entry.preBarriers.empty())
		{
			cmdList->SetResourceBarrier(
				static_cast<uint32_t>(entry.preBarriers.size()),
				entry.preBarriers.data()
			);
		}
		entry.pass->Execute(cmdList, scene);
	}
}
