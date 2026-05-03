# RHI 인터페이스 개선 계획

> 분석 기준: `Windows-Vulkan` 브랜치 (2026-05-04)  
> 목표: Engine 외부(Application, ExecWindows, ExecAndroid)에서 어떤 Graphics API가 사용되는지 몰라도 되도록 RHI 추상화를 완성한다.

---

## 현재 완성된 인터페이스

| 인터페이스 | DX12 구현체 | Vulkan 구현체 | 상태 |
|-----------|------------|--------------|------|
| `IRenderDevice` | `RenderDevice_DirectX` | `RenderDevice_Vulkan` | 완성 |
| `ICommandQueue` | `CommandQueue_DirectX` | `CommandQueue_Vulkan` | 부분 완성 |
| `ISwapChain` | `SwapChain_DirectX` | `SwapChain_Vulkan` | 부분 완성 |
| `IFence` | `Fence_DirectX` | `Fence_Vulkan` | 부분 완성 |

---

## 추상화가 불완전한 인터페이스

### 1. `ICommandList` — 선언만 있고 내용이 완전히 비어 있음

```cpp
// 현재 상태
class ICommandList
{
};  // 메서드 하나도 없음
```

`IRenderDevice::CreateCommandList()`도 DX12 구현체에서 빈 `unique_ptr`를 반환한다.  
렌더링 명령을 기록하고 제출하는 모든 과정이 추상화되지 않은 상태.

필요한 메서드:

| 메서드 | 역할 | DX12 대응 |
|--------|------|-----------|
| `Open()` | 커맨드 기록 시작 | `Reset()` + `Close()` 전 상태 |
| `Close()` | 기록 종료, GPU 제출 가능 상태로 전환 | `ID3D12GraphicsCommandList::Close()` |
| `Reset()` | 재사용을 위해 초기화 | `Reset(allocator, nullptr)` |
| `ResourceBarrier(resource, before, after)` | 리소스 상태 전환 | `CD3DX12_RESOURCE_BARRIER::Transition` |
| `SetRenderTargets(rtv, dsv)` | 렌더 타겟·뎁스 바인딩 | `OMSetRenderTargets` |
| `ClearRenderTarget(rtv, color)` | RT 클리어 | `ClearRenderTargetView` |
| `ClearDepthStencil(dsv, depth, stencil)` | 뎁스 클리어 | `ClearDepthStencilView` |
| `SetViewport(x, y, w, h, minZ, maxZ)` | 뷰포트 설정 | `RSSetViewports` |
| `SetScissorRect(left, top, right, bottom)` | 시저 설정 | `RSSetScissorRects` |
| `SetPipelineState(IPipelineState*)` | 파이프라인 바인딩 | `SetPipelineState` |
| `SetVertexBuffers(slot, IBuffer*)` | 버텍스 버퍼 바인딩 | `IASetVertexBuffers` |
| `SetIndexBuffer(IBuffer*)` | 인덱스 버퍼 바인딩 | `IASetIndexBuffer` |
| `DrawInstanced(vertexCount, ...)` | 드로우 콜 | `DrawInstanced` |
| `DrawIndexedInstanced(indexCount, ...)` | 인덱스 드로우 콜 | `DrawIndexedInstanced` |

---

### 2. `ISwapChain` — 렌더 타겟 접근 방법 없음

```cpp
// 현재 ISwapChain
class ISwapChain
{
    virtual void     Present(bool bVsync) = 0;
    virtual void     Resize(uint32_t w, uint32_t h) = 0;
    virtual uint32_t GetCurrentBackBufferIndex() const = 0;
    // 현재 백 버퍼를 렌더 타겟으로 얻는 방법이 없음
};
```

`SwapChain_DirectX`에는 `GetCurrentBackBufferRTV()`가 있지만 인터페이스에 없다.  
렌더링 코드에서 `ISwapChain*`을 `SwapChain_DirectX*`로 다운캐스팅해야만 RT를 얻을 수 있다.

추가 필요:

