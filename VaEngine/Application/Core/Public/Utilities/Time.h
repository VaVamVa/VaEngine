#pragma once

#include "System/ITime.h"
#include "Utilities/Locator.h"

namespace Time
{
    inline float DeltaTime() { return Locator<ITimeReader>::Get().Delta();     }
    inline float Elapsed()   { return Locator<ITimeReader>::Get().Running();   }
    inline float FPS()       { return Locator<ITimeReader>::Get().FPS();       }
    inline bool  IsStopped() { return Locator<ITimeReader>::Get().IsStopped(); }
}
