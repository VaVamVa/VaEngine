# ToDo

끝난 `## Phase` 는 아래의 `# Finished`로 순차적으로 이동

## Phase 2

멀티플랫폼 프러스텀 컬링(CPU-Driven·GPU-Driven) 구현. VaEngine을 "빠른 상품화"가 아니라 "최적화 선택지와 적용 시점을 비교·수치로 보여주는" 포트폴리오로 삼는다는 방향에 따라, 브루트포스/Loose-Octree(CPU, 멀티스레드)와 GPU Compute Culling(Desktop 전용)을 모두 구현하고 런타임/컴파일타임 전환이 가능하게 만든다. 근거·설계 논의는 [2026-07-16_Q&A.md](./2026-07-16_Q&A.md) 참조.

1. 공통 선행 인프라 — 아래 CPU/GPU 경로가 공통으로 의존
    - [ ] 바운딩 볼륨 데이터 추가 (AABB/Sphere) — `RenderCommand`/`WorldObject`에 필드 추가, 메시 로컬 바운드 계산(런타임 1회 캐시 또는 에셋에 굽기) + 매 프레임 `worldMatrix` 기반 월드공간 갱신 (Phase 4-3 Collider의 바운딩 수식과 겹칠 가능성 있음 — 서로 독립 구현 가능하나, 재사용 가능하도록 범용 위치(Math/)에 배치 권장)
    - [ ] 카메라 프러스텀 6-평면 추출 — `BaseCamera`/`RenderScene`에 view-proj 기반 추출 로직 신설(현재 전무 확인)
    - [ ] `RenderScene`에 컬링 모드 런타임 토글 필드 추가 — 기존 `ssaoEnabled`/`iblEnabled`/`showCascades` 패턴 재사용("무엇을 그릴 것인가" 데이터로 취급)
    - [ ] `SceneRenderer`/`RenderGraph`에 컬링 경로 조립 지점 신설 — GPU-Driven 코드 자체는 API(DX12/Vulkan) 축이 아니라 GPU 아키텍처(IMR/TBDR) 축으로 컴파일 배제해야 함(`Engine/CMakeLists.txt`가 이미 쓰는 `ANDROID` 매크로 등 플랫폼 기준 — `USE_DIRECTX`/`USE_VULKAN`은 API 선택이라 이 용도로 쓰면 안 됨, Desktop-Vulkan 빌드가 생겨도 IMR이면 GPU-Driven이 있어야 하므로). Desktop 내 CPU↔GPU 전환은 `RenderScene` 런타임 필드로 토글("어떻게 그릴 것인가" 실행 정책으로 취급, RHI는 CPU/GPU 개념을 모르게 유지)
    - [ ] `RenderScene::AddMesh`/`AddSkinnedMesh` 결과에 컬링 필터링 단계 삽입 지점 마련 — 브루트포스·Octree 공용 인터페이스
2. CPU-Driven 컬링 — 모바일 필수 경로, GPU 경로보다 의존성 적어 우선 구현
    - [ ] 멀티스레딩 인프라 (Thread Pool / Job System) — Phase 3 "병렬처리 가능한 부분들 탐색, 적용" 항목을 이 작업으로 구체화해 진행
    - [ ] 브루트포스 병렬 컬링 — SoA 배열 + 스레드 균등 분할, 락 없는 결과 기록. 가장 단순해 먼저 구현해 비교 baseline 확보
    - [ ] Loose-Octree 컨테이너 + 영속성 정책 — `WorldObject` 생성/삭제/이동 훅으로 증분 갱신(매 프레임 재구축 금지, `RenderScene::Clear()` 주기와 분리된 저장 위치 필요)
    - [ ] Loose-Octree 병렬 컬링 — 상위 레벨 fork-join 태스크 분배 + work-stealing 부하 분산
    - [ ] (선택) Octree 노드 경계 디버그 시각화 — 기존 `DebugLineRenderer` 재사용
3. GPU-Driven 컬링 — Desktop 전용, RHI 확장 필요해 CPU 경로 이후 진행
    - [ ] RHI `ExecuteIndirect`/Indirect Dispatch 지원 추가 (현재 전무 확인)
    - [ ] GPU 상주 바운딩볼륨 구조화 버퍼 업로드 경로
    - [ ] 컴퓨트 컬링 셰이더 — 평면 테스트 + 수동 카운터(`RWStructuredBuffer<uint>` + `InterlockedAdd`)로 가시 인덱스 압축(`AppendStructuredBuffer`는 RHI가 카운터 전용 UAV를 지원하지 않아 채택 안 함 — `Plan_Phase2.md` §3 GPU-Driven 참조)
    - [ ] 압축된 가시 리스트 → Indirect Draw 연결
