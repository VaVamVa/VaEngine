# Asset Importer 구현 계획

작성일: 2026-05-09  
최종 수정: 2026-05-09

---

## 목표

FBX 파일을 엔진 전용 바이너리 포맷(`.mesh`)으로 변환하는 오프라인 도구(VaImportTool)를 구축하고,
Engine이 런타임에 `.mesh` 파일을 읽어 `MeshPrimitive`를 생성할 수 있게 한다.

**범위 (Phase 1 — 구현 완료)**
- 정적 메시 (스켈레톤 / 애니메이션 없음)
- FBX → `.mesh` 바이너리 (정점 + 인덱스, 서브메시 단위)
- ~~재질 이름만 `.mesh`에 포함 (실제 `.matl` 파일은 Phase 2)~~ → **Phase 1에서 구현 완료**
- `.matl` 텍스트 파일 (색상 + 텍스처 경로) + 텍스처 파일 복사/저장
- embedded 텍스처 (압축 raw / RGBA→PNG) + 외부 텍스처 파일 복사 지원
- 런타임 텍스처 렌더링 파이프라인 (`RenderScene` + `ForwardRenderer` per-group 바인딩)
- `WO_Tower` WorldObject 구현

**범위 밖 (Phase 2 이후)**
- 스켈레톤 / 애니메이션 클립
- LOD, 충돌 메시
- 서브메시별 개별 텍스처 매핑 (현재 첫 번째 `diffuse_tex` 전체 적용)
- `EIndexFormat::UInt32` 대형 메시 지원

---

## 아키텍처

```
[FBX 파일]  ──►  VaImportTool  ──►  [.mesh 바이너리]
                  (asset-importer         ├── [.matl 텍스트]
                   CMake 프리셋)          └── [텍스처 파일들]
                                                   │
                                        수동 복사 (_Assets/)
                                                   │
                                    ┌──────────────┼──────────────┐
                                    ▼              ▼              ▼
                             MeshLoader      matl 파서      ITexture::LoadFromFile
                                    │              │
                                    └──────┬───────┘
                                           ▼
                                  MeshPrimitive + ITexture
                                  (WO_Tower::Initialize)
```

**핵심 원칙**
- Engine / Application / ExecWindows 는 Assimp를 **전혀 모름**
- ImportTool 이 Engine 헤더(`MeshAsset.h`)를 include 경로로만 참조 — 링크 없음
- `asset-importer` 프리셋 활성 시 ImportTool **만** 빌드됨

---

## 파일 레이아웃

```
VaEngine/
├── Tools/
│   └── ImportTool/                   (신규)
│       ├── CMakeLists.txt
│       ├── Main.cpp                  콘솔 interactive 진입점 (std::cin)
│       ├── Converter.h / .cpp        aiScene → 중간 구조체
│       ├── Exporter.h  / .cpp        중간 구조체 → .mesh/.matl/텍스처 출력
│       └── Assets/
│           ├── Input/{name}/{name}.fbx   입력 경로 (고정)
│           └── Output/{name}/            출력 경로 (고정)
│
├── Engine/
│   ├── Public/Asset/                 (신규)
│   │   ├── MeshAsset.h               .mesh 포맷 정의 (ImportTool과 공유)
│   │   └── MeshLoader.h              .mesh → MeshData (런타임, Assimp 무관)
│   └── Private/Asset/               (신규)
│       └── MeshLoader.cpp
│
├── Application/
│   └── Source/
│       ├── Public/WorldObjects/
│       │   └── WO_Tower.h            (신규)
│       └── Private/WorldObjects/
│           └── WO_Tower.cpp          (신규)
│
└── _Assets/                         (신규 — 런타임 에셋, gitignore 권장)
    └── {name}/
        ├── {name}.mesh
        ├── {name}.matl
        └── *.png / *.jpg  (텍스처)
```

---

## .mesh 바이너리 포맷