| 메서드 | 역할 |
|--------|------|
| `GetCurrentRenderTarget()` → `IRenderTarget*` | 현재 백 버퍼를 추상 렌더 타겟으로 반환 |
| `BeginFrame()` / `EndFrame()` | 프레임 시작·종료 명시 (CommandList 획득 포함 가능) |
| `GetWidth()` / `GetHeight()` | 해상도 조회 |

> `Resize()` 본문도 DX12 구현체에서 미완성(TODO 주석만 존재)이므로 구현 필요.

---

### 3. `ICommandQueue::ExecuteCommandLists()` — DX12 구현체 본문 비어 있음

```cpp
void CommandQueue_DirectX::ExecuteCommandLists(const std::vector<ICommandList*>& commandLists)
{
    // 본문 없음
}
```

`ICommandList`가 완성되면 함께 구현해야 한다.

---

### 4. `IFence` — 플랫폼 종속 이벤트 핸들 노출

```cpp
virtual void SetEventOnComplete(uint64_t value, void* inEventHandle) = 0;
```

`void* inEventHandle`은 사실상 Windows `HANDLE`이다.  
Android·Linux에서 `HANDLE` 개념이 없으므로 크로스 플랫폼에서 이 시그니처가 문제가 된다.

개선 방향:

| 방안 | 설명 |
|------|------|
| `WaitForValue(uint64_t value)` 추가 | CPU 블로킹 대기를 인터페이스 안에 캡슐화. 플랫폼 이벤트는 내부에서 처리 |
| `SetEventOnComplete` 제거 | 외부에서 이벤트 핸들을 전달하는 방식 자체를 폐기 |

---

## 누락된 인터페이스 (신규 추가 필요)

### 1. `IBuffer` — GPU 버퍼 추상화

현재 `Buffer_Vulkan`만 존재하며 DX12 대응 없음.  
Engine 외부에서 버퍼를 생성하려면 Vulkan 타입을 직접 써야 한다.

```cpp
class IBuffer
{
public:
    virtual ~IBuffer() = default;
    virtual void  Upload(const void* data, size_t size) = 0;  // CPU→GPU (dynamic)
    virtual void* GetNativeHandle() const = 0;                 // 내부 캐스팅용
};

struct BufferDesc
{
    size_t        size;
    EBufferUsage  usage;   // Vertex / Index / Constant / Staging
    EMemoryAccess access;  // DeviceLocal / HostVisible
};
```

`IRenderDevice`에 추가:
```cpp
virtual std::unique_ptr<IBuffer> CreateBuffer(const BufferDesc& desc) = 0;
```

---

### 2. `IPipelineState` — 그래픽스 파이프라인 + 셰이더 추상화

현재 `Pipeline_Vulkan`만 존재. DX12의 `ID3D12PipelineState + Root Signature`에 해당하는 추상화가 없다.

```cpp
class IPipelineState
{
public:
    virtual ~IPipelineState() = default;
};

struct PipelineStateDesc
{
    std::string  vertexShaderPath;
    std::string  pixelShaderPath;
    // 버텍스 레이아웃 (position, normal, uv 등 attribute 목록)
    // 블렌드 / 래스터라이저 / 뎁스 스텐실 상태
    // 렌더 타겟 포맷
};
```

`IRenderDevice`에 추가:
```cpp
virtual std::unique_ptr<IPipelineState> CreatePipelineState(const PipelineStateDesc& desc) = 0;
```

> 셰이더 포맷이 DX12(DXIL)와 Vulkan(SPIR-V)에서 다르다.  
> 단일 경로를 넘기면 내부에서 포맷을 선택하는 방식으로 설계하거나,  
> `IShader`를 별도로 두고 컴파일된 셰이더 객체를 `PipelineStateDesc`에 주입하는 방식을 검토.

> `IPipelineState`가 완성되면 셰이더 컴파일 스텝을 `ExecWindows`/`ExecAndroid`에서  
> `Engine/CMakeLists.txt`로 일원화할 수 있다.

---

### 3. `IRenderTarget` / `ITexture` — 렌더 타겟·텍스처 추상화

