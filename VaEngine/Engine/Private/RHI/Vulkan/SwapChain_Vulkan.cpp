#include "SwapChain_Vulkan.h"

#include "RenderDevice_Vulkan.h"
#include "CommandQueue_Vulkan.h"

#include <algorithm>
#include <limits>

// ============================================================
// 소멸자
// ============================================================
SwapChain_Vulkan::~SwapChain_Vulkan()
{
    if (vulkanDevice == nullptr) { return; }
    VkDevice vkDev = vulkanDevice->GetDevice();

    vkDeviceWaitIdle(vkDev);
    CleanupSwapchain();

    if (surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(vulkanDevice->GetInstance(), surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
}

// ============================================================
// Register
// ============================================================
void SwapChain_Vulkan::Register(IRenderDevice* inDevice, const SwapChainDesc& desc)
{
    vulkanDevice = static_cast<RenderDevice_Vulkan*>(inDevice);
    vulkanQueue  = static_cast<CommandQueue_Vulkan*>(desc.RegisteredQueue);

    if (!vulkanDevice || !vulkanQueue || !desc.displayInfo.Handle)
    {
        throw std::runtime_error("[Vulkan] SwapChain: 유효하지 않은 인자입니다.");
    }

    CreateSurface(vulkanDevice->GetInstance(), desc.displayInfo.Handle);

    // Graphics 큐가 Present도 지원하는지 확인
    VkBool32 presentSupported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(
        vulkanDevice->GetPhysicalDevice(),
        vulkanQueue->GetFamilyIndex(),
        surface,
        &presentSupported
    );
    if (!presentSupported)
    {
        throw std::runtime_error("[Vulkan] Graphics 큐가 Present를 지원하지 않습니다.");
    }

    QuerySwapchainSupport();
    CreateSwapchain(desc);
    RetrieveImages();
    CreateImageViews();
    CreateRenderPass();
    CreateFramebuffers();
    AllocateCommandBuffers();
    CreateSyncObjects();
}

// ============================================================
// BeginFrame
//
// 매 프레임 OnRender() 시작 시 호출합니다.
//   1. 이전 프레임 완료 대기
//   2. 다음 백버퍼 이미지 획득
//   3. CommandBuffer 기록 시작
//   4. RenderPass 시작 (Clear 포함)
//
// 반환값: 이번 프레임에 Draw 명령을 기록할 VkCommandBuffer
// ============================================================
VkCommandBuffer SwapChain_Vulkan::BeginFrame()
{
    VkDevice vkDev = vulkanDevice->GetDevice();

    // ---- 이전 프레임 GPU 완료 대기 ----
    vkWaitForFences(vkDev, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(vkDev, 1, &inFlightFence);

    // ---- 다음 백버퍼 획득 ----
    VkResult acquireResult = vkAcquireNextImageKHR(
        vkDev,
        swapChain,
        UINT64_MAX,
        imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &currentImageIndex
    );

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        throw std::runtime_error("[Vulkan] 스왑체인 만료. Resize() 필요.");
    }
    else if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("[Vulkan] vkAcquireNextImageKHR 실패.");
    }

    // ---- CommandBuffer 리셋 & 기록 시작 ----
    // 이전 프레임에서 기록된 내용을 지우고 새로 기록 준비합니다.
    VkCommandBuffer cb = commandBuffers[currentImageIndex];
    VK_CHECK(vkResetCommandBuffer(cb, 0));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // ONE_TIME_SUBMIT: 이 CB는 한 번만 제출됩니다 (매 프레임 재기록).
    // 드라이버 최적화 힌트입니다.
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cb, &beginInfo));

    // ---- RenderPass 시작 (검은 색 초기화) ----
    // Clear 색상: 검은 색 (RGBA = 0, 0, 0, 1)
    // 이후 Clear 색상 변경이나 배경 색상 설정은 여기서 합니다.
    VkClearValue clearColor{};
    clearColor.color = {{ 0.05f, 0.05f, 0.1f, 1.0f }}; // 어두운 남색 배경

    VkRenderPassBeginInfo rpBeginInfo{};
    rpBeginInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass        = renderPass;
    rpBeginInfo.framebuffer       = framebuffers[currentImageIndex];
    rpBeginInfo.renderArea.offset = { 0, 0 };
    rpBeginInfo.renderArea.extent = extent;
    rpBeginInfo.clearValueCount   = 1;
    rpBeginInfo.pClearValues      = &clearColor;

    // INLINE: 모든 렌더링 명령을 이 CB에 직접 기록합니다.
    vkCmdBeginRenderPass(cb, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    bFrameStarted = true;
    return cb;
}

// ============================================================
// EndFrame
//
// CommandBuffer 기록을 종료합니다. Present() 전에 반드시 호출해야 합니다.
// ============================================================
void SwapChain_Vulkan::EndFrame()
{
    if (!bFrameStarted)
    {
        throw std::runtime_error("[Vulkan] EndFrame(): BeginFrame()이 먼저 호출되어야 합니다.");
    }

    VkCommandBuffer cb = commandBuffers[currentImageIndex];

    // RenderPass 종료
    // finalLayout=PRESENT_SRC_KHR 전환이 자동으로 수행됩니다.
    // DX12의 ResourceBarrier(RenderTarget → Present)와 동일합니다.
    vkCmdEndRenderPass(cb);

    VK_CHECK(vkEndCommandBuffer(cb));
    bFrameStarted = false;
}

