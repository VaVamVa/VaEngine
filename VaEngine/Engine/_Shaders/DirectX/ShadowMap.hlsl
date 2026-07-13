#pragma pack_matrix(row_major)

// b0 — Directional Light 시점 view*proj (Orthographic)
cbuffer CB_ShadowViewProj : register(b0)
{
    float4x4 gLightViewProj;
};

struct VS_INPUT
{
    // slot 0 — per vertex (POSITION만 사용, 나머지 속성은 무시)
    float3 pos  : POSITION;
    // slot 1 — per instance: world matrix rows
    float4 row0 : INSTANCETRANSFORM0;
    float4 row1 : INSTANCETRANSFORM1;
    float4 row2 : INSTANCETRANSFORM2;
    float4 row3 : INSTANCETRANSFORM3;
};

float4 VSMain(VS_INPUT input) : SV_POSITION
{
    float4x4 world = float4x4(input.row0, input.row1, input.row2, input.row3);
    float4   wPos  = mul(float4(input.pos, 1.0f), world);
    return mul(wPos, gLightViewProj);
}

// Depth Only — Color 출력 없음. RHI가 그래픽스 PSO에 VS+PS 쌍을 요구하므로 빈 PS로 충족.
void PSMain()
{
}
