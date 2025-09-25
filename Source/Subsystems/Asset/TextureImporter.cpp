#include "TextureImporter.h"
#include "AssetData.h"
#include "../../Core/Logger.h"

#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "../../ThirdParty/stb_image.h"

namespace AstralEngine {

	std::shared_ptr<void> TextureImporter::Import(const std::string& filePath) {
		Logger::Trace("TextureImporter", "Loading TextureData from file: '{}'", filePath);

		auto textureData = std::make_shared<TextureData>(filePath);
		int width, height, channels;

		stbi_uc* data = stbi_load(filePath.c_str(), &width, &height, &channels, 0);

		if (!data) {
			Logger::Error("TextureImporter", "Failed to load texture '{}': {}", filePath, stbi_failure_reason());
			return nullptr;
		}

		if (!textureData->Allocate(width, height, channels)) {
			Logger::Error("TextureImporter", "Failed to allocate memory for texture '{}'", filePath);
			stbi_image_free(data);
			return nullptr;
		}

		memcpy(textureData->data, data, (size_t)width * height * channels);
		stbi_image_free(data);

		textureData->isValid = true;
		textureData->name = std::filesystem::path(filePath).filename().string();

		Logger::Info("TextureImporter", "Successfully loaded texture '{}' ({}x{}, {} channels)", 
					 textureData->name, width, height, channels);

		return textureData;
	}

} // namespace AstralEngine
