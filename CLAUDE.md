# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## 빌드

모든 CMake 명령은 `VaEngine/` 디렉토리 기준으로 실행한다.

```powershell
# 구성 (프리셋 선택)
cmake --preset windows-directx      # DX12 메인 빌드
cmake --preset asset-importer       # VaImportTool (FBX 변환기)
cmake --preset engine               # 엔진 디버깅 전용

# 빌드
cmake --build Build/windows-directx
cmake --build Build/asset-importer

# 실행 (ExecWindows)
.\Build\windows-directx\Platforms\ExecWindows\Debug\ExecWindows.exe

# VaImportTool 실행 (FBX → .mesh/.smesh/.clip)
.\Build\asset-importer\Tools\VaImportTool.exe
```

셰이더(`.hlsl`)를 수정하면 빌드 시 `VaShaderCompiler`가 자동으로 `.cso`를 재생성한다. 별도 셰이더 컴파일 명령은 불필요하다.

테스트 시스템은 없다.

---

## 아키텍처

### 레이어 구조

```
Platform (ExecWindows / ExecAndroid)
    └─ Execute::OnLoop()            [Engine] — 프레임 루프 오케스트레이터
           ├─ management->OnUpdate()        [Application] — 게임 로직
           ├─ management->SubmitRenderState() — RenderScene 스냅샷 제출
           └─ RenderGraph::Execute()        [Engine] — GPU 커맨드 기록/실행
```

- **Engine** (`Engine/`): Static Library. RHI 추상화, 렌더러, 애니메이션, 에셋 로더. `ApplicationManager` 인터페이스를 모름.
- **Application/Core** (`Application/Core/`): FreeCamera, LightManager, Time 유틸.
- **Application/Source** (`Application/Source/`): 프로그램 고유 로직. `VaProgramName`이 `ApplicationManager` 구현. `WorldObject` 서브클래스 정의.
- **Platform** (`Platforms/`): OS 진입점. `IExecute` 구현체를 인스턴스화하고 루프 실행.

### RHI 계층

모든 GPU 리소스는 `Engine/Public/RHI/` 인터페이스를 통해 접근한다.  
DX12 구현체는 `Engine/Private/RHI/DirectX/`에 위치하며 `USE_DIRECTX` 매크로로 활성화된다.

중요 제약:
- DX12는 shader-visible CBV/SRV/UAV heap을 한 번에 하나만 바인딩 가능. `RenderDevice_DirectX`의 **전역 SRV Heap**(1024 slots)에서 모든 텍스처가 슬롯을 할당받는다. 텍스처별 private heap 생성 금지.
- 리소스 상태 전환(Barrier)은 항상 명시적으로 기록해야 한다.

### WorldObject 계층

```
WorldObject                 (Engine/Public/Object/)  — Transform 소유
├─ WO_Cube / WO_Tower       (Application/Source/)    — 정적 메시
└─ WorldAnimatedModel       (Engine)                 — 스켈레탈 메시
       └─ WO_Kachujin       (Application/Source/)
```

`WorldObject` 서브클래스는 `Initialize(IRenderDevice*)` + `Update(float)` + `AddToScene(RenderScene&)` 패턴으로 구성한다.

### 스켈레탈 애니메이션 파이프라인

```
FBX → VaImportTool → .smesh + .clip × N
    수동 복사 → _Assets/{name}/

런타임:
SkmLoader → Skeleton + SkinnedMesh
ClipLoader → AnimClipData (boneNames 포함, version 2)
BakeTransformsMap → Texture2DArray [clipIdx][frameIdx][boneIdx×4]
AnimController → TweenFrameDesc (CB_TweenFrame, b3)
AnimationDemo.hlsl → ComputeSkinMatrix + inter-clip lerp
```

- `.smesh`와 `.clip`의 본 수/순서가 달라도 된다. `BakeTransformsMap`은 `unordered_map<name, idx>`로 이름 기반 매핑을 수행한다.
- 클립 전환: `WorldAnimatedModel::PlayTween(nextClip, blendTime)` → `AnimController`가 `tweenTime` 0→1 증가, HLSL에서 `lerp(currentSkinMat, nextSkinMat, TweenTime)`.

### 에셋 포맷

| 확장자 | 내용 | 로더 |
|---|---|---|
| `.mesh` | 정적 메시 (MeshAsset.h, version 1) | `MeshLoader` |
| `.smesh` | 스켈레톤 + 스키닝 정점 (SkmAsset.h, version 1) | `SkmLoader` |
| `.clip` | 애니메이션 키프레임 + 본 이름 (ClipAsset.h, version 2) | `ClipLoader` |

`.clip` version 2부터 bone 이름 배열이 필수. 하위 호환 없음.

---

## 네이밍 규칙

| 대상 | 규칙 | 예시 |
|---|---|---|
| 변수 | camelCase | `runningTime`, `clipCount` |
| 함수 / 클래스 / 구조체 | PascalCase | `BakeTransformsMap`, `AnimController` |
| 순수 가상 클래스 | `I` 접두사 | `IRenderDevice`, `ITexture` |
| 공통 구현 포함 가상 클래스 | `Base` 접두사 | `BaseCamera` |
| NVI 훅 / Bridge 구현 함수 | `Impl_` 접두사 | `Impl_Draw()` |

---

## 코드 규칙

- COM 객체는 반드시 `ComPtr<T>` 사용 (RAII).
- `HRESULT` 반환값은 항상 검사. 실패 시 예외(`std::runtime_error`) 처리.
- 커맨드 큐/리스트는 용도별로 분리 (Direct / Copy / Compute).
- 새 텍스처/SRV 생성 시 `RenderDevice_DirectX::AllocateSRVDescriptor()`로 전역 힙에서 슬롯 할당.
- 소스코드는 사용자가 직접 작성한다. 요청 없이 구현 코드를 먼저 제안하지 않는다.
- 파일 이동 / 이름 변경은 IDE(VS / Rider)에서만 수행한다. 파일시스템 직접 조작 금지.

---

## 문서 위치

- 프로젝트 현황판: `Agent/AgentLog/README.md`
- Q&A / Log: `Agent/AgentLog/{YYYY-MM-DD}_Q&A.md` / `_Log.md`
- 구현 계획: `Agent/AgentLog/Plan/`
- 메모리: `C:\Users\User\.claude\projects\E--Project-VaEngine\memory\`
