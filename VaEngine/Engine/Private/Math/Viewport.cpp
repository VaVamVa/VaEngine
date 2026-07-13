#include "Math/Viewport.h"

namespace Viewport
{

// row-vector 규칙: result[j] = sum_i( input[i] * M.m[i][j] )
// 즉 [x y z w] * M 에서 결과 j번째 = x*M[0][j] + y*M[1][j] + z*M[2][j] + w*M[3][j]

bool WorldToScreen(const Vector3& worldPos,
                   const Matrix4x4& view, const Matrix4x4& proj,
                   uint32_t screenW, uint32_t screenH,
                   Vector2& outScreenPos)
{
    const Matrix4x4 vp = view * proj;
    const float x = worldPos.x, y = worldPos.y, z = worldPos.z;

    const float cx = x*vp.m[0][0] + y*vp.m[1][0] + z*vp.m[2][0] + vp.m[3][0];
    const float cy = x*vp.m[0][1] + y*vp.m[1][1] + z*vp.m[2][1] + vp.m[3][1];
    const float cw = x*vp.m[0][3] + y*vp.m[1][3] + z*vp.m[2][3] + vp.m[3][3];

    if (cw <= 0.0f)
        return false;  // 카메라 뒤

    const float invW = 1.0f / cw;
    const float ndcX =  cx * invW;
    const float ndcY =  cy * invW;

    // NDC [-1,1] → 화면 픽셀 (Y: NDC +1=상단, 화면 Y=0=상단 → 반전)
    outScreenPos.x = ( ndcX + 1.0f) * 0.5f * static_cast<float>(screenW);
    outScreenPos.y = (-ndcY + 1.0f) * 0.5f * static_cast<float>(screenH);
    return true;
}

void ScreenToRay(float screenX, float screenY,
                 const Matrix4x4& view, const Matrix4x4& proj,
                 uint32_t screenW, uint32_t screenH,
                 Vector3& outOrigin, Vector3& outDir)
{
    // 1. 화면 픽셀 → NDC (Y: 화면 Y-down → NDC Y-up 반전)
    const float ndcX =  2.0f * screenX / static_cast<float>(screenW) - 1.0f;
    const float ndcY = -2.0f * screenY / static_cast<float>(screenH) + 1.0f;

    // 2. inv(V * P) 계산 (내부에서 처리)
    const Matrix4x4 invVP = (view * proj).Inverse();

    // 3. NDC 점 → 월드 좌표 (동차 나누기 포함) — Container.h::UnprojectPoint 공용 구현 사용
    //    (ShadowMapRenderer::ComputeCascades의 프러스텀 코너 언프로젝션과 동일 연산, 중복 제거)
    // DX12 깊이 범위 [0, 1]: z=0 → Near plane, z=1 → Far plane
    const Vector3 nearWorld = UnprojectPoint({ ndcX, ndcY, 0.0f }, invVP);
    const Vector3 farWorld  = UnprojectPoint({ ndcX, ndcY, 1.0f }, invVP);

    outOrigin = nearWorld;
    outDir    = (farWorld - nearWorld).Normalized();
}

} // namespace Viewport
