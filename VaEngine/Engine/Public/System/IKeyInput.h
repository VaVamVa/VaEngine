#pragma once

#include <memory>
#include <cstdint>

/*
EKeyCode 값은 Windows VK_ 코드와 일치한다.
Windows 구현은 별도 변환 테이블 없이 배열을 직접 인덱싱 가능.
다른 플랫폼(Android, Console 등)은 플랫폼 코드 → EKeyCode 매핑을 구현체 내부에서 처리.
*/
enum class EKeyCode : uint32_t
{
	// 제어 키
	Backspace = 0x08,
	Tab       = 0x09,
	Enter     = 0x0D,
	Escape    = 0x1B,
	Space     = 0x20,
	Delete    = 0x2E,

	// 방향키
	Left  = 0x25,
	Up    = 0x26,
	Right = 0x27,
	Down  = 0x28,

	// 숫자 (0–9)
	Num0 = 0x30, Num1, Num2, Num3, Num4,
	Num5,        Num6, Num7, Num8, Num9,

	// 알파벳 (A–Z)
	A = 0x41, B, C, D, E, F, G, H, I, J, K, L, M,
	N,        O, P, Q, R, S, T, U, V, W, X, Y, Z,

	// 펑션 키
	F1  = 0x70, F2,  F3,  F4,  F5,  F6,
	F7,         F8,  F9,  F10, F11, F12,

	// 수정자 키
	LeftShift  = 0xA0,
	RightShift = 0xA1,
	LeftCtrl   = 0xA2,
	RightCtrl  = 0xA3,
	LeftAlt    = 0xA4,
	RightAlt   = 0xA5,

	// 게임패드 (0x100–0x10F) — Windows KeyInput은 미지원, 항상 0
	GamepadA      = 0x100,
	GamepadB      = 0x101,
	GamepadX      = 0x102,
	GamepadY      = 0x103,
	GamepadLB     = 0x104,
	GamepadRB     = 0x105,
	GamepadLT     = 0x106,
	GamepadRT     = 0x107,
	GamepadStart  = 0x108,
	GamepadSelect = 0x109,
	GamepadDUp    = 0x10A,
	GamepadDDown  = 0x10B,
	GamepadDLeft  = 0x10C,
	GamepadDRight = 0x10D,
	GamepadLS     = 0x10E,
	GamepadRS     = 0x10F,

	Max = 0x200
};

enum class EInputTrigger : uint8_t
{
	Down,   // 누르는 첫 프레임
	Press,  // 누르는 동안 매 프레임
	Up,     // 떼는 첫 프레임
};

class IKeyInput
{
public:
	static std::unique_ptr<IKeyInput> Create();
	virtual ~IKeyInput() = default;

	virtual void Update() = 0;

	virtual bool IsDown (EKeyCode key) const = 0;  // 이번 프레임에 처음 눌림
	virtual bool IsUp   (EKeyCode key) const = 0;  // 이번 프레임에 처음 뗌
	virtual bool IsPress(EKeyCode key) const = 0;  // 계속 누르고 있음
};
