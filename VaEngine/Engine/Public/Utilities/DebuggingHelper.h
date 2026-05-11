#pragma once
#include "Math/Container.h"
#include <string>
#include <vector>
#include <map>
#include <fstream>

// ─────────────────────────────────────────────────────────────────────────────
// Debug Configuration
// ─────────────────────────────────────────────────────────────────────────────
#ifndef VA_DEBUG
#define VA_DEBUG 1
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Debug Macros
// ─────────────────────────────────────────────────────────────────────────────
#if VA_DEBUG
    #define VA_LOG(category, message)        DebuggingHelper::Log(category, message)
    #define VA_DRAW_TEXT(text, pos, ...)     DebuggingHelper::DrawTextFree(text, pos, ##__VA_ARGS__)
    #define VA_DRAW_PANEL(key, text, ...)    DebuggingHelper::DrawTextPanel(key, text, ##__VA_ARGS__)
    #define VA_DRAW_LINE(start, end, ...)       DebuggingHelper::DrawLine(start, end, ##__VA_ARGS__)
    #define VA_DRAW_LINE_DEPTH(start, end, ...) DebuggingHelper::DrawLineDepth(start, end, ##__VA_ARGS__)
#else
    #define VA_LOG(category, message)
    #define VA_DRAW_TEXT(text, pos, ...)
    #define VA_DRAW_PANEL(key, text, ...)
    #define VA_DRAW_LINE(start, end, ...)
    #define VA_DRAW_LINE_DEPTH(start, end, ...)
#endif

struct DebugTextEntry {
    std::string text;
    Vector2     position;
    Vector4     color;
};

struct DebugPanelEntry {
    std::string text;
    Vector4     color;
};

struct DebugLineEntry {
    Vector3 start;
    Vector3 end;
    Vector4 color;
    bool    depthTest = false;
};

class DebuggingHelper {
public:
    static void DrawTextFree(const std::string& text, Vector2 pos, Vector4 color = {1, 1, 1, 1});
    static void DrawTextPanel(uint32_t key, const std::string& text, Vector4 color = {1, 1, 1, 1});
	static void DrawLine     (Vector3 start, Vector3 end, Vector4 color = { 1, 1, 1, 1 });
	static void DrawLineDepth(Vector3 start, Vector3 end, Vector4 color = { 1, 1, 1, 1 });

    // logDir 경로에 YYYY-MM-DD_HH-MM-SS.log 파일 생성 (프로세스 시작 시 1회)
    static void InitLog(const char* logDir);
    static void Log(const std::string& category, const std::string& message);

    static const std::vector<DebugTextEntry>&           GetTextEntries();
    static const std::map<uint32_t, DebugPanelEntry>&   GetPanelEntries();
	static const std::vector<DebugLineEntry>&           GetLineEntries();

    static void Clear();

private:
    static std::vector<DebugTextEntry>         textEntries;
    static std::map<uint32_t, DebugPanelEntry> panelEntries;
	static std::vector<DebugLineEntry>         lineEntries;
    static std::ofstream                       logFile;
};
