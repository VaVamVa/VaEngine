#include "System/TimerSystem.h"

#include <algorithm>

TimerSystem::Handle TimerSystem::SetTimer(float delay, Callback callback)
{
    const Handle h = nextHandle++;
    entries.push_back({ h, delay, delay, false, std::move(callback) });
    return h;
}

TimerSystem::Handle TimerSystem::SetRepeating(float interval, Callback callback)
{
    const Handle h = nextHandle++;
    entries.push_back({ h, interval, interval, true, std::move(callback) });
    return h;
}

void TimerSystem::ClearTimer(Handle& handle)
{
    if (handle == Invalid) return;

    auto it = std::find_if(entries.begin(), entries.end(),
        [handle](const Entry& e) { return e.handle == handle; }
    );

    if (it != entries.end())
    {
        entries.erase(it);
    }

    handle = Invalid;
}

bool TimerSystem::IsActive(Handle handle) const
{
    if (handle == Invalid) return false;

    return std::any_of(entries.begin(), entries.end(),
        [handle](const Entry& e) { return e.handle == handle; });
}

void TimerSystem::Update(float deltaTime)
{
    // 먼저 발동 대상 수집 (콜백 중 entries 변경 방지)
    std::vector<Handle> fired;

    for (auto& e : entries)
    {
        e.remaining -= deltaTime;
        if (e.remaining <= 0.0f)
            fired.push_back(e.handle);
    }

    for (Handle h : fired)
    {
        auto it = std::find_if(entries.begin(), entries.end(),
            [h](const Entry& e) { return e.handle == h; });
        if (it == entries.end()) continue;

        it->callback();

        // 콜백 내부에서 ClearTimer가 호출됐을 수 있으므로 재탐색
        it = std::find_if(entries.begin(), entries.end(),
            [h](const Entry& e) { return e.handle == h; });
        if (it == entries.end()) continue;

        if (it->repeating)
            it->remaining += it->interval;  // 누적 오차 흡수
        else
            entries.erase(it);
    }
}