// ============================================================
// Present
//
// CommandBuffer를 GPU에 제출하고 화면에 출력합니다.
// BeginFrame() + EndFrame() 쌍이 먼저 호출되어야 합니다.
// ============================================================
void SwapChain_Vulkan::Present(bool bVsync)
{
    VkQueue vkQueue = vulkanQueue->GetQueue();

    // ---- CommandBuffer 제출 ----
    // imageAvailableSemaphore를 기다린 후 commandBuffer 실행.
    // 완료 시 renderFinishedSemaphore에 신호, inFlightFence에 신호.
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &commandBuffers[currentImageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &renderFinishedSemaphore;

    VK_CHECK(vkQueueSubmit(vkQueue, 1, &submitInfo, inFlightFence));

    // ---- Present ----
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &renderFinishedSemaphore;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &swapChain;
    presentInfo.pImageIndices      = &currentImageIndex;

    VkResult result = vkQueuePresentKHR(vkQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("[Vulkan] 스왑체인 만료. Resize() 필요.");
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("[Vulkan] vkQueuePresentKHR 실패.");
    }
}

// ============================================================
// Resize
// ============================================================
void SwapChain_Vulkan::Resize(uint32_t width, uint32_t height)
{
    // TODO:
    // 1. vkDeviceWaitIdle()
    // 2. CleanupSwapchain()
    // 3. QuerySwapchainSupport() 재호출
    // 4. CreateSwapchain() ~ CreateSyncObjects() 재생성
}

// ============================================================
// [내부] CreateSurface
// ============================================================
void SwapChain_Vulkan::CreateSurface(VkInstance instance, void* hWnd)
{
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hwnd      = static_cast<HWND>(hWnd);
    surfaceInfo.hinstance = GetModuleHandle(nullptr);
    VK_CHECK(vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface));
#else
    throw std::runtime_error("[Vulkan] 이 플랫폼은 Surface 생성을 지원하지 않습니다.");
#endif
}

// ============================================================
// [내부] QuerySwapchainSupport
// ============================================================
void SwapChain_Vulkan::QuerySwapchainSupport()
{
    VkPhysicalDevice phyDev = vulkanDevice->GetPhysicalDevice();

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phyDev, surface, &supportInfo.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phyDev, surface, &formatCount, nullptr);
    if (formatCount > 0)
    {
        supportInfo.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(phyDev, surface, &formatCount, supportInfo.formats.data());
    }

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phyDev, surface, &modeCount, nullptr);
    if (modeCount > 0)
    {
        supportInfo.presentModes.resize(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(phyDev, surface, &modeCount, supportInfo.presentModes.data());
    }

    if (supportInfo.formats.empty() || supportInfo.presentModes.empty())
    {
        throw std::runtime_error("[Vulkan] Surface가 스왑체인을 지원하지 않습니다.");
    }
}

// ============================================================
// [내부] CreateSwapchain
// ============================================================
void SwapChain_Vulkan::CreateSwapchain(const SwapChainDesc& desc)
{
    VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat();
    VkPresentModeKHR   presentMode   = ChoosePresentMode(true);
    VkExtent2D         swapExtent    = ChooseExtent(supportInfo.capabilities, desc.width, desc.height);

    imageFormat = surfaceFormat.format;
    extent      = swapExtent;

    uint32_t reqCount = supportInfo.capabilities.minImageCount + 1;
    if (supportInfo.capabilities.maxImageCount > 0)
        reqCount = std::min(reqCount, supportInfo.capabilities.maxImageCount);
    reqCount = std::min(reqCount, static_cast<uint32_t>(desc.bufferCount));
    reqCount = std::max(reqCount, supportInfo.capabilities.minImageCount);

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = surface;
    createInfo.minImageCount    = reqCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = swapExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform     = supportInfo.capabilities.currentTransform;
    createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode      = presentMode;
    createInfo.clipped          = VK_TRUE;
    createInfo.oldSwapchain     = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(vulkanDevice->GetDevice(), &createInfo, nullptr, &swapChain));
}

// ============================================================
// [내부] RetrieveImages
// ============================================================
void SwapChain_Vulkan::RetrieveImages()
{
    vkGetSwapchainImagesKHR(vulkanDevice->GetDevice(), swapChain, &imageCount, nullptr);
    if (imageCount > MAX_BUFFER_COUNT) { imageCount = MAX_BUFFER_COUNT; }
    vkGetSwapchainImagesKHR(vulkanDevice->GetDevice(), swapChain, &imageCount, images);
}

// ============================================================
// [내부] CreateImageViews
// ============================================================
void SwapChain_Vulkan::CreateImageViews()
{
    VkDevice vkDev = vulkanDevice->GetDevice();
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = images[i];
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = imageFormat;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;
        VK_CHECK(vkCreateImageView(vkDev, &viewInfo, nullptr, &imageViews[i]));
    }
}

