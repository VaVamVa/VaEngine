# Native C++: Modern C++ 및 Low-Level 시스템 엔지니어링 역량

본 문서는 박원서(VaVamVa) 개발자가 다양한 프로젝트(Worms, Portal, TTTK, CitRush)를 진행하며 활용한 **Modern C++(C++11/14/17/20)** 기술과 **Low-Level 메모리/시스템 제어** 역량을 소스 코드 분석을 통해 정리한 보고서입니다.

---

## 1. Modern C++ 최신 표준 활용 (C++20 & Concepts)

### 1.1 C++20 Concepts를 이용한 타입 안전성 강화
*   **관련 프로젝트**: [CitRush]
*   **코드 사례 (`WidgetDataAsset.h`)**:
    ```cpp
    template <typename T>
    concept IsDerivedFromUserWidget = std::derived_from<T, UUserWidget>;

    template <IsDerivedFromUserWidget T>
    TSubclassOf<T> GetWidgetClass(UClass* uClass) const;
    ```
*   **선정 이유**: UE5 환경에서 C++20의 `concept`를 선제적으로 도입하여, 런타임에 발생할 수 있는 `UClass` 캐스팅 오류를 컴파일 단계에서 차단했습니다.
*   **기술적 분석**: `std::derived_from`과 결합된 정적 제약을 통해 템플릿 인스턴스화 시점에 상속 관계를 검증함으로써 **Zero-cost 추상화**와 **코드 신뢰성**을 동시에 확보했습니다.

### 1.2 가변 인자 매크로와 메타 프로그래밍
*   **관련 프로젝트**: [Portal]
*   **코드 사례 (`DebugMacros.h`)**:
    ```cpp
    #define __WRAP__(expression) expression
    #define __ARGS__(_1, _2, _3, _4, expression, ...) expression
    #define __GET_ARGS_COUNT__(...) __WRAP__(__ARGS__(__VA_ARGS__, 3, 2, 1, 0))
    ```
*   **기술적 분석**: 가변 인자 매크로(`__VA_ARGS__`)와 토큰 결합 기법을 활용하여 인자 개수에 따라 오버로딩되는 디버깅 시스템을 구축했습니다. 이는 전처리기 수준에서의 메타 프로그래밍 능력을 보여줍니다.

---

## 2. Low-Level 최적화 및 하드웨어 가속

### 2.1 SIMD Intrinsics를 활용한 연산 최적화
*   **관련 프로젝트**: [Portal]
*   **코드 사례 (`PortalMathLibrary.cpp`)**:
    ```cpp
    float UPortalMathLibrary::FusedMultiplyAdd(float MultiplyForward, float MultiplyBackward, float AddLast) {
    #if PLATFORM_WINDOWS && PLATFORM_64BITS
        return _mm_cvtss_f32(_mm_fmadd_ss(_mm_set_ss(MultiplyForward), _mm_set_ss(MultiplyBackward), _mm_set_ss(AddLast)));
    #elif (PLATFORM_MAC || PLATFORM_IOS) && defined(__aarch64__)
        return vfams_32(AddLast, MultiplyForward, MultiplyBackward);
    #else
        return MultiplyForward * MultiplyBackward + AddLast;
    #endif
    }
    ```
*   **기술적 분석**: 플랫폼별(x64 FMA 명령어, ARM NEON) SIMD 명령어를 직접 호출하는 **Intrinsics**를 사용하여 하드웨어 가속을 구현했습니다. 이는 단순 로직 구현을 넘어 CPU 아키텍처 수준의 성능 최적화가 가능함을 증명합니다.

### 2.2 비트 연산자 오버로딩 기반 상태 압축
*   **관련 프로젝트**: [Portal], [TTTK]
*   **코드 사례 (`ControlComponent.h`)**:
    ```cpp
    enum class EPressedKeys : uint8 {
        W = 1 << 0, A = 1 << 1, S = 1 << 2, D = 1 << 3, ...
    };
    FORCEINLINE EPressedKeys operator|(EPressedKeys L, EPressedKeys R) {
        return static_cast<EPressedKeys>(static_cast<uint8>(L) | static_cast<uint8>(R));
    }
    FORCEINLINE bool IsPressedOnce(const EPressedKeys& BitMask, const EPressedKeys& PrevBitMask, EPressedKeys&& BitFlag) {
        return static_cast<bool>(BitMask & BitFlag) ^ static_cast<bool>(PrevBitMask & BitFlag);
    }
    ```
*   **기술적 분석**: `enum class`에 대한 비트 연산자(`|`, `&`, `^`)를 직접 오버로딩하여 입력 상태를 1바이트 내에 효율적으로 관리했습니다. 특히 `XOR` 연산을 통한 '상태 변화 감지(PressedOnce)' 로직은 연산 최적화의 정수를 보여줍니다.

---

## 3. 엔진 내부 시스템 커스터마이징 및 메모리 관리

### 3.1 CRTP 패턴과 엔진 생명주기 동기화
*   **관련 프로젝트**: [CitRush]
*   **코드 사례 (`ObjectLifeCycleSingleton.h`)**:
    ```cpp
    template<class DerivedClass>
    class TObjectLifeCycleSingleton {
        static DerivedClass* Get() {
            if (!Instance) {
                Instance = new DerivedClass();
                FCoreDelegates::OnEnginePreExit.AddStatic(&TObjectLifeCycleSingleton::Destroy);
            }
            return Instance;
        }
    };
    ```
*   **기술적 분석**: **CRTP(Curiously Recurring Template Pattern)**를 사용하여 타입 안전한 싱글톤을 설계하고, 엔진 종료 델리게이트(`OnEnginePreExit`)에 명시적으로 바인딩하여 **RAII(Resource Acquisition Is Initialization)** 원칙을 엔진 생명주기에 확장 적용했습니다.

### 3.2 WinAPI 리소스 직접 관리 및 픽셀 연산
*   **관련 프로젝트**: [Worms]
*   **코드 사례 (`Texture.cpp`)**:
    ```cpp
    Texture::Texture(std::wstring fileName, ...) {
        memDC = CreateCompatibleDC(hdc);
        hBitmap = (HBITMAP)LoadImage(...);
        SelectObject(memDC, hBitmap);
    }
    Texture::~Texture() {
        DeleteDC(memDC);
        DeleteObject(hBitmap);
    }
    ```
*   **기술적 분석**: GDI 객체(HDC, HBITMAP)의 핸들을 직접 관리하고 소멸자에서 해제하는 명시적 메모리 관리를 수행했습니다. 또한 `GetPixel` 등을 통해 픽셀 데이터를 직접 추출하여 물리 충돌 판정에 활용하는 저수준 엔진 구현 능력을 보여줍니다.

---

## 4. 핵심 역량 요약
- **하드웨어 수준의 성능 최적화**: SIMD Intrinsics, 비트 연산, FMA 등을 통한 연산 병목 해결.
- **최신 표준 선제적 도입**: C++20 Concepts, SFINAE 대체 기법 등을 실제 엔진 프로젝트에 안정적으로 적용.
- **견고한 시스템 아키텍처**: CRTP, RAII, 의존성 역전(TFunction)을 활용한 유지보수성 높은 프레임워크 설계.
- **엔진 저수준 제어**: WinAPI 리소스 관리부터 UE 엔진 델리게이트 깊숙이 연동되는 커스텀 시스템 구축.
