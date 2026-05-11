# CubeRenderer 구현 피드백

- 작성일: 2026-05-08
- 대상 계획: `Plan_CubeRenderer.md`
- 분석 기준: 현재 코드베이스 상태 + 멀티플랫폼/GPU API 다형성 관점

---

## 1. 현재 진척도 요약

| Phase | 항목 | 상태 |
|---|---|---|
| Phase 1 | HLSL 셰이더 작성 | ✅ `CubeShader.hlsl` 완료 |
| Phase 1 | Root Signature 생성 | ✅ `CreateRootSignature()` 구현됨 |
| Phase 1 | PSO 생성 | ✅ `CreatePipelineState()` 구현됨 |
| Phase 2 | 정점/인덱스 데이터 정의 | ✅ 8 vertices / 36 indices 정의 완료 |
| Phase 2 | 버퍼 생성 (`CreateBuffer`) | ❌ **`IGPUBuffer` 미정의 / `IRenderDevice::CreateBuffer` 없음** |
| Phase 2 | 데이터 업로드 | ❌ **`IGPUBuffer::Upload` 미정의** |
| Phase 2 | 버퍼 뷰 생성 | ❌ **`GetNativeHandle()` 메서드명 불일치** |
| Phase 3 | `Execute::OnRender` 연동 | ❌ **아직 연결 안 됨** |
| Phase 3 | `Execute::OnInitialize` 연동 | ❌ **아직 연결 안 됨** |

현재 **`CubeRenderer_DX12.cpp`는 컴파일되지 않는 상태**다.
`IGPUBuffer` 인터페이스와 `IRenderDevice::CreateBuffer()` 가 정의되어야 Phase 2 이후가 가능하다.

---

## 2. 블로킹 이슈 (컴파일 오류 원인)

### 2-1. `IGPUBuffer.h` 비어 있음

`Engine/Public/RHI/IGPUBuffer.h`가 현재 `#pragma once`만 있는 빈 파일이다.
`CubeRenderer_DX12.cpp`는 다음을 사용한다:

```cpp
// 사용 중인 타입/메서드 — 현재 미정의 상태
GPUBufferDesc { .size, .usage, .stride, .access }
EBufferUsage::VertexBuffer / IndexBuffer
EMemoryAccess::Upload
IGPUBuffer::Upload(void* data, size_t size)
IGPUBuffer::GetResource()   // IRHIResource* 반환
```

이 모든 것이 `IGPUBuffer.h`와 `Common_RHI.h`에 정의되어야 한다.

### 2-2. `IRenderDevice`에 `CreateBuffer()` 없음

`CubeRenderer_DX12.cpp:148`에서 `device->CreateBuffer(vbDesc)`를 호출하지만,
`IRenderDevice`에는 해당 메서드가 없다.
`CreateBuffer`를 `IRenderDevice` 인터페이스에 추가해야 한다.

### 2-3. `GetNativeHandle()` vs `GetNativeResource()` 불일치

`IRHIResource`에 정의된 메서드는 `GetNativeResource()`인데,
`CubeRenderer_DX12.cpp:161`은 `GetNativeHandle()`을 호출한다.
이름을 통일해야 한다.

---

## 3. 멀티플랫폼 / GPU API 다형성 관점 피드백

현재 계획과 구현은 DX12에서 동작하는 것을 목표로 하고 있어 단기적으로는 문제없다.
다만 이후 Vulkan 지원을 추가할 때 재작업이 필요해지는 부분들이 있다.
지금 당장 고칠 필요는 없으나 **설계 의도를 명확히 이해하고 진행**하는 것이 중요하다.

### 3-1. `CubeRenderer::Create()` — 팩토리 하드코딩 문제

```cpp
// CubeRenderer_DX12.cpp
std::unique_ptr<CubeRenderer> CubeRenderer::Create()
{
    return std::make_unique<CubeRenderer_DX12>(); // DX12 고정
}
```

