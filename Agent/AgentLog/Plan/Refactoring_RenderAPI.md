# Refactoring_RenderAPI.md

> 작성일: 2026-05-15  
> 대상 세션: 2026-05-11 (RenderCommand 통합 · TransparentPass · SortKey 자동계산)  
> 목적: 급하게 작성된 코드의 문제점 검증 및 수정 항목 정리

---

## 검증 요약

| 번호 | 분류 | 위치 | 요약 | 상태 |
|---|---|---|---|---|
| 1 | **Critical** | Execute.cpp / AnimationRenderer | AnimationPass가 TransparentPass 이후 실행 | 완료 |
| 2 | **Medium** | TransparentPass::Execute | depth `DontCare` → 후속 패스 depth 신뢰 불가 (이식성) | 완료 |
| 3 | **Medium** | ForwardRenderer::AddPasses | DebugTextPass가 AnimationPass보다 먼저 실행 | 완료 |
| 4 | Performance | ForwardRenderer::Render | viewProj / lights CB 이중 업로드 | 완료 |
| 5 | Design | RenderScene::CalculateSortKey | depth 정규화 매직 넘버 100.0f | 완료 |
| 6a | Design | ITexture2DArray / ITextureUAV | HasAlpha() 제거 (호출 경로 없음) | 완료 |
| 6b | **Critical** | Texture_DirectX | HasAlpha() 미구현 → TransparentPass 완전 비동작 | 완료 |
| 7 | Cleanup | AnimationRenderer::Render | 스키닝 커맨드 없을 때도 PSO/CB 세팅 실행 | 완료 |
| 8 | Cleanup | ForwardRenderer::Render | 필터 조건 중복 (`|| cmd.skinnedMesh`) | 완료 |
| 9 | Refactoring | Texture_DirectX / TextureFloat / Texture2DArray | 업로드 인프라 코드 3중 중복 → ImmediateSubmit 래핑 | 완료 |

---

### 실제 패스 실행 순서 (Execute.cpp 기준)

```
Execute::OnRender()
  renderer.AddPasses()
    ├─ [1] SkyPass                     ← RT Clear + HDRi
    ├─ [2] ForwardPass                 ← 불투명 정적 메시 (DepthWrite)
    ├─ [3] TransparentPass             ← 반투명 정적 메시 (DepthRead)
    └─ [4] DebugTextPass               ← 디버그 텍스트 오버레이   ← 문제 #3

  animationRenderer.AddPasses()
    ├─ [5] BonePaletteComputePass      ← Compute: 본 팔레트 계산
    └─ [6] AnimationPass               ← 불투명 스키닝 메시 (DepthWrite)  ← 문제 #1

  renderer.AddDebugLinePasses()
    └─ [7] DebugLinePass               ← 디버그 라인
```

이상적인 순서:  
`SkyPass → ForwardPass(Opaque Static) → AnimationPass(Opaque Skinned) → TransparentPass → DebugLinePass → DebugTextPass`

---

### 수정 우선순위

| 우선순위 | 항목 | 이유 | 상태 |
|---|---|---|---|
| 1순위 | #6b HasAlpha() 구현 | TransparentPass 자체가 동작하지 않음, 모든 투명 관련 기능의 전제조건 | 완료 |
| 2순위 | #1 AnimationPass 순서 | 반투명 오브젝트 추가 시 즉시 시각적 오류 발생 | 완료 |
| 3순위 | #3 DebugTextPass 순서 | 디버그 작업 시 텍스트가 메시에 가려짐 | 완료 |
| 4순위 | #2 DontCare storeAction | Android 이식 시 depth 오류로 즉시 터지는 버그 | 완료 |
| 5순위 | #5 depth 정규화 | 씬 확장 시 정렬 품질 저하 | 완료 |
| 6순위 | #4 CB 이중 업로드 | 성능, 현재 씬에서 영향 미미 | 완료 |
| 7순위 | #9 ImmediateSubmit 래핑 | 코드 중복 제거 + 큐 생성 오버헤드 제거 | 완료 |
| 8순위 | #6a HasAlpha 제거 | 인터페이스 정리 | 완료 |
| 9순위 | #7, #8 | 소규모 코드 정리 | 완료 |

