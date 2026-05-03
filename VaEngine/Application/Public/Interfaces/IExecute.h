#pragma once

#include "Common_RHI.h"

// ============================================================
// IExecute
//
// 플랫폼 루프(ExecWindows, ExecAndroid 등)와 앱 로직 사이의
// 계약 인터페이스입니다.
//
// 플랫폼은 이 인터페이스만 알면 되고, 구체적인 앱 로직은
// Application 프로젝트 안의 Execute 구현체에 위치합니다.
// ============================================================
class IExecute
{
public:
    virtual ~IExecute() = default;

#pragma region Lifecycle
    virtual void OnInitialize(NativeDisplayInfo displayInfo) = 0;
    virtual void OnDestroy() = 0;

    virtual void OnLoop() = 0;
    virtual void OnSuspend() {}
    virtual void OnResume()  {}

protected:
    virtual void OnUpdate()     = 0;
    virtual void OnRender()     = 0;
    virtual void OnPostRender() = 0;
#pragma endregion Lifecycle
};
