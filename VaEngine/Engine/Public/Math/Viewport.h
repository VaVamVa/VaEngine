#pragma once
#include "Math/Container.h"
#include <cstdint>

namespace Viewport
{
    // 월드 좌표 → 화면 픽셀 좌표.
    // 카메라 뒤(w ≤ 0)이면 false 반환.
    bool WorldToScreen(const Vector3& worldPos,
                       const Matrix4x4& view, const Matrix4x4& proj,
                       uint32_t screenW, uint32_t screenH,
                       Vector2& outScreenPos);

    // 화면 픽셀 → 월드 레이.
    // outOrigin : Near plane 위의 월드 좌표
    // outDir    : 정규화된 레이 방향 (Near → Far)
    void ScreenToRay(float screenX, float screenY,
                     const Matrix4x4& view, const Matrix4x4& proj,
                     uint32_t screenW, uint32_t screenH,
                     Vector3& outOrigin, Vector3& outDir);
}
