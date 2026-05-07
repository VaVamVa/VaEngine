# DirectX 12 큐브 메시 렌더링 구현 계획

이 문서는 DirectX 12 환경에서 정육면체(Cube Mesh)를 화면에 렌더링하기 위한 구현 단계와 구조를 정의합니다. DX11의 암시적 상태 관리와 달리, DX12의 명시적 파이프라인(PSO) 및 리소스 관리 원칙에 맞춰 설계되었습니다.

---

## 1. 디렉토리 구조 및 파일 배치

렌더링 로직의 모듈화를 위해 `Agent` 디렉토리를 신설하고 관련 파일들을 배치합니다.

*   **인터페이스 및 데이터 구조 (Public)**
    *   `Engine/Public/RHI/Agent/CubeRenderer.h`: 큐브 렌더링을 담당하는 핵심 클래스의 인터페이스 정의.
*   **DirectX 12 구현체 (Private)**
    *   `Engine/Private/RHI/Agent/DirectX/CubeRenderer_DX12.h`: DX12 전용 리소스(PSO, RootSignature 등)를 멤버로 가지는 클래스 선언.
    *   `Engine/Private/RHI/Agent/DirectX/CubeRenderer_DX12.cpp`: 실제 DX12 API를 활용한 초기화 및 렌더링 로직 구현.
*   **셰이더 파일 (Shaders)**
    *   `Engine/Private/RHI/DirectX/_Shaders/CubeShader.hlsl`: 큐브 렌더링용 버텍스 및 픽셀 셰이더.

---

## 2. 핵심 구현 단계 (Phase 1 ~ 3)

### Phase 1: 셰이더 및 파이프라인 상태(PSO) 준비
DX12에서는 그리기 전에 그래픽스 파이프라인의 모든 상태를 미리 정의해야 합니다.

1.  **HLSL 셰이더 작성**: 위치(Position)와 색상(Color)을 입력받아 변환 없이(World/View/Proj 없이 일단 화면 좌표계로) 출력하는 아주 간단한 셰이더 작성.
2.  **루트 시그니처(Root Signature) 생성**: 셰이더가 외부 리소스(상수 버퍼 등)를 어떻게 사용할지 정의. 초기 버전은 파라미터가 없는 빈 시그니처로 구성.
3.  **파이프라인 상태 객체(PSO) 생성**: 셰이더 바이트코드, 정점 레이아웃(Input Layout), 렌더 타겟 포맷(`R8G8B8A8_UNORM`), 래스터라이저 상태 등을 묶어 `ID3D12PipelineState` 생성.

### Phase 2: 버텍스/인덱스 버퍼 생성 및 데이터 업로드
정육면체를 구성하는 정점 데이터를 GPU 메모리에 올립니다.

1.  **정점 데이터 정의**: 8개의 정점(위치, 색상)과 36개의 인덱스(삼각형 12개) 데이터 배열 정의.
2.  **버퍼 생성 (`CreateBuffer`)**: `IRenderDevice`를 통해 Upload 힙에 Vertex Buffer와 Index Buffer 생성.
3.  **데이터 복사 (`Upload`)**: CPU 배열 데이터를 생성된 GPU 버퍼 메모리로 복사.
4.  **뷰 생성 (View)**: 렌더링 파이프라인에 바인딩하기 위해 `D3D12_VERTEX_BUFFER_VIEW` 및 `D3D12_INDEX_BUFFER_VIEW` 구조체 준비.

### Phase 3: 렌더링 루프 연동
생성된 리소스와 파이프라인을 커맨드 리스트에 기록하여 실제로 화면에 그립니다.

1.  **`CubeRenderer::Draw` 구현**:
    *   인자로 받은 `ICommandList`의 네이티브 핸들(`ID3D12GraphicsCommandList`)을 가져옴.
    *   `SetGraphicsRootSignature` 호출.
    *   `SetPipelineState` 호출.
    *   `IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)` 호출.
    *   `IASetVertexBuffers` 및 `IASetIndexBuffer` 호출.
    *   `DrawIndexedInstanced(36, 1, 0, 0, 0)` 명령 기록.
2.  **`Execute.cpp` 연동**:
    *   `OnInitialize`에서 `CubeRenderer` 객체 생성 및 `Initialize()` 호출.
    *   `OnRender`에서 Clear 명령 직후 `CubeRenderer->Draw(commandList.get())` 호출.

---

## 3. 구조 요약 (Sudo-Code)

```cpp
// CubeRenderer.h
class CubeRenderer {
public:
    virtual ~CubeRenderer() = default;
    virtual void Initialize(IRenderDevice* device) = 0;
    virtual void Draw(ICommandList* cmdList) = 0;
};

// CubeRenderer_DX12.h
class CubeRenderer_DX12 : public CubeRenderer {
    // ID3D12PipelineState, ID3D12RootSignature
    // IGPUBuffer(Vertex/Index), Views
};

// Execute.cpp 연동
void Execute::OnRender() {
    // ... Reset, SetRenderTarget, Clear ...
    
    // 큐브 그리기 명령 추가
    cubeRenderer->Draw(commandList.get());

    // ... Close, Execute ...
}
```

이 계획을 바탕으로 구현을 진행합니다.
