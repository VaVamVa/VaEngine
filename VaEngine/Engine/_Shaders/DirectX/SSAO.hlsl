#include "../Common/Sampler.hlsli"

#pragma pack_matrix(row_major)

#define SSAO_KERNEL_SIZE 32

// b0 — 카메라/화면 정보 (매 프레임 업로드)
cbuffer CB_SSAOCamera : register(b0)
{
    float4x4 InvViewProj;
    float4x4 ViewProj;
    float3   EyePos;
    float    _pad;
    uint     ScreenW;
    uint     ScreenH;
    float2   _pad2;
};

// b1 — 커널 + 파라미터 (Initialize 시 1회 업로드)
cbuffer CB_SSAOParams : register(b1)
{
    float4 kernel[SSAO_KERNEL_SIZE];  // xyz = 접선 공간 샘플 오프셋(반경 이미 반영, 0~1 스케일)
    float  radius;                    // world-space, meter 단위 (1 unit = 1m 확정)
    float  bias;
    float  power;
    float  _paramPad;
};

Texture2D<float4> gNormalRough : register(t0);  // G-Buffer RT1 — WorldNormal(XYZ)
Texture2D<float>  gDepth       : register(t1);
Texture2D<float2> gNoise       : register(t2);  // 4x4 랜덤 회전 벡터 타일

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

// SV_VertexID 기반 전체화면 삼각형 (Blit.hlsl과 동일 컨벤션)
VS_OUT VSMain(uint vid : SV_VertexID)
{
    VS_OUT o;
    o.uv  = float2((vid << 1) & 2, vid & 2);
    o.pos = float4(o.uv.x * 2.0f - 1.0f, 1.0f - o.uv.y * 2.0f, 0.0f, 1.0f);
    return o;
}

// DeferredLighting.hlsl과 동일한 Depth → World Position 역투영
float3 ReconstructWorldPos(float2 uv, float depth)
{
    float4 ndcPos = float4(uv * 2.0f - 1.0f, depth, 1.0f);
    ndcPos.y      = -ndcPos.y;
    float4 wPos   = mul(ndcPos, InvViewProj);
    return wPos.xyz / wPos.w;
}

float PSMain(VS_OUT input) : SV_TARGET
{
    float depth = gDepth.Sample(PointSampler, input.uv);
    if (depth >= 1.0f)
        return 1.0f;  // Sky — AO 없음(완전히 밝음)

    float3 N        = normalize(gNormalRough.Sample(PointSampler, input.uv).xyz);
    float3 worldPos = ReconstructWorldPos(input.uv, depth);

    // 노이즈를 화면 전체에 4x4 텍셀 단위로 타일링 — SSAOBlur.hlsl의 4x4 박스 블러가
    // 이 주기 패턴을 정확히 상쇄한다.
    float2 noiseScale = float2(ScreenW, ScreenH) / 4.0f;
    float3 randomVec  = float3(gNoise.Sample(PointSampler, input.uv * noiseScale) * 2.0f - 1.0f, 0.0f);

    // 노이즈 기반 Gram-Schmidt — 매 픽셀 커널을 다르게 회전시켜야 블러로 밴딩이 사라진다
    // (IrradianceConvolve.hlsl의 up-vector 방식과 달리, 1회성 대량 샘플링이 아니라 실시간
    // 소량 샘플링이라 랜덤 회전이 필수).
    float3 tangent   = normalize(randomVec - N * dot(randomVec, N));
    float3 bitangent = cross(N, tangent);
    float3x3 TBN     = float3x3(tangent, bitangent, N);

    const float centerDist = length(EyePos - worldPos);

    float occlusion = 0.0f;
    [loop]
    for (int i = 0; i < SSAO_KERNEL_SIZE; ++i)
    {
        float3 sampleOffset = mul(kernel[i].xyz, TBN);
        float3 samplePos    = worldPos + sampleOffset * radius;

        float4 clip = mul(float4(samplePos, 1.0f), ViewProj);
        float2 ndc  = clip.xy / clip.w;
        float2 sampleUV = ndc * 0.5f + 0.5f;
        sampleUV.y = 1.0f - sampleUV.y;

        if (sampleUV.x < 0.0f || sampleUV.x > 1.0f || sampleUV.y < 0.0f || sampleUV.y > 1.0f)
            continue;

        const float  actualDepth     = gDepth.SampleLevel(PointSampler, sampleUV, 0);
        const float3 actualWorldPos  = ReconstructWorldPos(sampleUV, actualDepth);
        const float  actualDist      = length(EyePos - actualWorldPos);
        const float  sampleDist      = length(EyePos - samplePos);

        // world-space 거리 근사(이 엔진은 view-space Z를 별도로 갖지 않음 — 카메라 거리로 대체,
        // 커널 반경이 작아 시야각 가장자리에서도 오차가 실질적으로 무시할 수준).
        const float rangeCheck = smoothstep(0.0f, 1.0f, radius / max(abs(centerDist - actualDist), 0.0001f));
        occlusion += (actualDist <= sampleDist - bias ? 1.0f : 0.0f) * rangeCheck;
    }

    occlusion = 1.0f - (occlusion / float(SSAO_KERNEL_SIZE));
    return pow(saturate(occlusion), power);
}
