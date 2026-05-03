#include "RenderDevice_Vulkan.h"

#include "CommandQueue_Vulkan.h"
#include "SwapChain_Vulkan.h"
#include "Fence_Vulkan.h"
#include "Interfaces/ICommandList.h"

#include <vector>
#include <cstring>
#include <iostream>

// ============================================================
// Validation Layer
//
// DX12의 D3D12 Debug Layer에 해당하는 Vulkan의 디버그 도구입니다.
// Vulkan은 기본적으로 오류 검사를 하지 않아 매우 빠르지만,
// 개발 중에는 이 레이어를 켜서 잘못된 API 사용을 즉시 감지합니다.
//
// 시스템에 Vulkan SDK가 설치되어 있으면 사용 가능합니다.
// ============================================================
static constexpr const char* VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";

// GPU가 반드시 지원해야 하는 Device Extension 목록.
// VK_KHR_swapchain: 렌더링 결과를 화면에 출력(present)하기 위한 스왑체인 기능.
// DX12에서는 IDXGISwapChain이 자동으로 이를 처리하지만,
// Vulkan에서는 명시적으로 Extension을 요청해야 합니다.
static const std::vector<const char*> REQUIRED_DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// ============================================================
// Initialize
// ============================================================
void RenderDevice_Vulkan::Initialize()
{
    // DX12의 초기화 순서와 동일한 흐름입니다:
    //   EnableDebugLayer → CreateFactory → PickAdapter → CreateDevice
    //       ↕                  ↕               ↕              ↕
    //  SetupDebugMessenger  CreateInstance  PickPhysical  CreateLogical

    CreateInstance();
    SetupDebugMessenger();  // Debug 빌드에서만 실제 동작
    PickPhysicalDevice();
    CreateLogicalDevice();
}

// ============================================================
// Shutdown
// ============================================================
void RenderDevice_Vulkan::Shutdown()
{
    // Vulkan 객체는 생성 역순으로 소멸시켜야 합니다.
    // 자식 객체(Device)를 먼저 소멸하고, 부모 객체(Instance)를 나중에 소멸합니다.

    if (device != VK_NULL_HANDLE)
    {
        // Device를 소멸하기 전에 GPU 작업이 모두 완료될 때까지 대기합니다.
        // DX12에서 FlushCommandQueue() 또는 WaitForGpu() 를 호출하는 것과 같습니다.
        vkDeviceWaitIdle(device);
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

#ifdef _DEBUG
    if (debugMessenger != VK_NULL_HANDLE)
    {
        // vkDestroyDebugUtilsMessengerEXT는 Extension 함수이므로,
        // Instance에서 함수 포인터를 직접 조회해야 합니다.
        auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
        );
        if (destroyFn)
        {
            destroyFn(instance, debugMessenger, nullptr);
        }
        debugMessenger = VK_NULL_HANDLE;
    }
#endif

    if (instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}

// ============================================================
// CreateSwapChain
// ============================================================
std::unique_ptr<ISwapChain> RenderDevice_Vulkan::CreateSwapChain(const SwapChainDesc& desc)
{
    if (desc.RegisteredQueue == nullptr || desc.displayInfo.Handle == nullptr)
    {
        throw std::runtime_error("[Vulkan] SwapChain 생성 전에 Queue와 Window Handle이 필요합니다.");
    }
    auto swapChain = std::make_unique<SwapChain_Vulkan>();
    swapChain->Register(this, desc);
    return swapChain;
}

// ============================================================
// CreateCommandQueue
// ============================================================
std::unique_ptr<ICommandQueue> RenderDevice_Vulkan::CreateCommandQueue(const CommandQueueDesc& desc)
{
    auto queue = std::make_unique<CommandQueue_Vulkan>();
    queue->Register(this, desc);
    return queue;
}

// ============================================================
// CreateFence
// ============================================================
std::unique_ptr<IFence> RenderDevice_Vulkan::CreateFence()
{
    auto fence = std::make_unique<Fence_Vulkan>();
    fence->Register(this);
    return fence;
}

// ============================================================
// CreateCommandList
// ============================================================
std::unique_ptr<ICommandList> RenderDevice_Vulkan::CreateCommandList(const CommandListDesc& desc)
{
    // CommandList는 이후 단계에서 구현합니다.
    return std::unique_ptr<ICommandList>();
}

// ============================================================
// [내부] CreateInstance
//
// VkInstance는 Vulkan 라이브러리와의 연결 진입점입니다.
// DX12의 CreateDXGIFactory()에 해당하며, 여기서 사용할
// Instance Extension(Surface 지원 등)과 Validation Layer를 결정합니다.
// ============================================================
void RenderDevice_Vulkan::CreateInstance()
{
    // ---- Instance Extension 요청 목록 ----
    // Extension이란 Vulkan 코어에 포함되지 않은 부가 기능입니다.
    // 사용하려면 Instance 또는 Device 생성 시 명시적으로 요청해야 합니다.
    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,          // Surface(화면 출력) 기반 기능
        VK_PLATFORM_SURFACE_EXTENSION_NAME,     // Windows: VK_KHR_win32_surface
    };

#ifdef _DEBUG
    // VK_EXT_debug_utils: Validation Layer 메시지를 받는 콜백(Messenger)을 등록하는 Extension.
    // DX12의 ID3D12InfoQueue와 유사한 역할입니다.
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    // ---- Validation Layer 설정 ----
    std::vector<const char*> layers;
#ifdef _DEBUG
    if (CheckValidationLayerSupport())
    {
        layers.push_back(VALIDATION_LAYER_NAME);
    }
    else
    {
        // Vulkan SDK가 설치되어 있지 않으면 Validation Layer를 사용할 수 없습니다.
        // 오류를 던지지 않고 경고만 출력합니다.
        std::cerr << "[Vulkan] Warning: Validation Layer를 찾을 수 없습니다. Vulkan SDK가 설치되었는지 확인하세요.\n";
    }
#endif

    // ---- 애플리케이션 정보 ----
    // Vulkan 드라이버가 최적화 힌트로 사용할 수 있는 메타데이터입니다. 필수는 아닙니다.
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "VaEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "VaEngine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    // API 버전: Vulkan 1.2를 요구합니다. (Timeline Semaphore 등 최신 기능 포함)
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    // ---- Instance 생성 정보 ----
    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames     = layers.data();

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
}

