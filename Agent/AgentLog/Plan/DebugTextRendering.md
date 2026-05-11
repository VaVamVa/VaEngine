# 구현 계획: Debug Text Rendering (stb_truetype + Glyph Atlas)

## 1. 개요

런타임 화면에 디버그 텍스트(FPS, DrawCall 수 등)를 출력하는 시스템.  
`DebuggingHelper`가 수집한 `DebugTextEntry` 목록을 매 프레임 화면에 렌더링한다.

**기술 선택**: stb_truetype (이미 stb 의존성 포함) + 동적 Glyph Atlas 텍스처  
**크로스플랫폼**: CPU 래스터라이징 + `ITexture` / `ICommandList` 추상화 → DX12/Vulkan/Android 동일 적용  
**폰트**: `_Files/Font/NotoSansKR-Regular.ttf` (한글 포함)

---

## 2. 아키텍처

```
DebuggingHelper::DrawText(text, pos, color)
  → s_textEntries[] 수집 (매 프레임)

DebugTextPass::Execute(cmdList, scene)
  → DebuggingHelper::GetTextEntries() 읽기
  → GlyphAtlas::GetGlyph(char) — 없으면 래스터라이징 후 Atlas 갱신
  → GlyphVertex[] 빌드 (글자당 쿼드 2삼각형 = 6 정점)
  → DebugTextRenderer::Render(cmdList)
  → Glyph.hlsl : Atlas UV 샘플링 + 색상 → backbuffer

DebuggingHelper::Clear()
  → s_textEntries 초기화 (Execute::OnLoop 끝)
```

---

## 3. 데이터 구조

### 3.1 GlyphInfo

```cpp
struct GlyphInfo {
    float u0, v0, u1, v1;   // Atlas 내 UV 범위
    int   width,  height;    // 글리프 픽셀 크기
    int   bearingX, bearingY; // 레이아웃 오프셋
    int   advance;           // 다음 글자까지 이동량
};
```

### 3.2 GlyphVertex (동적 정점 버퍼)

```cpp
struct GlyphVertex {
    float x, y;        // 화면 픽셀 좌표
    float u, v;        // Atlas UV
    float r, g, b, a;  // 색상
};
// 글자당 6정점 (Triangle Strip → 2삼각형)
```

### 3.3 상수 버퍼 (b0)

```cpp
struct CB_DebugText {
    float screenWidth;
    float screenHeight;
    float _pad[2];
};
```

---

## 4. 클래스 설계

### 4.1 GlyphAtlas

**위치**: `Engine/Private/Utilities/GlyphAtlas.h/.cpp`

```
Initialize(device, ttfPath, fontSize)
  → stbtt_InitFont, ASCII 128자 pre-bake
  → 512×512 Atlas 텍스처 생성 (R8 단채널 or RGBA8)

GetGlyph(uint32_t codepoint) → GlyphInfo*
  → 캐시에 없으면: stbtt_GetCodepointBitmap → Atlas에 pack → GPU reupload → 반환

UploadAtlas(device)
  → ITexture::LoadFromMemory로 현재 Atlas 픽셀 업로드
```

**Atlas 전략**:
- 내부 bin-packing: 간단한 shelf-packing (행 단위 좌→우, 넘치면 다음 행)
- Atlas 크기: 512×512 (ASCII + 일반 한글 수백 자 수용)
- 가득 차면 1024×512 등으로 자동 확장 (재업로드)

### 4.2 DebugTextRenderer

**위치**: `Engine/Private/Render/DebugTextRenderer.h/.cpp`

```
Initialize(device, shaderDesc)
  → GlyphAtlas 생성 및 TTF 로드
  → 동적 정점 버퍼 (Upload, 최대 MAX_GLYPH_VERTS)
  → PSO: depthEnable=false, cullMode=None, blendMode=AlphaBlend
  → CB_DebugText 버퍼

Render(cmdList, entries, screenW, screenH)
  → entries 순회 → 글자별 GlyphVertex 6개 생성
  → 정점 버퍼 Upload → DrawInstanced(vertCount, 1)
```

### 4.3 DebugTextPass (RenderGraph 패스)

**위치**: `ForwardRenderer.cpp` 내 anonymous namespace

```
DeclareResources: backBuffer Write (EResourceState::RenderTarget)
Execute:
  → BeginRenderPass (ELoadAction::Load — ForwardPass 결과 보존)
  → renderer->RenderDebugText(cmdList, scene)
  → EndRenderPass
```

`AddPasses()`에서 **ForwardPass 이후** 마지막으로 등록.

---

## 5. 셰이더 — Glyph.hlsl

**위치**: `Engine/_Shaders/DirectX/Glyph.hlsl`

```hlsl
#pragma pack_matrix(row_major)

cbuffer CB_DebugText : register(b0) {
    float screenWidth;
    float screenHeight;
    float2 _pad;
};

Texture2D    gAtlas  : register(t0);
SamplerState gSampler: register(s0);  // Point 또는 Linear

struct VS_INPUT {
    float2 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

struct PS_INPUT {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

PS_INPUT VSMain(VS_INPUT input) {
    float2 ndc;
    ndc.x =  (input.pos.x / screenWidth)  * 2.0f - 1.0f;
    ndc.y = -(input.pos.y / screenHeight) * 2.0f + 1.0f;  // Y 반전 (픽셀 좌상→NDC 좌하)

    PS_INPUT o;
    o.pos   = float4(ndc, 0.0f, 1.0f);
    o.uv    = input.uv;
    o.color = input.color;
    return o;
}

float4 PSMain(PS_INPUT input) : SV_Target {
    float alpha = gAtlas.Sample(gSampler, input.uv).r;  // R8 단채널
    return float4(input.color.rgb, input.color.a * alpha);
}
```

