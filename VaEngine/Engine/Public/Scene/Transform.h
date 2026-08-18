#pragma once

#include "Math/Container.h"

class Transform
{
public:
    // ── Position ──────────────────────────────────────────────────────────────
    void    SetPosition(float x, float y, float z) { position = { x, y, z }; dirty = true; }
    void    SetPosition(const Vector3& p)           { position = p;           dirty = true; }
    void    Translate  (const Vector3& delta)       { position += delta;      dirty = true; }
    Vector3 GetPosition() const { return position; }

    // ── Rotation ──────────────────────────────────────────────────────────────
    void SetRotation(const Quaternion& q)
        { rotation = q.Normalized(); dirty = true; }

    void SetEuler(float yaw, float pitch, float roll)
        { rotation = Quaternion::RotationYawPitchRoll(yaw, pitch, roll); dirty = true; }

    void SetEulerDeg(float yaw, float pitch, float roll)
        { SetEuler(yaw * Math::DEG2RAD, pitch * Math::DEG2RAD, roll * Math::DEG2RAD); }

    void Rotate(const Vector3& axis, float radians)
        { rotation = (rotation * Quaternion::RotationAxis(axis, radians)).Normalized(); dirty = true; }

    Quaternion GetRotation() const { return rotation; }

    // ── Scale (디자인 타임 크기 · 임포트 단위 보정) ────────────────────────────
    void    SetScale(const Vector3& s) { scale = s;                        dirty = true; }
    void    SetScale(float uniform)    { scale = { uniform, uniform, uniform }; dirty = true; }
    Vector3 GetScale() const { return scale; }

    // ── Scale Multiplier (런타임 게임플레이 효과 전용 — grow/shrink/pulse 등) ──
    // Scale과 완전히 독립적으로 합성되어(성분별 곱) 서로의 존재를 몰라도 충돌하지 않는다.
    void    SetScaleMultiplier(const Vector3& m) { scaleMultiplier = m;                        dirty = true; }
    void    SetScaleMultiplier(float uniform)    { scaleMultiplier = { uniform, uniform, uniform }; dirty = true; }
    Vector3 GetScaleMultiplier() const { return scaleMultiplier; }

    // ── Matrix ────────────────────────────────────────────────────────────────
    const Matrix4x4& GetMatrix() const
    {
        if (dirty)
        {
            const Vector3 effectiveScale = { scale.x * scaleMultiplier.x,
                                             scale.y * scaleMultiplier.y,
                                             scale.z * scaleMultiplier.z };
            matrix = Matrix4x4::Scale(effectiveScale)
                   * Matrix4x4::RotationQuat(rotation)
                   * Matrix4x4::Translation(position);
            dirty = false;
        }
        return matrix;
    }

    // ── Direction vectors (rotation 기준) ──────────────────────────────────────
    Vector3 Forward() const { return TransformDirection(Vector3::Forward, Matrix4x4::RotationQuat(rotation)); }
    Vector3 Right()   const { return TransformDirection(Vector3::Right,   Matrix4x4::RotationQuat(rotation)); }
    Vector3 Up()      const { return TransformDirection(Vector3::Up,      Matrix4x4::RotationQuat(rotation)); }

private:
    Vector3    position        = { 0.0f, 0.0f, 0.0f };
    Quaternion rotation        = Quaternion::Identity();
    Vector3    scale           = { 1.0f, 1.0f, 1.0f };
    Vector3    scaleMultiplier = { 1.0f, 1.0f, 1.0f };

    mutable Matrix4x4 matrix;
    mutable bool      dirty = true;
};
