#include "AssetManager.h"
#include "../../Core/Logger.h"
#include <filesystem>
#include <mutex>
#include <fstream>
#include <sstream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

// stb_image implementation
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace AstralEngine {

AssetManager::AssetManager() {
    Logger::Debug("AssetManager", "AssetManager created");
}

AssetManager::~AssetManager() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("AssetManager", "AssetManager destroyed");
}

bool AssetManager::Initialize(const std::string& assetDirectory) {
    if (m_initialized) {
        Logger::Warning("AssetManager", "AssetManager already initialized");
        return true;
    }
    
    m_assetDirectory = assetDirectory;
    
    // Asset dizininin var olup olmadığını kontrol et
    if (!std::filesystem::exists(assetDirectory)) {
        Logger::Error("AssetManager", "Asset directory does not exist: '{}'", assetDirectory);
        return false;
    }
    
    if (!std::filesystem::is_directory(assetDirectory)) {
        Logger::Error("AssetManager", "Asset path is not a directory: '{}'", assetDirectory);
        return false;
    }

    // Thread pool'u başlat
    size_t num_threads = std::max(1u, std::thread::hardware_concurrency());
    try {
        m_threadPool = std::make_unique<ThreadPool>(num_threads);
        Logger::Info("AssetManager", "ThreadPool initialized with {} threads.", num_threads);
    } catch (const std::exception& e) {
        Logger::Error("AssetManager", "Failed to initialize ThreadPool: {}", e.what());
        return false;
    }
    
    m_initialized = true;
    Logger::Info("AssetManager", "AssetManager initialized with directory: '{}'", assetDirectory);
    
    return true;
}

void AssetManager::Update() {
    if (!m_initialized) return;

    ProcessGpuUploadQueue();
}

void AssetManager::ProcessGpuUploadQueue() {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    while (!m_gpuUploadQueue.empty()) {
        auto& uploadTask = m_gpuUploadQueue.front();
        uploadTask();
        m_gpuUploadQueue.pop();
    }
}

void AssetManager::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    Logger::Info("AssetManager", "Shutting down AssetManager...");
    
    if (m_threadPool) {
        Logger::Debug("AssetManager", "Stopping ThreadPool...");
        m_threadPool.reset();
        Logger::Info("AssetManager", "ThreadPool stopped.");
    }

    ClearAssetCache();
    m_registry.ClearAll();
    m_initialized = false;
    
    Logger::Info("AssetManager", "AssetManager shutdown complete");
}

std::shared_ptr<ModelData> AssetManager::LoadModel(const std::string& filePath) {
    std::string fullPath = GetFullPath(filePath);
    if (!std::filesystem::exists(fullPath)) {
        Logger::Error("AssetManager", "Model file not found: '{}'", fullPath);
        return nullptr;
    }
    
    Logger::Info("AssetManager", "Loading ModelData from file: '{}'", fullPath);
    
    auto modelData = std::make_shared<ModelData>(fullPath);
    
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fullPath, 
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | 
        aiProcess_GenNormals | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        Logger::Error("AssetManager", "Assimp failed to load model '{}': {}", fullPath, importer.GetErrorString());
        return nullptr;
    }
    
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[i];
        if (!mesh->HasPositions()) continue;

        std::vector<Vertex> vertices;
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
            Vertex vertex;
            vertex.position = {mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z};
            if (mesh->HasNormals()) vertex.normal = {mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z};
            if (mesh->HasTextureCoords(0)) vertex.texCoord = {mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y};
            if (mesh->HasTangentsAndBitangents()) {
                vertex.tangent = {mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z};
                vertex.bitangent = {mesh->mBitangents[j].x, mesh->mBitangents[j].y, mesh->mBitangents[j].z};
            }
            vertices.push_back(vertex);
        }

        std::vector<uint32_t> indices;
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j) {
            const aiFace& face = mesh->mFaces[j];
            if (face.mNumIndices != 3) continue;
            for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                indices.push_back(face.mIndices[k]);
            }
        }
        modelData->vertices.insert(modelData->vertices.end(), vertices.begin(), vertices.end());
        modelData->indices.insert(modelData->indices.end(), indices.begin(), indices.end());
    }
    
    if (modelData->vertices.empty()) {
        Logger::Error("AssetManager", "No valid geometry found in model '{}'", fullPath);
        return nullptr;
    }

    // Modelin sınırlayıcı kutusunu (AABB) hesapla
    AABB boundingBox;
    for (const auto& vertex : modelData->vertices) {
        boundingBox.Extend(vertex.position);
    }
    modelData->boundingBox = boundingBox;
    Logger::Info("AssetManager", "Calculated bounding box for model '{}': Min({:.2f}, {:.2f}, {:.2f}), Max({:.2f}, {:.2f}, {:.2f})", 
        fullPath, boundingBox.min.x, boundingBox.min.y, boundingBox.min.z, boundingBox.max.x, boundingBox.max.y, boundingBox.max.z);
    
    modelData->isValid = true;
    modelData->name = std::filesystem::path(fullPath).filename().string();
    return modelData;
}

