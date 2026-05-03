// ============================================================
// ExecAndroid 진입점
//
// android_native_app_glue 라이브러리를 활용한 NativeActivity 기반 진입점입니다.
//
// 동작 원리:
//   1. Android OS가 java Activity를 시작하면 JNI를 통해 libExecAndroid.so를 로드합니다.
//   2. android_native_app_glue 라이브러리가 ANativeActivity_onCreate()를 구현하고,
//      별도 C++ 스레드를 생성하여 android_main()을 호출합니다.
//   3. android_main은 ALooper 이벤트 루프를 돌며 OS 이벤트(창 생성/소멸, 포커스 등)를
//      처리하고, 창이 준비되면 Execute를 생성하여 렌더 루프를 실행합니다.
//
// Windows의 WinMain과 대응 관계:
//   WinMain(hInstance, ...)    ↔  android_main(android_app* app)
//   CreateWindowExW(...)       ↔  APP_CMD_INIT_WINDOW 이벤트 (OS가 창 생성)
//   PeekMessage 루프           ↔  ALooper_pollAll 루프
//   WM_ACTIVATE                ↔  APP_CMD_GAINED/LOST_FOCUS
//   WM_DESTROY                 ↔  APP_CMD_TERM_WINDOW
// ============================================================

#include <android_native_app_glue.h>
#include <memory>

#include "Execute.h"

// 전역 Execute 인스턴스.
// NativeActivity 수명 주기를 따라 APP_CMD_INIT_WINDOW에서 생성,
// APP_CMD_TERM_WINDOW에서 소멸합니다.
static std::unique_ptr<IExecute> execEngine;

// ============================================================
// HandleCommand — OS 이벤트(앱 수명 주기) 콜백
//
// android_native_app_glue가 OS 이벤트를 C++에서 처리하기 쉬운
// 정수 커맨드로 변환하여 이 함수를 호출합니다.
// ============================================================
static void HandleCommand(android_app* app, int32_t cmd)
{
    switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
    {
        // 창이 준비됨 → Execute 초기화
        //
        // app->window          : ANativeWindow* — 렌더링 대상 창 (Windows의 HWND에 대응)
        // app->activity->assetManager : AAssetManager* — APK assets 접근 핸들
        //                         Execute가 셰이더(.spv)를 APK에서 읽을 때 사용합니다.
        if (app->window)
        {
            execEngine = std::make_unique<Execute>();

            NativeDisplayInfo info{};
            info.platform = NativeDisplayInfo::EPlatform::Android;
            info.Handle   = app->window;                        // ANativeWindow*
            info.Display  = app->activity->assetManager;       // AAssetManager*

            execEngine->OnInitialize(info);
        }
        break;
    }
    case APP_CMD_TERM_WINDOW:
    {
        // 창이 소멸됨 → GPU 리소스 해제
        // 앱이 백그라운드로 가면 창이 먼저 파괴됩니다.
        if (execEngine)
        {
            execEngine->OnDestroy();
            execEngine.reset();
        }
        break;
    }
    case APP_CMD_GAINED_FOCUS:
    {
        // 포커스 획득 (홈에서 돌아옴 등) → 렌더링 재개
        if (execEngine) execEngine->OnResume();
        break;
    }
    case APP_CMD_LOST_FOCUS:
    {
        // 포커스 상실 (알림창 열림, 홈 버튼 등) → 렌더링 일시중지
        if (execEngine) execEngine->OnSuspend();
        break;
    }
    default:
        break;
    }
}

// ============================================================
// android_main — C++ 진입점
//
// android_native_app_glue가 ANativeActivity_onCreate() 내부에서
// 새 스레드를 생성하고 이 함수를 호출합니다.
// 함수가 반환되면 앱이 종료됩니다.
// ============================================================
void android_main(android_app* app)
{
    // 커맨드 콜백 등록
    app->onAppCmd = HandleCommand;

    // ---- 메인 루프 ----
    //
    // ALooper_pollAll:
    //   - 두 번째 인자 timeoutMillis:
    //       -1: 이벤트가 올 때까지 무한 대기 (execEngine이 없을 때, CPU 낭비 없음)
    //        0: 즉시 반환 (execEngine이 있을 때, 렌더 루프를 계속 돌기 위해)
    //   - Windows의 PeekMessage(PM_NOREMOVE, 0 타임아웃) 패턴과 동일합니다.
    while (!app->destroyRequested)
    {
        int                  events;
        android_poll_source* source;

        // execEngine이 있으면 타임아웃 0 (즉시 반환 → 렌더 루프 진행)
        // 없으면 타임아웃 -1 (이벤트 대기 → CPU 절약)
        int timeoutMs = (execEngine != nullptr) ? 0 : -1;

        while (ALooper_pollAll(timeoutMs, nullptr, &events,
                               reinterpret_cast<void**>(&source)) >= 0)
        {
            // 이벤트 소스 처리 (HandleCommand가 여기서 호출됨)
            if (source) source->process(app, source);
        }

        // 렌더 루프: Execute가 초기화된 경우에만 실행
        if (execEngine) execEngine->OnLoop();
    }

    // 루프 종료 시 Execute가 아직 살아있으면 정리
    if (execEngine)
    {
        execEngine->OnDestroy();
        execEngine.reset();
    }
}