**Alpha Blend 설정** (PSO):
- `srcBlend = SrcAlpha`
- `dstBlend = InvSrcAlpha`

---

## 6. VaEngine 결정사항

### 6.1 VA_DRAW_TEXT 매크로 수정

현재 매크로가 3인수(`text, pos, color`)인데 Execute.cpp 호출부는 2인수. VA_DEBUG=1 시 컴파일 오류 발생.  
→ 가변 인수 매크로로 수정:

```cpp
#define VA_DRAW_TEXT(text, pos, ...) DebuggingHelper::DrawText(text, pos, ##__VA_ARGS__)
```

### 6.2 Atlas 텍스처 포맷

stb_truetype은 8비트 단채널 비트맵 출력 → `DXGI_FORMAT_R8_UNORM`(DX12).  
단, 현재 `Texture_DirectX::Upload`가 RGBA8 고정. R8 포맷 지원을 위해:
- **Option A**: 래스터 결과를 RGBA8로 확장(r=g=b=val, a=255)해서 기존 경로 재사용 → 메모리 4배이지만 구현 단순
- **Option B**: R8 전용 업로드 경로 추가

→ **Option A 권장** (구현 최소화, Atlas 512×512 = 1MB 수준으로 허용 가능)

### 6.3 AlphaBlend PSO

현재 `EBlendMode`에 `AlphaBlend` 없음. `PipelineState_DirectX`에 추가 필요:

```cpp
enum class EBlendMode { Opaque, Additive, AlphaBlend };
```

DX12 블렌드 스테이트:
```
SrcBlend = D3D12_BLEND_SRC_ALPHA
DstBlend = D3D12_BLEND_INV_SRC_ALPHA
BlendOp  = D3D12_BLEND_OP_ADD
```

### 6.4 DebugTextRenderer 초기화 위치

`Execute.cpp` `#ifdef USE_DIRECTX` 블록에서 `renderer.InitializeDebugText(renderDevice.get(), {...})` 호출.  
폰트 경로: `_FILES_DIR "Font/NotoSansKR-Regular.ttf"` — CMakeLists에 매크로 추가 필요:
```cmake
target_compile_definitions(Engine PRIVATE
    _FILES_DIR="${CMAKE_SOURCE_DIR}/_Files/"
)
```

### 6.5 한글 동적 로딩 범위

- 초기화 시: ASCII 0x20~0x7E (95자) pre-bake
- 런타임: 한글 포함 신규 코드포인트 → on-demand 래스터 → Atlas 갱신 후 reupload
- UTF-8 → 코드포인트 변환: `std::string` → `uint32_t` 이터레이터 (C++20 `std::u8string` 또는 수동 파싱)

---

## 7. 구현 순서

| # | 작업 | 파일 | 상태 |
|:---:|---|---|:---:|
| 1 | `VA_DRAW_TEXT` 매크로 가변인수 수정 | `DebuggingHelper.h` | ✅ |
| 2 | `EBlendMode::AlphaBlend` 추가 + PSO 구현 | `PipelineDesc.h`, `PipelineState_DirectX.cpp` | ✅ |
| 3 | `_FILES_DIR` CMake 매크로 추가 | `Engine/CMakeLists.txt` | ✅ |
| 4 | `GlyphAtlas` 구현 (stb_truetype + ASCII pre-bake) | `Private/Utilities/GlyphAtlas.h/.cpp` | ✅ |
| 5 | `Glyph.hlsl` 작성 + CMakeLists 셰이더 추가 | `_Shaders/DirectX/Glyph.hlsl` | ✅ |
| 6 | `DebugTextRenderer` 구현 | `Private/Render/DebugTextRenderer.h/.cpp` | ✅ |
| 7 | `DebugTextPass` + `ForwardRenderer::AddPasses()` 등록 | `ForwardRenderer.cpp` | ✅ |
| 8 | `Execute.cpp` `InitializeDebugText` 호출 | `Execute.cpp` | ✅ |
| 9 | 한글 동적 로딩 (UTF-8 파싱 + on-demand 래스터) | `GlyphAtlas.cpp` | ✅ |
| 10 | `VA_DRAW_PANEL` key 기반 좌상단 패널 시스템 | `DebuggingHelper.h/.cpp`, `DebugTextRenderer` | ✅ |

---

## 8. VA_DRAW_PANEL — key 기반 좌상단 패널 (추가 구현)

`VA_DRAW_TEXT` 자유 위치 방식과 병행하는 고정 레이아웃 패널.

```cpp
VA_DRAW_PANEL(0, std::format("FPS: {:.1f}", fps));   // key 0 → 1번째 줄
VA_DRAW_PANEL(1, std::format("Draw Calls: {}", n));  // key 1 → 2번째 줄
```

- 저장: `std::map<uint32_t, DebugPanelEntry>` — key 오름차순 자동 정렬, 같은 key는 덮어씀
- 레이아웃: `PANEL_LEFT=10`, `PANEL_TOP=15`, `PANEL_PADDING=4`, y = TOP + i×(lineHeight + PADDING)
- 렌더 순서: 패널 먼저 → 자유 위치 나중 (겹칠 때 자유 위치가 위에 표시)

---

## 9. 미지원 / 후속 과제

- **텍스트 줄바꿈**: 현재 단일 행 기준. `\n` 파싱은 GlyphAtlas 외부에서 처리.
- **파일 출력 로그**: `DebuggingHelper::Log()` → `std::ofstream` 연결 (별도 Step)
- **Vulkan 셰이더**: `Glyph.hlsl` → SPIR-V 컴파일 (VaShaderCompiler Vulkan 경로 확보 후)
- **Android 폰트 경로**: `AAssetManager` 기반 TTF 버퍼 로딩 (ExecAndroid 담당)
