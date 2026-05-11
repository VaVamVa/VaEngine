#pragma pack_matrix(row_major)

cbuffer CB_DebugText : register(b0)
{
    float screenWidth;
    float screenHeight;
    float2 _pad;
};

Texture2D    gAtlas   : register(t0);
SamplerState gSampler : register(s0);

struct VS_INPUT
{
    float2 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR;
};

PS_INPUT VSMain(VS_INPUT input)
{
    // 화면 픽셀 좌표 → NDC (좌상 원점, Y 아래 증가 → NDC Y 반전)
    float2 ndc;
    ndc.x =  (input.pos.x / screenWidth)  * 2.0f - 1.0f;
    ndc.y = -(input.pos.y / screenHeight) * 2.0f + 1.0f;

    PS_INPUT o;
    o.pos   = float4(ndc, 0.0f, 1.0f);
    o.uv    = input.uv;
    o.color = input.color;
    return o;
}

float4 PSMain(PS_INPUT input) : SV_Target
{
    float alpha = gAtlas.Sample(gSampler, input.uv).a;
    return float4(input.color.rgb, input.color.a * alpha);
}
