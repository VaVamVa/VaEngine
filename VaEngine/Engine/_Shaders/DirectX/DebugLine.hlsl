#pragma pack_matrix(row_major)

cbuffer CB_ViewProj : register(b0)
{
    float4x4 gViewProj;
}

struct VS_INPUT
{
    float3 pos   : POSITION;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT o;
    o.pos   = mul(float4(input.pos, 1.0f), gViewProj);
    o.color = input.color;
    return o;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    return input.color;
}
