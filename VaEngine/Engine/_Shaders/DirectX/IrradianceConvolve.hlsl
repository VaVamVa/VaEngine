#include "../Common/Sampler.hlsli"

static const float PI = 3.14159265f;

Texture2D<float4>   gSourceHDR  : register(t0);  // 원본 파노라마(equirect) — sky든 향후 Light Probe든 무관
RWTexture2D<float4> gIrradiance : register(u0);  // 출력 — 저해상도 equirect Diffuse Irradiance

// Sky.hlsl과 동일한 LatLong 정방향 매핑 (방향 → UV) — 샘플 방향을 원본 HDRI에서 조회할 때 사용
float2 DirToEquirectUV(float3 dir)
{
    float theta = atan2(dir.z, dir.x);
    float phi   = asin(clamp(dir.y, -1.0f, 1.0f));
    return float2(theta / (2.0f * PI) + 0.5f, 0.5f - phi / PI);
}

// Sky.hlsl의 역변환(UV → 방향) — 출력 텍셀이 나타내는 법선 방향 N을 구할 때 사용
float3 EquirectUVToDir(float2 uv)
{
    float theta = (uv.x - 0.5f) * 2.0f * PI;
    float phi   = (0.5f - uv.y) * PI;
    float cosPhi = cos(phi);
    return float3(cosPhi * cos(theta), sin(phi), cosPhi * sin(theta));
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    gIrradiance.GetDimensions(width, height);
    if (id.x >= width || id.y >= height)
        return;

    float2 uv = float2((id.x + 0.5f) / width, (id.y + 0.5f) / height);
    float3 N  = normalize(EquirectUVToDir(uv));

    float3 up    = (abs(N.y) > 0.99f) ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    // 반구 코사인 가중 리만 합 (LearnOpenGL IBL 레퍼런스와 동일한 sampleDelta) — 출력 텍셀당 1회만
    // 계산되는 프리컴퓨트라 샘플 수(~15,800개/텍셀)는 실시간 비용이 아니다.
    //
    // 태양 성분 클램프: 이 씬은 이미 태양을 표현하는 Directional Light를 별도로 갖고 있다.
    // HDRI의 태양(또는 태양 주변 헤이즈) raw 값은 클램프 없이 그대로 적분하면 수백~수천에 달해,
    // 태양을 마주보는 면만 Directional Light와 중복으로 태양 에너지를 한 번 더 받는 것처럼
    // 극단적으로 밝아진다(실측: 태양 방향 면만 과다 발광, 반대쪽은 정상). Diffuse Irradiance는
    // "부드러운 하늘 반사광"만 담당해야 하므로, 컨볼루션에 들어가는 원본 샘플 자체를 상한선으로
    // 제한한다 — Directional Light의 직접광은 이 클램프와 무관하게 그대로 유지된다.
    const float kMaxSampleContribution = 10.0f;
    const float kMinSampleContribution = 0.1f;

    float3 irradiance   = float3(0.0f, 0.0f, 0.0f);
    float  sampleCount  = 0.0f;
    const float sampleDelta = 0.025f;

    for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
    {
        for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta)
        {
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec     = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

            float2 sampleUV     = DirToEquirectUV(normalize(sampleVec));
            float3 sampleColor  = gSourceHDR.SampleLevel(LinearSampler, sampleUV, 0).rgb;
            sampleColor         = min(sampleColor, kMaxSampleContribution.xxx);
            sampleColor         = max(sampleColor, kMinSampleContribution.xxx);
            irradiance += sampleColor * cos(theta) * sin(theta);
            sampleCount += 1.0f;
        }
    }

    irradiance = PI * irradiance / sampleCount;
    gIrradiance[id.xy] = float4(irradiance, 1.0f);
}
