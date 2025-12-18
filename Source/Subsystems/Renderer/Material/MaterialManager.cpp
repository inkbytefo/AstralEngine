#include "MaterialManager.h"
#include "../../Asset/AssetManager.h"
#include "../../Asset/AssetData.h"
#include "../../Asset/ShaderImporter.h"
#include "../Core/VulkanDevice.h"
#include "../Shaders/VulkanShader.h"
#include "../../Core/Logger.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace AstralEngine {

// Static helper function to convert ShaderStage to VkShaderStageFlagBits
static VkShaderStageFlagBits ShaderStageToVulkanStage(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Compute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderStage::Geometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::TessControl:
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::TessEvaluation:
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case ShaderStage::Task:
            return VK_SHADER_STAGE_TASK_BIT_EXT;
        case ShaderStage::Mesh:
            return VK_SHADER_STAGE_MESH_BIT_EXT;
        default:
            return VK_SHADER_STAGE_VERTEX_BIT; // Default fallback
    }
}

// Static helper function to convert VkShaderStageFlagBits to ShaderStage
static ShaderStage VulkanStageToShaderStage(VkShaderStageFlagBits stage) {
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:
            return ShaderStage::Vertex;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            return ShaderStage::Fragment;
        case VK_SHADER_STAGE_COMPUTE_BIT:
            return ShaderStage::Compute;
        case VK_SHADER_STAGE_GEOMETRY_BIT:
            return ShaderStage::Geometry;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            return ShaderStage::TessControl;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            return ShaderStage::TessEvaluation;
        case VK_SHADER_STAGE_TASK_BIT_EXT:
            return ShaderStage::Task;
        case VK_SHADER_STAGE_MESH_BIT_EXT:
            return ShaderStage::Mesh;
        default:
            return ShaderStage::Vertex; // Default fallback
    }
}

// MaterialManager class implementation
MaterialManager::MaterialManager()
    : m_assetManager(nullptr)
    , m_vulkanDevice(nullptr)
    , m_initialized(false)
    , m_lastCacheCleanup(std::chrono::steady_clock::now()) {

    Logger::Debug("MaterialManager", "MaterialManager created");
}

MaterialManager::~MaterialManager() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("MaterialManager", "MaterialManager destroyed");
}

// ===== LIFECYCLE MANAGEMENT =====

bool MaterialManager::Initialize(VulkanDevice* vulkanDevice, AssetManager* assetManager, VulkanBindlessSystem* bindlessSystem) {
    if (m_initialized) {
        Logger::Warning("MaterialManager", "MaterialManager already initialized");
        return true;
    }

    if (!assetManager) {
        SetError("Invalid AssetManager pointer");
        return false;
    }

    if (!vulkanDevice) {
        SetError("Invalid VulkanDevice pointer");
        return false;
    }

    Logger::Info("MaterialManager", "Initializing MaterialManager with Bindless Support: {}", bindlessSystem != nullptr);

    m_assetManager = assetManager;
    m_vulkanDevice = vulkanDevice;
    m_bindlessSystem = bindlessSystem;

    // Initialize cache statistics
    m_cacheHits = 0;
    m_cacheMisses = 0;
    m_lastCacheCleanup = std::chrono::steady_clock::now();

    // Create default materials
    if (!CreateDefaultMaterials()) {
        Logger::Error("MaterialManager", "Failed to create default materials");
        return false;
    }

    m_initialized = true;
    Logger::Info("MaterialManager", "MaterialManager initialized successfully");
    return true;
}

void MaterialManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Logger::Info("MaterialManager", "Shutting down MaterialManager");

    // Clear shader cache first
    ClearShaderCache();

    // Clear materials
    {
        std::lock_guard<std::mutex> lock(m_materialMutex);
        m_materials.clear();
    }

    // Clear default materials
    m_defaultPBRMaterial.reset();
    m_defaultUnlitMaterial.reset();

    m_assetManager = nullptr;
    m_vulkanDevice = nullptr;
    m_initialized = false;
    m_lastError.clear();

    Logger::Info("MaterialManager", "MaterialManager shutdown completed");
}

