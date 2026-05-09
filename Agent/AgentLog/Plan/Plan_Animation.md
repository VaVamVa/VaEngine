# Animation 구현 계획

작성일: 2026-05-09  
최종 수정: 2026-05-09

참고자료: `E:\Code\DX3D\lesson\DirectX11_3D_19` (ModelAnimator / ModelClip / 00_Render.fx)

---

## 목표

FBX 스켈레탈 메시 + 애니메이션 클립을 VaEngine에서 재생한다.

**Phase 1 — 기반 (구현 완료)**
- `.smesh` 포맷 — 스켈레톤 + 스키닝 정점 데이터
- `.clip` 포맷 — 애니메이션 키프레임 (본별 SRT + 본 이름, version 2)
- `Skeleton` 클래스 — 본 계층구조 + Offset 행렬
- `SkinnedMesh` — GPU 버퍼 관리 (스키닝 정점)
- `ITexture2DArray` RHI 확장 — 본 변환 행렬 GPU 업로드
- `AnimationDemo.hlsl` — VS_Animation (GPU 스키닝 + Tween Lerp), PS_Animation
- `WorldAnimatedModel` — 스켈레탈 월드 오브젝트 (Play / PlayTween)
- ImportTool 확장 — 스켈레톤 + 클립 추출 및 직렬화
- Tween Transition — `AnimController::PlayTween()` + HLSL inter-clip lerp

**Phase 2 (이후)**
- Blend 모드 (3개 클립 가중치 합성)
- Compute Shader 본 팔레트 계산 오프로드

---

## 아키텍처

```
[FBX 파일]  ──►  VaImportTool  ──►  .smesh (스켈레톤 + 스키닝 정점)
(asset-importer                 ──►  .clip × N (애니메이션 클립)
 프리셋)                         ──►  .matl + 텍스처

                    수동 복사 (_Assets/{name}/)
                               │
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
         SkmLoader         ClipLoader      MeshLoader(기존)
              │                │
              ▼                ▼
         Skeleton          AnimClipData
         SkinnedMesh         (CPU, 본 이름 포함)
              │
              ▼
        ITexture2DArray  ◄─── BakeTransformsMap (이름 기반 매핑)
        (GPU, 본 행렬)         [clipIdx][frameIdx][boneIdx]
              │
              ▼
        AnimationRenderer::Render()
          CB_TweenFrame (인스턴스별 상태)
          → VS_Animation (Skinning + Tween)
          → PS_Animation (Phong + Texture)
```

---

## 새 파일 포맷

### .smesh 바이너리 (Skinned Mesh)

> 구버전 확장자 `.skm`에서 변경됨.

```
── Header ──────────────────────────────────────────────────────
  [u32] magic       = 0x204D4B53  ('SKM ' little-endian)
  [u32] version     = 1
  [u32] boneCount
  [u32] meshCount
  [u32] reserved    = 0

── Bone × boneCount ────────────────────────────────────────────
  [str] name                  (u32 len + chars)
  [i32] parentIndex           (-1 = 루트)
  [f32×16] bindPose           (본 → 모델 공간, row-major 4×4)
  [f32×16] offsetMatrix       (모델 → 본 공간, Inverse Bind Pose)

── SubMesh × meshCount ─────────────────────────────────────────
  [str] name
  [str] materialName
  [u32] vertexCount
  [u32] vertexStride           = 80  (SkinnedVertex)
  [u8 ] vertices[count × 80]
  [u32] indexCount
  [u16] indices[indexCount]
  [u8 ] _pad  (indexCount 홀수 시 2바이트)
```

### .clip 바이너리 (Animation Clip, version 2)

> version 1 → 2: keyframe 앞에 본 이름 배열 추가.  
> 이름 기반 매핑으로 `.smesh`와 본 수/순서가 달라도 정상 동작.

```
── Header ──────────────────────────────────────────────────────
  [u32] magic       = 0x50494C43  ('CLIP' little-endian)
  [u32] version     = 2
  [f32] duration    (초 단위 전체 길이)
  [f32] frameRate   (e.g. 30.0)
  [u32] frameCount
  [u32] boneCount

── Clip name ───────────────────────────────────────────────────
  [str] name

── Bone names × boneCount ──────────────────────────────────────
  [str] boneName    (클립 내 본 이름, 인덱스 순서)

── Keyframe  [boneCount × frameCount] ──────────────────────────
  (본 우선, 프레임 내부 순서)
  Per entry:
    [f32×3]  scale       (x, y, z)
    [f32×4]  rotation    (quaternion x, y, z, w)
    [f32×3]  translation (x, y, z)
  → 1 entry = 40 bytes
  → 전체 = boneCount × frameCount × 40 bytes
```

