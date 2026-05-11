# 렌더링 파이프라인 기술 명세서: Meshless Sky Rendering

## 1. 개요 (Overview)
본 문서는 정점 버퍼(Vertex Buffer)와 인덱스 버퍼(Index Buffer)를 배제하고, 단일 드로우 콜(Draw Call)과 셰이더 연산만으로 하늘(Sky) 및 배경 환경을 렌더링하는 파이프라인의 명세를 정의합니다. 
Vulkan 및 DirectX 12(DX12) 환경에서 동일한 논리적 구조를 가지며, 2:1 비율의 LatLong(Equirectangular) HDRi 텍스처를 환경 맵으로 사용합니다.

---

## 2. 파이프라인 상태 설정 (Pipeline State Object, PSO)

추상화된 그래픽스 파이프라인 생성 시 다음 상태를 보장해야 합니다.

* **Input Assembly**: 
    * Topology: `Triangle List`
    * Vertex Input Layout: **없음 (NULL)**
* **Rasterizer State**:
    * Cull Mode: `None` (화면을 덮는 NDC 삼각형이므로 컬링 비활성화)
* **Depth Stencil State**:
    * Depth Write: `False` (하늘은 다른 오브젝트를 가리지 않음)
    * Depth Test: `True`
    * Depth Compare Op: `LessEqual` (일반 Z-Buffer 기준) 또는 `GreaterEqual` (Reversed-Z 기준)
* **Resource Binding (Root Signature / Descriptor Set)**:
    * 1x Texture2D (HDRi LatLong Map)
    * 1x Sampler (Linear Filter, Wrap/Repeat Address Mode)
    * Constant Buffer (Inverse View Matrix, Inverse Projection Matrix)

---

## 3. 호스트(C++) 호출 명세

API 호출 시 버퍼 바인딩을 생략하고 3개의 정점을 직접 그립니다.

* **DirectX 12**: `pCommandList->DrawInstanced(3, 1, 0, 0);`
* **Vulkan**: `vkCmdDraw(commandBuffer, 3, 1, 0, 0);`

---

## 4. 셰이더 수학 모델 (Mathematical Model)

### 4.1 정점 셰이더 (Vertex Shader)
`VertexID` (0, 1, 2)를 기반으로 클립 공간(NDC)의 좌표를 생성합니다. $Z$값은 렌더링 파이프라인의 깊이 설정에 따라 투영 평면의 끝에 맞춥니다.

$$X_{ndc} = (VertexID \ll 1) \ \& \ 2$$
$$Y_{ndc} = VertexID \ \& \ 2$$
$$Pos_{clip} = (X_{ndc} \times 2.0 - 1.0, Y_{ndc} \times 2.0 - 1.0, Z_{far}, 1.0)$$

*(참고: $Z_{far}$은 일반적인 투영에서 1.0, Reversed-Z에서는 0.0을 사용합니다.)*

### 4.2 픽셀 셰이더 (Pixel / Fragment Shader)
클립 공간 좌표를 월드 공간의 방향 벡터(View Ray)로 변환한 뒤, 구면 좌표계(Spherical Coordinates)를 이용해 2D UV 좌표로 매핑합니다.

#### A. View Ray 도출 ($V_{world}$)
카메라의 이동을 배제하기 위해, View 행렬의 $3 \times 3$ 회전 성분만 사용한 역행렬($V_{rot}^{-1}$)과 투영 역행렬($P^{-1}$)을 사용합니다.

$$V_{view} = P^{-1} \cdot (Pos_{clip.x}, Pos_{clip.y}, 1.0, 1.0)$$
$$V_{world} = \text{Normalize}(V_{rot}^{-1} \cdot V_{view})$$

#### B. LatLong UV 매핑
정규화된 $V_{world} = (x, y, z)$ 벡터를 기준으로 방위각($\theta$)과 고도각($\phi$)을 구합니다.

$$\theta = \arctan2(z, x)$$
$$\phi = \arcsin(y)$$

이를 [0, 1] 범위의 텍스처 $UV$ 좌표로 정규화합니다.

$$U = \frac{\theta}{2\pi} + 0.5$$
$$V = \frac{\phi}{\pi} + 0.5$$

---

## 5. API별 추상화 시 주의사항 (Vulkan vs DX12)