- `CubeRenderer.h`는 `Engine/Public`에 있어 API 무관 인터페이스처럼 보인다.
- 그런데 `Create()`의 구현이 DX12 `.cpp`에 있으므로, Vulkan 빌드에서도 DX12 객체를 생성한다.
- 이것이 의도된 설계라면 문제없으나, 빌드 프리셋(`USE_DIRECTX` / `USE_VULKAN`)으로 분기해야 한다.

**권장 방향:**

```
CubeRenderer_DX12.cpp  → #ifdef USE_DIRECTX 로 감싸기
CubeRenderer_VK.cpp    → #ifdef USE_VULKAN 에서 다른 Create() 정의
```

또는 `IRenderDevice`를 팩토리로 사용하는 방식:

```cpp
// CubeRenderer를 IRenderDevice가 생성하는 패턴
std::unique_ptr<CubeRenderer> cubeRenderer = renderDevice->CreateCubeRenderer();
```

이 경우 `CubeRenderer.h`를 `ISwapChain`처럼 `I` 접두사(`ICubeRenderer`)로 변경하는 것이 일관성 있다.

### 3-2. DX12 타입 직접 캐스팅 — 추상화 누수

```cpp
// CubeRenderer_DX12.cpp
RenderDevice_DirectX* dxDevice = static_cast<RenderDevice_DirectX*>(device);
ID3D12Device* d3d12Device = dxDevice->GetDevice();

CommandList_DirectX* dxCmd = static_cast<CommandList_DirectX*>(cmdList);
ID3D12GraphicsCommandList* d3d12Cmd = dxCmd->GetHandle();
```

이 캐스팅 자체는 DX12 구현체(`Private/RHI/Agent/DirectX/`) 내부에서 이루어지므로
**설계 원칙상 허용되는 패턴**이다. RHI 인터페이스 레이어는 통과했고, DX12 구현 코드에서
DX12 타입으로 다운캐스팅하는 것은 다른 DX12 구현체(`SwapChain_DirectX`, `CommandList_DirectX`)도 동일하게 사용하는 패턴이다.

단, Vulkan 구현체(`CubeRenderer_VK.cpp`)를 작성할 때는 같은 `Initialize(IRenderDevice*)`를
`VulkanRenderDevice*`로 캐스팅하는 형태가 되므로, 이 패턴이 반복되는 점은 감안해야 한다.

### 3-3. `IGPUBuffer` 설계 시 Vulkan 호환성 고려 필요

DX12에서는 버퍼 뷰(`D3D12_VERTEX_BUFFER_VIEW`, `D3D12_INDEX_BUFFER_VIEW`)를
GPU 주소(`GetGPUVirtualAddress()`)로 채운다.

Vulkan에서는 `vkCmdBindVertexBuffers`, `vkCmdBindIndexBuffer`에
`VkBuffer` 핸들과 오프셋을 직접 전달한다. "뷰" 개념이 없다.

따라서:
- `IGPUBuffer`의 공용 인터페이스는 **버퍼 뷰 구조체를 노출하지 않아야** 한다.
- 뷰 구조체(`D3D12_VERTEX_BUFFER_VIEW` 등)는 DX12 구현체(`GPUBuffer_DX12`) 내부에만 존재해야 한다.
- `Draw()` 내에서 뷰를 채우는 로직도 DX12 구현체에 위치해야 한다.

**현재 `CubeRenderer_DX12.h`의 구조는 이 점을 이미 잘 반영하고 있다:**
```cpp
// DX12 전용 멤버 — 올바르게 Private/DX12 구현체에 위치
D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
D3D12_INDEX_BUFFER_VIEW  indexBufferView{};
```

다만 뷰를 채우는 코드(`CreateMesh()` 후반부)가 현재 `IGPUBuffer::GetResource()->GetNativeHandle()`에 의존하므로,
이 메서드 체인의 반환 타입이 `void*`일 경우 `static_cast<ID3D12Resource*>` 가 필요하다.
이는 현재 `TRHIResource<T>` 설계와 연결된 부분이므로, `GetNativeResource()`의 반환 타입을 명확히 결정할 때 함께 정리하는 것이 좋다.