// ============================================================
// [내부] CreateRenderPass
//
// Single color attachment, loadOp=CLEAR, finalLayout=PRESENT_SRC
// ============================================================
void SwapChain_Vulkan::CreateRenderPass()
{
    // ---- 색상 첨부 기술 ----
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = imageFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;   // 시작 시 지우기
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;  // 결과 저장
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;         // 이전 내용 무관
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // Present 가능 상태로 전환

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorAttachmentRef;

    // Subpass 의존성: 이미지 레이아웃 전환 타이밍 제어
    // DX12의 ResourceBarrier와 유사한 역할입니다.
    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &colorAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    VK_CHECK(vkCreateRenderPass(vulkanDevice->GetDevice(), &renderPassInfo, nullptr, &renderPass));
}

// ============================================================
// [내부] CreateFramebuffers
// ============================================================
void SwapChain_Vulkan::CreateFramebuffers()
{
    VkDevice vkDev = vulkanDevice->GetDevice();
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = &imageViews[i];
        fbInfo.width           = extent.width;
        fbInfo.height          = extent.height;
        fbInfo.layers          = 1;
        VK_CHECK(vkCreateFramebuffer(vkDev, &fbInfo, nullptr, &framebuffers[i]));
    }
}

// ============================================================
// [내부] AllocateCommandBuffers
// ============================================================
void SwapChain_Vulkan::AllocateCommandBuffers()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = vulkanQueue->GetCommandPool();
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = imageCount;
    VK_CHECK(vkAllocateCommandBuffers(vulkanDevice->GetDevice(), &allocInfo, commandBuffers));
}

// ============================================================
// [내부] CreateSyncObjects
// ============================================================
void SwapChain_Vulkan::CreateSyncObjects()
{
    VkDevice vkDev = vulkanDevice->GetDevice();

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // 첫 프레임 즉시 통과

    VK_CHECK(vkCreateSemaphore(vkDev, &semaphoreInfo, nullptr, &imageAvailableSemaphore));
    VK_CHECK(vkCreateSemaphore(vkDev, &semaphoreInfo, nullptr, &renderFinishedSemaphore));
    VK_CHECK(vkCreateFence(vkDev, &fenceInfo, nullptr, &inFlightFence));
}

// ============================================================
// [내부] CleanupSwapchain
// ============================================================
void SwapChain_Vulkan::CleanupSwapchain()
{
    VkDevice vkDev = vulkanDevice->GetDevice();

    if (inFlightFence           != VK_NULL_HANDLE) { vkDestroyFence(vkDev, inFlightFence, nullptr);              inFlightFence = VK_NULL_HANDLE; }
    if (renderFinishedSemaphore != VK_NULL_HANDLE) { vkDestroySemaphore(vkDev, renderFinishedSemaphore, nullptr); renderFinishedSemaphore = VK_NULL_HANDLE; }
    if (imageAvailableSemaphore != VK_NULL_HANDLE) { vkDestroySemaphore(vkDev, imageAvailableSemaphore, nullptr); imageAvailableSemaphore = VK_NULL_HANDLE; }

    if (commandBuffers[0] != VK_NULL_HANDLE && vulkanQueue)
    {
        vkFreeCommandBuffers(vkDev, vulkanQueue->GetCommandPool(), imageCount, commandBuffers);
        for (uint32_t i = 0; i < MAX_BUFFER_COUNT; ++i) { commandBuffers[i] = VK_NULL_HANDLE; }
    }

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        if (framebuffers[i] != VK_NULL_HANDLE) { vkDestroyFramebuffer(vkDev, framebuffers[i], nullptr); framebuffers[i] = VK_NULL_HANDLE; }
    }

    if (renderPass != VK_NULL_HANDLE) { vkDestroyRenderPass(vkDev, renderPass, nullptr); renderPass = VK_NULL_HANDLE; }

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        if (imageViews[i] != VK_NULL_HANDLE) { vkDestroyImageView(vkDev, imageViews[i], nullptr); imageViews[i] = VK_NULL_HANDLE; }
    }

    if (swapChain != VK_NULL_HANDLE) { vkDestroySwapchainKHR(vkDev, swapChain, nullptr); swapChain = VK_NULL_HANDLE; }
}

// ============================================================
// [유틸] 포맷·모드 선택
// ============================================================
VkSurfaceFormatKHR SwapChain_Vulkan::ChooseSurfaceFormat() const
{
    for (const auto& fmt : supportInfo.formats)
    {
        if (fmt.format     == VK_FORMAT_B8G8R8A8_UNORM &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return fmt;
        }
    }
    return supportInfo.formats[0];
}

VkPresentModeKHR SwapChain_Vulkan::ChoosePresentMode(bool bVsync) const
{
    if (!bVsync)
    {
        for (const auto& mode : supportInfo.presentModes)
        {
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) { return mode; }
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain_Vulkan::ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h) const
{
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return caps.currentExtent;
    }
    VkExtent2D actualExtent{ w, h };
    actualExtent.width  = std::clamp(actualExtent.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return actualExtent;
}
