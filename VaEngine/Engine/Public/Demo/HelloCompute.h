#pragma once

class IRenderDevice;

namespace HelloCompute
{
	// Step 2 — RHI Compute 인프라 end-to-end 검증.
	//
	// HelloCompute_CS.cso 를 실행해 Output[i] = Input[i] * Input[i] 계산.
	// 입력  : { 0, 1, 2, ..., 63 }
	// 기대값: { 0, 1, 4, 9, ..., 3969 }
	// 결과  : VA_LOG + 매 프레임 VA_DRAW_PANEL(99) 에 PASS / FAIL 표시.
	//
	// 검증 통과 후 Run + RenderResult 호출 양쪽 모두 제거하면 정리 끝.
	void Run(IRenderDevice* device);

	// Run() 결과를 매 프레임 VA_DRAW_PANEL로 재출력. Execute::OnUpdate에서 호출.
	// VA_DRAW_PANEL은 매 프레임 Clear되므로 일회성 호출로는 화면에 남지 않음.
	void RenderResult();
}
