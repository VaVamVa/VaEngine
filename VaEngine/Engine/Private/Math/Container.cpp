#include "Math/Container.h"

#include <cmath>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// Vector2 — static const 정의
// ─────────────────────────────────────────────────────────────────────────────
const Vector2 Vector2::Zero  = { 0.0f, 0.0f };
const Vector2 Vector2::One   = { 1.0f, 1.0f };
const Vector2 Vector2::UnitX = { 1.0f, 0.0f };
const Vector2 Vector2::UnitY = { 0.0f, 1.0f };

float Vector2::Length() const
{
    return std::sqrt(LengthSq());
}

Vector2 Vector2::Normalized() const
{
    float l = Length();
    return l > Math::EPSILON ? *this * (1.0f / l) : Vector2{};
}

// ─────────────────────────────────────────────────────────────────────────────
// Vector3 — static const 정의
// ─────────────────────────────────────────────────────────────────────────────
const Vector3 Vector3::Zero    = { 0.0f, 0.0f, 0.0f };
const Vector3 Vector3::One     = { 1.0f, 1.0f, 1.0f };
const Vector3 Vector3::Up      = { 0.0f, 1.0f, 0.0f };
const Vector3 Vector3::Right   = { 1.0f, 0.0f, 0.0f };
const Vector3 Vector3::Forward = { 0.0f, 0.0f, 1.0f };

float Vector3::Length() const
{
    return std::sqrt(LengthSq());
}

Vector3 Vector3::Normalized() const
{
    float l = Length();
    return l > Math::EPSILON ? *this * (1.0f / l) : Vector3{};
}

// ─────────────────────────────────────────────────────────────────────────────
// Quaternion
// ─────────────────────────────────────────────────────────────────────────────
Quaternion Quaternion::operator*(const Quaternion& o) const
{
    return {
        w * o.x + x * o.w + y * o.z - z * o.y,
        w * o.y - x * o.z + y * o.w + z * o.x,
        w * o.z + x * o.y - y * o.x + z * o.w,
        w * o.w - x * o.x - y * o.y - z * o.z
    };
}

Quaternion Quaternion::Normalized() const
{
    float inv = 1.0f / std::sqrt(x * x + y * y + z * z + w * w);
    return { x * inv, y * inv, z * inv, w * inv };
}

Quaternion Quaternion::RotationAxis(const Vector3& axis, float radians)
{
    float half = radians * 0.5f;
    float s    = std::sin(half);
    Vector3 a  = axis.Normalized();
    return { a.x * s, a.y * s, a.z * s, std::cos(half) };
}

