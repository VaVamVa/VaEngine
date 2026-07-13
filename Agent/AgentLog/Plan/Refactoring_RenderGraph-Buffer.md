# RenderGraph 리소스 상태 캡슐화 리팩터링 (BaseRHIResource)

작성일: 2026-07-12
상태: **구현 완료 · 로그로 검증 완료(2026-07-12)**
참조: `Plan_RenderQuality.md > 추가로 확인된 문제`(1차 수정, `RegisterPersistentResource`/`Locator<RenderGraph>` — 이번 리팩터링으로 전부 대체·제거됨)

---

## 검증 완료 (2026-07-12)

빌드 후 실행 로그(`_Files/Log/2026-07-12_06-17-01.log`)로 계획서가 원래 검증하고자 했던 3가지 항목을 전부 확인했다. 검증에는 `RenderGraph::Compile()`에 추가한 `TraceResourceTransition()`(처음 5프레임만 전수 기록 후 정지, 처음 보는 (리소스,이전상태,이후상태) 조합이 나타나면 재개 — `VA_DEBUG`로 완전히 감싸져 있어 꺼두면 컴파일에서 제거됨)과 `Execute::OnLoop()`의 `--- Frame N start ---` 프레임 경계 마커를 사용했다.

- **BonePalette 첫 프레임 UAV 쓰기**: `[리소스] Common -> UnorderedAccess`가 Frame 1에 정확히 기록됨 — `Buffer_DirectX::Create()`가 실제 초기 상태(Common)를 정확히 보고하고, 첫 컴퓨트 패스가 이를 정확히 `UnorderedAccess`로 전환하는 배리어가 걸림(과거엔 이 배리어 자체가 누락되어 있었음).
- **Shadow Map 프레임 간 재사용**: 트랜지언트 Shadow Depth가 `DepthWrite -> NonPixelShaderResource -> DepthWrite -> ...`로 프레임을 넘나들며 두 상태만 정확히 순환 — 과거 크래시(0x87A)를 유발했던 "매 프레임 무조건 DepthWrite로 리셋" 패턴이 재발하지 않음을 확인.
- **백버퍼 Present 전환(더블 버퍼링 포함)**: 두 백버퍼 인스턴스가 각각 `Common -> RenderTarget -> Present`(최초 1회) 이후 `Present -> RenderTarget -> Present`로 정확히 순환 — `PresentTransitionPass`만으로 `ImportResource` 없이도 더블 버퍼링이 정상 동작함을 확인.
- Frame 6부터 Frame 1580(로그 종료 시점)까지 `[ResourceState]` 로그가 재발하지 않음(리사이즈·신규 오브젝트 스폰 등 "새 패턴"이 없었던 정상 실행) — 로그 크기도 무제한 기록 대비 정상 범위(수백~수천 줄)로 유지됨.

이로써 `BaseRHIResource` 리팩터링의 핵심 목표(리소스가 스스로 상태를 소유, `RenderGraph::Compile()`이 유일한 mutator, 등록/해제 개념 완전 제거)가 실제 런타임에서 의도대로 동작함을 코드 리뷰뿐 아니라 실행 증거로 확인했다.

---

## 배경

Shadow Map 크래시(0x87A) 수정 과정에서 `RegisterPersistentResource`/`UnregisterPersistentResource` + `Locator<RenderGraph>` 설계를 도입했으나, 실제 Unreal Engine RDG 소스(`E:\UE\UnrealEngine\Engine\Source\Runtime\RenderCore`)를 확인한 결과 더 근본적인 설계가 있음을 확인했다: **리소스 상태(`EResourceState`)를 리소스 객체 자신이 멤버로 소유**하고(`FRHIViewableResource::TrackedAccess`), RenderGraph는 그 상태를 별도 자료구조 없이 포인터로 직접 읽고 쓴다.

이 방식을 채택하면:
- `RegisterPersistentResource`/`UnregisterPersistentResource`/`persistentResourceStates` 맵/`Locator<RenderGraph>` — **전부 불필요해짐** (등록이라는 개념 자체가 사라짐. 리소스가 생성되면 상태도 함께 생기고, 파괴되면 함께 사라짐 — 별도 생명주기 동기화 불필요).
- `TransientDepthEntry::lastKnownState`(Shadow Map 수정 때 추가한 필드) — 리소스 자신의 상태로 흡수되어 제거.
- `RenderGraph::Compile()`의 `resourceStates` 맵 자체도 대부분 불필요해짐 — barrier 계산이 리소스 포인터에서 직접 읽고 쓰는 방식으로 단순화.
- 부수적으로, `Buffer_DirectX`(BonePalette 등)의 실제 초기 상태가 지금까지 잘못 가정되어 있던 것(아래 참조)도 함께 바로잡힌다.

