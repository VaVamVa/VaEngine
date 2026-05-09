#pragma once

#include "Converter.h"
#include <string>

class SkmExporter
{
public:
    bool ExportSkm(const SkinnedConvertResult& result,
                   const std::string& outputDir,
                   const std::string& stem);
};
