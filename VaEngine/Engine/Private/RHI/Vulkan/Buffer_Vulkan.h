#pragma once

#include "Common_Vulkan.h"

#include <cstdint>
#include <stdexcept>

// ============================================================
// Buffer_Vulkan
//
// Vulkan에서의 GPU 버퍼 (Vertex/Index/Uniform)를 관리합니다.
//
// DX12             │ Vulkan
// ─────────────────────────────────────────────────────
// ID3D12Resource   │ VkBuffer       — 버퍼 핸들
// CD3DX12_HEAP_PROPERTIES → VkMemoryAllocateInfo — 메모리 할당
// UpdateSubresource│ Map/Memcpy/Unmap 또는 Staging Buffer
//
// Vulkan 메모리 관리의 핵심:
//   DX12는 메모리 유형을 HeapType(Upload/Default/Readback)으로 간단히 구분합니다.
//   Vulkan은 VkPhysicalDeviceMemoryProperties를 통해 세밀한 메모리 타입을 직접 선택합니다:
//     - HOST_VISIBLE + HOST_COHERENT  : CPU에서 직접 쓸 수 있는 메모리 (Upload Heap)
//     - DEVICE_LOCAL                  : GPU 전용 고속 메모리 (Default Heap)
//   성능이 중요한 Vertex/Index Buffer는 Staging Buffer(CPU visible)를
//   통해 Device Local 메모리에 복사하는 것이 이상적이지만,
//   학습 목적으로 HOST_VISIBLE 메모리에 직접 쓰는 방법도 사용합니다.
// ============================================================
class Buffer_Vulkan
{
public:
    ~Buffer_Vulkan();

    // ---- 버퍼 생성 ----

    // CPU에서 직접 쓸 수 있는 버퍼 생성 (Uniform Buffer, 동적 데이터용)
    // DX12의 CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)에 해당합니다.
    void CreateHostVisible(VkPhysicalDevice physicalDevice, VkDevice device,
                           VkDeviceSize size, VkBufferUsageFlags usage);

    // GPU 전용 고속 버퍼 생성 + Staging 복사 (Vertex/Index Buffer용)
    // 1단계: Staging(CPU visible) 버퍼에 데이터 쓰기
    // 2단계: Device Local 버퍼로 복사
    // DX12의 CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)에 해당합니다.
    void CreateDeviceLocal(VkPhysicalDevice physicalDevice, VkDevice device,
                           VkCommandPool commandPool, VkQueue queue,
                           const void* data, VkDeviceSize size, VkBufferUsageFlags usage);

    // ---- 데이터 갱신 (HOST_VISIBLE 버퍼만 가능) ----
    // DX12의 Map(0, nullptr, &pData) + memcpy + Unmap(0, nullptr)에 해당합니다.
    void Upload(const void* data, VkDeviceSize size);

    // ---- Getter ----
    VkBuffer     GetBuffer() const { return buffer; }
    VkDeviceSize GetSize()   const { return bufferSize; }

private:
    static void CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice device,
                             VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties,
                             VkBuffer& outBuffer, VkDeviceMemory& outMemory);

    static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                                   uint32_t typeFilter,
                                   VkMemoryPropertyFlags properties);

    static void CopyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                           VkBuffer src, VkBuffer dst, VkDeviceSize size);

private:
    VkBuffer       buffer     = VK_NULL_HANDLE;
    VkDeviceMemory memory     = VK_NULL_HANDLE;
    VkDeviceSize   bufferSize = 0;

    // Device 핸들 (소유권 없음)
    VkDevice deviceHandle = VK_NULL_HANDLE;
};