void MaterialManager::Update() {
    if (!m_initialized) {
        return;
    }

    // Perform periodic cache cleanup
    auto now = std::chrono::steady_clock::now();
    auto timeSinceCleanup = std::chrono::duration_cast<std::chrono::minutes>(now - m_lastCacheCleanup);

    if (timeSinceCleanup >= m_cacheCleanupInterval) {
        CleanupUnusedShaders();
        m_lastCacheCleanup = now;
    }

    // Update cache statistics logging
    if (m_cacheHits + m_cacheMisses > 0) {
        float hitRate = static_cast<float>(m_cacheHits) / (m_cacheHits + m_cacheMisses) * 100.0f;
        Logger::Debug("MaterialManager", "Shader cache hit rate: {:.2f}% ({} hits, {} misses)",
                     hitRate, m_cacheHits, m_cacheMisses);
    }
}

// ===== SHADER MANAGEMENT =====

std::shared_ptr<IShader> MaterialManager::LoadShader(const AssetHandle& handle, ShaderStage stage) {
    if (!handle.IsValid()) {
        SetError("Invalid AssetHandle provided");
        return nullptr;
    }

    if (!m_initialized) {
        SetError("MaterialManager not initialized");
        return nullptr;
    }

    Logger::Debug("MaterialManager", "Loading shader from handle: {} (stage: {})",
                 handle.GetID(), static_cast<int>(stage));

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(m_shaderCacheMutex);
        auto it = m_shaderCache.find(handle);
        if (it != m_shaderCache.end()) {
            // Return cached shader
            m_cacheHits++;
            Logger::Debug("MaterialManager", "Shader found in cache: {}", handle.GetID());
            return it->second.shader;
        }
    }

    m_cacheMisses++;

    // Load shader data from asset manager
    std::vector<uint32_t> shaderData = LoadShaderData(handle);
    if (shaderData.empty()) {
        Logger::Error("MaterialManager", "Failed to load shader data for handle: {}", handle.GetID());
        return nullptr;
    }

    // Create shader from data
    std::shared_ptr<IShader> shader = CreateShaderFromData(shaderData, stage, handle);
    if (!shader) {
        Logger::Error("MaterialManager", "Failed to create shader from data for handle: {}", handle.GetID());
        return nullptr;
    }

    // Cache the shader
    {
        std::lock_guard<std::mutex> lock(m_shaderCacheMutex);
        uint64_t hash = CalculateShaderHash(shaderData);
        size_t memoryUsage = shaderData.size() * sizeof(uint32_t);

        ShaderCacheEntry entry(shader, stage, hash, memoryUsage);
        m_shaderCache[handle] = entry;

        Logger::Info("MaterialManager", "Shader loaded and cached: {} (hash: {}, memory: {} bytes)",
                    handle.GetID(), hash, memoryUsage);
    }

    return shader;
}

std::shared_ptr<IShader> MaterialManager::GetShader(const AssetHandle& handle) const {
    if (!handle.IsValid()) {
        return nullptr;
    }

    if (!m_initialized) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_shaderCacheMutex);
    auto it = m_shaderCache.find(handle);
    if (it != m_shaderCache.end()) {
        return it->second.shader;
    }

    return nullptr;
}

std::shared_ptr<IShader> MaterialManager::CreateShaderFromData(const std::vector<uint32_t>& shaderData,
                                                             ShaderStage stage,
                                                             const AssetHandle& assetHandle) {
    if (shaderData.empty()) {
        SetError("Empty shader data provided");
        return nullptr;
    }

    if (!m_initialized) {
        SetError("MaterialManager not initialized");
        return nullptr;
    }

    Logger::Debug("MaterialManager", "Creating shader from data (size: {} words, stage: {})",
                 shaderData.size(), static_cast<int>(stage));

    // Create Vulkan shader
    std::shared_ptr<IShader> shader = std::make_shared<VulkanShader>();

    VkShaderStageFlagBits vkStage = ShaderStageToVulkanStage(stage);
    if (!shader->Initialize(m_vulkanDevice, shaderData, vkStage)) {
        SetError("Failed to initialize VulkanShader: " + shader->GetLastError());
        Logger::Error("MaterialManager", "VulkanShader initialization failed: {}",
                     shader->GetLastError());
        return nullptr;
    }

    Logger::Info("MaterialManager", "Shader created successfully from data");
    return shader;
}

