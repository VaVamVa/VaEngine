//Deprecated: This shader is no longer used. It was replaced by a more efficient implementation that uses a single draw call for all characters, reducing CPU overhead and improving performance.
// Glyph rendering is now handled in a more optimized way, allowing for better batching and less state changes, which significantly enhances the rendering performance of debug text in the engine.

#include "../Common/Global.hlsli"

struct VS_INPUT
{
    float3 pos   : POSITION;
    float2 uv    : TEXCOORD;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD;
    float4 color : COLOR;
};

Texture2D    gAtlas : register(t0);
SamplerState gSampler : register(s0);

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    
    // UI 좌표계 (픽셀 단위) -> NDC 변환
    // x: [0, 1280] -> [-1, 1]
    // y: [0, 720] -> [1, -1] (Y-down to Y-up flip)
    output.pos.x = (input.pos.x / 1280.0f) * 2.0f - 1.0f;
    output.pos.y = -((input.pos.y / 720.0f) * 2.0f - 1.0f);
    output.pos.z = 0.0f;
    output.pos.w = 1.0f;
    
    output.uv = input.uv;
    output.color = input.color;
    
    return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    float alpha = gAtlas.Sample(gSampler, input.uv).r;
    return float4(input.color.rgb, input.color.a * alpha);
}