4. 계측/비교 인프라 — 비교 대상(CPU 2종·GPU 1종)이 갖춰진 뒤에 의미 있어 마지막 진행
    - [ ] GPU 타임스탬프 쿼리 (`ID3D12QueryHeap` + `D3D12_QUERY_TYPE_TIMESTAMP`, 현재 전무 확인) — CPU `std::chrono`(`Time.cpp`)만으론 비동기 GPU 실행 시간 측정 불가
    - [ ] `VA_DRAW_PANEL` HUD에 Visible/Culled 오브젝트 수·CPU/GPU 컬링 소요 시간 표시
    - [ ] 스트레스 테스트 씬(오브젝트 N개 그리드 스폰) — 현재 씬 규모론 브루트포스/Octree/GPU 간 차이가 수치로 드러나지 않음
    - [ ] UI 추가 작업 — 컬링 모드 전환·오브젝트 수 조절을 키 입력 대신 UI로 노출하는 데 사용(기존 Phase 4 항목에서 이동)
        - Input Box
        - Check Box
        - Slider

예상 구현 순서: 1(공통 인프라) → 2(CPU-Driven, 모바일 우선순위) → 3(GPU-Driven, Desktop) → 4(계측/비교). 2·3은 서로 독립적이라 순서를 바꿔도 되지만, RHI 확장이 필요한 3이 상대적으로 더 오래 걸려 2를 먼저 끝내는 편이 중간 산출물(모바일 대응 완료)을 더 빨리 확보할 수 있음.

## Phase 3

1. [ ] AgentLog/Readme.md 및 AgentLog 內 파일들 정리. `문서화`
2. 화면 크기 동적 조절 대응
    - windows의 경우 width와 height가 고정되어서 적용되고 있는데, 이를 동적으로 변경 가능하게 하고, Engine 또한 이를 인식하게.
    - 현재 화면 크기를 바꾸면 Application에서 동작 중인 Click 행위가 제대로 이뤄지지 않음
    - [ ] 해상도 하드코딩 정리 — `ExecWindows.cpp:42`(CreateWindow), `Execute.cpp:69-70/81/263-264`(SwapChain·SceneRenderer·FrameOutput), `CameraManager.cpp:36`(FreeCamera 종횡비), `VaProgramName.cpp:153,186`(ScreenToRay/WorldToScreen) — 총 4개 파일 6곳
    - [ ] `RenderGraph`의 트랜지언트 depth 캐시(`DeclareTransientDepth`)에 desc Update/Evict 로직 추가 — 화면 크기에 비례하는 트랜지언트 리소스가 생기면 handle을 유지한 채 재생성해야 하는데, 현재는 재생성 시 옛 desc의 캐시 항목이 orphan으로 남음(GPU 메모리 누수). (2026-07-12 갱신: `RegisterPersistentResource`/`UnregisterPersistentResource`는 `BaseRHIResource` 리팩터링으로 완전히 삭제됨 — 영속 리소스 쪽은 이제 별도 등록/해제 자체가 불필요해져 이 항목과 무관해졌음. 트랜지언트 depth 캐시의 Update/Evict만 남은 과제)
3. 병렬처리 가능한 부분들 탐색, 적용
    - 멀티 스레딩으로 성능 개선
    - (→ Phase 2-2에서 프러스텀 컬링 멀티스레딩으로 구체화되어 진행)
4. [ ] 모의 면접 중 나온 문제점 분석 및 수정
    - [모의 면접 대화 내역](./Plan/AI%20Catch%20Problems_2026-05-26_03-38-47.json)
5. [ ] [전체 Refactoring](./Plan/Refactoring_At260711.md) — 의도적으로 미룬 항목(Vulkan 픽셀 포맷 매핑·`EBindingType::Sampler` 일반화·`.matl` 파서 일반화)

## Phase 4

1. [x] Timer 시스템 구현 (반복, 단일)
    - [ ] bLoop parameter를 두어 반복 동작 가능하게 만들기
2. [ ] Camera 추가 구현
    1. [ ] Camera에 World Object 처럼 별도의 Transform(Transform.h) 을 적용하는 것에 대한 피드백 및 (필요하다면) 수정
    2. [ ] Free Camera에서 Input 완전 분리 (현재 고정형 NameKey 사용 안하면 Free Cam 사용 불가)
    3. [ ] TopView Camera (Pointer 혹은 Character Zone이 창 Deadzone 진입 시 World 이동)
    4. [ ] Transform 위탁 고정식 Camera (FPS)
    5. [ ] Camera With Boom (Cam Chase with Easing)