---

## 정점 레이아웃

### SkinnedVertex (stride = 80 bytes)

| 필드 | 타입 | 오프셋 | 바이트 |
|---|---|---|---|
| `pos[3]` | float | 0 | 12 |
| `normal[3]` | float | 12 | 12 |
| `color[4]` | float | 24 | 16 |
| `uv[2]` | float | 40 | 8 |
| `boneIndex[4]` | uint32 | 48 | 16 |
| `boneWeight[4]` | float | 64 | 16 |

> `PrimitiveVertex`(48B)에서 boneIndex + boneWeight 32B 추가.

---

## 파일 레이아웃

```
Engine/
  Public/
    Object/
      Skeleton.h              BoneData + Skeleton 클래스
      WorldAnimatedModel.h
    Asset/
      SkmAsset.h              SKM_MAGIC, SkmFileHeader
      SkmLoader.h
      ClipAsset.h             CLIP_MAGIC, ClipFileHeader (version=2)
      ClipLoader.h
    Mesh/
      SkinnedVertex.h
      SkinnedMesh.h           IMesh 구현 (스키닝 정점용)
    Animation/
      AnimClip.h              AnimClipData (boneNames 포함)
      AnimController.h        TweenFrame 상태 관리
    RHI/
      Texture/
        ITexture2DArray.h
    Render/
      AnimationRenderer.h

  Private/
    Object/
      Skeleton.cpp
      WorldAnimatedModel.cpp  BakeTransformsMap: 이름 기반 본 매핑
    Asset/
      SkmLoader.cpp
      ClipLoader.cpp          bone 이름 읽기 포함
    Mesh/
      SkinnedMesh.cpp
    Animation/
      AnimController.cpp
    RHI/DirectX/Texture/
      Texture2DArray_DX.h
      Texture2DArray_DX.cpp
    Render/
      AnimationRenderer.cpp

  _Shaders/DirectX/
    AnimationDemo.hlsl

Application/Source/
  Public/WorldObjects/
    WO_Kachujin.h
  Private/WorldObjects/
    WO_Kachujin.cpp           경로: Kachujin.smesh + 각 .clip

Tools/ImportTool/
  SkmExporter.h / .cpp        출력 확장자 .smesh
  ClipExporter.h / .cpp       bone 이름 출력 포함
  Converter.h / .cpp          AnimClipIntermediate에 boneNames 추가
  Main.cpp                    분기: hasAnimations 우선 → hasBones → 정적 메시
```

---

## 클래스 설계

### Skeleton

```cpp
struct BoneData {
    std::string name;
    int         parentIndex;   // -1 = 루트
    Matrix4x4   bindPose;      // 본 → 모델 공간
    Matrix4x4   offsetMatrix;  // 모델 → 본 공간 (Inverse Bind Pose)
};

class Skeleton {
public:
    const std::vector<BoneData>& GetBones() const;
    int FindBone(const std::string& name) const;  // -1 = 없음
private:
    std::vector<BoneData> bones;
};
```

### AnimClipData

```cpp
struct KeyframeEntry {
    Vector3    scale;
    Quaternion rotation;
    Vector3    translation;
};

struct AnimClipData {
    std::string name;
    float       duration;
    float       frameRate;
    uint32_t    frameCount;
    uint32_t    boneCount;
    std::vector<std::string>                boneNames;  // [boneIndex]
    std::vector<std::vector<KeyframeEntry>> keyframes;  // [boneIndex][frameIndex]
};
```

### BakeTransformsMap 매핑 방식

`.smesh` 본 수(N)와 `.clip` 본 수(M)가 다를 수 있음.  
skeleton의 각 bone을 이름으로 clip 내 인덱스에 매핑; 없는 본은 identity 처리.

```cpp
std::unordered_map<std::string, uint32_t> clipBoneIdx;
for (uint32_t i = 0; i < clip.boneCount; ++i)
    clipBoneIdx[clip.boneNames[i]] = i;

// per bone:
auto it = clipBoneIdx.find(bones[b].name);
Matrix4x4 local = (it != clipBoneIdx.end())
    ? SRT(clip.keyframes[it->second][frameIdx])
    : Matrix4x4::Identity();
```

### ITexture2DArray