---


## 항목별 상세

---

### #1 [Critical] AnimationPass가 TransparentPass 이후 실행

**파일**: `Engine/Private/Execute.cpp:226-228`

```cpp
renderer.AddPasses(renderGraph, output, scene);          // ForwardPass, TransparentPass 포함
animationRenderer.AddPasses(renderGraph, output, scene); // AnimationPass ← TransparentPass 이후
renderer.AddDebugLinePasses(renderGraph, output);
```

**문제**:  
불투명 스키닝 메시(Kachujin 등)가 반투명 정적 메시 이후에 그려진다.  
반투명 오브젝트가 Kachujin과 겹칠 경우, 반투명 오브젝트가 먼저 FB에 기록된 후 Kachujin이 그 위를 덮어쓴다. 알파 블렌딩의 Back-to-Front 정렬 결과가 무효화된다.

**수정 방향**:  
`AnimationRenderer::AddPasses`를 두 단계로 분리한다.

```cpp
// AnimationRenderer에 메서드 추가
void AddComputePasses(RenderGraph& graph, const RenderScene& scene);   // BonePaletteComputePass
void AddGraphicsPasses(RenderGraph& graph, const FrameOutput& output,  // AnimationPass
                       uint32_t depthHandle);

// Execute.cpp 수정 후 순서
renderer.AddOpaquePasses(renderGraph, output, scene);          // SkyPass + ForwardPass
animationRenderer.AddComputePasses(renderGraph, scene);
animationRenderer.AddGraphicsPasses(renderGraph, output, depthHandle);
renderer.AddTransparentPasses(renderGraph, output, depthHandle); // TransparentPass
renderer.AddDebugLinePasses(renderGraph, output);
renderer.AddDebugTextPasses(renderGraph, output);              // 맨 마지막
```

`ForwardRenderer::AddPasses`도 `AddOpaquePasses` / `AddTransparentPasses`로 분리 필요.

---

### #2 [Medium] TransparentPass depth `DontCare` → 후속 패스 depth 신뢰 불가

**파일**: `Engine/Private/Render/ForwardRenderer.cpp:170`

```cpp
// TransparentPass::Execute
passDesc.depthStencil.storeAction = EStoreAction::DontCare;
```

**문제**:  
`TransparentPass` 이후에 `AnimationPass`와 `DebugLinePass` 모두 `ELoadAction::Load`로 depth를 로드한다.  
`DontCare`는 타일 기반 GPU(Android 등)에서 pass 종료 시 타일 데이터를 DRAM에 flush하지 않으므로,  
후속 패스에서 depth를 Load할 때 유효하지 않은 값이 될 수 있다.  
현재 데스크탑 DX12에서는 동작하나 이식성 측면에서 잘못된 코드다.

**수정**:
```cpp
passDesc.depthStencil.storeAction = EStoreAction::Store;
```

---

### #3 [Medium] DebugTextPass가 AnimationPass 전에 실행

**파일**: `Engine/Private/Render/ForwardRenderer.cpp:358-359`

```cpp
graph.AddPass<TransparentPass>(this, output, depthHandle);
if (debugTextRenderer)
    graph.AddPass<DebugTextPass>(this, output);  // ← [4] AnimationPass([6])보다 먼저
```

**문제**:  
디버그 패널 텍스트가 Kachujin이 렌더링되기 전에 FB에 기록된다.  
Kachujin과 텍스트가 겹치는 픽셀에서 Kachujin이 텍스트 위를 덮어쓴다.  
디버그 텍스트는 항상 최상단에 표시되어야 하므로 모든 geometry 이후에 위치해야 한다.

**수정**:  
`DebugTextPass` 등록을 `AddPasses`에서 분리하여 Execute.cpp에서 직접 순서 제어.

```cpp
// ForwardRenderer에 분리 메서드 추가
void AddDebugTextPasses(RenderGraph& graph, const FrameOutput& output);

// Execute.cpp에서 맨 마지막에 호출
renderer.AddDebugTextPasses(renderGraph, output);
```

---

### #4 [Performance] viewProj / lights 상수 버퍼 이중 업로드

**파일**: `Engine/Private/Render/ForwardRenderer.cpp:428-454`

