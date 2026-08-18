#include "../Common/Lighting.hlsli"

#pragma pack_matrix(row_major)

// b0 — Deferred 전용 카메라 상수
cbuffer CB_DeferredCamera : register(b0)
{
    float4x4 InvViewProj;
    float3   EyePos;
    float    _pad;
    uint     ScreenW;
    uint     ScreenH;
    uint     SSAOEnabled;    // 0/1 — 런타임 디버그 토글(비교분석용). false면 gSSAO를 아예 샘플링하지 않음
    uint     ShowCascades;   // 0/1 — CSM 캐스케이드 색상 오버레이 토글(구 _pad2 슬롯 재사용)
    float3   CameraForward;  // 캐스케이드 선택용 뷰 스페이스 깊이 계산 — CB_ShadowLightVP의 split distance와
    float    _pad3;          // 같은 기준(카메라 forward축 거리)을 쓰기 위해 유클리드 거리 대신 사용
};

// b2 — CB_Lights (Lighting.hlsli)

// b1 — Shadow Map 광원 공간 행렬(CSM, 캐스케이드별. 기존 b0/b2 root index 불변). 항상 MAX_CASCADE_COUNT(8)분
// 크기로 고정 — 실제 사용 개수는 gActiveCascadeCount로 런타임 지정(Mesh LOD와 동일 패턴, Q&A Q7 참조).
#define MAX_CASCADE_COUNT 8
cbuffer CB_ShadowLightVP : register(b1)
{
    float4x4 gLightViewProj[MAX_CASCADE_COUNT];
    // 캐스케이드 경계(최대 7개, camera 거리 기준). float4[2]+이중 동적 인덱싱(gSplitDistances[i/4][i%4])을
    // 썼다가 캐스케이드 6개 이상(두 번째 float4 원소를 처음 건드리는 시점)부터 그림자가 갈라지는 버그가
    // 발생 — "배열 인덱스 + 벡터 컴포넌트 인덱스"를 동시에 런타임 값으로 계산하는 이중 동적 인덱싱이
    // 컴파일러에서 불안정하게 처리된 것으로 판단, 평범한 스칼라 배열 float[7]로 교체했었음(260713-CompactLog#7).
    // 그런데 실제로는 스칼라 배열이 "원소당 16바이트"가 아니라 레지스터 하나에 4개씩 빽빽하게 패킹되도록
    // 컴파일되어(260713-CompactLog#9 진단으로 확인 — C++은 원소당 16바이트로 패딩해서 올렸는데 GPU가 읽는
    // activeCascadeCount가 항상 0이었음), 배열 뒤에 오는 필드 전체의 오프셋이 C++ 구조체와 어긋나는
    // 새 버그가 생겼다. float4 배열(원소당 16바이트가 타입으로 보장됨) + 단일 인덱스(.x만 사용)로 교체해
    // "원소당 16바이트"를 컴파일러 해석에 기대지 않고 타입 자체로 강제한다.
    float4   gSplitDistances[MAX_CASCADE_COUNT - 1];  // .x만 사용, [1..3]은 패딩(C++ float[7][4]와 정확히 대응)
    float    gShadowMapTexelSize;
    float    gBlendWidth;          // 캐스케이드 전환 smooth blend 폭(카메라 거리 단위)
    uint     gActiveCascadeCount;  // 실제 사용할 캐스케이드 개수(1~MAX_CASCADE_COUNT)
    float    _shadowPad;
};

float GetSplitDistance(uint i)
{
    return gSplitDistances[i].x;
}

// G-Buffer SRV
Texture2D<float4>      gAlbedoAO    : register(t0);  // RT0: Albedo(RGB) + AO(A)
Texture2D<float4>      gNormalRough : register(t1);  // RT1: Normal(XYZ) + Roughness(W)
Texture2D<float4>      gMaterialBuf : register(t2);  // RT2: Metallic(R) + Emissive(GBA)
Texture2D<float>       gDepth       : register(t3);  // Depth
Texture2DArray<float>  gShadowMap   : register(t4);  // CSM — 슬라이스당 캐스케이드 1개
Texture2D<float4>      gIrradiance  : register(t5);  // Diffuse Irradiance (IBL Stage A)
Texture2D<float4>      gPrefiltered : register(t6);  // Specular Prefiltered Map (IBL Stage B)
Texture2D<float2>      gBRDFLUT     : register(t7);  // BRDF LUT (IBL Stage B)
Texture2D<float>       gSSAO        : register(t8);  // Blurred SSAO (append, 신규) — ambient에만 곱함