Quaternion Quaternion::RotationYawPitchRoll(float yaw, float pitch, float roll)
{
    float cy = std::cos(yaw   * 0.5f), sy = std::sin(yaw   * 0.5f);
    float cp = std::cos(pitch * 0.5f), sp = std::sin(pitch * 0.5f);
    float cr = std::cos(roll  * 0.5f), sr = std::sin(roll  * 0.5f);
    return {
        cy * sp * cr + sy * cp * sr,
        sy * cp * cr - cy * sp * sr,
        cy * cp * sr - sy * sp * cr,
        cy * cp * cr + sy * sp * sr
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Matrix4x4
// ─────────────────────────────────────────────────────────────────────────────
Matrix4x4 Matrix4x4::FromFloats(const float* src16)
{
    Matrix4x4 r;
    std::memcpy(r.m, src16, 64);
    return r;
}

Matrix4x4 Matrix4x4::operator*(const Matrix4x4& o) const
{
    Matrix4x4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                r.m[i][j] += m[i][k] * o.m[k][j];
    return r;
}

Matrix4x4 Matrix4x4::Transposed() const
{
    Matrix4x4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r.m[i][j] = m[j][i];
    return r;
}

Matrix4x4::Decomposed Matrix4x4::Decompose() const
{
    Vector3 t = { m[3][0], m[3][1], m[3][2] };

    float sx = std::sqrt(m[0][0]*m[0][0] + m[0][1]*m[0][1] + m[0][2]*m[0][2]);
    float sy = std::sqrt(m[1][0]*m[1][0] + m[1][1]*m[1][1] + m[1][2]*m[1][2]);
    float sz = std::sqrt(m[2][0]*m[2][0] + m[2][1]*m[2][1] + m[2][2]*m[2][2]);

    float r00 = m[0][0]/sx, r01 = m[0][1]/sx, r02 = m[0][2]/sx;
    float r10 = m[1][0]/sy, r11 = m[1][1]/sy, r12 = m[1][2]/sy;
    float r20 = m[2][0]/sz, r21 = m[2][1]/sz, r22 = m[2][2]/sz;

    float trace = r00 + r11 + r22;
    Quaternion q;
    if (trace > 0.0f)
    {
        float ss = 2.0f * std::sqrt(trace + 1.0f);
        q.w = 0.25f * ss;
        q.x = (r12 - r21) / ss;
        q.y = (r20 - r02) / ss;
        q.z = (r01 - r10) / ss;
    }
    else if (r00 > r11 && r00 > r22)
    {
        float ss = 2.0f * std::sqrt(1.0f + r00 - r11 - r22);
        q.w = (r12 - r21) / ss;
        q.x = 0.25f * ss;
        q.y = (r01 + r10) / ss;
        q.z = (r20 + r02) / ss;
    }
    else if (r11 > r22)
    {
        float ss = 2.0f * std::sqrt(1.0f + r11 - r00 - r22);
        q.w = (r20 - r02) / ss;
        q.x = (r01 + r10) / ss;
        q.y = 0.25f * ss;
        q.z = (r12 + r21) / ss;
    }
    else
    {
        float ss = 2.0f * std::sqrt(1.0f + r22 - r00 - r11);
        q.w = (r01 - r10) / ss;
        q.x = (r20 + r02) / ss;
        q.y = (r12 + r21) / ss;
        q.z = 0.25f * ss;
    }

    return { t, q.Normalized(), { sx, sy, sz } };
}

Matrix4x4 Matrix4x4::Identity()
{
    Matrix4x4 r;
    r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
    return r;
}

Matrix4x4 Matrix4x4::Translation(const Vector3& t)
{
    Matrix4x4 r = Identity();
    r.m[3][0] = t.x; r.m[3][1] = t.y; r.m[3][2] = t.z;
    return r;
}

Matrix4x4 Matrix4x4::Scale(const Vector3& s)
{
    Matrix4x4 r;
    r.m[0][0] = s.x; r.m[1][1] = s.y; r.m[2][2] = s.z; r.m[3][3] = 1.0f;
    return r;
}

Matrix4x4 Matrix4x4::RotationX(float rad)
{
    float c = std::cos(rad), s = std::sin(rad);
    Matrix4x4 r = Identity();
    r.m[1][1] =  c; r.m[1][2] = s;
    r.m[2][1] = -s; r.m[2][2] = c;
    return r;
}

Matrix4x4 Matrix4x4::RotationY(float rad)
{
    float c = std::cos(rad), s = std::sin(rad);
    Matrix4x4 r = Identity();
    r.m[0][0] =  c; r.m[0][2] = -s;
    r.m[2][0] =  s; r.m[2][2] =  c;
    return r;
}

Matrix4x4 Matrix4x4::RotationZ(float rad)
{
    float c = std::cos(rad), s = std::sin(rad);
    Matrix4x4 r = Identity();
    r.m[0][0] =  c; r.m[0][1] = s;
    r.m[1][0] = -s; r.m[1][1] = c;
    return r;
}

Matrix4x4 Matrix4x4::RotationQuat(const Quaternion& q)
{
    float xx = q.x*q.x, yy = q.y*q.y, zz = q.z*q.z;
    float xy = q.x*q.y, xz = q.x*q.z, yz = q.y*q.z;
    float wx = q.w*q.x, wy = q.w*q.y, wz = q.w*q.z;
    Matrix4x4 r;
    r.m[0][0] = 1-2*(yy+zz); r.m[0][1] = 2*(xy+wz);   r.m[0][2] = 2*(xz-wy);   r.m[0][3] = 0;
    r.m[1][0] = 2*(xy-wz);   r.m[1][1] = 1-2*(xx+zz); r.m[1][2] = 2*(yz+wx);   r.m[1][3] = 0;
    r.m[2][0] = 2*(xz+wy);   r.m[2][1] = 2*(yz-wx);   r.m[2][2] = 1-2*(xx+yy); r.m[2][3] = 0;
    r.m[3][0] = 0;            r.m[3][1] = 0;            r.m[3][2] = 0;            r.m[3][3] = 1;
    return r;
}

Matrix4x4 Matrix4x4::LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up)
{
    Vector3 zAxis = (target - eye).Normalized();        // 전방 (+Z)
    Vector3 xAxis = up.Cross(zAxis).Normalized();       // 우측 (+X)
    Vector3 yAxis = zAxis.Cross(xAxis);                 // 상방 (+Y)
    Matrix4x4 r;
    r.m[0][0] = xAxis.x; r.m[0][1] = yAxis.x; r.m[0][2] = zAxis.x; r.m[0][3] = 0;
    r.m[1][0] = xAxis.y; r.m[1][1] = yAxis.y; r.m[1][2] = zAxis.y; r.m[1][3] = 0;
    r.m[2][0] = xAxis.z; r.m[2][1] = yAxis.z; r.m[2][2] = zAxis.z; r.m[2][3] = 0;
    r.m[3][0] = -xAxis.Dot(eye);
    r.m[3][1] = -yAxis.Dot(eye);
    r.m[3][2] = -zAxis.Dot(eye);
    r.m[3][3] = 1;
    return r;
}

Matrix4x4 Matrix4x4::LookAtRH(const Vector3& eye, const Vector3& target, const Vector3& up)
{
    Vector3 zAxis = (eye - target).Normalized();        // 카메라 +Z (장면 반대방향)
    Vector3 xAxis = up.Cross(zAxis).Normalized();
    Vector3 yAxis = zAxis.Cross(xAxis);
    Matrix4x4 r;
    r.m[0][0] = xAxis.x; r.m[0][1] = yAxis.x; r.m[0][2] = zAxis.x; r.m[0][3] = 0;
    r.m[1][0] = xAxis.y; r.m[1][1] = yAxis.y; r.m[1][2] = zAxis.y; r.m[1][3] = 0;
    r.m[2][0] = xAxis.z; r.m[2][1] = yAxis.z; r.m[2][2] = zAxis.z; r.m[2][3] = 0;
    r.m[3][0] = -xAxis.Dot(eye);
    r.m[3][1] = -yAxis.Dot(eye);
    r.m[3][2] = -zAxis.Dot(eye);
    r.m[3][3] = 1;
    return r;
}

Matrix4x4 Matrix4x4::PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ)
{
    float f  = 1.0f / std::tan(fovY * 0.5f);
    float dz = farZ / (farZ - nearZ);
    Matrix4x4 r;
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = dz;           r.m[2][3] = 1.0f;
    r.m[3][2] = -nearZ * dz;
    return r;
}