```cpp
void ForwardRenderer::Render(ICommandList* cmdList, const RenderScene& scene, bool isTransparentPass)
{
    viewProjBuffer->Upload(...);  // Opaque 호출 시 1회
    lightsBuffer->Upload(...);    // Opaque 호출 시 1회
    ...
}
// TransparentPass에서 동일한 Render() 재호출 → 동일 데이터 GPU 재업로드
```

**문제**: 동일한 데이터를 CPU → GPU로 프레임당 2회 memcpy.

**수정**:
```cpp
void ForwardRenderer::Render(ICommandList* cmdList, const RenderScene& scene, bool isTransparentPass)
{
    if (!isTransparentPass)  // Opaque pass에서만 업로드
    {
        // viewProj, lights 업로드
    }
    pipelineState->Bind(cmdList);
    ...
}
```

---

### #5 [Design] CalculateSortKey depth 정규화 매직 넘버

**파일**: `Engine/Public/Scene/RenderScene.h:178`

```cpp
desc.depth = std::clamp(dist / 100.0f, 0.0f, 1.0f); // 0.1~100 range normalized
```

**문제**:  
100.0f는 카메라 far plane과 무관한 임의의 값이다.  
100유닛 이상 거리의 오브젝트는 모두 depth=1.0f로 처리되어 정렬 정밀도가 소실된다.  
씬 스케일이 커지면 거의 모든 오브젝트가 동일한 depth값을 가져 정렬이 무의미해진다.

**수정 방향**:  
`CameraData`에 `farZ` 필드를 추가하고, `SetCamera`에서 함께 설정.

```cpp
struct CameraData
{
    Matrix4x4 view;
    Matrix4x4 proj;
    float     eyePos[3] = {};
    float     farZ      = 1000.0f;  // 추가
};

// CalculateSortKey에서
desc.depth = std::clamp(dist / camera.farZ, 0.0f, 1.0f);
```

---

### #6a [Design] ITexture2DArray / ITextureUAV에 HasAlpha() 불필요 추가

**파일**: `Engine/Public/RHI/Texture/ITexture2DArray.h:27`, `ITextureUAV.h:28`

```cpp
virtual bool HasAlpha() const = 0;  // 호출하는 코드가 현재 없음
```

**문제**:  
`CalculateSortKey`는 `ITexture*` 타입만 받으므로 `ITexture2DArray::HasAlpha()`와 `ITextureUAV::HasAlpha()`를 호출하는 곳이 어디에도 없다. `ITexture`에 추가할 때 반사적으로 다른 텍스처 인터페이스에도 추가한 것으로 보임.

**수정**:  
`ITexture2DArray`, `ITextureUAV`에서 `HasAlpha()` 선언 제거.  
구현체 `Texture2DArray_DX.h`, `TextureUAV_DirectX.h`에서도 함께 제거.

---

### #6b [Critical] Texture_DirectX::HasAlpha() 미구현 → TransparentPass 완전 비동작

**파일**: `Engine/Private/RHI/DirectX/Texture/Texture_DirectX.h:14`

```cpp
bool HasAlpha() const override { return false; }  // 항상 false
```

**문제**:  
`CalculateSortKey`의 `desc.translucent = (texture != nullptr && texture->HasAlpha())`가 항상 `false`가 되어, `sortKey`의 bit[59]가 영구히 0으로 고정된다.  
결과적으로 `TransparentPass`에서 `translucent != isTransparentPass` → `false != true` 조건이 모든 커맨드에 성립하여, **TransparentPass는 현재 아무것도 그리지 않는다.**

**`TextureFloat_DirectX`는 `return false` 유지가 맞음**:  
HDR 환경맵은 알파 채널이 없으므로 `false` 고정이 의미상 올바르다.

**수정**:  
`Texture_DirectX`에서 로드 시 `stbi_load`의 원본 채널 수를 멤버로 저장하고 반환.

```cpp
// Texture_DirectX.h
class Texture_DirectX : public ITexture
{
    ...
    bool HasAlpha() const override { return hasAlpha; }
private:
    bool hasAlpha = false;
    ...
};

// Texture_DirectX.cpp — LoadFromFile에서
int width, height, channels;
stbi_uc* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
hasAlpha = (channels == 4);  // 원본이 알파 채널을 가졌는지 저장
```

