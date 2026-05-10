#ifndef GLOBAL_HLSLI
#define GLOBAL_HLSLI

#pragma pack_matrix(row_major)

// Per-frame constants (b0)
cbuffer CB_PerFrame : register(b0)
{
    float4x4 gView;
    float4x4 gProjection;
    float4x4 gViewProjection;
    float3   gCameraPos;
    float    _perFramePad;
};

// NOTE: 월드 행렬은 인스턴스 버퍼(slot 1)에서 직접 읽음. 별도 b1 cbuffer 사용 안 함.

// --- Vertex input layouts ---

struct VS_INPUT_POS_COLOR
{
    float3 pos   : POSITION;
    float4 color : COLOR;
};

struct VS_INPUT_POS_UV
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
};

struct VS_INPUT_POS_COLOR_UV
{
    float3 pos   : POSITION;
    float4 color : COLOR;
    float2 uv    : TEXCOORD;
};

struct VS_INPUT_PNT
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float2 uv     : TEXCOORD;
};

struct VS_INPUT_PNTB
{
    float3 pos      : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
    float3 tangent  : TANGENT;
    float3 binormal : BINORMAL;
};

// --- Pixel shader input structs ---

struct PS_INPUT_COLOR
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR;
};

struct PS_INPUT_UV
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

struct PS_INPUT_COLOR_UV
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR;
    float2 uv    : TEXCOORD;
};

struct PS_INPUT_PNT
{
    float4 pos    : SV_POSITION;
    float3 wPos   : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv     : TEXCOORD2;
};

struct PS_INPUT_PNTB
{
    float4 pos      : SV_POSITION;
    float3 wPos     : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float3 tangent  : TEXCOORD3;
    float3 binormal : TEXCOORD4;
};

// --- Utility ---

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    float3 normalT = 2.0f * normalMapSample - 1.0f;
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);
    return mul(normalT, TBN);
}

#endif // GLOBAL_HLSLI
