#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

inline std::vector<uint8_t> LoadBinaryFile(const wchar_t* path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Cannot open binary file: " + std::filesystem::path(path).string());

    const auto size = static_cast<std::size_t>(file.tellg());
    file.seekg(0);

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    return data;
}
