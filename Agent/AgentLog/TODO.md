# ToDo

끝난 `## Phase` 는 아래의 `# Finished`로 순차적으로 이동

## Phase 2

1. 화면 크기 동적 조절 대응
    - windows의 경우 width와 height가 고정되어서 적용되고 있는데, 이를 동적으로 변경 가능하게 하고, Engine 또한 이를 인식하게.
    - 현재 화면 크기를 바꾸면 Application에서 동작 중인 Click 행위가 제대로 이뤄지지 않음
    - [ ] 해상도 하드코딩 정리 — `ExecWindows.cpp:42`(CreateWindow), `Execute.cpp:69-70/81/263-264`(SwapChain·SceneRenderer·FrameOutput), `CameraManager.cpp:36`(FreeCamera 종횡비), `VaProgramName.cpp:153,186`(ScreenToRay/WorldToScreen) — 총 4개 파일 6곳
    - [ ] `RenderGraph`의 트랜지언트 depth 캐시(`DeclareTransientDepth`)에 desc Update/Evict 로직 추가 — 화면 크기에 비례하는 트랜지언트 리소스가 생기면 handle을 유지한 채 재생성해야 하는데, 현재는 재생성 시 옛 desc의 캐시 항목이 orphan으로 남음(GPU 메모리 누수). (2026-07-12 갱신: `RegisterPersistentResource`/`UnregisterPersistentResource`는 `BaseRHIResource` 리팩터링으로 완전히 삭제됨 — 영속 리소스 쪽은 이제 별도 등록/해제 자체가 불필요해져 이 항목과 무관해졌음. 트랜지언트 depth 캐시의 Update/Evict만 남은 과제)
2. 병렬처리 가능한 부분들 탐색, 적용
    - 멀티 스레딩으로 성능 개선
3. [ ] 모의 면접 중 나온 문제점 분석 및 수정
    - [모의 면접 대화 내역](./Plan/AI%20Catch%20Problems_2026-05-26_03-38-47.json)
4. [ ] [전체 Refactoring](./Plan/Refactoring_At260711.md) — 의도적으로 미룬 항목(Vulkan 픽셀 포맷 매핑·`EBindingType::Sampler` 일반화·`.matl` 파서 일반화)

## Phase 3

1. UI 추가 작업
    - Input Box
    - Check Box
    - Slider   
2. [x] Timer 시스템 구현 (반복, 단일)
    - [ ] bLoop parameter를 두어 연속 동작 가능하게 만들기
3. [ ] Camera 추가 구현
    1. Camera에 World Object 처럼 별도의 Transform(Transform.h) 을 적용하는 것에 대한 피드백 및 (필요하다면) 수정
    2. Free Camera에서 Input 완전 분리 (현재 고정형 NameKey 사용 안하면 Free Cam 사용 불가)
    3. TopView Camera (Pointer 혹은 Character Zone이 창 Deadzone 진입 시 World 이동)
    4. Transform 위탁 고정식 Camera (FPS)
    5. Camera With Boom (Cam Chase with Easing)
4. [ ] Collider 구현
    1. CapsuleShape 구현
    2. Mesh에서 이용할 Collision Component 구현 (충돌 관련 DebugLine, 수식 존재)
5. [ ] 모자 등 장식의 동적 착탈(런타임 equip/unequip) — [참고 자료](./2026-07-12_Log.md)의 Compact Log #4. 엔진은 이미 리깅된 서브메시 방식을 지원하나, 별도 export된 액세서리를 런타임에 캐릭터 스켈레톤에 추가하는 기능은 없음. 실제 모자 에셋 확보 시 설계
    - 서브메시 vs Parenting vs Constraint 중 고민

## Phase 4

1. [ ] Online Multiplay 환경 구축
    - [ ] RPC 시스템 설계
2. [ ] Directional Light 외에 Shadow, Transparent 객체에 대한 Shadow Lookup
3. [ ] Particle 시스템 구현
4. [ ] [Texture Streaming](./2026-07-13_Q&A.md#3-텍스처-스트리밍--더-정교한-버전) 으로 VRAM 최적화


# Finished

## Phase 1

0. [x] [Refacoring](.\Plan\Plan_RenderQuality.md) — Pre-0~3 + Phase 1-1~4 전체 완료(2026-07-12), 부수적으로 `RenderGraph` 리소스 상태 캡슐화(`BaseRHIResource`) 리팩터링까지 완료·로그로 검증 완료(`Plan/Refactoring_RenderGraph-Buffer.md` 참조)
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