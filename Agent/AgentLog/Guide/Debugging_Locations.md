# VaEngine Debugging Locations

이 문서는 엔진 전반에 부착된 로깅(`VA_LOG`) 및 실시간 패널(`VA_DRAW_PANEL`)의 위치와 용도를 추적하기 위해 작성되었습니다. 

## 1. 단발성 로깅 (`VA_LOG`)
초기화, 리소스 생성, 시스템 상태 변경 등 단발성 이벤트 추적에 사용됩니다. 로그 파일 비대화를 막기 위해 프레임 루프(Update/Render) 내부에는 사용을 지양합니다.

| 카테고리 | 위치 (File) | 용도 및 시점 |
|:---:|---|---|
| **Execute** | `Engine/Private/Execute.cpp` | 엔진 라이프사이클 (초기화, 소멸) |
| **System** | `Engine/Private/Execute.cpp` | OS 레벨 애플리케이션 상태 (Suspended, Resumed) |
| **RHI** | `Engine/Private/Execute.cpp` | 렌더링 백엔드 선택 (DirectX 12 / Vulkan 활성화) |
| **RHI** | `Engine/Private/RHI/DirectX/RenderDevice_DirectX.cpp` | 각종 GPU 리소스(Texture, Buffer, Pipeline 등) 생성 시점 |
| **Scene** | `Engine/Public/Scene/RenderScene.h` | 렌더링 커맨드 정렬 시작 시점 (디버깅 빌드에서 주로 활용) |
| **HelloCompute**| `Engine/Private/Demo/HelloCompute.cpp` | 초기 Compute Shader 인프라 자기 검증 성공/실패 여부 |
| **RenderGraph** | `Engine/Private/Render/RenderGraph.cpp` | Transient 리소스 (Depth Buffer 등) 최초 생성 시점 |
| **Asset** | `Application/Source/Private/VaProgramName.cpp` | 리소스별 로드 완료 시점 (Kachujin clips, Cube, Tower, HDRi) |
| **Light** | `Application/Source/Private/VaProgramName.cpp` | Directional light 활성화 시점 |

---

## 2. 실시간 패널 출력 (`VA_DRAW_PANEL`)
매 프레임 갱신되는 연속적인 데이터(프레임율, 드로우 콜, DAG 통계 등)를 모니터링하기 위해 사용됩니다. 화면 좌측 상단 등에 실시간 텍스트로 렌더링됩니다.

| 패널 Key | 위치 (File) | 출력 데이터 | 설명 |
|:---:|---|---|---|
| **0** | `Engine/Private/Execute.cpp` | `FPS: 00.0 (0.00 ms)` | 현재 프레임률 및 Delta Time |
| **1** | `Engine/Private/Execute.cpp` | `Draw Calls: N` | 이번 프레임에 제출된 전체 드로우 콜(렌더 커맨드) 개수 |
| **2** | `Engine/Private/Render/RenderGraph.cpp` | `RenderGraph: N Passes, M Barriers` | DAG 파이프라인 상의 렌더 패스 개수 및 자동 주입된 배리어 개수 |
| **3** | `Application/Source/Private/VaProgramName.cpp` | `Camera: (x.xx, y.xx, z.xx)` | FreeCamera 월드 위치 |
| **4** | `Application/Source/Private/VaProgramName.cpp` | `Yaw: 000.0  Pitch: 00.0` | FreeCamera 회전 각도 (도 단위) |
| **5** | `Application/Source/Private/VaProgramName.cpp` | `BlendAlpha: 0.00` | 애니메이션 블렌딩 가중치 상태 |
| **6** | `Application/Source/Private/VaProgramName.cpp` | `Clip[N]: ClipName` | 현재 재생 중인 애니메이션 클립 인덱스 및 이름 |
| **99** | `Engine/Private/Demo/HelloCompute.cpp` | `Compute: PASS / FAIL` | Compute Shader 인프라 테스트 결과의 실시간 유지 |
