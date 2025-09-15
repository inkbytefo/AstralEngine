#pragma once

#include "../Core/VulkanDevice.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace AstralEngine {

/**
 * @class VulkanCommandPool
 * @brief Vulkan komut havuzu yönetimi için sınıf
 * 
 * Bu sınıf, GPU'ya gönderilecek komutların kaydedileceği
 * komut tamponlarını yönetmek için kullanılan VkCommandPool nesnesini
 * yönetir. RAII prensibi ile çalışır ve kaynakları otomatik temizler.
 */
class VulkanCommandPool {
public:
    /**
     * @brief Komut havuzu yapılandırma parametreleri
     */
    struct Config {
        uint32_t queueFamilyIndex;  ///< Kuyruk ailesi indeksi
        VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; ///< Oluşturma flag'leri
    };

    VulkanCommandPool();
    ~VulkanCommandPool();

    // Non-copyable
    VulkanCommandPool(const VulkanCommandPool&) = delete;
    VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;

    // Lifecycle management
    bool Initialize(VulkanDevice* device, const Config& config);
    void Shutdown();

    // Command buffer management
    bool AllocateCommandBuffers(uint32_t count, VkCommandBuffer* commandBuffers);
    void FreeCommandBuffers(uint32_t count, VkCommandBuffer* commandBuffers);

    // Getters
    VkCommandPool GetCommandPool() const { return m_commandPool; }
    bool IsInitialized() const { return m_isInitialized; }
    const std::string& GetLastError() const { return m_lastError; }

private:
    // Helper methods
    void SetError(const std::string& error);
    void HandleVulkanError(VkResult result, const std::string& operation);

    // Member variables
    VulkanDevice* m_device = nullptr;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    Config m_config;
    std::string m_lastError;
    bool m_isInitialized = false;
};

} // namespace AstralEngine
