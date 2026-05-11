#include "Utilities/DebuggingHelper.h"
#include <iostream>
#include <format>
#include <filesystem>
#include <chrono>
#include <ctime>

std::vector<DebugTextEntry>         DebuggingHelper::textEntries;
std::map<uint32_t, DebugPanelEntry> DebuggingHelper::panelEntries;
std::vector<DebugLineEntry>         DebuggingHelper::lineEntries;
std::ofstream                       DebuggingHelper::logFile;

void DebuggingHelper::DrawTextFree(const std::string& text, Vector2 pos, Vector4 color)
{
    textEntries.push_back({ text, pos, color });
}

void DebuggingHelper::DrawTextPanel(uint32_t key, const std::string& text, Vector4 color)
{
    panelEntries[key] = { text, color };
}

void DebuggingHelper::DrawLine(Vector3 start, Vector3 end, Vector4 color)
{
    lineEntries.push_back({ start, end, color, false });
}

void DebuggingHelper::DrawLineDepth(Vector3 start, Vector3 end, Vector4 color)
{
    lineEntries.push_back({ start, end, color, true });
}

void DebuggingHelper::InitLog(const char* logDir)
{
    std::filesystem::create_directories(logDir);

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);

    char filename[32];
    std::strftime(filename, sizeof(filename), "%Y-%m-%d_%H-%M-%S.log", &tm);

    logFile.open(std::string(logDir) + filename, std::ios::out);
}

void DebuggingHelper::Log(const std::string& category, const std::string& message)
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);

    char ts[12];
    std::strftime(ts, sizeof(ts), "%H:%M:%S", &tm);

    std::string line = std::format("[{}] [{}] {}", ts, category, message);
    std::cout << line << '\n';
    if (logFile.is_open())
        logFile << line << '\n' << std::flush;
}

const std::vector<DebugTextEntry>& DebuggingHelper::GetTextEntries()
{
    return textEntries;
}

const std::map<uint32_t, DebugPanelEntry>& DebuggingHelper::GetPanelEntries()
{
    return panelEntries;
}

const std::vector<DebugLineEntry>& DebuggingHelper::GetLineEntries()
{
    return lineEntries;
}

void DebuggingHelper::Clear()
{
    textEntries.clear();
    panelEntries.clear();
    lineEntries.clear();
}
