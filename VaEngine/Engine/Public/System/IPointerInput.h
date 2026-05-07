#pragma once

#include <memory>
#include <cstdint>

enum class EPointerButton : uint32_t
{
	Primary   = 0,  // 마우스 좌클릭 / 터치 탭
	Secondary = 1,  // 마우스 우클릭 / 길게 누르기
	Tertiary  = 2,  // 마우스 휠클릭 / 세 손가락 탭
	Max
};

// 포인터 이동 축 — InputContext에서 바인딩 소스로 사용
enum class EPointerSource : uint32_t
{
	DeltaX,      // 이번 프레임 수평 이동량
	DeltaY,      // 이번 프레임 수직 이동량
	ScrollDelta, // 이번 프레임 스크롤 이동량
};

struct PointerState
{
	float x           = 0.0f;  // 현재 스크린 좌표
	float y           = 0.0f;
	float deltaX      = 0.0f;  // 이번 프레임 이동량
	float deltaY      = 0.0f;
	float scroll      = 0.0f;  // 누적 스크롤값
	float scrollDelta = 0.0f;  // 이번 프레임 스크롤 이동량
};

class IPointerInput
{
public:
	static std::unique_ptr<IPointerInput> Create();
	virtual ~IPointerInput() = default;

	virtual void Update() = 0;

	// 플랫폼 네이티브 이벤트 전달 (Windows: WM_MOUSEWHEEL 등)
	// 기본 구현은 no-op — 다른 플랫폼 구현체는 재정의 불필요
	virtual void ProcessNativeEvent(uintptr_t message, uintptr_t wParam, uintptr_t lParam) {}

	virtual bool IsDown (EPointerButton btn) const = 0;
	virtual bool IsUp   (EPointerButton btn) const = 0;
	virtual bool IsPress(EPointerButton btn) const = 0;

	virtual PointerState GetState() const = 0;
};
