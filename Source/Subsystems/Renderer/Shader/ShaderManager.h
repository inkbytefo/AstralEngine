#pragma once

#include "ShaderCompiler.h"
#include "VulkanShader.h"
#include "../Material/Material.h"
#include "../Texture/TextureManager.h"
#include "../Core/VulkanDevice.h"
#include "../../Asset/AssetManager.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace AstralEngine {

/**
 * @enum ShaderPipelineType
 * @brief Desteklenen shader pipeline tipleri
 */
enum class ShaderPipelineType {
    Forward = 0,         ///< Forward rendering pipeline
    Deferred,           ///< Deferred rendering pipeline
    Compute,            ///< Compute pipeline
    RayTracing,         ///< Ray tracing pipeline
    Custom              ///< Custom pipeline
};

/**
 * @struct ShaderPipelineInfo
 * @brief Shader pipeline bilgileri
 */
struct ShaderPipelineInfo {
    ShaderPipelineType type = ShaderPipelineType::Forward; ///< Pipeline tipi
    std::string name;                                     ///< Pipeline adı
    std::unordered_map<ShaderStage, std::string> shaderFiles; ///< Shader dosyaları
    std::vector<std::string> defines;                       ///< Shader tanımlamaları
    bool enableDepthTest = true;                            ///< Depth test aktif
    bool enableDepthWrite = true;                           ///< Depth write aktif
    bool enableStencilTest = false;                         ///< Stencil test aktif
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;      ///< Culling modu
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; ///< Front face yönü
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; ///< Primitive topology
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;       ///< Polygon modu
    bool enableBlending = false;                            ///< Blending aktif
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT; ///< MSAA sample count
};

/**
 * @struct ShaderBindingInfo
 * @brief Shader binding bilgileri
 */
struct ShaderBindingInfo {
    uint32_t set = 0;                          ///< Descriptor set index
    uint32_t binding = 0;                      ///< Binding index
    VkDescriptorType descriptorType;           ///< Descriptor tipi
    VkShaderStageFlags stageFlags;             ///< Kullanılan shader aşamaları
    uint32_t count = 1;                        ///< Array boyutu
    std::string name;                          ///< Binding adı
};

/**
 * @struct ShaderProgramInfo
 * @brief Shader program bilgileri
 */
struct ShaderProgramInfo {
    std::string name;                           ///< Program adı
    std::vector<std::shared_ptr<VulkanShader>> shaders; ///< Shader'lar
    std::vector<ShaderBindingInfo> bindings;   ///< Binding bilgileri
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE; ///< Pipeline layout
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts; ///< Descriptor set layout'lar
    bool isValid = false;                       ///< Geçerli mi?
    
    explicit operator bool() const { return isValid; }
};

/**
 * @class ShaderManager
 * @brief Shader yönetimi ve pipeline oluşturma sistemi
 * 
 * Bu sınıf, shader programlarını yönetir, pipeline'lar oluşturur ve
 * Material/Texture ile shader entegrasyonunu sağlar.
 */
class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();

    // Non-copyable
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, AssetManager* assetManager, TextureManager* textureManager);
    void Shutdown();
    void Update();

    // Shader program yönetimi
    std::shared_ptr<ShaderProgramInfo> CreateShaderProgram(const ShaderPipelineInfo& pipelineInfo);
    std::shared_ptr<ShaderProgramInfo> GetShaderProgram(const std::string& name) const;
    bool RegisterShaderProgram(const std::string& name, std::shared_ptr<ShaderProgramInfo> program);
    void UnregisterShaderProgram(const std::string& name);
    bool HasShaderProgram(const std::string& name) const;
    
    // Pipeline oluşturma
    VkPipeline CreateGraphicsPipeline(const ShaderPipelineInfo& pipelineInfo, 
                                   VkRenderPass renderPass, VkExtent2D extent);
    VkPipeline CreateComputePipeline(const ShaderPipelineInfo& pipelineInfo);
    void DestroyPipeline(VkPipeline pipeline);
    
    // Material ile entegrasyon
    bool BindMaterialToShader(std::shared_ptr<Material> material, 
                              const std::string& shaderProgramName);
    bool UpdateMaterialBindings(std::shared_ptr<Material> material);
    VkDescriptorSetLayout GetMaterialDescriptorSetLayout(const std::string& shaderProgramName) const;
    
    // Texture ile entegrasyon
    bool BindTextureToShader(std::shared_ptr<Texture> texture, 
                             const std::string& shaderProgramName, uint32_t set, uint32_t binding);
    bool UpdateTextureBindings(const std::string& shaderProgramName);
    
    // Descriptor set yönetimi
    VkDescriptorSet AllocateDescriptorSet(const std::string& shaderProgramName, uint32_t setIndex);
    void FreeDescriptorSet(VkDescriptorSet descriptorSet);
    bool UpdateDescriptorSet(const std::string& shaderProgramName, uint32_t setIndex, 
                           VkDescriptorSet descriptorSet, const std::vector<VkWriteDescriptorSet>& writes);
    
    // Uniform buffer yönetimi
    VkBuffer CreateUniformBuffer(const std::string& shaderProgramName, const std::string& bufferName, 
                                VkDeviceSize size);
    void DestroyUniformBuffer(VkBuffer buffer, VkDeviceMemory memory);
    bool UpdateUniformBuffer(const std::string& shaderProgramName, const std::string& bufferName, 
                            const void* data, VkDeviceSize size);
    
    // Push constant yönetimi
    bool SetPushConstants(const std::string& shaderProgramName, VkCommandBuffer commandBuffer,
                         VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* data);
    
    // Shader derleme ve yenileme
    bool RecompileShaderProgram(const std::string& name);
    bool ReloadShaderProgram(const std::string& name);
    bool HotReloadShaderProgram(const std::string& name);
    
    // Cache ve optimizasyon
    void EnableShaderCache(bool enable);
    void ClearShaderCache();
    void OptimizeShaderPrograms();
    
    // Debug ve bilgi
    std::string GetShaderProgramInfo(const std::string& name) const;
    void PrintShaderProgramInfo(const std::string& name) const;
    void PrintAllShaderPrograms() const;
    
    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }
    bool HasError() const { return !m_lastError.empty(); }

