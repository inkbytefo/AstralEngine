#include "AssetManager.h"
#include "Subsystems/Renderer/Shaders/VulkanShader.h"
#include "Subsystems/Renderer/Core/VulkanDevice.h"
#include "Subsystems/Renderer/Buffers/VulkanMesh.h"
#include "Subsystems/Renderer/Buffers/VulkanTexture.h"
#include "../../Core/Logger.h"
#include <filesystem>
#include <mutex>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include "Subsystems/Renderer/RendererTypes.h"

// stb_image implementation
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// Forward declarations for Vulkan device
namespace AstralEngine {
class VulkanDevice;
}

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
    
    m_initialized = true;
    Logger::Info("AssetManager", "AssetManager initialized with directory: '{}'", assetDirectory);
    
    return true;
}

void AssetManager::Update() {
    // TODO: Async yükleme operasyonlarını kontrol et
    // Future'ları check et, tamamlananları callback'leri çağır
    
    // Asset registry'deki durumları güncelle
    // Burada ileride async loading işlemleri yönetilecek
}

void AssetManager::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    Logger::Info("AssetManager", "Shutting down AssetManager. Loaded assets: {}, Memory usage: {} bytes", 
                GetLoadedAssetCount(), GetMemoryUsage());
    
    ClearCache();
    ClearAssetCache();
    m_registry.ClearAll();
    m_initialized = false;
    
    Logger::Info("AssetManager", "AssetManager shutdown complete");
}

std::shared_ptr<Model> AssetManager::LoadModel(const std::string& filePath) {
    // Device parametresi almayan versiyon, log uyarısı verip nullptr döndür
    Logger::Warning("AssetManager", "LoadModel called without device parameter. Use LoadModel(filePath, device) for 3D model loading.");
    return nullptr;
}

