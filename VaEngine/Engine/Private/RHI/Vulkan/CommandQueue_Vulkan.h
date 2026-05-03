#pragma once

#include "Interfaces/ICommandQueue.h"
#include "Common_Vulkan.h"

// ============================================================
// CommandQueue_Vulkan
//
// DX12의 CommandQueue_DirectX(ID3D12CommandQueue)와 대응합니다.
//
// DX12             │ Vulkan
// ──────────────────────────────────────────────
// ID3D12CommandQueue │ VkQueue         — GPU 명령 제출 큐 핸들
// CreateCommandQueue │ vkGetDeviceQueue— Device에서 큐 핸들 획득
// CommandAllocator   │ VkCommandPool   — CommandBuffer 메모리 풀
//
// Vulkan에서의 큐(Queue)는 DX12와 달리 Device 생성 시 이미 내부적으로
// 생성되어 있습니다. vkGetDeviceQueue()는 그 핸들을 가져오는 것뿐입니다.
//
// VkCommandPool은 CommandBuffer 메모리를 관리하는 풀입니다.
// SwapChain_Vulkan이 이 풀에서 CommandBuffer를 할당합니다.
// ============================================================
class CommandQueue_Vulkan : public ICommandQueue
{
public:
    ~CommandQueue_Vulkan() override;

    void Register(IRenderDevice* inDevice, const CommandQueueDesc& inDesc) override;
    void ExecuteCommandLists(const std::vector<ICommandList*>& commandLists) override;
    void Signal(IFence* fence, uint64_t value) override;
    void Wait(IFence* fence, uint64_t value) override;

    // ---- Vulkan 핸들 접근자 ----
    // SwapChain_Vulkan이 vkQueueSubmit / vkQueuePresentKHR 호출에 사용합니다.
    VkQueue       GetQueue()       const { return queue; }
    VkCommandPool GetCommandPool() const { return commandPool; }
    uint32_t      GetFamilyIndex() const { return familyIndex; }

private:
    // VkQueue: GPU에 명령(CommandBuffer)을 제출하는 핸들.
    // DX12의 ID3D12CommandQueue에 해당합니다.
    // Device에 의해 관리되므로 직접 소멸시키지 않습니다.
    VkQueue queue = VK_NULL_HANDLE;

    // VkCommandPool: CommandBuffer를 할당하기 위한 메모리 풀.
    // DX12의 CommandAllocator와 유사하지만, 여러 CommandBuffer를 공유할 수 있습니다.
    // 특정 큐 패밀리와 연결되어, 해당 패밀리에서만 사용할 CommandBuffer를 생성합니다.
    VkCommandPool commandPool = VK_NULL_HANDLE;

    // 이 큐가 속한 큐 패밀리 인덱스.
    // CommandPool 생성과 vkGetDeviceQueue() 호출에 사용됩니다.
    uint32_t familyIndex = UINT32_MAX;

    // Device 핸들 (소유권 없음 — RenderDevice_Vulkan이 소유).
    // Shutdown 시 CommandPool 소멸에 필요합니다.
    VkDevice deviceHandle = VK_NULL_HANDLE;
};
