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
		bool isHDR = stbi_is_hdr(filePath.c_str());

		void* data = nullptr;
		uint32_t bpc = 1;

		if (isHDR) {
			data = stbi_loadf(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
			bpc = 4;
			textureData->isHDR = true;
		} else {
			data = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
			bpc = 1;
			textureData->isHDR = false;
		}

		if (!data) {
			Logger::Error("TextureImporter", "Failed to load texture '{}': {}", filePath, stbi_failure_reason());
			return nullptr;
		}

		// Since we forced RGBA, channels will be 4 in the output buffer
		channels = 4;

		if (!textureData->Allocate(width, height, channels, bpc)) {
			Logger::Error("TextureImporter", "Failed to allocate memory for texture '{}'", filePath);
			stbi_image_free(data);
			return nullptr;
		}

		memcpy(textureData->data, data, (size_t)width * height * channels * bpc);
		stbi_image_free(data);

		textureData->isValid = true;
		textureData->name = std::filesystem::path(filePath).filename().string();

		Logger::Info("TextureImporter", "Successfully loaded texture '{}' ({}x{}, {} channels, HDR: {})", 
					 textureData->name, width, height, channels, isHDR);

		return textureData;
	}

} // namespace AstralEngine