```
── Header ──────────────────────────────────────────────────────
  [u32] magic      = 0x4853454D  ('M','E','S','H' little-endian)
  [u32] version    = 1
  [u32] meshCount  서브메시 수
  [u32] reserved   = 0

── SubMesh × meshCount ─────────────────────────────────────────
  [u32]  nameLen
  [u8 ]  name[nameLen]                 (null 미포함)
  [u32]  materialNameLen
  [u8 ]  materialName[materialNameLen]
  [u32]  vertexCount
  [u32]  vertexStride                  = 48  (PrimitiveVertex)
  [u8 ]  vertices[vertexCount × 48]
  [u32]  indexCount                    uint16 개수
  [u16]  indices[indexCount]
  [u8 ]  _pad (indexCount 홀수 시 2바이트 패딩 — 4-byte 정렬)
```

**정점 레이아웃 매핑 (FBX → PrimitiveVertex)**

| PrimitiveVertex 필드 | Assimp 소스                                            |
|----------------------|--------------------------------------------------------|
| `pos[3]`             | `aiMesh->mVertices[i]`                                 |
| `normal[3]`          | `aiMesh->mNormals[i]` (`aiProcess_GenNormals` 보장)    |
| `color[4]`           | `aiMesh->mColors[0][i]`, 없으면 `{1,1,1,1}`            |
| `uv[2]`              | `aiMesh->mTextureCoords[0][i]`, 없으면 `{0,0}`         |

---

## .matl 텍스트 포맷 (Phase 1에서 추가)

```ini
[MaterialName]
ambient=r,g,b,a
diffuse=r,g,b,a
specular=r,g,b
shininess=s
emissive=r,g,b,a
diffuse_tex=filename.png
normal_tex=filename.png
specular_tex=filename.png
```

---

## 구현 순서 체크리스트

### Step 1 — CMake 구성
- [x] `CMakePresets.json` — `base`에 `USE_IMPORT_TOOL: "OFF"`, `asset-importer` 프리셋 추가
- [x] 루트 `CMakeLists.txt` — `if(USE_IMPORT_TOOL)` 분기
- [x] `Tools/ImportTool/CMakeLists.txt` 생성 (Assimp + stb FetchContent, `BUILD_SHARED_LIBS OFF`)

### Step 2 — 공유 포맷 헤더
- [x] `Engine/Public/Asset/MeshAsset.h` — 매직·헤더 상수 정의

### Step 3 — ImportTool 구현
- [x] `Converter.h / .cpp` — `aiScene` → `vector<SubMeshIntermediate>` + `vector<MaterialIntermediate>` 변환
- [x] `Exporter.h / .cpp` — `.mesh` 직렬화 + `.matl` 텍스트 + 텍스처 파일 출력
- [x] `Main.cpp` — 콘솔 interactive (고정 경로 + asset name 입력, `std::istream` 방식)

### Step 4 — Engine MeshLoader
- [x] `Engine/Public/Asset/MeshLoader.h` — `Load(path) → vector<MeshData>` 선언
- [x] `Engine/Private/Asset/MeshLoader.cpp` — `.mesh` 역직렬화 구현
- [x] `Engine/CMakeLists.txt` — Asset 소스 파일 추가

### Step 5 — 텍스처 렌더링 파이프라인
- [x] `Engine/Public/Scene/RenderScene.h` — `RenderCommand`에 `ITexture* texture`, `AddMesh` 오버로드 추가
- [x] `Engine/Private/Render/ForwardRenderer.cpp` — `DrawGroup {mesh, tex, count}` per-group 텍스처 바인딩
- [x] `Application/CMakeLists.txt` — `ASSETS_DIR` 컴파일 매크로 추가

### Step 6 — WO_Tower 구현
- [x] `Application/Source/Public/WorldObjects/WO_Tower.h`
- [x] `Application/Source/Private/WorldObjects/WO_Tower.cpp` — MeshLoader + matl 파서 + ITexture 로드
- [x] `VaProgramName` — tower 초기화·제출·해제
- [x] 화면 출력 확인 ✅

