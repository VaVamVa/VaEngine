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

    // ── Scale ─────────────────────────────────────────────────────────────────
    void    SetScale(const Vector3& s) { scale = s;                        dirty = true; }
    void    SetScale(float uniform)    { scale = { uniform, uniform, uniform }; dirty = true; }
    Vector3 GetScale() const { return scale; }

    // ── Matrix ────────────────────────────────────────────────────────────────
    const Matrix4x4& GetMatrix() const
    {
        if (dirty)
        {
            matrix = Matrix4x4::Scale(scale)
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
    Vector3    position = { 0.0f, 0.0f, 0.0f };
    Quaternion rotation = Quaternion::Identity();
    Vector3    scale    = { 1.0f, 1.0f, 1.0f };

    mutable Matrix4x4 matrix;
    mutable bool      dirty = true;
};