// ============================================================
// [내부] SetupDebugMessenger
//
// Validation Layer로부터 오류·경고 메시지를 받는 콜백을 등록합니다.
// DX12의 ID3D12InfoQueue::SetBreakOnSeverity()와 유사합니다.
// Debug 빌드에서만 동작합니다.
// ============================================================
void RenderDevice_Vulkan::SetupDebugMessenger()
{
#ifndef _DEBUG
    return;
#else
    // vkCreateDebugUtilsMessengerEXT는 Extension 함수이므로,
    // vkGetInstanceProcAddr()로 함수 포인터를 직접 가져와야 합니다.
    // DX12에서 DLL에서 함수를 직접 로드하는 것과 유사한 개념입니다.
    auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
    );
    if (!createFn) { return; }

    VkDebugUtilsMessengerCreateInfoEXT messengerInfo{};
    messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    // 콜백을 받을 메시지 심각도 필터:
    // VERBOSE(상세) 는 제외하고, WARNING 이상만 받습니다.
    messengerInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    // 콜백을 받을 메시지 유형 필터:
    // GENERAL(일반), VALIDATION(규격 위반), PERFORMANCE(성능 경고) 모두 받습니다.
    messengerInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    messengerInfo.pfnUserCallback = DebugCallback;

    VK_CHECK(createFn(instance, &messengerInfo, nullptr, &debugMessenger));
#endif
}

// ============================================================
// [내부] PickPhysicalDevice
//
// 시스템의 GPU 목록을 열거하고 가장 적합한 GPU를 선택합니다.
// DX12의 EnumAdapterByGpuPreference() + D3D12CreateDevice() 검사와 동일합니다.
//
// '적합한 GPU' 기준:
//   1. Graphics 큐 패밀리를 보유
//   2. VK_KHR_swapchain Extension 지원
// ============================================================
void RenderDevice_Vulkan::PickPhysicalDevice()
{
    // 시스템에 설치된 모든 Vulkan 지원 GPU를 열거합니다.
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        throw std::runtime_error("[Vulkan] Vulkan을 지원하는 GPU를 찾을 수 없습니다.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (VkPhysicalDevice candidateDevice : devices)
    {
        uint32_t familyIndex = UINT32_MAX;
        if (IsDeviceSuitable(candidateDevice, familyIndex))
        {
            physicalDevice         = candidateDevice;
            graphicsQueueFamilyIndex = familyIndex;

            // 선택된 GPU 이름을 출력합니다 (디버깅 용도).
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(physicalDevice, &props);
            std::cout << "[Vulkan] 선택된 GPU: " << props.deviceName << "\n";
            return;
        }
    }

    throw std::runtime_error("[Vulkan] 요구 사항을 충족하는 GPU를 찾을 수 없습니다.");
}

// ============================================================
// [내부] CreateLogicalDevice
//
// 선택한 물리 장치(PhysicalDevice)에 대한 논리적 연결(Device)을 생성합니다.
// 이 시점에 사용할 큐(Queue)와 Device Extension도 함께 지정합니다.
// DX12의 D3D12CreateDevice()에 해당합니다.
// ============================================================
void RenderDevice_Vulkan::CreateLogicalDevice()
{
    // ---- 큐 생성 정보 ----
    // Vulkan에서 큐는 Device 생성 시 함께 요청해야 합니다.
    // DX12는 Device 생성 후 따로 ID3D12CommandQueue를 생성하지만,
    // Vulkan은 Device 생성 시에 큐 패밀리와 개수를 미리 지정합니다.
    // (큐 핸들은 Device 생성 후 vkGetDeviceQueue()로 가져옵니다.)
    float queuePriority = 1.0f; // 우선순위 [0.0, 1.0]. 단일 큐이므로 최대값.
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount       = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // ---- 사용할 GPU 기능 (PhysicalDeviceFeatures) ----
    // 지금은 기본 기능만 사용합니다. 이후 텍스처 샘플링 등에서 필요한 기능을 추가합니다.
    VkPhysicalDeviceFeatures deviceFeatures{};

    // ---- Validation Layer ----
    std::vector<const char*> layers;
#ifdef _DEBUG
    if (CheckValidationLayerSupport())
    {
        layers.push_back(VALIDATION_LAYER_NAME);
    }
#endif

    // ---- Logical Device 생성 ----
    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = 1;
    createInfo.pQueueCreateInfos       = &queueCreateInfo;
    createInfo.pEnabledFeatures        = &deviceFeatures;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(REQUIRED_DEVICE_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data();
    createInfo.enabledLayerCount       = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames     = layers.data();

    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));
}