### Step 7 — 통합 검증
- [x] 샘플 FBX(Tower) → `VaImportTool` 실행 → `.mesh` / `.matl` / 텍스처 생성 확인
- [x] `MeshLoader::Load()` → `MeshPrimitive::Initialize()` → 화면 출력 확인

---

## 각 단계 상세

### Step 1 — CMake 구성

**CMakePresets.json 변경점**

`base` cacheVariables에 추가:
```json
"USE_IMPORT_TOOL": "OFF"
```

신규 configurePreset:
```json
{
  "name": "asset-importer",
  "displayName": "Asset Importer",
  "inherits": "base",
  "generator": "Visual Studio 18 2026",
  "architecture": { "value": "x64", "strategy": "set" },
  "binaryDir": "${sourceDir}/Build/asset-importer",
  "cacheVariables": { "USE_IMPORT_TOOL": "ON" }
}
```

신규 buildPreset:
```json
{ "name": "Asset Importer", "configurePreset": "asset-importer" }
```

**루트 CMakeLists.txt 변경점**

```cmake
if(USE_IMPORT_TOOL)
    add_subdirectory(Tools/ImportTool)
else()
    if(USE_DIRECTX OR USE_VULKAN)
        add_subdirectory(Tools/ShaderCompiler)
    endif()
    add_subdirectory(Engine)
    add_subdirectory(Application)
    add_subdirectory(Platforms/ExecWindows)
    add_subdirectory(Platforms/ExecAndroid)
endif()
```

**Tools/ImportTool/CMakeLists.txt** (실제 구현)

```cmake
cmake_minimum_required(VERSION 3.20)

include(FetchContent)
FetchContent_Declare(assimp GIT_REPOSITORY ... GIT_TAG v5.3.1)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)  # ← DLL 없이 정적 빌드
set(ASSIMP_BUILD_TESTS OFF ...) ...
FetchContent_MakeAvailable(assimp)

FetchContent_Declare(stb GIT_REPOSITORY ... GIT_TAG master GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(stb)

add_executable(VaImportTool Main.cpp Converter.cpp Exporter.cpp)
target_include_directories(VaImportTool PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine/Public
    ${stb_SOURCE_DIR}
)
target_link_libraries(VaImportTool PRIVATE assimp)

# 고정 에셋 경로 매크로
set(_asset_input  "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Input")
set(_asset_output "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Output")
target_compile_definitions(VaImportTool PRIVATE
    ASSET_INPUT_DIR="${_asset_input}"
    ASSET_OUTPUT_DIR="${_asset_output}"
)
set_target_properties(VaImportTool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Tools"
)
```

> **계획 대비 변경**: `BUILD_SHARED_LIBS OFF` 추가 (런타임 DLL 의존성 제거),
> stb FetchContent 추가 (embedded RGBA 텍스처 PNG 저장용)

---

### Step 2 — 공유 포맷 헤더

**Engine/Public/Asset/MeshAsset.h**

```cpp
#pragma once
#include <cstdint>

constexpr uint32_t MESH_MAGIC   = 0x4853454D;
constexpr uint32_t MESH_VERSION = 1;

struct MeshFileHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t meshCount;
    uint32_t reserved = 0;
};
```

---

### Step 3 — ImportTool 구현

**중간 구조체 (Converter.h)**

```cpp
struct SubMeshIntermediate {
    std::string           name;
    std::string           materialName;
    std::vector<uint8_t>  vertices;
    std::vector<uint16_t> indices;
};

struct TextureSlot {
    std::string          filename;
    std::vector<uint8_t> bytes;
    uint32_t width = 0, height = 0;
    bool IsEmpty()    const;
    bool IsEmbedded() const;  // bytes가 있으면 embedded
    bool IsRGBA()     const;  // width/height > 0 이면 비압축 RGBA
};

struct MaterialIntermediate {
    std::string name;
    float ambient[4], diffuse[4], specular[4], emissive[4];
    // specular[3] = shininess
    TextureSlot diffuseTex, normalTex, specularTex;
};
```