std::shared_ptr<Model> AssetManager::LoadModel(const std::string& filePath, VulkanDevice* device) {
    if (!m_initialized) {
        Logger::Error("AssetManager", "Cannot load model: AssetManager not initialized");
        return nullptr;
    }
    
    if (!device) {
        Logger::Error("AssetManager", "Cannot load model: VulkanDevice is null");
        return nullptr;
    }
    
    std::string normalizedPath = NormalizePath(filePath);
    
    // Cache'den kontrol et
    auto cached = GetFromCache<Model>(normalizedPath);
    if (cached) {
        Logger::Debug("AssetManager", "Model loaded from cache: '{}'", filePath);
        return cached;
    }
    
    // Dosyadan yükle
    std::string fullPath = GetFullPath(normalizedPath);
    if (!std::filesystem::exists(fullPath)) {
        Logger::Error("AssetManager", "Model file not found: '{}'", fullPath);
        return nullptr;
    }
    
    Logger::Info("AssetManager", "Loading model from file: '{}'", fullPath);
    
    // Assimp ile modeli yükle
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fullPath, 
        aiProcess_Triangulate |           // Üçgenlere dönüştür
        aiProcess_FlipUVs |              // Texture koordinatlarını flip et
        aiProcess_CalcTangentSpace |     // Tangent space hesapla
        aiProcess_GenNormals |           // Normal vektörleri oluştur
        aiProcess_JoinIdenticalVertices | // Aynı vertex'leri birleştir
        aiProcess_ImproveCacheLocality); // Cache locality'i iyileştir
    
    // Yükleme kontrolü
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        Logger::Error("AssetManager", "Assimp failed to load model '{}': {}", 
                     fullPath, importer.GetErrorString());
        return nullptr;
    }
    
    if (scene->mNumMeshes == 0) {
        Logger::Warning("AssetManager", "Model '{}' contains no meshes", fullPath);
        return nullptr;
    }
    
    // Model nesnesini oluştur
    auto model = std::make_shared<Model>(fullPath);
    
    // Tüm mesh'leri işle
    size_t totalVertices = 0;
    size_t totalIndices = 0;
    
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[i];
        
        if (!mesh->HasPositions()) {
            Logger::Warning("AssetManager", "Mesh {} has no positions, skipping", i);
            continue;
        }
        
        Logger::Debug("AssetManager", "Processing mesh {}: {} vertices, {} faces", 
                     i, mesh->mNumVertices, mesh->mNumFaces);
        
        // Vertex ve index verilerini hazırla
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        
        // Vertex verilerini dönüştür
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
            Vertex vertex;
            
            // Position (3D)
            vertex.pos = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
            
            // Color (varsayılan beyaz, veya vertex colors varsa kullan)
            if (mesh->HasVertexColors(0)) {
                vertex.color = glm::vec3(
                    mesh->mColors[0][j].r,
                    mesh->mColors[0][j].g,
                    mesh->mColors[0][j].b
                );
            } else {
                vertex.color = glm::vec3(1.0f, 1.0f, 1.0f); // Beyaz
            }
            
            // Texture coordinates (varsayılan sıfır, veya texture coords varsa kullan)
            if (mesh->HasTextureCoords(0)) {
                vertex.texCoord = glm::vec2(
                    mesh->mTextureCoords[0][j].x,
                    mesh->mTextureCoords[0][j].y
                );
            } else {
                vertex.texCoord = glm::vec2(0.0f, 0.0f); // Sıfır
            }
            
            vertices.push_back(vertex);
        }
        
        // Index verilerini dönüştür
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j) {
            const aiFace& face = mesh->mFaces[j];
            
            // Sadece üçgen yüzleri destekle
            if (face.mNumIndices != 3) {
                Logger::Warning("AssetManager", "Face {} has {} indices (expected 3), skipping", 
                               j, face.mNumIndices);
                continue;
            }
            
            for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                indices.push_back(face.mIndices[k]);
            }
        }
        
        if (vertices.empty() || indices.empty()) {
            Logger::Warning("AssetManager", "Mesh {} has no valid geometry, skipping", i);
            continue;
        }
        
        // VulkanMesh oluştur ve başlat
        auto vulkanMesh = std::make_shared<VulkanMesh>();
        if (!vulkanMesh->Initialize(device, vertices, indices)) {
            Logger::Error("AssetManager", "Failed to initialize VulkanMesh for mesh {}", i);
            continue;
        }
        
        // Model'e mesh'i ekle
        model->AddMesh(vulkanMesh);
        
        totalVertices += vertices.size();
        totalIndices += indices.size();
        
        Logger::Debug("AssetManager", "Mesh {} loaded successfully: {} vertices, {} indices", 
                     i, vertices.size(), indices.size());
    }
    
    // Model'in geçerliliğini kontrol et
    if (model->GetMeshCount() == 0) {
        Logger::Error("AssetManager", "No valid meshes found in model '{}'", fullPath);
        return nullptr;
    }
    
    // Model'i geçerli olarak işaretle
    // Model sınıfında bunu yapacak bir metod yok, doğrudan erişemeyiz
    // Bu yüzden sadece loglama yapıyoruz
    
    // Cache'e ekle
    size_t estimatedSize = std::filesystem::file_size(fullPath) + 
                          (totalVertices * sizeof(Vertex)) + 
                          (totalIndices * sizeof(uint32_t));
    AddToCache(normalizedPath, model, estimatedSize);
    
    Logger::Info("AssetManager", "Model loaded successfully: '{}', {} meshes, {} vertices, {} indices", 
                 filePath, model->GetMeshCount(), totalVertices, totalIndices);
    
    return model;
}

// Yeni AssetHandle tabanlı metodlar
AssetHandle AssetManager::RegisterAsset(const std::string& filePath, AssetHandle::Type type) {
    if (!m_initialized) {
        Logger::Error("AssetManager", "Cannot register asset: AssetManager not initialized");
        return AssetHandle();
    }
    
    // AssetHandle oluştur
    AssetHandle handle(filePath, type);
    if (!handle.IsValid()) {
        Logger::Error("AssetManager", "Failed to create asset handle for: {}", filePath);
        return AssetHandle();
    }
    
    // Registry'e kaydet
    if (!m_registry.RegisterAsset(handle, filePath, type)) {
        Logger::Error("AssetManager", "Failed to register asset: {}", filePath);
        return AssetHandle();
    }
    
    Logger::Debug("AssetManager", "Asset registered: {} (ID: {}, Type: {})", 
                 filePath, handle.GetID(), static_cast<int>(type));
    
    return handle;
}