private:
    // Yardımcı metotlar
    bool CreateDescriptorSetLayouts(const ShaderPipelineInfo& pipelineInfo, 
                                   ShaderProgramInfo& programInfo);
    bool CreatePipelineLayout(const ShaderPipelineInfo& pipelineInfo, 
                             ShaderProgramInfo& programInfo);
    bool CreateDescriptorPool(const ShaderProgramInfo& programInfo);
    
    std::vector<VkPipelineShaderStageCreateInfo> CreateShaderStages(const ShaderProgramInfo& programInfo) const;
    VkPipelineVertexInputStateCreateInfo CreateVertexInputState(const ShaderPipelineInfo& pipelineInfo) const;
    VkPipelineInputAssemblyStateCreateInfo CreateInputAssemblyState(const ShaderPipelineInfo& pipelineInfo) const;
    VkPipelineViewportStateCreateInfo CreateViewportState(const ShaderPipelineInfo& pipelineInfo, VkExtent2D extent) const;
    VkPipelineRasterizationStateCreateInfo CreateRasterizationState(const ShaderPipelineInfo& pipelineInfo) const;
    VkPipelineMultisampleStateCreateInfo CreateMultisampleState(const ShaderPipelineInfo& pipelineInfo) const;
    VkPipelineDepthStencilStateCreateInfo CreateDepthStencilState(const ShaderPipelineInfo& pipelineInfo) const;
    VkPipelineColorBlendStateCreateInfo CreateColorBlendState(const ShaderPipelineInfo& pipelineInfo) const;
    VkPipelineDynamicStateCreateInfo CreateDynamicState(const ShaderPipelineInfo& pipelineInfo) const;
    
    std::vector<VkDescriptorSetLayoutBinding> GetDescriptorSetLayoutBindings(const ShaderProgramInfo& programInfo, uint32_t setIndex) const;
    VkDescriptorType GetDescriptorTypeFromName(const std::string& name) const;
    VkShaderStageFlags GetShaderStageFlagsFromName(const std::string& name) const;
    
    bool ValidateShaderProgram(const ShaderProgramInfo& programInfo) const;
    bool ValidatePipelineCompatibility(const ShaderPipelineInfo& pipelineInfo, VkRenderPass renderPass) const;
    
    void CleanupUnusedPrograms();
    void UpdateShaderStatistics();
    
    void SetError(const std::string& error);
    void ClearError();

    // Member değişkenler
    VulkanDevice* m_device = nullptr;
    AssetManager* m_assetManager = nullptr;
    TextureManager* m_textureManager = nullptr;
    ShaderCompiler* m_shaderCompiler = nullptr;
    
    // Shader program önbelleği
    std::unordered_map<std::string, std::shared_ptr<ShaderProgramInfo>> m_shaderPrograms;
    
    // Descriptor pool yönetimi
    std::unordered_map<std::string, VkDescriptorPool> m_descriptorPools;
    
    // Pipeline cache
    VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;
    
    // İstatistikler
    size_t m_totalPrograms = 0;
    size_t m_activePipelines = 0;
    uint64_t m_totalCompileTime = 0;
    
    // Ayarlar
    bool m_shaderCacheEnabled = true;
    bool m_hotReloadEnabled = true;
    bool m_validationEnabled = true;
    
    // Hata yönetimi
    std::string m_lastError;
    
    bool m_initialized = false;
    mutable std::mutex m_mutex;
};

/**
 * @class ShaderBindingManager
 * @brief Shader binding yönetimi
 * 
 * Bu sınıf, shader binding'lerini yönetir ve Material/Texture
 * ile shader arasındaki bağlantıyı kurar.
 */
class ShaderBindingManager {
public:
    ShaderBindingManager();
    ~ShaderBindingManager();