3. [ ] Collider 구현
    - [ ] 바운딩 볼륨 타입 공유 — Sphere/AABB는 Phase 2(컬링)에서 정의하는 Math/ 공용 구조체 재사용, Capsule은 이 항목에서 신규 정의
    1. [ ] CapsuleShape 구현
        - [ ] `CubeShape`/`IcoSphereShape`/`UVSphereShape`와 동일 패턴의 절차적 지오메트리 생성
    2. [ ] Mesh에서 이용할 Collision Component 구현 (충돌 관련 DebugLine, 수식 존재)
        - [ ] 기존 임시 구현 승격 — `VaProgramName.cpp::PointerPickTest()`의 하드코딩된 Ray-Sphere 교차 테스트(중심/반지름 매직넘버, Application 레이어 1회성 코드)가 사실상 이 항목의 출발점. Engine 레벨 재사용 가능한 함수로 승격
        - [ ] Shape-vs-Shape 교차 테스트 함수군 확장 — Ray-Capsule, Ray-AABB, Sphere-Sphere, Capsule-Capsule, Capsule-Mesh(narrow-phase, 가장 복잡)
        - [ ] 설계 결정 필요 — 이 엔진은 `WorldObject` 상속 계층 기반이라 Component(컴포지션) 시스템 자체가 없음. Collider를 `WorldObject` 서브클래스 필드로 둘지, 별도 Component 추상화를 새로 도입할지 결정
        - [ ] 충돌 판정 결과 시각화/응답 — 접촉 지점·노멀을 `DebugLineRenderer`로 표시(기존 `pickRayLineHandle`+`TimerSystem` 패턴 재사용), 판정만 할지 반발/밀어내기까지 할지 범위 확정
4. Level(World) 적용. (height map으로..?)
5. [ ] 모자 등 장식의 동적 착탈(런타임 equip/unequip) — [참고 자료](./2026-07-12_Log.md)의 Compact Log #4. 엔진은 이미 리깅된 서브메시 방식을 지원하나, 별도 export된 액세서리를 런타임에 캐릭터 스켈레톤에 추가하는 기능은 없음. 실제 모자 에셋 확보 시 설계
    - 서브메시 vs Parenting vs Constraint 중 고민

## Phase 5

1. [ ] Online Multiplay 환경 구축
    - [ ] RPC 시스템 설계
2. [ ] Directional Light 외에 Shadow, Transparent 객체에 대한 Shadow Lookup
3. [ ] Particle 시스템 구현
4. [ ] [Texture Streaming](./2026-07-13_Q&A.md#3-텍스처-스트리밍--더-정교한-버전) 으로 VRAM 최적화


# Finished

## Phase 1

0. [x] [Refacoring](./Plan/Plan_RenderQuality.md) — Pre-0~3 + Phase 1-1~4 전체 완료(2026-07-12), 부수적으로 `RenderGraph` 리소스 상태 캡슐화(`BaseRHIResource`) 리팩터링까지 완료·로그로 검증 완료(`Plan/Refactoring_RenderGraph-Buffer.md` 참조)
1. [x] HDR 활용. Tone Mapping — 완료, 육안 검증 완료
2. [x] Normal Mapping 구현 — 완료, 육안 검증 완료
3. [x] Shadow 시스템 구현 (Stage A: 단일 Directional Light) — 완료. CSM(Stage B)도 구현 완료(2026-07-13, `2026-07-13_Log.md` Compact Log #1~9) — 기술 부채 표(위 "기술 부채 해결 1" 3번) 갱신 완료
4. [x] PostProcessing 구현 (Bloom만) — 완료

## 기술 부채 해결 1

1. G_Smith IBL k 분리. [참고자료](./Plan/HDRI&IBL.txt)
    - [x] IBL
    - [x] SSAO — Compact Log #6, #7
    - [x] OIT — Compact Log #8
2. World Space 기반 렌더링 -> Scene Scale 기반 렌더링(1 unit = 1m 확정). 전환 시 IBL 등 이전에 육안으로 튜닝한 값도 재검증 필요 — 대상 소스 경로는 [이전 Log](./2026-07-11_Log.md)의 Compact Log #4 참조
    - [x] Converter.cpp 단위 보정(cm→m) 구현 완료 — Compact Log #1
    - [x] VaImportTool 재빌드 + Kachujin FBX 재-임포트 + `_Assets/` 복사(사용자 수행) + `VaProgramName.cpp`의 `SetScale(0.01f,...)` 제거
    - [x] Transform Scale/ScaleMultiplier 분리, `.matl` scale= 지원
    - [x] WorldObject Local/World Hierarchy — Compact Log #3 (CMake 재구성 필요, 빌드·육안 검증 대기)
3. [x] CSM — `2026-07-13_Log.md` Compact Log #1~9, 구현·버그 수정·육안 검증 전부 완료. Texture2DArray Depth(최대 8캐스케이드, 활성 개수 런타임 조정 Num4/5), smooth blend, 캐스케이드 색상 오버레이(Num3). 버퍼 재사용 타이밍(#3)·행렬 row/column 의미론(#7)·HLSL cbuffer 스칼라 배열 패킹 오해(#9, `gActiveCascadeCount`/split 경계값 오염) 3개 버그 발견·수정. 텍셀 스냅핑·캐스케이드별 컬링은 별도 기술 부채로 남김
4. [x] Skinned Mesh Shadow