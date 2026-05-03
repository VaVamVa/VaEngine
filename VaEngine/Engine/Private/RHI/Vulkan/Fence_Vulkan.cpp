#include "Fence_Vulkan.h"

#include "RenderDevice_Vulkan.h"

// ============================================================
// 소멸자
// ============================================================
Fence_Vulkan::~Fence_Vulkan()
{
    if (fence != VK_NULL_HANDLE && deviceHandle != VK_NULL_HANDLE)
    {
        vkDestroyFence(deviceHandle, fence, nullptr);
        fence = VK_NULL_HANDLE;
    }
}

// ============================================================
// Register
//
// VkFence를 생성합니다.
// DX12의 device->CreateFence(0, D3D12_FENCE_FLAG_NONE, ...)에 해당합니다.
// ============================================================
void Fence_Vulkan::Register(IRenderDevice* inDevice)
{
    RenderDevice_Vulkan* vulkanDevice = static_cast<RenderDevice_Vulkan*>(inDevice);
    if (!vulkanDevice)
    {
        throw std::runtime_error("[Vulkan] Fence: 유효하지 않은 RenderDevice입니다.");
    }

    deviceHandle = vulkanDevice->GetDevice();

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // SIGNALED_BIT: 생성 즉시 신호된 상태로 만듭니다.
    // 첫 프레임에서 vkWaitForFences가 즉시 통과하도록 하기 위함입니다.
    // DX12에서 초기값 0으로 생성하고, 처음 Wait할 때 0 이하면 즉시 통과하는 것과 유사합니다.
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(deviceHandle, &fenceInfo, nullptr, &fence));
}

// ============================================================
// SetEventOnComplete
//
// DX12: fence->SetEventOnCompletion(value, eventHandle)
// Vulkan: vkWaitForFences() 후 SetEvent(HANDLE)
//
// DX12의 SetEventOnCompletion은 비동기적으로 이벤트를 설정하지만,
// Vulkan에서는 CPU가 직접 vkWaitForFences로 대기합니다.
//
// 현재 Execute::OnRender/OnPostRender가 비어 있어 호출되지 않습니다.
// CommandList 구현 시 실제 GPU-CPU 동기화에 사용됩니다.
// ============================================================
void Fence_Vulkan::SetEventOnComplete(uint64_t value, void* inEventHandle)
{
    // GPU가 이 Fence를 신호할 때까지 CPU에서 대기합니다.
    // 타임아웃: UINT64_MAX (무한정 대기)
    VK_CHECK(vkWaitForFences(deviceHandle, 1, &fence, VK_TRUE, UINT64_MAX));

    // 내부 완료 값 갱신
    completedValue = value;

    // Windows 이벤트 핸들에 신호를 보내 WaitForSingleObject()가 반환되게 합니다.
    // DX12의 SetEventOnCompletion + WaitForSingleObject 패턴과 동일합니다.
#ifdef _WIN32
    if (inEventHandle)
    {
        SetEvent(static_cast<HANDLE>(inEventHandle));
    }
#endif
}

// ============================================================
// GetCompletedValue
//
// DX12의 fence->GetCompletedValue()는 GPU가 현재까지 완료한 타임라인 값을 반환합니다.
// Vulkan VkFence는 Binary이므로, 내부 카운터로 시뮬레이션합니다.
// ============================================================
uint64_t Fence_Vulkan::GetCompletedValue() const
{
    return completedValue;
}

// ============================================================
// MarkCompleted
//
// GPU Submit 완료 후 내부 카운터를 갱신합니다.
// CommandQueue_Vulkan::Signal() 구현 시 호출됩니다.
// ============================================================
void Fence_Vulkan::MarkCompleted(uint64_t value)
{
    completedValue = value;
}
