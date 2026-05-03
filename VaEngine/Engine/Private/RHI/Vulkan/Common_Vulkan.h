#pragma once

#include <stdexcept>
#include <string>

// ============================================================
// Vulkan 헤더 포함
//
// DX12의 Common_DirectX.h가 d3d12.h와 dxgi.h를 포함하듯,
// 여기서는 Vulkan 공통 헤더와 플랫폼별 Surface 확장을 포함합니다.
//
// Vulkan은 플랫폼 중립적인 API이지만, 화면 출력(Surface)은
// 플랫폼마다 별도의 확장(Extension)을 통해 처리합니다.
//   - Windows : VK_KHR_win32_surface
//   - Android : VK_KHR_android_surface
//   - Linux   : VK_KHR_xcb_surface (또는 xlib_surface)
// ============================================================

#ifdef _WIN32
    // vulkan_win32.h는 HWND, HINSTANCE 등 Windows 타입을 직접 사용합니다.
    // windows.h가 반드시 먼저 포함되어야 하므로 여기서 명시적으로 포함합니다.
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX  // windows.h의 min/max 매크로가 std::min/max와 충돌하는 것을 방지
    #endif
    #include <windows.h>
#endif

#include <vulkan/vulkan.h>

#ifdef _WIN32
    // Windows용 Surface 생성 구조체(VkWin32SurfaceCreateInfoKHR)와
    // vkCreateWin32SurfaceKHR() 함수를 사용하려면 이 헤더가 필요합니다.
    // windows.h → vulkan.h → vulkan_win32.h 순서를 지켜야 합니다.
    #include <vulkan/vulkan_win32.h>

    // CMake의 find_package(Vulkan)이 설정해 주는 매크로를 사용하는 대신,
    // 플랫폼 Surface Extension 이름을 상수로 정의해 둡니다.
    // SwapChain이 VkInstance 생성 시 이 Extension을 요청합니다.
    #define VK_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif

// ============================================================
// VK_CHECK 매크로
//
// DX12의 FAILED() 검사 + throw 패턴과 동일한 역할을 합니다.
// Vulkan API는 성공 시 VK_SUCCESS(0), 실패 시 음수 값을 반환합니다.
//
// 사용 예:
//   VK_CHECK(vkCreateDevice(...));
// ============================================================
#define VK_CHECK(expr)                                                          \
    do {                                                                        \
        VkResult _vkResult = (expr);                                            \
        if (_vkResult != VK_SUCCESS)                                            \
        {                                                                       \
            throw std::runtime_error(                                           \
                std::string("[Vulkan] " #expr " failed, VkResult = ") +         \
                std::to_string(static_cast<int>(_vkResult))                     \
            );                                                                  \
        }                                                                       \
    } while(false)