```cpp
class ITexture2DArray {
public:
    virtual ~ITexture2DArray() = default;
    // width = boneCount*4, height = maxFrames, arraySize = clipCount
    // format = R32G32B32A32_FLOAT
    virtual void Upload(IRenderDevice* device,
                        const void*    data,
                        uint32_t       width,
                        uint32_t       height,
                        uint32_t       arraySize) = 0;
    virtual void Bind(ICommandList* cmdList, uint32_t slot) = 0;
};
```

### AnimController

```cpp
struct AnimFrameDesc {
    int32_t  clip      = 0;
    uint32_t curFrame  = 0;
    uint32_t nextFrame = 0;
    float    time      = 0.0f;  // 프레임 간 보간값 [0, 1]
};

struct TweenFrameDesc {
    float         tweenTime = 0.0f;  // 전환 진행도 [0,1] — 0=current만, 1=next만
    float         _pad[3]   = {};
    AnimFrameDesc current;
    AnimFrameDesc next;
};

class AnimController {
public:
    void Resize(uint32_t instanceCount);
    void Play    (uint32_t instance, uint32_t clip, float speed = 1.0f);
    void PlayTween(uint32_t instance, uint32_t nextClip, float blendTime, float speed = 1.0f);
    void Update  (float deltaTime, const std::vector<AnimClipData>& clips);
    const std::vector<TweenFrameDesc>& GetTweenFrames() const;
    uint32_t InstanceCount() const;
};
```

`PlayTween()` 동작:
- `blendTime` 초 동안 `tweenTime` 0→1 증가
- 1.0 도달 시: next 클립이 current로 승격, `tweenTime` 리셋
- current / next 클립 프레임은 독립적으로 진행

---

## 셰이더 설계 (AnimationDemo.hlsl)

### cbuffer / 리소스 바인딩

```
b0: CB_ViewProj        (기존 유지)
b2: CB_Lights          (기존 유지)
b3: CB_TweenFrame      (신규 — 인스턴스별 애니메이션 상태)

t0: DiffuseMap         (기존)
t1: TransformsMap      (신규 — Texture2DArray, 본 변환 행렬)
```

### Texture2DArray 레이아웃

```
ArraySize = 클립 수
Width     = boneCount × 4   (행렬 1개 = float4 행 × 4)
Height    = maxFrameCount
Format    = R32G32B32A32_FLOAT

접근:
  float4 row = TransformsMap.Load(int4(boneIndex*4 + r, frameIndex, clipIndex, 0));
```

### VS_Animation 처리 흐름

```
1. SV_InstanceID → TweenFrames[id] 로드
2. ComputeSkinMatrix(current):
     4개 boneIndex 각각에 대해
       curMat  = TransformsMap.Load(current.clip, current.curFrame,  boneIdx)
       nextMat = TransformsMap.Load(current.clip, current.nextFrame, boneIdx)
       boneMat = lerp(curMat, nextMat, current.time)  // intra-frame lerp
     boneWeight 가중합 → currentSkinMat
3. TweenTime > 0 이면:
     ComputeSkinMatrix(next) → nextSkinMat  (동일 방식)
     skinMat = lerp(currentSkinMat, nextSkinMat, TweenTime)  // inter-clip lerp
   그렇지 않으면:
     skinMat = currentSkinMat
4. 인스턴스 world 행렬 × gViewProj 적용
5. wPos, normal, uv → PS로 전달
```

---

## ImportTool FBX 분류 로직

```
Inspect(fbx):
  hasBones        = scene->mMeshes에 bone이 있는가
  hasAnimations   = duration > 0.5초인 animation이 있는가
                    (bind pose 트랙 false positive 방지)

분기:
  hasAnimations                → clip 추출 (.clip)
                                  메시 있으면 .smesh도 생성
  !hasAnimations && hasBones   → skinned mesh 추출 (.smesh)
  !hasAnimations && !hasBones  → static mesh 추출 (.mesh)

ConvertSkinned():
  boneNameSet = mesh bones
  boneNameSet가 비어있으면 → animation channels에서 보강
  (animation-only FBX 지원)
```

---

## 구현 순서 체크리스트

### Step 1 — 공유 포맷 헤더
- [x] `Engine/Public/Mesh/SkinnedVertex.h`
- [x] `Engine/Public/Asset/SkmAsset.h`
- [x] `Engine/Public/Asset/ClipAsset.h`  (version=2)

### Step 2 — Skeleton 클래스
- [x] `Engine/Public/Object/Skeleton.h`
- [x] `Engine/Private/Object/Skeleton.cpp`

