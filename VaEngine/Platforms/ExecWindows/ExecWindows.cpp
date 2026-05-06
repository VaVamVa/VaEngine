#include "framework.h"
#include "ExecWindows.h"

#include "Execute.h"

namespace 
{
    std::unique_ptr<IExecute> execEngine = nullptr;
}

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    constexpr LPCWSTR kClassName    = L"VaEngine";
    constexpr LPCWSTR kWindowName   = L"VaEngine";

    WNDCLASSEXW wcexw                = {};
    wcexw.cbSize                     = sizeof(WNDCLASSEX);
    wcexw.style                      = CS_HREDRAW | CS_VREDRAW;
    wcexw.lpfnWndProc                = WndProc;
    wcexw.hInstance                  = hInstance;
    wcexw.hCursor                    = LoadCursor(nullptr, IDC_ARROW);
    wcexw.hbrBackground              = GetSysColorBrush(COLOR_WINDOW);
    wcexw.lpszClassName              = kClassName;
    RegisterClassExW(&wcexw);

    HWND hWnd = CreateWindowExW(
        0,
        kClassName, kWindowName,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, /* TODO: 추후 custom 가능하게 */
        nullptr, nullptr, hInstance, nullptr
    );
    if (hWnd == nullptr)
    {
        return -1;
    }

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    execEngine = std::make_unique<Execute>();
    
    NativeDisplayInfo displayInfo = NativeDisplayInfo();
    displayInfo.platform = NativeDisplayInfo::EPlatform::Windows;
    displayInfo.Display = hInstance;
    displayInfo.Handle = hWnd;
    execEngine->OnInitialize(displayInfo);
    
    MSG msg = {};

    while (true)
    {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        else
        {
            execEngine->OnLoop();
        }
    }

    execEngine->OnDestroy();
    execEngine.reset();

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_ACTIVATE:
        {
            if (execEngine)
            {
                LOWORD(wParam) == WA_INACTIVE ?
                    execEngine->OnSuspend() : execEngine->OnResume();
            }
            break;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            break;
        }
        default:
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    return 0;
}
