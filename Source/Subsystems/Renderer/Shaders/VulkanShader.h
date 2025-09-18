#pragma once

#include "../Core/VulkanDevice.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace AstralEngine {

/**
 * @class VulkanShader
 * @brief Vulkan shader modülü yönetimi için sınıf
 * 
 * Bu sınıf, SPIR-V formatında derlenmiş shader dosyalarını yükler,
 * VkShaderModule oluşturur ve yönetir. RAII prensibine uygun olarak
 * kaynakları temizler.
 */
class VulkanShader {
public:
    VulkanShader();
    ~VulkanShader();

    // Non-copyable
    VulkanShader(const VulkanShader&) = delete;
    VulkanShader& operator=(const VulkanShader&) = delete;

    // Yaşam döngüsü
    bool Initialize(VulkanDevice* device, const std::vector<uint32_t>& spirvCode, VkShaderStageFlagBits stage);
    void Shutdown();

    // Getter'lar
    VkShaderModule GetModule() const { return m_shaderModule; }
    VkShaderStageFlagBits GetStage() const { return m_stage; }
    bool IsInitialized() const { return m_isInitialized; }

    // Hata yönetimi
    const std::string& GetLastError() const { return m_lastError; }

private:
    // Yardımcı metotlar
    bool CreateShaderModule(const std::vector<uint32_t>& spirvCode);
    void SetError(const std::string& error);

    // Member değişkenler
    VulkanDevice* m_device = nullptr;
    VkShaderModule m_shaderModule = VK_NULL_HANDLE;
    VkShaderStageFlagBits m_stage;
    std::string m_lastError;
    bool m_isInitialized = false;
};

} // namespace AstralEngine
