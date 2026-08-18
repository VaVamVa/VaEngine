#pragma once

#include "Scene/Transform.h"
#include "Scene/RenderScene.h"
#include "Utilities/DebuggingHelper.h"

#include <vector>

class WorldObject
{
public:
    virtual ~WorldObject();

    Transform        transform;   // 부모 기준 로컬 트랜스폼(부모 없으면 월드와 동일)
    RenderObjectDesc renderDesc;  // Client(Application)가 설정하는 렌더링 속성

    // 부모-자식 계층 — 소유권 없음(Application이 실제 WorldObject 수명을 관리).
    // 카메라 붐, 정적 오브젝트 부착 등에 사용. 스켈레탈 메시의 본 단위 부착(모자 등 장식)은
    // 이 계층과 무관한 별도 메커니즘(리깅된 서브메시 — WorldAnimatedModel이 이미 지원)으로 처리한다.
    void AttachTo(WorldObject* newParent);
    void Detach();
    WorldObject* GetParent() const { return parent; }

    // 부모 체인을 따라 누적한 월드 트랜스폼. 매 호출마다 다시 계산한다(캐싱 없음) —
    // 계층 깊이가 얕아 비용이 무시할 만한 수준이고, 부모 변경 시 자식의 캐시를 무효화하는
    // 전파 로직을 별도로 두지 않아도 되어 stale-cache 버그가 구조적으로 생길 수 없다.
    Matrix4x4 GetWorldMatrix() const;

    void AddToScene(RenderScene& scene) const
    {
        Impl_AddToScene(scene);
#if VA_DEBUG
        DrawGizmo();
#endif
        for (WorldObject* child : children)
            child->AddToScene(scene);
    }

protected:
    virtual void Impl_AddToScene(RenderScene& scene) const = 0;

private:
    void DrawGizmo() const;

    WorldObject*              parent = nullptr;
    std::vector<WorldObject*> children;  // 비소유 — Application이 실제 수명 관리
};