---

## 확정된 설계

### 1. `IRHIResource` → `BaseRHIResource` 이름 변경 (이번 건에 한해 파일명 변경 허용됨)

CLAUDE.md 네이밍 규칙(`I` = 순수 가상, `Base` = 공통 구현 포함 가상 클래스)에 따라, 상태 필드+구현을 갖게 되는 시점부터 `IRHIResource`는 더 이상 순수 인터페이스가 아니다.

- `Engine/Public/RHI/IRHIResource.h` → `Engine/Public/RHI/BaseRHIResource.h` (IDE 이름변경)
- `struct IRHIResource` → `struct BaseRHIResource`
- **`TRHIResource<T>` 템플릿 삭제** — 엔진 전체에서 `IRHIResource.h` 자기 자신 외에는 참조하는 곳이 없는 죽은 코드로 확인됨(grep 결과 1개 파일만 매치).

```cpp
// BaseRHIResource.h (신규)
#pragma once

#include "RHI/Common_RHI.h"

class RenderGraph;  // friend 선언용 전방 선언 — RHI가 Render 계층을 #include하지 않음(계층 방향 유지)

struct BaseRHIResource
{
public:
    virtual ~BaseRHIResource() = default;

    virtual void* GetNativeResource() const = 0;

    // 현재 추적 상태 조회 — 제약 없음(디버그 패널 등에서 자유롭게 조회 가능)
    EResourceState GetTrackedState() const { return trackedState; }

protected:
    // Compile() 안에서만 호출할 것(barrier 계획 단계). 각 구현체의 Create()에서 실제 초기 상태를
    // 설정할 때도 파생 클래스 자격으로 사용(protected이므로 상속 계층 전체에서 접근 가능).
    void SetTrackedState(EResourceState state) { trackedState = state; }

private:
    friend class RenderGraph;  // Compile()의 transition()에서 SetTrackedState() 호출 허용
    EResourceState trackedState = EResourceState::Uninitialized;  // 각 구현체 Create()가 실제 상태로 갱신 필수
};
```

**Setter/Getter 접근 제어 확정**: `GetTrackedState()`는 public, `SetTrackedState()`는 `protected` + `friend class RenderGraph`(그래프의 자동 배리어 계획 전용 — `private`로는 안 됨: 파생 클래스인 각 구현체의 `Create()`가 자기 상태를 못 씀). **`ForceTrackedState()` 같은 public 이스케이프 해치는 두지 않는다** — 아래 6번 항목 참조. `RenderGraph`를 우회해 상태를 직접 바꿀 수 있는 통로를 열어두면, 상태 변경이 항상 `Compile()`을 거친다는 불변식이 깨지고, 호출부마다 "이 리소스가 그래프 관리 대상인지"를 개별 판단해야 해 자동 배리어의 이점이 퇴색된다(구현 착수 직전 재검토 과정에서 발견 — 아래 배경 참조).

**설계 수정 배경(2026-07-12 재검토)**: 최초 설계는 스왑체인 백버퍼의 Present 전환을 `Execute.cpp`가 그래프 밖에서 수동 배리어로 처리하고, 그 결과를 `ForceTrackedState()`로 그래프 쪽에 "알려주는" 방식이었다. 그러나 `RenderGraph::Execute()`(`RenderGraph.cpp:125-138`)를 보면 각 Pass는 `preBarriers`가 자동 삽입된 뒤 `Execute()`가 호출되는 구조이므로, **`Execute()` 본문이 비어있는 Pass를 추가하는 것만으로 Present 전환도 다른 모든 리소스와 동일하게 `Compile()`이 자동 계산**할 수 있다. 근본 원인은 "그래프 밖 전환을 그래프에 알리는 방법이 없다"가 아니라 "Present 전환이 애초에 그래프 밖에서 일어난다"는 것이었으므로, 후자를 없애는 쪽(6번 항목 `PresentTransitionPass`)이 더 근본적인 해법이다.

### 2. 각 구현체 `Create()`에서 실제 초기 상태를 정확히 설정