### Step 3 — AnimClipData + AnimController
- [x] `Engine/Public/Animation/AnimClip.h`  (boneNames 추가)
- [x] `Engine/Public/Animation/AnimController.h`
- [x] `Engine/Private/Animation/AnimController.cpp`

### Step 4 — SkinnedMesh
- [x] `Engine/Public/Mesh/SkinnedMesh.h`
- [x] `Engine/Private/Mesh/SkinnedMesh.cpp`

### Step 5 — ITexture2DArray RHI
- [x] `Engine/Public/RHI/Texture/ITexture2DArray.h`
- [x] `Engine/Private/RHI/DirectX/Texture/Texture2DArray_DX.h/.cpp`
- [x] `IRenderDevice::CreateTexture2DArray()` 추가

### Step 6 — Loaders
- [x] `Engine/Public/Asset/SkmLoader.h`
- [x] `Engine/Private/Asset/SkmLoader.cpp`
- [x] `Engine/Public/Asset/ClipLoader.h`
- [x] `Engine/Private/Asset/ClipLoader.cpp`  (bone 이름 읽기 포함)

### Step 7 — AnimationDemo.hlsl + AnimationRenderer
- [x] `Engine/_Shaders/DirectX/AnimationDemo.hlsl`
- [x] `Engine/Public/Render/AnimationRenderer.h`
- [x] `Engine/Private/Render/AnimationRenderer.cpp`
- [x] `Engine/CMakeLists.txt` 셰이더 컴파일 타겟 추가

### Step 8 — WorldAnimatedModel
- [x] `Engine/Public/Object/WorldAnimatedModel.h`
- [x] `Engine/Private/Object/WorldAnimatedModel.cpp`
  - .smesh + .clip 로드
  - BakeTransformsMap: 이름 기반 본 매핑 (본 수/순서 불일치 대응)
  - AnimController 소유

### Step 9 — ImportTool 확장
- [x] `Tools/ImportTool/SkmExporter.h/.cpp`  (출력 확장자 .smesh)
- [x] `Tools/ImportTool/ClipExporter.h/.cpp`  (bone 이름 출력)
- [x] `Converter.h` — `AnimClipIntermediate.boneNames` 추가
- [x] `Converter.cpp` — animation-only FBX 본 수집 보강, boneNames 채우기
- [x] `Main.cpp` — hasAnimations 우선 분기, bind pose 오분류 수정

### Step 10 — 통합 검증 (WO_Kachujin)
- [x] Kachujin FBX → ImportTool로 .smesh + .clip 출력
- [x] `WO_Kachujin` : `WorldAnimatedModel` 상속
- [x] Kachujin 화면 렌더링 최종 확인

### Step 11 — Tween Transition (클립 간 부드러운 전환)
- [x] `AnimController.h`: `tweenTime=0.0f`; `InstanceState` tween 필드 추가; `PlayTween()` 선언
- [x] `AnimController.cpp`: `AdvanceClip()` 헬퍼; `PlayTween()` 구현; `Update()` tween 로직
- [x] `AnimationDemo.hlsl`: `if (TweenTime > 0)` 브랜치 — `ComputeSkinMatrix(next)` + `lerp(currentSkinMat, nextSkinMat, TweenTime)`
- [x] `WorldAnimatedModel.h/.cpp`: `PlayTween(nextClip, blendTime, instanceIndex, speed)` wrapper
- [x] 동작 확인 (2026-05-09)

---

## 제약 사항

| 항목 | 제한 | 이유 |
|---|---|---|
| 최대 본 수 | 250 | Texture2DArray width 및 참고자료 기준 |
| 최대 인스턴스 | 500 | CB_TweenFrame 크기 (500 × 48B = 24KB) |
| 최대 키프레임 | 500 | Texture2DArray height |
| 인덱스 포맷 | uint16 고정 | 정적 메시와 동일 |
| 블렌드 모드 | Phase 2 | 3클립 가중치 합성 — CB_BlendFrame 별도 설계 필요 |
| Tween 전환 | 완료 (Step 11) | `AnimController::PlayTween()` + HLSL inter-clip lerp |
| .clip 하위호환 | 없음 | version 2부터 bone 이름 필수 |

---

## Phase 2 예정

- `CB_BlendFrame` + `SetBlendWorld()` — 3클립 가중치 블렌딩
- Compute Shader 본 팔레트 계산 GPU 오프로드
- `EIndexFormat::UInt32` (대형 스켈레탈 메시)