`LoadFromMemory`는 호출자가 명시적으로 알파 포함 여부를 알고 있으므로, 파라미터로 전달받거나 기본값 `false`로 유지.

---

### #9 [Refactoring] 업로드 인프라 코드 3중 중복 → ImmediateSubmit 래핑

**파일**:
- `Engine/Private/RHI/DirectX/Texture/Texture_DirectX.cpp:66-105`
- `Engine/Private/RHI/DirectX/Texture/TextureFloat_DirectX.cpp:64-111`
- `Engine/Private/RHI/DirectX/Texture/Texture2DArray_DX.cpp:74-114`

**문제**:  
세 파일 모두 아래 패턴이 완전히 동일하게 반복된다.

```cpp
// 임시 큐/할당자/커맨드리스트 매번 생성
D3D12_COMMAND_QUEUE_DESC qDesc = { D3D12_COMMAND_LIST_TYPE_DIRECT };
ComPtr<ID3D12CommandQueue>        uploadQueue;
ComPtr<ID3D12CommandAllocator>    uploadAlloc;
ComPtr<ID3D12GraphicsCommandList> uploadCmd;
device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&uploadQueue));
device->CreateCommandAllocator(..., IID_PPV_ARGS(&uploadAlloc));
device->CreateCommandList(..., IID_PPV_ARGS(&uploadCmd));

// (파일마다 다른 복사 명령 기록)

// 실행 + GPU 완료 대기
ID3D12CommandList* lists[] = { uploadCmd.Get() };
uploadQueue->ExecuteCommandLists(1, lists);
ComPtr<ID3D12Fence> fence;
device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
uploadQueue->Signal(fence.Get(), 1);
fence->SetEventOnCompletion(1, event);
WaitForSingleObject(event, INFINITE);
CloseHandle(event);
```

추가 문제: 텍스처 로드마다 `D3D12CommandQueue`를 신규 생성한다. 큐 생성은 드라이버 레벨 작업으로 오버헤드가 크며, CLAUDE.md 규칙("커맨드 큐/리스트는 용도별로 분리")에도 어긋난다.

---

**수정 방향**:  
`RenderDevice_DirectX`에 `ImmediateSubmit` 메서드를 추가한다.  
Upload 전용 Copy 큐·할당자·커맨드리스트·펜스를 Device 초기화 시 한 번만 생성하고 캐싱한다.  
각 텍스처 클래스는 "복사 명령만 람다로 전달"하고, 나머지는 Device에 위임한다.

```cpp
// RenderDevice_DirectX.h — 추가
class RenderDevice_DirectX : public IRenderDevice
{
public:
    // 복사 명령을 람다로 받아 즉시 실행하고 GPU 완료까지 대기
    void ImmediateSubmit(std::function<void(ID3D12GraphicsCommandList*)> recordFn);

private:
    // Upload 전용 리소스 (Initialize() 시 생성, 이후 재사용)
    ComPtr<ID3D12CommandQueue>        copyQueue;      // Copy 타입
    ComPtr<ID3D12CommandAllocator>    copyAlloc;
    ComPtr<ID3D12GraphicsCommandList> copyCmdList;
    ComPtr<ID3D12Fence>               copyFence;
    uint64_t                          copyFenceValue = 0;
    HANDLE                            copyFenceEvent = nullptr;
};

// RenderDevice_DirectX.cpp — ImmediateSubmit 구현
void RenderDevice_DirectX::ImmediateSubmit(
    std::function<void(ID3D12GraphicsCommandList*)> recordFn)
{
    copyAlloc->Reset();
    copyCmdList->Reset(copyAlloc.Get(), nullptr);

    recordFn(copyCmdList.Get());

    copyCmdList->Close();
    ID3D12CommandList* lists[] = { copyCmdList.Get() };
    copyQueue->ExecuteCommandLists(1, lists);

    ++copyFenceValue;
    copyQueue->Signal(copyFence.Get(), copyFenceValue);
    copyFence->SetEventOnCompletion(copyFenceValue, copyFenceEvent);
    WaitForSingleObject(copyFenceEvent, INFINITE);
}
```