bool MaterialManager::PreloadShader(const AssetHandle& handle, ShaderStage stage) {
    if (!handle.IsValid()) {
        SetError("Invalid AssetHandle provided");
        return false;
    }

    if (!m_initialized) {
        SetError("MaterialManager not initialized");
        return false;
    }

    Logger::Debug("MaterialManager", "Preloading shader: {} (stage: {})", handle.GetID(), static_cast<int>(stage));

    std::shared_ptr<IShader> shader = LoadShader(handle, stage);
    return shader != nullptr;
}

bool MaterialManager::UnloadShader(const AssetHandle& handle) {
    if (!handle.IsValid()) {
        SetError("Invalid AssetHandle provided");
        return false;
    }

    if (!m_initialized) {
        SetError("MaterialManager not initialized");
        return false;
    }

    std::lock_guard<std::mutex> lock(m_shaderCacheMutex);
    auto it = m_shaderCache.find(handle);
    if (it != m_shaderCache.end()) {
        m_shaderCache.erase(it);
        Logger::Info("MaterialManager", "Shader unloaded from cache: {}", handle.GetID());
        return true;
    }

    Logger::Debug("MaterialManager", "Shader not found in cache: {}", handle.GetID());
    return false;
}

void MaterialManager::ClearShaderCache() {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_shaderCacheMutex);
    size_t cacheSize = m_shaderCache.size();
    size_t totalMemory = 0;

    for (const auto& entry : m_shaderCache) {
        totalMemory += entry.second.memoryUsage;
    }

    m_shaderCache.clear();
    m_cacheHits = 0;
    m_cacheMisses = 0;

    Logger::Info("MaterialManager", "Shader cache cleared: {} shaders, {} bytes freed",
                cacheSize, totalMemory);
}

// ===== MATERIAL MANAGEMENT =====

std::shared_ptr<Material> MaterialManager::CreateMaterial(const Material::Config& config) {
    if (!m_initialized) {
        SetError("MaterialManager not initialized");
        return nullptr;
    }

    Logger::Debug("MaterialManager", "Creating material: {}", config.name);

    // Note: Configuration validation would be done here if needed
    // For now, we validate the material after creation

    // Create material instance
    std::shared_ptr<Material> material = std::make_shared<Material>();
    if (!material->Initialize(config)) {
        SetError("Failed to initialize material: " + material->GetLastError());
        Logger::Error("MaterialManager", "Material initialization failed for '{}': {}",
                     config.name, material->GetLastError());
        return nullptr;
    }

    // Set the MaterialManager reference in the material for shader loading
    material->SetMaterialManager(this);

    // Register material
    {
        std::lock_guard<std::mutex> lock(m_materialMutex);
        m_materials[config.name] = material;
    }

    Logger::Info("MaterialManager", "Material created successfully: {}", config.name);
    return material;
}

std::shared_ptr<Material> MaterialManager::GetMaterial(const std::string& materialName) const {
    if (!m_initialized) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_materialMutex);
    auto it = m_materials.find(materialName);
    if (it != m_materials.end()) {
        return it->second;
    }

    return nullptr;
}