RWTexture2D<float4> outHDR : register(u0);

// cascadeIndex 슬라이스로 3×3 PCF. 범위 밖이면 1.0(완전히 밝음)
float SampleCascade(uint cascadeIndex, float3 worldPos)
{
    float4 lightClip = mul(float4(worldPos, 1.0f), gLightViewProj[cascadeIndex]);
    float2 shadowUV  = lightClip.xy * 0.5f + 0.5f;
    shadowUV.y       = 1.0f - shadowUV.y;  // NDC Y-up → UV Y-down
    float currentDepth = lightClip.z;

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f || shadowUV.y < 0.0f || shadowUV.y > 1.0f || currentDepth > 1.0f)
        return 1.0f; 

    float shadow = 0.0f;
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * gShadowMapTexelSize;
            shadow += gShadowMap.SampleCmpLevelZero(ShadowSampler, float3(shadowUV + offset, cascadeIndex), currentDepth);
        }
    }
    return shadow / 9.0f;
}

// camDist(카메라로부터의 거리)로 캐스케이드 인덱스를 고른다. 디버그 오버레이(색상)에도 재사용.
// 활성 캐스케이드 개수만큼만 순회(gActiveCascadeCount, 런타임 값) — [loop]는 데이터 종속 반복 횟수용.
uint SelectCascade(float camDist)
{
    [loop]
    for (uint i = 0; i < gActiveCascadeCount - 1; ++i)
    {
        if (camDist < GetSplitDistance(i))
            return i;
    }
    return gActiveCascadeCount - 1;
}

