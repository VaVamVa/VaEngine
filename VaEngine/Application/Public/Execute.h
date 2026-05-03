#pragma once

#include "Interfaces/IExecute.h"

#ifdef USE_VULKAN
    #include "Pipeline_Vulkan.h"
    #include "Buffer_Vulkan.h"
    #include <vulkan/vulkan.h>
    #include <chrono>
#endif

// ============================================================
// Execute
//
// 앱(게임, 시뮬레이션 등) 로직의 진입 클래스입니다.
// IExecute 인터페이스를 구현하며, 플랫폼 루프에서 호출됩니다.
//
// 씬 구성, 오브젝트 조작(이동·회전·색상 등), 업데이트 로직이
// 이 클래스와 하위 시스템에 위치합니다.
//
// RHI는 IRenderDevice/ISwapChain 등 추상 인터페이스로만 접근하므로
// DX12·Vulkan 백엔드를 교체해도 이 클래스는 변경이 불필요합니다.
// ============================================================
class Execute : public IExecute
{
public:
    void OnInitialize(NativeDisplayInfo displayInfo) override;
    void OnDestroy() override;

    void OnLoop() override;
    void OnSuspend() override;
    void OnResume()  override;

protected:
    void OnUpdate()     override;
    void OnRender()     override;
    void OnPostRender() override;

private:
    // NativeDisplayInfo 보관 (HWND/HINSTANCE 등 플랫폼 타입은 직접 멤버로 갖지 않음)
    NativeDisplayInfo displayInfo{};

    // ---- RHI 추상 인터페이스 (DX12/Vulkan 공통) ----
    std::unique_ptr<class IRenderDevice> renderDevice;
    std::unique_ptr<class ICommandQueue> commandQueue;
    std::unique_ptr<class IFence>        frameFence;
    std::unique_ptr<class ISwapChain>    swapChain;
    std::unique_ptr<class ICommandList>  commandList;

#ifdef USE_VULKAN
    // ---- Vulkan 3D 렌더링 리소스 ----
    std::unique_ptr<Pipeline_Vulkan> pipeline;
    std::unique_ptr<Buffer_Vulkan>   vertexBuffer;
    std::unique_ptr<Buffer_Vulkan>   indexBuffer;
    std::unique_ptr<Buffer_Vulkan>   uniformBuffer;
    VkDescriptorSet                  descriptorSet = VK_NULL_HANDLE;

    float rotationAngle = 0.0f;
    std::chrono::time_point<std::chrono::steady_clock> lastFrameTime;

    void InitVulkan3DScene();
    void ShutdownVulkan3DScene();
#endif
};
