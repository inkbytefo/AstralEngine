#pragma once

#include "../../Asset/AssetHandle.h"
#include "../RendererTypes.h"
#include "../Core/VulkanDevice.h"
#include "../Shaders/IShader.h"
#include "Material.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace AstralEngine {

class AssetManager;  // Forward declaration

/**
 * @class MaterialManager
 * @brief Advanced material and shader management system with caching and validation
 *
 * The MaterialManager class provides comprehensive material and shader management functionality
 * including shader loading, caching, validation, and thread-safe operations. It serves as
 * the central hub for material creation, shader management, and resource optimization.
 *
 * Key Features:
 * - Shader loading and caching with AssetHandle-based lookup
 * - Comprehensive material validation including shader compatibility checks
 * - Thread-safe operations with mutex protection
 * - VulkanDevice integration for shader creation
 * - Enhanced error handling and logging
 * - Memory-efficient resource management
 *
 * @note This class is designed to work closely with the AssetManager and VulkanDevice
 * subsystems to provide seamless material and shader management across the engine.
 */
class MaterialManager {
public:
    /**
     * @brief Shader cache entry containing shader object and metadata
     */
    struct ShaderCacheEntry {
        std::shared_ptr<IShader> shader;    ///< Cached shader object
        ShaderStage stage;                   ///< Shader stage (vertex/fragment/etc.)
        uint64_t hash;                       ///< Shader content hash for validation
        size_t memoryUsage;                  ///< Memory usage in bytes
        std::chrono::steady_clock::time_point lastAccessTime; ///< Last access timestamp

        ShaderCacheEntry() = default;
        ShaderCacheEntry(std::shared_ptr<IShader> s, ShaderStage st, uint64_t h, size_t mem)
            : shader(s), stage(st), hash(h), memoryUsage(mem),
              lastAccessTime(std::chrono::steady_clock::now()) {}
    };

    /**
     * @brief Material validation result with detailed error information
     */
    struct ValidationResult {
        bool isValid = false;                    ///< Overall validation result
        std::vector<std::string> errors;         ///< List of validation errors
        std::vector<std::string> warnings;       ///< List of validation warnings
        std::unordered_map<std::string, bool> checks; ///< Individual validation check results

        ValidationResult() = default;
        ValidationResult(bool valid) : isValid(valid) {}

        /**
         * @brief Add an error message to the validation result
         * @param error The error message to add
         */
        void AddError(const std::string& error) {
            isValid = false;
            errors.push_back(error);
        }

        /**
         * @brief Add a warning message to the validation result
         * @param warning The warning message to add
         */
        void AddWarning(const std::string& warning) {
            warnings.push_back(warning);
        }

        /**
         * @brief Check if validation passed
         * @return true if all checks passed, false otherwise
         */
        bool IsValid() const { return isValid && errors.empty(); }

        /**
         * @brief Get combined error and warning messages
         * @return String containing all messages separated by newlines
         */
        std::string GetMessages() const;
    };

    MaterialManager();
    ~MaterialManager();

    // Non-copyable
    MaterialManager(const MaterialManager&) = delete;
    MaterialManager& operator=(const MaterialManager&) = delete;

    // Lifecycle management
    /**
     * @brief Initialize the MaterialManager with required dependencies
     * @param assetManager Pointer to the AssetManager for asset loading
     * @param vulkanDevice Pointer to VulkanDevice for shader creation
     * @return true if initialization successful, false otherwise
     */
    bool Initialize(AssetManager* assetManager, VulkanDevice* vulkanDevice);

    /**
     * @brief Shutdown the MaterialManager and clean up resources
     */
    void Shutdown();

    /**
     * @brief Update the MaterialManager (cleanup, statistics, etc.)
     */
    void Update();

    // Shader management
    /**
     * @brief Load a shader from an AssetHandle with caching
     * @param handle AssetHandle of the shader to load
     * @param stage Shader stage (vertex/fragment/etc.)
     * @return Pointer to loaded shader, nullptr if loading failed
     */
    std::shared_ptr<IShader> LoadShader(const AssetHandle& handle, ShaderStage stage);

    /**
     * @brief Get a cached shader by AssetHandle
     * @param handle AssetHandle of the shader to retrieve
     * @return Pointer to cached shader, nullptr if not found
     */
    std::shared_ptr<IShader> GetShader(const AssetHandle& handle) const;

    /**
     * @brief Create a shader from raw shader data
     * @param shaderData Vector containing shader bytecode/SPIR-V
     * @param stage Shader stage for the new shader
     * @param assetHandle Optional AssetHandle to associate with the shader
     * @return Pointer to created shader, nullptr if creation failed
     */
    std::shared_ptr<IShader> CreateShaderFromData(const std::vector<uint32_t>& shaderData,
                                                  ShaderStage stage,
                                                  const AssetHandle& assetHandle = AssetHandle());

