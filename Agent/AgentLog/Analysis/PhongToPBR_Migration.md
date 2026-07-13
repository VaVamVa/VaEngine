# Phong → PBR 마이그레이션 분석 리뷰

작성일: 2026-07-06  
대상 커밋: `2901a7c` → `7a59ab2` → `660d924`

---

## 1. 커밋 역할 분담

| 커밋 | 날짜 | 역할 |
|---|---|---|
| `2901a7c` | 2026-05-27 | Deferred Renderer 인프라 구축. G-Buffer MRT, Compute Lighting(Phong 임시), SceneRenderer 오케스트레이션 |
| `7a59ab2` | 2026-06-15 | 설계 단계. Plan_Material_Driven.md 작성, IMaterial 재정의 방향 확정, CLAUDE.md 원칙 추가 |
| `660d924` | 2026-07-06 | 실행 단계. Cook-Torrance GGX PBR 완성, Material/Light 버퍼 분리, 2-Pass 투명, 크래시 2건 수정 |

---

## 2. 2901a7c — Deferred Renderer 구현 분석

### 2-1. G-Buffer 레이아웃 설계 결정

```
RT0: Albedo.RGB + AO.A            R8G8B8A8_UNORM
RT1: WorldNormal.XYZ + Rough.W    R16G16B16A16_FLOAT   ← 법선 정밀도 확보
RT2: Metallic.R (GBA 예약)        R8G8B8A8_UNORM
hdrOut: 조명 출력 UAV             R16G16B16A16_FLOAT
```

**RT1이 FLOAT인 이유**: 법선 벡터 성분이 [-1, 1] 범위이므로 UNORM 인코딩 시 별도 변환 필요 → FLOAT 직접 저장으로 단순화.  
**RT2 단일 채널**: 현재는 metallic만 사용. GBA는 향후 Normal Mapping tangent 저장, roughness 채널 추가, emissive 마스크 등으로 확장 가능.

### 2-2. 패스 아키텍처

```
Sky (hdrOut Clear)
  → BonePaletteCompute (UAV Write)
  → GBufferPass (MRT Write + Depth Write)
  → DeferredLightingPass (Compute, SRV Read → UAV Write)
  → BlitPass (SRV Read → Backbuffer RTV)
  → TransparentPass (Backbuffer Load, Depth Read-Only)
  → DebugLine/Text
```

RenderGraph가 `DeclareResources(reads, writes)`로 바이어리어를 자동 삽입하는 구조. 패스별 책임이 명확하게 분리되어 있다.

### 2-3. 최초 버전의 알려진 중간 상태 (이후 커밋에서 수정)

#### ① GBufferMaterialData vs MaterialData 크기 불일치

```cpp
// 2901a7c GBufferRenderer.cpp (신규 도입 시)
struct GBufferMaterialData {   // b1 — 16 bytes
    float roughness;
    float metallic;
    float _pad[2];
};

// 2901a7c MaterialData (IMaterial.h)
struct MaterialData {          // 48 bytes
    float albedo[4];
    float emissive[3];
    float roughness;
    float metallic;
    float ao;
    float alphaThreshold;
    float _pad;
};
```

GBufferRenderer가 b1에 `MaterialData`(48B)를 올리지만 HLSL은 `GBufferMaterialData`(16B) 레이아웃으로 읽어 roughness·metallic 값을 오독. 시각적으로 모든 오브젝트가 동일한 재질로 보이는 결함.  
→ **660d924에서 `GBufferMaterial.hlsli` 신규 도입으로 해소.**

#### ② DL_LightsBufferData에 MaterialData 잔류

```cpp
// 2901a7c DeferredLightingRenderer.cpp
struct DL_LightsBufferData {
    DirectionalLightData dirLight;      //  32B — offset 0
    PointLightData       pointLights[8]; // 384B — offset 32
    SpotLightData        spotLights[4];  // 256B — offset 416
    MaterialData         material;       //  48B — offset 672   ← 문제
    float                eyePosW[3];    //  12B — offset 720
    int32_t              numPointLights; //   4B
    int32_t              numSpotLights;  //   4B
    float                _lightPad[3];   //  12B
};
```

반면 HLSL `CB_Lights`는 `gEyePosW`가 offset 672에 위치. C++에서 쓴 `eyePosW`를 HLSL이 `material` 데이터로 읽음 → eyePos = 0 → specular 벡터 오산출. numPointLights도 오독(= 0) → Point Lights 전체 무효.  
→ **660d924에서 `MaterialData material` 제거(704B 정렬)로 해소.**

#### ③ Phong 임시 조명

DeferredLighting.hlsl에 `RoughnessToSpecPow()` 함수를 통해 `roughness → specular power`로 변환. G-Buffer는 PBR 레이아웃이지만 조명 계산은 Phong. 코드에 명시적으로 `// Phong 기반 — 추후 PBR로 교체` 주석 기재.  
→ **660d924에서 Cook-Torrance GGX로 전면 교체.**

---

## 3. 7a59ab2 — 설계 단계 분석