**Main.cpp — 콘솔 interactive 방식** (계획 대비 변경)

```
계획:  VaImportTool.exe <input.fbx> <output_dir>  (CLI 인수)
실제:  콘솔에서 asset 이름 반복 입력, 고정 경로 자동 조합

입력  : ASSET_INPUT_DIR/{name}/{name}.fbx
출력  : ASSET_OUTPUT_DIR/{name}/
정규화: std::filesystem::path(rawInput).stem()
         → "Tower", "Tower.fbx", "Tower/Tower.fbx" 모두 "Tower"로 처리
```

---

### Step 4 — Engine MeshLoader

계획과 동일하게 구현.
`materialName`은 파일에서 읽지만 현재 버림 (Phase 2 — 서브메시별 텍스처 매핑 시 활용 예정).

---

### Step 5 — 텍스처 렌더링 파이프라인 (Phase 1에서 추가)

**RenderScene.h 변경**

```cpp
struct RenderCommand {
    ...
    ITexture* texture = nullptr;  // nullptr → ForwardRenderer fallback texture
};

// 기존 오버로드 유지
void AddMesh(IMesh* mesh, const Matrix4x4& worldMatrix, SceneObjectID id = 0);
// 텍스처 오버로드 추가
void AddMesh(IMesh* mesh, const Matrix4x4& worldMatrix, ITexture* texture, SceneObjectID id = 0);
```

**ForwardRenderer.cpp 변경**

```
이전: 루프 전 texture->Bind(cmdList, 2) 1회 전역 바인딩
이후: DrawGroup { mesh, tex, count } 구조체로 (mesh, texture) 쌍 그룹화
      → tex != boundTex 일 때만 rebind
      → cmd.texture ? cmd.texture : texture.get()  (fallback = 체커보드)
```

**Application/CMakeLists.txt 변경**

```cmake
target_compile_definitions(Application PRIVATE
    ASSETS_DIR="${CMAKE_SOURCE_DIR}/_Assets"
)
```

---

### Step 6 — WO_Tower

```
WO_Tower::Initialize(device)
├── MeshLoader::Load(ASSETS_DIR/Tower/Tower.mesh)
│   └── MeshPrimitive::Initialize(device, data)  × 서브메시 수
├── ParseDiffuseTex(ASSETS_DIR/Tower/Tower.matl)
│   └── "diffuse_tex=" 첫 번째 항목 추출
└── ITexture::LoadFromFile(device, wpath)

SubmitRenderState
└── for each mesh → scene->AddMesh(mesh, worldMatrix, texture)
```

---

## 제약 사항

| 항목 | 제한 | 이유 |
|------|------|------|
| 인덱스 포맷 | uint16 고정 | 현재 `EIndexFormat::UInt16`만 존재 |
| 정점 레이아웃 | `PrimitiveVertex` 고정 | Skinned mesh는 Phase 2 |
| 서브메시당 정점 수 | 65535 이하 | uint16 인덱스 한계 |
| 텍스처 매핑 | 서브메시 전체에 첫 번째 diffuse 적용 | materialName이 MeshLoader에서 버려짐 |
| 좌표계 | 왼손 좌표계 강제 | `aiProcess_ConvertToLeftHanded` |

---

## Phase 2 예정 (참고)

- 서브메시별 개별 텍스처 매핑 — `MeshLoader`에서 `materialName` 반환, `.matl` 파서와 연동
- `EIndexFormat::UInt32` 추가 + 대형 메시 지원
- Skinned mesh: `SkinnedVertex` (bone indices/weights) + 본 계층 직렬화
