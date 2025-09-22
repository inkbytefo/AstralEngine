#include "MaterialImporter.h"
#include "AssetManager.h"
#include "AssetData.h"
#include "../../Core/Logger.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace AstralEngine {

	MaterialImporter::MaterialImporter(AssetManager* owner)
		: m_ownerManager(owner) {
	}

	std::shared_ptr<void> MaterialImporter::Import(const std::string& filePath) {
		Logger::Trace("MaterialImporter", "Loading MaterialData from file: '{}'", filePath);

		std::ifstream file(filePath);
		if (!file.is_open()) {
			Logger::Error("MaterialImporter", "Failed to open material file '{}'", filePath);
			return nullptr;
		}

		auto materialData = std::make_shared<MaterialData>(filePath);

		try {
			nlohmann::json materialJson;
			file >> materialJson;

			materialData->name = materialJson.value("name", std::filesystem::path(filePath).stem().string());

			// Load shaders and queue them for loading
			if (materialJson.contains("vertexShader")) {
				materialData->vertexShaderPath = materialJson["vertexShader"].get<std::string>();
				m_ownerManager->RegisterAsset(materialData->vertexShaderPath);
			}
			if (materialJson.contains("fragmentShader")) {
				materialData->fragmentShaderPath = materialJson["fragmentShader"].get<std::string>();
				m_ownerManager->RegisterAsset(materialData->fragmentShaderPath);
			}

			// Load textures and queue them for loading
			if (materialJson.contains("textures")) {
				for (const auto& item : materialJson["textures"].items()) {
					// We can add more sophisticated mapping later (e.g., "albedo", "normal")
					std::string texturePath = item.value().get<std::string>();
					materialData->texturePaths.push_back(texturePath);
					m_ownerManager->RegisterAsset(texturePath);
				}
			}

			// Load properties
			if (materialJson.contains("properties")) {
				const auto& props = materialJson["properties"];
				// Example of loading a property
				if (props.contains("baseColor")) {
					materialData->properties.baseColor = { props["baseColor"][0], props["baseColor"][1], props["baseColor"][2] };
				}
				materialData->properties.metallic = props.value("metallic", 0.0f);
				materialData->properties.roughness = props.value("roughness", 0.5f);
				// ... load other properties
			}

		} catch (const nlohmann::json::exception& e) {
			Logger::Error("MaterialImporter", "Failed to parse JSON material file '{}': {}", filePath, e.what());
			return nullptr;
		}

		materialData->isValid = true;
		Logger::Info("MaterialImporter", "Successfully loaded material '{}' and queued its dependencies.", materialData->name);

		return materialData;
	}

} // namespace AstralEngine
