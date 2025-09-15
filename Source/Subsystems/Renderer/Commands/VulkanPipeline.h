#pragma once

#include "../Core/VulkanDevice.h"
#include "../Core/VulkanSwapchain.h"
#include "../Shaders/VulkanShader.h"
#include "../RendererTypes.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace AstralEngine {

/**
 * @class VulkanPipeline
 * @brief Vulkan graphics pipeline yönetimi için sınıf
 * 
 * Bu sınıf, GPU'nun çizim işlemini nasıl yapacağını tanımlayan
 * VkPipeline ve VkPipelineLayout nesnelerini yönetir.
 * Shader aşamaları, vertex input, rasterization gibi tüm
 * pipeline durumlarını yapılandırır.
 */
class VulkanPipeline {
public:
    /**
     * @brief Pipeline yapılandırma parametreleri
     */
    struct Config {
        std::vector<VulkanShader*> shaders;      ///< Kullanılacak shader'lar
        VulkanSwapchain* swapchain;             ///< Swapchain (render pass için)
        VkExtent2D extent;                      ///< Pencere boyutu
        VkDescriptorSetLayout descriptorSetLayout; ///< Descriptor set layout (uniform buffer için)
        bool useMinimalVertexInput = false;     ///< For debugging - bypass vertex attributes
    };

    VulkanPipeline();
    ~VulkanPipeline();

    // Non-copyable
    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, const Config& config);
    void Shutdown();

    // Getter'lar
    VkPipeline GetPipeline() const { return m_pipeline; }
    VkPipelineLayout GetLayout() const { return m_pipelineLayout; }
    bool IsInitialized() const { return m_isInitialized; }

    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }
    
    // Vulkan result string conversion
    std::string GetVulkanResultString(VkResult result) const;

private:
    // Pipeline oluşturma yardımcıları
    bool CreatePipelineLayout();
    bool CreateGraphicsPipeline();
    
    // Pipeline aşama oluşturma yardımcıları
    std::vector<VkPipelineShaderStageCreateInfo> CreateShaderStages();
    VkPipelineVertexInputStateCreateInfo CreateVertexInputState(
        const VkVertexInputBindingDescription& bindingDescription,
        const std::array<VkVertexInputAttributeDescription, 2>& attributeDescriptions
    );
    VkPipelineInputAssemblyStateCreateInfo CreateInputAssemblyState();
    std::vector<VkDynamicState> GetDynamicStates();
    VkPipelineViewportStateCreateInfo CreateViewportState();
    VkPipelineRasterizationStateCreateInfo CreateRasterizationState();
    VkPipelineMultisampleStateCreateInfo CreateMultisampleState();
    VkPipelineDepthStencilStateCreateInfo CreateDepthStencilState();
    VkPipelineColorBlendStateCreateInfo CreateColorBlendState();
    
    // Hata yönetimi
    void SetError(const std::string& error);

    // Member değişkenler
    VulkanDevice* m_device = nullptr;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    Config m_config;
    std::string m_lastError;
    bool m_isInitialized = false;
};

} // namespace AstralEngine