bool AssetManager::LoadAsset(const AssetHandle& handle, VulkanDevice* device) {
    if (!m_initialized) {
        Logger::Error("AssetManager", "Cannot load asset: AssetManager not initialized");
        return false;
    }
    
    if (!handle.IsValid()) {
        Logger::Error("AssetManager", "Cannot load invalid asset handle");
        return false;
    }
    
    // Asset durumunu kontrol et
    AssetLoadState currentState = m_registry.GetAssetState(handle);
    if (currentState == AssetLoadState::Loaded) {
        Logger::Debug("AssetManager", "Asset already loaded: {}", handle.GetID());
        return true;
    }
    
    if (currentState == AssetLoadState::Loading) {
        Logger::Debug("AssetManager", "Asset already loading: {}", handle.GetID());
        return true;
    }
    
    // Yükleme durumunu güncelle
    m_registry.SetAssetState(handle, AssetLoadState::Loading);
    m_registry.SetLoadProgress(handle, 0.0f);
    
    const AssetMetadata* metadata = m_registry.GetMetadata(handle);
    if (!metadata) {
        Logger::Error("AssetManager", "Asset metadata not found: {}", handle.GetID());
        m_registry.SetAssetState(handle, AssetLoadState::Failed);
        return false;
    }
    
    // Asset tipine göre yükleme yap
    auto startTime = std::chrono::high_resolution_clock::now();
    bool success = false;
    
    try {
        switch (metadata->type) {
            case AssetHandle::Type::Model:
                if (device) {
                    auto model = LoadModel(metadata->filePath, device);
                    success = (model != nullptr);
                    if (success) {
                        std::lock_guard<std::mutex> lock(m_cacheMutex);
                        m_assetHandleCache[handle] = std::static_pointer_cast<void>(model);
                    }
                }
                break;
                
            case AssetHandle::Type::Texture:
                if (device) {
                    auto texture = LoadVulkanTexture(metadata->filePath, device);
                    success = (texture != nullptr);
                    if (success) {
                        std::lock_guard<std::mutex> lock(m_cacheMutex);
                        m_assetHandleCache[handle] = std::static_pointer_cast<void>(texture);
                    }
                }
                break;
                
            case AssetHandle::Type::Shader:
                if (device) {
                    auto shader = LoadShader(metadata->filePath, device);
                    success = (shader != nullptr);
                    if (success) {
                        std::lock_guard<std::mutex> lock(m_cacheMutex);
                        m_assetHandleCache[handle] = std::static_pointer_cast<void>(shader);
                    }
                }
                break;
                
            default:
                Logger::Error("AssetManager", "Unsupported asset type: {}", static_cast<int>(metadata->type));
                success = false;
                break;
        }
    } catch (const std::exception& e) {
        Logger::Error("AssetManager", "Exception while loading asset {}: {}", metadata->filePath, e.what());
        success = false;
    }
    
    // Yükleme süresini hesapla
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_registry.SetLoadTime(handle, duration);
    
    // Durumu güncelle
    if (success) {
        m_registry.SetAssetState(handle, AssetLoadState::Loaded);
        m_registry.SetLoadProgress(handle, 1.0f);
        m_registry.UpdateLastAccessTime(handle);
        
        // Bellek boyutunu güncelle (dosya boyutu + tahmini GPU bellek)
        size_t fileSize = std::filesystem::exists(metadata->filePath) ? 
                         std::filesystem::file_size(metadata->filePath) : 0;
        m_registry.SetMemorySize(handle, fileSize);
        
        Logger::Info("AssetManager", "Asset loaded successfully: {} ({:.2f} ms)", 
                     metadata->filePath, duration);
    } else {
        m_registry.SetAssetState(handle, AssetLoadState::Failed);
        m_registry.SetAssetError(handle, "Loading failed");
        Logger::Error("AssetManager", "Asset loading failed: {}", metadata->filePath);
    }
    
    return success;
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
    
    // Asset'in registered olup olmadığını kontrol et
    if (!m_registry.IsAssetRegistered(handle)) {
        Logger::Debug("AssetManager", "Asset not registered in registry: {} (type: {})", 
                     handle.GetID(), static_cast<int>(handle.GetType()));
        return AssetLoadState::NotLoaded;
    }
    
    return m_registry.GetAssetState(handle);
}

float AssetManager::GetAssetLoadProgress(const AssetHandle& handle) const {
    if (!handle.IsValid()) {
        return 0.0f;
    }
    
    return m_registry.GetLoadProgress(handle);
}

std::string AssetManager::GetAssetError(const AssetHandle& handle) const {
    if (!handle.IsValid()) {
        return "Invalid asset handle";
    }
    
    return m_registry.GetAssetError(handle);
}

void AssetManager::ClearAssetCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    Logger::Info("AssetManager", "Clearing asset handle cache. {} assets removed", 
                m_assetHandleCache.size());
    m_assetHandleCache.clear();
}

size_t AssetManager::GetLoadedAssetCountByType([[maybe_unused]] AssetHandle::Type type) const {
    return m_registry.GetAssetCountByState(AssetLoadState::Loaded);
}

