#include "TextureImporter.h"
#include "AssetData.h"
#include "../../Core/Logger.h"

#include <filesystem>

#include <stb_image.h>

namespace AstralEngine {

	std::shared_ptr<void> TextureImporter::Import(const std::string& filePath) {
		Logger::Trace("TextureImporter", "Loading TextureData from file: '{}'", filePath);

		auto textureData = std::make_shared<TextureData>(filePath);
		int width, height, channels;

		// Force loading as RGBA (4 channels)
		stbi_uc* data = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

		if (!data) {
			Logger::Error("TextureImporter", "Failed to load texture '{}': {}", filePath, stbi_failure_reason());
			return nullptr;
		}

		// Since we forced RGBA, channels will be 4 in the output buffer, 
		// but stbi_load returns the original channels in the 'channels' variable.
		// We should update it to 4 to reflect the data we have.
		channels = 4;

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
