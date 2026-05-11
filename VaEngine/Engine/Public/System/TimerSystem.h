#pragma once

#include <functional>
#include <vector>
#include <cstdint>

class TimerSystem
{
public:
    using Callback = std::function<void()>;
    using Handle   = uint64_t;
    static constexpr Handle Invalid = 0;

    // delay 초 후 1회 발동 후 자동 제거
    Handle SetTimer    (float delay, Callback callback);
    // interval 초마다 반복 발동 — ClearTimer로 직접 취소
    Handle SetRepeating(float interval, Callback callback);

    // 타이머 취소 + handle을 Invalid로 초기화
    void ClearTimer(Handle& handle);
    bool IsActive  (Handle  handle) const;

    void Update(float deltaTime);

private:
    struct Entry
    {
        Handle   handle;
        float    interval;   // 반복 시 원본 간격 (단일 타이머도 동일값 저장)
        float    remaining;  // 다음 발동까지 남은 시간
        bool     repeating;
        Callback callback;
    };

    std::vector<Entry> entries;
    Handle             nextHandle = 1;
};