| 구현체 | 실제 D3D12 초기 상태 | 근거 |
|---|---|---|
| `ColorBuffer_DirectX` | `RenderTarget` | `CreateCommittedResource(..., D3D12_RESOURCE_STATE_RENDER_TARGET, ...)` — 텍스처는 InitialState가 실제 적용됨 |
| `DepthBuffer_DirectX`(내부 `DepthResource`) | `DepthWrite` | 동일(텍스처) |
| `TextureUAV_DirectX`(hdrOut) | `UnorderedAccess` | 동일(텍스처) — `CreateCommittedResource(..., D3D12_RESOURCE_STATE_UNORDERED_ACCESS, ...)` 확인 |
| `Buffer_DirectX`(BonePalette 등) | **`Common`** (요청한 usage 플래그와 무관하게 항상) | **D3D12는 버퍼 리소스에 한해 InitialState 파라미터를 무시하고 항상 COMMON으로 생성한다** (디버그 레이어 경고로 확인: "Buffers are effectively created in state D3D12_RESOURCE_STATE_COMMON"). `Buffer_DirectX::Create()`가 `D3D12_RESOURCE_STATE_UNORDERED_ACCESS`를 요청해도 실제로는 무시됨 |
| `SwapChain_DirectX::BackBufferResource` | `Common` | DXGI 스왑체인 버퍼의 관행적 초기 상태(=Present와 동일 비트값 0) |
| `RawResource`(`Common_DirectX.h`, `ImmediateSubmit` 전용 임시 래퍼) | 기본값(`Common`) 그대로 | `RenderGraph::Compile()`의 barrier 계산을 절대 거치지 않으므로 무해 |

**중요 발견(기존 버그)**: `Buffer_DirectX`가 만드는 UAV 버퍼(BonePalette)는 지금까지 `EResourceState::UnorderedAccess`를 초기 상태로 가정해왔다(`ImportResource`/`RegisterPersistentResource` 양쪽 다). 실제로는 `Common`이 맞고, **최초 프레임의 BonePaletteCompute UAV 쓰기 전에 `Common→UnorderedAccess` 배리어가 실제로 필요한데 지금까지 생략되고 있었다.** 이번 리팩터링으로 자동 수정됨.

**Upload 힙 버퍼(CB들, viewProjBuffer/tonemapBuffer 등)**: `DeclareResources()`에 전혀 등장하지 않는다(barrier 추적 대상이 아님, `Map()/Unmap()`으로 직접 CPU 쓰기 + root CBV로 직접 바인딩). `trackedState` 기본값이 무엇이든 절대 조회되지 않으므로 신경 쓸 필요 없음.

### 3. `RenderGraph::Compile()` 단순화

```cpp
// 기존 (맵 기반)
auto it = resourceStates.find(decl.resource);
assert(it != resourceStates.end() && "...");
if (it != resourceStates.end() && it->second != decl.requiredState)
{
    entry.preBarriers.push_back({ decl.resource, it->second, decl.requiredState });
    it->second = decl.requiredState;
}

// 변경 후 (리소스 직접 조회)
EResourceState current = decl.resource->GetTrackedState();
if (current != decl.requiredState)
{
    entry.preBarriers.push_back({ decl.resource, current, decl.requiredState });
    decl.resource->SetTrackedState(decl.requiredState);
}
```

- `resourceStates`/`persistentResourceStates` 맵 — **완전히 제거**.
- `RegisterPersistentResource`/`UnregisterPersistentResource` — **완전히 제거**.
- `TransientDepthEntry::lastKnownState` — **제거** (리소스 자신의 `trackedState`가 대체).
- `RenderGraph::GetCurrentState(resource)` — **완전히 제거(확정)**. 아무도 호출하지 않게 됨 — 백버퍼 Present 전환도 6번 항목의 `PresentTransitionPass`로 그래프 안에 편입되므로, `Execute.cpp`가 상태를 직접 조회할 이유 자체가 사라진다.
- `RenderGraph::ImportResource(resource, state)` — **완전히 제거(확정)**. 마찬가지로 아무도 호출하지 않게 됨 — 백버퍼는 매 프레임 `Create()`(최초 1회)와 `Compile()`(매 프레임)만으로 상태가 항상 정확하게 유지되므로, 프레임 시작 시점에 그래프에게 "지금 상태"를 따로 알려줄 필요가 없다.

### 3-1. 백엔드 무관 안전장치 — `EResourceState::Uninitialized` 센티널

기존에 추가했던 "등록 누락 시 assert"는 등록 개념 자체가 사라져 무의미해졌고, D3D12 디버그 레이어에 기대는 것도 RHI 추상화 원칙에 어긋난다(Vulkan은 다른 검증 체계). 대신:

