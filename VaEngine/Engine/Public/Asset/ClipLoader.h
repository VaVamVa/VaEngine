#pragma once

#include "Animation/AnimClip.h"

#include <string>

namespace ClipLoader
{
    // .clip 파일 → AnimClipData (실패 시 frameCount == 0)
    AnimClipData Load(const std::string& path);
}
