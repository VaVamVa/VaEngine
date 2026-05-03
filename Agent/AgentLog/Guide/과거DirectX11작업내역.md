## Q3: DirectX11 초기화 과정 분석

**질문:**
"D:\class\DX3D\lesson\DirectX11_3D_19" 의 폴더를 읽고, DirectX11의 시작 방식을 알려줄 수 있어?

**답변:**

### DirectX11 초기화 플로우

#### 1. 진입점 (WinMain)
`Main.cpp:159`
```cpp
int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR param, int command)
{
    GRAPHICS_DESC desc;
    desc.AppName = L"DirectX11 3D";
    desc.Instance = instance;
    desc.bWindowed = true;
    desc.bVsync = false;
    desc.Width = 1280;
    desc.Height = 720;
    desc.Background = DEFAULT_COLOR;
    Graphics::SetDesc(desc);

    Main* main = new Main();
    Window* window = new Window(desc);
    WPARAM wParam = window->Run(main);  // 메인 루프
}
```

---

#### 2. Window 생성
`Window.cpp:7`
```cpp
Window::Window(GRAPHICS_DESC desc)
{
    // WNDCLASSEX 등록
    WNDCLASSEX wndClass;
    wndClass.lpfnWndProc = (WNDPROC)WndProc;
    wndClass.lpszClassName = desc.AppName.c_str();
    RegisterClassEx(&wndClass);

    // 윈도우 생성
    CreateWindowEx(...);

    // 윈도우 표시
    ShowWindow(desc.Handle, SW_SHOWNORMAL);
}
```

---

#### 3. DirectX11 초기화 (Graphics::Create)
`Window.cpp:81`
```cpp
WPARAM Window::Run(IExecute* main)
{
    Graphics::Create();  // DirectX 초기화! (싱글톤)
    ...
}
```

`Graphics.cpp:9`
```cpp
Graphics::Graphics()
{
    SetGPUInfo();                      // ① GPU 정보 수집
    CreateSwapChainAndDevice();        // ② Device, SwapChain 생성
    CreateBackBuffer(width, height);   // ③ RTV, DSV 생성
}
```

---

#### 4. SetGPUInfo() - GPU 정보 수집
`Graphics.cpp:78`
```cpp
void Graphics::SetGPUInfo()
{
    // DXGI Factory 생성
    CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);

    // GPU(Adapter) 열거
    factory->EnumAdapters(0, &adapter);

    // 모니터(Output) 열거
    adapter->EnumOutputs(0, &output);

    // 디스플레이 모드 리스트 가져오기
    output->GetDisplayModeList(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_ENUM_MODES_INTERLACED,
        &modeCount,
        displayModeList
    );

    // 주사율(RefreshRate) 찾기
    for (uint i = 0; i < modeCount; i++) {
        if (displayModeList[i].Width == desc.Width &&
            displayModeList[i].Height == desc.Height) {
            numerator = displayModeList[i].RefreshRate.Numerator;
            denominator = displayModeList[i].RefreshRate.Denominator;
        }
    }

    // GPU 메모리 크기와 이름 저장
    adapter->GetDesc(&adapterDesc);
    gpuMemorySize = adapterDesc.DedicatedVideoMemory / (1024 * 1024);
    gpuDescription = adapterDesc.Description;
}
```

---

#### 5. CreateSwapChainAndDevice() - 핵심 생성
`Graphics.cpp:142`
```cpp
void Graphics::CreateSwapChainAndDevice()
{
    // Swap Chain 설정
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    ZeroMem(swapChainDesc);

    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = handle;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = desc.bWindowed;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    // Feature Level 지정
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        ...
    };

    // ★ DirectX11 핵심 초기화 함수
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,                           // 어댑터 (NULL = 기본 GPU)
        D3D_DRIVER_TYPE_HARDWARE,       // 하드웨어 가속
        NULL,                           // 소프트웨어 래스터라이저
        creationFlags,                  // 생성 플래그
        featureLevels,                  // 지원할 Feature Level 배열
        1,                              // Feature Level 개수
        D3D11_SDK_VERSION,              // SDK 버전
        &swapChainDesc,                 // SwapChain 설정
        &swapChain,                     // [OUT] SwapChain
        &device,                        // [OUT] Device
        NULL,                           // [OUT] 실제 Feature Level
        &deviceContext                  // [OUT] DeviceContext
    );
}
```

---

