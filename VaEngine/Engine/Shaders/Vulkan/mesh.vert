#version 450
// ============================================================
// Vertex Shader: mesh.vert
//
// 각 정점에 대해 실행됩니다.
// MVP(Model-View-Projection) 행렬로 3D 좌표를 화면 좌표로 변환합니다.
//
// DX12의 HLSL:
//   cbuffer MatrixBuffer : register(b0) { float4x4 mvp; };
//   float4 main(float3 pos : POSITION) : SV_Position { return mul(mvp, float4(pos, 1)); }
// ============================================================

// ---- 입력 (Vertex Attributes) ----
// location=0: 정점 위치 (x, y, z)
layout(location = 0) in vec3 inPosition;
// location=1: 정점 색상 (r, g, b)
layout(location = 1) in vec3 inColor;

// ---- 출력 (Fragment Shader로 전달) ----
layout(location = 0) out vec3 fragColor;

// ---- Uniform Buffer (set=0, binding=0) ----
// Descriptor Set을 통해 CPU에서 매 프레임 MVP 행렬을 전달합니다.
// DX12의 CBV(Constant Buffer View)에 해당합니다.
layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 model;      // 모델 변환 (회전, 이동, 스케일)
    mat4 view;       // 카메라 변환
    mat4 projection; // 투영 변환 (원근감)
} ubo;

void main()
{
    // 클립 공간 좌표 = Projection × View × Model × 정점 위치
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor   = inColor;
}
