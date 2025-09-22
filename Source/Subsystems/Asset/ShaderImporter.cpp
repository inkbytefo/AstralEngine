#include "ShaderImporter.h"
#include "AssetData.h"
#include "../../Core/Logger.h"

#include <fstream>
#include <filesystem>

namespace AstralEngine {

	std::shared_ptr<void> ShaderImporter::Import(const std::string& filePath) {
		Logger::Trace("ShaderImporter", "Loading ShaderData from SPIR-V file: '{}'", filePath);

		auto shaderData = std::make_shared<ShaderData>(filePath);
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			Logger::Error("ShaderImporter", "Failed to open shader file '{}'", filePath);
			return nullptr;
		}

		size_t fileSize = (size_t)file.tellg();
		if (fileSize == 0) {
			Logger::Error("ShaderImporter", "Shader file is empty: '{}'", filePath);
			return nullptr;
		}

		// SPIR-V code is stored as 32-bit words
		if (fileSize % sizeof(uint32_t) != 0) {
			Logger::Error("ShaderImporter", "SPIR-V file size is not a multiple of 4 bytes: '{}'", filePath);
			return nullptr;
		}

		shaderData->spirvCode.resize(fileSize / sizeof(uint32_t));
		file.seekg(0);
		file.read(reinterpret_cast<char*>(shaderData->spirvCode.data()), fileSize);
		file.close();

		shaderData->isValid = true;
		shaderData->name = std::filesystem::path(filePath).filename().string();

		Logger::Info("ShaderImporter", "Successfully loaded shader '{}'", shaderData->name);

		return shaderData;
	}

} // namespace AstralEngine
