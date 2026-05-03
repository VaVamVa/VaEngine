#include "CommandQueue_Vulkan.h"

#include "RenderDevice_Vulkan.h"
#include "Common_RHI.h"

// ============================================================
// 소멸자: CommandPool 정리
// ============================================================
CommandQueue_Vulkan::~CommandQueue_Vulkan()
{
    if (commandPool != VK_NULL_HANDLE && deviceHandle != VK_NULL_HANDLE)
    {
        // CommandPool을 소멸하면 이 풀에서 할당된 모든 CommandBuffer도 함께 해제됩니다.
        // DX12에서 CommandAllocator를 Reset/Release하는 것과 유사합니다.
        vkDestroyCommandPool(deviceHandle, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }
}

// ============================================================
// Register
//
// DX12의 CreateCommandQueue()가 하는 일을 Vulkan에서 수행합니다:
//   1. VkQueue 핸들 획득
//   2. VkCommandPool 생성
// ============================================================
void CommandQueue_Vulkan::Register(IRenderDevice* inDevice, const CommandQueueDesc& inDesc)
{
    RenderDevice_Vulkan* vulkanDevice = static_cast<RenderDevice_Vulkan*>(inDevice);
    if (!vulkanDevice)
    {
        throw std::runtime_error("[Vulkan] CommandQueue: 유효하지 않은 RenderDevice입니다.");
    }

    deviceHandle = vulkanDevice->GetDevice();

    // ---- Queue Family Index 결정 ----
    // 현재는 Graphics 큐만 지원합니다.
    // ECommandQueueType::Compute / Copy는 이후 별도 큐 패밀리 탐색 시 확장합니다.
    switch (static_cast<ECommandQueueType>(inDesc.type))
    {
        case ECommandQueueType::Graphics:
        default:
            familyIndex = vulkanDevice->GetGraphicsQueueFamilyIndex();
            break;
    }

    // ---- VkQueue 핸들 획득 ----
    // vkGetDeviceQueue: Device 생성 시 이미 만들어진 큐 핸들을 가져옵니다.
    // DX12의 CreateCommandQueue()와 달리, 이미 존재하는 큐에 접근하는 것입니다.
    // 세 번째 인자(0)는 큐 패밀리 내에서의 인덱스입니다. (큐를 여러 개 요청했을 때 구분)
    vkGetDeviceQueue(deviceHandle, familyIndex, 0, &queue);

    // ---- VkCommandPool 생성 ----
    // CommandPool은 CommandBuffer를 할당하기 위한 메모리 풀입니다.
    // 특정 큐 패밀리와 연결되어, 해당 패밀리 큐에서 실행될 CommandBuffer를 만듭니다.
    //
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT:
    //   각 CommandBuffer를 개별적으로 Reset(재사용 준비)할 수 있게 합니다.
    //   이 플래그 없이는 Pool 전체를 Reset해야 합니다.
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = familyIndex;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(deviceHandle, &poolInfo, nullptr, &commandPool));
}

// ============================================================
// ExecuteCommandLists
//
// DX12: queue->ExecuteCommandLists(count, lists)
// Vulkan: vkQueueSubmit(queue, submitCount, pSubmits, fence)
//
// 현재는 CommandList 구현이 없으므로 스텁으로 남겨 둡니다.
// ICommandList 구현 시 여기서 VkSubmitInfo를 구성해 제출합니다.
// ============================================================
void CommandQueue_Vulkan::ExecuteCommandLists(const std::vector<ICommandList*>& commandLists)
{
    // TODO: CommandList_Vulkan 구현 시 채움
}

// ============================================================
// Signal / Wait
//
// DX12: queue->Signal(fence, value) / queue->Wait(fence, value)
// Vulkan에서는 vkQueueSubmit의 signalSemaphores / waitSemaphores로 처리합니다.
//
// 현재 Execute의 OnRender/OnPostRender가 비어 있어 이 함수들은
// 호출되지 않습니다. CommandList 구현 시 채웁니다.
// ============================================================
void CommandQueue_Vulkan::Signal(IFence* fence, uint64_t value)
{
    // TODO: CommandList_Vulkan 구현 시 채움
}

void CommandQueue_Vulkan::Wait(IFence* fence, uint64_t value)
{
    // TODO: CommandList_Vulkan 구현 시 채움
}