```cpp
// Common_RHI.h — 기존 비트플래그 값들과 겹치지 않는 새 값 추가
enum class EResourceState : uint32_t
{
    Uninitialized           = 1 << 16,  // BaseRHIResource 기본값 — Create()가 실제 상태로 갱신해야 함
    Common                  = 0,
    ...
};
```

```cpp
// BaseRHIResource.h
EResourceState trackedState = EResourceState::Uninitialized;
```

```cpp
// RenderGraph.cpp — transition() 진입부
EResourceState current = decl.resource->GetTrackedState();
assert(current != EResourceState::Uninitialized &&
       "BaseRHIResource: Create()가 SetTrackedState()로 실제 초기 상태를 설정하지 않음");
```

이러면 새 리소스 구현체가 초기 상태 설정을 빠뜨렸을 때, GPU에 커맨드가 올라가기 전 `Compile()` 시점에 즉시 실패한다. D3D12/Vulkan 어느 백엔드든 동일하게 동작 — 백엔드별 디버그 레이어보다 먼저, 더 이르게(GPU 실행 시점이 아니라 컴파일 시점) 잡아낸다.

### 3-2. 스레드 안전성 — 추가 설계 불필요, 다만 계약을 명시할 것

`SetTrackedState()`는 `RenderGraph::Compile()`의 `transition()` 단 한 곳에서만 호출되고, `Execute()`(커맨드 기록)는 `trackedState`를 전혀 읽거나 쓰지 않는다(이미 계획된 `preBarriers`를 그대로 제출할 뿐). 이는 곧:

- **`Compile()`(배리어 계획)은 항상 단일 스레드·순차 실행**해야 한다 — 패스 간 상태 의존성이 등록 순서에 있으므로 이는 상태를 어디(맵 vs 리소스)에 두든 동일하게 요구되는 제약이지, 이번 설계가 추가하는 제약이 아니다.
- **`Execute()`(커맨드 기록)는 이미 병렬화에 안전하다** — 패스별로 다른 스레드가 각자의 커맨드 리스트에 기록해도 `trackedState`에 동시 접근이 없다. GPU 제출 순서는 `ExecuteCommandLists` 호출 순서(Execute.cpp가 단일 스레드로 통제)로 보장되므로 어느 스레드가 기록했는지는 무관하다. → Log Phase 2의 "병렬처리 탐색" 항목은 이 설계를 그대로 활용해 "Compile 순차 + Execute 병렬" 패턴으로 바로 적용 가능하다.
- 이 전제("`SetTrackedState`는 `Compile()` 안에서만 호출")는 런타임 체크보다 **주석으로 계약을 명시**한다(`BaseRHIResource`가 `RenderGraph`를 알게 만들면 오히려 결합도가 올라감). `SetTrackedState()` 선언부와 `RenderGraph::Compile()` 양쪽에 주석 필수.
- 남는 유일한 한계는 **여러 `RenderGraph` 인스턴스가 리소스를 공유하는 미래 시나리오**(예: 멀티 뷰포트) — 이땐 맵 방식이었어도 동일하게 안 풀리는 문제라 이번 설계로 인해 더 나빠지는 것은 아니다. 발생 시점에 별도 처리.

### 4. `Locator<RenderGraph>` — 도입하지 않음

BonePalette 등록 문제 자체가 사라지므로(리소스가 스스로 상태를 가짐), `WorldAnimatedModel.cpp`에 `Locator<RenderGraph>`를 심을 필요가 없다. 이전 턴에서 설계만 하고 아직 구현하지 않았으므로 되돌릴 것도 없음.

### 5. 호출부 정리 (이번 세션에 추가했던 것들 원복)

- `SceneRenderer.cpp::AddPasses` — `RegisterPersistentResource(...)` 5줄 삭제(등록 자체가 불필요해짐).
- `BloomRenderer.cpp::AddPasses` — `RegisterPersistentResource(...)` 2줄 삭제.
- `AnimationRenderer.cpp::AddComputePasses` — `RegisterPersistentResource(...)` 삭제.
- `Execute.cpp` — 프레임 시작부의 `renderGraph.ImportResource(backBuffer, EResourceState::Present)` 호출과, 프레임 끝의 수동 `ResourceBarrier presentBarrier{...}; commandList->SetResourceBarrier(...)` 코드 블록을 **통째로 삭제**한다. 대신 `sceneRenderer.AddPasses(...)` 직후·`renderGraph.Compile(...)` 직전에 `renderGraph.AddPass<PresentTransitionPass>(backBuffer);` 한 줄만 추가한다. 나머지는 전부 6번 항목(`PresentTransitionPass`)이 기존 `Compile()`/`Execute()` 메커니즘을 그대로 타면서 자동 처리한다.