| 항목 | Vulkan | DirectX 12 | 보정 방법 |
| :--- | :--- | :--- | :--- |
| **NDC Y축 방향** | 위에서 아래로 (+Y) | 아래에서 위로 (+Y) | 투영 행렬 구성 시 API에 맞게 생성 로직을 분리 |
| **Texture UV 원점** | 좌측 상단 | 좌측 상단 | 동일 |
| **Depth 범위** | [0, 1] | [0, 1] | 동일 (OpenGL의 [-1, 1]과 다름) |

---

## 6. 성능 및 메모리 이점
1.  **Zero-Buffer Overhead**: GPU 메모리에 별도의 Vertex/Index Buffer를 할당할 필요가 없어 메모리 오버헤드가 제거됩니다.
2.  **Fill-rate 최적화**: 삼각형 1개로 화면을 덮으므로 사각형(Quad) 사용 시 발생하는 픽셀 셰이딩 중복(Overdraw)이 없습니다.
3.  **Asset 렌더링 통합**: 6장의 이미지(Cubemap)가 아닌 1장의 `.hdr` 파일을 사용하여 리소스 관리 로직이 단순해집니다.

---

## 7. VaEngine 적용 결정사항

### 7.1 `DrawInstanced` — ICommandList 추가
- `ITextureFloat` 같은 새 인터페이스 신설 없이, 기존 `ICommandList`에 메서드 추가
- VB/IB 없이 VertexID 기반 드로우를 지원하기 위해 반드시 필요

### 7.2 float 텍스처 — `TextureFloat_DirectX : ITexture`
- `ITextureFloat` 인터페이스 **신설하지 않음**
- 기존 `ITexture` 인터페이스를 그대로 구현하는 `TextureFloat_DirectX` 클래스 신설
- 팩토리: `IRenderDevice::CreateTextureFloat()` → `unique_ptr<ITexture>` 반환
- 내부 GPU 포맷: `DXGI_FORMAT_R32G32B32A32_FLOAT` (FP32 — stbi_loadf 변환 없이 직접 업로드)
- `LoadFromFile`: `stbi_loadf()` 사용, RGB → RGBA float4 패딩
- `Bind()`: `Texture_DirectX::Bind()`와 동일 (SRV 슬롯 바인딩)
- **주의**: `STB_IMAGE_IMPLEMENTATION`은 `Texture_DirectX.cpp`에 이미 정의됨. `TextureFloat_DirectX.cpp`에서 중복 정의 금지

### 7.3 렌더링 순서 — SkyPass 상시 등록
- `AddPasses()`가 `RenderScene`보다 먼저 호출되므로, 패스 구성 시점에 Sky 유무를 알 수 없음
- 해결: **SkyPass를 항상 등록** (ForwardPass보다 먼저)
  - SkyPass: 항상 `ELoadAction::Clear`로 RT 클리어 담당. `scene.GetSkybox() != nullptr`이면 DrawInstanced(3) 추가 수행
  - ForwardPass: `ELoadAction::Clear` → **`ELoadAction::Load`로 고정 변경**
- Sky가 없을 때 SkyPass 비용: `BeginRenderPass` + `EndRenderPass` 1회 (무시 가능)

### 7.4 WorldObject — 사용하지 않음
- 스카이는 위치·회전·크기 개념이 없는 씬 전역 환경 설정
- `WorldObject` 계층 밖에서 Application이 `ITexture*`를 직접 소유
- `SubmitRenderState()`에서 `scene.SetSkybox(skyTexture.get())` 한 줄로 씬에 전달

### 7.5 역행렬 계산
- HLSL에 `inverse()` 내장 함수 없음 → CPU에서 계산 후 CB로 전달
- **InvViewRot**: View 행렬에서 translation 행(`m[3][0..2]`)을 0으로 제거 후 `Transposed()` (직교 행렬이므로 역행렬 = 전치)
- **InvProj**: `Matrix4x4::Inverse()` 사용 (범용 4×4 역행렬 — Container.h에 신설)
- CB 크기: `InvProj(64) + InvViewRot(64)` = 128 bytes

---

## 8. VaEngine 구현 프로세스

### Step 1: `Matrix4x4::Inverse()` 추가
**파일**: `Engine/Public/Math/Container.h` (선언) + Math 구현 파일 (정의)

- 범용 4×4 역행렬 (Gauss-Jordan 소거법 또는 Cramer 공식)
- View 회전 역행렬은 `Transposed()`로 해결되므로, 실질적 용도는 Proj 역행렬

---

