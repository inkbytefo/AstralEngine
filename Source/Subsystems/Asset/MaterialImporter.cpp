#include "MaterialImporter.h"
#include "../../Core/Logger.h"
#include "AssetData.h"
#include "AssetManager.h"


#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>


namespace AstralEngine {

MaterialImporter::MaterialImporter(AssetManager *owner)
    : m_ownerManager(owner) {}

std::shared_ptr<void> MaterialImporter::Import(const std::string &filePath) {
  Logger::Trace("MaterialImporter", "Loading MaterialData from file: '{}'",
                filePath);

  std::ifstream file(filePath);
  if (!file.is_open()) {
    Logger::Error("MaterialImporter", "Failed to open material file '{}'",
                  filePath);
    return nullptr;
  }

  auto materialData = std::make_shared<MaterialData>(filePath);

  try {
    nlohmann::json materialJson;
    file >> materialJson;

    materialData->name = materialJson.value(
        "name", std::filesystem::path(filePath).stem().string());

    // Load shaders and queue them for loading
    if (materialJson.contains("vertexShader")) {
      materialData->vertexShaderPath =
          materialJson["vertexShader"].get<std::string>();
      m_ownerManager->RegisterAsset(materialData->vertexShaderPath);
    }
    if (materialJson.contains("fragmentShader")) {
      materialData->fragmentShaderPath =
          materialJson["fragmentShader"].get<std::string>();
      m_ownerManager->RegisterAsset(materialData->fragmentShaderPath);
    }

    // Load textures and queue them for loading
    if (materialJson.contains("textures")) {
      auto &texParams = materialJson["textures"];
      // Support legacy array format for albedo (or just basic list)
      if (texParams.is_array()) {
        for (const auto &item : texParams.items()) {
          std::string texturePath = item.value().get<std::string>();
          materialData->texturePaths.push_back(texturePath);
          m_ownerManager->RegisterAsset(texturePath);
        }
        // Mapping legacy array[0] to albedo if valid
        if (!materialData->texturePaths.empty()) {
          materialData->albedoMapPath = materialData->texturePaths[0];
        }
      }
      // Support new PBR object format
      else if (texParams.is_object()) {
        if (texParams.contains("albedo")) {
          materialData->albedoMapPath = texParams["albedo"].get<std::string>();
          m_ownerManager->RegisterAsset(materialData->albedoMapPath);
        }
        if (texParams.contains("normal")) {
          materialData->normalMapPath = texParams["normal"].get<std::string>();
          m_ownerManager->RegisterAsset(materialData->normalMapPath);
        }
        if (texParams.contains("metallic")) {
          materialData->metallicMapPath =
              texParams["metallic"].get<std::string>();
          m_ownerManager->RegisterAsset(materialData->metallicMapPath);
        }
        if (texParams.contains("roughness")) {
          materialData->roughnessMapPath =
              texParams["roughness"].get<std::string>();
          m_ownerManager->RegisterAsset(materialData->roughnessMapPath);
        }
        if (texParams.contains("ao")) {
          materialData->aoMapPath = texParams["ao"].get<std::string>();
          m_ownerManager->RegisterAsset(materialData->aoMapPath);
        }
        if (texParams.contains("emissive")) {
          materialData->emissiveMapPath =
              texParams["emissive"].get<std::string>();
          m_ownerManager->RegisterAsset(materialData->emissiveMapPath);
        }
      }
    }

    // Load properties
    if (materialJson.contains("properties")) {
      const auto &props = materialJson["properties"];
      // Base Color
      if (props.contains("baseColor")) {
        auto bc = props["baseColor"];
        materialData->properties.baseColor = {bc[0], bc[1], bc[2]};
      }
      // Opacity
      materialData->properties.opacity = props.value("opacity", 1.0f);

      // PBR Factors
      materialData->properties.metallic = props.value("metallic", 0.0f);
      materialData->properties.roughness = props.value("roughness", 0.5f);
      materialData->properties.ao = props.value("ao", 1.0f);

      // Emissive
      if (props.contains("emissiveColor")) {
        auto ec = props["emissiveColor"];
        materialData->properties.emissiveColor = {ec[0], ec[1], ec[2]};
      }
      materialData->properties.emissiveIntensity =
          props.value("emissiveIntensity", 1.0f);
    }

  } catch (const nlohmann::json::exception &e) {
    Logger::Error("MaterialImporter",
                  "Failed to parse JSON material file '{}': {}", filePath,
                  e.what());
    return nullptr;
  }

  materialData->isValid = true;
  Logger::Info("MaterialImporter",
               "Successfully loaded material '{}' and queued its dependencies.",
               materialData->name);

  return materialData;
}

} // namespace AstralEngine