Matrix4x4 Matrix4x4::PerspectiveFovRH(float fovY, float aspect, float nearZ, float farZ)
{
    float f  = 1.0f / std::tan(fovY * 0.5f);
    float dz = farZ / (nearZ - farZ);           // 음수
    Matrix4x4 r;
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = dz;            r.m[2][3] = -1.0f;
    r.m[3][2] = nearZ * dz;
    return r;
}

Matrix4x4 Matrix4x4::Inverse() const
{
    // Gauss-Jordan 소거법 (부분 피벗)
    float a[4][8];
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j) a[i][j]     = m[i][j];
        for (int j = 0; j < 4; ++j) a[i][j + 4] = (i == j) ? 1.0f : 0.0f;
    }
    for (int col = 0; col < 4; ++col)
    {
        int pivot = col;
        for (int row = col + 1; row < 4; ++row)
            if (std::fabs(a[row][col]) > std::fabs(a[pivot][col]))
                pivot = row;
        if (pivot != col)
            for (int j = 0; j < 8; ++j)
                std::swap(a[col][j], a[pivot][j]);
        float inv = 1.0f / a[col][col];
        for (int j = 0; j < 8; ++j)
            a[col][j] *= inv;
        for (int row = 0; row < 4; ++row)
        {
            if (row == col) continue;
            float factor = a[row][col];
            for (int j = 0; j < 8; ++j)
                a[row][j] -= factor * a[col][j];
        }
    }
    Matrix4x4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r.m[i][j] = a[i][j + 4];
    return r;
}

Matrix4x4 Matrix4x4::Lerp(const Matrix4x4& a, const Matrix4x4& b, float t)
{
    Matrix4x4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r.m[i][j] = Math::Lerp(a.m[i][j], b.m[i][j], t);
    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// Free functions
// ─────────────────────────────────────────────────────────────────────────────
Vector3 TransformPoint(const Vector3& v, const Matrix4x4& m)
{
    return {
        v.x*m.m[0][0] + v.y*m.m[1][0] + v.z*m.m[2][0] + m.m[3][0],
        v.x*m.m[0][1] + v.y*m.m[1][1] + v.z*m.m[2][1] + m.m[3][1],
        v.x*m.m[0][2] + v.y*m.m[1][2] + v.z*m.m[2][2] + m.m[3][2]
    };
}

Vector3 TransformDirection(const Vector3& v, const Matrix4x4& m)
{
    return {
        v.x*m.m[0][0] + v.y*m.m[1][0] + v.z*m.m[2][0],
        v.x*m.m[0][1] + v.y*m.m[1][1] + v.z*m.m[2][1],
        v.x*m.m[0][2] + v.y*m.m[1][2] + v.z*m.m[2][2]
    };
}
