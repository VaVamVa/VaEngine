#pragma once

#include "Interfaces/ISwapChain.h"
#include "Common_Vulkan.h"

#include <vector>

class RenderDevice_Vulkan;
class CommandQueue_Vulkan;

// ============================================================
// SwapChain_Vulkan
//
// DX12의 SwapChain_DirectX(IDXGISwapChain3)와 대응합니다.
//
// DX12                     │ Vulkan
// ──────────────────────────────────────────────────────────────
// IDXGISwapChain3          │ VkSwapchainKHR       — 스왑체인
// ID3D12Resource(백버퍼)   │ VkImage              — 렌더 타겟 이미지
// D3D12_CPU_DESCRIPTOR(RTV)│ VkImageView          — 이미지 뷰
// [없음]                   │ VkRenderPass         — 렌더링 단계 명세
// [없음]                   │ VkFramebuffer        — RenderPass + ImageView 결합
//
// 프레임 렌더링 흐름:
//   [Execute::OnRender()]
//     1. BeginFrame()    → 다음 백버퍼 이미지 획득 + CommandBuffer 기록 시작
//     2. (Draw calls)    → 호출자가 CommandBuffer에 렌더링 명령 기록
//     3. EndFrame(CB)    → CommandBuffer 기록 종료
//   [Execute::OnLoop()]
//     4. Present()       → CommandBuffer 제출 + 화면 출력
//
// DX12와의 핵심 차이:
//   DX12는 Present만 호출해도 화면에 출력됩니다 (드라이버가 동기화 처리).
//   Vulkan은 RenderPass가 포함된 CommandBuffer 제출이 필수입니다.
//   또한 GPU-GPU 동기화를 위한 VkSemaphore를 명시적으로 관리합니다.
// ============================================================
class SwapChain_Vulkan : public ISwapChain
{
public:
    ~SwapChain_Vulkan() override;

    void Register(IRenderDevice* inDevice, const SwapChainDesc& desc) override;
    void Present(bool bVsync) override;
    void Resize(uint32_t width, uint32_t height) override;
    uint32_t GetCurrentBackBufferIndex() const override { return currentImageIndex; }

    // --------------------------------------------------------
    // 3D 렌더링을 위한 확장 API (ISwapChain 인터페이스 외부)
    // Execute::OnRender()에서 Vulkan 전용 렌더링 코드를 작성할 때 사용합니다.
    // --------------------------------------------------------

    // BeginFrame: 다음 백버퍼를 획득하고 CommandBuffer 기록을 시작합니다.
    // 반환값: 이번 프레임에 명령을 기록할 VkCommandBuffer
    VkCommandBuffer BeginFrame();

    // EndFrame: CommandBuffer 기록을 종료합니다. Present() 전에 호출해야 합니다.
    void EndFrame();

    // Pipeline 생성 시 필요한 RenderPass 핸들 반환
    VkRenderPass GetRenderPass() const { return renderPass; }

    // Viewport/Scissor 설정 시 필요한 렌더 영역 크기 반환
    VkExtent2D GetExtent() const { return extent; }

private:
    // ---- 초기화 단계별 헬퍼 ----
    void CreateSurface(VkInstance instance, void* hWnd);
    void QuerySwapchainSupport();
    void CreateSwapchain(const SwapChainDesc& desc);
    void RetrieveImages();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFramebuffers();
    void AllocateCommandBuffers();
    void CreateSyncObjects();

    // ---- 포맷·모드 선택 헬퍼 ----
    VkSurfaceFormatKHR ChooseSurfaceFormat() const;
    VkPresentModeKHR   ChoosePresentMode(bool bVsync) const;
    VkExtent2D         ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h) const;

    // ---- 정리 ----
    void CleanupSwapchain();

private:
    static constexpr uint32_t MAX_BUFFER_COUNT = 3;

    // ---- Vulkan 스왑체인 핵심 객체 ----

    // VkSurfaceKHR: OS 창과 Vulkan을 연결하는 추상적인 "화면 면".
    // Windows에서는 HWND를 감싸는 Win32 Surface입니다.
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // VkSwapchainKHR: 화면에 표시될 이미지들의 Ring Buffer.
    // DX12의 IDXGISwapChain3에 해당합니다.
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;

    // ---- 스왑체인 이미지와 뷰 ----
    VkImage     images[MAX_BUFFER_COUNT]{};     // 스왑체인 소유 (직접 소멸 금지)
    VkImageView imageViews[MAX_BUFFER_COUNT]{}; // DX12의 RTV에 해당

    // ---- RenderPass와 Framebuffer ----

    // VkRenderPass: "어떻게 렌더링할지" 명세 (초기화 방법, 전환 방법).
    // Pipeline_Vulkan 생성 시에도 이 핸들이 필요합니다.
    VkRenderPass renderPass = VK_NULL_HANDLE;

    // VkFramebuffer: RenderPass + 실제 ImageView 결합.
    VkFramebuffer framebuffers[MAX_BUFFER_COUNT]{};

    // ---- 프레임별 CommandBuffer ----
    // BeginFrame()에서 기록 시작, EndFrame()에서 기록 종료합니다.
    // 호출자(Execute)가 BeginFrame()과 EndFrame() 사이에 Draw 명령을 기록합니다.
    VkCommandBuffer commandBuffers[MAX_BUFFER_COUNT]{};

    // ---- 동기화 객체 ----

    // imageAvailableSemaphore:
    //   스왑체인이 "이 이미지를 이제 써도 된다"고 GPU에 신호.
    //   vkAcquireNextImageKHR 완료 시 이 세마포어가 신호됩니다.
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;

    // renderFinishedSemaphore:
    //   GPU 렌더링 완료 후 Present 큐에게 알리는 세마포어.
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;

    // inFlightFence:
    //   CPU가 이전 프레임 GPU 작업 완료를 대기하는 Fence.
    //   DX12의 WaitForSingleObject(fenceEvent) 패턴과 동일합니다.
    VkFence inFlightFence = VK_NULL_HANDLE;

    // ---- 메타데이터 ----
    VkFormat   imageFormat     = VK_FORMAT_UNDEFINED;
    VkExtent2D extent          {};
    uint32_t   imageCount      = 0;
    uint32_t   currentImageIndex = 0;
    bool       bFrameStarted   = false; // BeginFrame/EndFrame 쌍 검증용

    struct SwapchainSupportInfo
    {
        VkSurfaceCapabilitiesKHR        capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    } supportInfo;

    // 빌린 포인터 (소유권 없음)
    RenderDevice_Vulkan* vulkanDevice = nullptr;
    CommandQueue_Vulkan* vulkanQueue  = nullptr;
};