#### 6. CreateBackBuffer() - RTV, DSV 생성
`Graphics.cpp:208`
```cpp
void Graphics::CreateBackBuffer(float width, float height)
{
    // ① SwapChain에서 백버퍼 가져오기
    ID3D11Texture2D* pbackBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pbackBuffer);

    // ② Render Target View 생성
    device->CreateRenderTargetView(pbackBuffer, NULL, &renderTargetView);
    SAFE_RELEASE(pbackBuffer);

    // ③ Depth Stencil Texture 생성
    D3D11_TEXTURE2D_DESC dstDesc = { 0 };
    dstDesc.Width = (UINT)width;
    dstDesc.Height = (UINT)height;
    dstDesc.MipLevels = 1;
    dstDesc.ArraySize = 1;
    dstDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dstDesc.SampleDesc.Count = 1;
    dstDesc.Usage = D3D11_USAGE_DEFAULT;
    dstDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    device->CreateTexture2D(&dstDesc, NULL, &backBuffer);

    // ④ Depth Stencil View 생성
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    ZeroMem(dsvDesc);
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    device->CreateDepthStencilView(backBuffer, &dsvDesc, &depthStencilView);

    // ⑤ Output Merger에 바인딩
    SetRenderTarget(renderTargetView, depthStencilView);
}
```

---

#### 7. 시스템 초기화 및 메인 루프
`Window.cpp:77`
```cpp
WPARAM Window::Run(IExecute* main)
{
    mainExecute = main;

    Graphics::Create();     // DirectX 초기화
    Mouse::Create();
    Keyboard::Create();
    Time::Create();
    GUI::Create();
    Context::Create();      // Viewport, Projection 관리
    Lighting::Create();

    mainExecute->Initialize();  // 사용자 코드 초기화

    // 메시지 루프
    MSG msg = { 0 };
    while (true) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            MainRender();  // 렌더링
        }
    }

    // 정리
    mainExecute->Destroy();
    Lighting::Delete();
    Context::Delete();
    GUI::Delete();
    Time::Delete();
    Keyboard::Delete();
    Mouse::Delete();
    Graphics::Delete();

    return msg.wParam;
}
```

---

#### 8. 렌더링 루프
`Window.cpp:164`
```cpp
void Window::MainRender()
{
    // 업데이트
    Time::Get()->Update();
    Keyboard::Get()->Update();
    Mouse::Get()->Update();
    GUI::Get()->Update();
    Context::Get()->Update();
    mainExecute->Update();          // 게임 로직

    mainExecute->PreRender();       // 렌더링 전처리

    // 렌더링
    Graphics::Get()->SetRenderTarget();
    Graphics::Get()->Clear(desc.Background);
    {
        Context::Get()->Render();
        mainExecute->Render();      // 실제 렌더링
        DebugLine::Get()->Render();
        mainExecute->PostRender();  // 후처리
        GUI::Get()->Render();       // UI 렌더링
    }
    Graphics::Get()->Present();     // 화면에 출력
}
```

---

### DirectX11 초기화 순서 요약

| 단계 | 작업 | 핵심 함수/클래스 |
|------|------|------------------|
| 1 | 진입점 설정 | `WinMain` |
| 2 | 윈도우 생성 | `Window::Window()` |
| 3 | GPU 정보 수집 | `Graphics::SetGPUInfo()` |
| 4 | Device/Context/SwapChain 생성 | `D3D11CreateDeviceAndSwapChain()` |
| 5 | RenderTargetView 생성 | `CreateRenderTargetView()` |
| 6 | DepthStencilView 생성 | `CreateDepthStencilView()` |
| 7 | 시스템 초기화 | Mouse, Keyboard, Time, GUI 등 |
| 8 | 메시지 루프 | `PeekMessage()` + `MainRender()` |
| 9 | 렌더링 루프 | Update → Render → Present |

---

### DirectX11의 특징 (이 코드 기준)

**장점:**
1. **간단한 초기화**: `D3D11CreateDeviceAndSwapChain()` 한 번 호출로 핵심 객체 생성
2. **자동 관리**: Device가 많은 것을 자동으로 처리
3. **즉시 렌더링**: DeviceContext에 명령하면 바로 처리
4. **단순한 리소스 관리**: View를 만들고 바로 바인딩

**DirectX12와의 차이점:**
1. **암묵적 동기화**: `Present()`가 알아서 동기화 처리
2. **단일 Context**: DeviceContext 하나만 사용 (멀티스레딩 제한적)
3. **자동 상태 관리**: PSO(Pipeline State Object) 없이 상태를 개별적으로 설정
4. **간단한 백버퍼**: SwapChain에서 바로 가져와서 RTV 생성
5. **Command List 없음**: DeviceContext에 직접 명령 기록

---

### 핵심 객체 관계도

```
WinMain
  └─> Window::Window()           (윈도우 생성)
  └─> Window::Run()
       ├─> Graphics::Create()    (싱글톤)
       │    ├─> SetGPUInfo()                (DXGI로 GPU 정보 수집)
       │    ├─> CreateSwapChainAndDevice()  (Device/Context/SwapChain)
       │    └─> CreateBackBuffer()          (RTV/DSV 생성)
       │
       ├─> mainExecute->Initialize()
       └─> 메시지 루프
            └─> MainRender()
                 ├─> Update()
                 ├─> Render()
                 └─> Present()
```

---