std::shared_ptr<Material> MaterialManager::GetMaterial(const AssetHandle& materialHandle) {
    if (!m_initialized) {
        SetError("MaterialManager not initialized");
        Logger::Error("MaterialManager", "Cannot get material - MaterialManager not initialized");
        return nullptr;
    }

    if (!materialHandle.IsValid()) {
        SetError("Invalid AssetHandle provided");
        Logger::Error("MaterialManager", "Cannot get material - invalid AssetHandle");
        return nullptr;
    }

    Logger::Debug("MaterialManager", "Getting material from handle: {}", materialHandle.GetID());

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(m_materialMutex);
        auto it = m_materialsByHandle.find(materialHandle);
        if (it != m_materialsByHandle.end()) {
            Logger::Debug("MaterialManager", "Material found in handle cache: {}", materialHandle.GetID());
            return it->second;
        }
    }

    // Fetch material data from asset manager
    auto materialData = m_assetManager->GetAsset<MaterialData>(materialHandle);
    if (!materialData) {
        SetError("Failed to load MaterialData from AssetManager");
        Logger::Error("MaterialManager", "AssetManager returned null MaterialData for handle: {}",
                     materialHandle.GetID());
        return nullptr;
    }

    if (!materialData->IsValid()) {
        SetError("MaterialData is invalid");
        Logger::Error("MaterialManager", "MaterialData is invalid for handle: {}", materialHandle.GetID());
        return nullptr;
    }

    Logger::Debug("MaterialManager", "Material data loaded successfully: {}", materialData->name);

    // Build Material::Config from MaterialData
    Material::Config config;
    config.name = materialData->name;
    config.vertexShaderHandle = m_assetManager->RegisterAsset(materialData->vertexShaderPath);
    config.fragmentShaderHandle = m_assetManager->RegisterAsset(materialData->fragmentShaderPath);

    // Determine material type based on shader paths or default to PBR
    if (materialData->vertexShaderPath.find("unlit") != std::string::npos ||
        materialData->fragmentShaderPath.find("unlit") != std::string::npos) {
        config.type = MaterialType::Unlit;
    } else if (materialData->vertexShaderPath.find("skybox") != std::string::npos ||
               materialData->fragmentShaderPath.find("skybox") != std::string::npos) {
        config.type = MaterialType::Skybox;
    } else {
        config.type = MaterialType::PBR;
    }

    Logger::Debug("MaterialManager", "Creating material with config - Name: {}, Type: {}, VertexShader: {}, FragmentShader: {}",
                  config.name, static_cast<int>(config.type),
                  config.vertexShaderHandle.IsValid() ? config.vertexShaderHandle.GetID() : 0,
                  config.fragmentShaderHandle.IsValid() ? config.fragmentShaderHandle.GetID() : 0);

    // Create material instance
    auto material = std::make_shared<Material>();
    if (!material->Initialize(config)) {
        SetError("Failed to initialize material: " + material->GetLastError());
        Logger::Error("MaterialManager", "Material initialization failed for '{}': {}",
                     config.name, material->GetLastError());
        return nullptr;
    }

    // Set the MaterialManager reference in the material for shader loading
    material->SetMaterialManager(this);

    // Register material in both caches
    {
        std::lock_guard<std::mutex> lock(m_materialMutex);
        m_materials[config.name] = material;
        m_materialsByHandle[materialHandle] = material;
    }

    Logger::Info("MaterialManager", "Material created and cached successfully: {} (handle: {})",
                 config.name, materialHandle.GetID());

    return material;
}

void MaterialManager::RegisterMaterial(const std::string& name, std::shared_ptr<Material> material) {
    if (!m_initialized || !material) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_materialMutex);
    m_materials[name] = material;

    Logger::Debug("MaterialManager", "Material registered: {}", name);
}

void MaterialManager::UnregisterMaterial(const std::string& name) {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_materialMutex);
    m_materials.erase(name);

    Logger::Debug("MaterialManager", "Material unregistered: {}", name);
}

bool MaterialManager::HasMaterial(const std::string& name) const {
    if (!m_initialized) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_materialMutex);
    return m_materials.find(name) != m_materials.end();
}

// ===== VALIDATION =====

