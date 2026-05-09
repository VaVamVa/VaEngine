#include "../Common/Lighting.hlsli"

#pragma pack_matrix(row_major)

// b0 — per-frame: view-projection
cbuffer CB_ViewProj : register(b0)
{
    float4x4 gViewProj;
};

// b2 — CB_Lights (Lighting.hlsli 에 정의)
// t0 — gDiffuse
Texture2D gDiffuse : register(t0);

struct VS_INPUT
{
    // slot 0 — per vertex
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float4 color  : COLOR;
    float2 uv     : TEXCOORD;
    // slot 1 — per instance: world matrix rows
    float4 row0   : INSTANCETRANSFORM0;
    float4 row1   : INSTANCETRANSFORM1;
    float4 row2   : INSTANCETRANSFORM2;
    float4 row3   : INSTANCETRANSFORM3;
};

struct PS_INPUT
{
    float4 pos    : SV_POSITION;
    float3 wPos   : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 color  : TEXCOORD2;
    float2 uv     : TEXCOORD3;
};

PS_INPUT VSMain(VS_INPUT input)
{
    float4x4 world = float4x4(input.row0, input.row1, input.row2, input.row3);
    float4x4 mvp   = mul(world, gViewProj);

    PS_INPUT o;
    o.pos    = mul(float4(input.pos, 1.0f), mvp);
    o.wPos   = mul(float4(input.pos, 1.0f), world).xyz;
    o.normal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
    o.color  = input.color;
    o.uv     = input.uv;
    return o;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    float3 toEye  = normalize(gEyePosW - input.wPos);

    float4 ambient = float4(0, 0, 0, 0);
    float4 diffuse = float4(0, 0, 0, 0);
    float4 spec    = float4(0, 0, 0, 0);
    float4 A, D, S;

    ComputeDirectionalLight(gMaterial, gDirLight, normal, toEye, A, D, S);
    ambient += A; diffuse += D; spec += S;

    for (int i = 0; i < gNumPointLights; ++i)
    {
        ComputePointLight(gMaterial, gPointLights[i], input.wPos, normal, toEye, A, D, S);
        ambient += A; diffuse += D; spec += S;
    }

    for (int j = 0; j < gNumSpotLights; ++j)
    {
        ComputeSpotLight(gMaterial, gSpotLights[j], input.wPos, normal, toEye, A, D, S);
        ambient += A; diffuse += D; spec += S;
    }

    float4 texColor = gDiffuse.Sample(LinearSampler, input.uv) * input.color;
    float4 litColor = texColor * (ambient + diffuse) + spec;
    litColor.a = texColor.a * gMaterial.diffuse.a;
    return litColor;
}
