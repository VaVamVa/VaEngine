#include "Buffer_Vulkan.h"

#include <cstring>

// ============================================================
// 소멸자
// ============================================================
Buffer_Vulkan::~Buffer_Vulkan()
{
    if (deviceHandle == VK_NULL_HANDLE) { return; }
    if (buffer != VK_NULL_HANDLE) { vkDestroyBuffer(deviceHandle, buffer, nullptr); buffer = VK_NULL_HANDLE; }
    if (memory != VK_NULL_HANDLE) { vkFreeMemory(deviceHandle, memory, nullptr);   memory = VK_NULL_HANDLE; }
}

// ============================================================
// CreateHostVisible
//
// CPU에서 직접 쓸 수 있는 버퍼를 생성합니다.
// Uniform Buffer처럼 매 프레임 갱신이 필요한 데이터에 사용합니다.
// ============================================================
void Buffer_Vulkan::CreateHostVisible(
    VkPhysicalDevice physicalDevice, VkDevice device,
    VkDeviceSize size, VkBufferUsageFlags usage)
{
    deviceHandle = device;
    bufferSize   = size;

    // HOST_VISIBLE  : CPU에서 vkMapMemory()로 접근 가능
    // HOST_COHERENT : CPU 쓰기가 즉시 GPU에서 보임 (명시적 flush 불필요)
    CreateBuffer(
        physicalDevice, device, size, usage,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        buffer, memory
    );
}

// ============================================================
// CreateDeviceLocal
//
// GPU 전용 고속 메모리에 버퍼를 생성하고 데이터를 복사합니다.
// Vertex/Index Buffer처럼 초기화 후 변경 없는 데이터에 최적입니다.
//
// 과정:
//   1. CPU-가시 Staging Buffer 생성 → 데이터 복사
//   2. GPU 전용 Buffer 생성
//   3. CommandBuffer로 Staging → Device Local 복사
//   4. Staging Buffer 해제
// ============================================================
void Buffer_Vulkan::CreateDeviceLocal(
    VkPhysicalDevice physicalDevice, VkDevice device,
    VkCommandPool commandPool, VkQueue queue,
    const void* data, VkDeviceSize size, VkBufferUsageFlags usage)
{
    deviceHandle = device;
    bufferSize   = size;

    // ---- 1단계: Staging Buffer (CPU visible) ----
    // Staging Buffer: CPU → GPU 데이터 전송을 위한 임시 버퍼
    // DX12의 UpdateSubresources()가 내부적으로 하는 과정과 동일합니다.
    VkBuffer       stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    CreateBuffer(
        physicalDevice, device, size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,  // 전송 소스로 사용
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory
    );

    // CPU → Staging Buffer 복사
    void* mappedPtr = nullptr;
    vkMapMemory(device, stagingMemory, 0, size, 0, &mappedPtr);
    std::memcpy(mappedPtr, data, static_cast<size_t>(size));
    vkUnmapMemory(device, stagingMemory);

    // ---- 2단계: Device Local Buffer ----
    // DEVICE_LOCAL: GPU 전용 메모리 (VRAM). 가장 빠른 GPU 접근 속도.
    // TRANSFER_DST: Staging에서 복사 받을 목적지로 사용
    CreateBuffer(
        physicalDevice, device, size,
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        buffer, memory
    );

    // ---- 3단계: GPU CommandBuffer로 Staging → Device Local 복사 ----
    CopyBuffer(device, commandPool, queue, stagingBuffer, buffer, size);

    // ---- 4단계: Staging Buffer 정리 ----
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
}

// ============================================================
// Upload
//
// HOST_VISIBLE 버퍼의 데이터를 갱신합니다. (매 프레임 MVP 갱신 등)
// DX12의 Map → memcpy → Unmap 패턴과 동일합니다.
// ============================================================
void Buffer_Vulkan::Upload(const void* data, VkDeviceSize size)
{
    void* mappedPtr = nullptr;
    vkMapMemory(deviceHandle, memory, 0, size, 0, &mappedPtr);
    std::memcpy(mappedPtr, data, static_cast<size_t>(size));
    vkUnmapMemory(deviceHandle, memory);
}

// ============================================================
// [static] CreateBuffer
//
// VkBuffer + VkDeviceMemory를 생성하는 헬퍼입니다.
// ============================================================
void Buffer_Vulkan::CreateBuffer(
    VkPhysicalDevice physicalDevice, VkDevice device,
    VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& outBuffer, VkDeviceMemory& outMemory)
{
    // ---- VkBuffer 생성 ----
    // 이 시점에서는 "버퍼의 명세"만 생성됩니다. 실제 메모리는 아직 없습니다.
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    // EXCLUSIVE: 한 번에 하나의 큐 패밀리만 접근 (복수 큐 공유 시 CONCURRENT 필요)
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &outBuffer));

    // ---- 메모리 요구 사항 조회 ----
    // 버퍼가 어떤 메모리 타입을 필요로 하는지, 크기·정렬 요구 사항을 조회합니다.
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, outBuffer, &memRequirements);

    // ---- 메모리 할당 ----
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    // 요구 사항에 맞는 메모리 타입 인덱스를 찾아 지정합니다.
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &outMemory));

    // 버퍼와 메모리를 연결합니다.
    // DX12의 경우 Resource 생성 시 메모리가 자동으로 연결되지만,
    // Vulkan에서는 명시적으로 바인딩해야 합니다.
    VK_CHECK(vkBindBufferMemory(device, outBuffer, outMemory, 0));
}

// ============================================================
// [static] FindMemoryType
//
// GPU가 지원하는 메모리 타입 중 요구 사항을 만족하는 인덱스를 찾습니다.
// DX12에서 D3D12_HEAP_TYPE으로 추상화된 것을 Vulkan에서 직접 선택하는 과정입니다.
// ============================================================
uint32_t Buffer_Vulkan::FindMemoryType(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties)
{
    // GPU가 지원하는 모든 메모리 타입 조회
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        // typeFilter의 i번째 비트가 1 = 이 메모리 타입을 버퍼가 허용
        // propertyFlags = 이 메모리 타입이 가진 속성
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("[Vulkan] 적절한 메모리 타입을 찾을 수 없습니다.");
}

// ============================================================
// [static] CopyBuffer
//
// 임시 CommandBuffer를 만들어 GPU에서 버퍼 간 복사를 수행합니다.
// DX12의 CopyBufferRegion()과 동일한 역할입니다.
// ============================================================
void Buffer_Vulkan::CopyBuffer(
    VkDevice device, VkCommandPool commandPool, VkQueue queue,
    VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    // ---- 임시 CommandBuffer 할당 ----
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer tempCB = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &allocInfo, &tempCB);

    // ---- 복사 명령 기록 ----
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // 단발성 사용
    vkBeginCommandBuffer(tempCB, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = size;
    vkCmdCopyBuffer(tempCB, src, dst, 1, &copyRegion);

    vkEndCommandBuffer(tempCB);

    // ---- 즉시 제출 & 완료 대기 ----
    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &tempCB;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    // GPU가 복사를 완료할 때까지 CPU 대기
    // 성능이 중요하다면 Fence를 사용해 비동기로 처리할 수 있습니다.
    vkQueueWaitIdle(queue);

    // ---- 임시 CommandBuffer 해제 ----
    vkFreeCommandBuffers(device, commandPool, 1, &tempCB);
}