// Template implementations for the new GetAssetFromCache method
template<typename T>
std::shared_ptr<T> AssetManager::GetAssetFromCache(const AssetHandle& handle) {
    return GetAsset<T>(handle);
}

std::shared_ptr<Texture> AssetManager::LoadTexture(const std::string& filePath) {
    if (!m_initialized) {
        Logger::Error("AssetManager", "Cannot load texture: AssetManager not initialized");
        return nullptr;
    }
    
    std::string normalizedPath = NormalizePath(filePath);
    
    // Cache'den kontrol et
    auto cached = GetFromCache<Texture>(normalizedPath);
    if (cached) {
        Logger::Debug("AssetManager", "Texture loaded from cache: '{}'", filePath);
        return cached;
    }
    
    // Dosyadan yükle
    std::string fullPath = GetFullPath(normalizedPath);
    if (!std::filesystem::exists(fullPath)) {
        Logger::Error("AssetManager", "Texture file not found: '{}'", fullPath);
        return nullptr;
    }
    
    // TODO: Gerçek texture yükleme implementasyonu (stb_image, FreeImage vs.)
    auto texture = std::make_shared<Texture>(fullPath);
    
    // Cache'e ekle
    size_t estimatedSize = std::filesystem::file_size(fullPath);
    AddToCache(normalizedPath, texture, estimatedSize);
    
    Logger::Info("AssetManager", "Texture loaded successfully: '{}'", filePath);
    return texture;
}

std::shared_ptr<VulkanTexture> AssetManager::LoadVulkanTexture(const std::string& filePath, VulkanDevice* device) {
    if (!m_initialized) {
        Logger::Error("AssetManager", "Cannot load texture: AssetManager not initialized");
        return nullptr;
    }
    
    if (!device) {
        Logger::Error("AssetManager", "Cannot load texture: VulkanDevice is null");
        return nullptr;
    }
    
    std::string normalizedPath = NormalizePath(filePath);
    
    // Cache'den kontrol et
    auto cached = GetFromCache<VulkanTexture>(normalizedPath);
    if (cached) {
        Logger::Debug("AssetManager", "VulkanTexture loaded from cache: '{}'", filePath);
        return cached;
    }
    
    // Dosyadan yükle
    std::string fullPath = GetFullPath(normalizedPath);
    if (!std::filesystem::exists(fullPath)) {
        Logger::Error("AssetManager", "Texture file not found: '{}'", fullPath);
        return nullptr;
    }
    
    Logger::Info("AssetManager", "Loading VulkanTexture from file: '{}'", fullPath);
    
    // VulkanTexture oluştur ve başlat
    auto texture = std::make_shared<VulkanTexture>();
    if (!texture->Initialize(device, fullPath)) {
        Logger::Error("AssetManager", "Failed to initialize VulkanTexture '{}': {}", 
                     fullPath, texture->GetLastError());
        return nullptr;
    }
    
    // Cache'e ekle
    size_t estimatedSize = std::filesystem::file_size(fullPath);
    AddToCache(normalizedPath, texture, estimatedSize);
    
    Logger::Info("AssetManager", "VulkanTexture loaded successfully: '{}'", filePath);
    return texture;
}

