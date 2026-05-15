# VaEngine Skeletal Animation System: Tween & Blend

VaEngine의 스켈레탈 애니메이션 시스템은 CPU의 논리적 제어와 GPU의 고성능 연산을 결합한 **2단계 실행 구조**를 가집니다. 특히 Structured Buffer와 Compute Shader를 활용하여 수천 개의 본(Bone) 연산을 효율적으로 처리합니다.

---

## 1. 아키텍처 개요 (CPU-GPU Hybrid)

애니메이션 연산은 단순히 프레임을 재생하는 것을 넘어, 클립 간의 부부드러운 전환(Tween)과 여러 클립의 동시 합성(Blend)이 필요합니다.

- **CPU (`AnimController`)**: 애니메이션의 상태(시간, 재생 속도, 현재/다음 프레임 인덱스)를 관리합니다. 실질적인 행렬 보간 연산은 수행하지 않고, 연산에 필요한 **'가중치(Weight)'와 '인덱스'**만 계산하여 GPU로 전달합니다.
- **GPU (`BonePaletteCompute.hlsl`)**: CPU로부터 전달받은 인덱스를 사용해 `Texture2DArray`(본 행렬 데이터셋)에서 데이터를 읽어와, 실제 **행렬 보간 및 본 팔레트 생성**을 수행합니다.

---

## 2. 핵심 재생 모드

### A. PlayTween (Cross-fade Transition)
두 개의 애니메이션 클립을 시간에 따라 부드럽게 섞어주는 모드입니다. (예: 걷기 → 달리기 전환)

- **작동 원리**:
  1. `PlayTween()` 호출 시 `blendTime` 동안 `tweenTime`이 0에서 1로 증가합니다.
  2. GPU는 `CurrentClip`의 데이터와 `NextClip`의 데이터를 `lerp(Current, Next, tweenTime)`로 합성합니다.
  3. `tweenTime`이 1에 도달하면 자동으로 `NextClip`이 `CurrentClip`으로 승격되며 전환이 완료됩니다.

### B. PlayBlend (Multi-clip Weighted Blending)
동시에 3개의 클립을 특정 가중치에 따라 합성하는 모드입니다. (예: 부상 정도에 따른 걷기 애니메이션 변화)

- **작동 원리**:
  1. `blendAlpha`라는 [0, 2] 범위의 제어 값을 가집니다.
  2. **0 ≤ alpha ≤ 1**: Clip 0과 Clip 1 사이를 보간합니다.
  3. **1 < alpha ≤ 2**: Clip 1과 Clip 2 사이를 보간합니다.
  4. 이를 통해 단일 파라미터로 세 가지 상태 사이를 매끄럽게 오갈 수 있습니다.

---

## 3. GPU Offloading (Structured Buffer & Compute Shader)

기존의 방식은 CPU에서 모든 본 행렬을 계산하여 매 프레임 전송(Upload)했으나, VaEngine은 **GPU 메모리 내에서 모든 연산을 완결**합니다.

### 데이터 구조
- **TransformsMap (`Texture2DArray`)**: 모든 애니메이션 클립의 모든 프레임 본 행렬 데이터가 GPU 메모리에 미리 로드되어 있습니다.
- **TweenBuffer (`Constant/Structured Buffer`)**: CPU가 매 프레임 업데이트하는 인스턴스별 애니메이션 상태 정보(`AnimFrameDesc`)입니다.
- **BonePalette (`Structured Buffer / UAV`)**: Compute Shader가 최종 계산한 인스턴스별 본 행렬들이 저장되는 버퍼입니다.

### 실행 흐름
1. **Compute Pass**: `BonePaletteCompute_CS.hlsl`이 실행됩니다.
   - 각 스레드는 하나의 본(Bone)을 담당합니다.
   - `TransformsMap`에서 4개의 행렬(Clip A의 cur/next, Clip B의 cur/next)을 샘플링합니다.
   - 프레임 간 보간 → 클립 간 보간 순서로 최종 행렬을 산출합니다.
   - 결과를 `BonePalette` UAV 버퍼에 기록합니다.
2. **Graphics Pass**: 스킨드 메쉬 렌더링 시, 위에서 계산된 `BonePalette`를 SRV로 바인딩하여 정점 쉐이더(VS)에서 참조합니다.

---

## 4. 포트폴리오 강조 포인트
- **성능 최적화**: 본 연산을 Compute Shader로 오프로드하여 CPU 부하를 거의 0에 가깝게 유지.
- **유연성**: 단순 Tween을 넘어 3-way Blend 모드를 지원하여 정교한 애니메이션 블렌딩 가능.
- **확장성**: `Texture2DArray` 기반의 본 데이터 관리로 대량의 애니메이션 클립을 효율적으로 핸들링.