**사용 측 변경 예시** (`Texture_DirectX::Upload`):

```cpp
void Texture_DirectX::Upload(RenderDevice_DirectX* rdDevice,
                              const void* pixels, uint32_t width, uint32_t height)
{
    // ... 리소스 생성, 업로드 버퍼 준비 ...

    rdDevice->ImmediateSubmit([&](ID3D12GraphicsCommandList* cmd)
    {
        cmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            textureResource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd->ResourceBarrier(1, &barrier);
    });

    // SRV 등록
    auto srv = rdDevice->AllocateSRVDescriptor();
    // ...
}
```

`TextureFloat_DirectX`, `Texture2DArray_DX`도 동일한 방식으로 교체. 임시 큐/할당자/펜스/이벤트 생성 코드는 전부 제거.

---

### #7 [Cleanup] AnimationRenderer::Render 조기 반환 누락

**파일**: `Engine/Private/Render/AnimationRenderer.cpp:237-241`

```cpp
void AnimationRenderer::Render(ICommandList* cmdList, const RenderScene& scene)
{
    const auto& cmds = scene.GetCommands();
    if (cmds.empty()) return;  // 전체가 비어있는 경우만 확인
    // ...
    // PSO Bind, CB Upload 실행 후...
    for (const RenderCommand& cmd : cmds)
        if (!cmd.skinnedMesh) continue;  // 정적 메시는 루프마다 skip
```

**문제**:  
정적 메시 커맨드만 존재할 경우, `cmds.empty()` 체크를 통과한 뒤 PSO Bind, CB Upload가 실행된 후 루프에서 전부 `continue`된다. 불필요한 GPU 상태 변경.

**수정**:
```cpp
bool hasSkinnedCmd = std::any_of(cmds.begin(), cmds.end(),
    [](const RenderCommand& c) { return c.skinnedMesh != nullptr; });
if (!hasSkinnedCmd) return;
```

---

### #8 [Cleanup] ForwardRenderer::Render 필터 조건 중복

**파일**: `Engine/Private/Render/ForwardRenderer.cpp:476`

```cpp
if (!cmd.mesh || cmd.skinnedMesh)
    continue;
```

**문제**:  
`AddSkinnedMesh`는 항상 `cmd.mesh = nullptr`로 두므로, `cmd.skinnedMesh != nullptr`이면 이미 `!cmd.mesh`가 true다. `|| cmd.skinnedMesh`는 도달 불가능한 조건이다.

**수정**:
```cpp
if (!cmd.mesh)
    continue;
```

---

## 추상화 리팩토링 계획

> 작성일: 2026-05-15  
> 배경: DX12 구현 중 드러난 플랫폼 경계 침범 패턴을 정리. Vulkan / Android 지원 시 하나씩 수정하는 비용을 줄이기 위해 선행 설계.

---

### 항목 요약

| 번호 | 위치 | 문제 | 우선순위 | 상태 |
|---|---|---|---|---|
| A | `ICommandList` | 복사 명령(CopyTexture) 추상화 부재 | 1순위 | 완료 |
| B | `IRenderDevice` | `ImmediateSubmit` 추상화 부재 | 1순위 (A 선행) | 완료 |
| C | `AnimationRenderer` | `AddComputePasses` / `AddGraphicsPasses` 암묵적 순서 의존 | 2순위 | 완료 |
| D | `IRenderer` | 패스 단계 인터페이스 미분리 | 3순위 | 미착수 |

---

### A. `ICommandList` — 복사 명령 추상화 부재

**현재 상태:**

`ICommandList`에 드로우 / 배리어 / 디스패치 명령은 있으나, 리소스 복사 명령이 없다.  
결과적으로 텍스처 업로드 코드가 `ID3D12GraphicsCommandList*`를 직접 사용한다.

```cpp
// Texture_DirectX.cpp — ImmediateSubmit 람다 내부
cmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);  // DX12 전용 API 직접 호출
cmd->ResourceBarrier(1, &barrier);                            // DX12 전용 구조체 직접 사용
```

**문제:**  
`ICommandList` 추상화가 있음에도 텍스처 업로드 경로만 DX12 전용 타입으로 우회한다.  
Vulkan 구현 시 `Texture_Vulkan::Upload`에서 `vkCmdCopyBufferToImage`를 직접 호출하는 동일한 우회 패턴이 반복된다.

