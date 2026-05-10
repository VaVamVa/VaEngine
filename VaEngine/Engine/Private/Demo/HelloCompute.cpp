#include "Demo/HelloCompute.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/ICommandQueue.h"
#include "RHI/ICommandAlloc.h"
#include "RHI/IFence.h"
#include "RHI/IBuffer.h"
#include "RHI/IResourceView.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"
#include "RHI/Pipeline/ComputePipelineDesc.h"

#include "Utilities/DebuggingHelper.h"

#include <cstdint>
#include <cstring>
#include <vector>
#include <format>
#include <stdexcept>

namespace HelloCompute
{

namespace {
constexpr uint32_t kElementCount = 64;
constexpr uint32_t kElementSize  = sizeof(uint32_t);
constexpr uint32_t kBufferBytes  = kElementCount * kElementSize;

// Run() 결과 캐시 — RenderResult()가 매 프레임 재출력
std::string gResultText  = "HelloCompute: not run";
Vector4     gResultColor = { 0.7f, 0.7f, 0.7f, 1.0f };
bool        gHasResult   = false;
}

void Run(IRenderDevice* device)
{
	if (!device)
	{
		VA_LOG("HelloCompute", "device is null — skip");
		return;
	}

	VA_LOG("HelloCompute", "begin Step 2 validation");

	try
	{
		// ── Compute shader ───────────────────────────────────────────────
		ShaderDesc csDesc{};
		csDesc.csPath  = SHADER_DIR L"/HelloCompute_CS.cso";
		csDesc.csEntry = "CSMain";
		auto csShader = device->CreateShader(csDesc);

		// ── Binding layout (compute root signature) ──────────────────────
		// 루트 파라미터 [0] = t0 (BufferSRV : Input)
		// 루트 파라미터 [1] = u0 (UAV       : Output)
		BindingEntry entries[] = {
			{ EBindingType::BufferSRV, 0, EShaderStage::Compute },
			{ EBindingType::UAV,       0, EShaderStage::Compute },
		};
		auto bindingLayout = device->CreateBindingLayout(entries, 2, /*isCompute*/ true);

		// ── Compute PSO ──────────────────────────────────────────────────
		ComputePipelineStateDesc psoDesc{};
		psoDesc.shader        = csShader.get();
		psoDesc.bindingLayout = bindingLayout.get();
		auto pso = device->CreateComputePipelineState(psoDesc);

		// ── Input buffer (upload heap, CPU 작성 → GPU SRV로 읽힘) ────────
		BufferDesc inputDesc{
			.size   = kBufferBytes,
			.usage  = EBufferUsage::None,
			.access = EMemoryAccess::Upload,
			.stride = kElementSize,
		};
		auto inputBuffer = device->CreateBuffer(inputDesc);

		// 입력값 채우기: { 0, 1, 2, ..., 63 }
		{
			uint32_t hostInput[kElementCount];
			for (uint32_t i = 0; i < kElementCount; ++i)
				hostInput[i] = i;
			inputBuffer->Upload(hostInput, kBufferBytes);
		}

		// ── Output buffer (default heap, UAV) ────────────────────────────
		BufferDesc outputDesc{
			.size   = kBufferBytes,
			.usage  = EBufferUsage::UnorderedAccess,
			.access = EMemoryAccess::Default,
			.stride = kElementSize,
		};
		auto outputBuffer = device->CreateBuffer(outputDesc);

		// ── Readback buffer (GPU → CPU 회수용) ───────────────────────────
		BufferDesc readbackDesc{
			.size   = kBufferBytes,
			.usage  = EBufferUsage::None,
			.access = EMemoryAccess::Readback,
			.stride = kElementSize,
		};
		auto readbackBuffer = device->CreateBuffer(readbackDesc);

		// ── 리소스 뷰 ────────────────────────────────────────────────────
		// 현재 SetComputeSRV/UAV는 root descriptor 사용 → view는 리소스 wrapper 역할
		auto inputSRV  = device->CreateBufferSRV(inputBuffer.get(),  kElementCount, kElementSize);
		auto outputUAV = device->CreateBufferUAV(outputBuffer.get(), kElementCount, kElementSize);

		// ── Command 인프라 (일회용) ──────────────────────────────────────
		auto queue = device->CreateCommandQueue({ ECommandQueueType::Graphics, 0 });
		auto alloc = device->CreateCommandAllocator({ ECommandQueueType::Graphics, 0 });
		auto cmds  = device->CreateCommandList({ ECommandQueueType::Graphics, 0 });
		auto fence = device->CreateFence();

		// ── 명령 기록 ────────────────────────────────────────────────────
		cmds->Begin(alloc.get());

		pso->Bind(cmds.get());                       // compute root sig + PSO
		cmds->SetComputeSRV(inputSRV.get(),  0);     // 루트 파라미터 [0] → t0
		cmds->SetComputeUAV(outputUAV.get(), 1);     // 루트 파라미터 [1] → u0
		cmds->Dispatch(1, 1, 1);                     // 1 group × numthreads(64) = 64 thread

		cmds->UAVBarrier(outputBuffer.get());

		// 출력 버퍼: UnorderedAccess → CopySource
		ResourceBarrier toCopySrc{
			outputBuffer.get(),
			EResourceState::UnorderedAccess,
			EResourceState::CopySource
		};
		cmds->SetResourceBarrier(1, &toCopySrc);

		// readback 으로 복사
		cmds->CopyBuffer(readbackBuffer.get(), outputBuffer.get(), kBufferBytes);

		cmds->Close();

		// ── 제출 + GPU 완료 대기 ─────────────────────────────────────────
		std::vector<ICommandList*> lists{ cmds.get() };
		queue->ExecuteCommandLists(1, lists);
		queue->Signal(fence.get(), 1);
		fence->Wait(1);

		// ── 결과 검증 ────────────────────────────────────────────────────
		uint32_t hostOutput[kElementCount] = {};
		{
			void* mapped = readbackBuffer->Map();
			std::memcpy(hostOutput, mapped, kBufferBytes);
			readbackBuffer->Unmap();
		}

		bool     pass         = true;
		uint32_t firstFailIdx = 0;
		uint32_t firstFailGot = 0;
		uint32_t firstFailExp = 0;
		for (uint32_t i = 0; i < kElementCount; ++i)
		{
			const uint32_t expected = i * i;
			if (hostOutput[i] != expected)
			{
				pass         = false;
				firstFailIdx = i;
				firstFailGot = hostOutput[i];
				firstFailExp = expected;
				break;
			}
		}

		if (pass)
		{
			VA_LOG("HelloCompute", "PASS — 64 elements squared correctly on GPU");
			gResultText  = "HelloCompute: PASS";
			gResultColor = { 0.4f, 1.0f, 0.4f, 1.0f };  // green
		}
		else
		{
			VA_LOG("HelloCompute",
				std::format("FAIL — Output[{}]={} (expected {})",
					firstFailIdx, firstFailGot, firstFailExp));
			gResultText = std::format("HelloCompute: FAIL @[{}] got={} exp={}",
				firstFailIdx, firstFailGot, firstFailExp);
			gResultColor = { 1.0f, 0.4f, 0.4f, 1.0f };  // red
		}
		gHasResult = true;
	}
	catch (const std::exception& e)
	{
		VA_LOG("HelloCompute", std::format("EXCEPTION: {}", e.what()));
		gResultText  = std::format("HelloCompute: EXCEPTION {}", e.what());
		gResultColor = { 1.0f, 0.4f, 0.4f, 1.0f };
		gHasResult   = true;
	}
}

void RenderResult()
{
	if (!gHasResult) return;
	VA_DRAW_PANEL(99, gResultText, gResultColor);
}

} // namespace HelloCompute