### Step 2: `ICommandList::DrawInstanced()` 추가
**파일**:
- `Engine/Public/RHI/ICommandList.h` — 순수 가상 메서드 선언
- `Engine/Private/RHI/DirectX/CommandList_DirectX.h/.cpp` — DX12 구현

DX12 구현 내용: `GetHandle()->DrawInstanced(vertexCount, instanceCount, 0, 0)`

---

### Step 3: `TextureFloat_DirectX` 구현
**파일**:
- `Engine/Private/RHI/DirectX/Texture/TextureFloat_DirectX.h`
- `Engine/Private/RHI/DirectX/Texture/TextureFloat_DirectX.cpp`

구현 포인트:
- `LoadFromFile`: `stbi_loadf()` → RGB3채널 → float4 배열로 패딩(A=1.0f) → `Upload()`
- `Upload()` 변경 부분 (`Texture_DirectX::Upload` 대비):
  - 리소스 포맷: `DXGI_FORMAT_R32G32B32A32_FLOAT`
  - 픽셀 복사 stride: `width * sizeof(float) * 4` (16 bytes/pixel)
  - SRV 포맷: `DXGI_FORMAT_R32G32B32A32_FLOAT`
- `Bind()`: `Texture_DirectX::Bind()`와 동일
- `STB_IMAGE_IMPLEMENTATION` 정의 없이 `#include <stb_image.h>` 만 사용

---

### Step 4: `IRenderDevice::CreateTextureFloat()` 팩토리 추가
**파일**:
- `Engine/Public/RHI/IRenderDevice.h` — 순수 가상 메서드 추가
- `Engine/Private/RHI/DirectX/RenderDevice_DirectX.h/.cpp` — 구현 추가

반환 타입: `std::unique_ptr<ITexture>`

---

### Step 5: `RenderScene::SetSkybox()` 추가
**파일**: `Engine/Public/Scene/RenderScene.h`

추가 내용:
- `ITexture* skyTexture = nullptr;` 멤버 (비소유 포인터)
- `void SetSkybox(ITexture* tex)` — Application에서 호출
- `ITexture* GetSkybox() const`
- `Clear()`에 `skyTexture = nullptr;` 추가

---

### Step 6: `Sky.hlsl` 작성
**파일**: `Engine/_Shaders/DirectX/Sky.hlsl`

바인딩 레이아웃:
```
b0 (Pixel): CB_SkyData { float4x4 gInvProj; float4x4 gInvViewRot; }
t0 (Pixel): Texture2D gHDRMap
s0 (Pixel): SamplerState LinearSampler  ← Sampler.hlsli 공유
```

VS 역할:
- `SV_VertexID` (0, 1, 2) → NDC XY 생성
- `output.pos = float4(ndcX, ndcY, 1.0f, 1.0f)` (Z=1.0 far plane)
- `output.clipPos = output.pos` (PS에 전달)

PS 역할:
1. `clipPos`를 `gInvProj`로 변환 → view-space ray
2. view-space ray를 `gInvViewRot`으로 변환 → world-space direction (정규화)
3. `theta = atan2(dir.z, dir.x)`, `phi = asin(dir.y)`
4. `UV = float2(theta / (2*PI) + 0.5, phi / PI + 0.5)`
5. `gHDRMap.Sample(LinearSampler, UV)` 반환

---

### Step 7: `ForwardRenderer` SkyPass 추가 및 수정
**파일**:
- `Engine/Private/Render/ForwardRenderer.h` — 스카이 리소스 멤버 추가
- `Engine/Private/Render/ForwardRenderer.cpp` — SkyPass 구조체 + 로직 추가

`ForwardRenderer` 신규 멤버:
```cpp
// Sky 전용 GPU 리소스
std::unique_ptr<IBindingLayout> skyBindingLayout;
std::unique_ptr<IShader>        skyShader;
std::unique_ptr<IPipelineState> skyPipelineState;
std::unique_ptr<IBuffer>        skyDataBuffer;   // b0: CB_SkyData (128 bytes)
```

`Initialize()` 추가 작업:
- Sky 바인딩 레이아웃: `CB(b0, Pixel)` + `Texture(t0, Pixel)`
- Sky PSO: `cullMode=None`, `depthEnable=false`, `vertexInputs=nullptr`, `vertexInputCount=0`
- `skyDataBuffer`: size=128, ConstantBuffer, Upload