이 커밋의 핵심은 코드보다 **설계 결정 기록**이다.

### 3-1. 핵심 설계 결정 (Plan_Material_Driven.md)

#### D-1: Material 버퍼 업로드 전략

| 방안 | 장점 | 단점 |
|---|---|---|
| Dirty flag (채택) | 정적 오브젝트는 0회 업로드, 구현 단순 | 프레임간 일관성 책임이 Material에 있음 |
| 매 프레임 전체 업로드 | 단순, 타이밍 문제 없음 | 메모리 대역폭 낭비 |
| Push Constants | 업로드 비용 최소 | Root Signature 변경 비용, Android 우선 |
| Structured Buffer + index | GPU 친화적 | 구현 복잡도 높음, Material 수 많을 때 유효 |

Dirty flag 채택 근거: 현재 오브젝트 수가 적고 정적 오브젝트 비중이 높아 1프레임 후 업로드 0회.

#### D-2: Material과 CB_Lights 분리

기존에 CB_Lights(b2)에 `Material` 구조체가 포함되어 있어 조명 변경 시 재질도 함께 업로드해야 했다.  
이를 `CB_GBufferMaterial(b1)` / `CB_Lights(b2)`로 분리 → 조명·재질 독립 업로드 가능.

#### D-3: DrawGroup 기준 변경

```
기존: DrawGroup 기준 = (IMesh*, ITexture*)  → 같은 텍스처끼리 묶음
변경: DrawGroup 기준 = (IMesh*, IMaterial*) → 같은 Material 인스턴스끼리 묶음
```

Material 변경 시에만 버퍼 업로드·바인딩 실행. 같은 Material을 쓰는 오브젝트는 PSO·버퍼 전환 없이 연속 드로우.

---

## 4. 660d924 — PBR + Material 완성 분석

### 4-1. Cook-Torrance GGX 구현 검토

```hlsl
// D_GGX: Trowbridge-Reitz 법선 분포 함수
float D_GGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * d * d);
}

// F_Schlick: Fresnel 반사율 근사
float3 F_Schlick(float VdotH, float3 F0) {
    return F0 + (1.0f - F0) * pow(1.0f - VdotH, 5.0f);
}

// G_Smith: 기하 감쇠 (Schlick-GGX)
float G_Smith(float NdotV, float NdotL, float roughness) {
    float r  = roughness + 1.0f;
    float k  = (r * r) / 8.0f;
    float gv = NdotV / (NdotV * (1.0f - k) + k);
    float gl = NdotL / (NdotL * (1.0f - k) + k);
    return gv * gl;
}

// EvalBRDF
float3 EvalBRDF(...) {
    float3 spec = (D * F * G) / max(4.0f * NdotV * NdotL, 0.001f);
    float3 kD   = (1.0f - F) * (1.0f - metallic);
    return (kD * albedo / PI + spec) * radiance * NdotL;
}
```

**정확성 평가:**

| 항목 | 평가 | 비고 |
|---|---|---|
| D_GGX 분모 | ✅ | `NdotH=0, a2=1`일 때 `d=1` → `PI` (0 나눔 없음) |
| F_Schlick 범위 | ✅ | `VdotH = max(dot, 0)` → `1-VdotH ∈ [0,1]` |
| G_Smith k 리매핑 | ✅ | `k=(r+1)²/8` — 직접 조명용 Epic 리매핑. 정확 |
| NdotL 위치 | ✅ | diffuse + specular 모두 NdotL 적용. 렌더링 방정식과 일치 |
| 에너지 보존 | ✅ | `kD = (1-F)*(1-metallic)` → diffuse+spec 합이 입사 에너지 초과 안 함 |
| F0 기본값 | ✅ | 비금속 `float3(0.04)`. metallic=1 → F0=albedo |

**⚠ 향후 IBL 추가 시 주의점:**  
`G_Smith`의 k 리매핑 `(r+1)²/8`은 **직접 조명** 전용이다. IBL(Image-Based Lighting) 구현 시 IBL 항의 기하 감쇠에는 `k = roughness²/2`를 별도 적용해야 한다. 현재 직접 조명만 있으므로 문제없음.

### 4-2. 버퍼 레이아웃 정렬 확인 (C++ ↔ HLSL)

```
DL_LightsBufferData (C++, 704B):
  offset   0: DirectionalLightData  (32B)
  offset  32: PointLightData[8]    (384B)
  offset 416: SpotLightData[4]     (256B)
  offset 672: eyePosW[3]           (12B)
  offset 684: numPointLights        (4B)
  offset 688: numSpotLights         (4B)
  offset 692: _lightPad[3]         (12B)
  → CBV 256-align → 768B

CB_Lights (HLSL, b2):
  DirectionalLight gDirLight;          // 32B  (color 12, intensity 4, direction 12, pad 4)
  PointLight       gPointLights[8];    // 384B (color 12, range 4, pos 12, intensity 4, att 12, pad 4)
  SpotLight        gSpotLights[4];     // 256B (color 12, range 4, pos 12, intensity 4, dir 12, spot 4, att 12, pad 4)
  float3           gEyePosW;           // 12B  → offset 672 ✅
  int              gNumPointLights;    //  4B
  int              gNumSpotLights;     //  4B
  float3           _lightPad;          // 12B
```