    /**
     * @brief Preload and cache a shader for later use
     * @param handle AssetHandle of the shader to preload
     * @param stage Shader stage
     * @return true if preloading successful, false otherwise
     */
    bool PreloadShader(const AssetHandle& handle, ShaderStage stage);

    /**
     * @brief Unload a shader from cache
     * @param handle AssetHandle of the shader to unload
     * @return true if shader was unloaded, false if not found
     */
    bool UnloadShader(const AssetHandle& handle);

    /**
     * @brief Clear all cached shaders
     */
    void ClearShaderCache();

    // Material management (enhanced from base MaterialManager)
    /**
     * @brief Create a new material with validation
     * @param config Material configuration
     * @return Pointer to created material, nullptr if creation failed
     */
    std::shared_ptr<Material> CreateMaterial(const Material::Config& config);

    /**
     * @brief Get a material by name
     * @param materialName Name of the material to retrieve
     * @return Pointer to material, nullptr if not found
     */
    std::shared_ptr<Material> GetMaterial(const std::string& materialName) const;

    /**
     * @brief Get a material by AssetHandle
     * @param materialHandle AssetHandle of the material to retrieve
     * @return Pointer to material, nullptr if not found
     */
    std::shared_ptr<Material> GetMaterial(const AssetHandle& materialHandle);

    /**
     * @brief Register a material in the manager
     * @param name Name to register the material under
     * @param material Pointer to the material to register
     */
    void RegisterMaterial(const std::string& name, std::shared_ptr<Material> material);

    /**
     * @brief Unregister a material from the manager
     * @param name Name of the material to unregister
     */
    void UnregisterMaterial(const std::string& name);

    /**
     * @brief Check if a material exists
     * @param name Name of the material to check
     * @return true if material exists, false otherwise
     */
    bool HasMaterial(const std::string& name) const;

    // Validation methods
    /**
     * @brief Validate a material comprehensively
     * @param material Pointer to the material to validate
     * @return ValidationResult containing detailed validation information
     */
    ValidationResult ValidateMaterial(const Material* material) const;

    /**
     * @brief Validate a material by name
     * @param materialName Name of the material to validate
     * @return ValidationResult containing detailed validation information
     */
    ValidationResult ValidateMaterial(const std::string& materialName) const;

    /**
     * @brief Validate shader compatibility between vertex and fragment shaders
     * @param vertexShader Vertex shader to validate
     * @param fragmentShader Fragment shader to validate
     * @return ValidationResult containing compatibility information
     */
    ValidationResult ValidateShaderCompatibility(const IShader* vertexShader,
                                                const IShader* fragmentShader) const;

    /**
     * @brief Validate shader compatibility by AssetHandles
     * @param vertexShaderHandle AssetHandle of vertex shader
     * @param fragmentShaderHandle AssetHandle of fragment shader
     * @return ValidationResult containing compatibility information
     */
    ValidationResult ValidateShaderCompatibility(const AssetHandle& vertexShaderHandle,
                                                const AssetHandle& fragmentShaderHandle) const;

    // Default materials
    /**
     * @brief Get the default PBR material
     * @return Pointer to default PBR material
     */
    std::shared_ptr<Material> GetDefaultPBRMaterial() const { return m_defaultPBRMaterial; }

    /**
     * @brief Get the default unlit material
     * @return Pointer to default unlit material
     */
    std::shared_ptr<Material> GetDefaultUnlitMaterial() const { return m_defaultUnlitMaterial; }

    // Statistics and monitoring
    /**
     * @brief Get the number of cached shaders
     * @return Number of shaders in cache
     */
    size_t GetShaderCacheCount() const;

    /**
     * @brief Get the number of managed materials
     * @return Number of materials in manager
     */
    size_t GetMaterialCount() const { return m_materials.size(); }

    /**
     * @brief Get total memory usage of shader cache
     * @return Total memory usage in bytes
     */
    size_t GetShaderCacheMemoryUsage() const;

    /**
     * @brief Get shader cache statistics
     * @return String containing cache statistics
     */
    std::string GetShaderCacheStatistics() const;

    /**
     * @brief Clear unused materials from memory
     */
    void ClearUnusedMaterials();

    /**
     * @brief Clear unused shaders from cache
     */
    void ClearUnusedShaders();

    /**
     * @brief Get the last error message
     * @return String containing the last error
     */
    const std::string& GetLastError() const { return m_lastError; }

private:
    // Helper methods
    /**
     * @brief Create default materials (PBR, Unlit, etc.)
     * @return true if creation successful, false otherwise
     */
    bool CreateDefaultMaterials();

