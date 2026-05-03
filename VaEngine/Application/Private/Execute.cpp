#include "Execute.h"
#include <stdexcept>

#include "RHI/RHILoader.h"
#include "Interfaces/IRenderDevice.h"
#include "Interfaces/ICommandQueue.h"
#include "Interfaces/IFence.h"
#include "Interfaces/ISwapChain.h"
#include "Interfaces/ICommandList.h"

#include "Math/MatrixUtil.h"
#include "Geometry/CubeMesh.h"

#ifdef USE_VULKAN
    #include "SwapChain_Vulkan.h"
    #include "RenderDevice_Vulkan.h"
    #include "CommandQueue_Vulkan.h"
#endif

// ============================================================
// OnInitialize
// ============================================================
void Execute::OnInitialize(NativeDisplayInfo inDisplayInfo)
{
    displayInfo = inDisplayInfo;

    if (renderDevice = RHI::CreateRenderDevice())
    {
        renderDevice->Initialize();

        CommandQueueDesc queueDesc = {};
        queueDesc.type  = static_cast<uint32_t>(ECommandQueueType::Graphics);
        queueDesc.flags = 0;
        commandQueue = renderDevice->CreateCommandQueue(queueDesc);

        frameFence = renderDevice->CreateFence();

        SwapChainDesc swapChainDesc   = {};
        swapChainDesc.displayInfo     = displayInfo;
        swapChainDesc.RegisteredQueue = commandQueue.get();
        swapChainDesc.bufferCount     = 2;
        swapChainDesc.width           = 1280;
        swapChainDesc.height          = 720;
        swapChainDesc.pixelFormat     = EPixelFormat::R8G8B8A8_UNORM;
        swapChainDesc.bIsWindowed     = true;
        swapChain = renderDevice->CreateSwapChain(swapChainDesc);

#ifdef USE_VULKAN
        InitVulkan3DScene();
        lastFrameTime = std::chrono::steady_clock::now();
#endif
        return;
    }

    throw std::runtime_error("Failed to create Render Device");
}

// ============================================================
// OnDestroy
// ============================================================
void Execute::OnDestroy()
{
#ifdef USE_VULKAN
    ShutdownVulkan3DScene();
#endif

    commandList.reset();
    swapChain.reset();
    frameFence.reset();
    commandQueue.reset();

    renderDevice->Shutdown();
    renderDevice.reset();
}

// ============================================================
// OnLoop
// ============================================================
void Execute::OnLoop()
{
    OnUpdate();
    OnRender();
    OnPostRender();

    swapChain->Present(true);
}

void Execute::OnSuspend() {}
void Execute::OnResume()  {}

// ============================================================
// OnUpdate — 매 프레임 논리 갱신
// ============================================================
void Execute::OnUpdate()
{
#ifdef USE_VULKAN
    auto now      = std::chrono::steady_clock::now();
    float dt      = std::chrono::duration<float>(now - lastFrameTime).count();
    lastFrameTime = now;

    // 초당 90도 회전 (π/2 rad/s)
    rotationAngle += dt * 1.5708f;
    if (rotationAngle > 6.2832f) rotationAngle -= 6.2832f;

    // MVP 행렬 계산 후 Uniform Buffer에 업로드
    Pipeline_Vulkan::UniformBufferObject ubo{};
    MatUtil::RotateY(rotationAngle, ubo.model);
    MatUtil::LookAt(0.0f, 1.0f, 3.0f,  0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,  ubo.view);
    MatUtil::Perspective(0.7854f, 1280.0f / 720.0f, 0.1f, 100.0f, ubo.projection);

    uniformBuffer->Upload(&ubo, sizeof(ubo));
#endif
}

// ============================================================
// OnRender — GPU 렌더링 명령 기록
// ============================================================
void Execute::OnRender()
{
#ifdef USE_VULKAN
    auto* vkSwapChain = static_cast<SwapChain_Vulkan*>(swapChain.get());
    VkCommandBuffer cb = vkSwapChain->BeginFrame();

    VkExtent2D extent = vkSwapChain->GetExtent();

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(extent.width);
    viewport.height   = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cb, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;
    vkCmdSetScissor(cb, 0, 1, &scissor);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

    VkBuffer     vb     = vertexBuffer->GetBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cb, indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(cb, CubeMesh::INDEX_COUNT, 1, 0, 0, 0);

    vkSwapChain->EndFrame();
#endif
}

// ============================================================
// OnPostRender
// ============================================================
void Execute::OnPostRender()
{
}

// ============================================================
// [Vulkan 전용] InitVulkan3DScene
// ============================================================
#ifdef USE_VULKAN
void Execute::InitVulkan3DScene()
{
    auto* vkDevice = static_cast<RenderDevice_Vulkan*>(renderDevice.get());
    auto* vkQueue  = static_cast<CommandQueue_Vulkan*>(commandQueue.get());
    auto* vkSC     = static_cast<SwapChain_Vulkan*>(swapChain.get());

    VkPhysicalDevice physDev = vkDevice->GetPhysicalDevice();
    VkDevice         dev     = vkDevice->GetDevice();
    VkQueue          queue   = vkQueue->GetQueue();
    VkCommandPool    pool    = vkQueue->GetCommandPool();

    vertexBuffer = std::make_unique<Buffer_Vulkan>();
    vertexBuffer->CreateDeviceLocal(physDev, dev, pool, queue,
        CubeMesh::VERTICES, sizeof(CubeMesh::VERTICES),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    indexBuffer = std::make_unique<Buffer_Vulkan>();
    indexBuffer->CreateDeviceLocal(physDev, dev, pool, queue,
        CubeMesh::INDICES, sizeof(CubeMesh::INDICES),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    uniformBuffer = std::make_unique<Buffer_Vulkan>();
    uniformBuffer->CreateHostVisible(physDev, dev,
        sizeof(Pipeline_Vulkan::UniformBufferObject),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // Android에서는 displayInfo.Display에 AAssetManager*가 담겨 있습니다.
    // PC(Windows)에서는 nullptr이며, Pipeline이 파일 시스템 로딩으로 분기합니다.
#ifdef ANDROID
    void* assetMgr = displayInfo.Display; // AAssetManager*
#else
    void* assetMgr = nullptr;
#endif

    pipeline = std::make_unique<Pipeline_Vulkan>();
    pipeline->Initialize(physDev, dev,
        vkSC->GetRenderPass(), vkSC->GetExtent(),
        "Shaders/Vulkan/mesh.vert.spv",
        "Shaders/Vulkan/mesh.frag.spv",
        assetMgr);

    descriptorSet = pipeline->CreateDescriptorSet(
        uniformBuffer->GetBuffer(),
        sizeof(Pipeline_Vulkan::UniformBufferObject));
}

// ============================================================
// [Vulkan 전용] ShutdownVulkan3DScene
// ============================================================
void Execute::ShutdownVulkan3DScene()
{
    auto* vkDevice = static_cast<RenderDevice_Vulkan*>(renderDevice.get());
    if (vkDevice) vkDeviceWaitIdle(vkDevice->GetDevice());

    pipeline.reset();
    uniformBuffer.reset();
    indexBuffer.reset();
    vertexBuffer.reset();
}
#endif // USE_VULKAN