### 6. `PresentTransitionPass` — 백버퍼 Present 전환의 그래프 편입

**배경**: 최초 설계는 Present 전환을 `Execute.cpp`가 그래프 밖에서 수동 배리어로 걸고, 그 결과를 `ForceTrackedState()`로 리소스에 직접 통보하는 방식이었다. 그러나 이 방식은 호출부(`Execute.cpp`)가 "이 리소스는 그래프가 관리하는지 아닌지"를 스스로 판단해야 했고, 상태 변경 경로가 `Compile()`과 `ForceTrackedState()` 두 갈래로 갈라져 "상태는 항상 `Compile()`을 거쳐서만 바뀐다"는 불변식이 깨졌다 — 자동 배리어 시스템의 가치를 스스로 훼손하는 설계였다(재검토 계기: 사용자 지적).

`RenderGraph::Execute()`(`RenderGraph.cpp:125-138`)를 보면 각 Pass는 `preBarriers`가 먼저 자동 삽입된 뒤 `Execute()`가 호출된다. 즉 **`Execute()` 본문이 비어있는 Pass**도 `Compile()`의 배리어 계산(`RenderGraph.cpp:79-106`)을 동일하게 거친다 — Present 전환에 특별한 처리가 필요 없다는 뜻이다.

```cpp
// Engine/Private/Render/PresentTransitionPass.h (신규, Execute.cpp 전용이므로 Private)
#pragma once
#include "Render/IRenderPass.h"
#include "RHI/BaseRHIResource.h"

// 스왑체인 백버퍼를 Present 상태로 전환하는 것 자체를 그래프의 마지막 Pass로 등록한다.
// 실제 배리어는 Compile()이 다른 Pass와 완전히 동일한 경로로 계산·삽입하므로,
// 호출부가 "이 리소스가 그래프 관리 대상인지" 따로 판단할 필요가 없다.
class PresentTransitionPass : public IRenderPass
{
public:
    explicit PresentTransitionPass(BaseRHIResource* backBuffer) : backBuffer(backBuffer) {}

    void DeclareResources(std::vector<PassResourceDecl>& reads,
                           std::vector<PassResourceDecl>& writes) const override
    {
        writes.push_back({ backBuffer, EResourceState::Present });
    }

    void Execute(ICommandList*, const RenderScene&) override {}  // 배리어는 이미 preBarriers로 처리됨

private:
    BaseRHIResource* backBuffer;
};
```

**효과**:
- `Execute.cpp`의 수동 배리어 코드, `GetCurrentState()`/`ImportResource()` 호출이 전부 사라진다 — 백버퍼가 다른 리소스와 동일한 취급을 받는다.
- `BaseRHIResource`에 `ForceTrackedState()` 같은 public 이스케이프 해치를 둘 필요가 없다 — `SetTrackedState()`는 `protected + friend RenderGraph`로 완전히 닫힌 채로 유지.
- 향후 스크린샷 캡처 등 "그래프 밖처럼 보이는" 케이스도 동일 패턴(전용 트리비얼 Pass)으로 해결 — 지금 쓰이지 않는 이스케이프 해치를 미리 만들어두지 않는다(CLAUDE.md: 사용되지 않는 추상화는 추가하지 않는다).

---

## 수정 대상 파일 전체 목록

### 이름 변경 + 핵심 로직
- `Engine/Public/RHI/IRHIResource.h` → `Engine/Public/RHI/BaseRHIResource.h` (IDE 이름변경, `TRHIResource<T>` 삭제, `trackedState` 필드 추가)
- `Engine/Public/Render/RenderGraph.h` — `RegisterPersistentResource`/`UnregisterPersistentResource`/`persistentResourceStates`/`TransientDepthEntry::lastKnownState` 제거
- `Engine/Private/Render/RenderGraph.cpp` — `Compile()` 단순화, `transition()` 로직 교체

### `IRHIResource` 참조 갱신 (`BaseRHIResource`로 치환 — grep 확인된 20개 파일)
`Execute.cpp`, `RenderGraph.cpp`, `RenderGraph.h`, `Common_RHI.h`(`ResourceBarrier::resource` 필드), `CommandList_DirectX.cpp/.h`, `DepthBuffer_DirectX.h`, `IDepthBuffer.h`, `RenderDevice_DirectX.cpp`, `Common_DirectX.h`(`RawResource`), `ICommandList.h`, `ResourceView_DirectX.cpp/.h`, `IRenderPass.h`(`PassResourceDecl::resource` 필드), `SwapChain_DirectX.cpp/.h`, `IResourceView.h`, `ISwapChain.h`
(※ `RenderGraphh.txt`/`RenderGraphcpp.txt`는 빌드에 포함되지 않는 과거 백업 파일 — 건드리지 않음)

