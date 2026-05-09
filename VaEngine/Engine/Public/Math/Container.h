#pragma once

#include <cmath>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// Math  —  scalar constants & utilities  (constexpr, header-only)
// ─────────────────────────────────────────────────────────────────────────────
namespace Math
{
    inline constexpr float PI      = 3.14159265358979f;
    inline constexpr float EPSILON = 0.000001f;
    inline constexpr float DEG2RAD = PI / 180.0f;
    inline constexpr float RAD2DEG = 180.0f / PI;

    constexpr float ToRadian(float deg) { return deg * DEG2RAD; }
    constexpr float ToDegree(float rad) { return rad * RAD2DEG; }

    template<typename T>
    constexpr T Clamp(T val, T lo, T hi) { return val < lo ? lo : val > hi ? hi : val; }

    constexpr float Lerp(float a, float b, float t) { return a + (b - a) * t; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Vector2
// ─────────────────────────────────────────────────────────────────────────────
struct Vector2
{
    float x = 0.0f, y = 0.0f;

    constexpr Vector2() = default;
    constexpr Vector2(float x, float y) : x(x), y(y) {}

    Vector2  operator+ (const Vector2& o) const { return { x + o.x, y + o.y }; }
    Vector2  operator- (const Vector2& o) const { return { x - o.x, y - o.y }; }
    Vector2  operator* (float s)          const { return { x * s,   y * s   }; }
    Vector2  operator- ()                 const { return { -x, -y            }; }
    Vector2& operator+=(const Vector2& o)       { x += o.x; y += o.y; return *this; }
    Vector2& operator-=(const Vector2& o)       { x -= o.x; y -= o.y; return *this; }
    Vector2& operator*=(float s)                { x *= s;   y *= s;   return *this; }

    float   LengthSq() const { return x * x + y * y; }
    float   Dot(const Vector2& o) const { return x * o.x + y * o.y; }

    float   Length()     const;
    Vector2 Normalized() const;

    static const Vector2 Zero;
    static const Vector2 One;
    static const Vector2 UnitX;
    static const Vector2 UnitY;
};
inline Vector2 operator*(float s, const Vector2& v) { return v * s; }

// ─────────────────────────────────────────────────────────────────────────────
// Vector3
// ─────────────────────────────────────────────────────────────────────────────
struct Vector3
{
    float x = 0.0f, y = 0.0f, z = 0.0f;

    constexpr Vector3() = default;
    constexpr Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vector3  operator+ (const Vector3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vector3  operator- (const Vector3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vector3  operator* (float s)          const { return { x * s,   y * s,   z * s   }; }
    Vector3  operator/ (float s)          const { return { x / s,   y / s,   z / s   }; }
    Vector3  operator- ()                 const { return { -x, -y, -z                  }; }
    Vector3& operator+=(const Vector3& o)       { x += o.x; y += o.y; z += o.z; return *this; }
    Vector3& operator-=(const Vector3& o)       { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vector3& operator*=(float s)                { x *= s;   y *= s;   z *= s;   return *this; }
    Vector3& operator/=(float s)                { x /= s;   y /= s;   z /= s;   return *this; }

    float   LengthSq() const { return x * x + y * y + z * z; }
    float   Dot  (const Vector3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vector3 Cross(const Vector3& o) const
    {
        return { y * o.z - z * o.y,
                 z * o.x - x * o.z,
                 x * o.y - y * o.x };
    }

    float   Length()     const;
    Vector3 Normalized() const;

    // LH 좌표계 기저 벡터
    static const Vector3 Zero;
    static const Vector3 One;
    static const Vector3 Up;
    static const Vector3 Right;
    static const Vector3 Forward;  // LH: +Z 전방
};
inline Vector3 operator*(float s, const Vector3& v) { return v * s; }

// ─────────────────────────────────────────────────────────────────────────────
// Vector4
// ─────────────────────────────────────────────────────────────────────────────
struct Vector4
{
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

    constexpr Vector4() = default;
    constexpr Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    constexpr Vector4(const Vector3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}

    Vector3 XYZ() const { return { x, y, z }; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Quaternion  (xyzw 순서)
// ─────────────────────────────────────────────────────────────────────────────
struct Quaternion
{
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;

    constexpr Quaternion() = default;
    constexpr Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

    Quaternion operator*(const Quaternion& o) const;
    Quaternion Normalized() const;

    static constexpr Quaternion Identity() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }

    // yaw=Y축 / pitch=X축 / roll=Z축 (라디안), 적용 순서: Roll → Pitch → Yaw
    static Quaternion RotationAxis(const Vector3& axis, float radians);
    static Quaternion RotationYawPitchRoll(float yaw, float pitch, float roll);
};

// ─────────────────────────────────────────────────────────────────────────────
// Matrix4x4  (row-major, row-vector 규칙: v_out = v_in * M)
// ─────────────────────────────────────────────────────────────────────────────
struct Matrix4x4
{
    float m[4][4] = {};

    Matrix4x4() = default;

    // float[16] 호환 레이아웃에서 직접 생성
    static Matrix4x4 FromFloats(const float* src16);

    Matrix4x4  operator* (const Matrix4x4& o) const;
    Matrix4x4& operator*=(const Matrix4x4& o) { *this = *this * o; return *this; }

    Matrix4x4 Transposed() const;

    // TRS 분해 — Shepperd 방법 (row-major)
    struct Decomposed { Vector3 translation; Quaternion rotation; Vector3 scale; };
    Decomposed Decompose() const;

    // ── Static factory ────────────────────────────────────────────────────────
    static Matrix4x4 Identity();
    static Matrix4x4 Translation(const Vector3& t);
    static Matrix4x4 Scale(const Vector3& s);
    static Matrix4x4 RotationX(float rad);
    static Matrix4x4 RotationY(float rad);
    static Matrix4x4 RotationZ(float rad);
    static Matrix4x4 RotationQuat(const Quaternion& q);

    // LH LookAt  (DirectX: +Z 전방, 깊이 [0,1])
    static Matrix4x4 LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up);
    // RH LookAt  (Vulkan/OpenGL: -Z 전방)
    static Matrix4x4 LookAtRH(const Vector3& eye, const Vector3& target, const Vector3& up);

    // LH Perspective  (DirectX: 깊이 [0,1], +Y 상방)
    static Matrix4x4 PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ);
    // RH Perspective  (Vulkan/OpenGL: 깊이 [0,1], -Z 전방)
    // Vulkan +Y 하방 NDC 사용 시 m[1][1]을 부호 반전할 것
    static Matrix4x4 PerspectiveFovRH(float fovY, float aspect, float nearZ, float farZ);

    static Matrix4x4 Lerp(const Matrix4x4& a, const Matrix4x4& b, float t);
};

// ─────────────────────────────────────────────────────────────────────────────
// Free functions — 벡터-행렬 변환 (row-vector 규칙)
// ─────────────────────────────────────────────────────────────────────────────

// 점 변환 (w=1, 이동 포함)
Vector3 TransformPoint    (const Vector3& v, const Matrix4x4& m);
// 방향 변환 (w=0, 이동 제외)
Vector3 TransformDirection(const Vector3& v, const Matrix4x4& m);

// ─────────────────────────────────────────────────────────────────────────────
// DirectX 상호 변환
// DirectXMath.h가 이 헤더보다 먼저 include된 경우에만 활성화됨
// ─────────────────────────────────────────────────────────────────────────────
#ifdef DIRECTX_MATH_VERSION

inline DirectX::XMFLOAT2    ToXM(const Vector2&    v) { return { v.x, v.y }; }
inline DirectX::XMFLOAT3    ToXM(const Vector3&    v) { return { v.x, v.y, v.z }; }
inline DirectX::XMFLOAT4    ToXM(const Vector4&    v) { return { v.x, v.y, v.z, v.w }; }
inline DirectX::XMFLOAT4    ToXM(const Quaternion& q) { return { q.x, q.y, q.z, q.w }; }
inline DirectX::XMFLOAT4X4  ToXM(const Matrix4x4&  m)
{
    DirectX::XMFLOAT4X4 r;
    std::memcpy(&r, m.m, 64);
    return r;
}

inline Vector2    FromXM(const DirectX::XMFLOAT2&    v) { return { v.x, v.y }; }
inline Vector3    FromXM(const DirectX::XMFLOAT3&    v) { return { v.x, v.y, v.z }; }
inline Vector4    FromXM(const DirectX::XMFLOAT4&    v) { return { v.x, v.y, v.z, v.w }; }
inline Quaternion FromXMQuat(const DirectX::XMFLOAT4& v) { return { v.x, v.y, v.z, v.w }; }
inline Matrix4x4  FromXM(const DirectX::XMFLOAT4X4&  m)
{
    Matrix4x4 r;
    std::memcpy(r.m, &m, 64);
    return r;
}

#endif // DIRECTX_MATH_VERSION

// ─────────────────────────────────────────────────────────────────────────────
// GLM 상호 변환
// glm/glm.hpp 등이 먼저 include된 경우에만 활성화됨
// ─────────────────────────────────────────────────────────────────────────────
#ifdef GLM_VERSION

inline glm::vec2 ToGLM(const Vector2& v) { return { v.x, v.y }; }
inline glm::vec3 ToGLM(const Vector3& v) { return { v.x, v.y, v.z }; }
inline glm::vec4 ToGLM(const Vector4& v) { return { v.x, v.y, v.z, v.w }; }
inline glm::quat ToGLM(const Quaternion& q) { return glm::quat(q.w, q.x, q.y, q.z); }
inline glm::mat4 ToGLM(const Matrix4x4& m)
{
    // GLM은 column-major: 업로드 전 Transposed() 후 ToGLM 또는 glm::transpose 사용
    glm::mat4 r;
    std::memcpy(&r, m.m, 64);
    return r;
}

inline Vector2    FromGLM(const glm::vec2& v) { return { v.x, v.y }; }
inline Vector3    FromGLM(const glm::vec3& v) { return { v.x, v.y, v.z }; }
inline Vector4    FromGLM(const glm::vec4& v) { return { v.x, v.y, v.z, v.w }; }
inline Quaternion FromGLM(const glm::quat& q) { return { q.x, q.y, q.z, q.w }; }
inline Matrix4x4  FromGLM(const glm::mat4& m)
{
    Matrix4x4 r;
    std::memcpy(r.m, &m, 64);
    return r;
}

#endif // GLM_VERSION