MaterialManager::ValidationResult MaterialManager::ValidateMaterial(const Material* material) const {
    ValidationResult result;

    if (!material) {
        result.AddError("Material pointer is null");
        return result;
    }

    if (!material->IsInitialized()) {
        result.AddError("Material is not initialized");
    }

    // Validate shader handles
    AssetHandle vertexHandle = material->GetVertexShaderHandle();
    AssetHandle fragmentHandle = material->GetFragmentShaderHandle();

    if (!vertexHandle.IsValid()) {
        result.AddError("Invalid vertex shader handle");
    }

    if (!fragmentHandle.IsValid()) {
        result.AddError("Invalid fragment shader handle");
    }

    // Validate shader compatibility if both handles are valid
    if (vertexHandle.IsValid() && fragmentHandle.IsValid()) {
        ValidationResult shaderValidation = ValidateShaderCompatibility(vertexHandle, fragmentHandle);
        if (!shaderValidation.IsValid()) {
            result.AddError("Shader compatibility validation failed: " + shaderValidation.GetMessages());
        }

        // Add warnings from shader validation
        for (const auto& warning : shaderValidation.warnings) {
            result.AddWarning("Shader compatibility: " + warning);
        }
    }

    // Validate material properties
    const MaterialProperties& props = material->GetProperties();

    if (props.metallic < 0.0f || props.metallic > 1.0f) {
        result.AddWarning("Metallic value is outside normal range [0,1]");
    }

    if (props.roughness < 0.0f || props.roughness > 1.0f) {
        result.AddWarning("Roughness value is outside normal range [0,1]");
    }

    if (props.opacity < 0.0f || props.opacity > 1.0f) {
        result.AddWarning("Opacity value is outside normal range [0,1]");
    }

    if (props.emissiveIntensity < 0.0f) {
        result.AddWarning("Emissive intensity is negative");
    }

    result.isValid = result.errors.empty();
    return result;
}

MaterialManager::ValidationResult MaterialManager::ValidateMaterial(const std::string& materialName) const {
    std::shared_ptr<Material> material = GetMaterial(materialName);
    if (!material) {
        ValidationResult result;
        result.AddError("Material not found: " + materialName);
        return result;
    }

    return ValidateMaterial(material.get());
}

MaterialManager::ValidationResult MaterialManager::ValidateShaderCompatibility(const IShader* vertexShader,
                                                                              const IShader* fragmentShader) const {
    ValidationResult result;

    if (!vertexShader) {
        result.AddError("Vertex shader is null");
        return result;
    }

    if (!fragmentShader) {
        result.AddError("Fragment shader is null");
        return result;
    }

    if (!vertexShader->IsInitialized()) {
        result.AddError("Vertex shader is not initialized");
    }

    if (!fragmentShader->IsInitialized()) {
        result.AddError("Fragment shader is not initialized");
    }

    // Check shader stages
    if (vertexShader->GetShaderStage() != ShaderStage::Vertex) {
        result.AddError("Vertex shader has incorrect stage");
    }

    if (fragmentShader->GetShaderStage() != ShaderStage::Fragment) {
        result.AddError("Fragment shader has incorrect stage");
    }

    // Validate shader compatibility
    if (!ValidateShaderStages(vertexShader, fragmentShader)) {
        result.AddError("Shader stages are not compatible");
    }

    if (!ValidateShaderInterfaces(vertexShader, fragmentShader)) {
        result.AddError("Shader interfaces are not compatible");
    }

    result.isValid = result.errors.empty();
    return result;
}

MaterialManager::ValidationResult MaterialManager::ValidateShaderCompatibility(const AssetHandle& vertexShaderHandle,
                                                                              const AssetHandle& fragmentShaderHandle) const {
    std::shared_ptr<IShader> vertexShader = GetShader(vertexShaderHandle);
    std::shared_ptr<IShader> fragmentShader = GetShader(fragmentShaderHandle);

    if (!vertexShader) {
        ValidationResult result;
        result.AddError("Vertex shader not found in cache");
        return result;
    }

    if (!fragmentShader) {
        ValidationResult result;
        result.AddError("Fragment shader not found in cache");
        return result;
    }

    return ValidateShaderCompatibility(vertexShader.get(), fragmentShader.get());
}