### 구체 구현체 초기 상태 수정
- `Engine/Private/RHI/DirectX/Buffer/ColorBuffer_DirectX.cpp`
- `Engine/Private/RHI/DirectX/DepthBuffer_DirectX.cpp`
- `Engine/Private/RHI/DirectX/Texture/TextureUAV_DirectX.cpp`
- `Engine/Private/RHI/DirectX/Buffer/Buffer_DirectX.cpp` (버그 수정 포함)
- `Engine/Private/RHI/DirectX/SwapChain_DirectX.cpp`

### 호출부 정리
- `Engine/Private/Render/SceneRenderer.cpp`
- `Engine/Private/Render/BloomRenderer.cpp`
- `Engine/Private/Render/AnimationRenderer.cpp`
- `Engine/Private/Execute.cpp`

### 신규 파일
- `Engine/Private/Render/PresentTransitionPass.h` — 백버퍼 Present 전환을 그래프 Pass로 편입(6번 항목)

---

## 진행 순서

1. `TRHIResource<T>` 삭제
2. `Common_RHI.h`에 `EResourceState::Uninitialized` 추가
3. `IRHIResource.h` → `BaseRHIResource.h` 이름 변경(IDE) + 클래스명 변경 + `trackedState`(기본값 `Uninitialized`)/`GetTrackedState()`/`SetTrackedState()` 추가(`SetTrackedState()`에 "Compile() 안에서만 호출" 주석 명시, `ForceTrackedState()`는 만들지 않음)
4. 20개 참조 파일 일괄 치환(컴파일러가 안내하는 대로 기계적 진행)
5. 5개 구체 구현체의 `Create()`에 정확한 초기 상태 설정(`ColorBuffer_DirectX`→RenderTarget, `DepthBuffer_DirectX`→DepthWrite, `TextureUAV_DirectX`→UnorderedAccess, `Buffer_DirectX`→Common, `SwapChain_DirectX::BackBufferResource`→Common)
6. `RenderGraph.h/.cpp` 단순화 (맵/등록 메서드/`ImportResource`/`GetCurrentState` 전부 제거, `transition()`을 리소스 직접 조회 방식으로 교체 + `Uninitialized` assert 추가, `Compile()`에 스레드 계약 주석 명시)
7. `PresentTransitionPass.h` 신규 작성, `SceneRenderer`/`BloomRenderer`/`AnimationRenderer`에서 이번 세션에 추가했던 등록 호출 제거, `Execute.cpp`의 수동 Present 배리어 코드를 `renderGraph.AddPass<PresentTransitionPass>(backBuffer)` 한 줄로 교체
8. 빌드 → 전체 씬(Kachujin/Shadow/Bloom/Normal Map/Tonemap) 육안 재검증, 특히 BonePalette 첫 프레임 UAV 쓰기·Shadow Map 프레임 간 재사용·백버퍼 Present 전환이 여전히 정상인지 확인

---

## 결정 확정 (2026-07-12)

1. **`ImportResource`/`GetCurrentState` → 완전 제거.** 백버퍼 Present 전환은 `PresentTransitionPass`(6번 항목)로 그래프에 편입 — `Execute.cpp`가 리소스 상태를 직접 다루지 않음.
2. **Assert 안전장치 → `EResourceState::Uninitialized` 센티널 방식으로 대체.** 위 "3-1" 참조 — 백엔드 무관, GPU 실행 전 `Compile()` 시점에 즉시 실패.
3. **스레드 안전성 → 추가 설계 불필요.** 위 "3-2" 참조 — `SetTrackedState()`가 `Compile()` 한 곳에서만 호출되는 현재 구조가 이미 "Compile 순차 + Execute 병렬" 패턴에 안전. 주석으로 계약만 명시.
4. **`ForceTrackedState()` 같은 public 이스케이프 해치 → 두지 않는다.** 백버퍼 Present 전환을 `PresentTransitionPass`로 그래프에 편입시키면서 필요성 자체가 사라짐 — "상태 변경은 오직 `Compile()`을 통해서만 일어난다"는 불변식을 예외 없이 유지. 향후 유사 케이스(스크린샷 캡처 등)도 전용 트리비얼 Pass로 해결하는 것을 기본 원칙으로 한다.
5. **`RenderGraph` public API 4개 메서드(`ImportResource`/`GetCurrentState`/`RegisterPersistentResource`/`UnregisterPersistentResource`) → 선언·구현 전부 삭제 가능(grep 검증 완료).** 아래 참조.