### 3-4. Viewport / Scissor 미설정

`Draw()` 내에 `RSSetViewports`, `RSSetScissorRects` 호출이 없다.
DX12에서는 PSO에 포함되지 않고 커맨드 리스트에 매 프레임 설정해야 한다.
이를 `ICommandList` 인터페이스에 추가하면 Vulkan(`vkCmdSetViewport`, `vkCmdSetScissor`)과도 대응된다.

### 3-5. 셰이더 경로 하드코딩

```cpp
D3DCompileFromFile(L"../VaEngine/Engine/Private/RHI/DirectX/_Shaders/CubeShader.hlsl", ...)
```

- 실행 시 작업 디렉토리에 의존하는 상대 경로다. 빌드 환경/플랫폼에 따라 실패할 수 있다.
- Vulkan에서는 SPIR-V 바이너리를 사용하므로 셰이더 로딩 방식이 완전히 다르다.
- 현 단계에서 즉각 변경할 필요는 없으나, 셰이더 관리 방식을 CMake 빌드 시 복사 또는 절대 경로 매크로 방식으로 개선하는 것을 권장한다.

### 3-6. `#pragma comment(lib, "d3dcompiler.lib")`

소스 파일 내 링크 지시어는 CMakeLists.txt의 `target_link_libraries`로 이동하는 것이 좋다.
다른 DX12 구현체들도 동일 방식을 사용 중인지 확인 필요.

---

## 4. 다음 작업 순서 (권장)

다음 항목들을 순서대로 해결해야 `CubeRenderer`가 컴파일되고 실행된다.

1. **`IGPUBuffer` 인터페이스 정의** (`Engine/Public/RHI/IGPUBuffer.h`)
   - `GPUBufferDesc`, `EBufferUsage`, `EMemoryAccess` 열거형/구조체 추가 (`Common_RHI.h` 또는 `IGPUBuffer.h`)
   - `Upload(void*, size_t)`, `GetResource()` 순수 가상 메서드 추가

2. **`IRenderDevice::CreateBuffer()` 추가** (`Engine/Public/RHI/IRenderDevice.h`)
   - `[[nodiscard]] virtual std::unique_ptr<IGPUBuffer> CreateBuffer(const GPUBufferDesc& desc) = 0;`

3. **`GPUBuffer_DX12` 구현체 작성** (`Engine/Private/RHI/DirectX/`)
   - Upload Heap 기반 버퍼 생성 + CPU→GPU 데이터 복사

4. **`RenderDevice_DirectX::CreateBuffer()` 구현** (`RenderDevice_DirectX.cpp`)

5. **`GetNativeResource()` 이름 통일** — `IRHIResource.h` 기준으로 `CubeRenderer_DX12.cpp` 수정

6. **`Execute.cpp` Phase 3 연동**
   - `OnInitialize`: `cubeRenderer = CubeRenderer::Create(); cubeRenderer->Initialize(renderDevice.get());`
   - `OnRender`: Clear 명령 직후 `cubeRenderer->Draw(commandList.get());`

7. **Viewport / Scissor 설정** — `ICommandList`에 추가 또는 `Draw()` 내에 임시 직접 호출

---

## 5. 구조 정리 요약

```
[현재 상태]
Public/RHI/Agent/CubeRenderer.h         ✅ 인터페이스 완성
Private/RHI/Agent/DirectX/
  CubeRenderer_DX12.h                    ✅ 클래스 선언 완성
  CubeRenderer_DX12.cpp                  ⚠️  구현됨 — 단, 컴파일 불가 (IGPUBuffer 미정의)
Private/RHI/DirectX/_Shaders/
  CubeShader.hlsl                        ✅ 완성
Public/RHI/IGPUBuffer.h                  ❌ 비어 있음 (최우선 작업)
IRenderDevice.h                          ❌ CreateBuffer() 없음
Execute.cpp / Execute.h                  ❌ CubeRenderer 연동 없음
```
