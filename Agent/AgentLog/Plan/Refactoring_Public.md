# Public Interface 리팩터링 계획 (Refactoring_Public.md)

본 문서는 **Desktop**, **Android**, **Xbox(GDK)**, **PlayStation(AGC)** 등 멀티 플랫폼 대응을 목표로 하는 하이엔드 RHI 추상화 계획입니다.

---

## 1. 목표
- **다형성 극대화**: 특정 API의 고유 개념이 Public 계층으로 유출되는 것을 차단.
- **콘솔 최적화**: Xbox/PS의 명시적 메모리 및 동기화 모델을 수용할 수 있는 인터페이스 설계.
- **성능**: 타일 기반 아키텍처(Android)와 직렬화된 명령 기록(Consoles)을 위한 최적화 구조 도입.

---

## 2. 공통 자료구조 및 셰이더 추상화 (`Common_RHI.h`, `IShader.h`)

### 2-1. `IShader` 인터페이스 도입
경로 기반 로딩에서 바이너리 기반 추상화로 전환합니다.
```cpp
class IShader {
public:
    virtual ~IShader() = default;
};

struct ShaderLoadDesc {
    const void* data;      // DXIL(Xbox), SPIR-V(Android), AGC Binary(PS)
    size_t      dataSize;
    EShaderStage stage;
};
```

### 2-2. `EPixelFormat` 독립화 및 토폴로지 정의
API 수치와 무관한 독립 열거형을 사용합니다.

---

## 3. 명령 리스트 및 렌더 패스 (`ICommandList.h`)

### 3-1. `RenderPass` 개념 도입 (Mobile/Console 최적화)
단순 `SetRenderTarget` 대신 Load/Store Action을 명시하는 패스 구조를 사용합니다.
```cpp
struct RenderPassAttachment {
    IResourceView* view;
    ELoadAction    loadAction;  // Clear, Load, DontCare
    EStoreAction   storeAction; // Store, DontCare
    float          clearColor[4];
};

struct RenderPassDesc {
    uint32_t             renderTargetCount;
    RenderPassAttachment renderTargets[8];
    RenderPassAttachment depthStencil;
};

// ICommandList 메서드
virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;
virtual void EndRenderPass() = 0;
```

---

## 4. 리소스 인터페이스 통합 (`IBuffer.h`)

### 4-1. 버퍼 통합
`IConstantBuffer`, `IResourceBuffer`를 하나로 통합하여 콘솔의 유연한 메모리 접근을 수용합니다.
```cpp
class IBuffer : public IRHIResource {
public:
    virtual void* Map() = 0;
    virtual void  Unmap() = 0;
};
```

---

## 5. 팩토리 및 장치 관리 (`IRenderDevice.h`)

### 5-1. 크로스 플랫폼 팩토리 메서드
```cpp
virtual std::unique_ptr<IShader>        CreateShader(const ShaderLoadDesc& desc) = 0;
virtual std::unique_ptr<IGPUBuffer>     CreateBuffer(const GPUBufferDesc& desc) = 0;
virtual std::unique_ptr<ISampler>       CreateSampler(const SamplerDesc& desc) = 0;
virtual std::unique_ptr<IPipelineState> CreatePipelineState(const PipelineStateDesc& desc) = 0;
```

---

## 6. 스왑체인 및 동기화 (`ISwapChain.h`, `IFence.h`)

### 6-1. 라이프사이클 추상화
- `Resize(w, h)`: 콘솔의 해상도 변경 및 안드로이드 회전 대응.
- `WaitForValue(v)`: 플랫폼별 동기화 객체(Win32 Handle, Unix Futex, Console Semaphore) 은닉.

---

## 7. 플랫폼별 특이사항 고려 (Summary)
- **Xbox**: DX12와 유사하나 전용 메모리 할당 정책 필요 (Buffer 생성 시 플래그로 대응).
- **PlayStation**: 매우 낮은 수준의 메모리 관리 요구. `IGPUBuffer`의 `Map/Unmap` 및 `Alignment` 고려 필수.
- **Android**: `BeginRenderPass`에서의 `DontCare` 액션이 대역폭 절감의 핵심.

---

## 8. 실행 우선순위
1. **Shader & Buffer 통합**: 가장 기초가 되는 리소스 모델 정립.
2. **Render Pass 도입**: 그리기 명령의 큰 틀 확정.
3. **Common Enums 수정**: API 독립성 확보.
4. **Platform Logic 분리**: `IFence` 등 OS 종속성 제거.