// ===== STATISTICS AND MONITORING =====

size_t MaterialManager::GetShaderCacheCount() const {
    std::lock_guard<std::mutex> lock(m_shaderCacheMutex);
    return m_shaderCache.size();
}

size_t MaterialManager::GetShaderCacheMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_shaderCacheMutex);
    size_t totalMemory = 0;
    for (const auto& entry : m_shaderCache) {
        totalMemory += entry.second.memoryUsage;
    }
    return totalMemory;
}

std::string MaterialManager::GetShaderCacheStatistics() const {
    std::lock_guard<std::mutex> lock(m_shaderCacheMutex);
    std::stringstream ss;

    ss << "Shader Cache Statistics:\n";
    ss << "  Total shaders: " << m_shaderCache.size() << "\n";
    ss << "  Total memory: " << GetShaderCacheMemoryUsage() << " bytes\n";
    ss << "  Cache hits: " << m_cacheHits << "\n";
    ss << "  Cache misses: " << m_cacheMisses << "\n";

    if (m_cacheHits + m_cacheMisses > 0) {
        float hitRate = static_cast<float>(m_cacheHits) / (m_cacheHits + m_cacheMisses) * 100.0f;
        ss << "  Hit rate: " << std::fixed << std::setprecision(2) << hitRate << "%\n";
    }

    return ss.str();
}

void MaterialManager::ClearUnusedMaterials() {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_materialMutex);
    size_t initialCount = m_materials.size();

    // For now, just log the operation
    // In a more sophisticated implementation, we could track material usage
    Logger::Debug("MaterialManager", "ClearUnusedMaterials called - {} materials in memory",
                 initialCount);
}

void MaterialManager::ClearUnusedShaders() {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_shaderCacheMutex);
    size_t initialCount = m_shaderCache.size();

    // For now, just log the operation
    // In a more sophisticated implementation, we could track shader usage
    Logger::Debug("MaterialManager", "ClearUnusedShaders called - {} shaders in cache",
                 initialCount);
}

// ===== HELPER METHODS =====

bool MaterialManager::CreateDefaultMaterials() {
    Logger::Debug("MaterialManager", "Creating default materials");

    // Register default shader assets
    AssetHandle pbrVertexShaderHandle = m_assetManager->RegisterAsset("Assets/Shaders/Materials/pbr_material_vertex.slang");
    AssetHandle pbrFragmentShaderHandle = m_assetManager->RegisterAsset("Assets/Shaders/Materials/pbr_material_fragment.slang");
    AssetHandle unlitVertexShaderHandle = m_assetManager->RegisterAsset("Assets/Shaders/Materials/unlit_vertex.slang");
    AssetHandle unlitFragmentShaderHandle = m_assetManager->RegisterAsset("Assets/Shaders/Materials/unlit_fragment.slang");

    // Validate that shader registration was successful
    if (!pbrVertexShaderHandle.IsValid()) {
        Logger::Error("MaterialManager", "Failed to register PBR vertex shader asset");
        return false;
    }

    if (!pbrFragmentShaderHandle.IsValid()) {
        Logger::Error("MaterialManager", "Failed to register PBR fragment shader asset");
        return false;
    }

    if (!unlitVertexShaderHandle.IsValid()) {
        Logger::Error("MaterialManager", "Failed to register unlit vertex shader asset");
        return false;
    }

    if (!unlitFragmentShaderHandle.IsValid()) {
        Logger::Error("MaterialManager", "Failed to register unlit fragment shader asset");
        return false;
    }

    Logger::Info("MaterialManager", "Default shader assets registered successfully");

    // Create default PBR material with valid shader handles
    Material::Config pbrConfig;
    pbrConfig.type = MaterialType::PBR;
    pbrConfig.name = "DefaultPBR";
    pbrConfig.vertexShaderHandle = pbrVertexShaderHandle;
    pbrConfig.fragmentShaderHandle = pbrFragmentShaderHandle;

    m_defaultPBRMaterial = std::make_shared<Material>();
    if (!m_defaultPBRMaterial->Initialize(pbrConfig)) {
        Logger::Error("MaterialManager", "Failed to create default PBR material");
        return false;
    }

    // Create default unlit material with valid shader handles
    Material::Config unlitConfig;
    unlitConfig.type = MaterialType::Unlit;
    unlitConfig.name = "DefaultUnlit";
    unlitConfig.vertexShaderHandle = unlitVertexShaderHandle;
    unlitConfig.fragmentShaderHandle = unlitFragmentShaderHandle;

    m_defaultUnlitMaterial = std::make_shared<Material>();
    if (!m_defaultUnlitMaterial->Initialize(unlitConfig)) {
        Logger::Error("MaterialManager", "Failed to create default unlit material");
        return false;
    }

    Logger::Info("MaterialManager", "Default materials created successfully with valid shader handles");
    return true;
}

