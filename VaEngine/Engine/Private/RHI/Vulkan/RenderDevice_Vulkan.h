#pragma once

#include "Interfaces/IRenderDevice.h"
#include "Common_Vulkan.h"

#include <vector>

// ============================================================
// RenderDevice_Vulkan
//
// DX12의 RenderDevice_DirectX와 대응하는 클래스입니다.
//
// DX12          │ Vulkan
// ─────────────────────────────────────────────────────
// IDXGIFactory6 │ VkInstance        — Vulkan API 진입점
// IDXGIAdapter4 │ VkPhysicalDevice  — 물리적 GPU
// ID3D12Device  │ VkDevice          — 논리적 GPU (실제 사용 장치)
//
// Vulkan에서는 '물리 장치(PhysicalDevice)' 와
// '논리 장치(Device)' 를 명시적으로 구분합니다.
//   - VkPhysicalDevice: GPU 하드웨어 그 자체 (변경 불가)
//   - VkDevice        : 해당 GPU에 대한 논리적 연결 (큐·리소스 생성에 사용)
// ============================================================
class RenderDevice_Vulkan : public IRenderDevice
{
public:
    void Initialize() override;
    void Shutdown() override;

    std::unique_ptr<ISwapChain>    CreateSwapChain(const SwapChainDesc& desc) override;
    std::unique_ptr<ICommandQueue> CreateCommandQueue(const CommandQueueDesc& desc) override;
    std::unique_ptr<IFence>        CreateFence() override;
    std::unique_ptr<ICommandList>  CreateCommandList(const CommandListDesc& desc) override;

    // --------------------------------------------------------
    // 다른 Vulkan 클래스(CommandQueue, SwapChain 등)가
    // 리소스를 생성할 때 필요한 핵심 핸들을 제공합니다.
    // DX12의 GetDevice() / GetFactory() / GetAdapter()와 동일한 역할.
    // --------------------------------------------------------
    VkInstance       GetInstance()               const { return instance; }
    VkPhysicalDevice GetPhysicalDevice()         const { return physicalDevice; }
    VkDevice         GetDevice()                 const { return device; }
    uint32_t         GetGraphicsQueueFamilyIndex() const { return graphicsQueueFamilyIndex; }

private:
    // ---- 초기화 단계별 헬퍼 함수 ----
    void CreateInstance();
    void SetupDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();

    // ---- 내부 유틸리티 ----
    bool CheckValidationLayerSupport() const;
    bool IsDeviceSuitable(VkPhysicalDevice candidateDevice, uint32_t& outGraphicsFamily) const;
    bool CheckDeviceExtensionSupport(VkPhysicalDevice candidateDevice) const;

    // ---- Validation Layer 디버그 콜백 (static이어야 Vulkan이 호출 가능) ----
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT             messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*                                       pUserData
    );

private:
    // ---- Vulkan 핵심 객체 ----

    // VkInstance: Vulkan 라이브러리와의 연결점.
    // DX12의 IDXGIFactory에 해당하며, Extension·Layer 설정이 여기서 결정됩니다.
    VkInstance instance = VK_NULL_HANDLE;

    // VkDebugUtilsMessengerEXT: Validation Layer의 메시지를 받는 콜백 핸들.
    // DX12의 ID3D12InfoQueue에 해당합니다. Debug 빌드에서만 생성됩니다.
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    // VkPhysicalDevice: 실제 GPU 하드웨어를 나타내는 핸들.
    // DX12의 IDXGIAdapter에 해당합니다. Vulkan이 내부적으로 관리하므로 직접 소멸시키지 않습니다.
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    // VkDevice: GPU에 대한 논리적 연결. 모든 Vulkan 리소스 생성의 기반이 됩니다.
    // DX12의 ID3D12Device에 해당합니다.
    VkDevice device = VK_NULL_HANDLE;

    // Graphics 작업을 수행하는 큐 패밀리의 인덱스.
    // CommandQueue_Vulkan·SwapChain_Vulkan에서 큐 핸들을 가져오거나
    // CommandPool을 생성할 때 사용합니다.
    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
};