    // Non-copyable
    ShaderBindingManager(const ShaderBindingManager&) = delete;
    ShaderBindingManager& operator=(const ShaderBindingManager&) = delete;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, ShaderManager* shaderManager);
    void Shutdown();

    // Material binding yönetimi
    bool CreateMaterialBindings(std::shared_ptr<Material> material, const ShaderProgramInfo& programInfo);
    bool UpdateMaterialBindings(std::shared_ptr<Material> material);
    void DestroyMaterialBindings(std::shared_ptr<Material> material);
    
    // Texture binding yönetimi
    bool CreateTextureBindings(std::shared_ptr<Texture> texture, const ShaderProgramInfo& programInfo, 
                              uint32_t set, uint32_t binding);
    bool UpdateTextureBindings(std::shared_ptr<Texture> texture);
    void DestroyTextureBindings(std::shared_ptr<Texture> texture);
    
    // Descriptor set yönetimi
    VkDescriptorSet CreateDescriptorSet(const ShaderProgramInfo& programInfo, uint32_t setIndex);
    void UpdateDescriptorSet(VkDescriptorSet descriptorSet, const ShaderProgramInfo& programInfo,
                           uint32_t setIndex, const std::vector<VkWriteDescriptorSet>& writes);
    
    // Layout oluşturma
    VkDescriptorSetLayout CreateMaterialDescriptorSetLayout(const ShaderProgramInfo& programInfo) const;
    VkDescriptorSetLayout CreateTextureDescriptorSetLayout(const ShaderProgramInfo& programInfo) const;
    
    // Binding doğrulama
    bool ValidateMaterialBindings(const std::shared_ptr<Material>& material, 
                                 const ShaderProgramInfo& programInfo) const;
    bool ValidateTextureBindings(const std::shared_ptr<Texture>& texture, 
                                const ShaderProgramInfo& programInfo, uint32_t set, uint32_t binding) const;
    
    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }

private:
    // Yardımcı metotlar
    std::vector<VkDescriptorSetLayoutBinding> GetMaterialLayoutBindings(const ShaderProgramInfo& programInfo) const;
    std::vector<VkDescriptorSetLayoutBinding> GetTextureLayoutBindings(const ShaderProgramInfo& programInfo) const;
    
    VkWriteDescriptorSet CreateMaterialWriteDescriptor(const std::shared_ptr<Material>& material,
                                                     VkDescriptorSet descriptorSet, 
                                                     const ShaderProgramInfo& programInfo) const;
    VkWriteDescriptorSet CreateTextureWriteDescriptor(const std::shared_ptr<Texture>& texture,
                                                     VkDescriptorSet descriptorSet,
                                                     uint32_t binding) const;
    
    void SetError(const std::string& error);

    // Member değişkenler
    VulkanDevice* m_device = nullptr;
    ShaderManager* m_shaderManager = nullptr;
    
    std::string m_lastError;
    
    bool m_initialized = false;
    mutable std::mutex m_mutex;
};

/**
 * @class ShaderHotReloader
 * @brief Shader hot-reload sistemi
 * 
 * Bu sınıf, shader dosyalarındaki değişiklikleri izler ve
 * otomatik olarak yeniden derler.
 */
class ShaderHotReloader {
public:
    ShaderHotReloader();
    ~ShaderHotReloader();

    // Non-copyable
    ShaderHotReloader(const ShaderHotReloader&) = delete;
    ShaderHotReloader& operator=(const ShaderHotReloader&) = delete;

    // Yaşam döngüsü
    bool Initialize(ShaderManager* shaderManager);
    void Shutdown();
    void Update();

    // Dosya izleme
    void WatchShaderFile(const std::string& filePath, const std::string& programName);
    void UnwatchShaderFile(const std::string& filePath);
    void ClearWatchedFiles();

    // Hot-reload kontrolü
    void EnableHotReload(bool enable);
    bool IsHotReloadEnabled() const { return m_hotReloadEnabled; }
    void CheckForChanges();
    void ReloadChangedPrograms();

    // İstatistikler
    size_t GetWatchedFileCount() const { return m_watchedFiles.size(); }
    size_t GetReloadCount() const { return m_reloadCount; }
    void PrintWatchedFiles() const;

private:
    // Yardımcı metotlar
    bool HasFileChanged(const std::string& filePath) const;
    std::filesystem::file_time_type GetLastModifiedTime(const std::string& filePath) const;
    void ReloadProgram(const std::string& programName);
    
    void SetError(const std::string& error);

    // Member değişkenler
    ShaderManager* m_shaderManager = nullptr;
    
    struct WatchedFileInfo {
        std::string filePath;
        std::string programName;
        std::filesystem::file_time_type lastModified;
        bool needsReload = false;
    };
    
    std::unordered_map<std::string, WatchedFileInfo> m_watchedFiles;
    
    bool m_hotReloadEnabled = true;
    size_t m_reloadCount = 0;
    uint64_t m_lastCheckTime = 0;
    static const uint64_t CHECK_INTERVAL = 1000; // 1 saniye
    
    std::string m_lastError;
    
    bool m_initialized = false;
    mutable std::mutex m_mutex;
};

// Global shader manager erişimi
ShaderManager* GetShaderManager();

} // namespace AstralEngine
