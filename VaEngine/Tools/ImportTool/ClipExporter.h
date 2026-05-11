#pragma once

#include "Converter.h"
#include <string>
#include <vector>

class ClipExporter
{
public:
    void ExportClips(const std::vector<AnimClipIntermediate>& clips,
                     const std::string& outputDir,
                     const std::string& stem);
};