백 버퍼(`ID3D12Resource` / `VkImage`)를 외부에서 다룰 방법이 현재 없다.  
`ICommandList::SetRenderTargets()`가 인수로 받아야 할 타입이 여기에 해당한다.

```cpp
class IRenderTarget
{
public:
    virtual ~IRenderTarget() = default;
    // DX12: D3D12_CPU_DESCRIPTOR_HANDLE
    // Vulkan: VkImageView / VkFramebuffer
    // → 구체 타입은 ICommandList 구현체 내부에서만 사용
};
```

---

### 4. `IDescriptorSet` — 셰이더 리소스 바인딩 추상화

셰이더에 버퍼·텍스처를 연결하는 방식이 DX12(Root Descriptor / Descriptor Table)와 Vulkan(Descriptor Set)에서 크게 다르다.  
지금은 이 부분 전체가 `Pipeline_Vulkan` 내부에 하드코딩되어 있다.

```cpp
class IDescriptorSet
{
public:
    virtual ~IDescriptorSet() = default;
    virtual void BindBuffer(uint32_t binding, IBuffer* buffer) = 0;
    virtual void BindTexture(uint32_t binding, ITexture* texture) = 0;
};
```

---

## 설계상 개선이 필요한 사항

### 1. `EPixelFormat` 열거값이 DXGI_FORMAT 수치와 동일

```cpp
// 현재 Common_RHI.h
enum EPixelFormat : uint32_t
{
    R8G8B8A8_UNORM    = 28,  // DXGI_FORMAT_R8G8B8A8_UNORM = 28
    D24_UNORM_S8_UINT = 45,  // DXGI_FORMAT_D24_UNORM_S8_UINT = 45
    B8G8R8A8_UNORM    = 87,  // DXGI_FORMAT_B8G8R8A8_UNORM = 87
};
```

`SwapChain_DirectX`에서 `static_cast<DXGI_FORMAT>(desc.pixelFormat)`으로 직접 캐스팅하고 있다.  
Engine 내부 포맷과 API 포맷은 분리되어야 한다.

개선:
- `EPixelFormat` 열거값을 엔진 내부 의미 기준으로 재정의 (0부터 순번)
- 각 RHI 구현체 내부에 변환 함수 추가: `ToDXGIFormat(EPixelFormat)`, `ToVkFormat(EPixelFormat)`

---

### 2. `SwapChainDesc::bIsWindowed` — 모바일 플랫폼에 무의미

Android/iOS에는 windowed/fullscreen 전환 개념이 없다.  
`EDisplayMode { Windowed, Borderless, Fullscreen }`로 교체하고 플랫폼별로 무시하는 방향 검토.

---

## 요약표

| 항목 | 현재 상태 | 필요 작업 |
|------|----------|----------|
| `ICommandList` | 선언만 존재, 내용 없음 | 드로우/배리어/RT 설정 메서드 추가 |
| `ISwapChain` | RT 접근 방법 없음 | `GetCurrentRenderTarget()`, `GetWidth/Height()` 추가 |
| `ISwapChain::Resize()` | DX12 구현체 미완성 | 구현 필요 |
| `ICommandQueue::ExecuteCommandLists()` | DX12 구현체 본문 비어 있음 | `ICommandList` 완성 후 구현 |
| `IFence::SetEventOnComplete()` | Windows HANDLE 노출 | `WaitForValue()` 캡슐화로 교체 |
| `IBuffer` | 인터페이스 없음 | 신규 추가 |
| `IPipelineState` | 인터페이스 없음 | 신규 추가 |
| `IRenderTarget` | 인터페이스 없음 | 신규 추가 |
| `IDescriptorSet` | 인터페이스 없음 | 신규 추가 |
| `EPixelFormat` | DXGI 수치 그대로 사용 | 엔진 내부 값으로 분리, 변환 함수 추가 |
| `SwapChainDesc::bIsWindowed` | 모바일에서 무의미 | `EDisplayMode`로 교체 검토 |