std::shared_ptr<ShaderProgram> AssetManager::LoadShader(const std::string& shaderName, VulkanDevice* device) {
    if (!m_initialized) {
        Logger::Error("AssetManager", "Cannot load shader: AssetManager not initialized");
        return nullptr;
    }
    
    if (!device) {
        Logger::Error("AssetManager", "Cannot load shader: VulkanDevice is null");
        return nullptr;
    }
    
    // Shader adını normalize et
    std::string normalizedName = NormalizePath(shaderName);
    
    // Cache'den kontrol et
    auto cached = GetFromCache<ShaderProgram>(normalizedName);
    if (cached) {
        Logger::Debug("AssetManager", "Shader program loaded from cache: '{}'", shaderName);
        return cached;
    }
    
    // Shader yollarını oluştur
    std::string vertexPath = "Shaders/" + normalizedName + ".vert";
    std::string fragmentPath = "Shaders/" + normalizedName + ".frag";
    
    // Dosyalardan yükle
    std::string fullVertexPath = GetFullPath(vertexPath);
    std::string fullFragmentPath = GetFullPath(fragmentPath);
    
    // .spv dosyalarını kontrol et
    std::string vertexSpvPath = "Shaders/" + normalizedName + ".vert.spv";
    std::string fragmentSpvPath = "Shaders/" + normalizedName + ".frag.spv";
    std::string fullVertexSpvPath = GetFullPath(vertexSpvPath);
    std::string fullFragmentSpvPath = GetFullPath(fragmentSpvPath);
    
    // Eğer .spv dosyaları varsa onları kullan
    bool useCompiledShaders = false;
    if (std::filesystem::exists(fullVertexSpvPath) && std::filesystem::exists(fullFragmentSpvPath)) {
        fullVertexPath = fullVertexSpvPath;
        fullFragmentPath = fullFragmentSpvPath;
        useCompiledShaders = true;
        Logger::Debug("AssetManager", "Using compiled shader files (.spv) for '{}'", shaderName);
    } else {
        Logger::Debug("AssetManager", "Using source shader files (.vert/.frag) for '{}'", shaderName);
    }
    
    if (!std::filesystem::exists(fullVertexPath)) {
        Logger::Error("AssetManager", "Vertex shader file not found: '{}'", fullVertexPath);
        return nullptr;
    }
    
    if (!std::filesystem::exists(fullFragmentPath)) {
        Logger::Error("AssetManager", "Fragment shader file not found: '{}'", fullFragmentPath);
        return nullptr;
    }
    
    // Vertex shader'ı yükle ve başlat
    auto vertexShader = std::make_shared<VulkanShader>();
    VulkanShader::Config vertexConfig;
    vertexConfig.filePath = fullVertexPath;
    vertexConfig.stage = VK_SHADER_STAGE_VERTEX_BIT;
    
    if (!vertexShader->Initialize(device, vertexConfig)) {
        Logger::Error("AssetManager", "Failed to initialize vertex shader '{}': {}", shaderName, vertexShader->GetLastError());
        return nullptr;
    }
    
    // Fragment shader'ı yükle ve başlat
    auto fragmentShader = std::make_shared<VulkanShader>();
    VulkanShader::Config fragmentConfig;
    fragmentConfig.filePath = fullFragmentPath;
    fragmentConfig.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    if (!fragmentShader->Initialize(device, fragmentConfig)) {
        Logger::Error("AssetManager", "Failed to initialize fragment shader '{}': {}", shaderName, fragmentShader->GetLastError());
        return nullptr;
    }
    
    // Shader program oluştur
    auto shaderProgram = std::make_shared<ShaderProgram>(vertexShader, fragmentShader);
    
    // Cache'e ekle
    size_t estimatedSize = std::filesystem::file_size(fullVertexPath) + 
                          std::filesystem::file_size(fullFragmentPath);
    AddToCache(normalizedName, shaderProgram, estimatedSize);
    
    Logger::Info("AssetManager", "Shader program '{}' loaded and initialized successfully", shaderName);
    return shaderProgram;
}

void AssetManager::LoadModelAsync(const std::string& filePath, LoadCallback callback) {
    (void)callback; // Suppress unused parameter warning
    // TODO: Thread pool ile async yükleme implementasyonu
    Logger::Warning("AssetManager", "Async model loading not implemented yet: '{}'", filePath);
}

void AssetManager::LoadTextureAsync(const std::string& filePath, LoadCallback callback) {
    (void)callback; // Suppress unused parameter warning
    // TODO: Thread pool ile async yükleme implementasyonu
    Logger::Warning("AssetManager", "Async texture loading not implemented yet: '{}'", filePath);
}

void AssetManager::UnloadAsset(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    auto it = m_assetCache.find(filePath);
    if (it != m_assetCache.end()) {
        Logger::Debug("AssetManager", "Unloading asset: '{}'", filePath);
        m_assetCache.erase(it);
    }
}

void AssetManager::ClearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    Logger::Info("AssetManager", "Clearing asset cache. {} assets removed", m_assetCache.size());
    m_assetCache.clear();
}

bool AssetManager::IsAssetLoaded(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    return m_assetCache.find(filePath) != m_assetCache.end();
}

size_t AssetManager::GetLoadedAssetCount() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    return m_assetCache.size();
}

size_t AssetManager::GetMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    size_t totalMemory = 0;
    for (const auto& pair : m_assetCache) {
        totalMemory += pair.second.memorySize;
    }
    
    return totalMemory;
}

std::string AssetManager::GetFullPath(const std::string& relativePath) const {
    return m_assetDirectory + "/" + relativePath;
}

std::string AssetManager::NormalizePath(const std::string& path) const {
    // Path normalize işlemleri (slash/backslash dönüşümü vs.)
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

} // namespace AstralEngine
