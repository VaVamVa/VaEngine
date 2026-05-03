#pragma once

#include <cmath>

// ============================================================
// MatrixUtil
//
// 열 우선(Column-Major) 4x4 행렬 유틸리티입니다.
//
// Vulkan/GLSL은 열 우선 행렬을 사용합니다.
// float[16] 배열에서 원소 위치: m[col * 4 + row]
//
//   시각적 배치:
//   [ m[0]  m[4]  m[8]  m[12] ]
//   [ m[1]  m[5]  m[9]  m[13] ]
//   [ m[2]  m[6]  m[10] m[14] ]
//   [ m[3]  m[7]  m[11] m[15] ]
// ============================================================

namespace MatUtil
{
    using Mat4 = float[16];

    // 단위 행렬 (Identity Matrix)
    // 변환 없음을 나타냅니다. DX의 XMMatrixIdentity()에 해당.
    inline void Identity(Mat4 m)
    {
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    // 행렬 곱셈: out = a * b (Column-Major)
    // 변환을 합성할 때 사용합니다. (예: MVP = Projection * View * Model)
    inline void Multiply(const Mat4 a, const Mat4 b, Mat4 out)
    {
        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0; row < 4; ++row)
            {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k)
                {
                    sum += a[k * 4 + row] * b[col * 4 + k];
                }
                out[col * 4 + row] = sum;
            }
        }
    }

    // Y축 회전 행렬 (라디안, Column-Major)
    // 오른손 좌표계 기준. 양수 각도는 반시계 방향.
    inline void RotateY(float rad, Mat4 m)
    {
        Identity(m);
        float c = std::cos(rad);
        float s = std::sin(rad);
        // col0: (c, 0, -s, 0) / col2: (s, 0, c, 0)
        m[0]  =  c;
        m[2]  = -s;
        m[8]  =  s;
        m[10] =  c;
    }

    // 원근 투영 행렬 (Column-Major, Vulkan NDC)
    //
    // Vulkan NDC: Y축이 아래 방향(+Y = Down), Z 범위 [0, 1]
    // OpenGL과 달리 m[5]에 음수 부호를 붙여 Y를 뒤집습니다.
    // (뷰포트 높이를 음수로 설정하는 방법과 동일한 효과)
    //
    // @param fovY   수직 시야각 (라디안). 예: π/4 = 45도
    // @param aspect 종횡비 (width / height)
    // @param nearZ  근거리 클리핑 평면 (양수)
    // @param farZ   원거리 클리핑 평면 (양수, nearZ < farZ)
    inline void Perspective(float fovY, float aspect, float nearZ, float farZ, Mat4 m)
    {
        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        float f = 1.0f / std::tan(fovY * 0.5f);
        m[0]  =  f / aspect;
        m[5]  = -f;                                         // Vulkan Y-flip
        m[10] =  farZ / (nearZ - farZ);
        m[11] = -1.0f;
        m[14] =  (nearZ * farZ) / (nearZ - farZ);
    }

    // 뷰 행렬 (LookAt, Column-Major)
    //
    // 카메라 위치(eye)에서 목표 지점(at)을 바라보는 뷰 변환 행렬.
    // DX의 XMMatrixLookAtRH()에 해당합니다.
    //
    // @param eyeX/Y/Z  카메라 위치
    // @param atX/Y/Z   카메라가 바라보는 지점
    // @param upX/Y/Z   카메라의 위쪽 방향 (보통 (0, 1, 0))
    inline void LookAt(
        float eyeX, float eyeY, float eyeZ,
        float atX,  float atY,  float atZ,
        float upX,  float upY,  float upZ,
        Mat4 m)
    {
        auto normalize = [](float x, float y, float z, float& ox, float& oy, float& oz)
        {
            float len = std::sqrt(x*x + y*y + z*z);
            ox = x / len; oy = y / len; oz = z / len;
        };
        auto dot = [](float ax, float ay, float az, float bx, float by, float bz)
        {
            return ax*bx + ay*by + az*bz;
        };

        // forward = normalize(at - eye)
        float fx = atX - eyeX, fy = atY - eyeY, fz = atZ - eyeZ;
        float fnx, fny, fnz;
        normalize(fx, fy, fz, fnx, fny, fnz);

        // right = normalize(forward × up)
        float rx = fny*upZ - fnz*upY;
        float ry = fnz*upX - fnx*upZ;
        float rz = fnx*upY - fny*upX;
        float rnx, rny, rnz;
        normalize(rx, ry, rz, rnx, rny, rnz);

        // up = right × forward (재정규화)
        float unx = rny*fnz - rnz*fny;
        float uny = rnz*fnx - rnx*fnz;
        float unz = rnx*fny - rny*fnx;

        for (int i = 0; i < 16; ++i) m[i] = 0.0f;
        m[0]  = rnx; m[1]  = unx; m[2]  = -fnx;
        m[4]  = rny; m[5]  = uny; m[6]  = -fny;
        m[8]  = rnz; m[9]  = unz; m[10] = -fnz;
        m[12] = -dot(rnx, rny, rnz, eyeX, eyeY, eyeZ);
        m[13] = -dot(unx, uny, unz, eyeX, eyeY, eyeZ);
        m[14] =  dot(fnx, fny, fnz, eyeX, eyeY, eyeZ);
        m[15] = 1.0f;
    }

} // namespace MatUtil
