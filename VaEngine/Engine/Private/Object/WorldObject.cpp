#include "Object/WorldObject.h"

#include <algorithm>

WorldObject::~WorldObject()
{
    Detach();
    for (WorldObject* child : children)
        child->parent = nullptr;  // 소유권이 없으므로 delete하지 않고 orphan 처리만 한다
}

void WorldObject::AttachTo(WorldObject* newParent)
{
    Detach();
    parent = newParent;
    if (parent)
        parent->children.push_back(this);
}

void WorldObject::Detach()
{
    if (!parent)
        return;

    auto& siblings = parent->children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    parent = nullptr;
}

Matrix4x4 WorldObject::GetWorldMatrix() const
{
    return parent ? transform.GetMatrix() * parent->GetWorldMatrix() : transform.GetMatrix();
}

void WorldObject::DrawGizmo() const
{
    // Transform::Forward()/Up()/Right()는 로컬 회전 기준이라 부모 회전이 반영되지 않으므로,
    // 월드 행렬에서 직접 축을 뽑는다(Container.h::GetWorld*, row-vector 컨벤션).
    const Matrix4x4 world = GetWorldMatrix();
    const Vector3 origin  = GetWorldTranslation(world);
    const Vector3 right   = GetWorldRight(world).Normalized();
    const Vector3 up      = GetWorldUp(world).Normalized();
    const Vector3 forward = GetWorldForward(world).Normalized();

    VA_DRAW_LINE(origin, origin + forward * 0.5f, { 1.f, 0.f, 0.f, 1.f });
    VA_DRAW_LINE(origin, origin + up      * 0.5f, { 0.f, 1.f, 0.f, 1.f });
    VA_DRAW_LINE(origin, origin + right   * 0.5f, { 0.f, 0.f, 1.f, 1.f });
}