---

## `RenderGraph` API 삭제 가능성 검증 (grep, 2026-07-12)

`PresentTransitionPass` 도입 후 이 4개 메서드가 정말 죽은 API가 되는지 실제 호출부를 전수 조사했다(`Engine/` 전체 grep, 빌드 제외 백업 파일 `*.txt` 제외):

| 메서드 | 살아있는 호출부 | 비고 |
|---|---|---|
| `ImportResource` | `Execute.cpp:251` 1곳 | `PresentTransitionPass` 도입으로 제거 예정(6번 항목) |
| `GetCurrentState` | `Execute.cpp:273` 1곳 | 위와 동일 |
| `RegisterPersistentResource` | `SceneRenderer.cpp` 5곳, `BloomRenderer.cpp` 2곳, `AnimationRenderer.cpp` 1곳 | 이번 리팩터링(섹션 5)에서 등록 호출 자체를 삭제 예정 |
| `UnregisterPersistentResource` | **0곳** | 애초에 어디에도 연결되지 않은 죽은 메서드(화면 크기 변경 대응은 아직 미구현 — `2026-07-11_Log.md` Phase 2 ToDo로 별도 추적 중) |

이번 리팩터링의 나머지 항목(6번 `PresentTransitionPass`, 5번 호출부 정리)이 계획대로 반영되면 위 4개 메서드는 **살아있는 호출부가 0개**가 된다. 따라서 `RenderGraph.h`/`RenderGraph.cpp`에서 선언과 구현을 통째로 삭제한다 — 남겨두는 것은 CLAUDE.md "사용되지 않는 추상화는 추가하지 않는다" 원칙 위반이며, 죽은 public API가 남아있으면 이후 누군가 다시 "그래프 밖 수동 등록" 패턴을 재도입할 유혹이 생긴다(이번 세션에 실제로 그런 식으로 설계가 표류했던 전례가 있음).

`resourceStates`/`persistentResourceStates` 맵(섹션 3에서 이미 제거 대상으로 확정), `TransientDepthEntry::lastKnownState`(리소스 자신의 `trackedState`로 흡수) 역시 이 4개 메서드와 함께 완전히 사라진다 — `RenderGraph`가 갖는 상태는 `entries`(패스 목록)와 `transientDepths`(트랜지언트 depth 캐시)뿐으로 줄어든다.

### A안/B안/C안(`PresentTransitionPass`) 비교 — 왜 이스케이프 해치를 아예 안 만드는지

compact 이전 턴에서 두 가지 절충안이 나왔었다: **(A)** `RenderGraph::NotifyExternalTransition(resource, state)`(내부는 `resource->SetTrackedState(state)` 한 줄, "상태 변경은 전부 RenderGraph를 거친다"는 원칙 우선) vs **(B)** `BaseRHIResource::ForceTrackedState(state)`(public, friend 불필요, "RenderGraph API는 완전히 비운다"는 원칙 우선). 두 원칙을 다시 기준으로 채점하면:

| | ① 상태 변경은 항상 RenderGraph를 거친다 | ② RenderGraph API가 완전히 비워진다 |
|---|---|---|
| (A) `NotifyExternalTransition` | **형식만 만족** — 이름은 `RenderGraph`를 거치지만 내부가 setter 한 줄이라 `Compile()`의 `transition()`(배리어 계산) 로직을 타지 않음. "그래프를 호출한다" ≠ "그래프가 관리한다" | 불만족 — 새 public 메서드가 남음 |
| (B) `ForceTrackedState` | 불만족 — 우회 경로 자체가 원칙 위반 | **부분 만족** — `ImportResource`/`GetCurrentState`는 여전히 필요(Execute.cpp가 프레임 시작 상태 통보 + 끝에서 조회)해 API가 완전히 안 비워짐 |
| (C) `PresentTransitionPass` | **완전 만족** — `Compile()`의 `transition()`이 유일한 mutator, 예외 없음 | **완전 만족** — 위 표에서 확인했듯 4개 메서드 전부 삭제 가능 |

C는 A·B가 각자 우선시하던 기준에서도 A·B보다 더 강하게 이긴다(파레토 지배) — 즉 두 원칙 중 어느 쪽에 비중을 두더라도 결론이 뒤집히지 않는다. **결론: A/B 둘 다 도입하지 않는다.**