**수정 방향:**  
`ICommandList`에 버퍼 → 텍스처 복사 명령 추가.

```cpp
// ICommandList.h — 추가
// src: Upload heap 버퍼, dst: Default heap 텍스처, subresource 단위로 복사
virtual void CopyBufferToTexture(
    IRHIResource*  dstTexture,
    uint32_t       dstSubresource,
    IRHIResource*  srcBuffer,
    uint64_t       srcOffset,
    uint32_t       width,
    uint32_t       height,
    uint32_t       rowPitch) = 0;

// DX12 구현 — CommandList_DirectX.cpp
void CommandList_DirectX::CopyBufferToTexture(...)
{
    D3D12_TEXTURE_COPY_LOCATION dst = { ..., D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, dstSubresource };
    D3D12_TEXTURE_COPY_LOCATION src = { ..., D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, ... };
    cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
}
```

`ResourceBarrier`는 이미 `ICommandList::SetResourceBarrier`로 추상화되어 있으므로 별도 작업 불필요.

---

### B. `IRenderDevice` — `ImmediateSubmit` 추상화 부재

**현재 상태:**

```cpp
// RenderDevice_DirectX.h — DX12 전용, IRenderDevice 인터페이스에 없음
void ImmediateSubmit(std::function<void(ID3D12GraphicsCommandList*)> recordFn);
```

텍스처 구현체가 `static_cast<RenderDevice_DirectX*>(device)`로 캐스팅한 뒤 DX12 전용 메서드를 호출한다.

**문제:**  
- 람다 파라미터 `ID3D12GraphicsCommandList*`가 DX12 전용.
- `IRenderDevice` 인터페이스에 개념이 없어 Vulkan 구현이 동명 메서드를 제각각 정의하게 된다.

**수정 방향:**  
A에서 `CopyBufferToTexture`가 `ICommandList`에 추가된 후, `IRenderDevice`에 `ICommandList*`를 파라미터로 받는 `ImmediateSubmit`을 올릴 수 있다.

```cpp
// IRenderDevice.h — 추가
virtual void ImmediateSubmit(std::function<void(ICommandList*)> recordFn) = 0;

// RenderDevice_DirectX.cpp — 기존 구현 유지, 시그니처만 변경
void RenderDevice_DirectX::ImmediateSubmit(std::function<void(ICommandList*)> recordFn)
{
    uploadAlloc->Reset();
    uploadCmdList->Reset(uploadAlloc.Get(), nullptr);

    // ICommandList 래퍼를 스택에 임시 생성하여 람다에 전달
    CommandList_DirectX tempCmdList(uploadCmdList.Get());
    recordFn(&tempCmdList);

    uploadCmdList->Close();
    // ... Signal + Wait ...
}
```

**텍스처 업로드 측 변경 예시:**

```cpp
// Texture_DirectX::Upload — 변경 후
device->ImmediateSubmit([&](ICommandList* cmdList)
{
    cmdList->CopyBufferToTexture(
        textureResource.Get(), 0,
        uploadBuffer.Get(), footprint.Offset,
        width, height, footprint.Footprint.RowPitch);

    ResourceBarrier barrier{ textureResource.Get(),
        EResourceState::CopyDest, EResourceState::PixelShaderResource };
    cmdList->SetResourceBarrier(1, &barrier);
});
```

**구현 시 추가 사항:**

- `CommandList_DirectX`에 소유권 없는 래퍼 생성자 추가 (`handle->AddRef()` 후 `Attach`).
- `Common_DirectX.h`에 `RawResource` 헬퍼 추가 — 로컬 `ID3D12Resource*`를 `IRHIResource*`로 임시 래핑.
- `GetResourceState(EResourceState)` 함수를 switch-case에서 비트 플래그 방식으로 변경 — `PixelShaderResource | NonPixelShaderResource` 등 조합 상태 지원.

---

### C. `AnimationRenderer` — `AddComputePasses` / `AddGraphicsPasses` 암묵적 순서 의존

**현재 상태:**