C++ 구조체와 HLSL cbuffer 레이아웃이 완전 일치.

### 4-3. AnimationRenderer pre-bind 수정

**문제**: `WorldAnimatedModel`은 `cmd.material == nullptr`. 루프 진입 전 root1/root3가 미바인딩 상태.

```cpp
// 수정 후 — 루프 진입 전 폴백 바인딩
material->UpdateBufferIfDirty();
cmdList->SetConstantBuffer(material->GetBuffer(), 1);  // root 1 → b1
defaultTexture->Bind(cmdList, 3);                       // root 3 → t0
IMaterial* boundMat = nullptr;

for (const RenderCommand& cmd : cmds) {
    // cmd.material != boundMat 비교는 nullptr 전환도 정확히 처리
    if (cmd.material != boundMat) {
        IMaterial* effMat = cmd.material ? cmd.material : material.get();
        // ...rebind
        boundMat = cmd.material;   // ← nullptr 포함해서 추적 → 재전환 시 재바인딩 ✅
    }
}
```

`boundMat`이 `cmd.material`(원본 포인터)을 추적해 `nullptr → ptr → nullptr` 전환도 정확히 처리.

### 4-4. 2-Pass 투명 렌더링

```
Pass 1: CullMode::Front (뒷면 먼저)
         → 카메라 반대편 면 렌더. 볼록 오브젝트의 내면 가시화.
Pass 2: CullMode::Back  (앞면)
         → 카메라를 향하는 면 렌더. 앞면 위에 AlphaBlend 합성.
```

두 패스 모두 `depthWrite = false`. 불투명 오브젝트의 depth를 Read-Only DSV로 공유 → 투명 오브젝트가 불투명 오브젝트 뒤에 숨는 depth test는 동작.

**제약**: 비볼록(non-convex) 오브젝트나 교차하는 투명 오브젝트는 정렬 오류 발생 가능. `ResolveOIT()` stub이 확장 지점으로 보존되어 있음.

---

## 5. PBR 라이팅 이론 정의

### 5-1. 렌더링 방정식 (Kajiya, 1986)

모든 PBR 구현의 출발점. 한 점 `p`에서 방향 `ωo`로 나가는 빛의 양:

```
Lo(p, ωo) = Le(p, ωo) + ∫_Ω fr(p, ωi, ωo) · Li(p, ωi) · (N·ωi) dωi
```

| 항 | 의미 |
|---|---|
| `Lo(p, ωo)` | 점 p에서 방향 ωo로 나가는 복사 휘도(Radiance) |
| `Le(p, ωo)` | 자체 발광(Emissive) |
| `fr(p, ωi, ωo)` | BRDF — 입사 방향 ωi에서 반사 방향 ωo로 얼마나 반사하는가 |
| `Li(p, ωi)` | 방향 ωi에서 들어오는 입사 복사 휘도 |
| `(N·ωi)` | Lambert cosine term — 입사각에 따른 조사면 넓이 보정 |
| `∫_Ω dωi` | 반구 전체 방향에 대한 적분 |

실시간에서는 이산 광원 합산으로 적분을 근사한다:

```
Lo(p, ωo) = Le + Σ_lights fr(p, ωi, ωo) · Li · (N·ωi)
```

---

### 5-2. Cook-Torrance BRDF

실시간 PBR의 표준 BRDF. 미세면(microfacet) 이론 기반:

```
fr(ωi, ωo) = kD · fd + ks · fs

fd = albedo / π                              (Lambertian diffuse)
fs = D(h) · F(v, h) · G(l, v, h)            (Cook-Torrance specular)
    ─────────────────────────────
          4 · (N·v) · (N·l)
```

| 계수 | 의미 | 결정 인자 |
|---|---|---|
| `kD = (1 - F) · (1 - metallic)` | 확산 기여 비율 | Fresnel, 금속도 |
| `ks = F` | 정반사 기여 비율 | Fresnel (이미 fs 분자에 포함) |
| `D(h)` | 법선 분포 함수(NDF) — h 방향 미세면 밀도 | roughness |
| `F(v, h)` | Fresnel 반사율 — 입사각에 따른 반사 비율 | F0, 입사각 |
| `G(l, v, h)` | 기하 감쇠 — 미세면 간 자기 그림자/차폐 | roughness |

에너지 보존 조건: `kD + ks ≤ 1` — 반사량이 입사량을 초과하지 않음.  
→ `kD = (1-F)*(1-metallic)`으로 diffuse를 Fresnel 반사량만큼 제거해 보장.

---

### 5-3. 각 항의 수학적 정의 (레퍼런스)

#### D — Trowbridge-Reitz GGX (Walter et al., 2007)

```
D_GGX(N, H, α) = α² / (π · ((N·H)² · (α² - 1) + 1)²)

α = roughness²    (Disney perceptual remapping, Burley 2012)
```