**C의 한계(정직하게 남겨둘 것)**: C는 "같은 프레임·같은 커맨드리스트" 안에서 일어나는 전환만 해결한다. 향후 진짜 다른 큐(Copy/Compute)에서 비동기로 리소스를 건드리는 크로스 큐 시나리오(예: 비동기 텍스처 스트리밍)가 생기면 C로도 못 푼다 — 다만 현재 코드베이스에 그런 사례가 없고(`AnimationRenderer`의 BonePalette 컴퓨트도 같은 그래프·같은 프레임 안에서 처리됨), CLAUDE.md 원칙상 실제 필요가 생기기도 전에 A/B 같은 자리표시자 메서드를 미리 만들어두지 않는다. 그 시점이 오면 fence 기반 크로스 큐 동기화까지 포함한 별도 설계가 필요하며, 지금의 A/B는 애초에 그 문제의 정답도 아니다(같은 프레임 안이라는 전제가 A/B 설계에도 암묵적으로 깔려 있었음).

---

## 사이드 이펙트 재검토 결과

- **스레드 안전성**: 위 "3-2"에서 확정 — `Execute()`가 `trackedState`를 전혀 건드리지 않으므로 병렬 커맨드 기록에 이미 안전. 향후 병렬화 시 추가 설계 불필요, 주석 계약만 지키면 됨.
- **`RenderScene` 스냅샷과의 관계**: 완전히 다른 계층(CPU 게임로직 스레드 vs 렌더 스레드 데이터 격리)이라 이번 변경과 무관, 영향 없음.
- ~~**`IDepthBuffer`가 `BaseRHIResource`를 상속하지 않는 문제**: 실질적으로 문제 없음 — 배리어 코드가 이미 전부 `depthBuffer->GetResource()`를 거쳐 실제 `BaseRHIResource`에 접근하고 있음.~~ **후속 개선 완료(2026-07-12)** — 사용자가 원래 상속으로 설계하려 했으나 당시 방법을 못 찾아 합성(`GetResource()` 접근자)으로 남아있던 부분. `IDepthBuffer : public BaseRHIResource`로 직접 상속하도록 변경(`IColorBuffer`/`ITextureUAV`/`IBuffer`와 동일 패턴으로 통일). `DepthBuffer_DirectX`의 중첩 `DepthResource` 합성 구조체를 제거하고 `ComPtr<ID3D12Resource> depthResource` 멤버를 클래스에 직접 두어 `Create()`가 `SetTrackedState()`를 바로 호출(더 이상 `MarkCreated()` 우회 불필요). `GetResource()` 접근자를 삭제하고 9개 호출부(`GBufferRenderer`/`ForwardRenderer`×3/`AnimationRenderer`/`ShadowMapRenderer`/`DeferredLightingRenderer`×2)에서 `depthBuffer->GetResource()` → `depthBuffer`로 단순화(암시적 업캐스트). `SwapChain_DirectX`/`BackBufferResource`는 반대로 합성을 유지 — 스왑체인 1개가 백버퍼 N개(최대 3중 버퍼링)를 소유하는 1:N 관계라 `ISwapChain` 자신이 `BaseRHIResource`가 되는 것은 의미상 맞지 않음(1:1 관계인 DepthBuffer/ColorBuffer/TextureUAV/Buffer와 다른 케이스).
- **다중 백버퍼(2중 버퍼링)**: 각 `BackBufferResource` 인스턴스가 독립된 `trackedState`를 가지므로, 스왑체인이 버퍼를 순환시켜도 각자 정확한 마지막 상태를 유지 — 오히려 기존 맵 방식보다 자연스럽게 맞아떨어짐.
- **범위**: 파일 개수(20여 개)는 많지만 전부 `IRHIResource` → `BaseRHIResource` 기계적 치환이라 컴파일 에러를 따라가며 처리 가능. 로직이 실제로 바뀌는 곳은 `RenderGraph.cpp`(단순화), 5개 구체 구현체의 `Create()`(초기 상태 지정), `Execute.cpp`(수동 배리어 → `PresentTransitionPass`)뿐.
- **"그래프 관리 대상인지 판단해야 하는" 문제**: `ForceTrackedState()` 이스케이프 해치를 없애고 백버퍼 Present 전환을 `PresentTransitionPass`(6번 항목)로 그래프에 편입시키면서 근본적으로 해소됨 — 모든 `BaseRHIResource`는 "그래프의 어느 Pass가 참조하면 자동 추적되고, 아무 Pass도 참조하지 않으면 애초에 추적 대상이 아니다"라는 단일 규칙만 남는다. 예외 규칙이 없다.
