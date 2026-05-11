# Vulkan RHI 구현 계획 (Vulkan RHI Implementation Plan)

## 1. 개요
본 문서는 VaEngine의 Vulkan 백엔드(RHI) 구현을 위한 상세 계획을 기술합니다. 
DirectX 12 백엔드와 대칭되는 구조를 유지하면서, Vulkan 특유의 명시적 리소스 관리 및 동기화 요구사항을 충족하는 것을 목표로 합니다.

## 2. 인터페이스 확장 및 추가 (Engine/Public)
"억지로 만들어낼 필요 없이, 없으면 절대 안 되는 것" 위주로 선별하였습니다.

### 2.1 필수 추가 항목
1. **EPrimitiveTopology (Enum)**
   - 이유: 현재 `ICommandList`에는 드로우 시 사용할 토폴로지(TriangleList, PointList 등)를 설정하는 기능이 추상화되어 있지 않습니다.
   - 위치: `Engine/Public/RHI/Common_RHI.h` 혹은 별도 헤더.

2. **ICommandList::SetPrimitiveTopology(EPrimitiveTopology topology)**
   - 이유: DX12는 `IASetPrimitiveTopology`, Vulkan은 파이프라인 생성 시 혹은 동적 상태로 토폴로지를 결정해야 합니다.
   - 위치: `Engine/Public/RHI/ICommandList.h`

3. **ICommandList::ClearDepthStencilView(IResourceView* inViewHandle, float depth, uint8_t stencil)**
   - 이유: 깊이 버퍼 사용 시 필수적입니다. (현재 ClearRenderTargetView만 존재)
   - 위치: `Engine/Public/RHI/ICommandList.h`

4. **ISampler (Interface) & IRenderDevice::CreateSampler(const SamplerDesc& desc)**
   - 이유: Vulkan은 DX12와 달리 셰이더 바인딩 시 Sampler를 명시적으로 생성하고 바인딩해야 하는 경우가 많습니다 (Combined Image Sampler).
   - 위치: `Engine/Public/RHI/Texture/ISampler.h`

### 2.2 내부 은닉 항목 (Public 추가 불필요)
- **Semaphore**: Vulkan의 GPU-GPU 동기화(Swapchain Acquire/Present)는 `ISwapChain` 내부에서 캡슐화하여 관리 가능하므로 외부 노출이 필수적이지 않습니다.
- **Render Pass / Framebuffer**: `ICommandList::SetRenderTarget` 호출 시 내부적으로 관리하거나, 최적화가 필요한 시점에 `BeginRenderPass` 인터페이스를 고려할 수 있으나 현재는 `SetRenderTarget`으로 추상화 가능합니다.

---

## 3. 구현 전략

### 3.1 초기화 (RenderDevice_Vulkan)
- `VkInstance`, `VkPhysicalDevice`, `VkDevice` 생성.
- `VMA (Vulkan Memory Allocator)` 도입 검토 (메모리 관리 복잡도 완화).
- 윈도우/안드로이드 플랫폼별 Surface 생성 처리.

### 3.2 리소스 관리
- **Buffer**: `VkBuffer` 생성 및 `VkDeviceMemory` 할당/바인딩.
- **Texture**: `VkImage`, `VkImageView` 관리. `LoadFromMemory` 시 Staging Buffer 사용.
- **View**: `IResourceView` 구현체에서 `VkImageView` 혹은 `VkDescriptorBufferInfo` 등을 소유.

### 3.3 파이프라인 및 바인딩 (BindingLayout / PipelineState)
- `BindingLayout_Vulkan`: `VkDescriptorSetLayout` 및 `VkPipelineLayout` 생성.
- `PipelineState_Vulkan`: `VkPipeline` (Graphics/Compute) 생성. 셰이더 컴파일 결과(SPIR-V) 연동.
- **Descriptor Set**: 프레임별 혹은 리소스별 Descriptor Set 할당 및 업데이트 (`vkUpdateDescriptorSets`).

### 3.4 명령 실행 및 동기화
- `CommandQueue_Vulkan`: `vkQueueSubmit`, `vkQueuePresentKHR` 처리.
- `CommandList_Vulkan`: `vkCmd...` 시리즈 기록.
- `Fence_Vulkan`: `VkFence`를 이용한 CPU-GPU 동기화.

---

## 4. 단계별 로드맵
1. **1단계**: Vulkan 인스턴스/장치 초기화 및 SwapChain 생성 (화면 Clear 확인).
2. **2단계**: 기초 리소스(Buffer, Texture) 및 Descriptor Set 바인딩 구현.
3. **3단계**: SPIR-V 셰이더 로딩 및 PipelineState 구현.
4. **4단계**: CubeRenderer를 이용한 실제 렌더링 검증.
