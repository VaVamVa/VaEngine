# VaEngine 디버깅 및 로그 전략 계획 (Plan_Debugging_Logs.md)

본 문서는 포트폴리오 전시 및 엔진 개발 시 내부 상태를 시각화하기 위해 로그를 남겨야 할 핵심 지점들을 정의합니다.

---

## 1. RHI 리소스 생명주기 (Resource Lifetime)
가장 기초적인 안정성을 증명합니다.
- **지점**: `RenderDevice::CreateBuffer`, `CreateTexture`, `CreateShader` 및 각 소멸자.
- **내용**: 리소스의 유형, 크기(VRAM), 할당 주소 로그.
- **효과**: "엔진이 메모리 누수 없이 자원을 관리하고 있음"을 입증.

## 2. RenderGraph 배리어 전환 (Barrier Transitions) - [핵심]
RenderGraph의 기술력을 시각화하는 가장 좋은 방법입니다.
- **지점**: `RenderGraph::Compile` 및 `Execute`.
- **내용**: "Pass [A] -> Pass [B] 전환 시 Resource [C]가 [State_Present]에서 [State_RenderTarget]으로 변경됨"을 출력.
- **효과**: 자동 배리어 삽입 로직의 정확성을 증명.

## 3. 64비트 소팅 결과 (Sorting Key)
렌더링 최적화 로직을 보여줍니다.
- **지점**: `RenderScene::SortCommands`.
- **내용**: 정렬 전/후의 커맨드 목록 및 상위 10개 객체의 `SortKey` 비트값 분석 출력.
- **효과**: "불투명/반투명 및 재질별 정렬 최적화"를 수행하고 있음을 입증.

## 4. 프레임 통계 (Frame Statistics)
실시간 성능을 모니터링합니다.
- **지점**: `Execute::OnLoop`.
- **내용**: FPS, DeltaTime, 현재 프레임의 총 드로우 콜(Draw Call) 횟수, 제출된 Mesh 개수.
- **효과**: 엔진의 기본적인 성능 지표 확인.

## 5. 입력 및 시스템 이벤트 (Input & System)
- **지점**: `InputSystem::Update`, `Execute::OnSuspend/Resume`.
- **내용**: 눌린 키/마우스 버튼 정보, 안드로이드/윈도우 포커스 전환 이벤트.
- **효과**: 멀티 플랫폼 대응 및 입력 처리의 유연성 증명.

---

## 실시간 화면 출력(DrawText) 우선순위
포트폴리오 영상 촬영 시 화면 좌측 상단에 고정 출력할 항목:
1. **API 정보**: 현재 Graphics API (DirectX 12 / Vulkan)
2. **성능 정보**: FPS / FrameTime (ms)
3. **씬 정보**: Total Objects / Active Pass Count
4. **상태 정보**: 현재 카메라 좌표 (Pos: x, y, z)