```cpp
// AnimationRenderer.h
std::vector<SkinnedMesh*> frameUniqueMeshes; // AddComputePasses → AddGraphicsPasses 간 전달용

// Execute.cpp
animationRenderer.AddComputePasses(renderGraph, scene);   // frameUniqueMeshes 채움
animationRenderer.AddGraphicsPasses(renderGraph, output); // frameUniqueMeshes 소비
```

`AddComputePasses`를 먼저 호출하지 않으면 `AddGraphicsPasses`가 빈 리스트로 패스를 등록한다. 컴파일 타임에 순서를 강제할 수 없다.

**수정 방향:**  
`AddComputePasses`가 unique meshes를 반환하고, `AddGraphicsPasses`가 파라미터로 받는다. 멤버 캐싱을 제거한다.

```cpp
// AnimationRenderer.h — 변경 후
std::vector<SkinnedMesh*> AddComputePasses(RenderGraph& graph, const RenderScene& scene);
void AddGraphicsPasses(RenderGraph& graph, const FrameOutput& output,
                       const std::vector<SkinnedMesh*>& uniqueMeshes);

// Execute.cpp — 변경 후
auto meshes = animationRenderer.AddComputePasses(renderGraph, scene);
animationRenderer.AddGraphicsPasses(renderGraph, output, meshes);
```

`frameUniqueMeshes` 멤버 변수는 제거한다.

---

### D. `IRenderer` — 패스 단계 인터페이스 미분리

**현재 상태:**

`IRenderer`의 `AddPasses` 하나만 인터페이스에 노출되어 있고, `AddOpaquePasses` / `AddTransparentPasses` / `AddDebugTextPasses` 등은 구체 클래스의 메서드다. `Execute.cpp`가 구체 타입을 알아야 한다.

```cpp
class IRenderer
{
    virtual void AddPasses(RenderGraph&, const FrameOutput&, const RenderScene&) = 0;
    // AddOpaquePasses, AddTransparentPasses 등 없음
};
```

**문제:**  
`Execute.cpp`의 패스 등록 순서 제어 로직이 구체 타입에 묶여 있어, 렌더러 교체 시 Execute.cpp도 변경해야 한다.

**수정 방향 — 확정: SceneRenderer 합성 클래스**

`EPassStage enum` 방식으로는 Deferred 도입 후 내부 패스 순서(`GBuffer → Lighting → Sky → Transparent → Debug`)를 자연스럽게 표현할 수 없다.  
SceneRenderer가 내부 렌더러들을 조합하고 패스 순서를 보장하는 합성 클래스 방식으로 확정.

> **구체 설계 및 구현 계획** → [`DeferredRenderer_Design.md §7`](DeferredRenderer_Design.md)

---

### 권장 작업 순서

| 순서 | 항목 | 선행 조건 | 예상 영향 범위 | 상태 |
|---|---|---|---|---|
| 1 | C — AddComputePasses 반환값 | 없음 | AnimationRenderer.h/.cpp, Execute.cpp | 완료 |
| 2 | A — ICommandList CopyBufferToTexture | 없음 | ICommandList.h, CommandList_DirectX.cpp | 완료 |
| 3 | B — IRenderDevice ImmediateSubmit 추상화 | A 완료 | IRenderDevice.h, RenderDevice_DirectX.h/.cpp, Texture_DirectX.cpp × 3 | 완료 |
| — | Client World Object RenderScene 호출 구조 리팩토링 | 없음 | RenderScene.h, WorldObject.h, WorldModel.h | 완료 |
| — | **[Deferred Renderer 구현]** → [`DeferredRenderer_Design.md`](DeferredRenderer_Design.md) | B 완료 | GBufferRenderer, DeferredLightingRenderer, SceneRenderer | 미착수 |
| 4 | D — IRenderer 패스 단계 인터페이스 (SceneRenderer로 해결) | Deferred 구현 완료 | IRenderer.h, SceneRenderer, Execute.cpp | 미착수 |

---

## WorldObject — RenderScene 호출 구조 리팩토링

> 작업일: 2026-05-15  
> 배경: Client(Application)가 `scene.AddMesh` 등 Engine 내부 API를 직접 호출하는 구조적 문제를 해결.

### 사용자 요청 순서 및 분석