std::shared_ptr<TextureData> AssetManager::LoadTexture(const std::string& filePath) {
    std::string fullPath = GetFullPath(filePath);
    if (!std::filesystem::exists(fullPath)) {
        Logger::Error("AssetManager", "Texture file not found: '{}'", fullPath);
        return nullptr;
    }
    
    Logger::Info("AssetManager", "Loading TextureData from file: '{}'", fullPath);
    
    auto textureData = std::make_shared<TextureData>(fullPath);
    int width, height, channels;
    stbi_uc* data = stbi_load(fullPath.c_str(), &width, &height, &channels, 0);
    
    if (!data) {
        Logger::Error("AssetManager", "Failed to load texture '{}': {}", fullPath, stbi_failure_reason());
        return nullptr;
    }
    
    if (!textureData->Allocate(width, height, channels)) {
        Logger::Error("AssetManager", "Failed to allocate memory for texture '{}'", fullPath);
        stbi_image_free(data);
        return nullptr;
    }
    
    memcpy(textureData->data, data, width * height * channels);
    stbi_image_free(data);
    
    textureData->isValid = true;
    textureData->name = std::filesystem::path(fullPath).filename().string();
    return textureData;
}

std::shared_ptr<ShaderData> AssetManager::LoadShader(const std::string& filePath, ShaderData::Type type) {
    std::string fullPath = GetFullPath(filePath);
    if (!std::filesystem::exists(fullPath)) {
        Logger::Error("AssetManager", "Shader file not found: '{}'", fullPath);
        return nullptr;
    }
    
    Logger::Info("AssetManager", "Loading ShaderData from SPIR-V file: '{}'", fullPath);
    
    auto shaderData = std::make_shared<ShaderData>(fullPath, type);
    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        Logger::Error("AssetManager", "Failed to open shader file '{}'", fullPath);
        return nullptr;
    }
    
    size_t fileSize = file.tellg();
    file.seekg(0);
    
    // SPIR-V kodu 4-byte words olarak saklanır, bu yüzden boyutun 4'ün katı olduğundan emin olalım
    if (fileSize % sizeof(uint32_t) != 0) {
        Logger::Error("AssetManager", "SPIR-V file size is not a multiple of 4 bytes: '{}'", fullPath);
        return nullptr;
    }
    
    shaderData->spirvCode.resize(fileSize / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(shaderData->spirvCode.data()), fileSize);
    
    shaderData->isValid = true;
    shaderData->name = std::filesystem::path(fullPath).filename().string();
    return shaderData;
}

std::shared_ptr<MaterialData> AssetManager::LoadMaterial(const std::string& filePath) {
    std::string fullPath = GetFullPath(filePath);
    if (!std::filesystem::exists(fullPath)) {
        Logger::Error("AssetManager", "Material file not found: '{}'", fullPath);
        return nullptr;
    }
    
    Logger::Info("AssetManager", "Loading MaterialData from file: '{}'", fullPath);
    
    auto materialData = std::make_shared<MaterialData>(fullPath);
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        Logger::Error("AssetManager", "Failed to open material file '{}'", fullPath);
        return nullptr;
    }
    
    try {
        nlohmann::json materialJson;
        file >> materialJson;
        materialData->name = materialJson.value("name", std::filesystem::path(fullPath).filename().string());
        materialData->vertexShaderPath = materialJson.value("vertexShader", "");
        materialData->fragmentShaderPath = materialJson.value("fragmentShader", "");
        // Property and texture loading logic here...
    } catch (const std::exception& e) {
        Logger::Error("AssetManager", "Failed to parse JSON material file '{}': {}", fullPath, e.what());
        return nullptr;
    }
    
    materialData->isValid = true;
    return materialData;
}

AssetHandle AssetManager::RegisterAsset(const std::string& filePath, AssetHandle::Type type) {
    if (!m_initialized) {
        Logger::Error("AssetManager", "Cannot register asset: AssetManager not initialized");
        return AssetHandle();
    }
    
    AssetHandle handle(filePath, type);
    if (!handle.IsValid()) {
        Logger::Error("AssetManager", "Failed to create asset handle for: {}", filePath);
        return AssetHandle();
    }
    
    if (!m_registry.RegisterAsset(handle, filePath, type)) {
        // Already registered is not an error, just return the handle.
        return handle;
    }
    
    Logger::Debug("AssetManager", "Asset registered: {} (ID: {}, Type: {})", filePath, handle.GetID(), static_cast<int>(type));
    return handle;
}

