#pragma pack_matrix(row_major)

cbuffer CB_Transform : register(b0)
{
    float4x4 gMVP;
};

Texture2D    gDiffuse : register(t0);
SamplerState gSampler : register(s0);

struct VS_INPUT
{
    float3 pos   : POSITION;
    float4 color : COLOR;
    float2 uv    : TEXCOORD;
};

struct PS_INPUT
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR;
    float2 uv    : TEXCOORD;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    output.pos   = mul(float4(input.pos, 1.0f), gMVP);
    output.color = input.color;
    output.uv    = input.uv;
    return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    return gDiffuse.Sample(gSampler, input.uv) * input.color;
}