α²을 두 번 제곱해 tail이 넓고 하이라이트가 뚜렷한 분포를 만든다. 낮은 roughness에서 날카로운 스펙큘러, 높은 roughness에서 넓은 하이라이트.

#### F — Fresnel-Schlick 근사 (Schlick, 1994)

```
F(V, H) = F0 + (1 - F0) · (1 - V·H)^5

F0 = lerp(0.04, albedo, metallic)
```

`F0`는 수직 입사각(θ=0°) 반사율. 비금속은 약 4%(0.04), 금속은 albedo 색상이 그대로 F0. 입사각이 클수록(grazing angle) 반사율이 1에 수렴(Fresnel Edge).

#### G — Smith Schlick-GGX (Schlick 1994, Smith 1967)

```
G_SchlickGGX(NdotX, k) = NdotX / (NdotX · (1 - k) + k)
G_Smith(l, v) = G_SchlickGGX(N·l, k) · G_SchlickGGX(N·v, k)

k_direct = (roughness + 1)² / 8      (Epic/Karis, 2013 — 직접 조명)
k_IBL    = roughness² / 2            (IBL 전용)
```

두 방향(광원·시점)에서의 미세면 차폐를 독립적으로 계산해 곱한다. k가 클수록 더 많이 차폐 → 어둡게.

---

### 5-4. 단일 광원 최종 기여식

```
Lo_light = (kD · albedo/π + D·F·G / (4·NdotV·NdotL)) · radiance · NdotL

radiance = light.color * light.intensity (* attenuation_if_point_spot)
```

---

## 6. PBR 구현 정확성 평가

### 6-1. 핵심 함수 항목별 평가

#### D_GGX

```hlsl
float a  = roughness * roughness;   // α = r² (Disney remapping)
float a2 = a * a;                   // α²
float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
return a2 / (PI * d * d);
```

수식이 레퍼런스와 완전히 일치. Disney perceptual remapping(r²) 적용. ✅

**예외 케이스**: `roughness = 0`이면 `a = a2 = 0`, `d = 1 - NdotH²`.  
`NdotH → 1`에서 `d → 0` → `D = 0/0` 부정형. 실제 평가에서 분자도 0이므로 결과는 0 — 완전 거울면이 스펙큘러 0으로 퇴화. 클램프 없음.

```hlsl
// 권장 수정
float a = max(roughness * roughness, 0.0001f);
```

~~⚠️ **`roughness = 0` 설정 시 시각적 결함** (무결점 거울면 불가).~~

**[2026-07-11 해결]** `Lighting.hlsli` D_GGX에 `float a = max(roughness * roughness, 0.0001f);` 클램프 적용(권장 수정과 동일). Pre-0 항목으로 처리 완료.

---

#### F_Schlick

```hlsl
return F0 + (1.0f - F0) * pow(1.0f - VdotH, 5.0f);
```

수식이 레퍼런스와 정확히 일치. `VdotH = max(dot, 0)` clamp로 음수 pow 방지. ✅

---

#### G_Smith

```hlsl
float r  = roughness + 1.0f;
float k  = (r * r) / 8.0f;   // k_direct
```

직접 조명용 k 리매핑 정확. ~~IBL 추가 시 분리 필요.~~ ✅ (직접 조명 한정)

**[2026-07-11 해결]** `G_Smith(NdotV, NdotL, k)`를 k를 직접 받는 코어 함수로 두고, `G_Smith_Direct(NdotV, NdotL, roughness)`가 `k=(roughness+1)²/8`을 계산해 위임하는 구조로 분리. `EvalBRDF`는 `G_Smith_Direct` 호출로 교체. `G_Smith_IBL(k=roughness²/2)` 래퍼는 Specular IBL 호출부가 생기는 시점(Stage B)에 추가 예정 — 사용처 없는 함수를 미리 추가하지 않음.

---

#### EvalBRDF

```hlsl
float3 spec = (D * F * G) / max(4.0f * NdotV * NdotL, 0.001f);
float3 kD   = (1.0f - F) * (1.0f - metallic);
return (kD * albedo / PI + spec) * radiance * NdotL;
```

| 검증 항목 | 결과 |
|---|---|
| specular 분모 `4 · NdotV · NdotL` | ✅ 정확 |
| 0 나눔 보호 `max(_, 0.001)` | ✅ 적절 |
| kD = `(1-F)*(1-metallic)` | ✅ 에너지 보존 |
| diffuse = `albedo / π` | ✅ Lambertian 정의와 일치 |
| `* radiance * NdotL` | ✅ 렌더링 방정식 NdotL 항 정확 |
| F가 spec 분자에 포함 → kD·diffuse + spec | ✅ 이중 계산 없음 |

EvalBRDF는 레퍼런스 Cook-Torrance와 수식 수준에서 완전히 일치한다. ✅

---

### 6-2. G-Buffer 경로 vs Forward 경로 차이

#### ~~❌ Deferred 경로에 Emissive 누락~~ → ✅ 해결(2026-07-11)

```hlsl
// DeferredLighting.hlsl
outHDR[id.xy] = float4(Lo + ambient, 1.0f);   // ← gEmissive 없음
```

