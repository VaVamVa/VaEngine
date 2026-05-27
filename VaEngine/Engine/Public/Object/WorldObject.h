#pragma once

#include "Scene/Transform.h"
#include "Scene/RenderScene.h"
#include "Utilities/DebuggingHelper.h"

class WorldObject
{
public:
    virtual ~WorldObject() = default;

    Transform        transform;
    RenderObjectDesc renderDesc;  // Client(Application)가 설정하는 렌더링 속성

    void AddToScene(RenderScene& scene) const
    {
        Impl_AddToScene(scene);
#if VA_DEBUG
        DrawGizmo();
#endif
    }

protected:
    virtual void Impl_AddToScene(RenderScene& scene) const = 0;

private:
    void DrawGizmo() const
    {
        const Vector3 origin = transform.GetPosition();
        VA_DRAW_LINE(origin, origin + transform.Forward() * 0.5f, { 1.f, 0.f, 0.f, 1.f });
        VA_DRAW_LINE(origin, origin + transform.Up()      * 0.5f, { 0.f, 1.f, 0.f, 1.f });
        VA_DRAW_LINE(origin, origin + transform.Right()   * 0.5f, { 0.f, 0.f, 1.f, 1.f });
    }
};
