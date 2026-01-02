#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include "Logger.h"

namespace AstralEngine {

class FileUtils {
public:
    static std::vector<uint8_t> ReadBinaryFile(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            Logger::Error("FileUtils", "Failed to open file: {}", filePath);
            return {};
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<uint8_t> buffer(fileSize);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        return buffer;
    }
};

} // namespace AstralEngine