```hlsl
// ForwardOpaque.hlsl (정상)
float3 color = Lo + ambient + gEmissive;
```
- G-Buffer(RT0~RT2)에 Emissive 슬롯이 없다. 따라서 GBufferPass를 통하는 정적 메시는 `gEmissive > 0`을 설정해도 DeferredLighting 단계에서 반영되지 않는다. Forward 경로(투명, 스키닝)는 정상 동작.

**수정 방법**:  
RT2의 GBA 예약 채널 중 하나에 Emissive를 팩킹하거나, 별도 Emissive RT를 추가한다.

```hlsl
// GBuffer.hlsl
o.rt2 = float4(gMetallic, gEmissive.r, gEmissive.g, gEmissive.b);  // 예시

// DeferredLighting.hlsl
float3 emissive = gMaterialBuf[id.xy].gba;  // 재구성
outHDR[id.xy]  = float4(Lo + ambient + emissive, 1.0f);
```

**[2026-07-11 해결]** Pre-2 항목으로 `GBuffer.hlsl`/`GBufferSkinned.hlsl` 양쪽 RT2를 `Metallic(R) + Emissive(GBA)`로 팩킹, `DeferredLighting.hlsl`이 `gMaterialBuf[id.xy].gba`를 복원해 최종 출력에 가산 — 권장 수정과 동일한 방식으로 적용됨.

---

#### ⚠️ 법선 월드 스페이스 저장 — 비효율

```hlsl
// GBuffer.hlsl VSMain
o.normal = normalize(mul(float4(input.normal, 0.0f), world).xyz);

// GBuffer PSMain
o.rt1 = float4(N, gRoughness);   // XYZ 3채널 모두 사용
```
- 법선은 단위 벡터이므로 XYZ 세 값이 독립적이지 않다(`|N| = 1`). XY만 저장하고 Z를 재구성하면 RT1에서 채널 하나를 절약할 수 있다.

```hlsl
// 저장: XY만 (octahedral or hemisphere encoding)
o.rt1 = float4(N.xy, gRoughness, 0);

// 복원 (DeferredLighting.hlsl)
float2 nxy = normalRough.xy;
float  nz  = sqrt(saturate(1.0f - dot(nxy, nxy)));
float3 N   = normalize(float3(nxy, nz));
```
- 현재 R16G16B16A16_FLOAT 포맷이므로 16-bit 3채널 = 48-bit 사용. XY 저장 시 32-bit로 줄고 남은 슬롯에 다른 데이터 가능. Normal Mapping 구현(Phase 7) 전에 고려 가치 있음.

---

#### ~~⚠️ Point Light 감쇠 — 물리 비표준~~ → ✅ 해결(2026-07-11)

```hlsl
float att = 1.0f / dot(gPointLights[i].attenuation, float3(1.0f, dist, dist * dist));
```
- 물리적으로 정확한 점광원 감쇠는 역제곱 법칙 `1/d²`다. 현재 `a0 + a1·d + a2·d²` 형태는 Phong 시절 상수항으로 분모 0 방지를 하던 방식. `a0 > 0`이면 `dist = 0`에서도 유한값이 나오므로 에너지 보존을 위반한다.

PBR 표준 감쇠(Frostbite/UE 기반):
```hlsl
float distSq    = dist * dist;
float rangeSq   = range * range;
float att       = 1.0f / max(distSq, 0.0001f);
float windowing = pow(saturate(1.0f - (distSq / rangeSq)), 2.0f);  // 부드러운 차단
float3 radiance = color * intensity * att * windowing;
```
- 현재 구현은 아티스틱 조정 가능성이 높지만 물리적 에너지 보존을 만족하지 않는다. 광원 수가 많아지거나 HDR 파이프라인이 강화될 때 수정 가치가 있다.

**[2026-07-11 해결]** Pre-1 항목으로 `Lighting.hlsli`에 `CalcDistanceAttenuation(dist, range)` 공용 함수를 추가하고, `DeferredLighting.hlsl`/`ForwardOpaque.hlsl`/`Transparent.hlsli` 3곳에 중복 인라인되어 있던 감쇠 계산을 모두 이 함수 호출로 교체(Point+Spot 공통). 더 이상 쓰이지 않는 `attenuation` 필드(`PointLightData`/`SpotLightData`)와 `SetAttenuation` 인터페이스도 함께 제거.

---

#### ~~⚠️ Tonemapping 부재 — HDR 클램핑만 발생~~ → ✅ 해결(2026-07-11)

```hlsl
// Blit.hlsl (추정) — hdrOut SRV → Backbuffer (R8G8B8A8_UNORM)
```

`hdrOut`이 R16G16B16A16_FLOAT임에도 Blit 단계에서 단순 복사만 수행한다. 백버퍼 포맷이 UNORM이면 1.0 초과 값이 자동 클램핑되어 HDR 값이 날아간다. 결과적으로 밝은 영역이 흰색으로 날리는 "번짐(bloom blowout)" 현상이 발생한다.

