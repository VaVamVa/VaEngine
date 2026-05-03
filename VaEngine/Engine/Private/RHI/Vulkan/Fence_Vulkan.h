#pragma once

#include "Interfaces/IFence.h"
#include "Common_Vulkan.h"

#include <cstdint>

// ============================================================
// Fence_Vulkan
//
// DX12의 Fence_DirectX(ID3D12Fence)와 대응합니다.
//
// DX12              │ Vulkan
// ──────────────────────────────────────────────────────
// ID3D12Fence       │ VkFence        — CPU-GPU 동기화
// fence->Signal(v)  │ (큐 제출 시 vkQueueSubmit의 fence 인자)
// WaitForSingleObj  │ vkWaitForFences
//
// Vulkan 동기화 원칙:
//   DX12의 ID3D12Fence는 타임라인 값 기반으로 단조 증가합니다.
//   Vulkan VkFence는 Binary 상태(신호됨/아님)입니다.
//   IFence 인터페이스의 'value' 매개변수는 Vulkan에서 내부 카운터로 시뮬레이션합니다.
//
// 참고: SwapChain_Vulkan이 자체 inFlightFence를 내부적으로 관리합니다.
// IFence는 Execute에서 명시적 GPU-CPU 동기화가 필요할 때 사용합니다.
// ============================================================
class Fence_Vulkan : public IFence
{
public:
    ~Fence_Vulkan() override;

    void     Register(IRenderDevice* inDevice) override;
    void     SetEventOnComplete(uint64_t value, void* inEventHandle) override;
    uint64_t GetCompletedValue() const override;

    // Vulkan 내부에서 직접 접근할 때 사용 (CommandQueue_Vulkan 등)
    VkFence GetHandle() const { return fence; }

    // 완료된 것으로 표시 (vkQueueSubmit 후 내부적으로 카운터 갱신용)
    void MarkCompleted(uint64_t value);

private:
    // VkFence: CPU가 GPU 작업 완료를 대기하는 동기화 객체.
    // 처음에 신호 상태(VK_FENCE_CREATE_SIGNALED_BIT)로 생성됩니다.
    VkFence fence = VK_NULL_HANDLE;

    // Device 핸들 (소유권 없음)
    VkDevice deviceHandle = VK_NULL_HANDLE;

    // DX12의 타임라인 값(GetCompletedValue)을 시뮬레이션하는 내부 카운터.
    // VkFence가 신호되면 이 값을 증가시킵니다.
    uint64_t completedValue = 0;
};
