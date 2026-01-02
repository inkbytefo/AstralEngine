#include "AssetManager.h"
#include "../../Core/Logger.h"
#include "MaterialImporter.h"
#include "ModelImporter.h"
#include "ShaderImporter.h"
#include "TextureImporter.h"

// Other importers will be included as they are created

#include <filesystem>

namespace AstralEngine {

AssetManager::AssetManager() : m_initialized(false) {}

AssetManager::~AssetManager() {
  if (m_initialized) {
    Shutdown();
  }
}

bool AssetManager::Initialize(const std::string &assetDirectory) {
  if (m_initialized) {
    Logger::Warning("AssetManager", "AssetManager already initialized.");
    return true;
  }

  m_assetDirectory = assetDirectory;
  if (!std::filesystem::exists(m_assetDirectory)) {
    Logger::Error("AssetManager", "Asset directory does not exist: '{}'",
                  m_assetDirectory);
    return false;
  }

  size_t num_threads = std::max(1u, std::thread::hardware_concurrency());
  m_threadPool = std::make_unique<ThreadPool>(num_threads);

  RegisterImporters();

  m_initialized = true;
  Logger::Info("AssetManager", "AssetManager initialized with directory: '{}'",
               m_assetDirectory);
  return true;
}

void AssetManager::Shutdown() {
  if (!m_initialized) {
    return;
  }
  Logger::Info("AssetManager", "Shutting down AssetManager...");
  m_threadPool.reset(); // Shuts down the thread pool

  {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_assetCache.clear();
  }
  m_registry.ClearAll();
  m_importers.clear();

  m_initialized = false;
  Logger::Info("AssetManager", "AssetManager shutdown complete.");
}

void AssetManager::RegisterImporters() {
  RegisterImporter<TextureImporter>(AssetHandle::Type::Texture);
  RegisterImporter<ModelImporter>(AssetHandle::Type::Model);
  RegisterImporter<ShaderImporter>(AssetHandle::Type::Shader);
  m_importers[AssetHandle::Type::Material] =
      std::make_unique<MaterialImporter>(this);
  // Register other importers here
  // e.g., RegisterImporter<ShaderImporter>(AssetHandle::Type::Shader);
  // e.g., RegisterImporter<ShaderImporter>(AssetHandle::Type::Shader);
}

AssetHandle AssetManager::RegisterAsset(const std::string &filePath) {
  if (!m_initialized) {
    Logger::Error("AssetManager",
                  "Cannot register asset: AssetManager not initialized.");
    return AssetHandle();
  }

  AssetHandle::Type type = GetAssetTypeFromFileExtension(filePath);
  if (type == AssetHandle::Type::Unknown) {
    Logger::Warning("AssetManager",
                    "Cannot register asset with unknown type: {}", filePath);
    return AssetHandle();
  }

  AssetHandle handle(filePath, type);
  if (!m_registry.RegisterAsset(handle, filePath, type)) {
    // Already registered is not an error, just return the existing handle.
  }
  return handle;
}

bool AssetManager::UnloadAsset(const AssetHandle &handle) {
  if (!handle.IsValid()) {
    return false;
  }
  std::lock_guard<std::mutex> lock(m_cacheMutex);
  if (m_assetCache.erase(handle) > 0) {
    m_registry.SetAssetState(handle, AssetLoadState::Unloaded);
    Logger::Debug("AssetManager", "Unloaded asset '{}' from cache.",
                  handle.GetID());
    return true;
  }
  return false;
}

void AssetManager::LoadAssetAsync(const AssetHandle &handle) {
  if (!m_initialized || !m_threadPool || !handle.IsValid()) {
    return;
  }

  const AssetMetadata *metadata = m_registry.GetMetadata(handle);
  if (!metadata) {
    Logger::Error("AssetManager",
                  "Cannot load asset: Metadata not found for handle {}",
                  handle.GetID());
    return;
  }

  m_registry.SetAssetState(handle, AssetLoadState::Queued);

  // Create a promise and store its future in the cache
  std::promise<std::shared_ptr<void>> assetPromise;
  {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_assetCache[handle] = assetPromise.get_future().share();
  }

  // Submit the loading task to the thread pool
  m_threadPool->Submit([this, handle, metadata,
                        promise = std::move(assetPromise)]() mutable {
    m_registry.SetAssetState(handle, AssetLoadState::Loading);

    auto it = m_importers.find(metadata->type);
    if (it == m_importers.end()) {
      Logger::Error("AssetManager", "No importer registered for asset type: {}",
                    (int)metadata->type);
      m_registry.SetAssetState(handle, AssetLoadState::Failed);
      promise.set_value(nullptr); // Fulfill the promise with null
      return;
    }

    std::string fullPath = GetFullPath(metadata->filePath);
    std::shared_ptr<void> cpuData = it->second->Import(fullPath);

    if (!cpuData) {
      Logger::Error("AssetManager", "Importer failed to load asset: {}",
                    fullPath);
      m_registry.SetAssetState(handle, AssetLoadState::Failed);
      promise.set_value(nullptr); // Fulfill the promise with null
      return;
    }

    m_registry.SetAssetState(handle, AssetLoadState::Loaded_CPU);
    promise.set_value(cpuData); // Fulfill the promise with the loaded data
  });
}

bool AssetManager::IsAssetLoaded(const AssetHandle &handle) const {
  if (!handle.IsValid())
    return false;
  AssetLoadState state = m_registry.GetAssetState(handle);
  return state == AssetLoadState::Loaded_CPU || state == AssetLoadState::Loaded;
}

AssetLoadState AssetManager::GetAssetState(const AssetHandle &handle) const {
  if (!handle.IsValid())
    return AssetLoadState::NotLoaded;
  const AssetMetadata *metadata = m_registry.GetMetadata(handle);
  return metadata ? metadata->state.load() : AssetLoadState::NotLoaded;
}

const AssetMetadata *
AssetManager::GetMetadata(const AssetHandle &handle) const {
  return m_registry.GetMetadata(handle);
}

std::string AssetManager::GetFullPath(const std::string &relativePath) const {
  std::string path = relativePath;
  if (path.compare(0, 7, "Assets/") == 0) {
    path = path.substr(7);
  } else if (path.compare(0, 7, "assets/") == 0) {
    path = path.substr(7);
  }
  return (std::filesystem::path(m_assetDirectory) / path).string();
}

AssetHandle::Type
AssetManager::GetAssetTypeFromFileExtension(const std::string &filePath) const {
  std::string extension = std::filesystem::path(filePath).extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 ::tolower);

  if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
      extension == ".bmp" || extension == ".tga") {
    return AssetHandle::Type::Texture;
  }
  if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" ||
      extension == ".glb") {
    return AssetHandle::Type::Model;
  }
  if (extension == ".amat") { // Astral Material
    return AssetHandle::Type::Material;
  }
  if (extension == ".spv") { // SPIR-V Shader
    return AssetHandle::Type::Shader;
  }
  // Add other extensions here

  return AssetHandle::Type::Unknown;
}

void AssetManager::Update() {
  // Process loaded assets, handle garbage collection, etc.
}

void AssetManager::CheckForAssetChanges() {
  // Hot-reload logic
}

} // namespace AstralEngine