void MaterialManager::CleanupUnusedMaterials() {
    ClearUnusedMaterials();
}

void MaterialManager::CleanupUnusedShaders() {
    ClearUnusedShaders();
}

std::vector<uint32_t> MaterialManager::LoadShaderData(const AssetHandle& handle) {
    if (!m_assetManager) {
        SetError("AssetManager not available");
        return {};
    }

    Logger::Debug("MaterialManager", "Loading shader data for handle: {}", handle.GetID());

    // Get shader data from asset manager
    std::shared_ptr<ShaderData> shaderData = m_assetManager->GetAsset<ShaderData>(handle);
    if (!shaderData) {
        SetError("Failed to load ShaderData from AssetManager");
        Logger::Error("MaterialManager", "AssetManager returned null ShaderData for handle: {}",
                     handle.GetID());
        return {};
    }

    if (!shaderData->IsValid()) {
        SetError("ShaderData is invalid");
        Logger::Error("MaterialManager", "ShaderData is invalid for handle: {}", handle.GetID());
        return {};
    }

    Logger::Debug("MaterialManager", "Shader data loaded successfully: {} bytes",
                 shaderData->spirvCode.size() * sizeof(uint32_t));

    return shaderData->spirvCode;
}

uint64_t MaterialManager::CalculateShaderHash(const std::vector<uint32_t>& shaderData) const {
    if (shaderData.empty()) {
        return 0;
    }

    // Simple hash calculation using first and last 64 bits of shader code
    size_t codeSize = shaderData.size();
    uint64_t hash = (static_cast<uint64_t>(shaderData[0]) << 32) |
                   (codeSize > 1 ? shaderData[codeSize - 1] : 0);

    return hash;
}

std::shared_ptr<IShader> MaterialManager::CreateShaderObject(const std::vector<uint32_t>& shaderData,
                                                           ShaderStage stage) {
    return CreateShaderFromData(shaderData, stage);
}

bool MaterialManager::ValidateShaderStages(const IShader* vertexShader, const IShader* fragmentShader) const {
    if (!vertexShader || !fragmentShader) {
        return false;
    }

    return vertexShader->GetShaderStage() == ShaderStage::Vertex &&
           fragmentShader->GetShaderStage() == ShaderStage::Fragment;
}

bool MaterialManager::ValidateShaderInterfaces(const IShader* vertexShader, const IShader* fragmentShader) const {
    // For now, assume interfaces are compatible if stages are correct
    // In a more sophisticated implementation, we would check input/output interface matching
    return true;
}

void MaterialManager::SetError(const std::string& error) {
    m_lastError = error;
    Logger::Error("MaterialManager", "Error: {}", error);
}

// ===== SHADER LOADING HELPERS (MOVED FROM MATERIAL) =====