`SkyPass` 구조체 (`ForwardPass` 앞에 정의):
- `DeclareResources`: backBuffer만 Write 선언 (depth buffer 없음)
- `Execute`: `ELoadAction::Clear`로 BeginRenderPass → `renderer->RenderSky(cmdList, scene)` → EndRenderPass

`ForwardPass` 수정:
- `passDesc.renderTargets[0].loadAction = ELoadAction::Load` (Clear → Load 변경)

`ForwardRenderer::RenderSky()` 신규 메서드:
```
1. scene.GetSkybox() == nullptr → 얼리 리턴
2. CB_SkyData 계산:
   - invProj = cam.proj.Inverse()
   - viewRot = cam.view (m[3][0..2] = 0 으로 translation 제거)
   - invViewRot = viewRot.Transposed()
3. skyDataBuffer->Upload(&skyData)
4. skyPipelineState->Bind(cmdList)
5. SetConstantBuffer(skyDataBuffer, 0)
6. scene.GetSkybox()->Bind(cmdList, 1)
7. SetPrimitiveTopology(TriangleList)
8. DrawInstanced(3, 1)
```

`AddPasses()` 수정:
```cpp
graph.AddPass<SkyPass>(this, output);        // 항상 먼저
graph.AddPass<ForwardPass>(this, output, depthHandle);
```

---

### Step 8: CMakeLists 셰이더 컴파일 추가
**파일**: `Engine/CMakeLists.txt`

`add_custom_command` 블록 추가 (AnimationDemo 블록 이후):
- Input: `Sky.hlsl`
- Output: `Sky_VS.cso`, `Sky_PS.cso`
- `CompileShaders` 타겟 DEPENDS에 두 .cso 추가

---

### Step 9: Application 스카이 설정
**파일**: `Application/Source/`의 `VaProgramName` 구현체

추가 멤버:
```cpp
std::unique_ptr<ITexture> skyTexture;
```

`Initialize()`:
```cpp
skyTexture = device->CreateTextureFloat();
skyTexture->LoadFromFile(device, L"_Assets/sky.hdr");
```

`SubmitRenderState()`:
```cpp
scene.SetSkybox(skyTexture.get());
```

---

## 9. 구현 순서 요약

| 순서 | 작업 | 파일 | 상태 |
|:---:|---|---|:---:|
| 1 | `Matrix4x4::Inverse()` 추가 | `Container.h` + `Container.cpp` | ✅ |
| 2 | `DrawInstanced()` 추가 | `ICommandList.h` + `CommandList_DirectX` | ✅ |
| 3 | `TextureFloat_DirectX` 구현 | `Texture/TextureFloat_DirectX.h/.cpp` | ✅ |
| 4 | `CreateTextureFloat()` 팩토리 | `IRenderDevice.h` + `RenderDevice_DirectX` | ✅ |
| 5 | `RenderScene::SetSkybox()` | `RenderScene.h` | ✅ |
| 6 | `Sky.hlsl` 작성 | `_Shaders/DirectX/Sky.hlsl` | ✅ |
| 7 | `ForwardRenderer` SkyPass 추가 | `ForwardRenderer.h/.cpp` + `Execute.cpp` | ✅ |
| 8 | CMakeLists 셰이더 추가 | `Engine/CMakeLists.txt` | ✅ |
| 9 | Application 스카이 설정 | `VaProgramName.h/.cpp` | ✅ |

## 10. 구현 비고

- VaProgramName에는 테스트용 4×2 float4 그라디언트 텍스처(파란 하늘 / 주황 지평선)가 적용됨 → 런타임 정상 확인
- 실제 HDRi 파일 사용 시 `LoadFromMemory` 블록을 아래로 교체:
  ```cpp
  skyTexture = device->CreateTextureFloat().release();
  skyTexture->LoadFromFile(device, ASSETS_DIR "HDR/filename.hdr");
  ```
  파일 위치: `VaEngine/_Assets/HDR/filename.hdr` (.hdr 포맷, Poly Haven 등에서 다운로드)
- `STB_IMAGE_IMPLEMENTATION`은 `Texture_DirectX.cpp`에만 정의 — `TextureFloat_DirectX.cpp`는 define 없이 include만
- `ITexture::LoadFromFile` 시그니처는 `const char*` (narrow) — `ASSETS_DIR` 매크로와 문자열 리터럴 직접 연결 가능
- `ASSETS_DIR`은 후행 슬래시 포함: `"${CMAKE_SOURCE_DIR}/_Assets/"` → 경로 조합 시 폴더명 앞 슬래시 불필요