// 경계 근방이면 다음 캐스케이드와 smooth blend
float CalcShadowFactor(float3 worldPos, float camDist, uint cascade)
{
    float shadow = SampleCascade(cascade, worldPos);

    if (cascade < gActiveCascadeCount - 1)
    {
        float splitDist   = GetSplitDistance(cascade);
        float blendFactor = saturate((splitDist - camDist) / max(gBlendWidth, 0.0001f));
        if (blendFactor < 1.0f)
        {
            float nextShadow = SampleCascade(cascade + 1, worldPos);
            shadow = lerp(nextShadow, shadow, blendFactor);
        }
    }
    return shadow;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= ScreenW || id.y >= ScreenH)
        return;

    // --- 1. G-Buffer 읽기 ---
    float  depth       = gDepth[id.xy];
    float4 albedoAO    = gAlbedoAO[id.xy];
    float4 normalRough = gNormalRough[id.xy];
    float4 materialBuf = gMaterialBuf[id.xy];
    float  metallic    = materialBuf.r;
    float3 emissive    = materialBuf.gba;

    // depth == 1.0 → Sky 영역 (SkyPass가 이미 기록 완료)
    if (depth >= 1.0f)
        return;

    float3 albedo    = albedoAO.rgb;
    float  ao        = albedoAO.a;
    float3 N         = normalize(normalRough.xyz);
    float  roughness = normalRough.w;

    // --- 2. Depth → World Position 역투영 ---
    float2 uv       = (float2(id.xy) + 0.5f) / float2(ScreenW, ScreenH);
    float4 ndcPos   = float4(uv * 2.0f - 1.0f, depth, 1.0f);
    ndcPos.y        = -ndcPos.y;
    float4 wPos     = mul(ndcPos, InvViewProj);
    float3 worldPos = wPos.xyz / wPos.w;

    float3 V = normalize(EyePos - worldPos);

    // --- 3. Cook-Torrance GGX 조명 누적 ---
    float3 Lo = float3(0.0f, 0.0f, 0.0f);

    // cascade: 아래 캐스케이드 디버그 오버레이(ShowCascades)에서도 재사용하기 위해 함수 스코프로 유지.
    // camDist는 유클리드 거리가 아니라 카메라 forward축 기준 뷰 스페이스 깊이 — ComputeCascades(C++)가
    // 캐스케이드 박스를 이 기준으로 분할·피팅했으므로 선택 기준도 반드시 같아야 한다(불일치 시 화면
    // 가장자리·오블리크 각도에서 픽셀이 엉뚱한 캐스케이드에 걸려 그림자 누락/유령 그림자 발생).
    float camDist = dot(worldPos - EyePos, CameraForward);
    uint  cascade = SelectCascade(camDist);

    // Directional Light (Shadow Map 적용)
    {
        float3 L        = normalize(-gDirLight.direction);
        float3 radiance = gDirLight.color * gDirLight.intensity;
        float  shadow   = CalcShadowFactor(worldPos, camDist, cascade);
        Lo += EvalBRDF(N, V, L, albedo, roughness, metallic, radiance) * shadow;
    }

    // Point Lights
    for (int i = 0; i < gNumPointLights; ++i)
    {
        float3 toLight = gPointLights[i].position - worldPos;
        float  dist    = length(toLight);
        if (dist >= gPointLights[i].range) continue;

        float3 L        = toLight / dist;
        float  att      = CalcDistanceAttenuation(dist, gPointLights[i].range);
        float3 radiance = gPointLights[i].color * gPointLights[i].intensity * att;
        Lo += EvalBRDF(N, V, L, albedo, roughness, metallic, radiance);
    }

    // Spot Lights
    for (int j = 0; j < gNumSpotLights; ++j)
    {
        float3 toLight = gSpotLights[j].position - worldPos;
        float  dist    = length(toLight);
        if (dist >= gSpotLights[j].range) continue;

        float3 L    = toLight / dist;
        float  spot = pow(max(dot(-L, gSpotLights[j].direction), 0.0f), gSpotLights[j].spot);
        float  att  = spot * CalcDistanceAttenuation(dist, gSpotLights[j].range);
        float3 radiance = gSpotLights[j].color * gSpotLights[j].intensity * att;
        Lo += EvalBRDF(N, V, L, albedo, roughness, metallic, radiance);
    }

    // Diffuse(Stage A) + Specular(Stage B) IBL — gIBLEnabled==0(스카이박스 없음)이면
    // EvalAmbientIBL 내부에서 flat ambient로 자동 대체된다.
    float3 ambient = EvalAmbientIBL(N, V, albedo, roughness, metallic, ao,
                                    gIrradiance, gPrefiltered, gBRDFLUT);

    // SSAO — 간접광(ambient/IBL)에만 적용. 직접광(Lo)은 Shadow Map이 이미 가시성을 담당하므로
    // 중복 차폐하지 않는다. SSAOEnabled==0이면 런타임 비교분석을 위해 완전히 우회.
    if (SSAOEnabled != 0)
        ambient *= gSSAO[id.xy];

    float3 finalColor = Lo + ambient + emissive;

    // CSM 디버그 오버레이 — 캐스케이드별로 색을 50% 섞어 split 경계·blend 구간을 육안 확인.
    // MAX_CASCADE_COUNT(8)개 팔레트 고정 — activeCascadeCount가 몇이든 인덱스 범위 안에 들어온다.
    if (ShowCascades != 0)
    {
        static const float3 kCascadeColors[MAX_CASCADE_COUNT] = {
            float3(1.0f, 0.2f, 0.2f),  // 0 빨강
            float3(0.2f, 1.0f, 0.2f),  // 1 초록
            float3(0.2f, 0.4f, 1.0f),  // 2 파랑
            float3(1.0f, 1.0f, 0.2f),  // 3 노랑
            float3(1.0f, 0.2f, 1.0f),  // 4 마젠타
            float3(0.2f, 1.0f, 1.0f),  // 5 시안
            float3(1.0f, 0.6f, 0.2f),  // 6 주황
            float3(0.6f, 0.2f, 1.0f),  // 7 보라
        };
        finalColor = lerp(finalColor, kCascadeColors[cascade], 0.5f);
    }

    outHDR[id.xy] = float4(finalColor, 1.0f);
}
