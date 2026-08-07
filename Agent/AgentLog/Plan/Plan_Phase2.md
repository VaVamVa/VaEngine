# 멀티플랫폼 프러스텀 컬링 계획서 (Phase2)

작성일: 2026-07-16
상태: 인터페이스·데이터 흐름 설계(3번 섹션) 아키텍처 레벨 결정 완료 + 구현 착수 전 최종 검증 완료(2026-07-16). 검증 중 발견해 수정한 상호 모순 3건: (1) 공용 이동 목록이 `CPU_BruteForce` 모드에서 아무도 드레인하지 않아 무한정 쌓이는 문제 — 드레인 후 폐기하도록 수정, (2) GPU 바운딩볼륨 버퍼를 GBuffer depth급 영속 리소스로 서술해 §1-8(모드 전환 시 준비/해제)과 모순 — "활성 상태 동안만 영속"으로 정정, (3) Shadow용 GPU 압축 버퍼도 같은 이유로 "상시 할당"이 잘못됨 — §1-8 생명주기를 따르도록 정정. 남은 "미정" 항목은 전부 구현 시점에 정해도 되는 세부(바이트 정렬·워커 수 산정 등)뿐. 구현 착수 가능.
참조: [`2026-07-16_Q&A.md`](../2026-07-16_Q&A.md), [`TODO.md#phase-2`](../TODO.md#phase-2), [`2026-07-30_Log.md`](../2026-07-30_Log.md)

---

## 개요

**진행 기록 규칙** (2026-07-16 신설, [`README.md` 문서화 규칙](../README.md#문서화-규칙) 적용): 이 문서는 계획·설계·근거만 담는다. 실제 구현 내용(수정 파일, 코드 변경, 버그·검증 이력)은 전부 해당 날짜 `Log`에 기록하고, 이 문서에는 항목이 완료되는 시점에 `~~취소선~~` 처리 후 그 Log를 상대경로 링크로 태그만 한다. 세부 작업 체크리스트는 이 문서가 아니라 [`TODO.md#phase-2`](../TODO.md#phase-2)가 원본이며, 여기서는 중복 기술하지 않고 링크로만 참조한다.

---

## 목차

- [개요](#개요)
- [0. 목표 — 왜 이 작업을 하는가](#0-목표--왜-이-작업을-하는가)
- [1. 핵심 결정 사항](#1-핵심-결정-사항)
  - [1-1. CPU 사전 필터링이 표준 — GPU 클리핑은 컬링의 대체재가 아니다](#1-1-cpu-사전-필터링이-표준--gpu-클리핑은-컬링의-대체재가-아니다)
  - [1-2. CPU 경로: 브루트포스와 Loose-Octree를 모두 구현한다](#1-2-cpu-경로-브루트포스와-loose-octree를-모두-구현한다)
  - [1-3. CPU/GPU 분기는 RHI가 아니라 SceneRenderer 조립 지점에서 결정한다](#1-3-cpugpu-분기는-rhi가-아니라-scenerenderer-조립-지점에서-결정한다)
  - [1-4. 런타임/컴파일타임 분류 — 임의가 아니라 기술적 근거로 가른다](#1-4-런타임컴파일타임-분류--임의가-아니라-기술적-근거로-가른다)
  - [1-5. CPU 컬러와 GPU 컬러는 하나의 인터페이스로 묶지 않는다 — 서로 다른 레이어에 산다](#1-5-cpu-컬러와-gpu-컬러는-하나의-인터페이스로-묶지-않는다--서로-다른-레이어에-산다)
  - [1-6. Collider(Phase 4-3)와의 관계 — 독립 구현, 바운딩 볼륨 타입만 공유](#1-6-colliderphase-4-3와의-관계--독립-구현-바운딩-볼륨-타입만-공유)
  - [1-7. Shadow 컬링은 카메라 컬링과 구조적으로 다르다 — Phase 2 정식 항목으로 승격](#1-7-shadow-컬링은-카메라-컬링과-구조적으로-다르다--phase-2-정식-항목으로-승격)
    - [Shadow 가시성 판정 정확도 — 두 가지 실패 원인](#shadow-가시성-판정-정확도--두-가지-실패-원인)
    - [캐스케이드별 컬링 효과는 균일하지 않다](#캐스케이드별-컬링-효과는-균일하지-않다)
    - [GPU-Driven Shadow 컬링은 코드가 8배 늘어나는 게 아니라 실행 인스턴스가 늘어나는 것](#gpu-driven-shadow-컬링은-코드가-8배-늘어나는-게-아니라-실행-인스턴스가-늘어나는-것)
  - [1-8. 런타임 모드 전환 — Lazy 재구성, 정지 허용, 개발자 전용](#1-8-런타임-모드-전환--lazy-재구성-정지-허용-개발자-전용)
  - [1-9. 계측/비교 인프라 — 있는 것과 없는 것](#1-9-계측비교-인프라--있는-것과-없는-것)
  - [1-10. Debug 툴 훅 — 안정된 접점에만 의존하면 "계산"과 "표시"는 자연히 분리된다](#1-10-debug-툴-훅--안정된-접점에만-의존하면-계산과-표시는-자연히-분리된다)
- [2. 구현 순서](#2-구현-순서)
- [3. 인터페이스·데이터 흐름 설계](#3-인터페이스데이터-흐름-설계)
  - [결정된 항목](#결정된-항목)
  - [아직 미정 — 다음 논의 대상](#아직-미정--다음-논의-대상)
- [4. 완료 항목](#4-완료-항목)

---

## 0. 목표 — 왜 이 작업을 하는가

VaEngine은 "빠르게 상품화하는 엔진"이 아니라, **어떤 최적화가 가능하고 그 최적화가 언제 필요한지를 비교·수치로 보여주는 포트폴리오**로 삼는다(CSM/IBL 때와 동일한 접근). 이번 작업은 프러스텀 컬링을 소재로 이를 실현한다:

- CPU-Driven(브루트포스·Loose-Octree, 멀티스레드)과 GPU-Driven(Desktop Compute) 컬링을 **하나만 고르지 않고 모두 구현**한다.
- 멀티플랫폼 제약(모바일/TBDR은 GPU-Driven 불가)에 따라, 가능한 분기는 런타임 전환, 불가능한 분기는 컴파일타임(CMake/플랫폼 매크로)으로 고정한다.
- 구현 자체보다 "왜 이 선택지가 이 상황에 맞는가"를 수치(프레임 타임, 컬링된 오브젝트 수 등)로 증명하는 것이 목적이다.

---

## 1. 핵심 결정 사항

### 1-1. CPU 사전 필터링이 표준 — GPU 클리핑은 컬링의 대체재가 아니다

GPU 래스터라이저 클리핑은 draw call 제출 **이후** 개별 프리미티브 단위로 일어나는 하드웨어 고정 기능이라, 완전히 화면 밖에 있는 오브젝트라도 Vertex Shader 호출과 draw call 제출 비용은 그대로 낸다. 반대로 CPU 바운딩 볼륨-평면 교차 테스트는 오브젝트당 내적 몇 회 수준이라, 화면 밖 오브젝트의 draw call·VS 비용을 통째로 아끼는 이득이 압도적으로 크다. 멀티플랫폼 관점에서도 CPU 컬링은 IMR(DX12)·TBDR(Vulkan) 양쪽에서 동일하게 이득을 내는 반면, GPU-Driven Compute 컬링은 TBDR에서 대역폭 역효과가 있다(CLAUDE.md).

→ 근거: [`2026-07-16_Q&A.md` Q1](../2026-07-16_Q&A.md#q1-프러스텀-컬링-전략-비교--gpu-클리핑에-의존-vs-cpu-사전-필터링)

### 1-2. CPU 경로: 브루트포스와 Loose-Octree를 모두 구현한다

순수 성능만 보면 지금 씬 규모(오브젝트 수 적음)에서는 SIMD+멀티스레드 브루트포스가 Loose-Octree보다 오히려 빠를 수 있다(트리 순회의 포인터 체이싱 오버헤드가 노드 스킵 이득을 못 이길 정도로 N이 작음). 그럼에도 "다양한 자료구조·병렬 패턴을 보여준다"는 0번 목표가 명시적으로 있어, 성능 우위와 무관하게 둘 다 구현하고 런타임 전환·비교 결과를 산출물로 남긴다.

**가장 중요한 미해결 설계 질문**: Loose-Octree의 영속성. 지금 엔진은 매 프레임 `RenderScene::Clear()`로 커맨드를 전부 지우고 `WorldObject::AddToScene()`으로 다시 채우는 즉시 모드 패턴이다. 옥트리를 이 패턴 그대로 매 프레임 처음부터 재구축하면 "노드 단위로 건너뛰어 테스트 횟수를 줄인다"는 이득은 유지되지만 "삽입 비용 자체를 아낀다"는 이점은 사라진다. 옥트리의 이점을 온전히 살리려면 `WorldObject` 생성/삭제/이동 시점에 훅을 걸어 증분 갱신하는 영속 구조로 만들어야 하고, 이는 `RenderScene`과는 별도의 저장 위치가 필요하다 — 3번(인터페이스·데이터 흐름) 논의 대상.

→ 근거: [`2026-07-16_Q&A.md` Q2](../2026-07-16_Q&A.md#q2-멀티스레드-cpu-컬링--브루트포스-vs-loose-octree)

### 1-3. CPU/GPU 분기는 RHI가 아니라 SceneRenderer 조립 지점에서 결정한다

"같은 Vulkan API를 쓰더라도 Desktop은 GPU, Mobile은 CPU가 필요하다"는 전제 자체가, 이 분기가 RHI(API 축)가 아니라 알고리즘 축에서 결정돼야 함을 보여준다 — CLAUDE.md의 "알고리즘 선택(Forward/Deferred/GPU-Driven)과 API 선택(DX12/Vulkan)은 독립 축" 원칙과 정확히 같은 문제다. RHI는 현재 버퍼·텍스처·PSO·커맨드리스트 같은 GPU 리소스 추상화만 알고 `RenderScene`/오브젝트/컬링 개념을 전혀 모르는데, 여기에 "CPU냐 GPU냐" 파라미터를 넣으면 RHI가 씬 레벨 개념을 알아야 하게 되어 계층이 무너진다.

다만 확인 결과 `SceneRenderer`는 아직 `ISceneRenderer` 인터페이스가 아니라 Deferred+Forward(투명)를 고정 조합한 단일 콘크리트 클래스다(`SceneRenderer.h`) — 즉 컬링 전략을 꽂을 조립 지점 자체가 지금은 없고, 이번 작업이 처음 만들어야 하는 인프라다. 방향은 기존에 이미 쓰이는 패턴(`USE_DIRECTX`/`USE_VULKAN` CMake 플래그, `ExecWindows`/`ExecAndroid` 진입점)과 같은 층위 — 즉 런타임 파라미터가 아니라 빌드/조립 시점에 결정하는 구조.

### 1-4. 런타임/컴파일타임 분류 — 임의가 아니라 기술적 근거로 가른다

| 분기 | 시점 | 근거 |
|---|---|---|
| CPU 컬링 알고리즘(브루트포스 ↔ Loose-Octree) | **런타임** | 둘 다 순수 CPU 로직, 같은 입출력이라 RHI/플랫폼 의존 없음 |
| CPU-Driven ↔ GPU-Driven (Desktop 한정) | **런타임** | DX12는 둘 다 지원 가능 — "오브젝트 1만 개일 때 CPU 30fps, GPU 60fps" 같은 실시간 비교가 이번 작업의 핵심 산출물 |
| GPU-Driven 경로의 존재 자체 | **컴파일타임** | API(DX12/Vulkan) 축이 아니라 GPU 아키텍처(IMR/TBDR) 축으로 배제해야 함 — `USE_VULKAN`으로 게이팅하면 Desktop-Vulkan 빌드가 생겼을 때도 잘못 배제된다. `ANDROID` 매크로 등 플랫폼 기준 사용(`Engine/CMakeLists.txt`가 Vulkan 블록에서 이미 이렇게 씀) |

토글 값 자체는 `RenderScene`에 기존 `ssaoEnabled`/`iblEnabled`/`showCascades` 패턴으로 추가하고("무엇을 그릴 것인가" 데이터), 그 값을 보고 실제로 어느 경로를 탈지 결정하는 로직은 `SceneRenderer`가 담당한다("어떻게 그릴 것인가" 실행 정책). RHI는 끝까지 CPU/GPU 개념을 모른 채 내려온 커맨드만 실행한다.

### 1-5. CPU 컬러와 GPU 컬러는 하나의 인터페이스로 묶지 않는다 — 서로 다른 레이어에 산다

- **CPU 컬링**: 결과물이 "필터링된 CPU 커맨드 리스트"다. `RenderScene`이 커맨드를 다 모은 직후, `SceneRenderer::AddPasses`가 소비하기 전 시점에 끼어든다. 여전히 "무엇을 그릴 것인가"의 연장이라 `RenderScene`이 소유하거나 입력으로 받는 별도 클래스(컨테이너+병렬 테스트 로직)로 분리한다.
- **GPU 컬링**: 결과물이 GPU 상주 버퍼(compute가 압축한 가시 인덱스 리스트)이고 CPU로 리드백되지 않는다. `RenderScene`의 CPU 커맨드 리스트는 건드리지 않고 `SceneRenderer`/`RenderGraph` 층(컴퓨트 패스 + `ExecuteIndirect`)에서 완결된다 — "어떻게 그릴 것인가".

하나의 `ICuller` 같은 인터페이스로 둘을 묶으면 GPU 실행 로직이 "CPU 데이터를 다루는 클래스" 안에 들어가는 꼴이 되어 `RenderScene`/`SceneRenderer` 경계를 어긴다. 둘을 잇는 공통점은 같은 인터페이스가 아니라 "플랫폼별 빌드에서 둘 중 하나만 컴파일된다"는 조립 시점의 배타성뿐이다.

**참고(UE5 Nanite)**: 상용 엔진에서도 하나의 프레임 안에 CPU 컬링·GPU 컬링·전용 컬링이 오브젝트 종류에 따라 동시에 섞여 도는 게 정상이다(Nanite는 스켈레탈 메시·반투명 머티리얼을 지원하지 않아 항상 전통 경로로 빠지고, 그 전통 경로 자체도 CPU 옥트리 컬링 + GPU Scene instance 컬링을 함께 쓴다). 다만 VaEngine 규모에서는 UE 수준의 오브젝트-타입별 세분화까지 초반부터 필요하지 않고, 지금 정한 플랫폼 단위 분기로 충분하다 — 향후 확장 여지로만 남겨둔다.

### 1-6. Collider(Phase 4-3)와의 관계 — 독립 구현, 바운딩 볼륨 타입만 공유

Collider(TODO Phase 4-3)와 겹치는 부분은 Sphere 구조체 + `worldMatrix` 기반 월드 공간 갱신 정도로 작다. Collider의 본체(Capsule 지오메트리, 충돌 응답, Mesh Collision Component)는 컬링과 무관해 순서를 강제할 근거가 없다 — **서로 독립적으로 구현 가능**. 다만 Phase 2에서 Sphere를 정의할 때 `RenderCommand` 전용 타입으로 좁히지 않고 범용 위치(`Math/`)에 둬서, 나중에 Collider가 그대로 재사용할 수 있게만 해둔다.

**Sphere와 Capsule은 용도가 달라 영구히 분리 유지한다.** 컬링은 "실제로 보이는 걸 잘못 컬링하는 것(누락)"이 "안 보이는데 그리는 것(과대 포함)"보다 훨씬 나쁜 실패 방향이라, 정밀도보다 안전한 과대 근사가 우선이다 — Sphere면 충분하고, 나중에 Capsule이 구현되더라도 컬링을 Capsule로 교체할 이유가 없다(오탐 감소보다 테스트 비용 증가가 더 큼). 반대로 캐릭터 충돌체는 정밀도가 핵심이라 Sphere/Circle 근사는 부적절하다 — 모서리·발밑 접촉에서 실제로 체감되는 오류가 생기므로, Collider(Phase 4-3)의 실제 판정 형상은 처음부터 Capsule로 고정한다. Sphere는 어디까지나 컬링용 공용 바운딩 볼륨이지 Collider의 대체 형상이 아니다.

### 1-7. Shadow 컬링은 카메라 컬링과 구조적으로 다르다 — Phase 2 정식 항목으로 승격

카메라 프러스텀으로 한 번 거른 `RenderScene` 커맨드 리스트를 `ShadowMapPass`에 그대로 재사용하면 틀린다 — 카메라 시야 밖이라도 그림자가 화면 안으로 들어오는 오브젝트는 그려야 하는데, 카메라 프러스텀 기준으로 이미 걸러지면 사라진다. Shadow 컬링은 캐스케이드마다 다른 광원 시점 프러스텀으로 별도 테스트해야 하는, 구조적으로 독립된 작업이다.

다만 도구는 재사용된다 — `Math::ExtractFrustumPlanes(view, proj)`는 카메라든 광원이든 view-proj 행렬만 있으면 동작하는 순수 함수라 캐스케이드 8개 각각의 light view-proj에 그대로 적용 가능하다(3번 섹션 "결정된 항목"에서 이 함수를 `RenderScene` 전용이 아니라 `Math/`에 두기로 한 근거 중 하나가 바로 이 재사용성이었다). **다만 브루트포스/Octree의 쿼리 API 자체를 "카메라 프러스텀"에 하드코딩하지 않고 임의의 6평면을 받는 형태로 설계해야** 이 재사용이 실제로 성립한다 — 처음부터 이렇게 설계한다.

기존에 "캐스케이드별 프러스텀 컬링 없음"이 `2026-07-13_Q&A.md` Q3에서 이미 기술부채로 등록돼 있었으나, Shadow 패스가 오브젝트당 캐스케이드 수(최대 8배)만큼 비용이 커 컬링 이득이 가장 크게 드러나는 지점이라는 점에서 막연한 부채로 방치하지 않고 **Phase 2의 정식 항목으로 승격**한다. 순서는 뒤로 둔다 — 카메라용 컬링 인프라(Sphere, 평면 추출, 브루트포스/Octree)를 먼저 완성·검증한 뒤 같은 도구를 캐스케이드별로 재적용하는 게 순서상 자연스럽다.

**Transparent는 별도 항목이 필요 없다**: `RenderScene`이 Opaque/Transparent를 하나의 커맨드 스트림으로 통합 관리하므로(정렬 키의 translucent 비트로만 구분), `RenderScene::CullCommands()`가 이 통합 리스트를 거르면 `GBufferPass`·`TransparentPass` 둘 다 자동으로 커버된다.

#### Shadow 가시성 판정 정확도 — 두 가지 실패 원인

카메라 가시 리스트와 Shadow 가시 리스트는 서로 다른 부분집합이다 — `RenderScene::CullCommands()`가 `commands`를 파괴적으로(erase) 걸러내면 카메라엔 안 보이지만 그림자는 필요한 오브젝트가 `ShadowMapPass`에서도 사라지는 버그가 생긴다. 즉 카메라 필터링과 Shadow 필터링은 같은 리스트를 순차로 깎아나가는 방식이 아니라 **서로 다른 뷰(view)를 각자 만들어야** 한다. 이 Shadow 전용 판정에서 캐스터를 잘못 제외시키는(false negative) 원인이 두 가지 확인됐다:

1. **Peter-panning — 광원 프러스텀 근평면이 타이트하면 유효한 캐스터를 놓친다.** 캐스케이드 피팅용 직교 박스(카메라 서브 프러스텀 bounding sphere로 딱 맞게 계산, `2026-07-13_Log.md` Compact Log #1)를 캐스터 컬링에 그대로 쓰면, 박스 바깥에서 광원 쪽으로 더 가까이 있는 오브젝트의 그림자가 박스 안으로 들어오는 경우를 놓친다. 캐스케이드 **피팅용** 박스와 캐스터 **컬링용** 박스는 목적이 달라 별도 취급해야 하며, 컬링용은 광원 방향으로 근평면을 씬 경계까지 확장한다.
2. **스키닝 캐릭터의 rest-pose(T-pose) 바운드가 애니메이션 중 실제 포즈를 다 못 담는다.** 카메라 컬링에도 있는 근본 문제(3번 섹션 "로컬 바운드 계산 시점" 항목 참조)지만, Shadow에선 오브젝트가 아니라 그림자만 따로 깜빡이며 나타났다 사라지는 형태로 더 눈에 띈다. 마진(반지름 여유값)으로 완화하되 **마진을 캐스케이드 깊이에 따라 다르게 준다**: 근거리 캐스케이드는 텍셀 밀도가 높아 그림자가 크고 또렷해 오류가 잘 보이고 오브젝트 수도 적어 컬링 이득 자체가 작으니 마진을 넉넉하게(안전 우선), 원거리 캐스케이드는 텍셀 밀도가 낮아 오류가 잘 안 보이고 덮는 영역이 넓어 오브젝트가 많아 컬링 이득이 크니 마진을 타이트하게(성능 우선) 준다. 애니메이션의 실제 최대 편차(월드 공간 절대값) 자체가 캐스케이드에 따라 달라지는 게 아니라, "안전 마진 vs 컬링 타이트함"의 트레이드오프 지점이 캐스케이드마다 다르므로 마진 배율을 캐스케이드별로 조정하는 것이 맞다. `splitLambda`/`blendWidth`처럼 육안 튜닝이 필요한 파라미터로 취급한다.

#### 캐스케이드별 컬링 효과는 균일하지 않다

근거리 캐스케이드는 박스가 좁아 컬링 효과가 크고, 원거리 캐스케이드는 박스가 넓어(거의 씬 전체) 컬링해도 별로 안 걸러진다. §1-9 계측/비교 시 "평균 N배 절감" 하나로 뭉뚱그리지 않고 캐스케이드별로 나눠 보여줘야 이 최적화의 실제 특성이 정확히 전달된다 — 왜 근거리에서 더 효과적인지 자체가 좋은 설명 소재.

#### GPU-Driven Shadow 컬링은 코드가 8배 늘어나는 게 아니라 실행 인스턴스가 늘어나는 것

컴퓨트 컬링 셰이더와 `ExecuteIndirect` 메커니즘 자체는 한 번만 작성한다 — 기존 `ShadowMapPass`가 이미 "한 기능 = Pass 구조체를 캐스케이드 수만큼 등록"하는 컨벤션(Bloom/OIT와 동일)을 쓰고 있으므로, 같은 컴퓨트 패스를 캐스케이드마다 다른 light view-proj로 8번 인스턴스화해서 등록하면 된다. 늘어나는 건 GPU 제출 횟수(컴퓨트 디스패치·인다이렉트 드로우 각 최대 8회)와 `RenderGraph` 패스 등록 개수이지, 작성해야 할 셰이더/C++ 코드량이 아니다. 캐스케이드별로 압축 인덱스 버퍼를 따로 둘지 하나의 버퍼에 구간을 나눠 쓸지는 세부 구현 시점에 결정한다.

### 1-8. 런타임 모드 전환 — Lazy 재구성, 정지 허용, 개발자 전용

당초 "모든 모드의 리소스를 미리 다 만들어두고 값만 바꾼다"를 검토했으나, 다음 전제 하에서는 오히려 손해다:
- Octree를 비활성 모드에서도 계속 유지하면 `WorldObject` 생성/삭제/이동마다 불필요한 갱신 비용을 상시 지불한다.
- GPU 리소스(컴퓨트 PSO·인다이렉트 커맨드 시그니처·GPU 바운딩볼륨 버퍼)를 항상 유지하면 안 쓰는 모드의 메모리를 계속 붙잡는다.

전환 자체가 **개발자 전용 디버그 기능이고 인게임에서 트리거될 일이 없어 전환 순간 렌더링을 정지해도 무방하다**는 전제가 확인되어, 대신 아래 시퀀스로 확정한다:

1. **준비→할당** — 전환할 모드가 필요로 하는 리소스를 이 시점에 만든다(Octree면 현재 `WorldObject` 전체를 한 번에 순회해 새로 구축, GPU면 컴퓨트 PSO·인다이렉트 시그니처·GPU 바운딩볼륨 버퍼 생성/업로드).
2. **전환** — 기존 Fence 기반 GPU 동기화로 GPU idle까지 대기(이전 모드 리소스를 참조하는 in-flight 커맨드가 없음을 보장)한 뒤 `frustumCullMode` 값을 바꾼다.
3. **이전 리소스 해제** — GPU가 확실히 다 쓴 뒤이므로 안전하게 해제.
4. **재개방** — 전환 입력을 다시 받기 시작.

`RenderGraph`가 매 프레임 새로 빌드되는 구조라 이 시퀀스가 프레임 경계와 자연스럽게 맞물린다. Octree는 "항상 유지"가 아니라 **활성 모드일 때만 유지**하고, 비활성 전환 시 해제한다.

**Debug 훅(§1-10)**: 1~4 각 단계 진입 시점에 `VA_LOG`로 찍는다(예: "전환 준비 시작: CPU_BruteForce → CPU_LooseOctree", "GPU idle 대기 완료", "이전 리소스 해제 완료"). 이 시퀀스는 자주 실행되는 경로가 아니라 매 프레임 로깅 비용을 걱정할 필요가 없고, CSM 때처럼 캡처된 로그로 설계한 순서가 실제로 지켜졌는지 검증하는 용도로 그대로 쓸 수 있다.

**개발자 전용 게이팅**: 이 전환 트리거는 기존 `ShowCascades`(Num3)/`activeCascadeCount`(Num4/5)와 동일하게 `VA_DEBUG` 매크로로 감싸진 디버그 전용 키 입력으로만 노출하고, 일반 게임플레이 `InputContext`에는 매핑하지 않는다 — 릴리즈 빌드엔 이 코드 자체가 없어 구조적으로 인게임 접근이 차단된다.

여기서 "개발자"는 VaEngine 저자가 아니라 **VaEngine을 가져다 쓰는 가상의 사용자**를 가리킨다 — 그 개발자가 여러 컬링 방식을 비교해보고 자신의 프로젝트에 맞는 것을 고른 뒤 최종 빌드에서는 그 선택을 고정한다는 워크플로가 전제다. `VA_DEBUG` 빌드가 이 "비교해보는" 단계이고, 최종 고정은 `RenderScene::frustumCullMode`의 기본값을 소스에서 원하는 모드로 바꾼 뒤 `VA_DEBUG` 없이 다시 빌드하는 것으로 충분하다 — 별도 CMake 옵션 없이도 CLAUDE.md가 말하는 "코드 수준 고정"이 이미 성립한다.

### 1-9. 계측/비교 인프라 — 있는 것과 없는 것

- **있음**: `Time.cpp`(CPU wall-clock, `std::chrono`) — CPU 알고리즘 간(브루트포스 vs Octree) 비교에는 그대로 사용 가능. `VA_DRAW_PANEL`(온스크린 HUD 텍스트) — Visible/Culled 카운트 표시에 재사용 가능.
- **없음 — 신규 필요**: GPU 타임스탬프 쿼리(`ID3D12QueryHeap` + `D3D12_QUERY_TYPE_TIMESTAMP`). CPU `std::chrono`로 GPU Dispatch를 감싸면 커맨드 기록 시간만 잴 뿐 비동기 GPU 실행 시간은 반영되지 않아, GPU-Driven 경로를 CPU 경로와 "공정하게" 비교하려면 반드시 필요하다.
- 지금 씬 규모로는 세 방식(브루트포스/Octree/GPU) 간 차이가 수치로 드러나지 않아, 오브젝트 N개를 그리드로 스폰하는 스트레스 테스트 씬이 별도로 필요하다.

### 1-10. Debug 툴 훅 — 안정된 접점에만 의존하면 "계산"과 "표시"는 자연히 분리된다

나중에 Debug 툴을 개선하려 할 때마다 관련 시그니처를 바꾸고 호출부를 전부 고쳐야 하는 상황을 피하는 게 목적이다. 핵심은 "표시 수단을 아예 호출하지 않는다"가 아니라, **호출부가 안정된 접점(시그니처가 거의 안 바뀌는 진입점)에만 의존하게 만드는 것**이다. 이 기준으로 보면 두 도구 모두 써도 된다 — 용도가 다를 뿐이다.

- **연속적으로 변하는 상태** (큐 깊이, 이번 프레임 visible/culled 수 등): `ThreadPool`/컬링 클래스가 순수 데이터 구조체(`ThreadPoolStats`, `CullingStats` 등)에 값을 채우고 `GetStats()`류 접근자로 노출한다. 별도의 오케스트레이터/Debug 오버레이 함수가 이걸 가져와 지금은 `VA_DRAW_PANEL`로 그리고, 나중에 더 나은 Debug 툴이 생기면 표시하는 그 지점만 교체한다.
- **한 번 일어난 사건** (스레드풀 생성/종료, Octree 재구축, §1-8 모드 전환의 각 단계 등): 발생 지점에서 바로 `VA_LOG(category, message)`를 호출해도 된다 — `VA_LOG`는 이미 `DebuggingHelper::Log()`라는 안정된 진입점 하나로만 나가므로(Compact Log #10에서 GPU 검증 메시지 연결 때와 동일 방식), 내부 구현(지금은 파일+stdout)이 나중에 바뀌어도 호출부는 그대로다. `VA_DRAW_PANEL`처럼 슬롯 번호 등 표시 방식에 종속된 매크로만 계산 코드에서 직접 호출하지 않으면 된다.

수집 자체는 기존 `VA_DEBUG` 매크로로 감싸 릴리즈 빌드에 비용이 남지 않게 한다. **이 원칙은 이번 문서의 남은 모든 세부 설계(공통 인프라·CPU-Driven·GPU-Driven·Shadow 컬링)에 적용되며, 각 항목을 설계할 때 "이 서브시스템이 Debug 툴에 뭘 보여줄 수 있어야 하는가"를 함께 정한다.**

---

## 2. 구현 순서

세부 체크리스트는 [`TODO.md#phase-2`](../TODO.md#phase-2)의 1~4번 항목이 원본이다. 순서 근거만 기록한다:

1. **공통 선행 인프라** — 바운딩 볼륨, 프러스텀 평면 추출, `RenderScene` 토글, `SceneRenderer` 조립 지점. CPU/GPU 경로 둘 다 이 위에서 시작하므로 최우선.
2. **CPU-Driven** — 모바일 필수 경로이자 GPU 경로보다 의존성이 적어(RHI 확장 불필요) 먼저 끝낼 수 있는 중간 산출물.
3. **GPU-Driven**(Desktop) — `ExecuteIndirect` 등 RHI 확장이 필요해 상대적으로 오래 걸림. 2와 서로 독립적이라 순서를 바꿔도 무방하나, 모바일 대응을 더 빨리 확보하려고 2를 앞에 둔다.
4. **계측/비교 인프라** — 비교 대상(CPU 2종·GPU 1종)이 갖춰진 뒤에야 의미가 있어 마지막.
5. **Shadow 컬링(캐스케이드별, §1-7)** — 카메라용 컬링 인프라(2·3)가 검증된 뒤, 같은 도구(`Math::ExtractFrustumPlanes`, 브루트포스/Octree, GPU 컴퓨트)를 광원 view-proj에 재적용. 카메라 컬링과 독립된 별도 가시 리스트가 필요해 순서상 가장 마지막.

---

## 3. 인터페이스·데이터 흐름 설계

> **구현 착수 전 최종 검증**: 이 섹션의 각 항목은 논의 시점마다 개별적으로 확정됐다. 개별로는 맞아도 전체를 모아보면 앞뒤가 안 맞는 지점이 있을 수 있으므로(예: GPU-Driven 배제 기준을 API 축으로 잘못 썼다가 나중에 발견한 사례), "아직 미정" 목록이 다 정리되면 구현 착수 전에 "결정된 항목" 전체를 한 번에 다시 훑는 전체 맥락 재검증을 별도로 수행한다.

### 결정된 항목

**공통 인프라**
- **카메라 프러스텀 6-평면 추출**: `Math::ExtractFrustumPlanes(const Matrix4x4& view, const Matrix4x4& proj)` — `Viewport::WorldToScreen`/`ScreenToRay`와 동일하게 `Math/`의 순수 함수(카메라 상태를 직접 읽지 않음). 근거: (1) Snapshot Isolation 위반 방지 — 로직 스레드가 계속 갱신하는 `BaseCamera`를 컬링 경로가 직접 읽으면 경합 위험, `RenderScene`이 이미 스냅샷한 `view`/`proj`로만 호출한다. (2) CSM 캐스케이드 컬링(§1-7)·GPU-Driven CB 업로드 등 `RenderScene` 밖 재사용처가 이미 2곳 이상 확인됨.
- **컬링 모드**: `EFrustumCullMode`(flat enum: `CPU_BruteForce`/`CPU_LooseOctree`/`GPU_Driven`, 기본값 `CPU_BruteForce`) — 기존 래스터라이저 백페이스 컬링용 `ECullMode`와 이름이 겹쳐 혼동될 수 있어, 그 enum은 `EBackfaceCullMode`로 리네임 완료(무관한 개념 분리). `RenderScene::frustumCullMode` 필드로 보유("무엇을 그릴 것인가" 데이터).
- **분기 코드 위치**: CPU는 `Execute::OnLoop()`에서 `scene.SortCommands()` 직전 `scene.CullCommands()` 한 줄 추가(내부적으로 `frustumCullMode`를 보고 CPU 모드면 필터링, GPU 모드면 no-op) — 기존 패스는 무수정, 정렬 대상이 줄어드는 순서상 이점도 있음. GPU는 `SceneRenderer::AddPasses()` 내부 `BonePaletteCompute` 뒤·`GBufferPass` 앞에 컴퓨트 컬링 패스 삽입, `GBufferPass`가 `ExecuteIndirect`로 전환(Desktop 빌드에서만 이 분기 자체가 컴파일됨). `TransparentPass`는 통합 커맨드 스트림 덕분에 별도 처리 불필요(§1-7).
- **런타임 전환**: §1-8 참조 — 준비→할당→전환(GPU idle 대기)→이전 해제→재개방 시퀀스, 전환 중 렌더링 정지 허용, `VA_DEBUG` 전용 키 입력으로만 노출.
- **로컬 바운드 계산 시점**: 런타임 1회 캐시(메시 애셋 단위, 로드 시 또는 lazy). 에셋에 굽지 않음 — 절차적 메시(`CubeShape` 등)엔 굽기 자체가 적용 안 되고, 계산량 자체가 마이크로초 단위라 굽기로 아낄 비용이 사실상 없음(포맷 버전업·전체 재-임포트 비용만 더 큼).
- **바운딩 볼륨 타입**: Sphere만(§1-6) — Capsule(Collider 전용)과 영구 분리.
- **Mobility 태그: `WorldObject`에 `bool movable`**: 2단계(Static/Movable)뿐이라 enum 없이 bool 하나로 충분 — 기존 `dirty`/`ssaoEnabled` 같은 단순 bool 플래그 관례와 일치. `false`(Static)는 최초 1회 등록 후 어떤 갱신 경로에서도 다시 방문되지 않고(다음 항목 참조), `true`(Movable)만 Transform이 실제로 바뀐 프레임에 갱신 경로를 탄다.
- **공용 "이동한 오브젝트" 목록 — Container는 공유, 소비 방식은 모드별로 분리, 동시성 보호 불필요**: `WorldObject::Update(float)`가 `Impl_Update(delta)` 호출 후 Transform이 실제로 바뀐 경우(`movable==true`만 해당, `Material`의 `dirty` 플래그 관례 재사용) 컬링 모드를 전혀 모르는 중립 컨테이너에 자기 자신(`SceneObjectID` + 새 Sphere)을 등록만 한다. 이 컨테이너는 `RenderScene`이나 `SceneRenderer` 어느 한쪽에 속하지 않는 공용 자료구조이며, 실제 소비는 활성 `frustumCullMode`에 따라 셋으로 갈린다 — `CPU_LooseOctree`는 `RenderScene::CullCommands()`가 드레인해 `octree.Update()` 호출, `GPU_Driven`은 `SceneRenderer::AddPasses()`의 GPU 분기가 드레인해 GPU 버퍼 부분 갱신(다음 GPU-Driven 항목 참조), **`CPU_BruteForce`는 아무도 이 데이터를 쓰지 않지만 목록이 무한정 쌓이는 걸 막기 위해 `RenderScene::CullCommands()`가 그대로 드레인만 하고 버린다**(브루트포스는 매 프레임 전체를 다시 수집하므로 이 목록의 내용 자체가 애초에 필요 없음) — 즉 `CullCommands()`는 `GPU_Driven`일 때만 이 목록에 손대지 않고 넘어가고, 나머지 두 CPU 모드에서는 항상 드레인한다(Octree 모드는 적용, 브루트포스 모드는 폐기). `WorldObject`가 Octree나 `SceneRenderer` 내부에 직접 접근하지 않아도 되게 하는 것이 목적 — `SceneRenderer`는 렌더러 내부 계층이라 Application 레이어의 `WorldObject`가 알아서는 안 됨. **동시성**: 기존 Octree 훅과 같은 근거 재사용 — 쓰기(`OnUpdate()` 단계, 순차)와 드레인(`CullCommands()`/`AddPasses()` 단계, `ThreadPool` 병렬 Query 시작 전에 먼저 순차 처리)이 프레임 안에서 시간적으로 완전히 분리돼 있어 동시 접근 자체가 없다 — 락 없는 큐나 이중 버퍼링 불필요, 평범한 `std::vector`에 쌓았다가 드레인 후 `clear()`로 충분. 유일한 전제는 "오브젝트 Update 자체가 병렬화되지 않는다"는 기존 캐비어트와 동일(병렬화되면 재검토, 기술부채로 기록).

**CPU-Driven**
- **Thread Pool 소유**: `IThreadPool`(인터페이스, `ITime`과 동일한 인터페이스+`Create()` 팩토리 패턴) + `ThreadPool`(구현체), `Locator<IThreadPool>`로 등록/접근. 기존 `TimerSystem`/`IPointerInput`이 이미 이 패턴이라 일관성 유지. `Singleton<T>`은 구체 타입만 감쌀 수 있어 인터페이스 분리가 깨져 채택하지 않는다(`Singleton<ThreadPool>`이면 인터페이스 없이 구체 클래스를 직접 노출해야 함). 워커 스레드 join도 `Locator::Unregister()` 전 명시적으로 처리해 종료 순서를 제어한다.
- **작업 실행 모델**: 공유 뮤텍스 큐 + `JobGroup`(대기 중인 작업 수를 세는 원자적 카운터 핸들). `pool.Enqueue(group, task)`로 제출, `group.Wait()`로 완료 대기. 작업 내부에서 같은 group에 자식 작업을 추가로 enqueue할 수 있어 브루트포스(정적 `ParallelFor`)와 Loose-Octree(동적 fork-join) 둘 다 하나의 메커니즘으로 지원한다. Lock-free work-stealing deque는 채택하지 않는다 — 작업량이 프레임당 유한해 공유 큐로 충분하고, 실측으로 큐 경합이 병목임이 확인되면 그때 업그레이드.
- **Debug 훅(§1-10)**: `ThreadPool::GetStats()` — 워커 수, 큐 깊이, 마지막 `JobGroup::Wait()` 블로킹 시간(=그 프레임 컬링 소요 시간) 노출.
- **Loose-Octree는 포인터 트리가 아니라 Morton 코드 기반 Hash(Linear Octree)로 구현**: 원점(월드 원점 (0,0,0))·셀 크기·축당 비트 수(21비트/축, x·y·z 합쳐 63비트로 `uint64_t` 1개에 수납, 셀 크기 1유닛 기준 축당 약 ±2,097,152유닛≈2,000km 범위) 세 값만 고정하면 되고, 별도의 트리 사전 구성이나 씬 스캔이 필요 없다 — 해시맵은 원래 비어있고 삽입 시점에 각 오브젝트의 Morton 키를 계산해 넣을 뿐이라 자연히 sparse하다. 2,000km를 넘는 월드가 필요해지면 이 옥트리를 억지로 더 늘리기보다 월드 자체를 청크로 나누는(스트리밍) 쪽이 맞다고 판단, 그 이상은 범위 밖으로 남긴다.
- **주소 공간 비트 수(범위)와 연산 깊이 하드캡(10, 재귀 안전성)은 서로 다른 축이라 안 부딪힌다**: 21비트는 "얼마나 먼 위치까지 표현 가능한가"이고, 깊이 10은 "쿼리·삽입 때 실제로 몇 단계까지 세분화해서 순회하는가"다. 21비트 주소 공간 안에서 연산은 10단계까지만 내려가고 그 밑은 한 리프에 모아 담는다.
- **기각된 대안들**: (a) 포인터 트리의 "루트 더블링"은 Hash 방식엔 애초에 "루트"가 없어 불필요해짐. (b) 루트를 카메라(Player) 위치로 잡고 매 프레임 따라가는 방식은 기각 — Morton 키가 원점 기준으로 계산되므로 원점이 매 프레임 바뀌면 정지해 있는 오브젝트도 매 프레임 재해싱해야 해서 §1-2가 막으려던 "매 프레임 재구축"과 동일한 문제가 재발한다. (c) 카메라 기준 거리대로 자식 노드를 나누는 아이디어는 컬링 옥트리가 아니라 향후 그리드/하이트맵 레벨 스트리밍(LOD·로드/언로드 판단) 쪽에 적용 대상으로 이관 — 컬링은 프러스텀과 월드 공간의 교차만 보면 되어 카메라와의 거리가 조직 원리일 이유가 없다.
- **`WorldObject` 생명주기 훅**: 별도 Register/Unregister API가 아니라 기존 `Impl_` NVI 패턴을 확장한다 — `WorldObject::Initialize()`(비가상 래퍼)가 `Impl_Initialize()` 호출 직후 로컬 바운드를 계산해 공용 이동 목록(공통 인프라 "공용 이동한 오브젝트 목록" 항목 참조)에 "신규 등록"으로 표시, `WorldObject::Update(float)`가 `Impl_Update(delta)` 호출 후 Transform이 실제로 바뀐 경우에만(Movable만 해당) "갱신"으로 표시, `~WorldObject()`가 "제거"로 표시한다 — Octree를 직접 호출하지 않는다. `RenderScene::CullCommands()`가 CPU 모드일 때 이 목록을 드레인해 실제 `octree.Insert()`/`Update()`/`Remove()`를 호출한다. 서브클래스는 전혀 신경 쓸 필요가 없다. 스레드 안전성은 `OnUpdate()`(쓰기) 전체가 `CullCommands()`(읽기, ThreadPool 병렬 Query) 시작 전에 끝나는 루프 순서 덕분에 별도 락 없이 확보된다(오브젝트 Update 자체가 병렬화되지 않는다는 현재 전제 하에 — 병렬화되면 재검토 필요, 기술부채로 기록). **필요한 작은 확장**: 비활성 모드(브루트포스/GPU)에서 드레인이 아무 것도 안 하려면 `Locator<T>::Get()`의 assert-on-null을 우회할 `IsRegistered()` non-asserting 체크가 `Locator.h`에 추가로 필요하다. **종료 순서 제약**: 모든 `WorldObject`는 옥트리/ThreadPool이 `Unregister`되기 전에 소멸해야 한다 — `Execute.cpp` 셧다운 시퀀스에 명시.
- **브루트포스는 상태 없는 순수 함수**: 클래스도, `Locator` 등록도 필요 없다. `RenderScene::CullCommands()`가 `frustumCullMode`로 분기해 직접 호출한다. `RenderCommand`(AoS)를 그대로 순회하면 캐시 효율이 떨어지므로, 바운딩 스피어만 모은 SoA 스크래치 버퍼를 매 프레임 새로 채워(가벼운 gather 패스) 사용한다 — 프레임 간 유지할 상태가 없어 옥트리 같은 영속화가 필요 없다.
- **병렬 컬링 결과 수집 — 알고리즘마다 다른 방식을 쓴다(공유 강제 안 함)**: 브루트포스는 스레드마다 연속된 인덱스 범위를 담당하므로 `bool visible[N]`(원본 배열과 동일 크기)에 디스조인트하게 쓰고 이후 단일 압축 스캔으로 최종 리스트를 만든다(락·병합 로직 불필요, 제출 순서 자동 보존). Octree는 트리를 동적으로 타고 내려가는 fork-join이라 고정 인덱스 공간이 없어 이 방식이 안 맞는다 — 대신 워커별 로컬 버퍼에 담고 `JobGroup::Wait()` 이후 병합한다. 더 정교한 "재귀 되감기 시 계층적 병합"은 우리가 채택한 `JobGroup`(태스크 간 부모-자식 결과 전달 없이 카운터만 세는 모델)과 맞지 않아 채택하지 않는다. 두 방식 모두 최종적으로 `CullingStats`(§1-10)엔 동일한 형태(visible/culled 카운트, 소요 시간)로 노출되어 Debug 훅은 알고리즘 차이를 몰라도 된다.

**GPU-Driven**
- **`ICommandSignature` 신설**: `Pipeline/`에 추가(`IPipelineState`와 같은 카테고리). 인다이렉트 버퍼 항목 구조는 새로 정의하지 않고 기존 `DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance)`의 파라미터 5개를 그대로 재사용 — D3D12 `D3D12_DRAW_INDEXED_ARGUMENTS`와 이미 1:1 대응.
- **`IRenderDevice::CreateCommandSignature(...)`**: 기존 `CreateBuffer`/`CreatePipelineState`류와 동일한 `[[nodiscard]] std::unique_ptr<T> CreateX(desc)` 팩토리 패턴.
- **`ICommandList::ExecuteIndirect(sig, maxCount, argBuffer, argOffset, countBuffer, countOffset)`**: `DrawIndexedInstanced` 옆 Commands 섹션(Compute Commands 아님)에 추가.
- **카운터는 `AppendStructuredBuffer` 대신 수동 `RWStructuredBuffer<uint>` + `InterlockedAdd`**: 확인 결과 `IRenderDevice::CreateBufferUAV(buffer, numElements, strideBytes)`엔 카운터 전용(D3D12 `CounterOffsetInBytes`) 파라미터가 없어 Append/Consume buffer를 직접 지원하지 않는다. `ExecuteIndirect`의 `countBuffer` 인자는 Append/Consume의 숨은 카운터와 무관하게 버퍼 어딘가의 UINT 하나를 읽는 것뿐이라, 평범한 `RWStructuredBuffer<uint>`를 카운터로 써도 기존 `CreateBuffer`+`CreateBufferUAV`만으로 충분 — RHI 확장 없음.
- **카운터 버퍼는 압축 결과(args) 버퍼와 별도 리소스로 분리**: 매 프레임 리셋이 4바이트만 건드리면 되고, `countBuffer` 인자에 오프셋 계산이 필요 없으며, Shadow 컬링(캐스케이드별) 확장 시 카운터를 캐스케이드마다 독립적으로 늘리기 쉽다.
- **카운터 리셋은 1-스레드 컴퓨트 셰이더로, 새 RHI 메서드 추가 안 함**: D3D12 `ClearUnorderedAccessViewUint`는 GPU 디스크립터와 CPU 디스크립터를 동시에 요구하는 예외적인 API라 `IResourceView`의 "뷰 하나=핸들 하나" 전제와 안 맞고, Vulkan엔 대응 요구사항이 없어(`vkCmdFillBuffer`는 버퍼+오프셋+크기만 받음) DX12 고유의 특이사항을 RHI에 새어 들어오게 하는 셈이 된다. 대신 `Dispatch(1,1,1)` + 기존 `SetComputeUAV`로 `counter[0]=0`을 실행하는 셰이더 하나로 대체 — BonePaletteCompute·SSAO 등에서 이미 검증된 패턴 재사용, RHI 확장 없음.
- **UAV 배리어는 기존 `UAVBarrier` 재사용**: 컴퓨트가 args/counter 버퍼에 쓴 뒤 `ExecuteIndirect`가 읽기 전, 새 배리어 메커니즘 없이 기존 `ICommandList::UAVBarrier`로 충분.
- **GPU 상주 바운딩볼륨 버퍼는 `SceneRenderer`가 소유하되, 생명주기는 §1-8을 따른다**: `DeclareTransientDepth`류(매 프레임 리셋)는 아니다 — 크로스 프레임 상태(Static은 1회만 씀, Movable만 부분 갱신)가 핵심이라 매 프레임 리셋되는 트랜지언트 패턴과 안 맞는다. 다만 GBuffer depth·hdrOut처럼 엔진 수명 내내 존재하는 리소스는 아니다 — §1-8에서 이미 "GPU 리소스를 항상 유지하지 않는다"고 확정했으므로, 이 버퍼도 `GPU_Driven`으로 전환될 때(§1-8 1단계 준비→할당) 생성되고 다른 모드로 전환될 때(§1-8 3단계) 해제된다. 즉 여기서 "영속"은 "활성 상태인 동안 프레임을 넘어 상태를 유지한다"는 뜻이지 "항상 존재한다"는 뜻이 아니다 — 이 구분이 없으면 §1-8과 모순된다. 컴퓨트 컬링 패스가 이 버퍼를 SRV로 읽을 때는 `RenderGraph`에 입력으로 선언해 배리어는 자동으로 잡히게 한다. 갱신 트리거는 공통 인프라 "공용 이동한 오브젝트 목록"을 `SceneRenderer::AddPasses()`의 GPU 분기가 드레인하는 방식 — `WorldObject`가 `SceneRenderer` 내부에 직접 접근하지 않는다.
- **컴퓨트 컬링 셰이더 — 스레드그룹 크기 64**: AMD 웨이브프론트(64)에 정확히 맞고 NVIDIA 워프(32)의 배수라 양쪽에서 부분 웨이브 낭비가 없는 표준적인 선택. 기존 `BonePaletteCompute`의 `[numthreads(250,1,1)]`은 그 셰이더 고유 맥락(본 개수 등)에 맞춘 특수값이라 이번 것과 무관한 선례 — 오브젝트당 스레드 하나인 평범한 1D 워크로드엔 64가 기본값. 디스패치 그룹 수(`ceil(objectCount/64)`)는 CPU가 컴퓨트 패스 등록 시점에 이미 아는 값이라 인다이렉트 디스패치 불필요, 꼬리 스레드는 `if (index >= objectCount) return;`로 처리.
- **컴퓨트 셰이더 핵심 로직**: 스레드당 `gBounds[index]`(Sphere) 읽어 6평면(`[unroll]`) 테스트 → 통과 시 `InterlockedAdd(gCounter[0], 1, writeIndex)`로 압축 인덱스 획득 → `gVisibleArgs[writeIndex]`에 인다이렉트 args 기록.
- **바운딩볼륨 버퍼 필드 레이아웃 — 초안만 확정, 세부는 구현 시점에**: 압축 기록에 필요한 `indexCount`/`startIndex`/`baseVertex`가 오브젝트가 아니라 메시의 속성이라 Sphere만으론 컴팩션이 불가능함이 셰이더 설계 중 드러남 — 버퍼 항목을 `{Sphere, meshIndex}`로 하고, 메시별 인다이렉트 args 템플릿은 인스턴스 수가 아니라 고유 메시 개수만큼만 별도의 작은 SRV 테이블에 담아 `meshIndex`로 조회(`RenderCommand::mesh`가 `IMesh*`로 공유 메시를 참조하는 것과 동일한 이유 — 인스턴스마다 복제 안 함). 머티리얼 인덱스 등 나머지 필드·정확한 바이트 정렬은 구현 시점에 확정.
- **Mobility(Static/Movable)는 Octree·GPU 버퍼에만 적용, 브루트포스엔 적용 안 함**: 이유가 두 가지로 구분된다. (1) 표현 형태 문제 — Octree(Morton 키 영속 저장)·GPU(영속 버퍼의 안정된 슬롯)는 프레임을 넘어 유지되는 식별자가 있어야 Static/Movable 구분이 의미가 있는데, 브루트포스는 매 프레임 처음부터 다시 채우는 SoA 스크래치라 이런 식별자가 없다. (2) 더 근본적인 이유 — 설사 (1)을 해결해도, 브루트포스의 지배적 비용(N개 전부 × 6평면 테스트)은 Static/Movable과 무관하게 매 프레임 그대로 다 돈다. Octree는 정적 서브트리를 통째로 건너뛰고 GPU는 업로드 자체를 생략해 실질적 이득을 보는 반면, 브루트포스엔 "테스트를 건너뛴다"는 개념 자체가 없어 아낄 수 있는 게 월드 스피어 재계산 정도의 작은 부분뿐이다. (1)만 해결해도 (2) 때문에 실익이 작아 적용하지 않는다 — 같은 최적화가 알고리즘에 따라 왜 다르게 먹히는지 보여주는 비교 소재로 남긴다.

→ 근거: [`2026-07-16_Q&A.md` Q4](../2026-07-16_Q&A.md#q4-gpu-driven-컬링--카운터-버퍼-분리-여부와-리셋-방식)

**계측(§1-9)**
- **readback 버퍼는 이미 있음**: `Common_RHI.h`의 `EMemoryAccess::Readback`(D3D12_HEAP_TYPE_READBACK)이 이미 정의돼 있어 `CreateBuffer({size, usage, EMemoryAccess::Readback, stride})`로 바로 생성 가능 — 신규 RHI 추가 불필요.
- **신규 RHI 4가지**: `IQueryHeap`(신설, `IRenderDevice::CreateQueryHeap(count)` 팩토리) / `ICommandList::WriteTimestamp(heap, index)`(D3D12 `EndQuery(TIMESTAMP,...)`를 감싸되 Begin 없이 End만 있는 API 이름 특이사항이 RHI로 새지 않게 개명) / `ICommandList::ResolveTimestamps(heap, startIndex, count, dstBuffer, dstOffset)`(`ResolveQueryData` 래핑, `dstBuffer`는 위 Readback 버퍼 재사용) / `ICommandQueue::GetTimestampFrequency()`(틱→밀리초 환산용).
- **측정 지점**: GPU 컬링 컴퓨트 디스패치 직전·직후 2슬롯(`WriteTimestamp(heap,0)`/`(heap,1)`) — §1-9가 요구한 "GPU-Driven을 CPU와 공정 비교"에 필요한 최소 구성. `ExecuteIndirect`까지 포함해 재고 싶어지면 슬롯을 늘리면 됨.
- **프레임 지연은 있으나 새 동기화 불필요**: 타임스탬프 값은 GPU가 그 프레임을 다 끝내야 CPU가 안전하게 읽을 수 있어 "몇 프레임 전 값"이 된다(PIX·RenderDoc 등도 동일). 새 동기화 primitive를 만들지 않고, 이미 있는 프레임 인플라이트 커맨드 얼로케이터 순환·Fence 대기 메커니즘에 쿼리힙/readback 버퍼도 같이 얹어 순환시키면 된다.
- **Debug 훅(§1-10)**: 측정 결과는 다른 계측치와 동일하게 `GetStats()`로 노출 — 새 예외 없이 기존 패턴 그대로.

**Shadow 컬링(§1-7)**
- **`ExtractFrustumPlanes` 호출 지점**: `ShadowMapRenderer`가 매 프레임 활성 캐스케이드 전체의 `lightViewProj[cascadeIndex]`를 미리 계산해두는 기존 단계(PSSM split + bounding-sphere 피팅, `2026-07-13_Log.md` Compact Log #1)를 확장한다 — 캐스케이드마다 `lightViewProj[cascadeIndex]`가 확정되는 즉시 평면을 뽑고(근평면 확장 포함, 아래 항목) 컬링 쿼리까지 실행해, 그 결과(캐스케이드별 가시 캐스터 리스트)를 `ShadowMapRenderer`가 캐스케이드 수만큼 배열로 보유한다. `RenderShadowMap(cmdList, scene, cascadeIndex)`는 이미 계산된 자기 캐스케이드의 리스트를 읽어 그리기만 한다 — `lightViewProj[]`와 동일한 "미리 계산 → 나중에 소비" 흐름이라 기존 구조와 어긋나지 않는다.
  - **CPU 모드**: `RenderScene::CullCommands()`를 거치지 않고 같은 쿼리 함수(§1-7에서 임의의 6평면을 받는 형태로 확정)를 캐스케이드의 평면으로 직접 호출한다. Octree 접근도 `CullCommands()`와 동일한 경로를 재사용 — 카메라 경로가 매 프레임 Octree를 최신 상태로 유지해주므로(공통 인프라 "공용 이동한 오브젝트 목록" 드레인), Shadow 쪽에서 별도로 그 목록을 드레인할 필요가 없다.
  - **GPU 모드**: 동일한 컴퓨트 셰이더를 캐스케이드마다 다른 CB(그 캐스케이드의 평면)로 재디스패치한다(§1-7 "코드가 8배 느는 게 아니다" 원칙). 입력 바운딩볼륨 버퍼도 카메라 경로와 동일한 `SceneRenderer` 소유 영속 버퍼를 그대로 읽는다 — Shadow 전용 입력 버퍼는 없고, 출력(카운터+압축 args)만 캐스케이드별로 별도(아래 항목).
- **Peter-panning 대응 — 근평면을 `camera.farZ`만큼 광원 반대 방향으로 확장**: 렌더링용(캐스케이드 피팅) 박스는 그대로 두고, 컬링 전용 평면 집합만 근평면을 확장한다. 정확한 씬 AABB를 매 프레임 추적하는 새 인프라를 두는 대신, `RenderScene::CameraData`에 이미 있는 `camera.farZ`(카메라가 볼 수 있는 최대 거리)를 재사용한다 — 카메라 far보다 훨씬 먼 캐스터는 애초에 화면에 영향을 줄 수 없으므로, 이 값을 근평면 확장 거리로 쓰면 새 트래킹 구조 없이 안전한 상한을 얻는다. 확장 배율(`nearPlaneExtensionScale`, 기본값 1.0)을 튜닝 파라미터로 남겨 필요시 조정 가능하게 한다.
- **캐스케이드별 마진 배율 — 선형 보간 기본값**: `marginScale[cascadeIndex] = lerp(nearMarginScale, farMarginScale, cascadeIndex/(activeCascadeCount-1))`, 기본값 `nearMarginScale=1.5`(근거리 50% 여유), `farMarginScale=1.0`(원거리 기본값). `splitLambda`/`blendWidth`와 동일하게 육안 튜닝 대상 시작값이며, 최종 곡선은 구현 후 실측·육안으로 조정한다.
- **GPU 압축 버퍼는 캐스케이드별로 완전히 분리, 개수는 8(최대 캐스케이드 수)로 고정**: 개수는 기존 CSM의 "최대 8캐스케이드"(`kMaxCascadeCount`, README §8) 패턴에 맞춰 캐스케이드마다 독립된 카운터 버퍼(4바이트)+args 버퍼 쌍 8세트로 정한다. **다만 할당·해제 시점은 섀도우 뎁스 배열과 다르다** — 섀도우 뎁스 배열은 컬링 모드와 무관하게 항상 존재하는 기존 리소스지만, 이 압축 버퍼들은 GPU-Driven 컬링 전용 데이터라 §1-8의 GPU 모드 전환 시퀀스(전환 시 준비→할당, 다른 모드로 전환 시 해제)를 그대로 따른다 — `GPU_Driven`이 비활성일 땐 8세트 전부 존재하지 않는다. 활성 캐스케이드 수만큼만 그 프레임에 실제로 디스패치·소비하는 것은 원안 그대로. 하나의 버퍼에 8구간을 나눠 담는 방식은 채택하지 않는다 — 메모리 총량은 동일하지만(구간 8개 vs 버퍼 8개, 바이트 수는 같음) 리셋·오프셋 계산이 각자 독립적이라 관리가 더 단순하다.

→ 근거: `2026-07-13_Log.md` Compact Log #1(캐스케이드 피팅), §1-7(Shadow 컬링 원칙)

**공통 인프라**
- 바운딩 볼륨 구조체의 정확한 필드/배치 위치(`Math/` 하위 정확한 파일명), 메시 로더의 기존 정점 순회 루프에 계산을 끼워 넣을 수 있는지 추후 확인
- `RenderScene::CullCommands()`의 정확한 시그니처(무인자 예정, 내부에서 `Math::ExtractFrustumPlanes` 호출)
- 브루트포스/Octree 쿼리 함수가 임의의 6평면을 받는 형태라는 원칙은 확정(§1-7) — 정확한 함수 시그니처는 CPU-Driven 세부 설계 시 확정

**CPU-Driven**
- Thread Pool의 정확한 워커 수 산정 방식(`hardware_concurrency()`-1 등), `Enqueue` 콜러블 타입(`std::function` 오버헤드 vs 경량 타입 소거)
- Morton 코드 인코딩의 정확한 비트 인터리빙 구현, 해시맵 자료구조 선택(`std::unordered_map` vs 커스텀 오픈 어드레싱)
- `CullingStats` 구조체의 정확한 필드(Octree 노드 수·깊이·이번 프레임 삽입/삭제/이동 이벤트 수 포함, §1-10)

**GPU-Driven**
- 바운딩볼륨 버퍼 필드의 정확한 바이트 정렬·머티리얼 인덱스 등 추가 필드 여부(초안 `{Sphere, meshIndex}` + 별도 메시 args 테이블은 확정), 초기 전체 업로드 vs Static/Movable 부분 갱신의 구체적 구현(소유·트리거 지점은 확정, §3 GPU-Driven 결정 항목)
- GPU 경로의 Debug 훅(§1-10) — GPU 타임스탬프 쿼리(§1-9)와 결합해 `GetStats()`로 노출할 형태

---

## 4. 완료 항목

*(항목이 구현 완료될 때마다 여기에 `~~취소선~~` + 해당 날짜 Log 링크를 추가한다)*
