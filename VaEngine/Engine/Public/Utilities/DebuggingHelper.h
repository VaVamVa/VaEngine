#pragma once
#include "Math/Container.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Debug Configuration
// ─────────────────────────────────────────────────────────────────────────────
#ifndef VA_DEBUG
#define VA_DEBUG 0
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Debug Macros
// ─────────────────────────────────────────────────────────────────────────────
#if VA_DEBUG
    #define VA_LOG(category, message) DebuggingHelper::Log(category, message)
    #define VA_DRAW_TEXT(text, pos, color) DebuggingHelper::DrawText(text, pos, color)
#else
    #define VA_LOG(category, message) 
    #define VA_DRAW_TEXT(text, pos, color)
#endif

struct DebugTextEntry {
    std::string text;
    Vector2     position;
    Vector4     color;
};

class DebuggingHelper {
public:
    // 화면에 텍스트 기록 요청 (현재 프레임용)
    static void DrawText(const std::string& text, Vector2 pos, Vector4 color = {1, 1, 1, 1});
    
    // 콘솔 로그 (파일/콘솔 출력 전용)
    static void Log(const std::string& category, const std::string& message);

    // 렌더러가 호출: 수집된 텍스트 목록 반환
    static const std::vector<DebugTextEntry>& GetTextEntries();

    // 매 프레임 끝에 호출하여 초기화
    static void Clear();

private:
    static std::vector<DebugTextEntry> s_textEntries;
};