Reinhard Tonemapping (최소 구현):
```hlsl
float3 tonemap = color / (color + 1.0f);  // Reinhard
```

ACES Filmic (영화 표준):
```hlsl
float3 x = color;
float3 tonemap = (x * (2.51f*x + 0.03f)) / (x * (2.43f*x + 0.59f) + 0.14f);
```

**[2026-07-11 해결]** Phase 1-1 항목으로 Blit 단계에 ACES Filmic + Exposure를 적용. 동시에 발견된 회귀 요인(Transparent 패스가 Blit 이후 backBuffer에 직접 그려져 톤맵을 건너뛰던 문제)도 Transparent를 `hdrOut`(RTV) 타겟으로 재배치해 함께 해소.

---

### 6-3. 구현 평가 요약표

**[2026-07-11 갱신]** 아래 표는 2026-07-06 작성 시점 평가다. 이후 진행분은 각 행에 취소선 + 현재 상태로 반영.

| 항목 | 평가(작성 시점) | 비고 | 현재 상태 |
|---|---|---|---|
| D_GGX 수식 | ✅ 정확 | Disney r² remapping 포함 | 변경 없음 |
| F_Schlick 수식 | ✅ 정확 | | 변경 없음 |
| G_Smith (직접 조명) | ✅ 정확 | k = (r+1)²/8 | 변경 없음(수식 자체는 그대로, k 파라미터화만 진행) |
| EvalBRDF 렌더링 방정식 | ✅ 정확 | kD, NdotL, π 모두 정확 | 변경 없음 |
| 에너지 보존 kD 계산 | ✅ 정확 | (1-F)*(1-metallic) | 변경 없음 |
| Lambertian diffuse / π | ✅ 정확 | | 변경 없음 |
| F0 비금속/금속 처리 | ✅ 정확 | 0.04 / albedo lerp | 변경 없음 |
| ~~Emissive (Deferred)~~ | ~~❌ 누락~~ | ~~G-Buffer에 슬롯 없음~~ | ✅ 해결(2026-07-11) |
| ~~roughness = 0 처리~~ | ~~⚠️ 퇴화~~ | ~~최솟값 클램프 없음~~ | ✅ 해결(2026-07-11) |
| ~~Point Light 감쇠~~ | ~~⚠️ 비표준~~ | ~~물리적 1/d² 미적용~~ | ✅ 해결(2026-07-11) |
| ~~Tonemapping~~ | ~~⚠️ 미구현~~ | ~~1.0 초과 클램핑 발생~~ | ✅ 해결(2026-07-11) |
| ~~IBL~~ | ~~⚠️ 미구현~~ | ~~상수 ambient 대체~~ | ✅ 해결(2026-07-11) — Diffuse Irradiance(Stage A) + Specular Prefiltered Map/BRDF LUT(Stage B) 모두 완료. Deferred·Forward(Transparent) 경로 전부 적용 |
| ~~G_Smith IBL 리매핑~~ | ~~⚠️ 미준비~~ | ~~IBL 추가 전 분리 필요~~ | ✅ 해결(2026-07-11) |

---

## 7. 코드 리뷰 발견사항

### ~~[주의] G_Smith — IBL 전환 시 k 리매핑 변경 필요~~ → ✅ 해결(2026-07-11)

**위치**: `Engine/_Shaders/Common/Lighting.hlsli` — `G_Smith()`

```hlsl
float r = roughness + 1.0f;
float k = (r * r) / 8.0f;   // 직접 조명(Direct Lighting) 전용
```

IBL(Image-Based Lighting) Specular 항에는 `k = roughness * roughness / 2.0f`를 써야 한다. Epic Games의 리얼타임 PBR 논문(Brian Karis, 2013)에서 직접 조명과 IBL을 구분한 이유: 직접 조명의 경우 핫스팟이 두드러지므로 k를 줄여 G 값을 높인다.

Phase 7(IBL 구현) 전에 `G_SmithDirect()`와 `G_SmithIBL()` 로 분리할 것을 권장.

**[2026-07-11 해결]** `G_Smith(k)` 코어 + `G_Smith_Direct(roughness)` 래퍼로 분리 완료(권장 명칭과 동일한 역할 분담). `G_SmithIBL` 대응 래퍼는 Specular IBL(Stage B) 착수 시 함께 추가.

---

### [주의] ambient 상수 — IBL 미구현 임시값

**위치**: `DeferredLighting.hlsl`, `ForwardOpaque.hlsl`, `AnimationDemo.hlsl`, `Transparent.hlsli`

```hlsl
float3 ambient = float3(0.10f, 0.10f, 0.10f) * albedo * ao;
```

물리적으로 올바른 ambient는 IBL(Diffuse Irradiance Map 또는 Spherical Harmonics)에서 나온다. 상수 ambient는 방향·거리 독립적이어서 물리적으로 틀리나, IBL 미구현 동안 장면이 완전히 어두워지는 것을 방지하는 임시값. 값(0.10)은 IBL 도입 전까지 임의 조정 가능.