    /**
     * @brief Clean up unused materials
     */
    void CleanupUnusedMaterials();

    /**
     * @brief Clean up unused shaders from cache
     */
    void CleanupUnusedShaders();

    /**
     * @brief Load shader data from AssetManager
     * @param handle AssetHandle of the shader
     * @return Vector containing shader bytecode, empty if failed
     */
    std::vector<uint32_t> LoadShaderData(const AssetHandle& handle);

    /**
     * @brief Calculate hash for shader data
     * @param shaderData Vector containing shader bytecode
     * @return 64-bit hash of the shader data
     */
    uint64_t CalculateShaderHash(const std::vector<uint32_t>& shaderData) const;

    /**
     * @brief Create shader object from data
     * @param shaderData Vector containing shader bytecode
     * @param stage Shader stage
     * @return Pointer to created shader, nullptr if failed
     */
    std::shared_ptr<IShader> CreateShaderObject(const std::vector<uint32_t>& shaderData,
                                               ShaderStage stage);

    /**
     * @brief Validate shader stage compatibility
     * @param vertexShader Vertex shader to validate
     * @param fragmentShader Fragment shader to validate
     * @return true if stages are compatible, false otherwise
     */
    bool ValidateShaderStages(const IShader* vertexShader, const IShader* fragmentShader) const;

    /**
     * @brief Validate shader interface compatibility
     * @param vertexShader Vertex shader to validate
     * @param fragmentShader Fragment shader to validate
     * @return true if interfaces are compatible, false otherwise
     */
    bool ValidateShaderInterfaces(const IShader* vertexShader, const IShader* fragmentShader) const;

    /**
     * @brief Set error message
     * @param error The error message to set
     */
    void SetError(const std::string& error);

    /**
     * @brief Load shader data from asset handle (moved from Material)
     * @param handle AssetHandle of the shader
     * @param stage Shader stage
     * @return Pointer to ShaderData, nullptr if failed
     */
    std::shared_ptr<ShaderData> LoadShaderFromHandle(const AssetHandle& handle, ShaderStage stage);

    /**
     * @brief Create shader from data (moved from Material)
     * @param shaderData Shader data to create from
     * @param stage Shader stage
     * @return Pointer to created shader, nullptr if failed
     */
    std::shared_ptr<IShader> CreateShaderFromData(std::shared_ptr<ShaderData> shaderData, ShaderStage stage);

    /**
     * @brief Validate shader compatibility (moved from Material)
     * @return true if shaders are compatible, false otherwise
     */
    bool ValidateShaderCompatibility();

    /**
     * @brief Convert ShaderStage to Vulkan shader stage (moved from Material)
     * @param stage Shader stage to convert
     * @return VkShaderStageFlagBits corresponding to the shader stage
     */
    VkShaderStageFlagBits GetVulkanShaderStage(ShaderStage stage) const;

    /**
     * @brief Validate shader handles compatibility (moved from Material)
     * @return true if handles are compatible, false otherwise
     */
    bool ValidateShaderHandlesCompatibility();

    // Member variables
    AssetManager* m_assetManager = nullptr;     ///< Pointer to AssetManager
    VulkanDevice* m_vulkanDevice = nullptr;     ///< Pointer to VulkanDevice

    // Shader cache with thread safety
    std::unordered_map<AssetHandle, ShaderCacheEntry, AssetHandleHash> m_shaderCache;
    mutable std::mutex m_shaderCacheMutex;      ///< Mutex for shader cache operations

    // Material storage
    std::unordered_map<std::string, std::shared_ptr<Material>> m_materials;
    std::unordered_map<AssetHandle, std::shared_ptr<Material>, AssetHandleHash> m_materialsByHandle;
    mutable std::mutex m_materialMutex;        ///< Mutex for material operations

    // Default materials
    std::shared_ptr<Material> m_defaultPBRMaterial;
    std::shared_ptr<Material> m_defaultUnlitMaterial;

    // Cache management
    size_t m_maxCacheSize = 100;                ///< Maximum number of cached shaders
    size_t m_maxCacheMemoryMB = 256;            ///< Maximum cache memory in MB
    std::chrono::minutes m_cacheCleanupInterval = std::chrono::minutes(5); ///< Cache cleanup interval

    // State management
    bool m_initialized = false;
    std::string m_lastError;

    // Cache statistics
    mutable size_t m_cacheHits = 0;
    mutable size_t m_cacheMisses = 0;
    mutable std::chrono::steady_clock::time_point m_lastCacheCleanup;
};

} // namespace AstralEngine