std::shared_ptr<ShaderData> MaterialManager::LoadShaderFromHandle(const AssetHandle& handle, ShaderStage stage) const {
    if (!m_assetManager) {
        SetError("AssetManager not available");
        Logger::Error("MaterialManager", "AssetManager not available for shader loading");
        return nullptr;
    }

    Logger::Debug("MaterialManager", "Loading shader data for handle: {} (stage: {})", handle.GetID(), static_cast<int>(stage));

    // Get shader data from asset manager
    std::shared_ptr<ShaderData> shaderData = m_assetManager->GetAsset<ShaderData>(handle);
    if (!shaderData) {
        SetError("Failed to load ShaderData from AssetManager");
        Logger::Error("MaterialManager", "AssetManager returned null ShaderData for handle: {}", handle.GetID());
        return nullptr;
    }

    if (!shaderData->IsValid()) {
        SetError("ShaderData is invalid");
        Logger::Error("MaterialManager", "ShaderData is invalid for handle: {}", handle.GetID());
        return nullptr;
    }

    Logger::Debug("MaterialManager", "Shader data loaded successfully: {} bytes",
                  shaderData->spirvCode.size() * sizeof(uint32_t));

    return shaderData;
}

std::shared_ptr<IShader> MaterialManager::CreateShaderFromData(std::shared_ptr<ShaderData> shaderData, ShaderStage stage) const {
    if (!shaderData || !shaderData->IsValid()) {
        SetError("Invalid shader data provided");
        Logger::Error("MaterialManager", "Invalid shader data provided");
        return nullptr;
    }

    Logger::Debug("MaterialManager", "Creating shader from data (stage: {}, size: {} bytes)",
                  static_cast<int>(stage), shaderData->GetMemoryUsage());

    try {
        // Create Vulkan shader instance
        std::shared_ptr<IShader> shader = std::make_shared<VulkanShader>();

        VkShaderStageFlagBits vkStage = GetVulkanShaderStage(stage);
        if (!shader->Initialize(m_vulkanDevice, shaderData->spirvCode, vkStage)) {
            SetError("Failed to initialize VulkanShader: " + shader->GetLastError());
            Logger::Error("MaterialManager", "VulkanShader initialization failed: {}", shader->GetLastError());
            return nullptr;
        }

        Logger::Debug("MaterialManager", "Shader created successfully for stage: {}", static_cast<int>(stage));
        return shader;

    } catch (const std::exception& e) {
        SetError(std::string("Exception creating shader: ") + e.what());
        Logger::Error("MaterialManager", "Exception creating shader: {}", e.what());
        return nullptr;
    }
}

bool MaterialManager::ValidateShaderCompatibility() const {
    // This method would need access to the current material's shaders
    // For now, return true as a placeholder
    // In a real implementation, this would validate the loaded shaders
    Logger::Debug("MaterialManager", "Shader compatibility validation called (placeholder)");
    return true;
}

VkShaderStageFlagBits MaterialManager::GetVulkanShaderStage(ShaderStage stage) const {
    switch (stage) {
        case ShaderStage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::TessControl: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::TessEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case ShaderStage::Task: return VK_SHADER_STAGE_TASK_BIT_EXT;
        case ShaderStage::Mesh: return VK_SHADER_STAGE_MESH_BIT_EXT;
        default:
            Logger::Warning("MaterialManager", "Unknown shader stage: {}", static_cast<int>(stage));
            return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

bool MaterialManager::ValidateShaderHandlesCompatibility() const {
    // This method would need access to the current material's shader handles
    // For now, return true as a placeholder
    // In a real implementation, this would validate the shader handles
    Logger::Debug("MaterialManager", "Shader handles compatibility validation called (placeholder)");
    return true;
}

// ===== VALIDATION RESULT IMPLEMENTATION =====

std::string MaterialManager::ValidationResult::GetMessages() const {
    std::stringstream ss;

    for (const auto& error : errors) {
        ss << "ERROR: " << error << "\n";
    }

    for (const auto& warning : warnings) {
        ss << "WARNING: " << warning << "\n";
    }

    return ss.str();
}

} // namespace AstralEngine