#include "Utilities/DebuggingHelper.h"
#include <iostream>
#include <format>

std::vector<DebugTextEntry> DebuggingHelper::s_textEntries;

void DebuggingHelper::DrawText(const std::string& text, Vector2 pos, Vector4 color) {
    s_textEntries.push_back({ text, pos, color });
}

void DebuggingHelper::Log(const std::string& category, const std::string& message) {
    std::cout << std::format("[{}] {}\n", category, message);
}

const std::vector<DebugTextEntry>& DebuggingHelper::GetTextEntries() {
    return s_textEntries;
}

void DebuggingHelper::Clear() {
    s_textEntries.clear();
}
