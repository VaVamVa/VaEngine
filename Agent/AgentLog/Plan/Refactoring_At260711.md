# 리팩터링 트래킹 (2026-07-11)

`Plan_RenderQuality.md` 작업 과정에서 발견되었지만, 그 자체는 렌더링 품질이 아니라 **엔진 인프라/구조 정리**에 해당하는 항목을 모아 별도로 추적한다. 각 항목은 어떤 조사에서 왜 발견됐는지 `Plan_RenderQuality.md`의 관련 섹션을 역참조하고, "왜 지금 하지 않는지" 근거를 명시한다.

**진행 기록 규칙**: 완료된 항목은 삭제하지 않고 `~~취소선~~`으로 감싼 뒤, 바로 아래에 해결 내용을 기술한다.

---

## 항목 1. Vulkan `EPixelFormat` ↔ `VkFormat` 매핑 부재

- **출처**: `Plan_RenderQuality.md` Pre-3 조사.
- **현재 상태**: `EPixelFormat`(`Common_RHI.h`)이 DXGI_FORMAT 숫자를 그대로 미러링하는 설계라 DX12에서는 동작하지만, Vulkan 백엔드(`Engine/Private/RHI/Vulkan/Common_Vulkan.h`, 현재 스텁)가 실제 구현될 때는 `EPixelFormat → VkFormat` 매핑 테이블이 별도로 필요하다.
- **왜 지금 안 하는지**: Vulkan 백엔드 자체가 스텁 상태(ExecAndroid 실제 렌더링 미착수)라 지금 매핑 테이블을 만들어도 검증할 방법이 없고 실질적 영향이 없다.
- **처리 시점**: Vulkan 렌더러(ExecAndroid Forward 경로) 착수 시점.

## 항목 2. `EBindingType::Sampler` 범용화

- **출처**: `Plan_RenderQuality.md` §0-4, Phase 1-3.
- **현재 상태**: `BindingLayout_DirectX.cpp::Create`의 switch문에 `EBindingType::Sampler` 케이스가 없어 `default: throw`(사실상 미구현 enum 값). Shadow PCF는 s2 비교 샘플러를 `hasTexture` 분기에 하드코딩 추가하는 최소 구현으로 대응(Phase 1-3에 포함)했으나, PSO마다 임의의 샘플러(필터 모드, 비교 함수, Wrap/Clamp 등)를 선언 가능하게 하는 일반화는 별도 작업이다.
- **왜 지금 안 하는지**: 현재 필요한 샘플러는 s0(linear-wrap) + s2(shadow comparison) 2종으로 고정 가능해, 일반화 없이도 Phase 1-3을 완료할 수 있다. 인터페이스를 일반화하려면 `BindingEntry`에 샘플러 파라미터(필터/비교함수/wrap모드)를 추가하고 PSO 서술자 전체를 다시 설계해야 해 범위가 커진다.
- **처리 시점**: 세 번째 커스텀 샘플러 요구가 생기는 시점(예: Anisotropic 옵션, PostProcess의 Clamp 샘플러 등).

## 항목 3. `.matl` 파서를 key-value 파서로 리팩터링

- **출처**: `Plan_RenderQuality.md` §0-2, Phase 1-2.
- **현재 상태**: `WorldModel.cpp::ParseDiffuseTex`가 `diffuse_tex=`만 읽는 임시 라인 스캔 방식. Phase 1-2에서 `normal_tex=`도 같은 라인스캔 패턴으로 최소 추가했으나, 향후 `specular_tex=`/`emissive_tex=`/`roughness_tex=` 등이 늘어나면 라인마다 유사 코드가 반복되어 중복이 커진다.
- **왜 지금 안 하는지**: 현재 필요한 키가 2개(diffuse, normal)뿐이라 일반 key-value 파서 도입의 이득이 크지 않다.
- **처리 시점**: 3번째 텍스처 슬롯(예: Roughness/Metallic Map 분리, AO Map)이 `.matl`에 추가되는 시점.

---

## 판단 기준

각 항목은 CLAUDE.md "기술 부채는 구현 전에 고지" 원칙에 따라 기록한다. 방치가 아니라 판단에 따른 유예이며, 처리 시점 조건이 실제로 도래하면 뒤로 미루지 않고 즉시 처리한다.