**[2026-07-11 부분 해결]** `DeferredLighting.hlsl`만 Diffuse Irradiance(Stage A) 기반 `ambient = kD * albedo * irradiance * ao`로 교체(스카이박스 없을 때는 기존 flat 0.10 유지, `IBLEnabled` 플래그로 분기). `ForwardOpaque.hlsl`/`Transparent.hlsli`/`AnimationDemo.hlsl` 3곳은 아직 이 절의 상수 ambient 그대로 — Forward 경로 IBL 적용과 Specular IBL(Stage B)은 별도 작업으로 남음.

**[2026-07-11 완전 해결]** Specular Prefiltered Map + BRDF LUT(Stage B) 구현 완료(`IBLRenderer`), Transparent(Forward) 경로도 IBL 적용 완료. Deferred/Transparent 주요 경로는 flat ambient가 완전히 대체됨. (참고: 이 문단이 원래 지목한 `AnimationDemo.hlsl`은 이후 GBufferSkinned.hlsl 등으로 구조가 바뀌어 더 이상 존재하지 않음 — 별도 문서 정리 대상)

---

### [잠재적 결함] `RenderTransparent`의 `viewProjBuffer` 이중 업로드

**위치**: `Engine/Private/Render/ForwardRenderer.cpp`

`Render()`(불투명)와 `RenderTransparent()`(투명)가 각각 `viewProjBuffer->Upload()`를 호출한다. 같은 프레임에 두 함수가 모두 호출될 경우 동일 데이터를 두 번 업로드. 데이터 정합성은 문제없지만 업로드 비용이 중복된다.

현재 데이터 크기(64B)가 작아 실질적 영향은 무시할 수준. 오브젝트 수가 많아지거나 카메라 버퍼가 커질 경우 `Upload()` 호출 여부를 플래그로 제어하는 것을 고려.

---

### [정보] RT2 포맷 — 채널 낭비

**위치**: `Engine/Private/RHI/DirectX/Buffer/ColorBuffer_DirectX.cpp`

```cpp
// RT2: R8G8B8A8_UNORM — metallic(R)만 사용, GBA 예약
```

Metallic만 쓴다면 `R8_UNORM`으로도 충분. 다만 G-Buffer RT 포맷을 통일하면 RTV Heap 관리가 단순해지고, 향후 RT2에 AO 또는 emissive 마스크를 추가할 여지를 확보할 수 있다. 현재는 낭비지만 확장성 측면의 의도적 설계로 볼 수 있음.

---

## 8. 기술 부채 및 향후 작업 우선순위

| 항목 | 중요도 | 현재 상태 |
|---|---|---|
| ~~IBL(Diffuse Irradiance + Specular LUT)~~ | 높음 | ✅ 해결(2026-07-11) — Stage A(Diffuse Irradiance) + Stage B(Specular Prefiltered Map + BRDF LUT) 모두 완료, Deferred·Forward(Transparent) 경로 전부 적용 |
| ~~Tonemapping (Reinhard / ACES)~~ | 높음 | ✅ 해결(2026-07-11) — ACES Filmic 적용 |
| ~~Normal Mapping (Phase 7)~~ | 중간 | ✅ 해결 — Deferred/Forward 양쪽 적용, `.matl` `normal_tex=` 파싱 배선 완료 |
| ~~G_Smith IBL 리매핑 분리~~ | 중간 | ✅ 해결(2026-07-11) |
| OIT (Order-Independent Transparency) | 낮음 | ResolveOIT stub 보존. 현재 2-Pass로 볼록 오브젝트 대응 — 변경 없음 |
| ~~WorldAnimatedModel Material 지원~~ | 낮음 | ✅ 이미 구현되어 있었음이 확인됨(2026-07-11) — `IMaterial` 소유, `.matl`의 `diffuse_tex=`/`normal_tex=` 파싱해 Albedo·Normal Map을 GBufferSkinned에 바인딩 중 |

---

## 9. 전체 마이그레이션 평가

### PBR 핵심 수식 정확도

렌더링 방정식 `Lo = Le + Σ fr · Li · NdotL`의 직접 조명 이산 근사를 기준으로 항목별 검증한 결과 (섹션 5~6 상세 분석 근거):

**[2026-07-11 갱신]** 아래 표는 2026-07-06 작성 시점 평가. 이후 완료분은 취소선 + "현재" 열로 반영(삭제하지 않음).