**1단계 — Client 설정 가능 필드 분류**

| 분류 | 필드 | 근거 |
|---|---|---|
| Client 설정 | `layer`, `materialID`, `objectID` | 오브젝트의 "의도" 표현 (렌더 순서, 배칭, 피킹) |
| Engine 자동 | `depth` | 카메라·월드 위치로 자동 계산 |
| Engine 자동 | `translucent` | `texture->HasAlpha()`로 자동 결정 |
| Engine 자동 | `pass` | 불투명/반투명 분류 Engine 내부 결정 |
| Engine 내부 | mesh, worldMatrix, texture 등 | WorldObject 서브클래스 소유 |

**2단계 — 구조체 설계**

```cpp
// RenderScene.h에 추가
struct RenderObjectDesc
{
    uint8_t       layer      = 0;   // 렌더링 레이어 (0-15)
    uint16_t      materialID = 0;   // PSO 배칭용 (향후 확장)
    SceneObjectID objectID   = 0;   // 피킹/선택용 ID
};
```

`instanceCount`는 `AnimController` 내부 데이터이므로 Client 필드로 노출하지 않는다.

**3단계 — WorldObject 계층 괴리 분석**

현재 구조의 두 패턴:

```
WorldAnimatedModel  (Engine)  — Impl_AddToScene: scene.AddSkinnedMesh 내부 호출
WO_Cube             (Application) — Impl_AddToScene: scene.AddMesh 직접 호출
```

- **Engine 측 문제**: `layer=0`이 소스 내부 고정. Client가 레이어를 바꾸려면 Engine 파일 수정 필요.
- **Application 측 문제**: `RenderScene::AddMesh` 시그니처를 Application이 직접 알아야 함. 파라미터 순서가 익명 값(`0, 0, 1`)으로 채워짐.
- **근본 원인**: `WO_Cube`가 `WorldObject`를 직접 상속하면서 "단일 정적 메시 Scene 등록" 책임을 Application이 구현. Engine의 `WorldModel`과 패턴 중복 + 불필요한 노출.

**4단계 — 수정**

핵심 결정: **`WO_Cube`를 `WorldModel` 상속으로 전환.** Scene 등록은 Engine(`WorldModel::Impl_AddToScene`)이 책임지고, Application은 `renderDesc` 선언만 한다.

변경 후 계층:

```
WorldObject           (Engine) — renderDesc 소유
  ├─ WorldModel       (Engine) — Impl_AddToScene: renderDesc → scene.AddMesh
  │     └─ WO_Cube   (App)    — Initialize만 정의, scene API 전혀 모름
  └─ WorldAnimatedModel (Engine) — Impl_AddToScene: renderDesc → scene.AddSkinnedMesh
        └─ WO_Kachujin (App)
```

Application 호출부:

```cpp
cube->renderDesc.layer = 1;  // "이 오브젝트는 레이어 1에 그려라"
// AddMesh 시그니처, 파라미터 순서 등 Engine 내부 일절 불필요
```

### 변경 파일 목록

| 파일 | 내용 |
|---|---|
| `Engine/Public/Scene/RenderScene.h` | `RenderObjectDesc` 추가, `AddMesh`/`AddSkinnedMesh` 시그니처 변경, `CalculateSortKey`에 `materialID` 파라미터 추가 |
| `Engine/Public/Object/WorldObject.h` | `class RenderScene` forward declare → `#include "Scene/RenderScene.h"` 전환, `renderDesc` 멤버 추가 |
| `Engine/Private/Object/WorldModel.cpp` | `Impl_AddToScene`에서 `renderDesc` 전달 |
| `Engine/Private/Object/WorldAnimatedModel.cpp` | `Impl_AddToScene`에서 `renderDesc` 전달 |
| `Application/Source/Public/WorldObjects/WO_Cube.h` | `WorldObject` → `WorldModel` 상속 전환, `mesh` 멤버·`Impl_AddToScene` 제거 |
| `Application/Source/Private/WorldObjects/WO_Cube.cpp` | `Initialize`에서 `meshes.push_back`, `Impl_AddToScene` 제거 |
| `Application/Source/Private/VaProgramName.cpp` | `cube->renderDesc.layer = 1` 설정 추가 |