// ============================================================
// [내부 유틸] CheckValidationLayerSupport
// ============================================================
bool RenderDevice_Vulkan::CheckValidationLayerSupport() const
{
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const auto& layer : availableLayers)
    {
        if (std::strcmp(layer.layerName, VALIDATION_LAYER_NAME) == 0)
        {
            return true;
        }
    }
    return false;
}

// ============================================================
// [내부 유틸] IsDeviceSuitable
//
// 후보 GPU가 Graphics 큐 패밀리를 보유하고 있는지,
// 필요한 Extension을 지원하는지 확인합니다.
// ============================================================
bool RenderDevice_Vulkan::IsDeviceSuitable(VkPhysicalDevice candidateDevice, uint32_t& outGraphicsFamily) const
{
    // ---- 큐 패밀리 확인 ----
    // Vulkan의 큐 패밀리(Queue Family)는 DX12의 CommandQueue 타입(Direct/Copy/Compute)과 유사합니다.
    // 하나의 GPU에 여러 큐 패밀리가 있을 수 있으며, 각 패밀리는 지원하는 작업 유형이 다릅니다.
    //   - VK_QUEUE_GRAPHICS_BIT  : 그래픽스 렌더링 명령 (DX12 Direct 큐)
    //   - VK_QUEUE_COMPUTE_BIT   : 컴퓨트 셰이더 (DX12 Compute 큐)
    //   - VK_QUEUE_TRANSFER_BIT  : 버퍼/텍스처 복사 (DX12 Copy 큐)
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidateDevice, &familyCount, nullptr);

    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(candidateDevice, &familyCount, families.data());

    bool foundGraphics = false;
    for (uint32_t i = 0; i < familyCount; ++i)
    {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            outGraphicsFamily = i;
            foundGraphics = true;
            break;
        }
    }

    if (!foundGraphics) { return false; }

    // ---- Extension 지원 확인 ----
    return CheckDeviceExtensionSupport(candidateDevice);
}

// ============================================================
// [내부 유틸] CheckDeviceExtensionSupport
// ============================================================
bool RenderDevice_Vulkan::CheckDeviceExtensionSupport(VkPhysicalDevice candidateDevice) const
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(candidateDevice, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> available(extensionCount);
    vkEnumerateDeviceExtensionProperties(candidateDevice, nullptr, &extensionCount, available.data());

    for (const char* required : REQUIRED_DEVICE_EXTENSIONS)
    {
        bool found = false;
        for (const auto& ext : available)
        {
            if (std::strcmp(ext.extensionName, required) == 0)
            {
                found = true;
                break;
            }
        }
        if (!found) { return false; }
    }
    return true;
}

// ============================================================
// [static] DebugCallback
//
// Validation Layer가 오류·경고를 발견했을 때 호출됩니다.
// DX12의 ID3D12InfoQueue 콜백에 해당합니다.
// ============================================================
VKAPI_ATTR VkBool32 VKAPI_CALL RenderDevice_Vulkan::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT             /*messageType*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       /*pUserData*/
)
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::cerr << "[Vulkan Validation] " << pCallbackData->pMessage << "\n";
    }

    // VK_FALSE를 반환해야 합니다.
    // VK_TRUE를 반환하면 해당 Vulkan 호출이 VK_ERROR_VALIDATION_FAILED_EXT로 중단됩니다.
    return VK_FALSE;
}
