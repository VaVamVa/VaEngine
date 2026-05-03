#version 450
// ============================================================
// Fragment Shader: mesh.frag
//
// 각 픽셀에 대해 실행됩니다.
// Vertex Shader에서 보간된 색상을 그대로 출력합니다.
//
// DX12의 HLSL:
//   float4 main(float3 color : COLOR) : SV_Target { return float4(color, 1.0); }
// ============================================================

// ---- 입력 (Vertex Shader에서 보간된 값) ----
layout(location = 0) in vec3 fragColor;

// ---- 출력 (렌더 타겟 0번 = 스왑체인 이미지) ----
// DX12의 SV_Target 에 해당합니다.
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(fragColor, 1.0);
}
