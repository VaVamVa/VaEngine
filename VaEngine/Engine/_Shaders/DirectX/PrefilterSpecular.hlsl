#include "../Common/ImportanceSampling.hlsli"

// 원본 파노라마(equirect) — Stage A의 gSourceHDR과 동일한 소스, sky든 향후 Light Probe든 무관
Texture2D<float4>   gSourceHDR   : register(t0);
// 출력 — 이번 Dispatch가 담당하는 밉 레벨 하나(호출부가 밉마다 별도 Dispatch)
RWTexture2D<float4> gPrefiltered : register(u0);

cbuffer CB_PrefilterParams : register(b0)
{
    float gRoughness;  // 이 밉 레벨에 대응하는 roughness (0~1)
    float3 _pad;
};

static const uint kSampleCount = 256;  // 1회성 프리컴퓨트 — 실시간 비용 아님

// Stage A(IrradianceConvolve.hlsl)에서 확인된 것과 동일한 문제 — 이 씬은 태양을 표현하는
// Directional Light를 이미 갖고 있어, HDRI 원본의 태양 raw 값을 클램프 없이 그대로 컨볼루션에
// 태우면 반사 방향이 태양과 겹치는 지점에서 Directional Light의 스펙큘러 하이라이트와 중복으로
// 태양 에너지를 한 번 더 받는다(roughness가 낮을수록 GGX 로브가 좁아 거의 거울처럼 태양이
// 그대로 찍혀 문제가 더 뚜렷함). Stage A와 동일한 원리로 원본 샘플 자체를 상한선으로 제한한다.
static const float kMaxSampleContribution = 16.0f;

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    gPrefiltered.GetDimensions(width, height);
    if (id.x >= width || id.y >= height)
        return;

    float2 uv = float2((id.x + 0.5f) / width, (id.y + 0.5f) / height);
    float3 N  = normalize(EquirectUVToDir(uv));
    float3 V  = N;  // Epic 근사: 프리필터 단계는 V=N=R로 가정 (split-sum의 첫 번째 적분)

    float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);
    float  totalWeight      = 0.0f;

    for (uint i = 0; i < kSampleCount; ++i)
    {
        float2 Xi = Hammersley(i, kSampleCount);
        float3 H  = ImportanceSampleGGX(Xi, N, gRoughness);
        float3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = dot(N, L);
        if (NdotL > 0.0f)
        {
            float2 sampleUV    = DirToEquirectUV(L);
            float3 sampleColor = gSourceHDR.SampleLevel(LinearSampler, sampleUV, 0).rgb;
            sampleColor        = min(sampleColor, kMaxSampleContribution.xxx);
            prefilteredColor  += sampleColor * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor = (totalWeight > 0.0f) ? (prefilteredColor / totalWeight) : prefilteredColor;
    gPrefiltered[id.xy] = float4(prefilteredColor, 1.0f);
}