| 항목 | 등급(작성 시점) | 근거 | 현재 |
|---|---|---|---|
| D_GGX 수식 | ✅ 정확 | α = r², α² 순서 정확. Trowbridge-Reitz 원형과 일치 | 변경 없음 |
| F_Schlick 수식 | ✅ 정확 | pow(1-VdotH, 5) 정확. VdotH clamp로 음수 pow 방지 | 변경 없음 |
| G_Smith (직접조명) | ✅ 정확 | k = (r+1)²/8 (Epic/Karis 2013) | 변경 없음 |
| EvalBRDF 렌더링 방정식 | ✅ 정확 | 분모 4·NdotV·NdotL, kD·albedo/π + spec, ×radiance×NdotL | 변경 없음 |
| 에너지 보존 | ✅ 정확 | kD = (1-F)·(1-metallic) | 변경 없음 |
| F0 비금속/금속 | ✅ 정확 | lerp(0.04, albedo, metallic) — metallicity workflow 표준 | 변경 없음 |
| ~~Emissive (Deferred)~~ | ~~❌ 누락~~ | ~~G-Buffer에 Emissive 슬롯 없음. Lo = sum + ambient만 출력~~ | ✅ 해결(2026-07-11, Pre-2) |
| ~~roughness = 0 처리~~ | ~~⚠️ 퇴화~~ | ~~a = a2 = 0 → spec = 0. 거울면 표현 불가~~ | ✅ 해결(2026-07-11, Pre-0) |
| ~~Point Light 감쇠~~ | ~~⚠️ 비표준~~ | ~~1/(a0+a1d+a2d²) — 물리적 역제곱 1/d² 미적용~~ | ✅ 해결(2026-07-11, Pre-1) |
| ~~Tonemapping~~ | ~~⚠️ 미구현~~ | ~~HDR → UNORM 단순 복사. 1.0 초과 값 클램핑~~ | ✅ 해결(2026-07-11, Phase 1-1) |
| ~~IBL (간접 조명)~~ | ~~⚠️ 미구현~~ | ~~ambient 상수 0.10으로 대체~~ | ✅ 해결(2026-07-11) — Stage A+B 모두 완료, Deferred·Forward(Transparent) 경로 전부 적용 |
| ~~G_Smith IBL 분리~~ | ~~⚠️ 미준비~~ | ~~IBL 추가 시 k = r²/2로 분리 필요~~ | ✅ 해결(2026-07-11) |

---

### 잘 된 점

- **계획 후 실행**: 7a59ab2에서 설계 결정을 문서화하고 검증한 뒤 660d924에서 실행. 설계-구현 분리가 명확.
- **인터페이스 유지**: `IMaterial`, `ILight` Public 인터페이스만 변경. 렌더러 내부 구현(LightsBufferData 레이아웃 등)이 외부로 노출되지 않음.
- **크래시 원인 근본 수정**: 레이아웃 불일치를 "조정"이 아닌 "제거"로 해결. `MaterialData`를 CB_Lights에서 분리하는 것이 근본 해법.
- **PBR 핵심 수식**: Cook-Torrance GGX 구현이 Epic/Karis 2013 레퍼런스와 수식 수준에서 일치. 에너지 보존, F0 처리, NdotL 적용 모두 정확.
- **Forward/Deferred 일관성**: Emissive를 제외하면 ForwardOpaque, Transparent, Animation 셰이더가 동일한 EvalBRDF를 호출. 조명 결과 일관.

---

### 개선 여지

1. ~~**❌ Deferred Emissive 복원** — G-Buffer RT2 예약 채널에 Emissive 팩킹 또는 별도 RT 추가. 정적 메시의 발광 소재가 완전히 무시되는 버그.~~
   **[2026-07-11 해결]** `GBuffer.hlsl`/`GBufferSkinned.hlsl` RT2.GBA 팩킹 + `DeferredLighting.hlsl` 복원.
2. ~~**⚠️ roughness 최솟값 클램프** — `float a = max(roughness * roughness, 0.0001f)` 한 줄로 거울면 퇴화 방지.~~
   **[2026-07-11 해결]** 권장 코드 그대로 적용.
3. ~~**⚠️ Tonemapping** — Blit 단계에 Reinhard 또는 ACES Filmic 추가. HDR 하이라이트 표현 정확도 개선.~~
   **[2026-07-11 해결]** ACES Filmic + Exposure 적용, Transparent/Blit 순서 회귀도 함께 수정.
4. ~~**⚠️ IBL (Indirect Lighting)** — 상수 ambient를 환경 큐브맵 + 사전계산 LUT(DFG)로 대체. 재질 roughness·metallic이 ambient에 반영되지 않는 한계 해소.~~
   **[2026-07-11 부분 해결]** Diffuse Irradiance(Stage A)를 Deferred 경로에 적용 완료(태양 이중 계산 버그 수정 포함). **남은 범위**: Forward 경로(Opaque/Transparent/Animation) IBL 미적용, Specular Prefiltered Map + BRDF LUT(Stage B) 미착수.
   **[2026-07-11 완전 해결]** Specular Prefiltered Map + BRDF LUT(Stage B) 구현 완료, Transparent(Forward) 경로 IBL 적용 완료.
5. ~~**⚠️ G_Smith IBL k 분리** — IBL 추가 전에 `k_direct` / `k_ibl` 경로를 함수 파라미터로 분리.~~
   **[2026-07-11 해결]** `G_Smith(k)` / `G_Smith_Direct(roughness)` 분리 완료.
6. ~~**⚠️ Point Light 물리 감쇠** — Frostbite 스타일 `1/max(d², ε) × windowing` 전환으로 에너지 보존 준수.~~
   **[2026-07-11 해결]** `CalcDistanceAttenuation` 공용 함수로 교체(Point+Spot 공통).