bool AssetManager::UnloadAsset(const AssetHandle& handle) {
    if (!handle.IsValid()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_assetHandleCache.find(handle);
    if (it != m_assetHandleCache.end()) {
        Logger::Debug("AssetManager", "Unloading asset: {}", handle.GetID());
        m_assetHandleCache.erase(it);
        m_registry.SetAssetState(handle, AssetLoadState::Unloaded);
        return true;
    }
    
    return false;
}

void AssetManager::LoadAssetAsync(const AssetHandle& handle) {
    if (!m_initialized || !m_threadPool || !handle.IsValid()) {
        return;
    }

    const AssetMetadata* metadata = m_registry.GetMetadata(handle);
    if (!metadata) {
        Logger::Error("AssetManager", "RequestLoad failed: Asset metadata not found for handle {}", handle.GetID());
        return;
    }

    m_registry.SetAssetState(handle, AssetLoadState::Queued);

    m_threadPool->Submit([this, handle, filePath = metadata->filePath, type = metadata->type]() {
        m_registry.SetAssetState(handle, AssetLoadState::Loading);

        std::shared_ptr<void> cpuData;
        switch (type) {
            case AssetHandle::Type::Model:
                cpuData = LoadModel(filePath);
                break;
            case AssetHandle::Type::Texture:
                cpuData = LoadTexture(filePath);
                break;
            case AssetHandle::Type::Shader:
                cpuData = LoadShader(filePath);
                break;
            case AssetHandle::Type::Material:
                cpuData = LoadMaterial(filePath);
                break;
            default:
                Logger::Error("AssetManager", "Unsupported asset type for async load: {}", static_cast<int>(type));
                m_registry.SetAssetError(handle, "Unsupported asset type");
                m_registry.SetAssetState(handle, AssetLoadState::Failed);
                return;
        }

        if (!cpuData) {
            m_registry.SetAssetError(handle, "Failed to load asset from disk");
            m_registry.SetAssetState(handle, AssetLoadState::Failed);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_gpuUploadQueue.push([this, handle, cpuData, type]() {
                // This part runs on the main thread
                std::shared_ptr<void> gpuResource;

                // TODO: Replace with actual GPU resource creation
                // For now, we'll just store the CPU data in the cache
                gpuResource = cpuData;

                if (gpuResource) {
                    std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
                    // Create a promise, set its value, and store the shared_future in the cache
                    std::promise<std::shared_ptr<void>> promise;
                    promise.set_value(gpuResource);
                    m_assetHandleCache[handle] = promise.get_future().share();
                    m_registry.SetAssetState(handle, AssetLoadState::Loaded);
                    Logger::Info("AssetManager", "Asset {} is loaded and ready.", handle.GetID());
                } else {
                    m_registry.SetAssetError(handle, "Failed to create GPU resource");
                    m_registry.SetAssetState(handle, AssetLoadState::Failed);
                }
            });
        }
    });
}

template<typename T>
bool AssetManager::IsAssetLoaded(const AssetHandle& handle) const {
    if (!handle.IsValid()) {
        return false;
    }
    return m_registry.GetAssetState(handle) == AssetLoadState::Loaded;
}

AssetLoadState AssetManager::GetAssetState(const AssetHandle& handle) const {
    if (!handle.IsValid()) {
        return AssetLoadState::NotLoaded;
    }
    if (!m_registry.IsAssetRegistered(handle)) {
        return AssetLoadState::NotLoaded;
    }
    return m_registry.GetAssetState(handle);
}

float AssetManager::GetAssetLoadProgress(const AssetHandle& handle) const {
    if (!handle.IsValid()) return 0.0f;
    return m_registry.GetLoadProgress(handle);
}

std::string AssetManager::GetAssetError(const AssetHandle& handle) const {
    if (!handle.IsValid()) return "Invalid asset handle";
    return m_registry.GetAssetError(handle);
}

void AssetManager::ClearAssetCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    Logger::Info("AssetManager", "Clearing asset handle cache. {} assets removed", m_assetHandleCache.size());
    m_assetHandleCache.clear();
}

size_t AssetManager::GetLoadedAssetCountByType([[maybe_unused]] AssetHandle::Type type) const {
    return m_registry.GetAssetCountByState(AssetLoadState::Loaded);
}

size_t AssetManager::GetLoadedAssetCount() const {
    return m_registry.GetAssetCountByState(AssetLoadState::Loaded);
}

size_t AssetManager::GetMemoryUsage() const {
    return m_registry.GetTotalMemoryUsage();
}

std::string AssetManager::GetFullPath(const std::string& relativePath) const {
    return m_assetDirectory + "/" + relativePath;
}

void AssetManager::RequestLoad(const AssetHandle& handle) {
    if (!m_initialized || !m_threadPool || !handle.IsValid()) {
        return;
    }

    const AssetMetadata* metadata = m_registry.GetMetadata(handle);
    if (!metadata) {
        Logger::Error("AssetManager", "RequestLoad failed: Asset metadata not found for handle {}", handle.GetID());
        return;
    }

    // Check if already loaded or loading
    AssetLoadState currentState = m_registry.GetAssetState(handle);
    if (currentState == AssetLoadState::Loaded || currentState == AssetLoadState::Loading || currentState == AssetLoadState::Queued) {
        return;
    }

    LoadAssetAsync(handle);
}

std::string AssetManager::NormalizePath(const std::string& path) const {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

} // namespace AstralEngine
