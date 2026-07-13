#include "../Common/ImportanceSampling.hlsli"

// 스카이박스와 무관 — NdotV·roughness에만 의존하는 순수 BRDF 적분(split-sum의 두 번째 항).
// 소스 텍스처 입력이 없고, IBLRenderer::Initialize()에서 1회만 계산된다.
RWTexture2D<float2> gBRDFLUT : register(u0);

static const uint kSampleCount = 1024;

float2 IntegrateBRDFTexel(float NdotV, float roughness)
{
    float3 V;
    V.x = sqrt(1.0f - NdotV * NdotV);
    V.y = 0.0f;
    V.z = NdotV;

    float A = 0.0f;
    float B = 0.0f;

    const float3 N = float3(0.0f, 0.0f, 1.0f);

    for (uint i = 0; i < kSampleCount; ++i)
    {
        float2 Xi = Hammersley(i, kSampleCount);
        float3 H  = ImportanceSampleGGX(Xi, N, roughness);
        float3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0f);
        float NdotH = max(H.z, 0.0f);
        float VdotH = max(dot(V, H), 0.0f);

        if (NdotL > 0.0f)
        {
            float G     = G_Smith_IBL(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 0.0001f);
            float Fc    = pow(1.0f - VdotH, 5.0f);

            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(kSampleCount);
    B /= float(kSampleCount);
    return float2(A, B);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width, height;
    gBRDFLUT.GetDimensions(width, height);
    if (id.x >= width || id.y >= height)
        return;

    float NdotV     = (id.x + 0.5f) / width;
    float roughness = (id.y + 0.5f) / height;

    gBRDFLUT[id.xy] = IntegrateBRDFTexel(NdotV, roughness);
}
